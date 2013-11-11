/*
 * SGI NMI support routines
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 *  Copyright (c) 2009-2013 Silicon Graphics, Inc.  All Rights Reserved.
 *  Copyright (c) Mike Travis
 */

#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/kdb.h>
#include <linux/kexec.h>
#include <linux/kgdb.h>
#include <linux/module.h>
#include <linux/nmi.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <asm/apic.h>
#include <asm/current.h>
#include <asm/kdebug.h>
#include <asm/local64.h>
#include <asm/nmi.h>
#include <asm/traps.h>
#include <asm/uv/uv.h>
#include <asm/uv/uv_hub.h>
#include <asm/uv/uv_mmrs.h>

/*
 * UV handler for NMI
 *
 * Handle system-wide NMI events generated by the global 'power nmi' command.
 *
 * Basic operation is to field the NMI interrupt on each cpu and wait
 * until all cpus have arrived into the nmi handler.  If some cpus do not
 * make it into the handler, try and force them in with the IPI(NMI) signal.
 *
 * We also have to lessen UV Hub MMR accesses as much as possible as this
 * disrupts the UV Hub's primary mission of directing NumaLink traffic and
 * can cause system problems to occur.
 *
 * To do this we register our primary NMI notifier on the NMI_UNKNOWN
 * chain.  This reduces the number of false NMI calls when the perf
 * tools are running which generate an enormous number of NMIs per
 * second (~4M/s for 1024 cpu threads).  Our secondary NMI handler is
 * very short as it only checks that if it has been "pinged" with the
 * IPI(NMI) signal as mentioned above, and does not read the UV Hub's MMR.
 *
 */

static struct uv_hub_nmi_s **uv_hub_nmi_list;

DEFINE_PER_CPU(struct uv_cpu_nmi_s, __uv_cpu_nmi);
EXPORT_PER_CPU_SYMBOL_GPL(__uv_cpu_nmi);

static unsigned long nmi_mmr;
static unsigned long nmi_mmr_clear;
static unsigned long nmi_mmr_pending;

static atomic_t	uv_in_nmi;
static atomic_t uv_nmi_cpu = ATOMIC_INIT(-1);
static atomic_t uv_nmi_cpus_in_nmi = ATOMIC_INIT(-1);
static atomic_t uv_nmi_slave_continue;
static atomic_t uv_nmi_kexec_failed;
static cpumask_var_t uv_nmi_cpu_mask;

/* Values for uv_nmi_slave_continue */
#define SLAVE_CLEAR	0
#define SLAVE_CONTINUE	1
#define SLAVE_EXIT	2

/*
 * Default is all stack dumps go to the console and buffer.
 * Lower level to send to log buffer only.
 */
static int uv_nmi_loglevel = 7;
module_param_named(dump_loglevel, uv_nmi_loglevel, int, 0644);

/*
 * The following values show statistics on how perf events are affecting
 * this system.
 */
static int param_get_local64(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%lu\n", local64_read((local64_t *)kp->arg));
}

static int param_set_local64(const char *val, const struct kernel_param *kp)
{
	/* clear on any write */
	local64_set((local64_t *)kp->arg, 0);
	return 0;
}

static struct kernel_param_ops param_ops_local64 = {
	.get = param_get_local64,
	.set = param_set_local64,
};
#define param_check_local64(name, p) __param_check(name, p, local64_t)

static local64_t uv_nmi_count;
module_param_named(nmi_count, uv_nmi_count, local64, 0644);

static local64_t uv_nmi_misses;
module_param_named(nmi_misses, uv_nmi_misses, local64, 0644);

static local64_t uv_nmi_ping_count;
module_param_named(ping_count, uv_nmi_ping_count, local64, 0644);

static local64_t uv_nmi_ping_misses;
module_param_named(ping_misses, uv_nmi_ping_misses, local64, 0644);

/*
 * Following values allow tuning for large systems under heavy loading
 */
static int uv_nmi_initial_delay = 100;
module_param_named(initial_delay, uv_nmi_initial_delay, int, 0644);

