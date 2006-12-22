/*
 *  acpi.c - Architecture-Specific Low-Level ACPI Support
 *
 *  Copyright (C) 1999 VA Linux Systems
 *  Copyright (C) 1999,2000 Walt Drummond <drummond@valinux.com>
 *  Copyright (C) 2000, 2002-2003 Hewlett-Packard Co.
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *  Copyright (C) 2000 Intel Corp.
 *  Copyright (C) 2000,2001 J.I. Lee <jung-ik.lee@intel.com>
 *  Copyright (C) 2001 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2001 Jenna Hall <jenna.s.hall@intel.com>
 *  Copyright (C) 2001 Takayoshi Kochi <t-kochi@bq.jp.nec.com>
 *  Copyright (C) 2002 Erich Focht <efocht@ess.nec.de>
 *  Copyright (C) 2004 Ashok Raj <ashok.raj@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/irq.h>
#include <linux/acpi.h>
#include <linux/efi.h>
#include <linux/mmzone.h>
#include <linux/nodemask.h>
#include <asm/io.h>
#include <asm/iosapic.h>
#include <asm/machvec.h>
#include <asm/page.h>
#include <asm/system.h>
#include <asm/numa.h>
#include <asm/sal.h>
#include <asm/cyclone.h>

#define BAD_MADT_ENTRY(entry, end) (                                        \
		(!entry) || (unsigned long)entry + sizeof(*entry) > end ||  \
		((acpi_table_entry_header *)entry)->length < sizeof(*entry))

#define PREFIX			"ACPI: "

void (*pm_idle) (void);
EXPORT_SYMBOL(pm_idle);
void (*pm_power_off) (void);
EXPORT_SYMBOL(pm_power_off);

unsigned int acpi_cpei_override;
unsigned int acpi_cpei_phys_cpuid;

#define MAX_SAPICS 256
u16 ia64_acpiid_to_sapicid[MAX_SAPICS] = {[0 ... MAX_SAPICS - 1] = -1 };

EXPORT_SYMBOL(ia64_acpiid_to_sapicid);

const char *acpi_get_sysname(void)
{
#ifdef CONFIG_IA64_GENERIC
	unsigned long rsdp_phys;
	struct acpi20_table_rsdp *rsdp;
	struct acpi_table_xsdt *xsdt;
	struct acpi_table_header *hdr;

	rsdp_phys = acpi_find_rsdp();
	if (!rsdp_phys) {
		printk(KERN_ERR
		       "ACPI 2.0 RSDP not found, default to \"dig\"\n");
		return "dig";
	}

	rsdp = (struct acpi20_table_rsdp *)__va(rsdp_phys);
	if (strncmp(rsdp->signature, RSDP_SIG, sizeof(RSDP_SIG) - 1)) {
		printk(KERN_ERR
		       "ACPI 2.0 RSDP signature incorrect, default to \"dig\"\n");
		return "dig";
	}

	xsdt = (struct acpi_table_xsdt *)__va(rsdp->xsdt_address);
	hdr = &xsdt->header;
	if (strncmp(hdr->signature, XSDT_SIG, sizeof(XSDT_SIG) - 1)) {
		printk(KERN_ERR
		       "ACPI 2.0 XSDT signature incorrect, default to \"dig\"\n");
		return "dig";
	}

	if (!strcmp(hdr->oem_id, "HP")) {
		return "hpzx1";
	} else if (!strcmp(hdr->oem_id, "SGI")) {
		return "sn2";
	}

	return "dig";
#else
# if defined (CONFIG_IA64_HP_SIM)
	return "hpsim";
# elif defined (CONFIG_IA64_HP_ZX1)
	return "hpzx1";
# elif defined (CONFIG_IA64_HP_ZX1_SWIOTLB)
	return "hpzx1_swiotlb";
# elif defined (CONFIG_IA64_SGI_SN2)
	return "sn2";
# elif defined (CONFIG_IA64_DIG)
	return "dig";
# else
#	error Unknown platform.  Fix acpi.c.
# endif
#endif
}

#ifdef CONFIG_ACPI

#define ACPI_MAX_PLATFORM_INTERRUPTS	256

/* Array to record platform interrupt vectors for generic interrupt routing. */
int platform_intr_list[ACPI_MAX_PLATFORM_INTERRUPTS] = {
	[0 ... ACPI_MAX_PLATFORM_INTERRUPTS - 1] = -1
};

