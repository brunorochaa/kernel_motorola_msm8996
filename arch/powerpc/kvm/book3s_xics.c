/*
 * Copyright 2012 Michael Ellerman, IBM Corporation.
 * Copyright 2012 Benjamin Herrenschmidt, IBM Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/kvm_host.h>
#include <linux/err.h>
#include <linux/gfp.h>

#include <asm/uaccess.h>
#include <asm/kvm_book3s.h>
#include <asm/kvm_ppc.h>
#include <asm/hvcall.h>
#include <asm/xics.h>
#include <asm/debug.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "book3s_xics.h"

#if 1
#define XICS_DBG(fmt...) do { } while (0)
#else
#define XICS_DBG(fmt...) trace_printk(fmt)
#endif

/*
 * LOCKING
 * =======
 *
 * Each ICS has a mutex protecting the information about the IRQ
 * sources and avoiding simultaneous deliveries if the same interrupt.
 *
 * ICP operations are done via a single compare & swap transaction
 * (most ICP state fits in the union kvmppc_icp_state)
 */

/*
 * TODO
 * ====
 *
 * - To speed up resends, keep a bitmap of "resend" set bits in the
 *   ICS
 *
 * - Speed up server# -> ICP lookup (array ? hash table ?)
 *
 * - Make ICS lockless as well, or at least a per-interrupt lock or hashed
 *   locks array to improve scalability
 *
 * - ioctl's to save/restore the entire state for snapshot & migration
 */

/* -- ICS routines -- */

static void icp_deliver_irq(struct kvmppc_xics *xics, struct kvmppc_icp *icp,
			    u32 new_irq);

static int ics_deliver_irq(struct kvmppc_xics *xics, u32 irq, u32 level)
{
	struct ics_irq_state *state;
	struct kvmppc_ics *ics;
	u16 src;

	XICS_DBG("ics deliver %#x (level: %d)\n", irq, level);

	ics = kvmppc_xics_find_ics(xics, irq, &src);
	if (!ics) {
		XICS_DBG("ics_deliver_irq: IRQ 0x%06x not found !\n", irq);
		return -EINVAL;
	}
	state = &ics->irq_state[src];
	if (!state->exists)
		return -EINVAL;

	/*
	 * We set state->asserted locklessly. This should be fine as
	 * we are the only setter, thus concurrent access is undefined
	 * to begin with.
	 */
	if (level == KVM_INTERRUPT_SET_LEVEL)
		state->asserted = 1;
	else if (level == KVM_INTERRUPT_UNSET) {
		state->asserted = 0;
		return 0;
	}

	/* Attempt delivery */
	icp_deliver_irq(xics, NULL, irq);

	return 0;
}

static void ics_check_resend(struct kvmppc_xics *xics, struct kvmppc_ics *ics,
			     struct kvmppc_icp *icp)
{
	int i;

	mutex_lock(&ics->lock);

	for (i = 0; i < KVMPPC_XICS_IRQ_PER_ICS; i++) {
		struct ics_irq_state *state = &ics->irq_state[i];

		if (!state->resend)
			continue;

		XICS_DBG("resend %#x prio %#x\n", state->number,
			      state->priority);

		mutex_unlock(&ics->lock);
		icp_deliver_irq(xics, icp, state->number);
		mutex_lock(&ics->lock);
	}

	mutex_unlock(&ics->lock);
}

int kvmppc_xics_set_xive(struct kvm *kvm, u32 irq, u32 server, u32 priority)
{
	struct kvmppc_xics *xics = kvm->arch.xics;
	struct kvmppc_icp *icp;
	struct kvmppc_ics *ics;
	struct ics_irq_state *state;
	u16 src;
	bool deliver;

	if (!xics)
		return -ENODEV;

	ics = kvmppc_xics_find_ics(xics, irq, &src);
	if (!ics)
		return -EINVAL;
	state = &ics->irq_state[src];

	icp = kvmppc_xics_find_server(kvm, server);
	if (!icp)
		return -EINVAL;

	mutex_lock(&ics->lock);

	XICS_DBG("set_xive %#x server %#x prio %#x MP:%d RS:%d\n",
		 irq, server, priority,
		 state->masked_pending, state->resend);

	state->server = server;
	state->priority = priority;
	deliver = false;
	if ((state->masked_pending || state->resend) && priority != MASKED) {
		state->masked_pending = 0;
		deliver = true;
	}

	mutex_unlock(&ics->lock);

	if (deliver)
		icp_deliver_irq(xics, icp, irq);

	return 0;
}

