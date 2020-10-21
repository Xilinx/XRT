/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: Chien-Wei Lan <chienwei@xilinx.com>
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
/* ERT gpio config has two channels 
 * CHANNEL 0 is control channel :
 * BIT 0: 0x0 Selects interrupts from embedded scheduler HW block
 * 	  0x1 Selects interrupts from the CU INTCs
 * BIT 2-1: TBD
 *
 * CHANNEL 1 is status channel :
 * BIT 0: check microblazer status
 */

#define GPIO_CFG_CTRL_CHANNEL	0x0
#define GPIO_CFG_STA_CHANNEL	0x8

#define SWITCH_TO_CU_INTR	0x1
#define SWITCH_TO_ERT_INTR	~SWITCH_TO_CU_INTR

#define FORCE_MB_SLEEP		0x2
#define WAKE_MB_UP		~FORCE_MB_SLEEP


#ifdef SCHED_VERBOSE
#define	ERTUSER_ERR(ert_30, fmt, arg...)	\
	xocl_err(ert_30->dev, fmt "", ##arg)
#define	ERTUSER_INFO(ert_30, fmt, arg...)	\
	xocl_info(ert_30->dev, fmt "", ##arg)	
#define	ERTUSER_DBG(ert_30, fmt, arg...)	\
	xocl_info(ert_30->dev, fmt "", ##arg)
#else
#define	ERTUSER_ERR(ert_30, fmt, arg...)	\
	xocl_err(ert_30->dev, fmt "", ##arg)
#define	ERTUSER_INFO(ert_30, fmt, arg...)	\
	xocl_info(ert_30->dev, fmt "", ##arg)	
#define	ERTUSER_DBG(ert_30, fmt, arg...)
#endif


#define sched_debug_packet(packet, size)				\
({									\
	int i;								\
	u32 *data = (u32 *)packet;					\
	for (i = 0; i < size; ++i)					    \
		DRM_INFO("packet(0x%p) execbuf[%d] = 0x%x\n", data, i, data[i]); \
})

struct ert_30_event {
	struct mutex		  lock;
	void			 *client;
	int			  state;
};

struct ert_30_command {
	struct kds_command *xcmd;
	struct list_head    list;
	uint32_t	slot_idx;
};

struct xocl_ert_30 {
	struct device		*dev;
	struct platform_device	*pdev;
	void __iomem		*cfg_gpio;
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
	struct ert_30_command	*submit_queue[ERT_MAX_SLOTS];
	spinlock_t		sq_lock;
	u32			num_sq;


	u32			stop;
	bool			bad_state;

	struct ert_30_event	ev;

	struct task_struct	*thread;

	uint32_t 		ert_dmsg;
};

static ssize_t name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	//struct xocl_ert_30 *ert_30 = platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "ert_30");
}

static DEVICE_ATTR_RO(name);

static ssize_t ert_dmsg_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_ert_30 *ert_30 = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	mutex_lock(&ert_30->lock);
	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 2) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo 0 or 1 > ert_dmsg");
		return -EINVAL;
	}

	ert_30->ert_dmsg = val;

	mutex_unlock(&ert_30->lock);
	return count;
}
static DEVICE_ATTR_WO(ert_dmsg);

static struct attribute *ert_30_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_ert_dmsg.attr,
	NULL,
};

static struct attribute_group ert_30_attr_group = {
	.attrs = ert_30_attrs,
};

