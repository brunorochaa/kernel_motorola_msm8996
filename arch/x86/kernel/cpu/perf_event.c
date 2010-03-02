/*
 * Performance events x86 architecture code
 *
 *  Copyright (C) 2008 Thomas Gleixner <tglx@linutronix.de>
 *  Copyright (C) 2008-2009 Red Hat, Inc., Ingo Molnar
 *  Copyright (C) 2009 Jaswinder Singh Rajput
 *  Copyright (C) 2009 Advanced Micro Devices, Inc., Robert Richter
 *  Copyright (C) 2008-2009 Red Hat, Inc., Peter Zijlstra <pzijlstr@redhat.com>
 *  Copyright (C) 2009 Intel Corporation, <markus.t.metzger@intel.com>
 *  Copyright (C) 2009 Google, Inc., Stephane Eranian
 *
 *  For licencing details see kernel-base/COPYING
 */

#include <linux/perf_event.h>
#include <linux/capability.h>
#include <linux/notifier.h>
#include <linux/hardirq.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/kdebug.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <linux/cpu.h>
#include <linux/bitops.h>

#include <asm/apic.h>
#include <asm/stacktrace.h>
#include <asm/nmi.h>

static u64 perf_event_mask __read_mostly;

/* The maximal number of PEBS events: */
#define MAX_PEBS_EVENTS	4

/* The size of a BTS record in bytes: */
#define BTS_RECORD_SIZE		24

/* The size of a per-cpu BTS buffer in bytes: */
#define BTS_BUFFER_SIZE		(BTS_RECORD_SIZE * 2048)

/* The BTS overflow threshold in bytes from the end of the buffer: */
#define BTS_OVFL_TH		(BTS_RECORD_SIZE * 128)


/*
 * Bits in the debugctlmsr controlling branch tracing.
 */
#define X86_DEBUGCTL_TR			(1 << 6)
#define X86_DEBUGCTL_BTS		(1 << 7)
#define X86_DEBUGCTL_BTINT		(1 << 8)
#define X86_DEBUGCTL_BTS_OFF_OS		(1 << 9)
#define X86_DEBUGCTL_BTS_OFF_USR	(1 << 10)

/*
 * A debug store configuration.
 *
 * We only support architectures that use 64bit fields.
 */
struct debug_store {
	u64	bts_buffer_base;
	u64	bts_index;
	u64	bts_absolute_maximum;
	u64	bts_interrupt_threshold;
	u64	pebs_buffer_base;
	u64	pebs_index;
	u64	pebs_absolute_maximum;
	u64	pebs_interrupt_threshold;
	u64	pebs_event_reset[MAX_PEBS_EVENTS];
};

struct event_constraint {
	union {
		unsigned long	idxmsk[BITS_TO_LONGS(X86_PMC_IDX_MAX)];
		u64		idxmsk64;
	};
	u64	code;
	u64	cmask;
	int	weight;
};

struct amd_nb {
	int nb_id;  /* NorthBridge id */
	int refcnt; /* reference count */
	struct perf_event *owners[X86_PMC_IDX_MAX];
	struct event_constraint event_constraints[X86_PMC_IDX_MAX];
};

struct cpu_hw_events {
	struct perf_event	*events[X86_PMC_IDX_MAX]; /* in counter order */
	unsigned long		active_mask[BITS_TO_LONGS(X86_PMC_IDX_MAX)];
	unsigned long		interrupts;
	int			enabled;
	struct debug_store	*ds;

	int			n_events;
	int			n_added;
	int			assign[X86_PMC_IDX_MAX]; /* event to counter assignment */
	u64			tags[X86_PMC_IDX_MAX];
	struct perf_event	*event_list[X86_PMC_IDX_MAX]; /* in enabled order */
	struct amd_nb		*amd_nb;
};

#define __EVENT_CONSTRAINT(c, n, m, w) {\
	{ .idxmsk64 = (n) },		\
	.code = (c),			\
	.cmask = (m),			\
	.weight = (w),			\
}

#define EVENT_CONSTRAINT(c, n, m)	\
	__EVENT_CONSTRAINT(c, n, m, HWEIGHT(n))

#define INTEL_EVENT_CONSTRAINT(c, n)	\
	EVENT_CONSTRAINT(c, n, INTEL_ARCH_EVTSEL_MASK)

#define FIXED_EVENT_CONSTRAINT(c, n)	\
	EVENT_CONSTRAINT(c, (1ULL << (32+n)), INTEL_ARCH_FIXED_MASK)

#define EVENT_CONSTRAINT_END		\
	EVENT_CONSTRAINT(0, 0, 0)

#define for_each_event_constraint(e, c)	\
	for ((e) = (c); (e)->cmask; (e)++)

/*
 * struct x86_pmu - generic x86 pmu
 */
struct x86_pmu {
	const char	*name;
	int		version;
	int		(*handle_irq)(struct pt_regs *);
	void		(*disable_all)(void);
	void		(*enable_all)(void);
	void		(*enable)(struct perf_event *);
	void		(*disable)(struct perf_event *);
	unsigned	eventsel;
	unsigned	perfctr;
	u64		(*event_map)(int);
	u64		(*raw_event)(u64);
	int		max_events;
	int		num_events;
	int		num_events_fixed;
	int		event_bits;
	u64		event_mask;
	int		apic;
	u64		max_period;
	u64		intel_ctrl;
	void		(*enable_bts)(u64 config);
	void		(*disable_bts)(void);

	struct event_constraint *
			(*get_event_constraints)(struct cpu_hw_events *cpuc,
						 struct perf_event *event);

	void		(*put_event_constraints)(struct cpu_hw_events *cpuc,
						 struct perf_event *event);
	struct event_constraint *event_constraints;

	void		(*cpu_prepare)(int cpu);
	void		(*cpu_starting)(int cpu);
	void		(*cpu_dying)(int cpu);
	void		(*cpu_dead)(int cpu);
};

static struct x86_pmu x86_pmu __read_mostly;

static DEFINE_PER_CPU(struct cpu_hw_events, cpu_hw_events) = {
	.enabled = 1,
};

