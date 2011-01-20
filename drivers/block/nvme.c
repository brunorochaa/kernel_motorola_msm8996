/*
 * NVM Express device driver
 * Copyright (c) 2011, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/nvme.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>

#define NVME_Q_DEPTH 1024
#define SQ_SIZE(depth)		(depth * sizeof(struct nvme_command))
#define CQ_SIZE(depth)		(depth * sizeof(struct nvme_completion))
#define NVME_MINORS 64

static int nvme_major;
module_param(nvme_major, int, 0);

/*
 * Represents an NVM Express device.  Each nvme_dev is a PCI function.
 */
struct nvme_dev {
	struct list_head node;
	struct nvme_queue **queues;
	u32 __iomem *dbs;
	struct pci_dev *pci_dev;
	int instance;
	int queue_count;
	u32 ctrl_config;
	struct msix_entry *entry;
	struct nvme_bar __iomem *bar;
	struct list_head namespaces;
};

/*
 * An NVM Express namespace is equivalent to a SCSI LUN
 */
struct nvme_ns {
	struct list_head list;

	struct nvme_dev *dev;
	struct request_queue *queue;
	struct gendisk *disk;

	int ns_id;
	int lba_shift;
};

/*
 * An NVM Express queue.  Each device has at least two (one for admin
 * commands and one for I/O commands).
 */
struct nvme_queue {
	struct device *q_dmadev;
	spinlock_t q_lock;
	struct nvme_command *sq_cmds;
	volatile struct nvme_completion *cqes;
	dma_addr_t sq_dma_addr;
	dma_addr_t cq_dma_addr;
	wait_queue_head_t sq_full;
	struct bio_list sq_cong;
	u32 __iomem *q_db;
	u16 q_depth;
	u16 cq_vector;
	u16 sq_head;
	u16 sq_tail;
	u16 cq_head;
	u16 cq_cycle;
	unsigned long cmdid_data[];
};

/*
 * Check we didin't inadvertently grow the command struct
 */
static inline void _nvme_check_size(void)
{
	BUILD_BUG_ON(sizeof(struct nvme_rw_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_create_cq) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_create_sq) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_delete_queue) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_features) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_command) != 64);
	BUILD_BUG_ON(sizeof(struct nvme_id_ctrl) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_id_ns) != 4096);
	BUILD_BUG_ON(sizeof(struct nvme_lba_range_type) != 64);
}

/**
 * alloc_cmdid - Allocate a Command ID
 * @param nvmeq The queue that will be used for this command
 * @param ctx A pointer that will be passed to the handler
 * @param handler The ID of the handler to call
 *
 * Allocate a Command ID for a queue.  The data passed in will
 * be passed to the completion handler.  This is implemented by using
 * the bottom two bits of the ctx pointer to store the handler ID.
 * Passing in a pointer that's not 4-byte aligned will cause a BUG.
 * We can change this if it becomes a problem.
 */
static int alloc_cmdid(struct nvme_queue *nvmeq, void *ctx, int handler)
{
	int depth = nvmeq->q_depth;
	unsigned long data = (unsigned long)ctx | handler;
	int cmdid;

	BUG_ON((unsigned long)ctx & 3);

	do {
		cmdid = find_first_zero_bit(nvmeq->cmdid_data, depth);
		if (cmdid >= depth)
			return -EBUSY;
	} while (test_and_set_bit(cmdid, nvmeq->cmdid_data));

	nvmeq->cmdid_data[cmdid + BITS_TO_LONGS(depth)] = data;
	return cmdid;
}

static int alloc_cmdid_killable(struct nvme_queue *nvmeq, void *ctx,
								int handler)
{
	int cmdid;
	wait_event_killable(nvmeq->sq_full,
			(cmdid = alloc_cmdid(nvmeq, ctx, handler)) >= 0);
	return (cmdid < 0) ? -EINTR : cmdid;
}

/* If you need more than four handlers, you'll need to change how
 * alloc_cmdid and nvme_process_cq work
 */
enum {
	sync_completion_id = 0,
	bio_completion_id,
};

static unsigned long free_cmdid(struct nvme_queue *nvmeq, int cmdid)
{
	unsigned long data;

	data = nvmeq->cmdid_data[cmdid + BITS_TO_LONGS(nvmeq->q_depth)];
	clear_bit(cmdid, nvmeq->cmdid_data);
	wake_up(&nvmeq->sq_full);
	return data;
}

