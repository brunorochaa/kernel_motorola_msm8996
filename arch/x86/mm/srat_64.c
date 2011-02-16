/*
 * ACPI 3.0 based NUMA setup
 * Copyright 2004 Andi Kleen, SuSE Labs.
 *
 * Reads the ACPI SRAT table to figure out what memory belongs to which CPUs.
 *
 * Called from acpi_numa_init while reading the SRAT and SLIT tables.
 * Assumes all memory regions belonging to a single proximity domain
 * are in one chunk. Holes between them will be included in the node.
 */

#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/mmzone.h>
#include <linux/bitmap.h>
#include <linux/module.h>
#include <linux/topology.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <asm/proto.h>
#include <asm/numa.h>
#include <asm/e820.h>
#include <asm/apic.h>
#include <asm/uv/uv.h>

int acpi_numa __initdata;

static struct acpi_table_slit *acpi_slit;

static struct bootnode nodes_add[MAX_NUMNODES];

static __init int setup_node(int pxm)
{
	return acpi_map_pxm_to_node(pxm);
}

static __init void bad_srat(void)
{
	printk(KERN_ERR "SRAT: SRAT not used.\n");
	acpi_numa = -1;
	memset(nodes_add, 0, sizeof(nodes_add));
}

static __init inline int srat_disabled(void)
{
	return acpi_numa < 0;
}

/* Callback for SLIT parsing */
void __init acpi_numa_slit_init(struct acpi_table_slit *slit)
{
	int i, j;
	unsigned length;
	unsigned long phys;

	for (i = 0; i < slit->locality_count; i++)
		for (j = 0; j < slit->locality_count; j++)
			numa_set_distance(pxm_to_node(i), pxm_to_node(j),
				slit->entry[slit->locality_count * i + j]);

	/* acpi_slit is used only by emulation */
	length = slit->header.length;
	phys = memblock_find_in_range(0, max_pfn_mapped<<PAGE_SHIFT, length,
		 PAGE_SIZE);

	if (phys == MEMBLOCK_ERROR)
		panic(" Can not save slit!\n");

	acpi_slit = __va(phys);
	memcpy(acpi_slit, slit, length);
	memblock_x86_reserve_range(phys, phys + length, "ACPI SLIT");
}

/* Callback for Proximity Domain -> x2APIC mapping */
void __init
acpi_numa_x2apic_affinity_init(struct acpi_srat_x2apic_cpu_affinity *pa)
{
	int pxm, node;
	int apic_id;

	if (srat_disabled())
		return;
	if (pa->header.length < sizeof(struct acpi_srat_x2apic_cpu_affinity)) {
		bad_srat();
		return;
	}
	if ((pa->flags & ACPI_SRAT_CPU_ENABLED) == 0)
		return;
	pxm = pa->proximity_domain;
	node = setup_node(pxm);
	if (node < 0) {
		printk(KERN_ERR "SRAT: Too many proximity domains %x\n", pxm);
		bad_srat();
		return;
	}

	apic_id = pa->apic_id;
	if (apic_id >= MAX_LOCAL_APIC) {
		printk(KERN_INFO "SRAT: PXM %u -> APIC 0x%04x -> Node %u skipped apicid that is too big\n", pxm, apic_id, node);
		return;
	}
	set_apicid_to_node(apic_id, node);
	node_set(node, numa_nodes_parsed);
	acpi_numa = 1;
	printk(KERN_INFO "SRAT: PXM %u -> APIC 0x%04x -> Node %u\n",
	       pxm, apic_id, node);
}