enum acpi_irq_model_id acpi_irq_model = ACPI_IRQ_MODEL_IOSAPIC;

/*
 * Interrupt routing API for device drivers.  Provides interrupt vector for
 * a generic platform event.  Currently only CPEI is implemented.
 */
int acpi_request_vector(u32 int_type)
{
	int vector = -1;

	if (int_type < ACPI_MAX_PLATFORM_INTERRUPTS) {
		/* corrected platform error interrupt */
		vector = platform_intr_list[int_type];
	} else
		printk(KERN_ERR
		       "acpi_request_vector(): invalid interrupt type\n");
	return vector;
}

char *__acpi_map_table(unsigned long phys_addr, unsigned long size)
{
	return __va(phys_addr);
}

/* --------------------------------------------------------------------------
                            Boot-time Table Parsing
   -------------------------------------------------------------------------- */

static int total_cpus __initdata;
static int available_cpus __initdata;
struct acpi_table_madt *acpi_madt __initdata;
static u8 has_8259;

static int __init
acpi_parse_lapic_addr_ovr(acpi_table_entry_header * header,
			  const unsigned long end)
{
	struct acpi_table_lapic_addr_ovr *lapic;

	lapic = (struct acpi_table_lapic_addr_ovr *)header;

	if (BAD_MADT_ENTRY(lapic, end))
		return -EINVAL;

	if (lapic->address) {
		iounmap(ipi_base_addr);
		ipi_base_addr = ioremap(lapic->address, 0);
	}
	return 0;
}

static int __init
acpi_parse_lsapic(acpi_table_entry_header * header, const unsigned long end)
{
	struct acpi_table_lsapic *lsapic;

	lsapic = (struct acpi_table_lsapic *)header;

	if (BAD_MADT_ENTRY(lsapic, end))
		return -EINVAL;

	if (lsapic->flags.enabled) {
#ifdef CONFIG_SMP
		smp_boot_data.cpu_phys_id[available_cpus] =
		    (lsapic->id << 8) | lsapic->eid;
#endif
		ia64_acpiid_to_sapicid[lsapic->acpi_id] =
		    (lsapic->id << 8) | lsapic->eid;
		++available_cpus;
	}

	total_cpus++;
	return 0;
}

static int __init
acpi_parse_lapic_nmi(acpi_table_entry_header * header, const unsigned long end)
{
	struct acpi_table_lapic_nmi *lacpi_nmi;

	lacpi_nmi = (struct acpi_table_lapic_nmi *)header;

	if (BAD_MADT_ENTRY(lacpi_nmi, end))
		return -EINVAL;

	/* TBD: Support lapic_nmi entries */
	return 0;
}

static int __init
acpi_parse_iosapic(acpi_table_entry_header * header, const unsigned long end)
{
	struct acpi_table_iosapic *iosapic;

	iosapic = (struct acpi_table_iosapic *)header;

	if (BAD_MADT_ENTRY(iosapic, end))
		return -EINVAL;

	return iosapic_init(iosapic->address, iosapic->global_irq_base);
}

static unsigned int __initdata acpi_madt_rev;