static struct nvme_queue *get_nvmeq(struct nvme_ns *ns)
{
	return ns->dev->queues[1];
}

static void put_nvmeq(struct nvme_queue *nvmeq)
{
}

/**
 * nvme_submit_cmd: Copy a command into a queue and ring the doorbell
 * @nvmeq: The queue to use
 * @cmd: The command to send
 *
 * Safe to use from interrupt context
 */
static int nvme_submit_cmd(struct nvme_queue *nvmeq, struct nvme_command *cmd)
{
	unsigned long flags;
	u16 tail;
	/* XXX: Need to check tail isn't going to overrun head */
	spin_lock_irqsave(&nvmeq->q_lock, flags);
	tail = nvmeq->sq_tail;
	memcpy(&nvmeq->sq_cmds[tail], cmd, sizeof(*cmd));
	writel(tail, nvmeq->q_db);
	if (++tail == nvmeq->q_depth)
		tail = 0;
	nvmeq->sq_tail = tail;
	spin_unlock_irqrestore(&nvmeq->q_lock, flags);

	return 0;
}

struct nvme_req_info {
	struct bio *bio;
	int nents;
	struct scatterlist sg[0];
};

/* XXX: use a mempool */
static struct nvme_req_info *alloc_info(unsigned nseg, gfp_t gfp)
{
	return kmalloc(sizeof(struct nvme_req_info) +
			sizeof(struct scatterlist) * nseg, gfp);
}

static void free_info(struct nvme_req_info *info)
{
	kfree(info);
}

static void bio_completion(struct nvme_queue *nvmeq, void *ctx,
						struct nvme_completion *cqe)
{
	struct nvme_req_info *info = ctx;
	struct bio *bio = info->bio;
	u16 status = le16_to_cpup(&cqe->status) >> 1;

	dma_unmap_sg(nvmeq->q_dmadev, info->sg, info->nents,
			bio_data_dir(bio) ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
	free_info(info);
	bio_endio(bio, status ? -EIO : 0);
}

static int nvme_map_bio(struct device *dev, struct nvme_req_info *info,
		struct bio *bio, enum dma_data_direction dma_dir, int psegs)
{
	struct bio_vec *bvec;
	struct scatterlist *sg = info->sg;
	int i, nsegs;

	sg_init_table(sg, psegs);
	bio_for_each_segment(bvec, bio, i) {
		sg_set_page(sg, bvec->bv_page, bvec->bv_len, bvec->bv_offset);
		/* XXX: handle non-mergable here */
		nsegs++;
	}
	info->nents = nsegs;

	return dma_map_sg(dev, info->sg, info->nents, dma_dir);
}

static int nvme_submit_bio_queue(struct nvme_queue *nvmeq, struct nvme_ns *ns,
								struct bio *bio)
{
	struct nvme_rw_command *cmnd;
	struct nvme_req_info *info;
	enum dma_data_direction dma_dir;
	int cmdid;
	u16 control;
	u32 dsmgmt;
	unsigned long flags;
	int psegs = bio_phys_segments(ns->queue, bio);

	info = alloc_info(psegs, GFP_NOIO);
	if (!info)
		goto congestion;
	info->bio = bio;

	cmdid = alloc_cmdid(nvmeq, info, bio_completion_id);
	if (unlikely(cmdid < 0))
		goto free_info;

	control = 0;
	if (bio->bi_rw & REQ_FUA)
		control |= NVME_RW_FUA;
	if (bio->bi_rw & (REQ_FAILFAST_DEV | REQ_RAHEAD))
		control |= NVME_RW_LR;

	dsmgmt = 0;
	if (bio->bi_rw & REQ_RAHEAD)
		dsmgmt |= NVME_RW_DSM_FREQ_PREFETCH;

	spin_lock_irqsave(&nvmeq->q_lock, flags);
	cmnd = &nvmeq->sq_cmds[nvmeq->sq_tail].rw;

	if (bio_data_dir(bio)) {
		cmnd->opcode = nvme_cmd_write;
		dma_dir = DMA_TO_DEVICE;
	} else {
		cmnd->opcode = nvme_cmd_read;
		dma_dir = DMA_FROM_DEVICE;
	}

