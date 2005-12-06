/* hw_ops.c - query/set operations on active SPU context.
 *
 * Copyright (C) IBM 2005
 * Author: Mark Nutter <mnutter@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>

#include <asm/io.h>
#include <asm/spu.h>
#include <asm/spu_csa.h>
#include <asm/mmu_context.h>
#include "spufs.h"

static int spu_hw_mbox_read(struct spu_context *ctx, u32 * data)
{
	struct spu *spu = ctx->spu;
	struct spu_problem __iomem *prob = spu->problem;
	u32 mbox_stat;
	int ret = 0;

	spin_lock_irq(&spu->register_lock);
	mbox_stat = in_be32(&prob->mb_stat_R);
	if (mbox_stat & 0x0000ff) {
		*data = in_be32(&prob->pu_mb_R);
		ret = 4;
	}
	spin_unlock_irq(&spu->register_lock);
	return ret;
}

static u32 spu_hw_mbox_stat_read(struct spu_context *ctx)
{
	return in_be32(&ctx->spu->problem->mb_stat_R);
}

static int spu_hw_ibox_read(struct spu_context *ctx, u32 * data)
{
	struct spu *spu = ctx->spu;
	struct spu_problem __iomem *prob = spu->problem;
	struct spu_priv1 __iomem *priv1 = spu->priv1;
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	int ret;

	spin_lock_irq(&spu->register_lock);
	if (in_be32(&prob->mb_stat_R) & 0xff0000) {
		/* read the first available word */
		*data = in_be64(&priv2->puint_mb_R);
		ret = 4;
	} else {
		/* make sure we get woken up by the interrupt */
		out_be64(&priv1->int_mask_class2_RW,
			 in_be64(&priv1->int_mask_class2_RW) | 0x1);
		ret = 0;
	}
	spin_unlock_irq(&spu->register_lock);
	return ret;
}

static int spu_hw_wbox_write(struct spu_context *ctx, u32 data)
{
	struct spu *spu = ctx->spu;
	struct spu_problem __iomem *prob = spu->problem;
	struct spu_priv1 __iomem *priv1 = spu->priv1;
	int ret;

	spin_lock_irq(&spu->register_lock);
	if (in_be32(&prob->mb_stat_R) & 0x00ff00) {
		/* we have space to write wbox_data to */
		out_be32(&prob->spu_mb_W, data);
		ret = 4;
	} else {
		/* make sure we get woken up by the interrupt when space
		   becomes available */
		out_be64(&priv1->int_mask_class2_RW,
			 in_be64(&priv1->int_mask_class2_RW) | 0x10);
		ret = 0;
	}
	spin_unlock_irq(&spu->register_lock);
	return ret;
}

static u32 spu_hw_signal1_read(struct spu_context *ctx)
{
	return in_be32(&ctx->spu->problem->signal_notify1);
}

static void spu_hw_signal1_write(struct spu_context *ctx, u32 data)
{
	out_be32(&ctx->spu->problem->signal_notify1, data);
}

static u32 spu_hw_signal2_read(struct spu_context *ctx)
{
	return in_be32(&ctx->spu->problem->signal_notify1);
}

static void spu_hw_signal2_write(struct spu_context *ctx, u32 data)
{
	out_be32(&ctx->spu->problem->signal_notify2, data);
}

static void spu_hw_signal1_type_set(struct spu_context *ctx, u64 val)
{
	struct spu *spu = ctx->spu;
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	u64 tmp;

	spin_lock_irq(&spu->register_lock);
	tmp = in_be64(&priv2->spu_cfg_RW);
	if (val)
		tmp |= 1;
	else
		tmp &= ~1;
	out_be64(&priv2->spu_cfg_RW, tmp);
	spin_unlock_irq(&spu->register_lock);
}

static u64 spu_hw_signal1_type_get(struct spu_context *ctx)
{
	return ((in_be64(&ctx->spu->priv2->spu_cfg_RW) & 1) != 0);
}

static void spu_hw_signal2_type_set(struct spu_context *ctx, u64 val)
{
	struct spu *spu = ctx->spu;
	struct spu_priv2 __iomem *priv2 = spu->priv2;
	u64 tmp;

	spin_lock_irq(&spu->register_lock);
	tmp = in_be64(&priv2->spu_cfg_RW);
	if (val)
		tmp |= 2;
	else
		tmp &= ~2;
	out_be64(&priv2->spu_cfg_RW, tmp);
	spin_unlock_irq(&spu->register_lock);
}

static u64 spu_hw_signal2_type_get(struct spu_context *ctx)
{
	return ((in_be64(&ctx->spu->priv2->spu_cfg_RW) & 2) != 0);
}

static u32 spu_hw_npc_read(struct spu_context *ctx)
{
	return in_be32(&ctx->spu->problem->spu_npc_RW);
}

static void spu_hw_npc_write(struct spu_context *ctx, u32 val)
{
	out_be32(&ctx->spu->problem->spu_npc_RW, val);
}

static u32 spu_hw_status_read(struct spu_context *ctx)
{
	return in_be32(&ctx->spu->problem->spu_status_R);
}

static char *spu_hw_get_ls(struct spu_context *ctx)
{
	return ctx->spu->local_store;
}

static void spu_hw_runcntl_write(struct spu_context *ctx, u32 val)
{
	eieio();
	out_be32(&ctx->spu->problem->spu_runcntl_RW, val);
}

static void spu_hw_runcntl_stop(struct spu_context *ctx)
{
	spin_lock_irq(&ctx->spu->register_lock);
	out_be32(&ctx->spu->problem->spu_runcntl_RW, SPU_RUNCNTL_STOP);
	while (in_be32(&ctx->spu->problem->spu_status_R) & SPU_STATUS_RUNNING)
		cpu_relax();
	spin_unlock_irq(&ctx->spu->register_lock);
}

struct spu_context_ops spu_hw_ops = {
	.mbox_read = spu_hw_mbox_read,
	.mbox_stat_read = spu_hw_mbox_stat_read,
	.ibox_read = spu_hw_ibox_read,
	.wbox_write = spu_hw_wbox_write,
	.signal1_read = spu_hw_signal1_read,
	.signal1_write = spu_hw_signal1_write,
	.signal2_read = spu_hw_signal2_read,
	.signal2_write = spu_hw_signal2_write,
	.signal1_type_set = spu_hw_signal1_type_set,
	.signal1_type_get = spu_hw_signal1_type_get,
	.signal2_type_set = spu_hw_signal2_type_set,
	.signal2_type_get = spu_hw_signal2_type_get,
	.npc_read = spu_hw_npc_read,
	.npc_write = spu_hw_npc_write,
	.status_read = spu_hw_status_read,
	.get_ls = spu_hw_get_ls,
	.runcntl_write = spu_hw_runcntl_write,
	.runcntl_stop = spu_hw_runcntl_stop,
};