static int __init
acpi_parse_plat_int_src(acpi_table_entry_header * header,
			const unsigned long end)
{
	struct acpi_table_plat_int_src *plintsrc;
	int vector;

	plintsrc = (struct acpi_table_plat_int_src *)header;

	if (BAD_MADT_ENTRY(plintsrc, end))
		return -EINVAL;

	/*
	 * Get vector assignment for this interrupt, set attributes,
	 * and program the IOSAPIC routing table.
	 */
	vector = iosapic_register_platform_intr(plintsrc->type,
						plintsrc->global_irq,
						plintsrc->iosapic_vector,
						plintsrc->eid,
						plintsrc->id,
						(plintsrc->flags.polarity ==
						 1) ? IOSAPIC_POL_HIGH :
						IOSAPIC_POL_LOW,
						(plintsrc->flags.trigger ==
						 1) ? IOSAPIC_EDGE :
						IOSAPIC_LEVEL);

	platform_intr_list[plintsrc->type] = vector;
	if (acpi_madt_rev > 1) {
		acpi_cpei_override = plintsrc->plint_flags.cpei_override_flag;
	}

	/*
	 * Save the physical id, so we can check when its being removed
	 */
	acpi_cpei_phys_cpuid = ((plintsrc->id << 8) | (plintsrc->eid)) & 0xffff;

	return 0;
}

#ifdef CONFIG_HOTPLUG_CPU
unsigned int can_cpei_retarget(void)
{
	extern int cpe_vector;
	extern unsigned int force_cpei_retarget;

	/*
	 * Only if CPEI is supported and the override flag
	 * is present, otherwise return that its re-targettable
	 * if we are in polling mode.
	 */
	if (cpe_vector > 0) {
		if (acpi_cpei_override || force_cpei_retarget)
			return 1;
		else
			return 0;
	}
	return 1;
}

unsigned int is_cpu_cpei_target(unsigned int cpu)
{
	unsigned int logical_id;

	logical_id = cpu_logical_id(acpi_cpei_phys_cpuid);

	if (logical_id == cpu)
		return 1;
	else
		return 0;
}

void set_cpei_target_cpu(unsigned int cpu)
{
	acpi_cpei_phys_cpuid = cpu_physical_id(cpu);
}
#endif

unsigned int get_cpei_target_cpu(void)
{
	return acpi_cpei_phys_cpuid;
}

static int __init
acpi_parse_int_src_ovr(acpi_table_entry_header * header,
		       const unsigned long end)
{
	struct acpi_table_int_src_ovr *p;

	p = (struct acpi_table_int_src_ovr *)header;

	if (BAD_MADT_ENTRY(p, end))
		return -EINVAL;

	iosapic_override_isa_irq(p->bus_irq, p->global_irq,
				 (p->flags.polarity ==
				  1) ? IOSAPIC_POL_HIGH : IOSAPIC_POL_LOW,
				 (p->flags.trigger ==
				  1) ? IOSAPIC_EDGE : IOSAPIC_LEVEL);
	return 0;
}

static int __init
acpi_parse_nmi_src(acpi_table_entry_header * header, const unsigned long end)
{
	struct acpi_table_nmi_src *nmi_src;

	nmi_src = (struct acpi_table_nmi_src *)header;

	if (BAD_MADT_ENTRY(nmi_src, end))
		return -EINVAL;

	/* TBD: Support nimsrc entries */
	return 0;
}

static void __init acpi_madt_oem_check(char *oem_id, char *oem_table_id)
{
	if (!strncmp(oem_id, "IBM", 3) && (!strncmp(oem_table_id, "SERMOW", 6))) {

		/*
		 * Unfortunately ITC_DRIFT is not yet part of the
		 * official SAL spec, so the ITC_DRIFT bit is not
		 * set by the BIOS on this hardware.
		 */
		sal_platform_features |= IA64_SAL_PLATFORM_FEATURE_ITC_DRIFT;

		cyclone_setup();
	}
}

static int __init acpi_parse_madt(unsigned long phys_addr, unsigned long size)
{
	if (!phys_addr || !size)
		return -EINVAL;

	acpi_madt = (struct acpi_table_madt *)__va(phys_addr);

	acpi_madt_rev = acpi_madt->header.revision;

	/* remember the value for reference after free_initmem() */
#ifdef CONFIG_ITANIUM
	has_8259 = 1;		/* Firmware on old Itanium systems is broken */
#else
	has_8259 = acpi_madt->flags.pcat_compat;
#endif
	iosapic_system_init(has_8259);

	/* Get base address of IPI Message Block */

	if (acpi_madt->lapic_address)
		ipi_base_addr = ioremap(acpi_madt->lapic_address, 0);

	printk(KERN_INFO PREFIX "Local APIC address %p\n", ipi_base_addr);

	acpi_madt_oem_check(acpi_madt->header.oem_id,
			    acpi_madt->header.oem_table_id);

	return 0;
}