	nvme_map_bio(nvmeq->q_dmadev, info, bio, dma_dir, psegs);

	cmnd->flags = 1;
	cmnd->command_id = cmdid;
	cmnd->nsid = cpu_to_le32(ns->ns_id);
	cmnd->prp1 = cpu_to_le64(sg_phys(info->sg));
	/* XXX: Support more than one PRP */
	cmnd->slba = cpu_to_le64(bio->bi_sector >> (ns->lba_shift - 9));
	cmnd->length = cpu_to_le16((bio->bi_size >> ns->lba_shift) - 1);
	cmnd->control = cpu_to_le16(control);
	cmnd->dsmgmt = cpu_to_le32(dsmgmt);

	writel(nvmeq->sq_tail, nvmeq->q_db);
	if (++nvmeq->sq_tail == nvmeq->q_depth)
		nvmeq->sq_tail = 0;

	spin_unlock_irqrestore(&nvmeq->q_lock, flags);

	return 0;

 free_info:
	free_info(info);
 congestion:
	return -EBUSY;
}

/*
 * NB: return value of non-zero would mean that we were a stacking driver.
 * make_request must always succeed.
 */
static int nvme_make_request(struct request_queue *q, struct bio *bio)
{
	struct nvme_ns *ns = q->queuedata;
	struct nvme_queue *nvmeq = get_nvmeq(ns);

	if (nvme_submit_bio_queue(nvmeq, ns, bio)) {
		blk_set_queue_congested(q, rw_is_sync(bio->bi_rw));
		bio_list_add(&nvmeq->sq_cong, bio);
	}
	put_nvmeq(nvmeq);

	return 0;
}

struct sync_cmd_info {
	struct task_struct *task;
	u32 result;
	int status;
};

static void sync_completion(struct nvme_queue *nvmeq, void *ctx,
						struct nvme_completion *cqe)
{
	struct sync_cmd_info *cmdinfo = ctx;
	cmdinfo->result = le32_to_cpup(&cqe->result);
	cmdinfo->status = le16_to_cpup(&cqe->status) >> 1;
	wake_up_process(cmdinfo->task);
}

typedef void (*completion_fn)(struct nvme_queue *, void *,
						struct nvme_completion *);

static irqreturn_t nvme_process_cq(struct nvme_queue *nvmeq)
{
	u16 head, cycle;

	static const completion_fn completions[4] = {
		[sync_completion_id] = sync_completion,
		[bio_completion_id]  = bio_completion,
	};

	head = nvmeq->cq_head;
	cycle = nvmeq->cq_cycle;

	for (;;) {
		unsigned long data;
		void *ptr;
		unsigned char handler;
		struct nvme_completion cqe = nvmeq->cqes[head];
		if ((le16_to_cpu(cqe.status) & 1) != cycle)
			break;
		nvmeq->sq_head = le16_to_cpu(cqe.sq_head);
		if (++head == nvmeq->q_depth) {
			head = 0;
			cycle = !cycle;
		}

		data = free_cmdid(nvmeq, cqe.command_id);
		handler = data & 3;
		ptr = (void *)(data & ~3UL);
		completions[handler](nvmeq, ptr, &cqe);
	}

	/* If the controller ignores the cq head doorbell and continuously
	 * writes to the queue, it is theoretically possible to wrap around
	 * the queue twice and mistakenly return IRQ_NONE.  Linux only
	 * requires that 0.1% of your interrupts are handled, so this isn't
	 * a big problem.
	 */
	if (head == nvmeq->cq_head && cycle == nvmeq->cq_cycle)
		return IRQ_NONE;

	writel(head, nvmeq->q_db + 1);
	nvmeq->cq_head = head;
	nvmeq->cq_cycle = cycle;

	return IRQ_HANDLED;
}

static irqreturn_t nvme_irq(int irq, void *data)
{
	return nvme_process_cq(data);
}

/*
 * Returns 0 on success.  If the result is negative, it's a Linux error code;
 * if the result is positive, it's an NVM Express status code
 */
static int nvme_submit_sync_cmd(struct nvme_queue *q, struct nvme_command *cmd,
								u32 *result)
{
	int cmdid;
	struct sync_cmd_info cmdinfo;

	cmdinfo.task = current;
	cmdinfo.status = -EINTR;

	cmdid = alloc_cmdid_killable(q, &cmdinfo, sync_completion_id);
	if (cmdid < 0)
		return cmdid;
	cmd->common.command_id = cmdid;

	set_current_state(TASK_UNINTERRUPTIBLE);
	nvme_submit_cmd(q, cmd);
	schedule();

	if (result)
		*result = cmdinfo.result;

	return cmdinfo.status;
}

static int nvme_submit_admin_cmd(struct nvme_dev *dev, struct nvme_command *cmd,
								u32 *result)
{
	return nvme_submit_sync_cmd(dev->queues[0], cmd, result);
}

static int adapter_delete_queue(struct nvme_dev *dev, u8 opcode, u16 id)
{
	int status;
	struct nvme_command c;

	memset(&c, 0, sizeof(c));
	c.delete_queue.opcode = opcode;
	c.delete_queue.qid = cpu_to_le16(id);

	status = nvme_submit_admin_cmd(dev, &c, NULL);
	if (status)
		return -EIO;
	return 0;
}

static int adapter_alloc_cq(struct nvme_dev *dev, u16 qid,
						struct nvme_queue *nvmeq)
{
	int status;
	struct nvme_command c;
	int flags = NVME_QUEUE_PHYS_CONTIG | NVME_CQ_IRQ_ENABLED;