static int uv_nmi_slave_delay = 100;
module_param_named(slave_delay, uv_nmi_slave_delay, int, 0644);

static int uv_nmi_loop_delay = 100;
module_param_named(loop_delay, uv_nmi_loop_delay, int, 0644);

static int uv_nmi_trigger_delay = 10000;
module_param_named(trigger_delay, uv_nmi_trigger_delay, int, 0644);

static int uv_nmi_wait_count = 100;
module_param_named(wait_count, uv_nmi_wait_count, int, 0644);

static int uv_nmi_retry_count = 500;
module_param_named(retry_count, uv_nmi_retry_count, int, 0644);

/*
 * Valid NMI Actions:
 *  "dump"	- dump process stack for each cpu
 *  "ips"	- dump IP info for each cpu
 *  "kdump"	- do crash dump
 *  "kdb"	- enter KDB/KGDB (default)
 */
static char uv_nmi_action[8] = "kdb";
module_param_string(action, uv_nmi_action, sizeof(uv_nmi_action), 0644);

static inline bool uv_nmi_action_is(const char *action)
{
	return (strncmp(uv_nmi_action, action, strlen(action)) == 0);
}

/* Setup which NMI support is present in system */
static void uv_nmi_setup_mmrs(void)
{
	if (uv_read_local_mmr(UVH_NMI_MMRX_SUPPORTED)) {
		uv_write_local_mmr(UVH_NMI_MMRX_REQ,
					1UL << UVH_NMI_MMRX_REQ_SHIFT);
		nmi_mmr = UVH_NMI_MMRX;
		nmi_mmr_clear = UVH_NMI_MMRX_CLEAR;
		nmi_mmr_pending = 1UL << UVH_NMI_MMRX_SHIFT;
		pr_info("UV: SMI NMI support: %s\n", UVH_NMI_MMRX_TYPE);
	} else {
		nmi_mmr = UVH_NMI_MMR;
		nmi_mmr_clear = UVH_NMI_MMR_CLEAR;
		nmi_mmr_pending = 1UL << UVH_NMI_MMR_SHIFT;
		pr_info("UV: SMI NMI support: %s\n", UVH_NMI_MMR_TYPE);
	}
}

/* Read NMI MMR and check if NMI flag was set by BMC. */
static inline int uv_nmi_test_mmr(struct uv_hub_nmi_s *hub_nmi)
{
	hub_nmi->nmi_value = uv_read_local_mmr(nmi_mmr);
	atomic_inc(&hub_nmi->read_mmr_count);
	return !!(hub_nmi->nmi_value & nmi_mmr_pending);
}

static inline void uv_local_mmr_clear_nmi(void)
{
	uv_write_local_mmr(nmi_mmr_clear, nmi_mmr_pending);
}

/*
 * If first cpu in on this hub, set hub_nmi "in_nmi" and "owner" values and
 * return true.  If first cpu in on the system, set global "in_nmi" flag.
 */
static int uv_set_in_nmi(int cpu, struct uv_hub_nmi_s *hub_nmi)
{
	int first = atomic_add_unless(&hub_nmi->in_nmi, 1, 1);

	if (first) {
		atomic_set(&hub_nmi->cpu_owner, cpu);
		if (atomic_add_unless(&uv_in_nmi, 1, 1))
			atomic_set(&uv_nmi_cpu, cpu);

		atomic_inc(&hub_nmi->nmi_count);
	}
	return first;
}

/* Check if this is a system NMI event */
static int uv_check_nmi(struct uv_hub_nmi_s *hub_nmi)
{
	int cpu = smp_processor_id();
	int nmi = 0;

	local64_inc(&uv_nmi_count);
	uv_cpu_nmi.queries++;

	do {
		nmi = atomic_read(&hub_nmi->in_nmi);
		if (nmi)
			break;

		if (raw_spin_trylock(&hub_nmi->nmi_lock)) {

			/* check hub MMR NMI flag */
			if (uv_nmi_test_mmr(hub_nmi)) {
				uv_set_in_nmi(cpu, hub_nmi);
				nmi = 1;
				break;
			}

			/* MMR NMI flag is clear */
			raw_spin_unlock(&hub_nmi->nmi_lock);

		} else {
			/* wait a moment for the hub nmi locker to set flag */
			cpu_relax();
			udelay(uv_nmi_slave_delay);

			/* re-check hub in_nmi flag */
			nmi = atomic_read(&hub_nmi->in_nmi);
			if (nmi)
				break;
		}

		/* check if this BMC missed setting the MMR NMI flag */
		if (!nmi) {
			nmi = atomic_read(&uv_in_nmi);
			if (nmi)
				uv_set_in_nmi(cpu, hub_nmi);
		}

	} while (0);

	if (!nmi)
		local64_inc(&uv_nmi_misses);

	return nmi;
}