static int x86_perf_event_set_period(struct perf_event *event);

/*
 * Generalized hw caching related hw_event table, filled
 * in on a per model basis. A value of 0 means
 * 'not supported', -1 means 'hw_event makes no sense on
 * this CPU', any other value means the raw hw_event
 * ID.
 */

#define C(x) PERF_COUNT_HW_CACHE_##x

static u64 __read_mostly hw_cache_event_ids
				[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX];

/*
 * Propagate event elapsed time into the generic event.
 * Can only be executed on the CPU where the event is active.
 * Returns the delta events processed.
 */
static u64
x86_perf_event_update(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	int shift = 64 - x86_pmu.event_bits;
	u64 prev_raw_count, new_raw_count;
	int idx = hwc->idx;
	s64 delta;

	if (idx == X86_PMC_IDX_FIXED_BTS)
		return 0;

	/*
	 * Careful: an NMI might modify the previous event value.
	 *
	 * Our tactic to handle this is to first atomically read and
	 * exchange a new raw count - then add that new-prev delta
	 * count to the generic event atomically:
	 */
again:
	prev_raw_count = atomic64_read(&hwc->prev_count);
	rdmsrl(hwc->event_base + idx, new_raw_count);

	if (atomic64_cmpxchg(&hwc->prev_count, prev_raw_count,
					new_raw_count) != prev_raw_count)
		goto again;

	/*
	 * Now we have the new raw value and have updated the prev
	 * timestamp already. We can now calculate the elapsed delta
	 * (event-)time and add that to the generic event.
	 *
	 * Careful, not all hw sign-extends above the physical width
	 * of the count.
	 */
	delta = (new_raw_count << shift) - (prev_raw_count << shift);
	delta >>= shift;

	atomic64_add(delta, &event->count);
	atomic64_sub(delta, &hwc->period_left);

	return new_raw_count;
}

static atomic_t active_events;
static DEFINE_MUTEX(pmc_reserve_mutex);

static bool reserve_pmc_hardware(void)
{
#ifdef CONFIG_X86_LOCAL_APIC
	int i;

	if (nmi_watchdog == NMI_LOCAL_APIC)
		disable_lapic_nmi_watchdog();

	for (i = 0; i < x86_pmu.num_events; i++) {
		if (!reserve_perfctr_nmi(x86_pmu.perfctr + i))
			goto perfctr_fail;
	}

	for (i = 0; i < x86_pmu.num_events; i++) {
		if (!reserve_evntsel_nmi(x86_pmu.eventsel + i))
			goto eventsel_fail;
	}
#endif

	return true;

#ifdef CONFIG_X86_LOCAL_APIC
eventsel_fail:
	for (i--; i >= 0; i--)
		release_evntsel_nmi(x86_pmu.eventsel + i);

	i = x86_pmu.num_events;

perfctr_fail:
	for (i--; i >= 0; i--)
		release_perfctr_nmi(x86_pmu.perfctr + i);

	if (nmi_watchdog == NMI_LOCAL_APIC)
		enable_lapic_nmi_watchdog();

	return false;
#endif
}

static void release_pmc_hardware(void)
{
#ifdef CONFIG_X86_LOCAL_APIC
	int i;

	for (i = 0; i < x86_pmu.num_events; i++) {
		release_perfctr_nmi(x86_pmu.perfctr + i);
		release_evntsel_nmi(x86_pmu.eventsel + i);
	}

	if (nmi_watchdog == NMI_LOCAL_APIC)
		enable_lapic_nmi_watchdog();
#endif
}

static inline bool bts_available(void)
{
	return x86_pmu.enable_bts != NULL;
}

static void init_debug_store_on_cpu(int cpu)
{
	struct debug_store *ds = per_cpu(cpu_hw_events, cpu).ds;

	if (!ds)
		return;

	wrmsr_on_cpu(cpu, MSR_IA32_DS_AREA,
		     (u32)((u64)(unsigned long)ds),
		     (u32)((u64)(unsigned long)ds >> 32));
}

static void fini_debug_store_on_cpu(int cpu)
{
	if (!per_cpu(cpu_hw_events, cpu).ds)
		return;

	wrmsr_on_cpu(cpu, MSR_IA32_DS_AREA, 0, 0);
}

static void release_bts_hardware(void)
{
	int cpu;

	if (!bts_available())
		return;

	get_online_cpus();

	for_each_online_cpu(cpu)
		fini_debug_store_on_cpu(cpu);

	for_each_possible_cpu(cpu) {
		struct debug_store *ds = per_cpu(cpu_hw_events, cpu).ds;

		if (!ds)
			continue;

		per_cpu(cpu_hw_events, cpu).ds = NULL;

		kfree((void *)(unsigned long)ds->bts_buffer_base);
		kfree(ds);
	}

	put_online_cpus();
}

static int reserve_bts_hardware(void)
{
	int cpu, err = 0;

	if (!bts_available())
		return 0;

	get_online_cpus();

	for_each_possible_cpu(cpu) {
		struct debug_store *ds;
		void *buffer;

		err = -ENOMEM;
		buffer = kzalloc(BTS_BUFFER_SIZE, GFP_KERNEL);
		if (unlikely(!buffer))
			break;

		ds = kzalloc(sizeof(*ds), GFP_KERNEL);
		if (unlikely(!ds)) {
			kfree(buffer);
			break;
		}

		ds->bts_buffer_base = (u64)(unsigned long)buffer;
		ds->bts_index = ds->bts_buffer_base;
		ds->bts_absolute_maximum =
			ds->bts_buffer_base + BTS_BUFFER_SIZE;
		ds->bts_interrupt_threshold =
			ds->bts_absolute_maximum - BTS_OVFL_TH;

		per_cpu(cpu_hw_events, cpu).ds = ds;
		err = 0;
	}

	if (err)
		release_bts_hardware();
	else {
		for_each_online_cpu(cpu)
			init_debug_store_on_cpu(cpu);
	}

	put_online_cpus();

	return err;
}