int kvmppc_xics_get_xive(struct kvm *kvm, u32 irq, u32 *server, u32 *priority)
{
	struct kvmppc_xics *xics = kvm->arch.xics;
	struct kvmppc_ics *ics;
	struct ics_irq_state *state;
	u16 src;

	if (!xics)
		return -ENODEV;

	ics = kvmppc_xics_find_ics(xics, irq, &src);
	if (!ics)
		return -EINVAL;
	state = &ics->irq_state[src];

	mutex_lock(&ics->lock);
	*server = state->server;
	*priority = state->priority;
	mutex_unlock(&ics->lock);

	return 0;
}

/* -- ICP routines, including hcalls -- */

static inline bool icp_try_update(struct kvmppc_icp *icp,
				  union kvmppc_icp_state old,
				  union kvmppc_icp_state new,
				  bool change_self)
{
	bool success;

	/* Calculate new output value */
	new.out_ee = (new.xisr && (new.pending_pri < new.cppr));

	/* Attempt atomic update */
	success = cmpxchg64(&icp->state.raw, old.raw, new.raw) == old.raw;
	if (!success)
		goto bail;

	XICS_DBG("UPD [%04x] - C:%02x M:%02x PP: %02x PI:%06x R:%d O:%d\n",
		 icp->server_num,
		 old.cppr, old.mfrr, old.pending_pri, old.xisr,
		 old.need_resend, old.out_ee);
	XICS_DBG("UPD        - C:%02x M:%02x PP: %02x PI:%06x R:%d O:%d\n",
		 new.cppr, new.mfrr, new.pending_pri, new.xisr,
		 new.need_resend, new.out_ee);
	/*
	 * Check for output state update
	 *
	 * Note that this is racy since another processor could be updating
	 * the state already. This is why we never clear the interrupt output
	 * here, we only ever set it. The clear only happens prior to doing
	 * an update and only by the processor itself. Currently we do it
	 * in Accept (H_XIRR) and Up_Cppr (H_XPPR).
	 *
	 * We also do not try to figure out whether the EE state has changed,
	 * we unconditionally set it if the new state calls for it for the
	 * same reason.
	 */
	if (new.out_ee) {
		kvmppc_book3s_queue_irqprio(icp->vcpu,
					    BOOK3S_INTERRUPT_EXTERNAL_LEVEL);
		if (!change_self)
			kvmppc_fast_vcpu_kick(icp->vcpu);
	}
 bail:
	return success;
}

static void icp_check_resend(struct kvmppc_xics *xics,
			     struct kvmppc_icp *icp)
{
	u32 icsid;

	/* Order this load with the test for need_resend in the caller */
	smp_rmb();
	for_each_set_bit(icsid, icp->resend_map, xics->max_icsid + 1) {
		struct kvmppc_ics *ics = xics->ics[icsid];

		if (!test_and_clear_bit(icsid, icp->resend_map))
			continue;
		if (!ics)
			continue;
		ics_check_resend(xics, ics, icp);
	}
}

static bool icp_try_to_deliver(struct kvmppc_icp *icp, u32 irq, u8 priority,
			       u32 *reject)
{
	union kvmppc_icp_state old_state, new_state;
	bool success;

	XICS_DBG("try deliver %#x(P:%#x) to server %#x\n", irq, priority,
		 icp->server_num);

	do {
		old_state = new_state = ACCESS_ONCE(icp->state);

		*reject = 0;

		/* See if we can deliver */
		success = new_state.cppr > priority &&
			new_state.mfrr > priority &&
			new_state.pending_pri > priority;

		/*
		 * If we can, check for a rejection and perform the
		 * delivery
		 */
		if (success) {
			*reject = new_state.xisr;
			new_state.xisr = irq;
			new_state.pending_pri = priority;
		} else {
			/*
			 * If we failed to deliver we set need_resend
			 * so a subsequent CPPR state change causes us
			 * to try a new delivery.
			 */
			new_state.need_resend = true;
		}

	} while (!icp_try_update(icp, old_state, new_state, false));

	return success;
}