	memset(&c, 0, sizeof(c));
	c.create_cq.opcode = nvme_admin_create_cq;
	c.create_cq.prp1 = cpu_to_le64(nvmeq->cq_dma_addr);
	c.create_cq.cqid = cpu_to_le16(qid);
	c.create_cq.qsize = cpu_to_le16(nvmeq->q_depth - 1);
	c.create_cq.cq_flags = cpu_to_le16(flags);
	c.create_cq.irq_vector = cpu_to_le16(nvmeq->cq_vector);

	status = nvme_submit_admin_cmd(dev, &c, NULL);
	if (status)
		return -EIO;
	return 0;
}

static int adapter_alloc_sq(struct nvme_dev *dev, u16 qid,
						struct nvme_queue *nvmeq)
{
	int status;
	struct nvme_command c;
	int flags = NVME_QUEUE_PHYS_CONTIG | NVME_SQ_PRIO_MEDIUM;

	memset(&c, 0, sizeof(c));
	c.create_sq.opcode = nvme_admin_create_sq;
	c.create_sq.prp1 = cpu_to_le64(nvmeq->sq_dma_addr);
	c.create_sq.sqid = cpu_to_le16(qid);
	c.create_sq.qsize = cpu_to_le16(nvmeq->q_depth - 1);
	c.create_sq.sq_flags = cpu_to_le16(flags);
	c.create_sq.cqid = cpu_to_le16(qid);