static void hw_perf_event_destroy(struct perf_event *event)
{
	if (atomic_dec_and_mutex_lock(&active_events, &pmc_reserve_mutex)) {
		release_pmc_hardware();
		release_bts_hardware();
		mutex_unlock(&pmc_reserve_mutex);
	}
}

static inline int x86_pmu_initialized(void)
{
	return x86_pmu.handle_irq != NULL;
}

static inline int
set_ext_hw_attr(struct hw_perf_event *hwc, struct perf_event_attr *attr)
{
	unsigned int cache_type, cache_op, cache_result;
	u64 config, val;

	config = attr->config;

	cache_type = (config >>  0) & 0xff;
	if (cache_type >= PERF_COUNT_HW_CACHE_MAX)
		return -EINVAL;

	cache_op = (config >>  8) & 0xff;
	if (cache_op >= PERF_COUNT_HW_CACHE_OP_MAX)
		return -EINVAL;

	cache_result = (config >> 16) & 0xff;
	if (cache_result >= PERF_COUNT_HW_CACHE_RESULT_MAX)
		return -EINVAL;

	val = hw_cache_event_ids[cache_type][cache_op][cache_result];

	if (val == 0)
		return -ENOENT;

	if (val == -1)
		return -EINVAL;

	hwc->config |= val;

	return 0;
}

/*
 * Setup the hardware configuration for a given attr_type
 */
static int __hw_perf_event_init(struct perf_event *event)
{
	struct perf_event_attr *attr = &event->attr;
	struct hw_perf_event *hwc = &event->hw;
	u64 config;
	int err;

	if (!x86_pmu_initialized())
		return -ENODEV;

	err = 0;
	if (!atomic_inc_not_zero(&active_events)) {
		mutex_lock(&pmc_reserve_mutex);
		if (atomic_read(&active_events) == 0) {
			if (!reserve_pmc_hardware())
				err = -EBUSY;
			else
				err = reserve_bts_hardware();
		}
		if (!err)
			atomic_inc(&active_events);
		mutex_unlock(&pmc_reserve_mutex);
	}
	if (err)
		return err;

	event->destroy = hw_perf_event_destroy;

	/*
	 * Generate PMC IRQs:
	 * (keep 'enabled' bit clear for now)
	 */
	hwc->config = ARCH_PERFMON_EVENTSEL_INT;

	hwc->idx = -1;
	hwc->last_cpu = -1;
	hwc->last_tag = ~0ULL;

	/*
	 * Count user and OS events unless requested not to.
	 */
	if (!attr->exclude_user)
		hwc->config |= ARCH_PERFMON_EVENTSEL_USR;
	if (!attr->exclude_kernel)
		hwc->config |= ARCH_PERFMON_EVENTSEL_OS;

	if (!hwc->sample_period) {
		hwc->sample_period = x86_pmu.max_period;
		hwc->last_period = hwc->sample_period;
		atomic64_set(&hwc->period_left, hwc->sample_period);
	} else {
		/*
		 * If we have a PMU initialized but no APIC
		 * interrupts, we cannot sample hardware
		 * events (user-space has to fall back and
		 * sample via a hrtimer based software event):
		 */
		if (!x86_pmu.apic)
			return -EOPNOTSUPP;
	}

	/*
	 * Raw hw_event type provide the config in the hw_event structure
	 */
	if (attr->type == PERF_TYPE_RAW) {
		hwc->config |= x86_pmu.raw_event(attr->config);
		if ((hwc->config & ARCH_PERFMON_EVENTSEL_ANY) &&
		    perf_paranoid_cpu() && !capable(CAP_SYS_ADMIN))
			return -EACCES;
		return 0;
	}

	if (attr->type == PERF_TYPE_HW_CACHE)
		return set_ext_hw_attr(hwc, attr);

	if (attr->config >= x86_pmu.max_events)
		return -EINVAL;

	/*
	 * The generic map:
	 */
	config = x86_pmu.event_map(attr->config);

	if (config == 0)
		return -ENOENT;

	if (config == -1LL)
		return -EINVAL;

	/*
	 * Branch tracing:
	 */
	if ((attr->config == PERF_COUNT_HW_BRANCH_INSTRUCTIONS) &&
	    (hwc->sample_period == 1)) {
		/* BTS is not supported by this architecture. */
		if (!bts_available())
			return -EOPNOTSUPP;

		/* BTS is currently only allowed for user-mode. */
		if (hwc->config & ARCH_PERFMON_EVENTSEL_OS)
			return -EOPNOTSUPP;
	}

	hwc->config |= config;

	return 0;
}

static void x86_pmu_disable_all(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	int idx;

	for (idx = 0; idx < x86_pmu.num_events; idx++) {
		u64 val;

		if (!test_bit(idx, cpuc->active_mask))
			continue;
		rdmsrl(x86_pmu.eventsel + idx, val);
		if (!(val & ARCH_PERFMON_EVENTSEL_ENABLE))
			continue;
		val &= ~ARCH_PERFMON_EVENTSEL_ENABLE;
		wrmsrl(x86_pmu.eventsel + idx, val);
	}
}

void hw_perf_disable(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);

	if (!x86_pmu_initialized())
		return;

	if (!cpuc->enabled)
		return;

	cpuc->n_added = 0;
	cpuc->enabled = 0;
	barrier();

	x86_pmu.disable_all();
}

static void x86_pmu_enable_all(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	int idx;

	for (idx = 0; idx < x86_pmu.num_events; idx++) {
		struct perf_event *event = cpuc->events[idx];
		u64 val;

		if (!test_bit(idx, cpuc->active_mask))
			continue;

		val = event->hw.config;
		val |= ARCH_PERFMON_EVENTSEL_ENABLE;
		wrmsrl(x86_pmu.eventsel + idx, val);
	}
}

static const struct pmu pmu;

static inline int is_x86_event(struct perf_event *event)
{
	return event->pmu == &pmu;
}

