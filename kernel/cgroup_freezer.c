/*
 * cgroup_freezer.c -  control group freezer subsystem
 *
 * Copyright IBM Corporation, 2007
 *
 * Author : Cedric Le Goater <clg@fr.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/export.h>
#include <linux/slab.h>
#include <linux/cgroup.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/freezer.h>
#include <linux/seq_file.h>

enum freezer_state {
	CGROUP_THAWED = 0,
	CGROUP_FREEZING,
	CGROUP_FROZEN,
};

struct freezer {
	struct cgroup_subsys_state css;
	enum freezer_state state;
	spinlock_t lock; /* protects _writes_ to state */
};

static inline struct freezer *cgroup_freezer(
		struct cgroup *cgroup)
{
	return container_of(
		cgroup_subsys_state(cgroup, freezer_subsys_id),
		struct freezer, css);
}

static inline struct freezer *task_freezer(struct task_struct *task)
{
	return container_of(task_subsys_state(task, freezer_subsys_id),
			    struct freezer, css);
}

bool cgroup_freezing(struct task_struct *task)
{
	enum freezer_state state;
	bool ret;

	rcu_read_lock();
	state = task_freezer(task)->state;
	ret = state == CGROUP_FREEZING || state == CGROUP_FROZEN;
	rcu_read_unlock();

	return ret;
}

/*
 * cgroups_write_string() limits the size of freezer state strings to
 * CGROUP_LOCAL_BUFFER_SIZE
 */
static const char *freezer_state_strs[] = {
	"THAWED",
	"FREEZING",
	"FROZEN",
};

/*
 * State diagram
 * Transitions are caused by userspace writes to the freezer.state file.
 * The values in parenthesis are state labels. The rest are edge labels.
 *
 * (THAWED) --FROZEN--> (FREEZING) --FROZEN--> (FROZEN)
 *    ^ ^                    |                     |
 *    | \_______THAWED_______/                     |
 *    \__________________________THAWED____________/
 */

struct cgroup_subsys freezer_subsys;

/* Locks taken and their ordering
 * ------------------------------
 * cgroup_mutex (AKA cgroup_lock)
 * freezer->lock
 * css_set_lock
 * task->alloc_lock (AKA task_lock)
 * task->sighand->siglock
 *
 * cgroup code forces css_set_lock to be taken before task->alloc_lock
 *
 * freezer_create(), freezer_destroy():
 * cgroup_mutex [ by cgroup core ]
 *
 * freezer_can_attach():
 * cgroup_mutex (held by caller of can_attach)
 *
 * freezer_fork() (preserving fork() performance means can't take cgroup_mutex):
 * freezer->lock
 *  sighand->siglock (if the cgroup is freezing)
 *
 * freezer_read():
 * cgroup_mutex
 *  freezer->lock
 *   write_lock css_set_lock (cgroup iterator start)
 *    task->alloc_lock
 *   read_lock css_set_lock (cgroup iterator start)
 *
 * freezer_write() (freeze):
 * cgroup_mutex
 *  freezer->lock
 *   write_lock css_set_lock (cgroup iterator start)
 *    task->alloc_lock
 *   read_lock css_set_lock (cgroup iterator start)
 *    sighand->siglock (fake signal delivery inside freeze_task())
 *
 * freezer_write() (unfreeze):
 * cgroup_mutex
 *  freezer->lock
 *   write_lock css_set_lock (cgroup iterator start)
 *    task->alloc_lock
 *   read_lock css_set_lock (cgroup iterator start)
 *    task->alloc_lock (inside __thaw_task(), prevents race with refrigerator())
 *     sighand->siglock
 */
static struct cgroup_subsys_state *freezer_create(struct cgroup *cgroup)
{
	struct freezer *freezer;

	freezer = kzalloc(sizeof(struct freezer), GFP_KERNEL);
	if (!freezer)
		return ERR_PTR(-ENOMEM);

	spin_lock_init(&freezer->lock);
	freezer->state = CGROUP_THAWED;
	return &freezer->css;
}

static void freezer_destroy(struct cgroup *cgroup)
{
	struct freezer *freezer = cgroup_freezer(cgroup);

	if (freezer->state != CGROUP_THAWED)
		atomic_dec(&system_freezing_cnt);
	kfree(freezer);
}

/*
 * The call to cgroup_lock() in the freezer.state write method prevents
 * a write to that file racing against an attach, and hence we don't need
 * to worry about racing against migration.
 */
static void freezer_attach(struct cgroup *new_cgrp, struct cgroup_taskset *tset)
{
	struct freezer *freezer = cgroup_freezer(new_cgrp);
	struct task_struct *task;

	spin_lock_irq(&freezer->lock);

	/*
	 * Make the new tasks conform to the current state of @new_cgrp.
	 * For simplicity, when migrating any task to a FROZEN cgroup, we
	 * revert it to FREEZING and let update_if_frozen() determine the
	 * correct state later.
	 *
	 * Tasks in @tset are on @new_cgrp but may not conform to its
	 * current state before executing the following - !frozen tasks may
	 * be visible in a FROZEN cgroup and frozen tasks in a THAWED one.
	 * This means that, to determine whether to freeze, one should test
	 * whether the state equals THAWED.
	 */
	cgroup_taskset_for_each(task, new_cgrp, tset) {
		if (freezer->state == CGROUP_THAWED) {
			__thaw_task(task);
		} else {
			freeze_task(task);
			freezer->state = CGROUP_FREEZING;
		}
	}

	spin_unlock_irq(&freezer->lock);
}