	status = nvme_submit_admin_cmd(dev, &c, NULL);
	if (status)
		return -EIO;
	return 0;
}

static int adapter_delete_cq(struct nvme_dev *dev, u16 cqid)
{
	return adapter_delete_queue(dev, nvme_admin_delete_cq, cqid);
}

static int adapter_delete_sq(struct nvme_dev *dev, u16 sqid)
{
	return adapter_delete_queue(dev, nvme_admin_delete_sq, sqid);
}

static void nvme_free_queue(struct nvme_dev *dev, int qid)
{
	struct nvme_queue *nvmeq = dev->queues[qid];

	free_irq(dev->entry[nvmeq->cq_vector].vector, nvmeq);

	/* Don't tell the adapter to delete the admin queue */
	if (qid) {
		adapter_delete_sq(dev, qid);
		adapter_delete_cq(dev, qid);
	}

	dma_free_coherent(nvmeq->q_dmadev, CQ_SIZE(nvmeq->q_depth),
				(void *)nvmeq->cqes, nvmeq->cq_dma_addr);
	dma_free_coherent(nvmeq->q_dmadev, SQ_SIZE(nvmeq->q_depth),
					nvmeq->sq_cmds, nvmeq->sq_dma_addr);
	kfree(nvmeq);
}

static struct nvme_queue *nvme_alloc_queue(struct nvme_dev *dev, int qid,
							int depth, int vector)
{
	struct device *dmadev = &dev->pci_dev->dev;
	unsigned extra = (depth + BITS_TO_LONGS(depth)) * sizeof(long);
	struct nvme_queue *nvmeq = kzalloc(sizeof(*nvmeq) + extra, GFP_KERNEL);
	if (!nvmeq)
		return NULL;

	nvmeq->cqes = dma_alloc_coherent(dmadev, CQ_SIZE(depth),
					&nvmeq->cq_dma_addr, GFP_KERNEL);
	if (!nvmeq->cqes)
		goto free_nvmeq;
	memset((void *)nvmeq->cqes, 0, CQ_SIZE(depth));

	nvmeq->sq_cmds = dma_alloc_coherent(dmadev, SQ_SIZE(depth),
					&nvmeq->sq_dma_addr, GFP_KERNEL);
	if (!nvmeq->sq_cmds)
		goto free_cqdma;

	nvmeq->q_dmadev = dmadev;
	spin_lock_init(&nvmeq->q_lock);
	nvmeq->cq_head = 0;
	nvmeq->cq_cycle = 1;
	init_waitqueue_head(&nvmeq->sq_full);
	bio_list_init(&nvmeq->sq_cong);
	nvmeq->q_db = &dev->dbs[qid * 2];
	nvmeq->q_depth = depth;
	nvmeq->cq_vector = vector;

	return nvmeq;

 free_cqdma:
	dma_free_coherent(dmadev, CQ_SIZE(nvmeq->q_depth), (void *)nvmeq->cqes,
							nvmeq->cq_dma_addr);
 free_nvmeq:
	kfree(nvmeq);
	return NULL;
}

static __devinit struct nvme_queue *nvme_create_queue(struct nvme_dev *dev,
					int qid, int cq_size, int vector)
{
	int result;
	struct nvme_queue *nvmeq = nvme_alloc_queue(dev, qid, cq_size, vector);

	result = adapter_alloc_cq(dev, qid, nvmeq);
	if (result < 0)
		goto free_nvmeq;

	result = adapter_alloc_sq(dev, qid, nvmeq);
	if (result < 0)
		goto release_cq;

	result = request_irq(dev->entry[vector].vector, nvme_irq,
				IRQF_DISABLED | IRQF_SHARED, "nvme", nvmeq);
	if (result < 0)
		goto release_sq;

	return nvmeq;

 release_sq:
	adapter_delete_sq(dev, qid);
 release_cq:
	adapter_delete_cq(dev, qid);
 free_nvmeq:
	dma_free_coherent(nvmeq->q_dmadev, CQ_SIZE(nvmeq->q_depth),
				(void *)nvmeq->cqes, nvmeq->cq_dma_addr);
	dma_free_coherent(nvmeq->q_dmadev, SQ_SIZE(nvmeq->q_depth),
					nvmeq->sq_cmds, nvmeq->sq_dma_addr);
	kfree(nvmeq);
	return NULL;
}

static int __devinit nvme_configure_admin_queue(struct nvme_dev *dev)
{
	int result;
	u32 aqa;
	struct nvme_queue *nvmeq;

	dev->dbs = ((void __iomem *)dev->bar) + 4096;

	nvmeq = nvme_alloc_queue(dev, 0, 64, 0);

	aqa = nvmeq->q_depth - 1;
	aqa |= aqa << 16;

	dev->ctrl_config = NVME_CC_ENABLE | NVME_CC_CSS_NVM;
	dev->ctrl_config |= (PAGE_SHIFT - 12) << NVME_CC_MPS_SHIFT;
	dev->ctrl_config |= NVME_CC_ARB_RR | NVME_CC_SHN_NONE;

	writel(aqa, &dev->bar->aqa);
	writeq(nvmeq->sq_dma_addr, &dev->bar->asq);
	writeq(nvmeq->cq_dma_addr, &dev->bar->acq);
	writel(dev->ctrl_config, &dev->bar->cc);

	while (!(readl(&dev->bar->csts) & NVME_CSTS_RDY)) {
		msleep(100);
		if (fatal_signal_pending(current))
			return -EINTR;
	}

	result = request_irq(dev->entry[0].vector, nvme_irq,
			IRQF_DISABLED | IRQF_SHARED, "nvme admin", nvmeq);
	dev->queues[0] = nvmeq;
	return result;
}

static int nvme_identify(struct nvme_ns *ns, void __user *addr, int cns)
{
	struct nvme_dev *dev = ns->dev;
	int status;
	struct nvme_command c;
	void *page;
	dma_addr_t dma_addr;

	page = dma_alloc_coherent(&dev->pci_dev->dev, 4096, &dma_addr,
								GFP_KERNEL);

	memset(&c, 0, sizeof(c));
	c.identify.opcode = nvme_admin_identify;
	c.identify.nsid = cns ? 0 : cpu_to_le32(ns->ns_id);
	c.identify.prp1 = cpu_to_le64(dma_addr);
	c.identify.cns = cpu_to_le32(cns);

	status = nvme_submit_admin_cmd(dev, &c, NULL);

	if (status)
		status = -EIO;
	else if (copy_to_user(addr, page, 4096))
		status = -EFAULT;

	dma_free_coherent(&dev->pci_dev->dev, 4096, page, dma_addr);

	return status;
}

static int nvme_get_range_type(struct nvme_ns *ns, void __user *addr)
{
	struct nvme_dev *dev = ns->dev;
	int status;
	struct nvme_command c;
	void *page;
	dma_addr_t dma_addr;

	page = dma_alloc_coherent(&dev->pci_dev->dev, 4096, &dma_addr,
								GFP_KERNEL);

	memset(&c, 0, sizeof(c));
	c.features.opcode = nvme_admin_get_features;
	c.features.nsid = cpu_to_le32(ns->ns_id);
	c.features.prp1 = cpu_to_le64(dma_addr);
	c.features.fid = cpu_to_le32(NVME_FEAT_LBA_RANGE);

	status = nvme_submit_admin_cmd(dev, &c, NULL);

	/* XXX: Assuming first range for now */
	if (status)
		status = -EIO;
	else if (copy_to_user(addr, page, 64))
		status = -EFAULT;

	dma_free_coherent(&dev->pci_dev->dev, 4096, page, dma_addr);

	return status;
}

static int nvme_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd,
							unsigned long arg)
{
	struct nvme_ns *ns = bdev->bd_disk->private_data;

	switch (cmd) {
	case NVME_IOCTL_IDENTIFY_NS:
		return nvme_identify(ns, (void __user *)arg, 0);
	case NVME_IOCTL_IDENTIFY_CTRL:
		return nvme_identify(ns, (void __user *)arg, 1);
	case NVME_IOCTL_GET_RANGE_TYPE:
		return nvme_get_range_type(ns, (void __user *)arg);
	default:
		return -ENOTTY;
	}
}