/* Callback for Proximity Domain -> LAPIC mapping */
void __init
acpi_numa_processor_affinity_init(struct acpi_srat_cpu_affinity *pa)
{
	int pxm, node;
	int apic_id;

	if (srat_disabled())
		return;
	if (pa->header.length != sizeof(struct acpi_srat_cpu_affinity)) {
		bad_srat();
		return;
	}
	if ((pa->flags & ACPI_SRAT_CPU_ENABLED) == 0)
		return;
	pxm = pa->proximity_domain_lo;
	node = setup_node(pxm);
	if (node < 0) {
		printk(KERN_ERR "SRAT: Too many proximity domains %x\n", pxm);
		bad_srat();
		return;
	}

	if (get_uv_system_type() >= UV_X2APIC)
		apic_id = (pa->apic_id << 8) | pa->local_sapic_eid;
	else
		apic_id = pa->apic_id;

	if (apic_id >= MAX_LOCAL_APIC) {
		printk(KERN_INFO "SRAT: PXM %u -> APIC 0x%02x -> Node %u skipped apicid that is too big\n", pxm, apic_id, node);
		return;
	}

	set_apicid_to_node(apic_id, node);
	node_set(node, numa_nodes_parsed);
	acpi_numa = 1;
	printk(KERN_INFO "SRAT: PXM %u -> APIC 0x%02x -> Node %u\n",
	       pxm, apic_id, node);
}

#ifdef CONFIG_MEMORY_HOTPLUG_SPARSE
static inline int save_add_info(void) {return 1;}
#else
static inline int save_add_info(void) {return 0;}
#endif
/*
 * Update nodes_add[]
 * This code supports one contiguous hot add area per node
 */
static void __init
update_nodes_add(int node, unsigned long start, unsigned long end)
{
	unsigned long s_pfn = start >> PAGE_SHIFT;
	unsigned long e_pfn = end >> PAGE_SHIFT;
	int changed = 0;
	struct bootnode *nd = &nodes_add[node];

	/* I had some trouble with strange memory hotadd regions breaking
	   the boot. Be very strict here and reject anything unexpected.
	   If you want working memory hotadd write correct SRATs.

	   The node size check is a basic sanity check to guard against
	   mistakes */
	if ((signed long)(end - start) < NODE_MIN_SIZE) {
		printk(KERN_ERR "SRAT: Hotplug area too small\n");
		return;
	}

	/* This check might be a bit too strict, but I'm keeping it for now. */
	if (absent_pages_in_range(s_pfn, e_pfn) != e_pfn - s_pfn) {
		printk(KERN_ERR
			"SRAT: Hotplug area %lu -> %lu has existing memory\n",
			s_pfn, e_pfn);
		return;
	}

	/* Looks good */

	if (nd->start == nd->end) {
		nd->start = start;
		nd->end = end;
		changed = 1;
	} else {
		if (nd->start == end) {
			nd->start = start;
			changed = 1;
		}
		if (nd->end == start) {
			nd->end = end;
			changed = 1;
		}
		if (!changed)
			printk(KERN_ERR "SRAT: Hotplug zone not continuous. Partly ignored\n");
	}

	if (changed) {
		node_set(node, numa_nodes_parsed);
		printk(KERN_INFO "SRAT: hot plug zone found %Lx - %Lx\n",
				 nd->start, nd->end);
	}
}

/* Callback for parsing of the Proximity Domain <-> Memory Area mappings */
void __init
acpi_numa_memory_affinity_init(struct acpi_srat_mem_affinity *ma)
{
	unsigned long start, end;
	int node, pxm;

	if (srat_disabled())
		return;
	if (ma->header.length != sizeof(struct acpi_srat_mem_affinity)) {
		bad_srat();
		return;
	}
	if ((ma->flags & ACPI_SRAT_MEM_ENABLED) == 0)
		return;

	if ((ma->flags & ACPI_SRAT_MEM_HOT_PLUGGABLE) && !save_add_info())
		return;
	start = ma->base_address;
	end = start + ma->length;
	pxm = ma->proximity_domain;
	node = setup_node(pxm);
	if (node < 0) {
		printk(KERN_ERR "SRAT: Too many proximity domains.\n");
		bad_srat();
		return;
	}

	if (numa_add_memblk(node, start, end) < 0) {
		bad_srat();
		return;
	}

	printk(KERN_INFO "SRAT: Node %u PXM %u %lx-%lx\n", node, pxm,
	       start, end);

	if (ma->flags & ACPI_SRAT_MEM_HOT_PLUGGABLE)
		update_nodes_add(node, start, end);
}

