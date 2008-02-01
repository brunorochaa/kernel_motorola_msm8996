/* -*- mode: c; c-basic-offset: 8; -*-
 * vim: noexpandtab sw=8 ts=8 sts=0:
 *
 * slot_map.c
 *
 *
 *
 * Copyright (C) 2002, 2004 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/highmem.h>

#define MLOG_MASK_PREFIX ML_SUPER
#include <cluster/masklog.h>

#include "ocfs2.h"

#include "dlmglue.h"
#include "extent_map.h"
#include "heartbeat.h"
#include "inode.h"
#include "slot_map.h"
#include "super.h"
#include "sysfile.h"

#include "buffer_head_io.h"

struct ocfs2_slot_info {
	struct inode *si_inode;
	struct buffer_head *si_bh;
	unsigned int si_num_slots;
	unsigned int si_size;
	s16 si_global_node_nums[OCFS2_MAX_SLOTS];
};


static s16 __ocfs2_node_num_to_slot(struct ocfs2_slot_info *si,
				    s16 global);
static void __ocfs2_fill_slot(struct ocfs2_slot_info *si,
			      s16 slot_num,
			      s16 node_num);

/*
 * Post the slot information on disk into our slot_info struct.
 * Must be protected by osb_lock.
 */
static void ocfs2_update_slot_info(struct ocfs2_slot_info *si)
{
	int i;
	__le16 *disk_info;

	/* we don't read the slot block here as ocfs2_super_lock
	 * should've made sure we have the most recent copy. */
	disk_info = (__le16 *) si->si_bh->b_data;

	for (i = 0; i < si->si_size; i++)
		si->si_global_node_nums[i] = le16_to_cpu(disk_info[i]);
}

int ocfs2_refresh_slot_info(struct ocfs2_super *osb)
{
	int ret;
	struct ocfs2_slot_info *si = osb->slot_info;
	struct buffer_head *bh;

	if (si == NULL)
		return 0;

	bh = si->si_bh;
	ret = ocfs2_read_block(osb, bh->b_blocknr, &bh, 0, si->si_inode);
	if (ret == 0) {
		spin_lock(&osb->osb_lock);
		ocfs2_update_slot_info(si);
		spin_unlock(&osb->osb_lock);
	}

	return ret;
}

/* post the our slot info stuff into it's destination bh and write it
 * out. */
static int ocfs2_update_disk_slots(struct ocfs2_super *osb,
				   struct ocfs2_slot_info *si)
{
	int status, i;
	__le16 *disk_info = (__le16 *) si->si_bh->b_data;

	spin_lock(&osb->osb_lock);
	for (i = 0; i < si->si_size; i++)
		disk_info[i] = cpu_to_le16(si->si_global_node_nums[i]);
	spin_unlock(&osb->osb_lock);

	status = ocfs2_write_block(osb, si->si_bh, si->si_inode);
	if (status < 0)
		mlog_errno(status);

	return status;
}

/* try to find global node in the slot info. Returns
 * OCFS2_INVALID_SLOT if nothing is found. */
static s16 __ocfs2_node_num_to_slot(struct ocfs2_slot_info *si,
				    s16 global)
{
	int i;
	s16 ret = OCFS2_INVALID_SLOT;

	for(i = 0; i < si->si_num_slots; i++) {
		if (global == si->si_global_node_nums[i]) {
			ret = (s16) i;
			break;
		}
	}
	return ret;
}

static s16 __ocfs2_find_empty_slot(struct ocfs2_slot_info *si,
				   s16 preferred)
{
	int i;
	s16 ret = OCFS2_INVALID_SLOT;

	if (preferred >= 0 && preferred < si->si_num_slots) {
		if (OCFS2_INVALID_SLOT == si->si_global_node_nums[preferred]) {
			ret = preferred;
			goto out;
		}
	}

	for(i = 0; i < si->si_num_slots; i++) {
		if (OCFS2_INVALID_SLOT == si->si_global_node_nums[i]) {
			ret = (s16) i;
			break;
		}
	}
out:
	return ret;
}

int ocfs2_node_num_to_slot(struct ocfs2_super *osb, unsigned int node_num)
{
	s16 slot;
	struct ocfs2_slot_info *si = osb->slot_info;

	spin_lock(&osb->osb_lock);
	slot = __ocfs2_node_num_to_slot(si, node_num);
	spin_unlock(&osb->osb_lock);

	if (slot == OCFS2_INVALID_SLOT)
		return -ENOENT;

	return slot;
}

int ocfs2_slot_to_node_num_locked(struct ocfs2_super *osb, int slot_num,
				  unsigned int *node_num)
{
	struct ocfs2_slot_info *si = osb->slot_info;

	assert_spin_locked(&osb->osb_lock);

	BUG_ON(slot_num < 0);
	BUG_ON(slot_num > osb->max_slots);

	if (si->si_global_node_nums[slot_num] == OCFS2_INVALID_SLOT)
		return -ENOENT;

	*node_num = si->si_global_node_nums[slot_num];
	return 0;
}