static const struct block_device_operations nvme_fops = {
	.owner		= THIS_MODULE,
	.ioctl		= nvme_ioctl,
};

static struct nvme_ns *nvme_alloc_ns(struct nvme_dev *dev, int index,
			struct nvme_id_ns *id, struct nvme_lba_range_type *rt)
{
	struct nvme_ns *ns;
	struct gendisk *disk;
	int lbaf;

	if (rt->attributes & NVME_LBART_ATTRIB_HIDE)
		return NULL;

	ns = kzalloc(sizeof(*ns), GFP_KERNEL);
	if (!ns)
		return NULL;
	ns->queue = blk_alloc_queue(GFP_KERNEL);
	if (!ns->queue)
		goto out_free_ns;
	ns->queue->queue_flags = QUEUE_FLAG_DEFAULT | QUEUE_FLAG_NOMERGES |
				QUEUE_FLAG_NONROT | QUEUE_FLAG_DISCARD;
	blk_queue_make_request(ns->queue, nvme_make_request);
	ns->dev = dev;
	ns->queue->queuedata = ns;

	disk = alloc_disk(NVME_MINORS);
	if (!disk)
		goto out_free_queue;
	ns->ns_id = index;
	ns->disk = disk;
	lbaf = id->flbas & 0xf;
	ns->lba_shift = id->lbaf[lbaf].ds;

	disk->major = nvme_major;
	disk->minors = NVME_MINORS;
	disk->first_minor = NVME_MINORS * index;
	disk->fops = &nvme_fops;
	disk->private_data = ns;
	disk->queue = ns->queue;
	sprintf(disk->disk_name, "nvme%dn%d", dev->instance, index);
	set_capacity(disk, le64_to_cpup(&id->nsze) << (ns->lba_shift - 9));