#ifdef CONFIG_ACPI_NUMA

#undef SLIT_DEBUG

#define PXM_FLAG_LEN ((MAX_PXM_DOMAINS + 1)/32)

static int __initdata srat_num_cpus;	/* number of cpus */
static u32 __devinitdata pxm_flag[PXM_FLAG_LEN];
#define pxm_bit_set(bit)	(set_bit(bit,(void *)pxm_flag))
#define pxm_bit_test(bit)	(test_bit(bit,(void *)pxm_flag))
static struct acpi_table_slit __initdata *slit_table;

static int get_processor_proximity_domain(struct acpi_table_processor_affinity *pa)
{
	int pxm;

	pxm = pa->proximity_domain;
	if (ia64_platform_is("sn2"))
		pxm += pa->reserved[0] << 8;
	return pxm;
}

static int get_memory_proximity_domain(struct acpi_table_memory_affinity *ma)
{
	int pxm;

	pxm = ma->proximity_domain;
	if (ia64_platform_is("sn2"))
		pxm += ma->reserved1[0] << 8;
	return pxm;
}

/*
 * ACPI 2.0 SLIT (System Locality Information Table)
 * http://devresource.hp.com/devresource/Docs/TechPapers/IA64/slit.pdf
 */
void __init acpi_numa_slit_init(struct acpi_table_slit *slit)
{
	u32 len;

	len = sizeof(struct acpi_table_header) + 8
	    + slit->localities * slit->localities;
	if (slit->header.length != len) {
		printk(KERN_ERR
		       "ACPI 2.0 SLIT: size mismatch: %d expected, %d actual\n",
		       len, slit->header.length);
		memset(numa_slit, 10, sizeof(numa_slit));
		return;
	}
	slit_table = slit;
}

void __init
acpi_numa_processor_affinity_init(struct acpi_table_processor_affinity *pa)
{
	int pxm;

	if (!pa->flags.enabled)
		return;

	pxm = get_processor_proximity_domain(pa);

	/* record this node in proximity bitmap */
	pxm_bit_set(pxm);

	node_cpuid[srat_num_cpus].phys_id =
	    (pa->apic_id << 8) | (pa->lsapic_eid);
	/* nid should be overridden as logical node id later */
	node_cpuid[srat_num_cpus].nid = pxm;
	srat_num_cpus++;
}

void __init
acpi_numa_memory_affinity_init(struct acpi_table_memory_affinity *ma)
{
	unsigned long paddr, size;
	int pxm;
	struct node_memblk_s *p, *q, *pend;

	pxm = get_memory_proximity_domain(ma);

	/* fill node memory chunk structure */
	paddr = ma->base_addr_hi;
	paddr = (paddr << 32) | ma->base_addr_lo;
	size = ma->length_hi;
	size = (size << 32) | ma->length_lo;

	/* Ignore disabled entries */
	if (!ma->flags.enabled)
		return;

	/* record this node in proximity bitmap */
	pxm_bit_set(pxm);

	/* Insertion sort based on base address */
	pend = &node_memblk[num_node_memblks];
	for (p = &node_memblk[0]; p < pend; p++) {
		if (paddr < p->start_paddr)
			break;
	}
	if (p < pend) {
		for (q = pend - 1; q >= p; q--)
			*(q + 1) = *q;
	}
	p->start_paddr = paddr;
	p->size = size;
	p->nid = pxm;
	num_node_memblks++;
}