static void icp_deliver_irq(struct kvmppc_xics *xics, struct kvmppc_icp *icp,
			    u32 new_irq)
{
	struct ics_irq_state *state;
	struct kvmppc_ics *ics;
	u32 reject;
	u16 src;

	/*
	 * This is used both for initial delivery of an interrupt and
	 * for subsequent rejection.
	 *
	 * Rejection can be racy vs. resends. We have evaluated the
	 * rejection in an atomic ICP transaction which is now complete,
	 * so potentially the ICP can already accept the interrupt again.
	 *
	 * So we need to retry the delivery. Essentially the reject path
	 * boils down to a failed delivery. Always.
	 *
	 * Now the interrupt could also have moved to a different target,
	 * thus we may need to re-do the ICP lookup as well
	 */

 again:
	/* Get the ICS state and lock it */
	ics = kvmppc_xics_find_ics(xics, new_irq, &src);
	if (!ics) {
		XICS_DBG("icp_deliver_irq: IRQ 0x%06x not found !\n", new_irq);
		return;
	}
	state = &ics->irq_state[src];

	/* Get a lock on the ICS */
	mutex_lock(&ics->lock);

	/* Get our server */
	if (!icp || state->server != icp->server_num) {
		icp = kvmppc_xics_find_server(xics->kvm, state->server);
		if (!icp) {
			pr_warn("icp_deliver_irq: IRQ 0x%06x server 0x%x not found !\n",
				new_irq, state->server);
			goto out;
		}
	}

	/* Clear the resend bit of that interrupt */
	state->resend = 0;

	/*
	 * If masked, bail out
	 *
	 * Note: PAPR doesn't mention anything about masked pending
	 * when doing a resend, only when doing a delivery.
	 *
	 * However that would have the effect of losing a masked
	 * interrupt that was rejected and isn't consistent with
	 * the whole masked_pending business which is about not
	 * losing interrupts that occur while masked.
	 *
	 * I don't differenciate normal deliveries and resends, this
	 * implementation will differ from PAPR and not lose such
	 * interrupts.
	 */
	if (state->priority == MASKED) {
		XICS_DBG("irq %#x masked pending\n", new_irq);
		state->masked_pending = 1;
		goto out;
	}

	/*
	 * Try the delivery, this will set the need_resend flag
	 * in the ICP as part of the atomic transaction if the
	 * delivery is not possible.
	 *
	 * Note that if successful, the new delivery might have itself
	 * rejected an interrupt that was "delivered" before we took the
	 * icp mutex.
	 *
	 * In this case we do the whole sequence all over again for the
	 * new guy. We cannot assume that the rejected interrupt is less
	 * favored than the new one, and thus doesn't need to be delivered,
	 * because by the time we exit icp_try_to_deliver() the target
	 * processor may well have alrady consumed & completed it, and thus
	 * the rejected interrupt might actually be already acceptable.
	 */
	if (icp_try_to_deliver(icp, new_irq, state->priority, &reject)) {
		/*
		 * Delivery was successful, did we reject somebody else ?
		 */
		if (reject && reject != XICS_IPI) {
			mutex_unlock(&ics->lock);
			new_irq = reject;
			goto again;
		}
	} else {
		/*
		 * We failed to deliver the interrupt we need to set the
		 * resend map bit and mark the ICS state as needing a resend
		 */
		set_bit(ics->icsid, icp->resend_map);
		state->resend = 1;

		/*
		 * If the need_resend flag got cleared in the ICP some time
		 * between icp_try_to_deliver() atomic update and now, then
		 * we know it might have missed the resend_map bit. So we
		 * retry
		 */
		smp_mb();
		if (!icp->state.need_resend) {
			mutex_unlock(&ics->lock);
			goto again;
		}
	}
 out:
	mutex_unlock(&ics->lock);
}