/* Need to reset the NMI MMR register, but only once per hub. */
static inline void uv_clear_nmi(int cpu)
{
	struct uv_hub_nmi_s *hub_nmi = uv_hub_nmi;

	if (cpu == atomic_read(&hub_nmi->cpu_owner)) {
		atomic_set(&hub_nmi->cpu_owner, -1);
		atomic_set(&hub_nmi->in_nmi, 0);
		uv_local_mmr_clear_nmi();
		raw_spin_unlock(&hub_nmi->nmi_lock);
	}
}

/* Print non-responding cpus */
static void uv_nmi_nr_cpus_pr(char *fmt)
{
	static char cpu_list[1024];
	int len = sizeof(cpu_list);
	int c = cpumask_weight(uv_nmi_cpu_mask);
	int n = cpulist_scnprintf(cpu_list, len, uv_nmi_cpu_mask);

	if (n >= len-1)
		strcpy(&cpu_list[len - 6], "...\n");

	printk(fmt, c, cpu_list);
}

/* Ping non-responding cpus attemping to force them into the NMI handler */
static void uv_nmi_nr_cpus_ping(void)
{
	int cpu;

	for_each_cpu(cpu, uv_nmi_cpu_mask)
		atomic_set(&uv_cpu_nmi_per(cpu).pinging, 1);

	apic->send_IPI_mask(uv_nmi_cpu_mask, APIC_DM_NMI);
}

/* Clean up flags for cpus that ignored both NMI and ping */
static void uv_nmi_cleanup_mask(void)
{
	int cpu;

	for_each_cpu(cpu, uv_nmi_cpu_mask) {
		atomic_set(&uv_cpu_nmi_per(cpu).pinging, 0);
		atomic_set(&uv_cpu_nmi_per(cpu).state, UV_NMI_STATE_OUT);
		cpumask_clear_cpu(cpu, uv_nmi_cpu_mask);
	}
}

/* Loop waiting as cpus enter nmi handler */
static int uv_nmi_wait_cpus(int first)
{
	int i, j, k, n = num_online_cpus();
	int last_k = 0, waiting = 0;

	if (first) {
		cpumask_copy(uv_nmi_cpu_mask, cpu_online_mask);
		k = 0;
	} else {
		k = n - cpumask_weight(uv_nmi_cpu_mask);
	}

	udelay(uv_nmi_initial_delay);
	for (i = 0; i < uv_nmi_retry_count; i++) {
		int loop_delay = uv_nmi_loop_delay;

		for_each_cpu(j, uv_nmi_cpu_mask) {
			if (atomic_read(&uv_cpu_nmi_per(j).state)) {
				cpumask_clear_cpu(j, uv_nmi_cpu_mask);
				if (++k >= n)
					break;
			}
		}
		if (k >= n) {		/* all in? */
			k = n;
			break;
		}
		if (last_k != k) {	/* abort if no new cpus coming in */
			last_k = k;
			waiting = 0;
		} else if (++waiting > uv_nmi_wait_count)
			break;

		/* extend delay if waiting only for cpu 0 */
		if (waiting && (n - k) == 1 &&
		    cpumask_test_cpu(0, uv_nmi_cpu_mask))
			loop_delay *= 100;

		udelay(loop_delay);
	}
	atomic_set(&uv_nmi_cpus_in_nmi, k);
	return n - k;
}