void __init acpi_numa_arch_fixup(void)
{
	int i, j, node_from, node_to;

	/* If there's no SRAT, fix the phys_id and mark node 0 online */
	if (srat_num_cpus == 0) {
		node_set_online(0);
		node_cpuid[0].phys_id = hard_smp_processor_id();
		return;
	}

	/*
	 * MCD - This can probably be dropped now.  No need for pxm ID to node ID
	 * mapping with sparse node numbering iff MAX_PXM_DOMAINS <= MAX_NUMNODES.
	 */
	nodes_clear(node_online_map);
	for (i = 0; i < MAX_PXM_DOMAINS; i++) {
		if (pxm_bit_test(i)) {
			int nid = acpi_map_pxm_to_node(i);
			node_set_online(nid);
		}
	}

	/* set logical node id in memory chunk structure */
	for (i = 0; i < num_node_memblks; i++)
		node_memblk[i].nid = pxm_to_node(node_memblk[i].nid);

	/* assign memory bank numbers for each chunk on each node */
	for_each_online_node(i) {
		int bank;

		bank = 0;
		for (j = 0; j < num_node_memblks; j++)
			if (node_memblk[j].nid == i)
				node_memblk[j].bank = bank++;
	}

	/* set logical node id in cpu structure */
	for (i = 0; i < srat_num_cpus; i++)
		node_cpuid[i].nid = pxm_to_node(node_cpuid[i].nid);

	printk(KERN_INFO "Number of logical nodes in system = %d\n",
	       num_online_nodes());
	printk(KERN_INFO "Number of memory chunks in system = %d\n",
	       num_node_memblks);

	if (!slit_table)
		return;
	memset(numa_slit, -1, sizeof(numa_slit));
	for (i = 0; i < slit_table->localities; i++) {
		if (!pxm_bit_test(i))
			continue;
		node_from = pxm_to_node(i);
		for (j = 0; j < slit_table->localities; j++) {
			if (!pxm_bit_test(j))
				continue;
			node_to = pxm_to_node(j);
			node_distance(node_from, node_to) =
			    slit_table->entry[i * slit_table->localities + j];
		}
	}

#ifdef SLIT_DEBUG
	printk("ACPI 2.0 SLIT locality table:\n");
	for_each_online_node(i) {
		for_each_online_node(j)
		    printk("%03d ", node_distance(i, j));
		printk("\n");
	}
#endif
}
#endif				/* CONFIG_ACPI_NUMA */

/*
 * success: return IRQ number (>=0)
 * failure: return < 0
 */
int acpi_register_gsi(u32 gsi, int triggering, int polarity)
{
	if (acpi_irq_model == ACPI_IRQ_MODEL_PLATFORM)
		return gsi;

	if (has_8259 && gsi < 16)
		return isa_irq_to_vector(gsi);

	return iosapic_register_intr(gsi,
				     (polarity ==
				      ACPI_ACTIVE_HIGH) ? IOSAPIC_POL_HIGH :
				     IOSAPIC_POL_LOW,
				     (triggering ==
				      ACPI_EDGE_SENSITIVE) ? IOSAPIC_EDGE :
				     IOSAPIC_LEVEL);
}

EXPORT_SYMBOL(acpi_register_gsi);

void acpi_unregister_gsi(u32 gsi)
{
	iosapic_unregister_intr(gsi);
}

EXPORT_SYMBOL(acpi_unregister_gsi);

static int __init acpi_parse_fadt(unsigned long phys_addr, unsigned long size)
{
	struct acpi_table_header *fadt_header;
	struct fadt_descriptor *fadt;

	if (!phys_addr || !size)
		return -EINVAL;

	fadt_header = (struct acpi_table_header *)__va(phys_addr);
	if (fadt_header->revision != 3)
		return -ENODEV;	/* Only deal with ACPI 2.0 FADT */

	fadt = (struct fadt_descriptor *)fadt_header;

	acpi_register_gsi(fadt->sci_int, ACPI_LEVEL_SENSITIVE, ACPI_ACTIVE_LOW);
	return 0;
}

unsigned long __init acpi_find_rsdp(void)
{
	unsigned long rsdp_phys = 0;

	if (efi.acpi20 != EFI_INVALID_TABLE_ADDR)
		rsdp_phys = efi.acpi20;
	else if (efi.acpi != EFI_INVALID_TABLE_ADDR)
		printk(KERN_WARNING PREFIX
		       "v1.0/r0.71 tables no longer supported\n");
	return rsdp_phys;
}