static int x86_schedule_events(struct cpu_hw_events *cpuc, int n, int *assign)
{
	struct event_constraint *c, *constraints[X86_PMC_IDX_MAX];
	unsigned long used_mask[BITS_TO_LONGS(X86_PMC_IDX_MAX)];
	int i, j, w, wmax, num = 0;
	struct hw_perf_event *hwc;

	bitmap_zero(used_mask, X86_PMC_IDX_MAX);

	for (i = 0; i < n; i++) {
		c = x86_pmu.get_event_constraints(cpuc, cpuc->event_list[i]);
		constraints[i] = c;
	}

	/*
	 * fastpath, try to reuse previous register
	 */
	for (i = 0; i < n; i++) {
		hwc = &cpuc->event_list[i]->hw;
		c = constraints[i];

		/* never assigned */
		if (hwc->idx == -1)
			break;

		/* constraint still honored */
		if (!test_bit(hwc->idx, c->idxmsk))
			break;

		/* not already used */
		if (test_bit(hwc->idx, used_mask))
			break;

		set_bit(hwc->idx, used_mask);
		if (assign)
			assign[i] = hwc->idx;
	}
	if (i == n)
		goto done;

	/*
	 * begin slow path
	 */

	bitmap_zero(used_mask, X86_PMC_IDX_MAX);

	/*
	 * weight = number of possible counters
	 *
	 * 1    = most constrained, only works on one counter
	 * wmax = least constrained, works on any counter
	 *
	 * assign events to counters starting with most
	 * constrained events.
	 */
	wmax = x86_pmu.num_events;

	/*
	 * when fixed event counters are present,
	 * wmax is incremented by 1 to account
	 * for one more choice
	 */
	if (x86_pmu.num_events_fixed)
		wmax++;

	for (w = 1, num = n; num && w <= wmax; w++) {
		/* for each event */
		for (i = 0; num && i < n; i++) {
			c = constraints[i];
			hwc = &cpuc->event_list[i]->hw;

			if (c->weight != w)
				continue;

			for_each_set_bit(j, c->idxmsk, X86_PMC_IDX_MAX) {
				if (!test_bit(j, used_mask))
					break;
			}

			if (j == X86_PMC_IDX_MAX)
				break;

			set_bit(j, used_mask);

			if (assign)
				assign[i] = j;
			num--;
		}
	}
done:
	/*
	 * scheduling failed or is just a simulation,
	 * free resources if necessary
	 */
	if (!assign || num) {
		for (i = 0; i < n; i++) {
			if (x86_pmu.put_event_constraints)
				x86_pmu.put_event_constraints(cpuc, cpuc->event_list[i]);
		}
	}
	return num ? -ENOSPC : 0;
}

/*
 * dogrp: true if must collect siblings events (group)
 * returns total number of events and error code
 */
static int collect_events(struct cpu_hw_events *cpuc, struct perf_event *leader, bool dogrp)
{
	struct perf_event *event;
	int n, max_count;

	max_count = x86_pmu.num_events + x86_pmu.num_events_fixed;

	/* current number of events already accepted */
	n = cpuc->n_events;

	if (is_x86_event(leader)) {
		if (n >= max_count)
			return -ENOSPC;
		cpuc->event_list[n] = leader;
		n++;
	}
	if (!dogrp)
		return n;

	list_for_each_entry(event, &leader->sibling_list, group_entry) {
		if (!is_x86_event(event) ||
		    event->state <= PERF_EVENT_STATE_OFF)
			continue;

		if (n >= max_count)
			return -ENOSPC;

		cpuc->event_list[n] = event;
		n++;
	}
	return n;
}

static inline void x86_assign_hw_event(struct perf_event *event,
				struct cpu_hw_events *cpuc, int i)
{
	struct hw_perf_event *hwc = &event->hw;

	hwc->idx = cpuc->assign[i];
	hwc->last_cpu = smp_processor_id();
	hwc->last_tag = ++cpuc->tags[i];

	if (hwc->idx == X86_PMC_IDX_FIXED_BTS) {
		hwc->config_base = 0;
		hwc->event_base	= 0;
	} else if (hwc->idx >= X86_PMC_IDX_FIXED) {
		hwc->config_base = MSR_ARCH_PERFMON_FIXED_CTR_CTRL;
		/*
		 * We set it so that event_base + idx in wrmsr/rdmsr maps to
		 * MSR_ARCH_PERFMON_FIXED_CTR0 ... CTR2:
		 */
		hwc->event_base =
			MSR_ARCH_PERFMON_FIXED_CTR0 - X86_PMC_IDX_FIXED;
	} else {
		hwc->config_base = x86_pmu.eventsel;
		hwc->event_base  = x86_pmu.perfctr;
	}
}

static inline int match_prev_assignment(struct hw_perf_event *hwc,
					struct cpu_hw_events *cpuc,
					int i)
{
	return hwc->idx == cpuc->assign[i] &&
		hwc->last_cpu == smp_processor_id() &&
		hwc->last_tag == cpuc->tags[i];
}

static void x86_pmu_stop(struct perf_event *event);

void hw_perf_enable(void)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	struct perf_event *event;
	struct hw_perf_event *hwc;
	int i;

	if (!x86_pmu_initialized())
		return;

	if (cpuc->enabled)
		return;

	if (cpuc->n_added) {
		/*
		 * apply assignment obtained either from
		 * hw_perf_group_sched_in() or x86_pmu_enable()
		 *
		 * step1: save events moving to new counters
		 * step2: reprogram moved events into new counters
		 */
		for (i = 0; i < cpuc->n_events; i++) {

			event = cpuc->event_list[i];
			hwc = &event->hw;

			/*
			 * we can avoid reprogramming counter if:
			 * - assigned same counter as last time
			 * - running on same CPU as last time
			 * - no other event has used the counter since
			 */
			if (hwc->idx == -1 ||
			    match_prev_assignment(hwc, cpuc, i))
				continue;

			x86_pmu_stop(event);

			hwc->idx = -1;
		}

		for (i = 0; i < cpuc->n_events; i++) {

			event = cpuc->event_list[i];
			hwc = &event->hw;

			if (hwc->idx == -1) {
				x86_assign_hw_event(event, cpuc, i);
				x86_perf_event_set_period(event);
			}
			/*
			 * need to mark as active because x86_pmu_disable()
			 * clear active_mask and events[] yet it preserves
			 * idx
			 */
			set_bit(hwc->idx, cpuc->active_mask);
			cpuc->events[hwc->idx] = event;

			x86_pmu.enable(event);
			perf_event_update_userpage(event);
		}
		cpuc->n_added = 0;
		perf_events_lapic_init();
	}

	cpuc->enabled = 1;
	barrier();

	x86_pmu.enable_all();
}

