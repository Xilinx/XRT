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
#include "kds_client.h"

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

#define WAKE_MB_UP		0x2
#define CLEAR_MB_WAKEUP		~WAKE_MB_UP

/* XRT ERT timer macros */
/* A low frequence timer for ERT to check if command timeout */
#define ERT_TICKS_PER_SEC	2
#define ERT_TIMER		(HZ / ERT_TICKS_PER_SEC) /* in jiffies */
#define ERT_EXEC_DEFAULT_TTL	(5UL * ERT_TICKS_PER_SEC)

#ifdef SCHED_VERBOSE
#define	ERTUSER_ERR(ert_user, fmt, arg...)	\
	xocl_err(ert_user->dev, fmt "", ##arg)
#define	ERTUSER_INFO(ert_user, fmt, arg...)	\
	xocl_info(ert_user->dev, fmt "", ##arg)	
#define	ERTUSER_DBG(ert_user, fmt, arg...)	\
	xocl_info(ert_user->dev, fmt "", ##arg)
#else
#define	ERTUSER_ERR(ert_user, fmt, arg...)	\
	xocl_err(ert_user->dev, fmt "", ##arg)
#define	ERTUSER_INFO(ert_user, fmt, arg...)	\
	xocl_info(ert_user->dev, fmt "", ##arg)	
#define	ERTUSER_DBG(ert_user, fmt, arg...)
#endif


#define sched_debug_packet(packet, size)				\
({									\
	int i;								\
	u32 *data = (u32 *)packet;					\
	for (i = 0; i < size; ++i)					    \
		DRM_INFO("packet(0x%p) execbuf[%d] = 0x%x\n", data, i, data[i]); \
})

extern int kds_echo;

struct ert_user_event {
	struct mutex		  lock;
	void			 *client;
	int			  state;
};

struct ert_user_command {
	struct kds_command *xcmd;
	struct list_head    list;
	uint32_t	slot_idx;
	bool		completed;
	uint32_t	status;
};

struct xocl_ert_user {
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

	struct semaphore	sem;
	/* submitted queue */
	struct list_head	sq;
	struct ert_user_command	*submit_queue[ERT_MAX_SLOTS];
	u32			num_sq;

	struct list_head	cq;
	u32			num_cq;
	u32			stop;
	bool			bad_state;

	struct mutex		ev_lock;
	struct list_head	events;

	struct timer_list	timer;
	atomic_t		tick;

	struct task_struct	*thread;

	uint32_t 		ert_dmsg;
	uint32_t		echo;
	uint32_t		intr;
};

static ssize_t name_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	//struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));
	return sprintf(buf, "ert_user");
}

static DEVICE_ATTR_RO(name);

static ssize_t snap_shot_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "pending:%d, running:%d, submit:%d complete:%d\n", ert_user->num_pq, ert_user->num_rq, ert_user->num_sq
		,ert_user->num_cq);
}

static DEVICE_ATTR_RO(snap_shot);

static ssize_t ert_dmsg_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	mutex_lock(&ert_user->lock);
	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 2) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo 0 or 1 > ert_dmsg");
		return -EINVAL;
	}

	ert_user->ert_dmsg = val;

	mutex_unlock(&ert_user->lock);
	return count;
}
static DEVICE_ATTR_WO(ert_dmsg);

static ssize_t ert_echo_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	mutex_lock(&ert_user->lock);
	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 2) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo 0 or 1 > ert_echo");
		return -EINVAL;
	}

	ert_user->echo = val;

	mutex_unlock(&ert_user->lock);
	return count;
}
static DEVICE_ATTR_WO(ert_echo);

static ssize_t ert_intr_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	mutex_lock(&ert_user->lock);
	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 2) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo 0 or 1 > ert_intr");
		return -EINVAL;
	}

	ert_user->intr = val;

	mutex_unlock(&ert_user->lock);
	return count;
}
static DEVICE_ATTR_WO(ert_intr);

static struct attribute *ert_user_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_ert_dmsg.attr,
	&dev_attr_snap_shot.attr,
	&dev_attr_ert_echo.attr,
	&dev_attr_ert_intr.attr,
	NULL,
};

static struct attribute_group ert_user_attr_group = {
	.attrs = ert_user_attrs,
};