static void __ocfs2_free_slot_info(struct ocfs2_slot_info *si)
{
	if (si == NULL)
		return;

	if (si->si_inode)
		iput(si->si_inode);
	if (si->si_bh)
		brelse(si->si_bh);

	kfree(si);
}

static void __ocfs2_fill_slot(struct ocfs2_slot_info *si,
			      s16 slot_num,
			      s16 node_num)
{
	BUG_ON(slot_num == OCFS2_INVALID_SLOT);
	BUG_ON(slot_num >= si->si_num_slots);
	BUG_ON((node_num != O2NM_INVALID_NODE_NUM) &&
	       (node_num >= O2NM_MAX_NODES));

	si->si_global_node_nums[slot_num] = node_num;
}

int ocfs2_clear_slot(struct ocfs2_super *osb, s16 slot_num)
{
	struct ocfs2_slot_info *si = osb->slot_info;

	if (si == NULL)
		return 0;

	spin_lock(&osb->osb_lock);
	__ocfs2_fill_slot(si, slot_num, OCFS2_INVALID_SLOT);
	spin_unlock(&osb->osb_lock);

	return ocfs2_update_disk_slots(osb, osb->slot_info);
}

int ocfs2_init_slot_info(struct ocfs2_super *osb)
{
	int status, i;
	u64 blkno;
	struct inode *inode = NULL;
	struct buffer_head *bh = NULL;
	struct ocfs2_slot_info *si;

	si = kzalloc(sizeof(struct ocfs2_slot_info), GFP_KERNEL);
	if (!si) {
		status = -ENOMEM;
		mlog_errno(status);
		goto bail;
	}

	si->si_num_slots = osb->max_slots;
	si->si_size = OCFS2_MAX_SLOTS;

	for(i = 0; i < si->si_num_slots; i++)
		si->si_global_node_nums[i] = OCFS2_INVALID_SLOT;

	inode = ocfs2_get_system_file_inode(osb, SLOT_MAP_SYSTEM_INODE,
					    OCFS2_INVALID_SLOT);
	if (!inode) {
		status = -EINVAL;
		mlog_errno(status);
		goto bail;
	}

	status = ocfs2_extent_map_get_blocks(inode, 0ULL, &blkno, NULL, NULL);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	status = ocfs2_read_block(osb, blkno, &bh, 0, inode);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

	si->si_inode = inode;
	si->si_bh = bh;
	osb->slot_info = (struct ocfs2_slot_info *)si;
bail:
	if (status < 0 && si)
		__ocfs2_free_slot_info(si);

	return status;
}

void ocfs2_free_slot_info(struct ocfs2_super *osb)
{
	struct ocfs2_slot_info *si = osb->slot_info;

	osb->slot_info = NULL;
	__ocfs2_free_slot_info(si);
}

int ocfs2_find_slot(struct ocfs2_super *osb)
{
	int status;
	s16 slot;
	struct ocfs2_slot_info *si;

	mlog_entry_void();

	si = osb->slot_info;

	spin_lock(&osb->osb_lock);
	ocfs2_update_slot_info(si);

	/* search for ourselves first and take the slot if it already
	 * exists. Perhaps we need to mark this in a variable for our
	 * own journal recovery? Possibly not, though we certainly
	 * need to warn to the user */
	slot = __ocfs2_node_num_to_slot(si, osb->node_num);
	if (slot == OCFS2_INVALID_SLOT) {
		/* if no slot yet, then just take 1st available
		 * one. */
		slot = __ocfs2_find_empty_slot(si, osb->preferred_slot);
		if (slot == OCFS2_INVALID_SLOT) {
			spin_unlock(&osb->osb_lock);
			mlog(ML_ERROR, "no free slots available!\n");
			status = -EINVAL;
			goto bail;
		}
	} else
		mlog(ML_NOTICE, "slot %d is already allocated to this node!\n",
		     slot);

	__ocfs2_fill_slot(si, slot, osb->node_num);
	osb->slot_num = slot;
	spin_unlock(&osb->osb_lock);

	mlog(0, "taking node slot %d\n", osb->slot_num);

	status = ocfs2_update_disk_slots(osb, si);
	if (status < 0)
		mlog_errno(status);

bail:
	mlog_exit(status);
	return status;
}

void ocfs2_put_slot(struct ocfs2_super *osb)
{
	int status;
	struct ocfs2_slot_info *si = osb->slot_info;

	if (!si)
		return;

	spin_lock(&osb->osb_lock);
	ocfs2_update_slot_info(si);

	__ocfs2_fill_slot(si, osb->slot_num, OCFS2_INVALID_SLOT);
	osb->slot_num = OCFS2_INVALID_SLOT;
	spin_unlock(&osb->osb_lock);

	status = ocfs2_update_disk_slots(osb, si);
	if (status < 0) {
		mlog_errno(status);
		goto bail;
	}

bail:
	ocfs2_free_slot_info(osb);
}