static inline void __x86_pmu_enable_event(struct hw_perf_event *hwc)
{
	(void)checking_wrmsrl(hwc->config_base + hwc->idx,
			      hwc->config | ARCH_PERFMON_EVENTSEL_ENABLE);
}

static inline void x86_pmu_disable_event(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	(void)checking_wrmsrl(hwc->config_base + hwc->idx, hwc->config);
}

static DEFINE_PER_CPU(u64 [X86_PMC_IDX_MAX], pmc_prev_left);

/*
 * Set the next IRQ period, based on the hwc->period_left value.
 * To be called with the event disabled in hw:
 */
static int
x86_perf_event_set_period(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;
	s64 left = atomic64_read(&hwc->period_left);
	s64 period = hwc->sample_period;
	int err, ret = 0, idx = hwc->idx;

	if (idx == X86_PMC_IDX_FIXED_BTS)
		return 0;

	/*
	 * If we are way outside a reasonable range then just skip forward:
	 */
	if (unlikely(left <= -period)) {
		left = period;
		atomic64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (unlikely(left <= 0)) {
		left += period;
		atomic64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}
	/*
	 * Quirk: certain CPUs dont like it if just 1 hw_event is left:
	 */
	if (unlikely(left < 2))
		left = 2;

	if (left > x86_pmu.max_period)
		left = x86_pmu.max_period;

	per_cpu(pmc_prev_left[idx], smp_processor_id()) = left;

	/*
	 * The hw event starts counting from this event offset,
	 * mark it to be able to extra future deltas:
	 */
	atomic64_set(&hwc->prev_count, (u64)-left);

	err = checking_wrmsrl(hwc->event_base + idx,
			     (u64)(-left) & x86_pmu.event_mask);

	perf_event_update_userpage(event);

	return ret;
}

static void x86_pmu_enable_event(struct perf_event *event)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	if (cpuc->enabled)
		__x86_pmu_enable_event(&event->hw);
}

/*
 * activate a single event
 *
 * The event is added to the group of enabled events
 * but only if it can be scehduled with existing events.
 *
 * Called with PMU disabled. If successful and return value 1,
 * then guaranteed to call perf_enable() and hw_perf_enable()
 */
static int x86_pmu_enable(struct perf_event *event)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	struct hw_perf_event *hwc;
	int assign[X86_PMC_IDX_MAX];
	int n, n0, ret;

	hwc = &event->hw;

	n0 = cpuc->n_events;
	n = collect_events(cpuc, event, false);
	if (n < 0)
		return n;

	ret = x86_schedule_events(cpuc, n, assign);
	if (ret)
		return ret;
	/*
	 * copy new assignment, now we know it is possible
	 * will be used by hw_perf_enable()
	 */
	memcpy(cpuc->assign, assign, n*sizeof(int));

	cpuc->n_events = n;
	cpuc->n_added  = n - n0;

	return 0;
}

static int x86_pmu_start(struct perf_event *event)
{
	if (event->hw.idx == -1)
		return -EAGAIN;

	x86_perf_event_set_period(event);
	x86_pmu.enable(event);

	return 0;
}

static void x86_pmu_unthrottle(struct perf_event *event)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;

	if (WARN_ON_ONCE(hwc->idx >= X86_PMC_IDX_MAX ||
				cpuc->events[hwc->idx] != event))
		return;

	x86_pmu.enable(event);
}

void perf_event_print_debug(void)
{
	u64 ctrl, status, overflow, pmc_ctrl, pmc_count, prev_left, fixed;
	struct cpu_hw_events *cpuc;
	unsigned long flags;
	int cpu, idx;

	if (!x86_pmu.num_events)
		return;

	local_irq_save(flags);

	cpu = smp_processor_id();
	cpuc = &per_cpu(cpu_hw_events, cpu);

	if (x86_pmu.version >= 2) {
		rdmsrl(MSR_CORE_PERF_GLOBAL_CTRL, ctrl);
		rdmsrl(MSR_CORE_PERF_GLOBAL_STATUS, status);
		rdmsrl(MSR_CORE_PERF_GLOBAL_OVF_CTRL, overflow);
		rdmsrl(MSR_ARCH_PERFMON_FIXED_CTR_CTRL, fixed);

		pr_info("\n");
		pr_info("CPU#%d: ctrl:       %016llx\n", cpu, ctrl);
		pr_info("CPU#%d: status:     %016llx\n", cpu, status);
		pr_info("CPU#%d: overflow:   %016llx\n", cpu, overflow);
		pr_info("CPU#%d: fixed:      %016llx\n", cpu, fixed);
	}
	pr_info("CPU#%d: active:       %016llx\n", cpu, *(u64 *)cpuc->active_mask);

	for (idx = 0; idx < x86_pmu.num_events; idx++) {
		rdmsrl(x86_pmu.eventsel + idx, pmc_ctrl);
		rdmsrl(x86_pmu.perfctr  + idx, pmc_count);

		prev_left = per_cpu(pmc_prev_left[idx], cpu);

		pr_info("CPU#%d:   gen-PMC%d ctrl:  %016llx\n",
			cpu, idx, pmc_ctrl);
		pr_info("CPU#%d:   gen-PMC%d count: %016llx\n",
			cpu, idx, pmc_count);
		pr_info("CPU#%d:   gen-PMC%d left:  %016llx\n",
			cpu, idx, prev_left);
	}
	for (idx = 0; idx < x86_pmu.num_events_fixed; idx++) {
		rdmsrl(MSR_ARCH_PERFMON_FIXED_CTR0 + idx, pmc_count);

		pr_info("CPU#%d: fixed-PMC%d count: %016llx\n",
			cpu, idx, pmc_count);
	}
	local_irq_restore(flags);
}