static void icp_down_cppr(struct kvmppc_xics *xics, struct kvmppc_icp *icp,
			  u8 new_cppr)
{
	union kvmppc_icp_state old_state, new_state;
	bool resend;

	/*
	 * This handles several related states in one operation:
	 *
	 * ICP State: Down_CPPR
	 *
	 * Load CPPR with new value and if the XISR is 0
	 * then check for resends:
	 *
	 * ICP State: Resend
	 *
	 * If MFRR is more favored than CPPR, check for IPIs
	 * and notify ICS of a potential resend. This is done
	 * asynchronously (when used in real mode, we will have
	 * to exit here).
	 *
	 * We do not handle the complete Check_IPI as documented
	 * here. In the PAPR, this state will be used for both
	 * Set_MFRR and Down_CPPR. However, we know that we aren't
	 * changing the MFRR state here so we don't need to handle
	 * the case of an MFRR causing a reject of a pending irq,
	 * this will have been handled when the MFRR was set in the
	 * first place.
	 *
	 * Thus we don't have to handle rejects, only resends.
	 *
	 * When implementing real mode for HV KVM, resend will lead to
	 * a H_TOO_HARD return and the whole transaction will be handled
	 * in virtual mode.
	 */
	do {
		old_state = new_state = ACCESS_ONCE(icp->state);

		/* Down_CPPR */
		new_state.cppr = new_cppr;

		/*
		 * Cut down Resend / Check_IPI / IPI
		 *
		 * The logic is that we cannot have a pending interrupt
		 * trumped by an IPI at this point (see above), so we
		 * know that either the pending interrupt is already an
		 * IPI (in which case we don't care to override it) or
		 * it's either more favored than us or non existent
		 */
		if (new_state.mfrr < new_cppr &&
		    new_state.mfrr <= new_state.pending_pri) {
			WARN_ON(new_state.xisr != XICS_IPI &&
				new_state.xisr != 0);
			new_state.pending_pri = new_state.mfrr;
			new_state.xisr = XICS_IPI;
		}

		/* Latch/clear resend bit */
		resend = new_state.need_resend;
		new_state.need_resend = 0;

	} while (!icp_try_update(icp, old_state, new_state, true));

	/*
	 * Now handle resend checks. Those are asynchronous to the ICP
	 * state update in HW (ie bus transactions) so we can handle them
	 * separately here too
	 */
	if (resend)
		icp_check_resend(xics, icp);
}

static noinline unsigned long h_xirr(struct kvm_vcpu *vcpu)
{
	union kvmppc_icp_state old_state, new_state;
	struct kvmppc_icp *icp = vcpu->arch.icp;
	u32 xirr;

	/* First, remove EE from the processor */
	kvmppc_book3s_dequeue_irqprio(icp->vcpu,
				      BOOK3S_INTERRUPT_EXTERNAL_LEVEL);

	/*
	 * ICP State: Accept_Interrupt
	 *
	 * Return the pending interrupt (if any) along with the
	 * current CPPR, then clear the XISR & set CPPR to the
	 * pending priority
	 */
	do {
		old_state = new_state = ACCESS_ONCE(icp->state);

		xirr = old_state.xisr | (((u32)old_state.cppr) << 24);
		if (!old_state.xisr)
			break;
		new_state.cppr = new_state.pending_pri;
		new_state.pending_pri = 0xff;
		new_state.xisr = 0;

	} while (!icp_try_update(icp, old_state, new_state, true));

	XICS_DBG("h_xirr vcpu %d xirr %#x\n", vcpu->vcpu_id, xirr);

	return xirr;
}