	return ns;

 out_free_queue:
	blk_cleanup_queue(ns->queue);
 out_free_ns:
	kfree(ns);
	return NULL;
}

static void nvme_ns_free(struct nvme_ns *ns)
{
	put_disk(ns->disk);
	blk_cleanup_queue(ns->queue);
	kfree(ns);
}

static int set_queue_count(struct nvme_dev *dev, int sq_count, int cq_count)
{
	int status;
	u32 result;
	struct nvme_command c;
	u32 q_count = (sq_count - 1) | ((cq_count - 1) << 16);

	memset(&c, 0, sizeof(c));
	c.features.opcode = nvme_admin_get_features;
	c.features.fid = cpu_to_le32(NVME_FEAT_NUM_QUEUES);
	c.features.dword11 = cpu_to_le32(q_count);

	status = nvme_submit_admin_cmd(dev, &c, &result);
	if (status)
		return -EIO;
	return min(result & 0xffff, result >> 16) + 1;
}

/* XXX: Create per-CPU queues */
static int __devinit nvme_setup_io_queues(struct nvme_dev *dev)
{
	int this_cpu;

	set_queue_count(dev, 1, 1);

	this_cpu = get_cpu();
	dev->queues[1] = nvme_create_queue(dev, 1, NVME_Q_DEPTH, this_cpu);
	put_cpu();
	if (!dev->queues[1])
		return -ENOMEM;
	dev->queue_count++;

	return 0;
}

static void nvme_free_queues(struct nvme_dev *dev)
{
	int i;

	for (i = dev->queue_count - 1; i >= 0; i--)
		nvme_free_queue(dev, i);
}

static int __devinit nvme_dev_add(struct nvme_dev *dev)
{
	int res, nn, i;
	struct nvme_ns *ns, *next;
	void *id;
	dma_addr_t dma_addr;
	struct nvme_command cid, crt;

	res = nvme_setup_io_queues(dev);
	if (res)
		return res;

	/* XXX: Switch to a SG list once prp2 works */
	id = dma_alloc_coherent(&dev->pci_dev->dev, 8192, &dma_addr,
								GFP_KERNEL);

	memset(&cid, 0, sizeof(cid));
	cid.identify.opcode = nvme_admin_identify;
	cid.identify.nsid = 0;
	cid.identify.prp1 = cpu_to_le64(dma_addr);
	cid.identify.cns = cpu_to_le32(1);

	res = nvme_submit_admin_cmd(dev, &cid, NULL);
	if (res) {
		res = -EIO;
		goto out_free;
	}

	nn = le32_to_cpup(&((struct nvme_id_ctrl *)id)->nn);

	cid.identify.cns = 0;
	memset(&crt, 0, sizeof(crt));
	crt.features.opcode = nvme_admin_get_features;
	crt.features.prp1 = cpu_to_le64(dma_addr + 4096);
	crt.features.fid = cpu_to_le32(NVME_FEAT_LBA_RANGE);

	for (i = 0; i < nn; i++) {
		cid.identify.nsid = cpu_to_le32(i);
		res = nvme_submit_admin_cmd(dev, &cid, NULL);
		if (res)
			continue;

		if (((struct nvme_id_ns *)id)->ncap == 0)
			continue;

		crt.features.nsid = cpu_to_le32(i);
		res = nvme_submit_admin_cmd(dev, &crt, NULL);
		if (res)
			continue;

		ns = nvme_alloc_ns(dev, i, id, id + 4096);
		if (ns)
			list_add_tail(&ns->list, &dev->namespaces);
	}
	list_for_each_entry(ns, &dev->namespaces, list)
		add_disk(ns->disk);

	dma_free_coherent(&dev->pci_dev->dev, 4096, id, dma_addr);
	return 0;

 out_free:
	list_for_each_entry_safe(ns, next, &dev->namespaces, list) {
		list_del(&ns->list);
		nvme_ns_free(ns);
	}

	dma_free_coherent(&dev->pci_dev->dev, 4096, id, dma_addr);
	return res;
}