static void freezer_fork(struct task_struct *task)
{
	struct freezer *freezer;

	rcu_read_lock();
	freezer = task_freezer(task);

	/*
	 * The root cgroup is non-freezable, so we can skip the
	 * following check.
	 */
	if (!freezer->css.cgroup->parent)
		goto out;

	spin_lock_irq(&freezer->lock);
	/*
	 * @task might have been just migrated into a FROZEN cgroup.  Test
	 * equality with THAWED.  Read the comment in freezer_attach().
	 */
	if (freezer->state != CGROUP_THAWED)
		freeze_task(task);
	spin_unlock_irq(&freezer->lock);
out:
	rcu_read_unlock();
}

/*
 * We change from FREEZING to FROZEN lazily if the cgroup was only
 * partially frozen when we exitted write.  Caller must hold freezer->lock.
 *
 * Task states and freezer state might disagree while tasks are being
 * migrated into @cgroup, so we can't verify task states against @freezer
 * state here.  See freezer_attach() for details.
 */
static void update_if_frozen(struct cgroup *cgroup, struct freezer *freezer)
{
	struct cgroup_iter it;
	struct task_struct *task;

	if (freezer->state != CGROUP_FREEZING)
		return;

	cgroup_iter_start(cgroup, &it);

	while ((task = cgroup_iter_next(cgroup, &it))) {
		if (freezing(task)) {
			/*
			 * freezer_should_skip() indicates that the task
			 * should be skipped when determining freezing
			 * completion.  Consider it frozen in addition to
			 * the usual frozen condition.
			 */
			if (!frozen(task) && !task_is_stopped_or_traced(task) &&
			    !freezer_should_skip(task))
				goto notyet;
		}
	}

	freezer->state = CGROUP_FROZEN;
notyet:
	cgroup_iter_end(cgroup, &it);
}

static int freezer_read(struct cgroup *cgroup, struct cftype *cft,
			struct seq_file *m)
{
	struct freezer *freezer;
	enum freezer_state state;

	if (!cgroup_lock_live_group(cgroup))
		return -ENODEV;

	freezer = cgroup_freezer(cgroup);
	spin_lock_irq(&freezer->lock);
	update_if_frozen(cgroup, freezer);
	state = freezer->state;
	spin_unlock_irq(&freezer->lock);
	cgroup_unlock();

	seq_puts(m, freezer_state_strs[state]);
	seq_putc(m, '\n');
	return 0;
}

static void freeze_cgroup(struct cgroup *cgroup, struct freezer *freezer)
{
	struct cgroup_iter it;
	struct task_struct *task;

	cgroup_iter_start(cgroup, &it);
	while ((task = cgroup_iter_next(cgroup, &it)))
		freeze_task(task);
	cgroup_iter_end(cgroup, &it);
}

static void unfreeze_cgroup(struct cgroup *cgroup, struct freezer *freezer)
{
	struct cgroup_iter it;
	struct task_struct *task;

	cgroup_iter_start(cgroup, &it);
	while ((task = cgroup_iter_next(cgroup, &it)))
		__thaw_task(task);
	cgroup_iter_end(cgroup, &it);
}

static void freezer_change_state(struct cgroup *cgroup,
				 enum freezer_state goal_state)
{
	struct freezer *freezer = cgroup_freezer(cgroup);

	spin_lock_irq(&freezer->lock);

	switch (goal_state) {
	case CGROUP_THAWED:
		if (freezer->state != CGROUP_THAWED)
			atomic_dec(&system_freezing_cnt);
		freezer->state = CGROUP_THAWED;
		unfreeze_cgroup(cgroup, freezer);
		break;
	case CGROUP_FROZEN:
		if (freezer->state == CGROUP_THAWED)
			atomic_inc(&system_freezing_cnt);
		freezer->state = CGROUP_FREEZING;
		freeze_cgroup(cgroup, freezer);
		break;
	default:
		BUG();
	}

	spin_unlock_irq(&freezer->lock);
}

static int freezer_write(struct cgroup *cgroup,
			 struct cftype *cft,
			 const char *buffer)
{
	enum freezer_state goal_state;

	if (strcmp(buffer, freezer_state_strs[CGROUP_THAWED]) == 0)
		goal_state = CGROUP_THAWED;
	else if (strcmp(buffer, freezer_state_strs[CGROUP_FROZEN]) == 0)
		goal_state = CGROUP_FROZEN;
	else
		return -EINVAL;

	if (!cgroup_lock_live_group(cgroup))
		return -ENODEV;
	freezer_change_state(cgroup, goal_state);
	cgroup_unlock();
	return 0;
}

static struct cftype files[] = {
	{
		.name = "state",
		.flags = CFTYPE_NOT_ON_ROOT,
		.read_seq_string = freezer_read,
		.write_string = freezer_write,
	},
	{ }	/* terminate */
};

struct cgroup_subsys freezer_subsys = {
	.name		= "freezer",
	.create		= freezer_create,
	.destroy	= freezer_destroy,
	.subsys_id	= freezer_subsys_id,
	.attach		= freezer_attach,
	.fork		= freezer_fork,
	.base_cftypes	= files,

	/*
	 * freezer subsys doesn't handle hierarchy at all.  Frozen state
	 * should be inherited through the hierarchy - if a parent is
	 * frozen, all its children should be frozen.  Fix it and remove
	 * the following.
	 */
	.broken_hierarchy = true,
};