static uint32_t ert_30_gpio_cfg(struct platform_device *pdev, enum ert_gpio_cfg type)
{
	struct xocl_ert_30 *ert_30 = platform_get_drvdata(pdev);
	uint32_t ret = 0, val = 0;

	val = ioread32(ert_30->cfg_gpio);

	switch (type) {
	case INTR_TO_ERT:
		val &= SWITCH_TO_ERT_INTR;
		iowrite32(val, ert_30->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		/* TODO: This could return error code -EBUSY. */
		xocl_intc_set_mode(xocl_get_xdev(pdev), ERT_INTR);
		break;
	case INTR_TO_CU:
		val |= SWITCH_TO_CU_INTR;
		iowrite32(val, ert_30->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		/* TODO: This could return error code -EBUSY. */
		xocl_intc_set_mode(xocl_get_xdev(pdev), CU_INTR);
		break;
	case MB_WAKEUP:
		val &= WAKE_MB_UP;
		iowrite32(val, ert_30->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		break;
	case MB_STATUS:
		ret = ioread32(ert_30->cfg_gpio+GPIO_CFG_STA_CHANNEL);
		break;
	default:
		break;
	}

	return ret;
}

static int ert_30_configured(struct platform_device *pdev)
{
	struct xocl_ert_30 *ert_30 = platform_get_drvdata(pdev);

	return ert_30->config;
}

static struct xocl_ert_30_funcs ert_30_ops = {
	.gpio_cfg = ert_30_gpio_cfg,
	.configured = ert_30_configured,
};


static const unsigned int no_index = -1;
static void ert_30_reset(struct xocl_ert_30 *ert_30);

static void ert_30_free_cmd(struct ert_30_command* ecmd)
{
	vfree(ecmd);
}

static struct ert_30_command* ert_30_alloc_cmd(struct kds_command *xcmd)
{
	struct ert_30_command* ecmd = vzalloc(sizeof(struct ert_30_command));

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
cmd_opcode(struct ert_30_command *ecmd)
{
	return ecmd->xcmd->opcode;
}


/* Use for flush queue */
static inline void
flush_queue(struct list_head *q, u32 *len, int status, void *client)
{
	struct kds_command *xcmd;
	struct ert_30_command *ecmd, *next;

	if (*len == 0)
		return;

	list_for_each_entry_safe(ecmd, next, q, list) {
		xcmd = ecmd->xcmd;
		if (client && client != xcmd->client)
			continue;
		xcmd->cb.notify_host(xcmd, status);
		list_del(&ecmd->list);
		xcmd->cb.free(xcmd);
		ert_30_free_cmd(ecmd);
		--(*len);
	}
}

/* Use for flush submit queue */
static void
flush_submit_queue(struct xocl_ert_30 *ert_30, u32 *len, int status, void *client)
{
	struct kds_command *xcmd;
	struct ert_30_command *ecmd;
	u32 i = 0;
	unsigned long flags;


	for ( i = 0; i < ERT_MAX_SLOTS; ++i ) {
		spin_lock_irqsave(&ert_30->sq_lock, flags);
		ecmd = ert_30->submit_queue[i];
		if (ecmd) {
			xcmd = ecmd->xcmd;
			if (client && client != xcmd->client)
				continue;
			xcmd->cb.notify_host(xcmd, status);
			xcmd->cb.free(xcmd);
			ert_30->submit_queue[i] = NULL;
			--ert_30->num_sq;
		}
		spin_unlock_irqrestore(&ert_30->sq_lock, flags);
		ert_30_free_cmd(ecmd);
	}

}
/*
 * release_slot_idx() - Release specified slot idx
 */
static void
ert_release_slot_idx(struct xocl_ert_30 *ert_30, unsigned int slot_idx)
{
	clear_bit(slot_idx, ert_30->slot_status);
}

/**
 * release_slot() - Release a slot index for a command
 *
 * Special case for control commands that execute in slot 0.  This
 * slot cannot be marked free ever.
 */
static void
ert_release_slot(struct xocl_ert_30 *ert_30, struct ert_30_command *ecmd)
{
	if (ecmd->slot_idx == no_index)
		return;

	if (cmd_opcode(ecmd) == OP_CONFIG) {
		ERTUSER_DBG(ert_30, "do nothing %s\n", __func__);
		ert_30->ctrl_busy = false;
		ert_30->config = true;	
	} else {
		ERTUSER_DBG(ert_30, "ecmd->slot_idx %d\n", ecmd->slot_idx);
		ert_release_slot_idx(ert_30, ecmd->slot_idx);
	}
	ecmd->slot_idx = no_index;
}

/**
 * process_ert_cq() - Process completed queue
 * @ert_30: Target XRT CU
 */
static inline void process_ert_cq(struct xocl_ert_30 *ert_30)
{
	struct kds_command *xcmd;
	struct ert_30_command *ecmd;
	unsigned long flags = 0;

	if (!ert_30->num_cq)
		return;

	ERTUSER_DBG(ert_30, "-> %s\n", __func__);
	spin_lock_irqsave(&ert_30->sq_lock, flags);
	/* Notify host and free command */
	ecmd = list_first_entry(&ert_30->cq, struct ert_30_command, list);
	xcmd = ecmd->xcmd;
	ERTUSER_DBG(ert_30, "%s -> ecmd %llx xcmd%p\n", __func__, (u64)ecmd, xcmd);
	ert_release_slot(ert_30, ecmd);
	list_del(&ecmd->list);
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	xcmd->cb.free(xcmd);
	--ert_30->num_cq;
	spin_unlock_irqrestore(&ert_30->sq_lock, flags);
	ert_30_free_cmd(ecmd);
	ERTUSER_DBG(ert_30, "<- %s\n", __func__);
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
ert_30_isr(int irq, void *arg)
{
	struct xocl_ert_30 *ert_30 = (struct xocl_ert_30 *)arg;
	xdev_handle_t xdev;
	struct ert_30_command *ecmd;
	unsigned long flags = 0;

	if (!ert_30)
		return IRQ_HANDLED;

	ERTUSER_DBG(ert_30, "-> xocl_user_event %d\n", irq);
	xdev = xocl_get_xdev(ert_30->pdev);

	if (irq>=ERT_MAX_SLOTS)
		return IRQ_HANDLED;

	if (!ert_30->polling_mode) {

		spin_lock_irqsave(&ert_30->sq_lock, flags);
		ecmd = ert_30->submit_queue[irq];
		if (ecmd) {
			ert_30->submit_queue[irq] = NULL;
			list_add_tail(&ecmd->list, &ert_30->cq);
			ERTUSER_DBG(ert_30, "move to cq\n");
			--ert_30->num_sq;
			++ert_30->num_cq;			
		}

		spin_unlock_irqrestore(&ert_30->sq_lock, flags);

		up(&ert_30->sem);
		/* wake up all scheduler ... currently one only */
#if 0
		if (xs->stop)
			return;

		if (xs->reset) {
			SCHED_DEBUG("scheduler is resetting after timeout\n");
			scheduler_reset(xs);
		}
#endif
	} else if (ert_30) {
		ERTUSER_DBG(ert_30, "unhandled isr irq %d", irq);
	}
	ERTUSER_DBG(ert_30, "<- xocl_user_event %d\n", irq);
	return IRQ_HANDLED;
}

/**
 * process_ert_sq() - Process submitted queue
 * @ert_30: Target XRT CU
 */
static inline void process_ert_sq(struct xocl_ert_30 *ert_30)
{
	struct ert_30_command *ecmd;
	u32 mask = 0;
	u32 slot_idx = 0, section_idx = 0;
	unsigned long flags;
	xdev_handle_t xdev = xocl_get_xdev(ert_30->pdev);

	if (!ert_30->num_sq)
		return;

	if (!ert_30->polling_mode)
		return;

	for (section_idx = 0; section_idx < 4; ++section_idx) {
		mask = xocl_intc_ert_read32(xdev, (section_idx<<2));
		if (!mask)
			continue;
		ERTUSER_DBG(ert_30, "mask 0x%x\n", mask);
		for ( slot_idx = 0; slot_idx < 32; mask>>=1, ++slot_idx ) {
			u32 cmd_idx = slot_idx+(section_idx<<5);

			if (!mask)
				break;
			if (mask & 0x1) {
				spin_lock_irqsave(&ert_30->sq_lock, flags);
				if (ert_30->submit_queue[cmd_idx]) {
					ecmd = ert_30->submit_queue[cmd_idx];

					ert_30->submit_queue[cmd_idx] = NULL;
					list_add_tail(&ecmd->list, &ert_30->cq);
					ERTUSER_DBG(ert_30, "move to cq\n");
					--ert_30->num_sq;
					++ert_30->num_cq;
				} else
					ERTUSER_DBG(ert_30, "ERR: submit queue slot is empty\n");

				spin_unlock_irqrestore(&ert_30->sq_lock, flags);
			}
		}
	}
}

/*
 * acquire_slot_idx() - First available slot index
 */
static unsigned int
ert_acquire_slot_idx(struct xocl_ert_30 *ert_30)
{
	unsigned int idx = find_first_zero_bit(ert_30->slot_status, ERT_MAX_SLOTS);

	if (idx < ert_30->num_slots) {
		set_bit(idx, ert_30->slot_status);
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
ert20_acquire_slot(struct xocl_ert_30 *ert_30, struct ert_30_command *ecmd)
{
	// slot 0 is reserved for ctrl commands
	if (cmd_opcode(ecmd) == OP_CONFIG) {
		set_bit(0, ert_30->slot_status);

		if (ert_30->ctrl_busy) {
			ERTUSER_ERR(ert_30, "ctrl slot is busy\n");
			return -1;
		}
		ert_30->ctrl_busy = true;
		return (ecmd->slot_idx = 0);
	}

	return (ecmd->slot_idx = ert_acquire_slot_idx(ert_30));
}


static int ert_cfg_cmd(struct xocl_ert_30 *ert_30, struct ert_30_command *ecmd)
{
	xdev_handle_t xdev = xocl_get_xdev(ert_30->pdev);
	uint32_t *cdma = xocl_rom_cdma_addr(xdev);
	unsigned int dsa = ert_30->ert_cfg_priv.dsa;
	unsigned int major = ert_30->ert_cfg_priv.major;
	struct ert_configure_cmd *cfg = (struct ert_configure_cmd *)ecmd->xcmd->execbuf;
	bool ert = (XOCL_DSA_IS_VERSAL(xdev) || XOCL_DSA_IS_MPSOC(xdev)) ? 1 :
	    xocl_mb_sched_on(xdev);
	bool ert_full = (ert && cfg->ert && !cfg->dataflow);
	bool ert_poll = (ert && cfg->ert && cfg->dataflow);
	unsigned int ert_num_slots = 0;

	if (cmd_opcode(ecmd) != OP_CONFIG)
		return -EINVAL;

	if (major > 3) {
		DRM_INFO("Unknown ERT major version, fallback to KDS mode\n");
		ert_full = 0;
		ert_poll = 0;
	}

	ERTUSER_DBG(ert_30, "ert per feature rom = %d", ert);
	ERTUSER_DBG(ert_30, "dsa52 = %d", dsa);

	if (XOCL_DSA_IS_VERSAL(xdev) || XOCL_DSA_IS_MPSOC(xdev)) {
		ERTUSER_INFO(ert_30, "MPSoC polling mode %d", cfg->polling);

		// For MPSoC device, we will use ert_full if we are
		// configured as ert mode even dataflow is configured.
		// And we do not support ert_poll.
		ert_full = cfg->ert;
		ert_poll = false;
	}

	// Mark command as control command to force slot 0 execution
	//cfg->type = ERT_CTRL;

	ERTUSER_DBG(ert_30, "configuring scheduler cq_size(%lld)\n", ert_30->cq_range);
	if (ert_30->cq_range == 0 || cfg->slot_size == 0) {
		ERTUSER_ERR(ert_30, "should not have zeroed value of cq_size=%lld, slot_size=%d",
		    ert_30->cq_range, cfg->slot_size);
		return -EINVAL;
	}

	ert_num_slots = ert_30->cq_range / cfg->slot_size;

	if (ert_poll)
		// Adjust slot size for ert poll mode
		cfg->slot_size = ert_30->cq_range / MAX_CUS;

	if (ert_full && cfg->cu_dma && ert_num_slots > 32) {
		// Max slot size is 32 because of cudma bug
		ERTUSER_INFO(ert_30, "Limitting CQ size to 32 due to ERT CUDMA bug\n");
		ert_num_slots = 32;
		cfg->slot_size = ert_30->cq_range / ert_num_slots;
	}

	if (ert_poll) {
		ERTUSER_INFO(ert_30, "configuring dataflow mode with ert polling\n");
		cfg->slot_size = ert_30->cq_range / MAX_CUS;
		cfg->cu_isr = 0;
		cfg->cu_dma = 0;
		ert_30->polling_mode = cfg->polling;
		ert_30->num_slots = ert_30->cq_range / cfg->slot_size;
	} else if (ert_full) {
		ERTUSER_INFO(ert_30, "configuring embedded scheduler mode\n");
		ert_30->cq_intr = cfg->cq_int;
		ert_30->polling_mode = cfg->polling;
		ert_30->num_slots = ert_30->cq_range / cfg->slot_size;
		cfg->dsa52 = dsa;
		cfg->cdma = cdma ? 1 : 0;
	}

	if (XDEV(xdev)->priv.flags & XOCL_DSAFLAG_CUDMA_OFF)
		cfg->cu_dma = 0;

	cfg->dmsg = ert_30->ert_dmsg;

	// The KDS side of of the scheduler is now configured.  If ERT is
	// enabled, then the configure command will be started asynchronously
	// on ERT.  The shceduler is not marked configured until ERT has
	// completed (exec_finish_cmd); this prevents other processes from
	// submitting commands to same xclbin.  However we must also stop
	// other processes from submitting configure command on this same
	// xclbin while ERT asynchronous configure is running.
	//exec->configure_active = true;

	ERTUSER_INFO(ert_30, "scheduler config ert(%d), dataflow(%d), slots(%d), cudma(%d), cuisr(%d)\n"
		 , ert_poll | ert_full
		 , cfg->dataflow
		 , ert_30->num_slots
		 , cfg->cu_dma ? 1 : 0
		 , cfg->cu_isr ? 1 : 0);

	// TODO: reset all queues
	ert_30_reset(ert_30);

	return 0;
}
/**
 * process_ert_rq() - Process run queue
 * @ert_30: Target XRT CU
 *
 * Return: return 0 if run queue is empty or no credit
 *	   Otherwise, return 1
 */
static inline int process_ert_rq(struct xocl_ert_30 *ert_30)
{
	struct ert_30_command *ecmd, *next;
	u32 slot_addr = 0, i;
	struct ert_packet *epkt = NULL;
	xdev_handle_t xdev = xocl_get_xdev(ert_30->pdev);
	unsigned long flags;

	if (!ert_30->num_rq)
		return 0;

	list_for_each_entry_safe(ecmd, next, &ert_30->rq, list) {

		if (cmd_opcode(ecmd) == OP_CONFIG) {
			if (ert_cfg_cmd(ert_30, ecmd)) {
				struct kds_command *xcmd;

				ERTUSER_ERR(ert_30, "%s config cmd error\n", __func__);
				list_del(&ecmd->list);
				xcmd = ecmd->xcmd;
				xcmd->cb.notify_host(xcmd, KDS_ABORT);
				xcmd->cb.free(xcmd);
				ert_30_free_cmd(ecmd);
				--ert_30->num_rq;
				continue;
			}
		}

		if (ert20_acquire_slot(ert_30, ecmd) == no_index) {
			ERTUSER_DBG(ert_30, "%s not slot available\n", __func__);
			return 0;
		}
		epkt = (struct ert_packet *)ecmd->xcmd->execbuf;
		ERTUSER_DBG(ert_30, "%s op_code %d ecmd->slot_idx %d\n", __func__, cmd_opcode(ecmd), ecmd->slot_idx);

		//sched_debug_packet(epkt, epkt->count+sizeof(epkt->header)/sizeof(u32));

		if (cmd_opcode(ecmd) == OP_CONFIG && !ert_30->polling_mode) {
			for (i = 0; i < ert_30->num_slots; i++) {
				xocl_intc_ert_request(xdev, i, ert_30_isr, ert_30);
				xocl_intc_ert_config(xdev, i, true);
			}

		}
		slot_addr = ecmd->slot_idx * (ert_30->cq_range/ert_30->num_slots);

		ERTUSER_DBG(ert_30, "%s slot_addr %x\n", __func__, slot_addr);
		if (cmd_opcode(ecmd) == OP_CONFIG) {
			xocl_memcpy_toio(ert_30->cq_base + slot_addr + 4,
				  ecmd->xcmd->execbuf+1, epkt->count*sizeof(u32));
		} else {
			// write kds selected cu_idx in first cumask (first word after header)
			iowrite32(ecmd->xcmd->cu_idx, ert_30->cq_base + slot_addr + 4);

			// write remaining packet (past header and cuidx)
			xocl_memcpy_toio(ert_30->cq_base + slot_addr + 8,
					 ecmd->xcmd->execbuf+2, (epkt->count-1)*sizeof(u32));
		}

		iowrite32(epkt->header, ert_30->cq_base + slot_addr);

		if (ert_30->cq_intr) {
			u32 mask_idx = mask_idx32(ecmd->slot_idx);
			u32 cq_int_addr = (mask_idx << 2);
			u32 mask = 1 << idx_in_mask32(ecmd->slot_idx, mask_idx);

			ERTUSER_DBG(ert_30, "++ mb_submit writes slot mask 0x%x to CQ_INT register at addr 0x%x\n",
					mask, cq_int_addr);
			xocl_intc_ert_write32(xdev, mask, cq_int_addr);
		}
		spin_lock_irqsave(&ert_30->sq_lock, flags);
		ert_30->submit_queue[ecmd->slot_idx] = ecmd;
		list_del(&ecmd->list);
		--ert_30->num_rq;
		++ert_30->num_sq;
		spin_unlock_irqrestore(&ert_30->sq_lock, flags);
	}

	return 1;
}

/**
 * process_ert_rq() - Process pending queue
 * @ert_30: Target XRT CU
 *
 * Move all of the pending queue commands to the tail of run queue
 * and re-initialized pending queue
 */
static inline void process_ert_pq(struct xocl_ert_30 *ert_30)
{
	unsigned long flags;

	/* Get pending queue command number without lock.
	 * The idea is to reduce the possibility of conflict on lock.
	 * Need to check pending command number again after lock.
	 */
	if (!ert_30->num_pq)
		return;
	spin_lock_irqsave(&ert_30->pq_lock, flags);
	if (ert_30->num_pq) {
		list_splice_tail_init(&ert_30->pq, &ert_30->rq);
		ert_30->num_rq += ert_30->num_pq;
		ert_30->num_pq = 0;
	}
	spin_unlock_irqrestore(&ert_30->pq_lock, flags);
}

/**
 * process_event() - Process event
 * @ert_30: Target XRT CU
 *
 * This is used to process low frequency events.
 * For example, client abort event would happen when closing client.
 * Before the client close, make sure all of the client commands have
 * been handle properly.
 */
static inline void process_event(struct xocl_ert_30 *ert_30)
{
	void *client = NULL;

	mutex_lock(&ert_30->ev.lock);
	if (!ert_30->ev.client)
		goto done;

	client = ert_30->ev.client;

	flush_queue(&ert_30->rq, &ert_30->num_rq, KDS_ABORT, client);

	/* Let's check submitted commands one more time */
	process_ert_sq(ert_30);
	if (ert_30->num_sq) {
		flush_submit_queue(ert_30, &ert_30->num_sq, KDS_ABORT, client);
		ert_30->ev.state = ERT_STATE_BAD;
	}

	while (ert_30->num_cq)
		process_ert_cq(ert_30);

	/* Maybe pending queue has commands of this client */
	process_ert_pq(ert_30);
	flush_queue(&ert_30->rq, &ert_30->num_rq, KDS_ABORT, client);

	if (!ert_30->ev.state)
		ert_30->ev.state = ERT_STATE_GOOD;
done:
	mutex_unlock(&ert_30->ev.lock);
}


static void ert_30_reset(struct xocl_ert_30 *ert_30)
{
	process_event(ert_30);
	bitmap_zero(ert_30->slot_status, ERT_MAX_SLOTS);
}

static void ert_30_submit(struct kds_ert *ert, struct kds_command *xcmd)
{
	unsigned long flags;
	bool first_command = false;
	struct xocl_ert_30 *ert_30 = container_of(ert, struct xocl_ert_30, ert);
	struct ert_30_command *ecmd = ert_30_alloc_cmd(xcmd);

	if (!ecmd)
		return;

	ERTUSER_DBG(ert_30, "->%s ecmd %llx\n", __func__, (u64)ecmd);
	spin_lock_irqsave(&ert_30->pq_lock, flags);
	list_add_tail(&ecmd->list, &ert_30->pq);
	++ert_30->num_pq;
	first_command = (ert_30->num_pq == 1);
	spin_unlock_irqrestore(&ert_30->pq_lock, flags);
	/* Add command to pending queue
	 * wakeup service thread if it is the first command
	 */
	if (first_command)
		up(&ert_30->sem);

	ERTUSER_DBG(ert_30, "<-%s\n", __func__);
	return;
}

int ert_30_thread(void *data)
{
	struct xocl_ert_30 *ert_30 = (struct xocl_ert_30 *)data;
	int ret = 0;
	bool polling_sleep = false, intr_sleep = false;

	while (!ert_30->stop) {
		/* Make sure to submit as many commands as possible.
		 * This is why we call continue here. This is important to make
		 * CU busy, especially CU has hardware queue.
		 */
		if (process_ert_rq(ert_30))
			continue;
		/* process completed queue before submitted queue, for
		 * two reasons:
		 * - The last submitted command may be still running
		 * - while handling completed queue, running command might done
		 * - process_ert_sq will check CU status, which is thru slow bus
		 */
		process_ert_cq(ert_30);
		process_ert_sq(ert_30);
		process_event(ert_30);

		if (ert_30->bad_state)
			break;

		/* ert polling mode goes to sleep only if it doesn't have to poll
		 * submitted queue to check the completion
		 * ert interrupt mode goes to sleep if there is no cmd to be submitted
		 * OR submitted queue is full
		 */
		intr_sleep = (!ert_30->num_rq || ert_30->num_sq == (ert_30->num_slots-1))
					&& !ert_30->num_cq;
		polling_sleep = (ert_30->polling_mode && !ert_30->num_sq) && !ert_30->num_cq;
		if (intr_sleep || polling_sleep)
			if (down_interruptible(&ert_30->sem))
				ret = -ERESTARTSYS;

		process_ert_pq(ert_30);
	}

	if (!ert_30->bad_state)
		return ret;

	/* CU in bad state mode, abort all new commands */
	flush_submit_queue(ert_30, &ert_30->num_sq, KDS_ABORT, NULL);

	flush_queue(&ert_30->cq, &ert_30->num_cq, KDS_ABORT, NULL);
	while (!ert_30->stop) {
		flush_queue(&ert_30->rq, &ert_30->num_rq, KDS_ABORT, NULL);
		process_event(ert_30);

		if (down_interruptible(&ert_30->sem))
			ret = -ERESTARTSYS;

		process_ert_pq(ert_30);
	}

	return ret;
}

/**
 * xocl_ert_30_abort() - Sent an abort event to CU thread
 * @ert_30: Target XRT CU
 * @client: The client tries to abort commands
 *
 * This is used to ask CU thread to abort all commands from the client.
 */
int xocl_ert_30_abort(struct xocl_ert_30 *ert_30, void *client)
{
	int ret = 0;

	mutex_lock(&ert_30->ev.lock);
	if (ert_30->ev.client) {
		ret = -EAGAIN;
		goto done;
	}

	ert_30->ev.client = client;
	ert_30->ev.state = 0;

done:
	mutex_unlock(&ert_30->ev.lock);
	up(&ert_30->sem);
	return ret;
}

/**
 * xocl_ert_30_abort() - Get done flag of abort
 * @ert_30: Target XRT CU
 *
 * Use this to wait for abort event done
 */
int xocl_ert_30_abort_done(struct xocl_ert_30 *ert_30)
{
	int state = 0;

	mutex_lock(&ert_30->ev.lock);
	if (ert_30->ev.state) {
		ert_30->ev.client = NULL;
		state = ert_30->ev.state;
	}
	mutex_unlock(&ert_30->ev.lock);

	return state;
}

void xocl_ert_30_set_bad_state(struct xocl_ert_30 *ert_30)
{
	ert_30->bad_state = 1;
}

static int ert_30_remove(struct platform_device *pdev)
{
	struct xocl_ert_30 *ert_30;
	void *hdl;
	u32 i = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	ert_30 = platform_get_drvdata(pdev);
	if (!ert_30) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &ert_30_attr_group);

	xocl_drvinst_release(ert_30, &hdl);

	if (ert_30->cfg_gpio)
		iounmap(ert_30->cfg_gpio);

	if (ert_30->cq_base)
		iounmap(ert_30->cq_base);

	for (i = 0; i < ert_30->num_slots; i++) {
		xocl_intc_ert_config(xdev, i, false);
		xocl_intc_ert_request(xdev, i, NULL, NULL);
	}

	ert_30->stop = 1;
	up(&ert_30->sem);
	(void) kthread_stop(ert_30->thread);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

static int ert_30_probe(struct platform_device *pdev)
{
	struct xocl_ert_30 *ert_30;
	struct resource *res;
	int err = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_ert_sched_privdata *priv = NULL;

	ert_30 = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_ert_30));
	if (!ert_30)
		return -ENOMEM;

	ert_30->dev = &pdev->dev;
	ert_30->pdev = pdev;
	/* Initialize pending queue and lock */
	INIT_LIST_HEAD(&ert_30->pq);
	spin_lock_init(&ert_30->pq_lock);
	/* Initialize run queue */
	INIT_LIST_HEAD(&ert_30->rq);

	/* Initialize submit queue lock*/
	spin_lock_init(&ert_30->sq_lock);

	/* Initialize completed queue */
	INIT_LIST_HEAD(&ert_30->cq);

	mutex_init(&ert_30->ev.lock);
	ert_30->ev.client = NULL;

	sema_init(&ert_30->sem, 0);
	ert_30->thread = kthread_run(ert_30_thread, ert_30, "xrt_thread");

	platform_set_drvdata(pdev, ert_30);
	mutex_init(&ert_30->lock);

	if (XOCL_GET_SUBDEV_PRIV(&pdev->dev)) {
		priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
		memcpy(&ert_30->ert_cfg_priv, priv, sizeof(*priv));
	} else {
		xocl_err(&pdev->dev, "did not get private data");
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "did not get memory");
		err = -ENOMEM;
		goto done;
	}

	xocl_info(&pdev->dev, "CFG GPIO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	ert_30->cfg_gpio = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!ert_30->cfg_gpio) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		err = -ENOMEM;
		goto done;
	}

	xocl_info(&pdev->dev, "CQ IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	ert_30->cq_range = res->end - res->start + 1;
	ert_30->cq_base = ioremap_nocache(res->start, ert_30->cq_range);
	if (!ert_30->cq_base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &ert_30_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create ert_30 sysfs attrs failed: %d", err);
	}
	ert_30->ert.submit = ert_30_submit;
	xocl_kds_init_ert(xdev, &ert_30->ert);

done:
	if (err) {
		ert_30_remove(pdev);
		return err;
	}
	return 0;
}

struct xocl_drv_private ert_30_priv = {
	.ops = &ert_30_ops,
	.dev = -1,
};

struct platform_device_id ert_30_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ERT_30), (kernel_ulong_t)&ert_30_priv },
	{ },
};

static struct platform_driver	ert_30_driver = {
	.probe		= ert_30_probe,
	.remove		= ert_30_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ERT_30),
	},
	.id_table = ert_30_id_table,
};

int __init xocl_init_ert_30(void)
{
	return platform_driver_register(&ert_30_driver);
}

void xocl_fini_ert_30(void)
{
	platform_driver_unregister(&ert_30_driver);
}