static noinline int h_ipi(struct kvm_vcpu *vcpu, unsigned long server,
			  unsigned long mfrr)
{
	union kvmppc_icp_state old_state, new_state;
	struct kvmppc_xics *xics = vcpu->kvm->arch.xics;
	struct kvmppc_icp *icp;
	u32 reject;
	bool resend;
	bool local;

	XICS_DBG("h_ipi vcpu %d to server %lu mfrr %#lx\n",
		 vcpu->vcpu_id, server, mfrr);

	icp = vcpu->arch.icp;
	local = icp->server_num == server;
	if (!local) {
		icp = kvmppc_xics_find_server(vcpu->kvm, server);
		if (!icp)
			return H_PARAMETER;
	}

	/*
	 * ICP state: Set_MFRR
	 *
	 * If the CPPR is more favored than the new MFRR, then
	 * nothing needs to be rejected as there can be no XISR to
	 * reject.  If the MFRR is being made less favored then
	 * there might be a previously-rejected interrupt needing
	 * to be resent.
	 *
	 * If the CPPR is less favored, then we might be replacing
	 * an interrupt, and thus need to possibly reject it as in
	 *
	 * ICP state: Check_IPI
	 */
	do {
		old_state = new_state = ACCESS_ONCE(icp->state);

		/* Set_MFRR */
		new_state.mfrr = mfrr;

		/* Check_IPI */
		reject = 0;
		resend = false;
		if (mfrr < new_state.cppr) {
			/* Reject a pending interrupt if not an IPI */
			if (mfrr <= new_state.pending_pri)
				reject = new_state.xisr;
			new_state.pending_pri = mfrr;
			new_state.xisr = XICS_IPI;
		}

		if (mfrr > old_state.mfrr && mfrr > new_state.cppr) {
			resend = new_state.need_resend;
			new_state.need_resend = 0;
		}
	} while (!icp_try_update(icp, old_state, new_state, local));

	/* Handle reject */
	if (reject && reject != XICS_IPI)
		icp_deliver_irq(xics, icp, reject);

	/* Handle resend */
	if (resend)
		icp_check_resend(xics, icp);

	return H_SUCCESS;
}

static noinline void h_cppr(struct kvm_vcpu *vcpu, unsigned long cppr)
{
	union kvmppc_icp_state old_state, new_state;
	struct kvmppc_xics *xics = vcpu->kvm->arch.xics;
	struct kvmppc_icp *icp = vcpu->arch.icp;
	u32 reject;

	XICS_DBG("h_cppr vcpu %d cppr %#lx\n", vcpu->vcpu_id, cppr);

	/*
	 * ICP State: Set_CPPR
	 *
	 * We can safely compare the new value with the current
	 * value outside of the transaction as the CPPR is only
	 * ever changed by the processor on itself
	 */
	if (cppr > icp->state.cppr)
		icp_down_cppr(xics, icp, cppr);
	else if (cppr == icp->state.cppr)
		return;

	/*
	 * ICP State: Up_CPPR
	 *
	 * The processor is raising its priority, this can result
	 * in a rejection of a pending interrupt:
	 *
	 * ICP State: Reject_Current
	 *
	 * We can remove EE from the current processor, the update
	 * transaction will set it again if needed
	 */
	kvmppc_book3s_dequeue_irqprio(icp->vcpu,
				      BOOK3S_INTERRUPT_EXTERNAL_LEVEL);

	do {
		old_state = new_state = ACCESS_ONCE(icp->state);

		reject = 0;
		new_state.cppr = cppr;

		if (cppr <= new_state.pending_pri) {
			reject = new_state.xisr;
			new_state.xisr = 0;
			new_state.pending_pri = 0xff;
		}

	} while (!icp_try_update(icp, old_state, new_state, true));

	/*
	 * Check for rejects. They are handled by doing a new delivery
	 * attempt (see comments in icp_deliver_irq).
	 */
	if (reject && reject != XICS_IPI)
		icp_deliver_irq(xics, icp, reject);
}