int __init acpi_boot_init(void)
{

	/*
	 * MADT
	 * ----
	 * Parse the Multiple APIC Description Table (MADT), if exists.
	 * Note that this table provides platform SMP configuration
	 * information -- the successor to MPS tables.
	 */

	if (acpi_table_parse(ACPI_APIC, acpi_parse_madt) < 1) {
		printk(KERN_ERR PREFIX "Can't find MADT\n");
		goto skip_madt;
	}

	/* Local APIC */

	if (acpi_table_parse_madt
	    (ACPI_MADT_LAPIC_ADDR_OVR, acpi_parse_lapic_addr_ovr, 0) < 0)
		printk(KERN_ERR PREFIX
		       "Error parsing LAPIC address override entry\n");

	if (acpi_table_parse_madt(ACPI_MADT_LSAPIC, acpi_parse_lsapic, NR_CPUS)
	    < 1)
		printk(KERN_ERR PREFIX
		       "Error parsing MADT - no LAPIC entries\n");

	if (acpi_table_parse_madt(ACPI_MADT_LAPIC_NMI, acpi_parse_lapic_nmi, 0)
	    < 0)
		printk(KERN_ERR PREFIX "Error parsing LAPIC NMI entry\n");

	/* I/O APIC */

	if (acpi_table_parse_madt
	    (ACPI_MADT_IOSAPIC, acpi_parse_iosapic, NR_IOSAPICS) < 1)
		printk(KERN_ERR PREFIX
		       "Error parsing MADT - no IOSAPIC entries\n");

	/* System-Level Interrupt Routing */

	if (acpi_table_parse_madt
	    (ACPI_MADT_PLAT_INT_SRC, acpi_parse_plat_int_src,
	     ACPI_MAX_PLATFORM_INTERRUPTS) < 0)
		printk(KERN_ERR PREFIX
		       "Error parsing platform interrupt source entry\n");

	if (acpi_table_parse_madt
	    (ACPI_MADT_INT_SRC_OVR, acpi_parse_int_src_ovr, 0) < 0)
		printk(KERN_ERR PREFIX
		       "Error parsing interrupt source overrides entry\n");

	if (acpi_table_parse_madt(ACPI_MADT_NMI_SRC, acpi_parse_nmi_src, 0) < 0)
		printk(KERN_ERR PREFIX "Error parsing NMI SRC entry\n");
      skip_madt:

	/*
	 * FADT says whether a legacy keyboard controller is present.
	 * The FADT also contains an SCI_INT line, by which the system
	 * gets interrupts such as power and sleep buttons.  If it's not
	 * on a Legacy interrupt, it needs to be setup.
	 */
	if (acpi_table_parse(ACPI_FADT, acpi_parse_fadt) < 1)
		printk(KERN_ERR PREFIX "Can't find FADT\n");

#ifdef CONFIG_SMP
	if (available_cpus == 0) {
		printk(KERN_INFO "ACPI: Found 0 CPUS; assuming 1\n");
		printk(KERN_INFO "CPU 0 (0x%04x)", hard_smp_processor_id());
		smp_boot_data.cpu_phys_id[available_cpus] =
		    hard_smp_processor_id();
		available_cpus = 1;	/* We've got at least one of these, no? */
	}
	smp_boot_data.cpu_count = available_cpus;

	smp_build_cpu_map();
# ifdef CONFIG_ACPI_NUMA
	if (srat_num_cpus == 0) {
		int cpu, i = 1;
		for (cpu = 0; cpu < smp_boot_data.cpu_count; cpu++)
			if (smp_boot_data.cpu_phys_id[cpu] !=
			    hard_smp_processor_id())
				node_cpuid[i++].phys_id =
				    smp_boot_data.cpu_phys_id[cpu];
	}
# endif
#endif
#ifdef CONFIG_ACPI_NUMA
	build_cpu_to_node_map();
#endif
	/* Make boot-up look pretty */
	printk(KERN_INFO "%d CPUs available, %d CPUs total\n", available_cpus,
	       total_cpus);
	return 0;
}

