/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "../xocl_drv.h"

#define	ERT_MAX_SLOTS		128

#define	ERT_STATE_GOOD		0x1
#define	ERT_STATE_BAD		0x2

//#define	SCHED_VERBOSE	1

#ifdef SCHED_VERBOSE
#define	ERTVERSAL_ERR(ert_versal, fmt, arg...)	\
	xocl_err(ert_versal->dev, fmt "", ##arg)
#define	ERTVERSAL_INFO(ert_versal, fmt, arg...)	\
	xocl_info(ert_versal->dev, fmt "", ##arg)	
#define	ERTVERSAL_DBG(ert_versal, fmt, arg...)	\
	xocl_info(ert_versal->dev, fmt "", ##arg)
#else
#define	ERTVERSAL_ERR(ert_versal, fmt, arg...)	\
	xocl_err(ert_versal->dev, fmt "", ##arg)
#define	ERTVERSAL_INFO(ert_versal, fmt, arg...)	\
	xocl_info(ert_versal->dev, fmt "", ##arg)	
#define	ERTVERSAL_DBG(ert_versal, fmt, arg...)
#endif


#define sched_debug_packet(packet, size)				\
({									\
	int i;								\
	u32 *data = (u32 *)packet;					\
	for (i = 0; i < size; ++i)					    \
		DRM_INFO("packet(0x%p) execbuf[%d] = 0x%x\n", data, i, data[i]); \
})

struct ert_versal_event {
	struct mutex		  lock;
	void			 *client;
	int			  state;
};

struct ert_versal_command {
	struct kds_command *xcmd;
	struct list_head    list;
	uint32_t	slot_idx;
};

struct xocl_ert_versal {
	struct device		*dev;
	struct platform_device	*pdev;
	void __iomem		*cq_base;
	uint64_t		cq_range;
	bool			polling_mode;
	struct mutex 		lock;
	struct kds_ert		ert;


	/* Configure dynamically */ 
	unsigned int		num_slots;
	bool			cq_intr;
	bool			config;
	bool			ctrl_busy;
	// Bitmap tracks busy(1)/free(0) slots in command_queue
	DECLARE_BITMAP(slot_status, ERT_MAX_SLOTS);
	struct xocl_ert_sched_privdata ert_cfg_priv;

	struct list_head	pq;
	spinlock_t		pq_lock;
	u32			num_pq;
	/*
	 * Pending Q is used in thread that is submitting CU cmds.
	 * Other Qs are used in thread that is completing them.
	 * In order to prevent false sharing, they need to be in different
	 * cache lines. Hence we add a "padding" in between (assuming 128-byte
	 * is big enough for most CPU architectures).
	 */
	u64			padding[16];
	/* run queue */
	struct list_head	rq;
	u32			num_rq;
	/* completed queue */
	struct list_head	cq;
	u32			num_cq;
	struct semaphore	sem;
	/* submitted queue */
	struct ert_versal_command	*submit_queue[ERT_MAX_SLOTS];
	spinlock_t		sq_lock;
	u32			num_sq;


	u32			stop;
	bool			bad_state;

	struct ert_versal_event	ev;

	struct task_struct	*thread;
};

static const unsigned int no_index = -1;
static void ert_versal_reset(struct xocl_ert_versal *ert_versal);

static ssize_t name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	//struct xocl_ert_versal *ert_versal = platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "ert_versal");
}

static DEVICE_ATTR_RO(name);

static struct attribute *ert_versal_attrs[] = {
	&dev_attr_name.attr,
	NULL,
};

static struct attribute_group ert_versal_attr_group = {
	.attrs = ert_versal_attrs,
};

static void ert_versal_free_cmd(struct ert_versal_command* ecmd)
{
	vfree(ecmd);
}

static struct ert_versal_command* ert_versal_alloc_cmd(struct kds_command *xcmd)
{
	struct ert_versal_command* ecmd = vzalloc(sizeof(struct ert_versal_command));

	if (!ecmd)
		return NULL;

	ecmd->xcmd = xcmd;

	return ecmd;
}

/*
 * type() - Command type
 *
 * @cmd: Command object
 * Return: Type of command
 */
static inline u32
cmd_opcode(struct ert_versal_command *ecmd)
{
	return ecmd->xcmd->opcode;
}


/* Use for flush queue */
static inline void
flush_queue(struct list_head *q, u32 *len, int status, void *client)
{
	struct kds_command *xcmd;
	struct ert_versal_command *ecmd, *next;

	if (*len == 0)
		return;

	list_for_each_entry_safe(ecmd, next, q, list) {
		xcmd = ecmd->xcmd;
		if (client && client != xcmd->client)
			continue;
		xcmd->cb.notify_host(xcmd, status);
		list_del(&ecmd->list);
		xcmd->cb.free(xcmd);
		ert_versal_free_cmd(ecmd);
		--(*len);
	}
}

/* Use for flush submit queue */
static void
flush_submit_queue(struct xocl_ert_versal *ert_versal, u32 *len, int status, void *client)
{
	struct kds_command *xcmd;
	struct ert_versal_command *ecmd;
	u32 i = 0;
	unsigned long flags;


	spin_lock_irqsave(&ert_versal->sq_lock, flags);
	for ( i = 0; i < ERT_MAX_SLOTS; ++i ) {
		ecmd = ert_versal->submit_queue[i];
		if (ecmd) {
			xcmd = ecmd->xcmd;
			if (client && client != xcmd->client)
				continue;
			xcmd->cb.notify_host(xcmd, status);
			xcmd->cb.free(xcmd);
			ert_versal->submit_queue[i] = NULL;
			ert_versal_free_cmd(ecmd);
			--ert_versal->num_sq;
		}
	}
	spin_unlock_irqrestore(&ert_versal->sq_lock, flags);
}
/*
 * release_slot_idx() - Release specified slot idx
 */
static void
ert_release_slot_idx(struct xocl_ert_versal *ert_versal, unsigned int slot_idx)
{
	clear_bit(slot_idx, ert_versal->slot_status);
}

/**
 * release_slot() - Release a slot index for a command
 *
 * Special case for control commands that execute in slot 0.  This
 * slot cannot be marked free ever.
 */
static void
ert_release_slot(struct xocl_ert_versal *ert_versal, struct ert_versal_command *ecmd)
{
	if (ecmd->slot_idx == no_index)
		return;

	if (cmd_opcode(ecmd) == OP_CONFIG) {
		ERTVERSAL_DBG(ert_versal, "do nothing %s\n", __func__);
		ert_versal->ctrl_busy = false;
		ert_versal->config = true;	
	} else {
		ERTVERSAL_DBG(ert_versal, "ecmd->slot_idx %d\n", ecmd->slot_idx);
		ert_release_slot_idx(ert_versal, ecmd->slot_idx);
	}
	ecmd->slot_idx = no_index;
}

/**
 * process_ert_cq() - Process completed queue
 * @ert_versal: Target XRT CU
 */
static inline void process_ert_cq(struct xocl_ert_versal *ert_versal)
{
	struct kds_command *xcmd;
	struct ert_versal_command *ecmd;
	unsigned long flags = 0;

	if (!ert_versal->num_cq)
		return;

	ERTVERSAL_DBG(ert_versal, "-> %s\n", __func__);
	spin_lock_irqsave(&ert_versal->sq_lock, flags);
	/* Notify host and free command */
	ecmd = list_first_entry(&ert_versal->cq, struct ert_versal_command, list);
	xcmd = ecmd->xcmd;
	ERTVERSAL_DBG(ert_versal, "%s -> ecmd %llx xcmd%p\n", __func__, (u64)ecmd, xcmd);
	ert_release_slot(ert_versal, ecmd);
	list_del(&ecmd->list);
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	xcmd->cb.free(xcmd);
	ert_versal_free_cmd(ecmd);
	--ert_versal->num_cq;
	spin_unlock_irqrestore(&ert_versal->sq_lock, flags);
	ERTVERSAL_DBG(ert_versal, "<- %s\n", __func__);
}

/**
 * mask_idx32() - Slot mask idx index for a given slot_idx
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of the slot mask containing the slot_idx
 */
static inline unsigned int
mask_idx32(unsigned int idx)
{
	return idx >> 5;
}

static irqreturn_t
ert_versal_isr(void *arg)
{
	struct xocl_ert_versal *ert_versal = (struct xocl_ert_versal *)arg;
	xdev_handle_t xdev;
	struct ert_versal_command *ecmd;

	if (!ert_versal)
		return IRQ_HANDLED;

	ERTVERSAL_DBG(ert_versal, "-> %s \n", __func__);

	xdev = xocl_get_xdev(ert_versal->pdev);

	if (!ert_versal->polling_mode) {
		u32 slots[ERT_MAX_SLOTS];
		u32 cnt = 0;
		int slot;
		int i;

		while (!(xocl_mailbox_versal_get(xdev, &slot)))
			slots[cnt++] = slot;

		if (!cnt)
			return IRQ_HANDLED;

		spin_lock(&ert_versal->sq_lock);
		for (i = 0; i < cnt; i++) {
			slot = slots[i];
			ERTVERSAL_DBG(ert_versal, "slot %d\n", slot);
			ecmd = ert_versal->submit_queue[slot];
			if (ecmd) {
				ert_versal->submit_queue[slot] = NULL;
				list_add_tail(&ecmd->list, &ert_versal->cq);
				ERTVERSAL_DBG(ert_versal, "move to cq\n");
				--ert_versal->num_sq;
				++ert_versal->num_cq;			
			}
		}
		spin_unlock(&ert_versal->sq_lock);

		up(&ert_versal->sem);
		/* wake up all scheduler ... currently one only */
#if 0
		if (xs->stop)
			return;

		if (xs->reset) {
			SCHED_DEBUG("scheduler is resetting after timeout\n");
			scheduler_reset(xs);
		}
#endif
	} else if (ert_versal) {
		//ERTVERSAL_DBG(ert_versal, "unhandled isr irq %d", irq);
	}

	//ERTVERSAL_DBG(ert_versal, "<- xocl_user_event %d\n", irq);
	return IRQ_HANDLED;
}

/**
 * process_ert_sq() - Process submitted queue
 * @ert_versal: Target XRT CU
 */
static inline void process_ert_sq(struct xocl_ert_versal *ert_versal)
{
	struct ert_versal_command *ecmd;
	u32 mask = 0;
	u32 slot_idx = 0, section_idx = 0;
	unsigned long flags;
	xdev_handle_t xdev = xocl_get_xdev(ert_versal->pdev);

	if (!ert_versal->num_sq)
		return;

	if (!ert_versal->polling_mode)
		return;

	for (section_idx = 0; section_idx < 4; ++section_idx) {
		mask = xocl_intc_ert_read32(xdev, (section_idx<<2));
		if (!mask)
			continue;
		ERTVERSAL_DBG(ert_versal, "mask 0x%x\n", mask);
		for ( slot_idx = 0; slot_idx < 32; mask>>=1, ++slot_idx ) {
			u32 cmd_idx = slot_idx+(section_idx<<5);

			if (!mask)
				break;
			if (mask & 0x1) {
				spin_lock_irqsave(&ert_versal->sq_lock, flags);
				if (ert_versal->submit_queue[cmd_idx]) {
					ecmd = ert_versal->submit_queue[cmd_idx];

					ert_versal->submit_queue[cmd_idx] = NULL;
					list_add_tail(&ecmd->list, &ert_versal->cq);
					ERTVERSAL_DBG(ert_versal, "move to cq\n");
					--ert_versal->num_sq;
					++ert_versal->num_cq;
				} else
					ERTVERSAL_DBG(ert_versal, "ERR: submit queue slot is empty\n");

				spin_unlock_irqrestore(&ert_versal->sq_lock, flags);
			}
		}
	}
}

/*
 * acquire_slot_idx() - First available slot index
 */
static unsigned int
ert_acquire_slot_idx(struct xocl_ert_versal *ert_versal)
{
	unsigned int idx = find_first_zero_bit(ert_versal->slot_status, ERT_MAX_SLOTS);

	if (idx < ert_versal->num_slots) {
		set_bit(idx, ert_versal->slot_status);
		return idx;
	}
	return no_index;
}

/**
 * idx_in_mask32() - Index of command queue slot within the mask that contains it
 *
 * @slot_idx: Global [0..127] index of a CQ slot
 * Return: Index of slot within the mask that contains it
 */
static inline unsigned int
idx_in_mask32(unsigned int idx, unsigned int mask_idx)
{
	return idx - (mask_idx << 5);
}

/**
 * acquire_slot() - Acquire a slot index for a command
 *
 * This function makes a special case for control commands which
 * must always dispatch to slot 0, otherwise normal acquisition
 */
static int
ert20_acquire_slot(struct xocl_ert_versal *ert_versal, struct ert_versal_command *ecmd)
{
	// slot 0 is reserved for ctrl commands
	if (cmd_opcode(ecmd) == OP_CONFIG) {
		set_bit(0, ert_versal->slot_status);

		if (ert_versal->ctrl_busy) {
			ERTVERSAL_ERR(ert_versal, "ctrl slot is busy\n");
			return -1;
		}
		ert_versal->ctrl_busy = true;
		return (ecmd->slot_idx = 0);
	}

	return (ecmd->slot_idx = ert_acquire_slot_idx(ert_versal));
}


static int ert_cfg_cmd(struct xocl_ert_versal *ert_versal, struct ert_versal_command *ecmd)
{
	xdev_handle_t xdev = xocl_get_xdev(ert_versal->pdev);
	uint32_t *cdma = xocl_rom_cdma_addr(xdev);
	unsigned int dsa = ert_versal->ert_cfg_priv.dsa;
	unsigned int major = ert_versal->ert_cfg_priv.major;
	struct ert_configure_cmd *cfg = (struct ert_configure_cmd *)ecmd->xcmd->execbuf;
	bool ert = (XOCL_DSA_IS_VERSAL(xdev) || XOCL_DSA_IS_MPSOC(xdev)) ? 1 :
	    xocl_mb_sched_on(xdev);
	bool ert_full = (ert && cfg->ert && !cfg->dataflow);
	bool ert_poll = (ert && cfg->ert && cfg->dataflow);
	unsigned int ert_num_slots = 0;

	if (cmd_opcode(ecmd) != OP_CONFIG)
		return -EINVAL;

	if (major > 2) {
		DRM_INFO("Unknown ERT major version, fallback to KDS mode\n");
		ert_full = 0;
		ert_poll = 0;
	}

	ERTVERSAL_DBG(ert_versal, "ert per feature rom = %d", ert);
	ERTVERSAL_DBG(ert_versal, "dsa52 = %d", dsa);

	if (XOCL_DSA_IS_VERSAL(xdev) || XOCL_DSA_IS_MPSOC(xdev)) {
		ERTVERSAL_INFO(ert_versal, "MPSoC polling mode %d", cfg->polling);

		// For MPSoC device, we will use ert_full if we are
		// configured as ert mode even dataflow is configured.
		// And we do not support ert_poll.
		ert_full = cfg->ert;
		ert_poll = false;
	}

	// Mark command as control command to force slot 0 execution
	//cfg->type = ERT_CTRL;

	ERTVERSAL_DBG(ert_versal, "configuring scheduler cq_size(%lld)\n", ert_versal->cq_range);
	if (ert_versal->cq_range == 0 || cfg->slot_size == 0) {
		ERTVERSAL_ERR(ert_versal, "should not have zeroed value of cq_size=%lld, slot_size=%d",
		    ert_versal->cq_range, cfg->slot_size);
		return -EINVAL;
	}

	ert_num_slots = ert_versal->cq_range / cfg->slot_size;

	if (ert_poll)
		// Adjust slot size for ert poll mode
		cfg->slot_size = ert_versal->cq_range / MAX_CUS;

	if (ert_full && cfg->cu_dma && ert_num_slots > 32) {
		// Max slot size is 32 because of cudma bug
		ERTVERSAL_INFO(ert_versal, "Limitting CQ size to 32 due to ERT CUDMA bug\n");
		ert_num_slots = 32;
		cfg->slot_size = ert_versal->cq_range / ert_num_slots;
	}

	if (ert_poll) {
		ERTVERSAL_INFO(ert_versal, "configuring dataflow mode with ert polling\n");
		cfg->slot_size = ert_versal->cq_range / MAX_CUS;
		cfg->cu_isr = 0;
		cfg->cu_dma = 0;
		ert_versal->polling_mode = cfg->polling;
		ert_versal->num_slots = ert_versal->cq_range / cfg->slot_size;
	} else if (ert_full) {
		ERTVERSAL_INFO(ert_versal, "configuring embedded scheduler mode\n");
		ert_versal->cq_intr = cfg->cq_int;
		ert_versal->polling_mode = cfg->polling;
		ert_versal->num_slots = ert_versal->cq_range / cfg->slot_size;
		cfg->dsa52 = dsa;
		cfg->cdma = cdma ? 1 : 0;
	}

	if (XDEV(xdev)->priv.flags & XOCL_DSAFLAG_CUDMA_OFF)
		cfg->cu_dma = 0;

	// The KDS side of of the scheduler is now configured.  If ERT is
	// enabled, then the configure command will be started asynchronously
	// on ERT.  The shceduler is not marked configured until ERT has
	// completed (exec_finish_cmd); this prevents other processes from
	// submitting commands to same xclbin.  However we must also stop
	// other processes from submitting configure command on this same
	// xclbin while ERT asynchronous configure is running.
	//exec->configure_active = true;

	ERTVERSAL_INFO(ert_versal, "scheduler config ert(%d), dataflow(%d), slots(%d), cudma(%d), cuisr(%d)\n"
		 , ert_poll | ert_full
		 , cfg->dataflow
		 , ert_versal->num_slots
		 , cfg->cu_dma ? 1 : 0
		 , cfg->cu_isr ? 1 : 0);

	// TODO: reset all queues
	ert_versal_reset(ert_versal);

	return 0;
}
/**
 * process_ert_rq() - Process run queue
 * @ert_versal: Target XRT CU
 *
 * Return: return 0 if run queue is empty or no credit
 *	   Otherwise, return 1
 */
static inline int process_ert_rq(struct xocl_ert_versal *ert_versal)
{
	struct ert_versal_command *ecmd, *next;
	u32 slot_addr = 0;
	struct ert_packet *epkt = NULL;
	xdev_handle_t xdev = xocl_get_xdev(ert_versal->pdev);
	unsigned long flags;

	if (!ert_versal->num_rq)
		return 0;

	list_for_each_entry_safe(ecmd, next, &ert_versal->rq, list) {

		if (cmd_opcode(ecmd) == OP_CONFIG) {
			if (ert_cfg_cmd(ert_versal, ecmd)) {
				struct kds_command *xcmd;

				ERTVERSAL_ERR(ert_versal, "%s config cmd error\n", __func__);
				list_del(&ecmd->list);
				xcmd = ecmd->xcmd;
				xcmd->cb.notify_host(xcmd, KDS_ABORT);
				xcmd->cb.free(xcmd);
				ert_versal_free_cmd(ecmd);
				--ert_versal->num_rq;
				continue;
			}
		}

		if (ert20_acquire_slot(ert_versal, ecmd) == no_index) {
			ERTVERSAL_DBG(ert_versal, "%s not slot available\n", __func__);
			return 0;
		}
		epkt = (struct ert_packet *)ecmd->xcmd->execbuf;
		ERTVERSAL_DBG(ert_versal, "%s op_code %d ecmd->slot_idx %d\n", __func__, cmd_opcode(ecmd), ecmd->slot_idx);

		//sched_debug_packet(epkt, epkt->count+sizeof(epkt->header)/sizeof(u32));

		if (cmd_opcode(ecmd) == OP_CONFIG && !ert_versal->polling_mode) {
			xocl_mailbox_versal_request_intr(xdev, ert_versal_isr, ert_versal);
		}
		slot_addr = ecmd->slot_idx * (ert_versal->cq_range/ert_versal->num_slots);

		ERTVERSAL_DBG(ert_versal, "%s slot_addr %x\n", __func__, slot_addr);
		if (cmd_opcode(ecmd) == OP_CONFIG) {
			xocl_memcpy_toio(ert_versal->cq_base + slot_addr + 4,
				  ecmd->xcmd->execbuf+1, epkt->count*sizeof(u32));
		} else {
			// write kds selected cu_idx in first cumask (first word after header)
			iowrite32(ecmd->xcmd->cu_idx, ert_versal->cq_base + slot_addr + 4);

			// write remaining packet (past header and cuidx)
			xocl_memcpy_toio(ert_versal->cq_base + slot_addr + 8,
					 ecmd->xcmd->execbuf+2, (epkt->count-1)*sizeof(u32));
		}

		iowrite32(epkt->header, ert_versal->cq_base + slot_addr);

		if (ert_versal->cq_intr) {
			u32 mask_idx = mask_idx32(ecmd->slot_idx);
			u32 cq_int_addr = (mask_idx << 2);
			u32 mask = 1 << idx_in_mask32(ecmd->slot_idx, mask_idx);

			ERTVERSAL_DBG(ert_versal, "++ mb_submit writes slot mask 0x%x to CQ_INT register at addr 0x%x\n",
					mask, cq_int_addr);
			xocl_intc_ert_write32(xdev, mask, cq_int_addr);
		}
		spin_lock_irqsave(&ert_versal->sq_lock, flags);
		ert_versal->submit_queue[ecmd->slot_idx] = ecmd;
		list_del(&ecmd->list);
		--ert_versal->num_rq;
		++ert_versal->num_sq;
		spin_unlock_irqrestore(&ert_versal->sq_lock, flags);
	}

	return 1;
}

/**
 * process_ert_rq() - Process pending queue
 * @ert_versal: Target XRT CU
 *
 * Move all of the pending queue commands to the tail of run queue
 * and re-initialized pending queue
 */
static inline void process_ert_pq(struct xocl_ert_versal *ert_versal)
{
	unsigned long flags;

	/* Get pending queue command number without lock.
	 * The idea is to reduce the possibility of conflict on lock.
	 * Need to check pending command number again after lock.
	 */
	if (!ert_versal->num_pq)
		return;
	spin_lock_irqsave(&ert_versal->pq_lock, flags);
	if (ert_versal->num_pq) {
		list_splice_tail_init(&ert_versal->pq, &ert_versal->rq);
		ert_versal->num_rq += ert_versal->num_pq;
		ert_versal->num_pq = 0;
	}
	spin_unlock_irqrestore(&ert_versal->pq_lock, flags);
}

/**
 * process_event() - Process event
 * @ert_versal: Target XRT CU
 *
 * This is used to process low frequency events.
 * For example, client abort event would happen when closing client.
 * Before the client close, make sure all of the client commands have
 * been handle properly.
 */
static inline void process_event(struct xocl_ert_versal *ert_versal)
{
	void *client = NULL;

	mutex_lock(&ert_versal->ev.lock);
	if (!ert_versal->ev.client)
		goto done;

	client = ert_versal->ev.client;

	flush_queue(&ert_versal->rq, &ert_versal->num_rq, KDS_ABORT, client);

	/* Let's check submitted commands one more time */
	process_ert_sq(ert_versal);
	if (ert_versal->num_sq) {
		flush_submit_queue(ert_versal, &ert_versal->num_sq, KDS_ABORT, client);
		ert_versal->ev.state = ERT_STATE_BAD;
	}

	while (ert_versal->num_cq)
		process_ert_cq(ert_versal);

	/* Maybe pending queue has commands of this client */
	process_ert_pq(ert_versal);
	flush_queue(&ert_versal->rq, &ert_versal->num_rq, KDS_ABORT, client);

	if (!ert_versal->ev.state)
		ert_versal->ev.state = ERT_STATE_GOOD;
done:
	mutex_unlock(&ert_versal->ev.lock);
}


static void ert_versal_reset(struct xocl_ert_versal *ert_versal)
{
	process_event(ert_versal);
	bitmap_zero(ert_versal->slot_status, ERT_MAX_SLOTS);
}

static void ert_versal_submit(struct kds_ert *ert, struct kds_command *xcmd)
{
	unsigned long flags;
	bool first_command = false;
	struct xocl_ert_versal *ert_versal = container_of(ert, struct xocl_ert_versal, ert);
	struct ert_versal_command *ecmd = ert_versal_alloc_cmd(xcmd);

	if (!ecmd)
		return;

	ERTVERSAL_DBG(ert_versal, "->%s ecmd %llx\n", __func__, (u64)ecmd);
	spin_lock_irqsave(&ert_versal->pq_lock, flags);
	list_add_tail(&ecmd->list, &ert_versal->pq);
	++ert_versal->num_pq;
	first_command = (ert_versal->num_pq == 1);
	spin_unlock_irqrestore(&ert_versal->pq_lock, flags);
	/* Add command to pending queue
	 * wakeup service thread if it is the first command
	 */
	if (first_command)
		up(&ert_versal->sem);

	ERTVERSAL_DBG(ert_versal, "<-%s\n", __func__);
	return;
}

int ert_versal_thread(void *data)
{
	struct xocl_ert_versal *ert_versal = (struct xocl_ert_versal *)data;
	int ret = 0;
	bool polling_sleep = false, intr_sleep = false;

	while (!ert_versal->stop) {
		/* Make sure to submit as many commands as possible.
		 * This is why we call continue here. This is important to make
		 * CU busy, especially CU has hardware queue.
		 */
		if (process_ert_rq(ert_versal))
			continue;
		/* process completed queue before submitted queue, for
		 * two reasons:
		 * - The last submitted command may be still running
		 * - while handling completed queue, running command might done
		 * - process_ert_sq will check CU status, which is thru slow bus
		 */
		process_ert_cq(ert_versal);
		process_ert_sq(ert_versal);
		process_event(ert_versal);

		if (ert_versal->bad_state)
			break;


		/* ert polling mode goes to sleep only if it doesn't have to poll
		 * submitted queue to check the completion
		 * ert interrupt mode goes to sleep if there is no cmd to be submitted
		 * OR submitted queue is full
		 */
		intr_sleep = (!ert_versal->num_rq || ert_versal->num_sq == (ert_versal->num_slots-1))
					&& !ert_versal->num_cq;
		polling_sleep = (ert_versal->polling_mode && !ert_versal->num_sq) && !ert_versal->num_cq;
		if (intr_sleep || polling_sleep)
			if (down_interruptible(&ert_versal->sem))
				ret = -ERESTARTSYS;

		process_ert_pq(ert_versal);
	}

	if (!ert_versal->bad_state)
		return ret;

	/* CU in bad state mode, abort all new commands */
	flush_submit_queue(ert_versal, &ert_versal->num_sq, KDS_ABORT, NULL);

	flush_queue(&ert_versal->cq, &ert_versal->num_cq, KDS_ABORT, NULL);
	while (!ert_versal->stop) {
		flush_queue(&ert_versal->rq, &ert_versal->num_rq, KDS_ABORT, NULL);
		process_event(ert_versal);

		if (down_interruptible(&ert_versal->sem))
			ret = -ERESTARTSYS;

		process_ert_pq(ert_versal);
	}

	return ret;
}

/**
 * xocl_ert_versal_abort() - Sent an abort event to CU thread
 * @ert_versal: Target XRT CU
 * @client: The client tries to abort commands
 *
 * This is used to ask CU thread to abort all commands from the client.
 */
int xocl_ert_versal_abort(struct xocl_ert_versal *ert_versal, void *client)
{
	int ret = 0;

	mutex_lock(&ert_versal->ev.lock);
	if (ert_versal->ev.client) {
		ret = -EAGAIN;
		goto done;
	}

	ert_versal->ev.client = client;
	ert_versal->ev.state = 0;

done:
	mutex_unlock(&ert_versal->ev.lock);
	up(&ert_versal->sem);
	return ret;
}

/**
 * xocl_ert_versal_abort() - Get done flag of abort
 * @ert_versal: Target XRT CU
 *
 * Use this to wait for abort event done
 */
int xocl_ert_versal_abort_done(struct xocl_ert_versal *ert_versal)
{
	int state = 0;

	mutex_lock(&ert_versal->ev.lock);
	if (ert_versal->ev.state) {
		ert_versal->ev.client = NULL;
		state = ert_versal->ev.state;
	}
	mutex_unlock(&ert_versal->ev.lock);

	return state;
}

void xocl_ert_versal_set_bad_state(struct xocl_ert_versal *ert_versal)
{
	ert_versal->bad_state = 1;
}

static int ert_versal_configured(struct platform_device *pdev)
{
	struct xocl_ert_versal *ert_versal = platform_get_drvdata(pdev);

	return ert_versal->config;
}
static struct xocl_ert_versal_funcs ert_versal_ops = {
	.configured = ert_versal_configured,
};

static int ert_versal_remove(struct platform_device *pdev)
{
	struct xocl_ert_versal *ert_versal;
	void *hdl;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	ert_versal = platform_get_drvdata(pdev);
	if (!ert_versal) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &ert_versal_attr_group);

	xocl_drvinst_release(ert_versal, &hdl);

	if (ert_versal->cq_base)
		iounmap(ert_versal->cq_base);

	xocl_mailbox_versal_free_intr(xdev);

	ert_versal->stop = 1;
	up(&ert_versal->sem);
	(void) kthread_stop(ert_versal->thread);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

static int ert_versal_probe(struct platform_device *pdev)
{
	struct xocl_ert_versal *ert_versal;
	struct resource *res;
	int err = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_ert_sched_privdata *priv = NULL;

	ert_versal = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_ert_versal));
	if (!ert_versal)
		return -ENOMEM;

	ert_versal->dev = &pdev->dev;
	ert_versal->pdev = pdev;
	/* Initialize pending queue and lock */
	INIT_LIST_HEAD(&ert_versal->pq);
	spin_lock_init(&ert_versal->pq_lock);
	/* Initialize run queue */
	INIT_LIST_HEAD(&ert_versal->rq);

	/* Initialize submit queue lock*/
	spin_lock_init(&ert_versal->sq_lock);

	/* Initialize completed queue */
	INIT_LIST_HEAD(&ert_versal->cq);

	mutex_init(&ert_versal->ev.lock);
	ert_versal->ev.client = NULL;

	sema_init(&ert_versal->sem, 0);
	ert_versal->thread = kthread_run(ert_versal_thread, ert_versal, "xrt_thread_versal");

	platform_set_drvdata(pdev, ert_versal);
	mutex_init(&ert_versal->lock);

	if (XOCL_GET_SUBDEV_PRIV(&pdev->dev)) {
		priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
		memcpy(&ert_versal->ert_cfg_priv, priv, sizeof(*priv));
	} else {
		xocl_err(&pdev->dev, "did not get private data");
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}

	xocl_info(&pdev->dev, "CQ IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	ert_versal->cq_range = res->end - res->start + 1;
	ert_versal->cq_base = ioremap_nocache(res->start, ert_versal->cq_range);
	if (!ert_versal->cq_base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &ert_versal_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create ert_versal sysfs attrs failed: %d", err);
	}
	ert_versal->ert.submit = ert_versal_submit;
	xocl_kds_init_ert(xdev, &ert_versal->ert);

done:
	if (err) {
		ert_versal_remove(pdev);
		return err;
	}
	return 0;
}

struct xocl_drv_private ert_versal_priv = {
	.ops = &ert_versal_ops,
	.dev = -1,
};

struct platform_device_id ert_versal_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ERT_VERSAL), (kernel_ulong_t)&ert_versal_priv },
	{ },
};

static struct platform_driver	ert_versal_driver = {
	.probe		= ert_versal_probe,
	.remove		= ert_versal_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ERT_VERSAL),
	},
	.id_table = ert_versal_id_table,
};

int __init xocl_init_ert_versal(void)
{
	return platform_driver_register(&ert_versal_driver);
}

void xocl_fini_ert_versal(void)
{
	platform_driver_unregister(&ert_versal_driver);
}