static noinline int h_eoi(struct kvm_vcpu *vcpu, unsigned long xirr)
{
	struct kvmppc_xics *xics = vcpu->kvm->arch.xics;
	struct kvmppc_icp *icp = vcpu->arch.icp;
	struct kvmppc_ics *ics;
	struct ics_irq_state *state;
	u32 irq = xirr & 0x00ffffff;
	u16 src;

	XICS_DBG("h_eoi vcpu %d eoi %#lx\n", vcpu->vcpu_id, xirr);

	/*
	 * ICP State: EOI
	 *
	 * Note: If EOI is incorrectly used by SW to lower the CPPR
	 * value (ie more favored), we do not check for rejection of
	 * a pending interrupt, this is a SW error and PAPR sepcifies
	 * that we don't have to deal with it.
	 *
	 * The sending of an EOI to the ICS is handled after the
	 * CPPR update
	 *
	 * ICP State: Down_CPPR which we handle
	 * in a separate function as it's shared with H_CPPR.
	 */
	icp_down_cppr(xics, icp, xirr >> 24);

	/* IPIs have no EOI */
	if (irq == XICS_IPI)
		return H_SUCCESS;
	/*
	 * EOI handling: If the interrupt is still asserted, we need to
	 * resend it. We can take a lockless "peek" at the ICS state here.
	 *
	 * "Message" interrupts will never have "asserted" set
	 */
	ics = kvmppc_xics_find_ics(xics, irq, &src);
	if (!ics) {
		XICS_DBG("h_eoi: IRQ 0x%06x not found !\n", irq);
		return H_PARAMETER;
	}
	state = &ics->irq_state[src];

	/* Still asserted, resend it */
	if (state->asserted)
		icp_deliver_irq(xics, icp, irq);

	return H_SUCCESS;
}

int kvmppc_xics_hcall(struct kvm_vcpu *vcpu, u32 req)
{
	unsigned long res;
	int rc = H_SUCCESS;

	/* Check if we have an ICP */
	if (!vcpu->arch.icp || !vcpu->kvm->arch.xics)
		return H_HARDWARE;

	switch (req) {
	case H_XIRR:
		res = h_xirr(vcpu);
		kvmppc_set_gpr(vcpu, 4, res);
		break;
	case H_CPPR:
		h_cppr(vcpu, kvmppc_get_gpr(vcpu, 4));
		break;
	case H_EOI:
		rc = h_eoi(vcpu, kvmppc_get_gpr(vcpu, 4));
		break;
	case H_IPI:
		rc = h_ipi(vcpu, kvmppc_get_gpr(vcpu, 4),
			   kvmppc_get_gpr(vcpu, 5));
		break;
	}

	return rc;
}


/* -- Initialisation code etc. -- */

static int xics_debug_show(struct seq_file *m, void *private)
{
	struct kvmppc_xics *xics = m->private;
	struct kvm *kvm = xics->kvm;
	struct kvm_vcpu *vcpu;
	int icsid, i;

	if (!kvm)
		return 0;

	seq_printf(m, "=========\nICP state\n=========\n");

	kvm_for_each_vcpu(i, vcpu, kvm) {
		struct kvmppc_icp *icp = vcpu->arch.icp;
		union kvmppc_icp_state state;

		if (!icp)
			continue;

		state.raw = ACCESS_ONCE(icp->state.raw);
		seq_printf(m, "cpu server %#lx XIRR:%#x PPRI:%#x CPPR:%#x MFRR:%#x OUT:%d NR:%d\n",
			   icp->server_num, state.xisr,
			   state.pending_pri, state.cppr, state.mfrr,
			   state.out_ee, state.need_resend);
	}

	for (icsid = 0; icsid <= KVMPPC_XICS_MAX_ICS_ID; icsid++) {
		struct kvmppc_ics *ics = xics->ics[icsid];

		if (!ics)
			continue;

		seq_printf(m, "=========\nICS state for ICS 0x%x\n=========\n",
			   icsid);

		mutex_lock(&ics->lock);

		for (i = 0; i < KVMPPC_XICS_IRQ_PER_ICS; i++) {
			struct ics_irq_state *irq = &ics->irq_state[i];

			seq_printf(m, "irq 0x%06x: server %#x prio %#x save prio %#x asserted %d resend %d masked pending %d\n",
				   irq->number, irq->server, irq->priority,
				   irq->saved_priority, irq->asserted,
				   irq->resend, irq->masked_pending);

		}
		mutex_unlock(&ics->lock);
	}
	return 0;
}

static int xics_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, xics_debug_show, inode->i_private);
}