int acpi_gsi_to_irq(u32 gsi, unsigned int *irq)
{
	int vector;

	if (has_8259 && gsi < 16)
		*irq = isa_irq_to_vector(gsi);
	else {
		vector = gsi_to_vector(gsi);
		if (vector == -1)
			return -1;

		*irq = vector;
	}
	return 0;
}

/*
 *  ACPI based hotplug CPU support
 */
#ifdef CONFIG_ACPI_HOTPLUG_CPU
static
int acpi_map_cpu2node(acpi_handle handle, int cpu, long physid)
{
#ifdef CONFIG_ACPI_NUMA
	int pxm_id;
	int nid;

	pxm_id = acpi_get_pxm(handle);
	/*
	 * We don't have cpu-only-node hotadd. But if the system equips
	 * SRAT table, pxm is already found and node is ready.
  	 * So, just pxm_to_nid(pxm) is OK.
	 * This code here is for the system which doesn't have full SRAT
  	 * table for possible cpus.
	 */
	nid = acpi_map_pxm_to_node(pxm_id);
	node_cpuid[cpu].phys_id = physid;
	node_cpuid[cpu].nid = nid;
#endif
	return (0);
}

int additional_cpus __initdata = -1;

static __init int setup_additional_cpus(char *s)
{
	if (s)
		additional_cpus = simple_strtol(s, NULL, 0);

	return 0;
}

early_param("additional_cpus", setup_additional_cpus);

/*
 * cpu_possible_map should be static, it cannot change as cpu's
 * are onlined, or offlined. The reason is per-cpu data-structures
 * are allocated by some modules at init time, and dont expect to
 * do this dynamically on cpu arrival/departure.
 * cpu_present_map on the other hand can change dynamically.
 * In case when cpu_hotplug is not compiled, then we resort to current
 * behaviour, which is cpu_possible == cpu_present.
 * - Ashok Raj
 *
 * Three ways to find out the number of additional hotplug CPUs:
 * - If the BIOS specified disabled CPUs in ACPI/mptables use that.
 * - The user can overwrite it with additional_cpus=NUM
 * - Otherwise don't reserve additional CPUs.
 */
__init void prefill_possible_map(void)
{
	int i;
	int possible, disabled_cpus;

	disabled_cpus = total_cpus - available_cpus;

 	if (additional_cpus == -1) {
 		if (disabled_cpus > 0)
			additional_cpus = disabled_cpus;
 		else
			additional_cpus = 0;
 	}

	possible = available_cpus + additional_cpus;

	if (possible > NR_CPUS)
		possible = NR_CPUS;

	printk(KERN_INFO "SMP: Allowing %d CPUs, %d hotplug CPUs\n",
		possible, max((possible - available_cpus), 0));

	for (i = 0; i < possible; i++)
		cpu_set(i, cpu_possible_map);
}

int acpi_map_lsapic(acpi_handle handle, int *pcpu)
{
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	struct acpi_table_lsapic *lsapic;
	cpumask_t tmp_map;
	long physid;
	int cpu;

	if (ACPI_FAILURE(acpi_evaluate_object(handle, "_MAT", NULL, &buffer)))
		return -EINVAL;

	if (!buffer.length || !buffer.pointer)
		return -EINVAL;

	obj = buffer.pointer;
	if (obj->type != ACPI_TYPE_BUFFER ||
	    obj->buffer.length < sizeof(*lsapic)) {
		kfree(buffer.pointer);
		return -EINVAL;
	}

	lsapic = (struct acpi_table_lsapic *)obj->buffer.pointer;

	if ((lsapic->header.type != ACPI_MADT_LSAPIC) ||
	    (!lsapic->flags.enabled)) {
		kfree(buffer.pointer);
		return -EINVAL;
	}

	physid = ((lsapic->id << 8) | (lsapic->eid));

	kfree(buffer.pointer);
	buffer.length = ACPI_ALLOCATE_BUFFER;
	buffer.pointer = NULL;

	cpus_complement(tmp_map, cpu_present_map);
	cpu = first_cpu(tmp_map);
	if (cpu >= NR_CPUS)
		return -EINVAL;

	acpi_map_cpu2node(handle, cpu, physid);

	cpu_set(cpu, cpu_present_map);
	ia64_cpu_to_sapicid[cpu] = physid;
	ia64_acpiid_to_sapicid[lsapic->acpi_id] = ia64_cpu_to_sapicid[cpu];

	*pcpu = cpu;
	return (0);
}