static void x86_pmu_stop(struct perf_event *event)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	/*
	 * Must be done before we disable, otherwise the nmi handler
	 * could reenable again:
	 */
	clear_bit(idx, cpuc->active_mask);
	x86_pmu.disable(event);

	/*
	 * Drain the remaining delta count out of a event
	 * that we are disabling:
	 */
	x86_perf_event_update(event);

	cpuc->events[idx] = NULL;
}

static void x86_pmu_disable(struct perf_event *event)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	int i;

	x86_pmu_stop(event);

	for (i = 0; i < cpuc->n_events; i++) {
		if (event == cpuc->event_list[i]) {

			if (x86_pmu.put_event_constraints)
				x86_pmu.put_event_constraints(cpuc, event);

			while (++i < cpuc->n_events)
				cpuc->event_list[i-1] = cpuc->event_list[i];

			--cpuc->n_events;
			break;
		}
	}
	perf_event_update_userpage(event);
}

static int x86_pmu_handle_irq(struct pt_regs *regs)
{
	struct perf_sample_data data;
	struct cpu_hw_events *cpuc;
	struct perf_event *event;
	struct hw_perf_event *hwc;
	int idx, handled = 0;
	u64 val;

	perf_sample_data_init(&data, 0);

	cpuc = &__get_cpu_var(cpu_hw_events);

	for (idx = 0; idx < x86_pmu.num_events; idx++) {
		if (!test_bit(idx, cpuc->active_mask))
			continue;

		event = cpuc->events[idx];
		hwc = &event->hw;

		val = x86_perf_event_update(event);
		if (val & (1ULL << (x86_pmu.event_bits - 1)))
			continue;

		/*
		 * event overflow
		 */
		handled		= 1;
		data.period	= event->hw.last_period;

		if (!x86_perf_event_set_period(event))
			continue;

		if (perf_event_overflow(event, 1, &data, regs))
			x86_pmu.disable(event);
	}

	if (handled)
		inc_irq_stat(apic_perf_irqs);

	return handled;
}

void smp_perf_pending_interrupt(struct pt_regs *regs)
{
	irq_enter();
	ack_APIC_irq();
	inc_irq_stat(apic_pending_irqs);
	perf_event_do_pending();
	irq_exit();
}

void set_perf_event_pending(void)
{
#ifdef CONFIG_X86_LOCAL_APIC
	if (!x86_pmu.apic || !x86_pmu_initialized())
		return;

	apic->send_IPI_self(LOCAL_PENDING_VECTOR);
#endif
}

void perf_events_lapic_init(void)
{
#ifdef CONFIG_X86_LOCAL_APIC
	if (!x86_pmu.apic || !x86_pmu_initialized())
		return;

	/*
	 * Always use NMI for PMU
	 */
	apic_write(APIC_LVTPC, APIC_DM_NMI);
#endif
}

static int __kprobes
perf_event_nmi_handler(struct notifier_block *self,
			 unsigned long cmd, void *__args)
{
	struct die_args *args = __args;
	struct pt_regs *regs;

	if (!atomic_read(&active_events))
		return NOTIFY_DONE;

	switch (cmd) {
	case DIE_NMI:
	case DIE_NMI_IPI:
		break;

	default:
		return NOTIFY_DONE;
	}

	regs = args->regs;

#ifdef CONFIG_X86_LOCAL_APIC
	apic_write(APIC_LVTPC, APIC_DM_NMI);
#endif
	/*
	 * Can't rely on the handled return value to say it was our NMI, two
	 * events could trigger 'simultaneously' raising two back-to-back NMIs.
	 *
	 * If the first NMI handles both, the latter will be empty and daze
	 * the CPU.
	 */
	x86_pmu.handle_irq(regs);

	return NOTIFY_STOP;
}

static __read_mostly struct notifier_block perf_event_nmi_notifier = {
	.notifier_call		= perf_event_nmi_handler,
	.next			= NULL,
	.priority		= 1
};

static struct event_constraint unconstrained;
static struct event_constraint emptyconstraint;

static struct event_constraint *
x86_get_event_constraints(struct cpu_hw_events *cpuc, struct perf_event *event)
{
	struct event_constraint *c;

	if (x86_pmu.event_constraints) {
		for_each_event_constraint(c, x86_pmu.event_constraints) {
			if ((event->hw.config & c->cmask) == c->code)
				return c;
		}
	}

	return &unconstrained;
}

static int x86_event_sched_in(struct perf_event *event,
			  struct perf_cpu_context *cpuctx)
{
	int ret = 0;

	event->state = PERF_EVENT_STATE_ACTIVE;
	event->oncpu = smp_processor_id();
	event->tstamp_running += event->ctx->time - event->tstamp_stopped;

	if (!is_x86_event(event))
		ret = event->pmu->enable(event);

	if (!ret && !is_software_event(event))
		cpuctx->active_oncpu++;

	if (!ret && event->attr.exclusive)
		cpuctx->exclusive = 1;

	return ret;
}

static void x86_event_sched_out(struct perf_event *event,
			    struct perf_cpu_context *cpuctx)
{
	event->state = PERF_EVENT_STATE_INACTIVE;
	event->oncpu = -1;

	if (!is_x86_event(event))
		event->pmu->disable(event);

	event->tstamp_running -= event->ctx->time - event->tstamp_stopped;

	if (!is_software_event(event))
		cpuctx->active_oncpu--;

	if (event->attr.exclusive || !cpuctx->active_oncpu)
		cpuctx->exclusive = 0;
}

/*
 * Called to enable a whole group of events.
 * Returns 1 if the group was enabled, or -EAGAIN if it could not be.
 * Assumes the caller has disabled interrupts and has
 * frozen the PMU with hw_perf_save_disable.
 *
 * called with PMU disabled. If successful and return value 1,
 * then guaranteed to call perf_enable() and hw_perf_enable()
 */