/* Wait until all slave cpus have entered UV NMI handler */
static void uv_nmi_wait(int master)
{
	/* indicate this cpu is in */
	atomic_set(&uv_cpu_nmi.state, UV_NMI_STATE_IN);

	/* if not the first cpu in (the master), then we are a slave cpu */
	if (!master)
		return;

	do {
		/* wait for all other cpus to gather here */
		if (!uv_nmi_wait_cpus(1))
			break;

		/* if not all made it in, send IPI NMI to them */
		uv_nmi_nr_cpus_pr(KERN_ALERT
			"UV: Sending NMI IPI to %d non-responding CPUs: %s\n");
		uv_nmi_nr_cpus_ping();

		/* if all cpus are in, then done */
		if (!uv_nmi_wait_cpus(0))
			break;

		uv_nmi_nr_cpus_pr(KERN_ALERT
			"UV: %d CPUs not in NMI loop: %s\n");
	} while (0);

	pr_alert("UV: %d of %d CPUs in NMI\n",
		atomic_read(&uv_nmi_cpus_in_nmi), num_online_cpus());
}

static void uv_nmi_dump_cpu_ip_hdr(void)
{
	printk(KERN_DEFAULT
		"\nUV: %4s %6s %-32s %s   (Note: PID 0 not listed)\n",
		"CPU", "PID", "COMMAND", "IP");
}

static void uv_nmi_dump_cpu_ip(int cpu, struct pt_regs *regs)
{
	printk(KERN_DEFAULT "UV: %4d %6d %-32.32s ",
		cpu, current->pid, current->comm);

	printk_address(regs->ip, 1);
}

/* Dump this cpu's state */
static void uv_nmi_dump_state_cpu(int cpu, struct pt_regs *regs)
{
	const char *dots = " ................................. ";

	if (uv_nmi_action_is("ips")) {
		if (cpu == 0)
			uv_nmi_dump_cpu_ip_hdr();

		if (current->pid != 0)
			uv_nmi_dump_cpu_ip(cpu, regs);

	} else if (uv_nmi_action_is("dump")) {
		printk(KERN_DEFAULT
			"UV:%sNMI process trace for CPU %d\n", dots, cpu);
		show_regs(regs);
	}
	atomic_set(&uv_cpu_nmi.state, UV_NMI_STATE_DUMP_DONE);
}

/* Trigger a slave cpu to dump it's state */
static void uv_nmi_trigger_dump(int cpu)
{
	int retry = uv_nmi_trigger_delay;

	if (atomic_read(&uv_cpu_nmi_per(cpu).state) != UV_NMI_STATE_IN)
		return;

	atomic_set(&uv_cpu_nmi_per(cpu).state, UV_NMI_STATE_DUMP);
	do {
		cpu_relax();
		udelay(10);
		if (atomic_read(&uv_cpu_nmi_per(cpu).state)
				!= UV_NMI_STATE_DUMP)
			return;
	} while (--retry > 0);

	pr_crit("UV: CPU %d stuck in process dump function\n", cpu);
	atomic_set(&uv_cpu_nmi_per(cpu).state, UV_NMI_STATE_DUMP_DONE);
}

/* Wait until all cpus ready to exit */
static void uv_nmi_sync_exit(int master)
{
	atomic_dec(&uv_nmi_cpus_in_nmi);
	if (master) {
		while (atomic_read(&uv_nmi_cpus_in_nmi) > 0)
			cpu_relax();
		atomic_set(&uv_nmi_slave_continue, SLAVE_CLEAR);
	} else {
		while (atomic_read(&uv_nmi_slave_continue))
			cpu_relax();
	}
}