EXPORT_SYMBOL(acpi_map_lsapic);

int acpi_unmap_lsapic(int cpu)
{
	int i;

	for (i = 0; i < MAX_SAPICS; i++) {
		if (ia64_acpiid_to_sapicid[i] == ia64_cpu_to_sapicid[cpu]) {
			ia64_acpiid_to_sapicid[i] = -1;
			break;
		}
	}
	ia64_cpu_to_sapicid[cpu] = -1;
	cpu_clear(cpu, cpu_present_map);

#ifdef CONFIG_ACPI_NUMA
	/* NUMA specific cleanup's */
#endif

	return (0);
}

EXPORT_SYMBOL(acpi_unmap_lsapic);
#endif				/* CONFIG_ACPI_HOTPLUG_CPU */

#ifdef CONFIG_ACPI_NUMA
static acpi_status __devinit
acpi_map_iosapic(acpi_handle handle, u32 depth, void *context, void **ret)
{
	struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	struct acpi_table_iosapic *iosapic;
	unsigned int gsi_base;
	int pxm, node;

	/* Only care about objects w/ a method that returns the MADT */
	if (ACPI_FAILURE(acpi_evaluate_object(handle, "_MAT", NULL, &buffer)))
		return AE_OK;

	if (!buffer.length || !buffer.pointer)
		return AE_OK;

	obj = buffer.pointer;
	if (obj->type != ACPI_TYPE_BUFFER ||
	    obj->buffer.length < sizeof(*iosapic)) {
		kfree(buffer.pointer);
		return AE_OK;
	}

	iosapic = (struct acpi_table_iosapic *)obj->buffer.pointer;

	if (iosapic->header.type != ACPI_MADT_IOSAPIC) {
		kfree(buffer.pointer);
		return AE_OK;
	}

	gsi_base = iosapic->global_irq_base;

	kfree(buffer.pointer);

	/*
	 * OK, it's an IOSAPIC MADT entry, look for a _PXM value to tell
	 * us which node to associate this with.
	 */
	pxm = acpi_get_pxm(handle);
	if (pxm < 0)
		return AE_OK;

	node = pxm_to_node(pxm);

	if (node >= MAX_NUMNODES || !node_online(node) ||
	    cpus_empty(node_to_cpumask(node)))
		return AE_OK;

	/* We know a gsi to node mapping! */
	map_iosapic_to_node(gsi_base, node);
	return AE_OK;
}

static int __init
acpi_map_iosapics (void)
{
	acpi_get_devices(NULL, acpi_map_iosapic, NULL, NULL);
	return 0;
}

fs_initcall(acpi_map_iosapics);
#endif				/* CONFIG_ACPI_NUMA */

int acpi_register_ioapic(acpi_handle handle, u64 phys_addr, u32 gsi_base)
{
	int err;

	if ((err = iosapic_init(phys_addr, gsi_base)))
		return err;

#ifdef CONFIG_ACPI_NUMA
	acpi_map_iosapic(handle, 0, NULL, NULL);
#endif				/* CONFIG_ACPI_NUMA */

	return 0;
}

EXPORT_SYMBOL(acpi_register_ioapic);

int acpi_unregister_ioapic(acpi_handle handle, u32 gsi_base)
{
	return iosapic_remove(gsi_base);
}

EXPORT_SYMBOL(acpi_unregister_ioapic);

#endif				/* CONFIG_ACPI */