int hw_perf_group_sched_in(struct perf_event *leader,
	       struct perf_cpu_context *cpuctx,
	       struct perf_event_context *ctx)
{
	struct cpu_hw_events *cpuc = &__get_cpu_var(cpu_hw_events);
	struct perf_event *sub;
	int assign[X86_PMC_IDX_MAX];
	int n0, n1, ret;

	/* n0 = total number of events */
	n0 = collect_events(cpuc, leader, true);
	if (n0 < 0)
		return n0;

	ret = x86_schedule_events(cpuc, n0, assign);
	if (ret)
		return ret;

	ret = x86_event_sched_in(leader, cpuctx);
	if (ret)
		return ret;

	n1 = 1;
	list_for_each_entry(sub, &leader->sibling_list, group_entry) {
		if (sub->state > PERF_EVENT_STATE_OFF) {
			ret = x86_event_sched_in(sub, cpuctx);
			if (ret)
				goto undo;
			++n1;
		}
	}
	/*
	 * copy new assignment, now we know it is possible
	 * will be used by hw_perf_enable()
	 */
	memcpy(cpuc->assign, assign, n0*sizeof(int));

	cpuc->n_events  = n0;
	cpuc->n_added   = n1;
	ctx->nr_active += n1;

	/*
	 * 1 means successful and events are active
	 * This is not quite true because we defer
	 * actual activation until hw_perf_enable() but
	 * this way we* ensure caller won't try to enable
	 * individual events
	 */
	return 1;
undo:
	x86_event_sched_out(leader, cpuctx);
	n0  = 1;
	list_for_each_entry(sub, &leader->sibling_list, group_entry) {
		if (sub->state == PERF_EVENT_STATE_ACTIVE) {
			x86_event_sched_out(sub, cpuctx);
			if (++n0 == n1)
				break;
		}
	}
	return ret;
}

#include "perf_event_amd.c"
#include "perf_event_p6.c"
#include "perf_event_intel.c"

static int __cpuinit
x86_pmu_notifier(struct notifier_block *self, unsigned long action, void *hcpu)
{
	unsigned int cpu = (long)hcpu;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_UP_PREPARE:
		if (x86_pmu.cpu_prepare)
			x86_pmu.cpu_prepare(cpu);
		break;

	case CPU_STARTING:
		if (x86_pmu.cpu_starting)
			x86_pmu.cpu_starting(cpu);
		break;

	case CPU_DYING:
		if (x86_pmu.cpu_dying)
			x86_pmu.cpu_dying(cpu);
		break;

	case CPU_DEAD:
		if (x86_pmu.cpu_dead)
			x86_pmu.cpu_dead(cpu);
		break;

	default:
		break;
	}

	return NOTIFY_OK;
}

static void __init pmu_check_apic(void)
{
	if (cpu_has_apic)
		return;

	x86_pmu.apic = 0;
	pr_info("no APIC, boot with the \"lapic\" boot parameter to force-enable it.\n");
	pr_info("no hardware sampling interrupt available.\n");
}

void __init init_hw_perf_events(void)
{
	struct event_constraint *c;
	int err;

	pr_info("Performance Events: ");

	switch (boot_cpu_data.x86_vendor) {
	case X86_VENDOR_INTEL:
		err = intel_pmu_init();
		break;
	case X86_VENDOR_AMD:
		err = amd_pmu_init();
		break;
	default:
		return;
	}
	if (err != 0) {
		pr_cont("no PMU driver, software events only.\n");
		return;
	}

	pmu_check_apic();

	pr_cont("%s PMU driver.\n", x86_pmu.name);

	if (x86_pmu.num_events > X86_PMC_MAX_GENERIC) {
		WARN(1, KERN_ERR "hw perf events %d > max(%d), clipping!",
		     x86_pmu.num_events, X86_PMC_MAX_GENERIC);
		x86_pmu.num_events = X86_PMC_MAX_GENERIC;
	}
	perf_event_mask = (1 << x86_pmu.num_events) - 1;
	perf_max_events = x86_pmu.num_events;

	if (x86_pmu.num_events_fixed > X86_PMC_MAX_FIXED) {
		WARN(1, KERN_ERR "hw perf events fixed %d > max(%d), clipping!",
		     x86_pmu.num_events_fixed, X86_PMC_MAX_FIXED);
		x86_pmu.num_events_fixed = X86_PMC_MAX_FIXED;
	}

	perf_event_mask |=
		((1LL << x86_pmu.num_events_fixed)-1) << X86_PMC_IDX_FIXED;
	x86_pmu.intel_ctrl = perf_event_mask;

	perf_events_lapic_init();
	register_die_notifier(&perf_event_nmi_notifier);

	unconstrained = (struct event_constraint)
		__EVENT_CONSTRAINT(0, (1ULL << x86_pmu.num_events) - 1,
				   0, x86_pmu.num_events);

	if (x86_pmu.event_constraints) {
		for_each_event_constraint(c, x86_pmu.event_constraints) {
			if (c->cmask != INTEL_ARCH_FIXED_MASK)
				continue;

			c->idxmsk64 |= (1ULL << x86_pmu.num_events) - 1;
			c->weight += x86_pmu.num_events;
		}
	}

	pr_info("... version:                %d\n",     x86_pmu.version);
	pr_info("... bit width:              %d\n",     x86_pmu.event_bits);
	pr_info("... generic registers:      %d\n",     x86_pmu.num_events);
	pr_info("... value mask:             %016Lx\n", x86_pmu.event_mask);
	pr_info("... max period:             %016Lx\n", x86_pmu.max_period);
	pr_info("... fixed-purpose events:   %d\n",     x86_pmu.num_events_fixed);
	pr_info("... event mask:             %016Lx\n", perf_event_mask);

	perf_cpu_notifier(x86_pmu_notifier);
}

static inline void x86_pmu_read(struct perf_event *event)
{
	x86_perf_event_update(event);
}

static const struct pmu pmu = {
	.enable		= x86_pmu_enable,
	.disable	= x86_pmu_disable,
	.start		= x86_pmu_start,
	.stop		= x86_pmu_stop,
	.read		= x86_pmu_read,
	.unthrottle	= x86_pmu_unthrottle,
};