/* Walk through cpu list and dump state of each */
static void uv_nmi_dump_state(int cpu, struct pt_regs *regs, int master)
{
	if (master) {
		int tcpu;
		int ignored = 0;
		int saved_console_loglevel = console_loglevel;

		pr_alert("UV: tracing %s for %d CPUs from CPU %d\n",
			uv_nmi_action_is("ips") ? "IPs" : "processes",
			atomic_read(&uv_nmi_cpus_in_nmi), cpu);

		console_loglevel = uv_nmi_loglevel;
		atomic_set(&uv_nmi_slave_continue, SLAVE_EXIT);
		for_each_online_cpu(tcpu) {
			if (cpumask_test_cpu(tcpu, uv_nmi_cpu_mask))
				ignored++;
			else if (tcpu == cpu)
				uv_nmi_dump_state_cpu(tcpu, regs);
			else
				uv_nmi_trigger_dump(tcpu);
		}
		if (ignored)
			printk(KERN_DEFAULT "UV: %d CPUs ignored NMI\n",
				ignored);

		console_loglevel = saved_console_loglevel;
		pr_alert("UV: process trace complete\n");
	} else {
		while (!atomic_read(&uv_nmi_slave_continue))
			cpu_relax();
		while (atomic_read(&uv_cpu_nmi.state) != UV_NMI_STATE_DUMP)
			cpu_relax();
		uv_nmi_dump_state_cpu(cpu, regs);
	}
	uv_nmi_sync_exit(master);
}

static void uv_nmi_touch_watchdogs(void)
{
	touch_softlockup_watchdog_sync();
	clocksource_touch_watchdog();
	rcu_cpu_stall_reset();
	touch_nmi_watchdog();
}

#if defined(CONFIG_KEXEC)
static void uv_nmi_kdump(int cpu, int master, struct pt_regs *regs)
{
	/* Call crash to dump system state */
	if (master) {
		pr_emerg("UV: NMI executing crash_kexec on CPU%d\n", cpu);
		crash_kexec(regs);

		pr_emerg("UV: crash_kexec unexpectedly returned, ");
		if (!kexec_crash_image) {
			pr_cont("crash kernel not loaded\n");
			atomic_set(&uv_nmi_kexec_failed, 1);
			uv_nmi_sync_exit(1);
			return;
		}
		pr_cont("kexec busy, stalling cpus while waiting\n");
	}

	/* If crash exec fails the slaves should return, otherwise stall */
	while (atomic_read(&uv_nmi_kexec_failed) == 0)
		mdelay(10);

	/* Crash kernel most likely not loaded, return in an orderly fashion */
	uv_nmi_sync_exit(0);
}

#else /* !CONFIG_KEXEC */
static inline void uv_nmi_kdump(int cpu, int master, struct pt_regs *regs)
{
	if (master)
		pr_err("UV: NMI kdump: KEXEC not supported in this kernel\n");
}
#endif /* !CONFIG_KEXEC */

#ifdef CONFIG_KGDB_KDB
/* Call KDB from NMI handler */
static void uv_call_kdb(int cpu, struct pt_regs *regs, int master)
{
	int ret;

	if (master) {
		/* call KGDB NMI handler as MASTER */
		ret = kgdb_nmicallin(cpu, X86_TRAP_NMI, regs,
					&uv_nmi_slave_continue);
		if (ret) {
			pr_alert("KDB returned error, is kgdboc set?\n");
			atomic_set(&uv_nmi_slave_continue, SLAVE_EXIT);
		}
	} else {
		/* wait for KGDB signal that it's ready for slaves to enter */
		int sig;

		do {
			cpu_relax();
			sig = atomic_read(&uv_nmi_slave_continue);
		} while (!sig);

		/* call KGDB as slave */
		if (sig == SLAVE_CONTINUE)
			kgdb_nmicallback(cpu, regs);
	}
	uv_nmi_sync_exit(master);
}

#else /* !CONFIG_KGDB_KDB */
static inline void uv_call_kdb(int cpu, struct pt_regs *regs, int master)
{
	pr_err("UV: NMI error: KGDB/KDB is not enabled in this kernel\n");
}
#endif /* !CONFIG_KGDB_KDB */

/*
 * UV NMI handler
 */