static const struct file_operations xics_debug_fops = {
	.open = xics_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void xics_debugfs_init(struct kvmppc_xics *xics)
{
	char *name;

	name = kasprintf(GFP_KERNEL, "kvm-xics-%p", xics);
	if (!name) {
		pr_err("%s: no memory for name\n", __func__);
		return;
	}

	xics->dentry = debugfs_create_file(name, S_IRUGO, powerpc_debugfs_root,
					   xics, &xics_debug_fops);

	pr_debug("%s: created %s\n", __func__, name);
	kfree(name);
}

struct kvmppc_ics *kvmppc_xics_create_ics(struct kvm *kvm,
					  struct kvmppc_xics *xics, int irq)
{
	struct kvmppc_ics *ics;
	int i, icsid;

	icsid = irq >> KVMPPC_XICS_ICS_SHIFT;

	mutex_lock(&kvm->lock);

	/* ICS already exists - somebody else got here first */
	if (xics->ics[icsid])
		goto out;

	/* Create the ICS */
	ics = kzalloc(sizeof(struct kvmppc_ics), GFP_KERNEL);
	if (!ics)
		goto out;

	mutex_init(&ics->lock);
	ics->icsid = icsid;

	for (i = 0; i < KVMPPC_XICS_IRQ_PER_ICS; i++) {
		ics->irq_state[i].number = (icsid << KVMPPC_XICS_ICS_SHIFT) | i;
		ics->irq_state[i].priority = MASKED;
		ics->irq_state[i].saved_priority = MASKED;
	}
	smp_wmb();
	xics->ics[icsid] = ics;

	if (icsid > xics->max_icsid)
		xics->max_icsid = icsid;

 out:
	mutex_unlock(&kvm->lock);
	return xics->ics[icsid];
}

int kvmppc_xics_create_icp(struct kvm_vcpu *vcpu, unsigned long server_num)
{
	struct kvmppc_icp *icp;

	if (!vcpu->kvm->arch.xics)
		return -ENODEV;

	if (kvmppc_xics_find_server(vcpu->kvm, server_num))
		return -EEXIST;

	icp = kzalloc(sizeof(struct kvmppc_icp), GFP_KERNEL);
	if (!icp)
		return -ENOMEM;

	icp->vcpu = vcpu;
	icp->server_num = server_num;
	icp->state.mfrr = MASKED;
	icp->state.pending_pri = MASKED;
	vcpu->arch.icp = icp;

	XICS_DBG("created server for vcpu %d\n", vcpu->vcpu_id);

	return 0;
}

/* -- ioctls -- */

int kvm_vm_ioctl_xics_irq(struct kvm *kvm, struct kvm_irq_level *args)
{
	struct kvmppc_xics *xics;
	int r;

	/* locking against multiple callers? */

	xics = kvm->arch.xics;
	if (!xics)
		return -ENODEV;

	switch (args->level) {
	case KVM_INTERRUPT_SET:
	case KVM_INTERRUPT_SET_LEVEL:
	case KVM_INTERRUPT_UNSET:
		r = ics_deliver_irq(xics, args->irq, args->level);
		break;
	default:
		r = -EINVAL;
	}

	return r;
}

void kvmppc_xics_free(struct kvmppc_xics *xics)
{
	int i;
	struct kvm *kvm = xics->kvm;

	debugfs_remove(xics->dentry);

	if (kvm)
		kvm->arch.xics = NULL;

	for (i = 0; i <= xics->max_icsid; i++)
		kfree(xics->ics[i]);
	kfree(xics);
}

int kvm_xics_create(struct kvm *kvm, u32 type)
{
	struct kvmppc_xics *xics;
	int ret = 0;

	xics = kzalloc(sizeof(*xics), GFP_KERNEL);
	if (!xics)
		return -ENOMEM;

	xics->kvm = kvm;

	/* Already there ? */
	mutex_lock(&kvm->lock);
	if (kvm->arch.xics)
		ret = -EEXIST;
	else
		kvm->arch.xics = xics;
	mutex_unlock(&kvm->lock);

	if (ret)
		return ret;

	xics_debugfs_init(xics);

	return 0;
}

void kvmppc_xics_free_icp(struct kvm_vcpu *vcpu)
{
	if (!vcpu->arch.icp)
		return;
	kfree(vcpu->arch.icp);
	vcpu->arch.icp = NULL;
	vcpu->arch.irq_type = KVMPPC_IRQ_DEFAULT;
}