static int nvme_dev_remove(struct nvme_dev *dev)
{
	struct nvme_ns *ns, *next;

	/* TODO: wait all I/O finished or cancel them */

	list_for_each_entry_safe(ns, next, &dev->namespaces, list) {
		list_del(&ns->list);
		del_gendisk(ns->disk);
		nvme_ns_free(ns);
	}

	nvme_free_queues(dev);

	return 0;
}

/* XXX: Use an ida or something to let remove / add work correctly */
static void nvme_set_instance(struct nvme_dev *dev)
{
	static int instance;
	dev->instance = instance++;
}

static void nvme_release_instance(struct nvme_dev *dev)
{
}

static int __devinit nvme_probe(struct pci_dev *pdev,
						const struct pci_device_id *id)
{
	int result = -ENOMEM;
	struct nvme_dev *dev;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;
	dev->entry = kcalloc(num_possible_cpus(), sizeof(*dev->entry),
								GFP_KERNEL);
	if (!dev->entry)
		goto free;
	dev->queues = kcalloc(2, sizeof(void *), GFP_KERNEL);
	if (!dev->queues)
		goto free;

	INIT_LIST_HEAD(&dev->namespaces);
	dev->pci_dev = pdev;
	pci_set_drvdata(pdev, dev);
	dma_set_mask(&dev->pci_dev->dev, DMA_BIT_MASK(64));
	nvme_set_instance(dev);

	dev->bar = ioremap(pci_resource_start(pdev, 0), 8192);
	if (!dev->bar) {
		result = -ENOMEM;
		goto disable;
	}

	result = nvme_configure_admin_queue(dev);
	if (result)
		goto unmap;
	dev->queue_count++;

	result = nvme_dev_add(dev);
	if (result)
		goto delete;
	return 0;

 delete:
	nvme_free_queues(dev);
 unmap:
	iounmap(dev->bar);
 disable:
	pci_disable_msix(pdev);
	nvme_release_instance(dev);
 free:
	kfree(dev->queues);
	kfree(dev->entry);
	kfree(dev);
	return result;
}

static void __devexit nvme_remove(struct pci_dev *pdev)
{
	struct nvme_dev *dev = pci_get_drvdata(pdev);
	nvme_dev_remove(dev);
	pci_disable_msix(pdev);
	iounmap(dev->bar);
	nvme_release_instance(dev);
	kfree(dev->queues);
	kfree(dev->entry);
	kfree(dev);
}

/* These functions are yet to be implemented */
#define nvme_error_detected NULL
#define nvme_dump_registers NULL
#define nvme_link_reset NULL
#define nvme_slot_reset NULL
#define nvme_error_resume NULL
#define nvme_suspend NULL
#define nvme_resume NULL

static struct pci_error_handlers nvme_err_handler = {
	.error_detected	= nvme_error_detected,
	.mmio_enabled	= nvme_dump_registers,
	.link_reset	= nvme_link_reset,
	.slot_reset	= nvme_slot_reset,
	.resume		= nvme_error_resume,
};

/* Move to pci_ids.h later */
#define PCI_CLASS_STORAGE_EXPRESS	0x010802

static DEFINE_PCI_DEVICE_TABLE(nvme_id_table) = {
	{ PCI_DEVICE_CLASS(PCI_CLASS_STORAGE_EXPRESS, 0xffffff) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, nvme_id_table);

static struct pci_driver nvme_driver = {
	.name		= "nvme",
	.id_table	= nvme_id_table,
	.probe		= nvme_probe,
	.remove		= __devexit_p(nvme_remove),
	.suspend	= nvme_suspend,
	.resume		= nvme_resume,
	.err_handler	= &nvme_err_handler,
};

static int __init nvme_init(void)
{
	int result;

	nvme_major = register_blkdev(nvme_major, "nvme");
	if (nvme_major <= 0)
		return -EBUSY;

	result = pci_register_driver(&nvme_driver);
	if (!result)
		return 0;

	unregister_blkdev(nvme_major, "nvme");
	return result;
}

static void __exit nvme_exit(void)
{
	pci_unregister_driver(&nvme_driver);
	unregister_blkdev(nvme_major, "nvme");
}

MODULE_AUTHOR("Matthew Wilcox <willy@linux.intel.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
module_init(nvme_init);
module_exit(nvme_exit);