/*
 * validate a single event group
 *
 * validation include:
 *	- check events are compatible which each other
 *	- events do not compete for the same counter
 *	- number of events <= number of counters
 *
 * validation ensures the group can be loaded onto the
 * PMU if it was the only group available.
 */
static int validate_group(struct perf_event *event)
{
	struct perf_event *leader = event->group_leader;
	struct cpu_hw_events *fake_cpuc;
	int ret, n;

	ret = -ENOMEM;
	fake_cpuc = kmalloc(sizeof(*fake_cpuc), GFP_KERNEL | __GFP_ZERO);
	if (!fake_cpuc)
		goto out;

	/*
	 * the event is not yet connected with its
	 * siblings therefore we must first collect
	 * existing siblings, then add the new event
	 * before we can simulate the scheduling
	 */
	ret = -ENOSPC;
	n = collect_events(fake_cpuc, leader, true);
	if (n < 0)
		goto out_free;

	fake_cpuc->n_events = n;
	n = collect_events(fake_cpuc, event, false);
	if (n < 0)
		goto out_free;

	fake_cpuc->n_events = n;

	ret = x86_schedule_events(fake_cpuc, n, NULL);

out_free:
	kfree(fake_cpuc);
out:
	return ret;
}

const struct pmu *hw_perf_event_init(struct perf_event *event)
{
	const struct pmu *tmp;
	int err;

	err = __hw_perf_event_init(event);
	if (!err) {
		/*
		 * we temporarily connect event to its pmu
		 * such that validate_group() can classify
		 * it as an x86 event using is_x86_event()
		 */
		tmp = event->pmu;
		event->pmu = &pmu;

		if (event->group_leader != event)
			err = validate_group(event);

		event->pmu = tmp;
	}
	if (err) {
		if (event->destroy)
			event->destroy(event);
		return ERR_PTR(err);
	}

	return &pmu;
}

/*
 * callchain support
 */

static inline
void callchain_store(struct perf_callchain_entry *entry, u64 ip)
{
	if (entry->nr < PERF_MAX_STACK_DEPTH)
		entry->ip[entry->nr++] = ip;
}

static DEFINE_PER_CPU(struct perf_callchain_entry, pmc_irq_entry);
static DEFINE_PER_CPU(struct perf_callchain_entry, pmc_nmi_entry);


static void
backtrace_warning_symbol(void *data, char *msg, unsigned long symbol)
{
	/* Ignore warnings */
}

static void backtrace_warning(void *data, char *msg)
{
	/* Ignore warnings */
}

static int backtrace_stack(void *data, char *name)
{
	return 0;
}

static void backtrace_address(void *data, unsigned long addr, int reliable)
{
	struct perf_callchain_entry *entry = data;

	if (reliable)
		callchain_store(entry, addr);
}

static const struct stacktrace_ops backtrace_ops = {
	.warning		= backtrace_warning,
	.warning_symbol		= backtrace_warning_symbol,
	.stack			= backtrace_stack,
	.address		= backtrace_address,
	.walk_stack		= print_context_stack_bp,
};

#include "../dumpstack.h"

static void
perf_callchain_kernel(struct pt_regs *regs, struct perf_callchain_entry *entry)
{
	callchain_store(entry, PERF_CONTEXT_KERNEL);
	callchain_store(entry, regs->ip);

	dump_trace(NULL, regs, NULL, regs->bp, &backtrace_ops, entry);
}

/*
 * best effort, GUP based copy_from_user() that assumes IRQ or NMI context
 */
static unsigned long
copy_from_user_nmi(void *to, const void __user *from, unsigned long n)
{
	unsigned long offset, addr = (unsigned long)from;
	int type = in_nmi() ? KM_NMI : KM_IRQ0;
	unsigned long size, len = 0;
	struct page *page;
	void *map;
	int ret;

	do {
		ret = __get_user_pages_fast(addr, 1, 0, &page);
		if (!ret)
			break;

		offset = addr & (PAGE_SIZE - 1);
		size = min(PAGE_SIZE - offset, n - len);

		map = kmap_atomic(page, type);
		memcpy(to, map+offset, size);
		kunmap_atomic(map, type);
		put_page(page);

		len  += size;
		to   += size;
		addr += size;

	} while (len < n);

	return len;
}

static int copy_stack_frame(const void __user *fp, struct stack_frame *frame)
{
	unsigned long bytes;

	bytes = copy_from_user_nmi(frame, fp, sizeof(*frame));

	return bytes == sizeof(*frame);
}

static void
perf_callchain_user(struct pt_regs *regs, struct perf_callchain_entry *entry)
{
	struct stack_frame frame;
	const void __user *fp;

	if (!user_mode(regs))
		regs = task_pt_regs(current);

	fp = (void __user *)regs->bp;

	callchain_store(entry, PERF_CONTEXT_USER);
	callchain_store(entry, regs->ip);

	while (entry->nr < PERF_MAX_STACK_DEPTH) {
		frame.next_frame	     = NULL;
		frame.return_address = 0;

		if (!copy_stack_frame(fp, &frame))
			break;

		if ((unsigned long)fp < regs->sp)
			break;

		callchain_store(entry, frame.return_address);
		fp = frame.next_frame;
	}
}

static void
perf_do_callchain(struct pt_regs *regs, struct perf_callchain_entry *entry)
{
	int is_user;

	if (!regs)
		return;

	is_user = user_mode(regs);

	if (is_user && current->state != TASK_RUNNING)
		return;

	if (!is_user)
		perf_callchain_kernel(regs, entry);

	if (current->mm)
		perf_callchain_user(regs, entry);
}

struct perf_callchain_entry *perf_callchain(struct pt_regs *regs)
{
	struct perf_callchain_entry *entry;

	if (in_nmi())
		entry = &__get_cpu_var(pmc_nmi_entry);
	else
		entry = &__get_cpu_var(pmc_irq_entry);

	entry->nr = 0;

	perf_do_callchain(regs, entry);

	return entry;
}