int uv_handle_nmi(unsigned int reason, struct pt_regs *regs)
{
	struct uv_hub_nmi_s *hub_nmi = uv_hub_nmi;
	int cpu = smp_processor_id();
	int master = 0;
	unsigned long flags;

	local_irq_save(flags);

	/* If not a UV System NMI, ignore */
	if (!atomic_read(&uv_cpu_nmi.pinging) && !uv_check_nmi(hub_nmi)) {
		local_irq_restore(flags);
		return NMI_DONE;
	}

	/* Indicate we are the first CPU into the NMI handler */
	master = (atomic_read(&uv_nmi_cpu) == cpu);

	/* If NMI action is "kdump", then attempt to do it */
	if (uv_nmi_action_is("kdump"))
		uv_nmi_kdump(cpu, master, regs);

	/* Pause as all cpus enter the NMI handler */
	uv_nmi_wait(master);

	/* Dump state of each cpu */
	if (uv_nmi_action_is("ips") || uv_nmi_action_is("dump"))
		uv_nmi_dump_state(cpu, regs, master);

	/* Call KDB if enabled */
	else if (uv_nmi_action_is("kdb"))
		uv_call_kdb(cpu, regs, master);

	/* Clear per_cpu "in nmi" flag */
	atomic_set(&uv_cpu_nmi.state, UV_NMI_STATE_OUT);

	/* Clear MMR NMI flag on each hub */
	uv_clear_nmi(cpu);

	/* Clear global flags */
	if (master) {
		if (cpumask_weight(uv_nmi_cpu_mask))
			uv_nmi_cleanup_mask();
		atomic_set(&uv_nmi_cpus_in_nmi, -1);
		atomic_set(&uv_nmi_cpu, -1);
		atomic_set(&uv_in_nmi, 0);
	}

	uv_nmi_touch_watchdogs();
	local_irq_restore(flags);

	return NMI_HANDLED;
}

/*
 * NMI handler for pulling in CPUs when perf events are grabbing our NMI
 */
int uv_handle_nmi_ping(unsigned int reason, struct pt_regs *regs)
{
	int ret;

	uv_cpu_nmi.queries++;
	if (!atomic_read(&uv_cpu_nmi.pinging)) {
		local64_inc(&uv_nmi_ping_misses);
		return NMI_DONE;
	}

	uv_cpu_nmi.pings++;
	local64_inc(&uv_nmi_ping_count);
	ret = uv_handle_nmi(reason, regs);
	atomic_set(&uv_cpu_nmi.pinging, 0);
	return ret;
}

void uv_register_nmi_notifier(void)
{
	if (register_nmi_handler(NMI_UNKNOWN, uv_handle_nmi, 0, "uv"))
		pr_warn("UV: NMI handler failed to register\n");

	if (register_nmi_handler(NMI_LOCAL, uv_handle_nmi_ping, 0, "uvping"))
		pr_warn("UV: PING NMI handler failed to register\n");
}

void uv_nmi_init(void)
{
	unsigned int value;

	/*
	 * Unmask NMI on all cpus
	 */
	value = apic_read(APIC_LVT1) | APIC_DM_NMI;
	value &= ~APIC_LVT_MASKED;
	apic_write(APIC_LVT1, value);
}

void uv_nmi_setup(void)
{
	int size = sizeof(void *) * (1 << NODES_SHIFT);
	int cpu, nid;

	/* Setup hub nmi info */
	uv_nmi_setup_mmrs();
	uv_hub_nmi_list = kzalloc(size, GFP_KERNEL);
	pr_info("UV: NMI hub list @ 0x%p (%d)\n", uv_hub_nmi_list, size);
	BUG_ON(!uv_hub_nmi_list);
	size = sizeof(struct uv_hub_nmi_s);
	for_each_present_cpu(cpu) {
		nid = cpu_to_node(cpu);
		if (uv_hub_nmi_list[nid] == NULL) {
			uv_hub_nmi_list[nid] = kzalloc_node(size,
							    GFP_KERNEL, nid);
			BUG_ON(!uv_hub_nmi_list[nid]);
			raw_spin_lock_init(&(uv_hub_nmi_list[nid]->nmi_lock));
			atomic_set(&uv_hub_nmi_list[nid]->cpu_owner, -1);
		}
		uv_hub_nmi_per(cpu) = uv_hub_nmi_list[nid];
	}
	BUG_ON(!alloc_cpumask_var(&uv_nmi_cpu_mask, GFP_KERNEL));
}