static uint32_t ert_user_gpio_cfg(struct platform_device *pdev, enum ert_gpio_cfg type)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(ert_user->pdev);
	uint32_t ret = 0, val = 0;
	int i;

	if (!ert_user->cfg_gpio) {
		ERTUSER_ERR(ert_user, "%s ERT config gpio not found\n", __func__);
		return 0;
	}

	val = ioread32(ert_user->cfg_gpio);

	switch (type) {
	case INTR_TO_ERT:
		val &= SWITCH_TO_ERT_INTR;
		iowrite32(val, ert_user->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		for (i = 0; i < ert_user->num_slots; i++)
			xocl_intc_ert_config(xdev, i, false);
		/* TODO: This could return error code -EBUSY. */
		xocl_intc_set_mode(xocl_get_xdev(pdev), ERT_INTR);
		break;
	case INTR_TO_CU:
		val |= SWITCH_TO_CU_INTR;
		iowrite32(val, ert_user->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		for (i = 0; i < ert_user->num_slots; i++)
			xocl_intc_ert_config(xdev, i, false);
		/* TODO: This could return error code -EBUSY. */
		xocl_intc_set_mode(xocl_get_xdev(pdev), CU_INTR);
		break;
	case MB_WAKEUP:
		val |= WAKE_MB_UP;
		iowrite32(val, ert_user->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		break;
	case MB_SLEEP:
		val &= CLEAR_MB_WAKEUP;
		iowrite32(val, ert_user->cfg_gpio+GPIO_CFG_CTRL_CHANNEL);
		/* TODO: submit an EXIT command to ERT thread */
		iowrite32(ERT_EXIT_CMD, ert_user->cq_base);
		ret = ioread32(ert_user->cfg_gpio+GPIO_CFG_STA_CHANNEL);
		while (!ret)
			ret = ioread32(ert_user->cfg_gpio+GPIO_CFG_STA_CHANNEL);
		break;
	case MB_STATUS:
		ret = ioread32(ert_user->cfg_gpio+GPIO_CFG_STA_CHANNEL);
		break;
	default:
		break;
	}

	return ret;
}

static int ert_user_configured(struct platform_device *pdev)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(pdev);

	return ert_user->config;
}

static struct xocl_ert_user_funcs ert_user_ops = {
	.gpio_cfg = ert_user_gpio_cfg,
	.configured = ert_user_configured,
};


static const unsigned int no_index = -1;
static void ert_user_reset(struct xocl_ert_user *ert_user);

static void ert_user_free_cmd(struct ert_user_command* ecmd)
{
	kfree(ecmd);
}

static struct ert_user_command* ert_user_alloc_cmd(struct kds_command *xcmd)
{
	struct ert_user_command* ecmd = kzalloc(sizeof(struct ert_user_command), GFP_KERNEL);

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
cmd_opcode(struct ert_user_command *ecmd)
{
	return ecmd->xcmd->opcode;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static void ert_timer(unsigned long data)
{
	struct xocl_ert_user *ert_user = (struct xocl_ert_user *)data;
#else
static void ert_timer(struct timer_list *t)
{
	struct xocl_ert_user *ert_user = from_timer(ert_user, t, timer);
#endif

	atomic_inc(&ert_user->tick);

	mod_timer(&ert_user->timer, jiffies + ERT_TIMER);
}

static inline struct kds_client *
first_event_client_or_null(struct xocl_ert_user *ert_user)
{
	struct kds_client *curr = NULL;

	if (list_empty(&ert_user->events))
		return NULL;

	mutex_lock(&ert_user->ev_lock);
	if (list_empty(&ert_user->events))
		goto done;

	curr = list_first_entry(&ert_user->events, struct kds_client, ev_entry);

done:
	mutex_unlock(&ert_user->ev_lock);
	return curr;
}

/*
 * release_slot_idx() - Release specified slot idx
 */
static void
ert_release_slot_idx(struct xocl_ert_user *ert_user, unsigned int slot_idx)
{
	clear_bit(slot_idx, ert_user->slot_status);
}

/**
 * release_slot() - Release a slot index for a command
 *
 * Special case for control commands that execute in slot 0.  This
 * slot cannot be marked free ever.
 */
static void
ert_release_slot(struct xocl_ert_user *ert_user, struct ert_user_command *ecmd)
{
	if (ecmd->slot_idx == no_index)
		return;

	if (cmd_opcode(ecmd) == OP_CONFIG || cmd_opcode(ecmd) == OP_CONFIG_SK) {
		ERTUSER_DBG(ert_user, "do nothing %s\n", __func__);
		ert_user->ctrl_busy = false;
		ert_user->config = true;	
	} else {
		ERTUSER_DBG(ert_user, "ecmd->slot_idx %d\n", ecmd->slot_idx);
		ert_release_slot_idx(ert_user, ecmd->slot_idx);
	}
	ecmd->slot_idx = no_index;
}

/**
 * process_ert_cq() - Process cmd witch is completed
 * @ert_user: Target XRT CU
 */
static inline void process_ert_cq(struct xocl_ert_user *ert_user)
{
	struct kds_command *xcmd;
	struct ert_user_command *ecmd;

	if (!ert_user->num_cq)
		return;

	ERTUSER_DBG(ert_user, "-> %s\n", __func__);

	while (ert_user->num_cq) {
		ecmd = list_first_entry(&ert_user->cq, struct ert_user_command, list);
		list_del(&ecmd->list);
		xcmd = ecmd->xcmd;
		ert_release_slot(ert_user, ecmd);
		xcmd->cb.notify_host(xcmd, ecmd->status);
		xcmd->cb.free(xcmd);
		ert_user_free_cmd(ecmd);
		--ert_user->num_cq;
	}

	ERTUSER_DBG(ert_user, "<- %s\n", __func__);
}


/**
 * process_ert_sq() - Process cmd witch is submitted
 * @ert_user: Target XRT ERT
 */
static inline void process_ert_sq(struct xocl_ert_user *ert_user)
{
	struct kds_command *xcmd;
	struct ert_user_command *ecmd, *next;
	struct kds_client *ev_client = NULL;
	unsigned int tick;

	if (!ert_user->num_sq)
		return;

	ev_client = first_event_client_or_null(ert_user);

	list_for_each_entry_safe(ecmd, next, &ert_user->sq, list) {
		xcmd = ecmd->xcmd;
		if (ecmd->completed) {
			ecmd->status = KDS_COMPLETED;
		} else if (unlikely(ev_client)) {
			/* Client event happens rarely */
			if (xcmd->client != ev_client)
				continue;

			tick = atomic_read(&ert_user->tick);
			/* Record command tick to start timeout counting */
			if (!xcmd->tick) {
				xcmd->tick = tick;
				continue;
			}

			/* If xcmd haven't timeout */
			if (tick - xcmd->tick < ERT_EXEC_DEFAULT_TTL)
				continue;

			ecmd->status = KDS_TIMEOUT;
			/* Mark ERT as bad state */
			ert_user->bad_state = true;
		} else
			continue;

		ERTUSER_DBG(ert_user, "%s -> ecmd %llx xcmd%p\n", __func__, (u64)ecmd, xcmd);
		list_move_tail(&ecmd->list, &ert_user->cq);
		--ert_user->num_sq;
		++ert_user->num_cq;
		ert_user->submit_queue[ecmd->slot_idx] = NULL;

	}
	ERTUSER_DBG(ert_user, "<- %s\n", __func__);
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
ert_user_isr(int irq, void *arg)
{
	struct xocl_ert_user *ert_user = (struct xocl_ert_user *)arg;
	xdev_handle_t xdev;
	struct ert_user_command *ecmd;

	BUG_ON(!ert_user);

	ERTUSER_DBG(ert_user, "-> xocl_user_event %d\n", irq);
	xdev = xocl_get_xdev(ert_user->pdev);

	BUG_ON(irq>=ERT_MAX_SLOTS);

	if (!ert_user->polling_mode) {

		ecmd = ert_user->submit_queue[irq];
		if (ecmd)
			ecmd->completed = true;

		up(&ert_user->sem);
		/* wake up all scheduler ... currently one only */
#if 0
		if (xs->stop)
			return;

		if (xs->reset) {
			SCHED_DEBUG("scheduler is resetting after timeout\n");
			scheduler_reset(xs);
		}
#endif
	} else {
		ERTUSER_DBG(ert_user, "unhandled isr irq %d", irq);
		return IRQ_NONE;
	}
	ERTUSER_DBG(ert_user, "<- xocl_user_event %d\n", irq);
	return IRQ_HANDLED;
}

/**
 * process_ert_sq_polling() - Process submitted queue
 * @ert_user: Target XRT ERT
 */
static inline void process_ert_sq_polling(struct xocl_ert_user *ert_user)
{
	struct kds_command *xcmd;
	struct ert_user_command *ecmd;
	u32 mask = 0;
	u32 slot_idx = 0, section_idx = 0;
	struct kds_client *ev_client = NULL;
	xdev_handle_t xdev = xocl_get_xdev(ert_user->pdev);
	unsigned int tick;

	if (!ert_user->num_sq)
		return;

	if (!ert_user->polling_mode)
		return;

	for (section_idx = 0; section_idx < 4; ++section_idx) {
		mask = xocl_intc_ert_read32(xdev, (section_idx<<2));
		if (!mask)
			continue;
		ERTUSER_DBG(ert_user, "mask 0x%x\n", mask);
		for ( slot_idx = 0; slot_idx < 32; mask>>=1, ++slot_idx ) {
			u32 cmd_idx = slot_idx+(section_idx<<5);

			if (!mask)
				break;
			if (mask & 0x1) {
				ecmd = ert_user->submit_queue[cmd_idx];
				if (ecmd) {
					xcmd = ecmd->xcmd;
					ERTUSER_DBG(ert_user, "%s -> ecmd %llx xcmd%p\n", __func__, (u64)ecmd, xcmd);
					list_move_tail(&ecmd->list, &ert_user->cq);
					--ert_user->num_sq;
					++ert_user->num_cq;
					ert_user->submit_queue[cmd_idx] = NULL;
				} else
					ERTUSER_DBG(ert_user, "ERR: submit queue slot is empty\n");
			}
		}
	}

	ev_client = first_event_client_or_null(ert_user);
	if (likely(!ev_client))
		return;

	for (slot_idx = 0; slot_idx < ert_user->num_slots; ++slot_idx) {
		ecmd = ert_user->submit_queue[slot_idx];
		if (!ecmd)
			continue;
		xcmd = ecmd->xcmd;

		/* Client event happens rarely */
		if (xcmd->client != ev_client)
			continue;

		tick = atomic_read(&ert_user->tick);
		/* Record CU tick to start timeout counting */
		if (!xcmd->tick) {
			xcmd->tick = tick;
			continue;
		}

		/* If xcmd haven't timeout */
		if (tick - xcmd->tick < ERT_EXEC_DEFAULT_TTL)
			continue;

		ecmd->status = KDS_TIMEOUT;
		/* Mark this CU as bad state */
		ert_user->bad_state = true;

		ERTUSER_DBG(ert_user, "%s -> ecmd %llx xcmd%p\n", __func__, (u64)ecmd, xcmd);
		list_move_tail(&ecmd->list, &ert_user->cq);
		--ert_user->num_sq;
		++ert_user->num_cq;
		ert_user->submit_queue[slot_idx] = NULL;
	}

}

/*
 * acquire_slot_idx() - First available slot index
 */
static unsigned int
ert_acquire_slot_idx(struct xocl_ert_user *ert_user)
{
	unsigned int idx = find_first_zero_bit(ert_user->slot_status, ERT_MAX_SLOTS);

	if (idx < ert_user->num_slots) {
		set_bit(idx, ert_user->slot_status);
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
ert_acquire_slot(struct xocl_ert_user *ert_user, struct ert_user_command *ecmd)
{
	// slot 0 is reserved for ctrl commands
	if (cmd_opcode(ecmd) == OP_CONFIG || cmd_opcode(ecmd) == OP_CONFIG_SK) {
		set_bit(0, ert_user->slot_status);

		if (ert_user->ctrl_busy) {
			ERTUSER_ERR(ert_user, "ctrl slot is busy\n");
			return -1;
		}
		ert_user->ctrl_busy = true;
		return (ecmd->slot_idx = 0);
	}

	return (ecmd->slot_idx = ert_acquire_slot_idx(ert_user));
}


static int ert_cfg_cmd(struct xocl_ert_user *ert_user, struct ert_user_command *ecmd)
{
	xdev_handle_t xdev = xocl_get_xdev(ert_user->pdev);
	uint32_t *cdma = xocl_rom_cdma_addr(xdev);
	unsigned int dsa = ert_user->ert_cfg_priv.dsa;
	unsigned int major = ert_user->ert_cfg_priv.major;
	struct ert_configure_cmd *cfg = (struct ert_configure_cmd *)ecmd->xcmd->execbuf;
	bool ert = (XOCL_DSA_IS_VERSAL(xdev) || XOCL_DSA_IS_MPSOC(xdev)) ? 1 :
	    xocl_mb_sched_on(xdev);
	bool ert_full = (ert && !cfg->dataflow);
	bool ert_poll = (ert && cfg->dataflow);
	unsigned int ert_num_slots = 0;

	if (cmd_opcode(ecmd) != OP_CONFIG)
		return -EINVAL;

	if (major > 3) {
		DRM_INFO("Unknown ERT major version, fallback to KDS mode\n");
		ert_full = 0;
		ert_poll = 0;
	}

	ERTUSER_DBG(ert_user, "ert per feature rom = %d", ert);
	ERTUSER_DBG(ert_user, "dsa52 = %d", dsa);

	if (XOCL_DSA_IS_VERSAL(xdev) || XOCL_DSA_IS_MPSOC(xdev)) {
		ERTUSER_INFO(ert_user, "MPSoC polling mode %d", cfg->polling);

		// For MPSoC device, we will use ert_full if we are
		// configured as ert mode even dataflow is configured.
		// And we do not support ert_poll.
		ert_full = cfg->ert;
		ert_poll = false;
	}

	// Mark command as control command to force slot 0 execution
	//cfg->type = ERT_CTRL;

	ERTUSER_DBG(ert_user, "configuring scheduler cq_size(%lld)\n", ert_user->cq_range);
	if (ert_user->cq_range == 0 || cfg->slot_size == 0) {
		ERTUSER_ERR(ert_user, "should not have zeroed value of cq_size=%lld, slot_size=%d",
		    ert_user->cq_range, cfg->slot_size);
		return -EINVAL;
	}

	ert_num_slots = ert_user->cq_range / cfg->slot_size;

	if (ert_poll)
		// Adjust slot size for ert poll mode
		cfg->slot_size = ert_user->cq_range / MAX_CUS;

	if (ert_full && cfg->cu_dma && ert_num_slots > 32) {
		// Max slot size is 32 because of cudma bug
		ERTUSER_INFO(ert_user, "Limitting CQ size to 32 due to ERT CUDMA bug\n");
		ert_num_slots = 32;
		cfg->slot_size = ert_user->cq_range / ert_num_slots;
	}

	if (ert_poll) {
		ERTUSER_INFO(ert_user, "configuring dataflow mode with ert polling\n");
		cfg->slot_size = ert_user->cq_range / MAX_CUS;
		cfg->cu_isr = 0;
		cfg->cu_dma = 0;
		ert_user->polling_mode = cfg->polling;
		ert_user->num_slots = ert_user->cq_range / cfg->slot_size;
	} else if (ert_full) {
		ERTUSER_INFO(ert_user, "configuring embedded scheduler mode\n");
		ert_user->cq_intr = cfg->cq_int;
		ert_user->polling_mode = cfg->polling;
		ert_user->num_slots = ert_user->cq_range / cfg->slot_size;
		cfg->dsa52 = dsa;
		cfg->cdma = cdma ? 1 : 0;
	}

	if (XDEV(xdev)->priv.flags & XOCL_DSAFLAG_CUDMA_OFF)
		cfg->cu_dma = 0;

	cfg->dmsg = ert_user->ert_dmsg;
	cfg->echo = ert_user->echo;
	cfg->intr = ert_user->intr;

	// The KDS side of of the scheduler is now configured.  If ERT is
	// enabled, then the configure command will be started asynchronously
	// on ERT.  The shceduler is not marked configured until ERT has
	// completed (exec_finish_cmd); this prevents other processes from
	// submitting commands to same xclbin.  However we must also stop
	// other processes from submitting configure command on this same
	// xclbin while ERT asynchronous configure is running.
	//exec->configure_active = true;

	ERTUSER_INFO(ert_user, "scheduler config ert(%d), dataflow(%d), slots(%d), cudma(%d), cuisr(%d)\n"
		 , ert_poll | ert_full
		 , cfg->dataflow
		 , ert_user->num_slots
		 , cfg->cu_dma ? 1 : 0
		 , cfg->cu_isr ? 1 : 0);

	// TODO: reset all queues
	ert_user_reset(ert_user);

	return 0;
}
/**
 * process_ert_rq() - Process run queue
 * @ert_user: Target XRT ERT
 *
 * Return: return 0 if run queue is empty or no available slot
 *	   Otherwise, return 1
 */
static inline int process_ert_rq(struct xocl_ert_user *ert_user)
{
	struct ert_user_command *ecmd, *next;
	u32 slot_addr = 0, i;
	struct ert_packet *epkt = NULL;
	xdev_handle_t xdev = xocl_get_xdev(ert_user->pdev);
	struct kds_client *ev_client = NULL;

	if (!ert_user->num_rq)
		return 0;

	ev_client = first_event_client_or_null(ert_user);
	list_for_each_entry_safe(ecmd, next, &ert_user->rq, list) {
		struct kds_command *xcmd = ecmd->xcmd;

		if (unlikely(ert_user->bad_state || (ev_client == xcmd->client))) {
			ERTUSER_ERR(ert_user, "%s abort\n", __func__);
			ecmd->status = KDS_ERROR;
			list_move_tail(&ecmd->list, &ert_user->cq);
			--ert_user->num_rq;
			++ert_user->num_cq;
			continue;
		}

		if (cmd_opcode(ecmd) == OP_CONFIG) {
			if (ert_cfg_cmd(ert_user, ecmd)) {

				ERTUSER_ERR(ert_user, "%s config cmd error\n", __func__);
				ecmd->status = KDS_ABORT;
				list_move_tail(&ecmd->list, &ert_user->cq);
				--ert_user->num_rq;
				++ert_user->num_cq;
				continue;
			}
		} else if (cmd_opcode(ecmd) == OP_START) {
			if (ert_user->ctrl_busy || !ert_user->config)
				return 0;
		}

		if (ert_acquire_slot(ert_user, ecmd) == no_index) {
			ERTUSER_DBG(ert_user, "%s not slot available\n", __func__);
			return 0;
		}
		epkt = (struct ert_packet *)ecmd->xcmd->execbuf;
		ERTUSER_DBG(ert_user, "%s op_code %d ecmd->slot_idx %d\n", __func__, cmd_opcode(ecmd), ecmd->slot_idx);

		//sched_debug_packet(epkt, epkt->count+sizeof(epkt->header)/sizeof(u32));

		if (cmd_opcode(ecmd) == OP_CONFIG && !ert_user->polling_mode) {
			for (i = 0; i < ert_user->num_slots; i++) {
				xocl_intc_ert_request(xdev, i, ert_user_isr, ert_user);
				xocl_intc_ert_config(xdev, i, true);
			}

		}
		slot_addr = ecmd->slot_idx * (ert_user->cq_range/ert_user->num_slots);

		/* Hardware could be pretty fast, add to sq before touch the CQ_status or cmd queue*/
		list_move_tail(&ecmd->list, &ert_user->sq);
		ert_user->submit_queue[ecmd->slot_idx] = ecmd;
		--ert_user->num_rq;
		++ert_user->num_sq;

		ERTUSER_DBG(ert_user, "%s slot_addr %x\n", __func__, slot_addr);
		if (kds_echo) {
			ecmd->completed = true;
		} else {
			if (cmd_opcode(ecmd) == OP_START) {
				// write kds selected cu_idx in first cumask (first word after header)
				iowrite32(ecmd->xcmd->cu_idx, ert_user->cq_base + slot_addr + 4);

				// write remaining packet (past header and cuidx)
				xocl_memcpy_toio(ert_user->cq_base + slot_addr + 8,
						 ecmd->xcmd->execbuf+2, (epkt->count-1)*sizeof(u32));
			} else {
				xocl_memcpy_toio(ert_user->cq_base + slot_addr + 4,
					  ecmd->xcmd->execbuf+1, epkt->count*sizeof(u32));
			}

			iowrite32(epkt->header, ert_user->cq_base + slot_addr);
		}
		if (ert_user->cq_intr) {
			u32 mask_idx = mask_idx32(ecmd->slot_idx);
			u32 cq_int_addr = (mask_idx << 2);
			u32 mask = 1 << idx_in_mask32(ecmd->slot_idx, mask_idx);

			ERTUSER_DBG(ert_user, "++ mb_submit writes slot mask 0x%x to CQ_INT register at addr 0x%x\n",
					mask, cq_int_addr);
			xocl_intc_ert_write32(xdev, mask, cq_int_addr);
		}
	}

	return 1;
}

/**
 * process_ert_rq() - Process pending queue
 * @ert_user: Target XRT ERT
 *
 * Move all of the pending queue commands to the tail of run queue
 * and re-initialized pending queue
 */
static inline void process_ert_pq(struct xocl_ert_user *ert_user)
{
	unsigned long flags;

	/* Get pending queue command number without lock.
	 * The idea is to reduce the possibility of conflict on lock.
	 * Need to check pending command number again after lock.
	 */
	if (!ert_user->num_pq)
		return;
	spin_lock_irqsave(&ert_user->pq_lock, flags);
	if (ert_user->num_pq) {
		list_splice_tail_init(&ert_user->pq, &ert_user->rq);
		ert_user->num_rq += ert_user->num_pq;
		ert_user->num_pq = 0;
	}
	spin_unlock_irqrestore(&ert_user->pq_lock, flags);
}

static void ert_user_reset(struct xocl_ert_user *ert_user)
{
	bitmap_zero(ert_user->slot_status, ERT_MAX_SLOTS);
}

static void ert_user_submit(struct kds_ert *ert, struct kds_command *xcmd)
{
	unsigned long flags;
	bool first_command = false;
	struct xocl_ert_user *ert_user = container_of(ert, struct xocl_ert_user, ert);
	struct ert_user_command *ecmd = ert_user_alloc_cmd(xcmd);

	if (!ecmd)
		return;

	ERTUSER_DBG(ert_user, "->%s ecmd %llx\n", __func__, (u64)ecmd);
	spin_lock_irqsave(&ert_user->pq_lock, flags);
	list_add_tail(&ecmd->list, &ert_user->pq);
	++ert_user->num_pq;
	first_command = (ert_user->num_pq == 1);
	spin_unlock_irqrestore(&ert_user->pq_lock, flags);
	/* Add command to pending queue
	 * wakeup service thread if it is the first command
	 */
	if (first_command)
		up(&ert_user->sem);

	ERTUSER_DBG(ert_user, "<-%s\n", __func__);
	return;
}

int ert_user_thread(void *data)
{
	struct xocl_ert_user *ert_user = (struct xocl_ert_user *)data;
 	int ret = 0;
	bool polling_sleep = false, intr_sleep = false, no_completed_cmd = false, 
		cant_submit = false, no_need_to_fetch_new_cmd = false, no_event = false;


	mod_timer(&ert_user->timer, jiffies + ERT_TIMER);

	while (!ert_user->stop) {
		/* Make sure to submit as many commands as possible.
		 * This is why we call continue here. This is important to make
		 * CU busy, especially CU has hardware queue.
		 */
		if (process_ert_rq(ert_user))
			continue;
		/* process completed queue before submitted queue, for
		 * two reasons:
		 * - The last submitted command may be still running
		 * - while handling completed queue, running command might done
		 * - process_ert_sq_polling will check CU status, which is thru slow bus
		 */
		process_ert_sq(ert_user);
		process_ert_sq_polling(ert_user);

		process_ert_cq(ert_user);

		/* ert polling mode goes to sleep only if it doesn't have to poll
		 * submitted queue to check the completion
		 * ert interrupt mode goes to sleep if there is no cmd to be submitted
		 * OR submitted queue is full
		 */
		no_completed_cmd = !ert_user->num_cq;
		cant_submit = !ert_user->num_rq || (ert_user->num_sq == (ert_user->num_slots-1));
		no_need_to_fetch_new_cmd = ert_user->num_rq !=0 || !ert_user->num_pq;

		intr_sleep = no_completed_cmd && no_need_to_fetch_new_cmd && cant_submit;
		polling_sleep = (ert_user->polling_mode && !ert_user->num_sq);
		no_event = first_event_client_or_null(ert_user) == NULL;

		/* If any event occured, we should drain all the related commands ASAP
		 * It only goes to sleep if there is no event
		 */
		if (no_event && (intr_sleep || polling_sleep))
			if (down_interruptible(&ert_user->sem))
				ret = -ERESTARTSYS;

		process_ert_pq(ert_user);
	}
	del_timer_sync(&ert_user->timer);

	if (!ert_user->bad_state)
		ret = -EBUSY;

	return ret;
}

/**
 * xocl_ert_user_abort() - Sent an abort event to ERT thread
 * @ert_user: Target XRT ERT
 * @client: The client tries to abort commands
 *
 * This is used to ask ERT thread to abort all commands from the client.
 */
static void xocl_ert_user_abort(struct kds_ert *ert, struct kds_client *client, int cu_idx)
{
	struct kds_client *curr;
	struct xocl_ert_user *ert_user = container_of(ert, struct xocl_ert_user, ert);

	mutex_lock(&ert_user->ev_lock);
	if (list_empty(&ert_user->events))
		goto add_event;

	/* avoid re-add the same client */
	list_for_each_entry(curr, &ert_user->events, ev_entry) {
		if (client == curr)
			goto done;
	}

add_event:
	client->ev_type = EV_ABORT;
	list_add_tail(&client->ev_entry, &ert_user->events);
	/* The process thread may asleep, we should wake it up if 
	 * abort event takes place
	 */
	up(&ert_user->sem);
done:
	mutex_unlock(&ert_user->ev_lock);
}

/**
 * xocl_ert_user_abort() - Get done flag of abort
 * @ert_user: Target XRT ERT
 *
 * Use this to wait for abort event done
 */
static bool xocl_ert_user_abort_done(struct kds_ert *ert, struct kds_client *client, int cu_idx)
{
	struct kds_client *curr;
	struct kds_client *next;
	struct xocl_ert_user *ert_user = container_of(ert, struct xocl_ert_user, ert);

	mutex_lock(&ert_user->ev_lock);
	if (list_empty(&ert_user->events))
		goto done;

	list_for_each_entry_safe(curr, next, &ert_user->events, ev_entry) {
		if (client != curr)
			continue;

		list_del(&curr->ev_entry);
		break;
	}

done:
	mutex_unlock(&ert_user->ev_lock);

	return ert_user->bad_state;
}

static int ert_user_remove(struct platform_device *pdev)
{
	struct xocl_ert_user *ert_user;
	void *hdl;
	u32 i = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	ert_user = platform_get_drvdata(pdev);
	if (!ert_user) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &ert_user_attr_group);

	xocl_drvinst_release(ert_user, &hdl);

	if (ert_user->cfg_gpio)
		iounmap(ert_user->cfg_gpio);

	if (ert_user->cq_base)
		iounmap(ert_user->cq_base);

	for (i = 0; i < ert_user->num_slots; i++) {
		xocl_intc_ert_config(xdev, i, false);
		xocl_intc_ert_request(xdev, i, NULL, NULL);
	}

	ert_user->stop = 1;
	up(&ert_user->sem);
	(void) kthread_stop(ert_user->thread);

	platform_set_drvdata(pdev, NULL);

	xocl_drvinst_free(hdl);

	return 0;
}

static int ert_user_probe(struct platform_device *pdev)
{
	struct xocl_ert_user *ert_user;
	struct resource *res;
	int err = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_ert_sched_privdata *priv = NULL;

	ert_user = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_ert_user));
	if (!ert_user)
		return -ENOMEM;

	ert_user->dev = &pdev->dev;
	ert_user->pdev = pdev;
	/* Initialize pending queue and lock */
	INIT_LIST_HEAD(&ert_user->pq);
	spin_lock_init(&ert_user->pq_lock);
	/* Initialize run queue */
	INIT_LIST_HEAD(&ert_user->rq);

	/* Initialize completed queue */
	INIT_LIST_HEAD(&ert_user->cq);
	INIT_LIST_HEAD(&ert_user->sq);

	mutex_init(&ert_user->ev_lock);
	INIT_LIST_HEAD(&ert_user->events);

	sema_init(&ert_user->sem, 0);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	setup_timer(&ert_user->timer, ert_timer, (unsigned long)ert_user);
#else
	timer_setup(&ert_user->timer, ert_timer, 0);
#endif
	atomic_set(&ert_user->tick, 0);

	ert_user->thread = kthread_run(ert_user_thread, ert_user, "ert_thread");

	platform_set_drvdata(pdev, ert_user);
	mutex_init(&ert_user->lock);

	if (XOCL_GET_SUBDEV_PRIV(&pdev->dev)) {
		priv = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
		memcpy(&ert_user->ert_cfg_priv, priv, sizeof(*priv));
	} else {
		xocl_err(&pdev->dev, "did not get private data");
	}


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "did not get memory");
		err = -ENOMEM;
		goto done;
	}

	xocl_info(&pdev->dev, "CQ IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);

	ert_user->cq_range = res->end - res->start + 1;
	ert_user->cq_base = ioremap_wc(res->start, ert_user->cq_range);
	if (!ert_user->cq_base) {
		err = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto done;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		xocl_info(&pdev->dev, "CFG GPIO start: 0x%llx, end: 0x%llx",
			res->start, res->end);

		ert_user->cfg_gpio = ioremap_nocache(res->start, res->end - res->start + 1);
		if (!ert_user->cfg_gpio) {
			err = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto done;
		}
	}

	err = sysfs_create_group(&pdev->dev.kobj, &ert_user_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create ert_user sysfs attrs failed: %d", err);
	}
	ert_user->ert.submit = ert_user_submit;
	ert_user->ert.abort = xocl_ert_user_abort;
	ert_user->ert.abort_done = xocl_ert_user_abort_done;
	xocl_kds_init_ert(xdev, &ert_user->ert);

done:
	if (err) {
		ert_user_remove(pdev);
		return err;
	}
	return 0;
}

struct xocl_drv_private ert_user_priv = {
	.ops = &ert_user_ops,
	.dev = -1,
};

struct platform_device_id ert_user_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ERT_USER), (kernel_ulong_t)&ert_user_priv },
	{ },
};

static struct platform_driver	ert_user_driver = {
	.probe		= ert_user_probe,
	.remove		= ert_user_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ERT_USER),
	},
	.id_table = ert_user_id_table,
};

int __init xocl_init_ert_user(void)
{
	return platform_driver_register(&ert_user_driver);
}

void xocl_fini_ert_user(void)
{
	platform_driver_unregister(&ert_user_driver);
}