void __init acpi_numa_arch_fixup(void) {}

int __init x86_acpi_numa_init(void)
{
	int ret;

	ret = acpi_numa_init();
	if (ret < 0)
		return ret;
	return srat_disabled() ? -EINVAL : 0;
}

#ifdef CONFIG_NUMA_EMU
static int fake_node_to_pxm_map[MAX_NUMNODES] __initdata = {
	[0 ... MAX_NUMNODES-1] = PXM_INVAL
};
static s16 fake_apicid_to_node[MAX_LOCAL_APIC] __initdata = {
	[0 ... MAX_LOCAL_APIC-1] = NUMA_NO_NODE
};

/*
 * In NUMA emulation, we need to setup proximity domain (_PXM) to node ID
 * mappings that respect the real ACPI topology but reflect our emulated
 * environment.  For each emulated node, we find which real node it appears on
 * and create PXM to NID mappings for those fake nodes which mirror that
 * locality.  SLIT will now represent the correct distances between emulated
 * nodes as a result of the real topology.
 */
void __init acpi_fake_nodes(const struct bootnode *fake_nodes, int num_nodes)
{
	int i, j;

	for (i = 0; i < num_nodes; i++) {
		int nid, pxm;

		nid = find_node_by_addr(fake_nodes[i].start);
		if (nid == NUMA_NO_NODE)
			continue;
		pxm = node_to_pxm(nid);
		if (pxm == PXM_INVAL)
			continue;
		fake_node_to_pxm_map[i] = pxm;
		/*
		 * For each apicid_to_node mapping that exists for this real
		 * node, it must now point to the fake node ID.
		 */
		for (j = 0; j < MAX_LOCAL_APIC; j++)
			if (__apicid_to_node[j] == nid &&
			    fake_apicid_to_node[j] == NUMA_NO_NODE)
				fake_apicid_to_node[j] = i;
	}

	/*
	 * If there are apicid-to-node mappings for physical nodes that do not
	 * have a corresponding emulated node, it should default to a guaranteed
	 * value.
	 */
	for (i = 0; i < MAX_LOCAL_APIC; i++)
		if (__apicid_to_node[i] != NUMA_NO_NODE &&
		    fake_apicid_to_node[i] == NUMA_NO_NODE)
			fake_apicid_to_node[i] = 0;

	for (i = 0; i < num_nodes; i++)
		__acpi_map_pxm_to_node(fake_node_to_pxm_map[i], i);
	memcpy(__apicid_to_node, fake_apicid_to_node, sizeof(__apicid_to_node));

	for (i = 0; i < num_nodes; i++)
		if (fake_nodes[i].start != fake_nodes[i].end)
			node_set(i, numa_nodes_parsed);
}

int acpi_emu_node_distance(int a, int b)
{
	int index;

	if (!acpi_slit)
		return node_to_pxm(a) == node_to_pxm(b) ?
			LOCAL_DISTANCE : REMOTE_DISTANCE;
	index = acpi_slit->locality_count * node_to_pxm(a);
	return acpi_slit->entry[index + node_to_pxm(b)];
}
#endif /* CONFIG_NUMA_EMU */

#if defined(CONFIG_MEMORY_HOTPLUG_SPARSE) || defined(CONFIG_ACPI_HOTPLUG_MEMORY)
int memory_add_physaddr_to_nid(u64 start)
{
	int i, ret = 0;

	for_each_node(i)
		if (nodes_add[i].start <= start && nodes_add[i].end > start)
			ret = i;

	return ret;
}
EXPORT_SYMBOL_GPL(memory_add_physaddr_to_nid);
#endif
