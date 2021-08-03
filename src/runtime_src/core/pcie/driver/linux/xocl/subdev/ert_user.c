/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
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
#include "xrt_ert.h"

//#define	SCHED_VERBOSE	1

/* XRT ERT timer macros */
/* A low frequence timer for ERT to check if command timeout */
#define ERT_TICKS_PER_SEC	2
#define ERT_TIMER		(HZ / ERT_TICKS_PER_SEC) /* in jiffies */
#define ERT_EXEC_DEFAULT_TTL	(5UL * ERT_TICKS_PER_SEC)

#ifdef SCHED_VERBOSE
#define	ERTUSER_ERR(ert_user, fmt, arg...)	\
	xocl_err(ert_user->dev, fmt "", ##arg)
#define	ERTUSER_WARN(ert_user, fmt, arg...)	\
	xocl_warn(ert_user->dev, fmt "", ##arg)
#define	ERTUSER_INFO(ert_user, fmt, arg...)	\
	xocl_info(ert_user->dev, fmt "", ##arg)
#define	ERTUSER_DBG(ert_user, fmt, arg...)	\
	xocl_info(ert_user->dev, fmt "", ##arg)

#else
#define	ERTUSER_ERR(ert_user, fmt, arg...)	\
	xocl_err(ert_user->dev, fmt "", ##arg)
#define	ERTUSER_WARN(ert_user, fmt, arg...)	\
	xocl_warn(ert_user->dev, fmt "", ##arg)
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

struct ert_user_event {
	struct mutex		  lock;
	void			 *client;
	int			  state;
};

struct ert_user_queue {
	struct list_head	head;
	uint32_t		num;
};


struct ert_cu_stat {
	u64		usage;
	u32		inflight;
};

struct xocl_ert_user {
	struct device		*dev;
	struct platform_device	*pdev;
	bool			polling_mode;
	struct mutex		lock;
	struct kds_ert		ert;

	/* Configure dynamically */
	unsigned int		num_slots;
	unsigned int		slot_size;
	bool			is_configured;
	bool			ctrl_busy;
	struct xocl_ert_sched_privdata ert_cfg_priv;

	struct ert_user_queue	pq;
	struct ert_user_queue	pq_ctrl;

	spinlock_t		pq_lock;
	/*
	 * Pending Q is used in thread that is submitting CU cmds.
	 * Other Qs are used in thread that is completing them.
	 * In order to prevent false sharing, they need to be in different
	 * cache lines. Hence we add a "padding" in between (assuming 128-byte
	 * is big enough for most CPU architectures).
	 */
	u64			padding[16];
	/* run queue */
	struct ert_user_queue	rq;
	struct ert_user_queue	rq_ctrl;


	struct semaphore	sem;
	/* submitted queue */
	struct ert_user_queue	sq;

	struct ert_user_queue	cq;

	u32			stop;
	bool			bad_state;

	struct mutex		ev_lock;
	struct list_head	events;

	struct timer_list	timer;
	atomic_t		tick;

	struct task_struct	*thread;

	uint32_t		ert_dmsg;
	uint32_t		echo;
	uint32_t		intr;

	/* TODO: Before we have partition queue, we need
	 * record CU statistics in this place.
	 */
	struct ert_cu_stat	cu_stat[MAX_CUS];
	uint32_t		num_cus;

	struct ert_queue	*queue;
	/* ert validate result cache*/
	struct ert_validate_cmd ert_valid;
};

static void ert_submit_exit_cmd(struct xocl_ert_user *ert_user);

static ssize_t clock_timestamp_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%u\n", ert_user->ert_valid.timestamp);
}

static DEVICE_ATTR_RO(clock_timestamp);

static ssize_t snap_shot_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "pq:%d pq_ctrl:%d,  rq:%d, rq_ctrl:%d, sq:%d cq:%d\n", ert_user->pq.num, ert_user->pq_ctrl.num,
			ert_user->rq.num, ert_user->rq_ctrl.num, ert_user->sq.num, ert_user->cq.num);
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

static ssize_t mb_sleep_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_ert_user *ert_user = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	u32 go_sleep;

	if (kstrtou32(buf, 10, &go_sleep) == -EINVAL || go_sleep > 2) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo 0 or 1 > mb_sleep");
		return -EINVAL;
	}

	if (go_sleep) {
		ert_submit_exit_cmd(ert_user);
		xocl_gpio_cfg(xdev, MB_SLEEP);
	} else
		xocl_gpio_cfg(xdev, MB_WAKEUP);

	return count;
}

static ssize_t mb_sleep_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	return sprintf(buf, "%d", xocl_gpio_cfg(xdev, MB_STATUS));
}

static DEVICE_ATTR_RW(mb_sleep);

static ssize_t cq_read_cnt_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%d\n", ert_user->ert_valid.cq_read_single);
}

static DEVICE_ATTR_RO(cq_read_cnt);

static ssize_t cq_write_cnt_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%d\n", ert_user->ert_valid.cq_write_single);
}

static DEVICE_ATTR_RO(cq_write_cnt);

static ssize_t cu_read_cnt_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%d\n", ert_user->ert_valid.cu_read_single);
}

static DEVICE_ATTR_RO(cu_read_cnt);

static ssize_t cu_write_cnt_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));

	return sprintf(buf, "%d\n", ert_user->ert_valid.cu_write_single);
}

static DEVICE_ATTR_RO(cu_write_cnt);

static ssize_t stat_show(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(to_platform_device(dev));
	ssize_t sz = 0;
	char *fmt = "%lld %d\n";
	int i;

	/* formatted CU statistics one CU per line */
	for (i = 0; i < ert_user->num_cus; i++) {
		sz += scnprintf(buf+sz, PAGE_SIZE - sz, fmt,
				ert_user->cu_stat[i].usage,
				ert_user->cu_stat[i].inflight);
	}

	return sz;
}
static DEVICE_ATTR_RO(stat);

static struct attribute *ert_user_attrs[] = {
	&dev_attr_clock_timestamp.attr,
	&dev_attr_ert_dmsg.attr,
	&dev_attr_snap_shot.attr,
	&dev_attr_ert_echo.attr,
	&dev_attr_ert_intr.attr,
	&dev_attr_mb_sleep.attr,
	&dev_attr_cq_read_cnt.attr,
	&dev_attr_cq_write_cnt.attr,
	&dev_attr_cu_read_cnt.attr,
	&dev_attr_cu_write_cnt.attr,
	&dev_attr_stat.attr,
	NULL,
};

static struct attribute_group ert_user_attr_group = {
	.attrs = ert_user_attrs,
};

static inline void
ert_queue_poll(struct xocl_ert_user *ert_user)
{
	struct ert_queue *queue = ert_user->queue;

	if (!queue)
		return;

	queue->func->poll(queue->handle);
}

static inline int
ert_queue_submit(struct xocl_ert_user *ert_user, struct xrt_ert_command *ecmd)
{
	struct ert_queue *queue = ert_user->queue;

	if (!queue)
		return -ENODEV;

	return queue->func->submit(ecmd, queue->handle);
}

static irqreturn_t
ert_user_versal_isr(void *arg)
{
	struct xocl_ert_user *ert_user = (struct xocl_ert_user *)arg;
	struct ert_queue *queue = NULL;
	xdev_handle_t xdev;

	BUG_ON(!ert_user);

	ERTUSER_DBG(ert_user, "-> %s\n", __func__);
	xdev = xocl_get_xdev(ert_user->pdev);
	queue = ert_user->queue;

	if (!queue || ert_user->polling_mode)
		return IRQ_NONE;

	return queue->func->irq_handle(0, queue->handle);
}

static irqreturn_t
ert_user_isr(int irq, void *arg)
{
	struct xocl_ert_user *ert_user = (struct xocl_ert_user *)arg;
	struct ert_queue *queue = NULL;
	xdev_handle_t xdev;

	BUG_ON(!ert_user);

	ERTUSER_DBG(ert_user, "-> %s\n", __func__);
	xdev = xocl_get_xdev(ert_user->pdev);
	queue = ert_user->queue;

	if (!queue || ert_user->polling_mode)
		return IRQ_NONE;

	return queue->func->irq_handle(irq, queue->handle);
}

static inline void
ert_intc_config(struct xocl_ert_user *ert_user, bool enable)
{
	xdev_handle_t xdev = xocl_get_xdev(ert_user->pdev);
	uint32_t i;

	if (XOCL_DSA_IS_VERSAL(xdev)) {
		if (enable)
			xocl_mailbox_versal_request_intr(xdev, ert_user_versal_isr, ert_user);
		else
			xocl_mailbox_versal_free_intr(xdev);
		return;
	}

	for (i = 0; i < ert_user->num_slots; i++) {
		if (enable) {
			xocl_intc_ert_request(xdev, i, ert_user_isr, ert_user);
			xocl_intc_ert_config(xdev, i, true);
		} else {
			xocl_intc_ert_config(xdev, i, false);
			xocl_intc_ert_request(xdev, i, NULL, NULL);
		}
	}
}

static inline int
ert_config_queue(struct xocl_ert_user *ert_user, uint32_t slot_size)
{
	struct ert_queue *queue = ert_user->queue;

	if (!queue)
		return -ENODEV;

	return queue->func->queue_config(slot_size, ert_user, queue->handle);
}

static inline uint32_t
ert_queue_max_slot_num(struct xocl_ert_user *ert_user)
{
	struct ert_queue *queue = ert_user->queue;

	if (!queue)
		return 0;

	return queue->func->max_slot_num(queue->handle);
}

static int ert_user_bulletin(struct platform_device *pdev, struct ert_cu_bulletin *brd)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int ret = 0;

	if (!brd)
		return -EINVAL;

	brd->sta.configured = ert_user->is_configured;
	brd->cap.cu_intr = (xocl_gpio_cfg(xdev, MB_STATUS) != -ENODEV) ? 1 : 0;

	return ret;
}

static void ert_user_cmd_complete(struct xrt_ert_command *ecmd, void *core)
{
	struct xocl_ert_user *ert_user = ((struct xocl_ert_user *)(core));

	list_add_tail(&ecmd->list, &ert_user->cq.head);
	--ert_user->sq.num;
	++ert_user->cq.num;
}

static void ert_user_cmd_notify(void *core)
{
	struct xocl_ert_user *ert_user = ((struct xocl_ert_user *)(core));

	up(&ert_user->sem);
}

static void ert_submit_exit_cmd(struct xocl_ert_user *ert_user)
{
	struct xrt_ert_command *ecmd = kzalloc(sizeof(struct xrt_ert_command), GFP_KERNEL);
	bool first_command = false;
	unsigned long flags = 0;


	ERTUSER_ERR(ert_user, "%s\n", __func__);

	if (!ecmd)
		return;

	/* hack, find a in-place unused field as payload */
	ecmd->cu_idx = ERT_EXIT_CMD;

	ecmd->payload = &ecmd->cu_idx;
	ecmd->payload_size = 1;
	ecmd->cb.complete = ert_user_cmd_complete;
	ecmd->cb.notify = ert_user_cmd_notify;

	spin_lock_irqsave(&ert_user->pq_lock, flags);
	list_add_tail(&ecmd->list, &ert_user->pq_ctrl.head);
	++ert_user->pq_ctrl.num;
	first_command = ((ert_user->pq.num + ert_user->pq_ctrl.num) == 1);
	spin_unlock_irqrestore(&ert_user->pq_lock, flags);
	/* Add command to pending queue
	 * wakeup service thread if it is the first command
	 */
	if (first_command)
		up(&ert_user->sem);
}

static int ert_user_enable(struct platform_device *pdev, bool enable)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int ret = 0;

	if (enable) {
		xocl_gpio_cfg(xdev, MB_WAKEUP);

		ert_intc_config(ert_user, true);
		xocl_gpio_cfg(xdev, INTR_TO_ERT);
	} else {
		ert_submit_exit_cmd(ert_user);
		xocl_gpio_cfg(xdev, MB_SLEEP);

		ert_intc_config(ert_user, false);
		xocl_gpio_cfg(xdev, INTR_TO_CU);
	}

	return ret;
}

static void ert_user_init_queue(struct platform_device *pdev, void *core)
{
	struct xocl_ert_user *ert_user = platform_get_drvdata(pdev);
	struct ert_queue *queue = (struct ert_queue *)core;

	ert_user->queue = queue;
	ert_user->num_slots = ert_queue_max_slot_num(ert_user);
}

static struct xocl_ert_user_funcs ert_user_ops = {
	.bulletin = ert_user_bulletin,
	.enable = ert_user_enable,
	.init_queue = ert_user_init_queue,
};

static void ert_user_submit(struct kds_ert *ert, struct kds_command *xcmd);


/*
 * cmd_opcode() - Command opcode
 *
 * @cmd: Command object
 * Return: Opcode of command
 */
static inline u32
cmd_opcode(struct xrt_ert_command *ecmd)
{
	struct ert_packet *epkt = (struct ert_packet *)ecmd->payload;

	return epkt->opcode;
}

static void ert_free_cmd(struct xrt_ert_command *ecmd)
{
	kfree(ecmd);
}

static struct xrt_ert_command *ert_user_alloc_cmd(struct kds_command *xcmd, uint32_t return_max_size)
{
	uint32_t return_size = 0, cu_idx = 0;
	struct xrt_ert_command *ecmd = kzalloc(sizeof(struct xrt_ert_command), GFP_KERNEL);
	struct ert_packet *epkt = NULL;

	if (!ecmd)
		return NULL;

	ecmd->payload = xcmd->execbuf;

	switch (cmd_opcode(ecmd)) {
	case ERT_CU_STAT:
		return_size = return_max_size;
		break;
	case ERT_SK_START:
		return_size = 2 * sizeof(u32);
		break;
	case ERT_MB_VALIDATE:
	case ERT_CLK_CALIB:
		return_size = sizeof(struct ert_validate_cmd);
		break;
	case ERT_START_CU:
		cu_idx = xcmd->cu_idx;
		break;
	default:
		break;
	};

	ecmd->xcmd = xcmd;
	ecmd->return_size = return_size;

	epkt = (struct ert_packet *)xcmd->execbuf;

	ecmd->payload_size = epkt->count+1;
	ecmd->cu_idx = cu_idx;

	ecmd->cb.complete = ert_user_cmd_complete;
	ecmd->cb.notify = ert_user_cmd_notify;


	return ecmd;
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

static inline bool ert_special_cmd(struct xrt_ert_command *ecmd)
{
	bool ret;

	switch (cmd_opcode(ecmd)) {
	case ERT_EXIT:
	case ERT_CONFIGURE:
	case ERT_SK_CONFIG:
	case ERT_CU_STAT:
	case ERT_CLK_CALIB:
	case ERT_MB_VALIDATE:
		ret = true;
		break;
	default:
		ret = false;
	}

	return ret;
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

static int ert_cfg_cmd(struct xocl_ert_user *ert_user, struct xrt_ert_command *ecmd)
{
	xdev_handle_t xdev = xocl_get_xdev(ert_user->pdev);
	uint32_t *cdma = xocl_rom_cdma_addr(xdev);
	unsigned int dsa = ert_user->ert_cfg_priv.dsa;
	unsigned int major = ert_user->ert_cfg_priv.major;
	struct ert_configure_cmd *cfg = (struct ert_configure_cmd *)ecmd->xcmd->execbuf;
	bool ert_enabled = (XOCL_DSA_IS_VERSAL(xdev) || XOCL_DSA_IS_MPSOC(xdev)) ? 1 :
	    xocl_mb_sched_on(xdev);
	bool ert_full = !cfg->dataflow;
	bool ert_poll = cfg->dataflow;
	unsigned int ert_num_slots = 0, slot_size = 0, max_slot_num;
	uint64_t cq_range = ert_user->queue->size;

	BUG_ON(!ert_enabled);

	if (cmd_opcode(ecmd) != ERT_CONFIGURE)
		return -EINVAL;

	if (major > 3) {
		ERTUSER_ERR(ert_user, "Unknown ERT major version\n");
		return -EINVAL;
	}

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
	/*  1. cfg->slot_size need to be 32-bit aligned
	 *  2. the slot num max: 128
	 */
	ERTUSER_DBG(ert_user, "configuring scheduler cq_size(%lld)\n", cq_range);
	if (cq_range == 0 || cfg->slot_size == 0) {
		ERTUSER_ERR(ert_user, "should not have zeroed value of cq_size=%lld, slot_size=%d",
		    cq_range, cfg->slot_size);
		return -EINVAL;
	} else if (!IS_ALIGNED(cfg->slot_size, 4)) {
		ERTUSER_ERR(ert_user, "slot_size should be 4 bytes aligned, slot_size=%d",
		   cfg->slot_size);
		return -EINVAL;
	}

	slot_size = cfg->slot_size;

	max_slot_num = ert_queue_max_slot_num(ert_user);
	if (slot_size < (cq_range / max_slot_num))
		slot_size = cq_range / max_slot_num;

	ert_num_slots = cq_range / slot_size;

	if (ert_full && cfg->cu_dma && ert_num_slots > 32) {
		// Max slot size is 32 because of cudma bug
		ERTUSER_INFO(ert_user, "Limitting CQ size to 32 due to ERT CUDMA bug\n");
		slot_size = cq_range / 32;
	}

	cfg->slot_size = slot_size;

	if (ert_poll) {
		ERTUSER_INFO(ert_user, "configuring dataflow mode with ert polling\n");
		cfg->cu_isr = 0;
		cfg->cu_dma = 0;
	} else if (ert_full) {
		ERTUSER_INFO(ert_user, "configuring embedded scheduler mode\n");
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

	ERTUSER_INFO(ert_user, "scheduler config ert(%d), dataflow(%d), cudma(%d), cuisr(%d)\n"
		, cfg->ert
		, cfg->dataflow
		, cfg->cu_dma ? 1 : 0
		, cfg->cu_isr ? 1 : 0);

	return 0;
}

static int
ert_cfg_host(struct xocl_ert_user *ert_user, struct xrt_ert_command *ecmd)
{
	int ret = 0;
	xdev_handle_t xdev = xocl_get_xdev(ert_user->pdev);
	struct ert_configure_cmd *cfg = (struct ert_configure_cmd *)ecmd->xcmd->execbuf;
	bool ert = (XOCL_DSA_IS_VERSAL(xdev) || XOCL_DSA_IS_MPSOC(xdev)) ? 1 :
		xocl_mb_sched_on(xdev);

	BUG_ON(cmd_opcode(ecmd) != ERT_CONFIGURE);
	BUG_ON(!ert);

	ret = ert_config_queue(ert_user, cfg->slot_size);
	if (ret)
		return ret;

	ert_user->slot_size = cfg->slot_size;

	ert_user->num_slots = ert_user->queue->size / cfg->slot_size;

	ert_user->polling_mode = cfg->polling;
	/* if polling, disable intc, vice versa */
	ert_intc_config(ert_user, !ert_user->polling_mode);

	memset(ert_user->cu_stat, 0, cfg->num_cus * sizeof(struct ert_cu_stat));

	ert_user->num_cus = cfg->num_cus;

	ERTUSER_INFO(ert_user, "scheduler config ert completed, polling_mode(%d), slots(%d)\n"
		 , ert_user->polling_mode
		 , ert_user->num_slots);

	return 0;
}

static inline void
ert_post_process(struct xocl_ert_user *ert_user, struct xrt_ert_command *ecmd)
{
	if (likely(!ert_special_cmd(ecmd)))
		return;

	ERTUSER_DBG(ert_user, "%s %d", __func__, cmd_opcode(ecmd));
	switch (cmd_opcode(ecmd)) {
	case ERT_CONFIGURE:
		ert_user->is_configured = true;
		break;
	case ERT_MB_VALIDATE:
	case ERT_CLK_CALIB:
		memcpy(&ert_user->ert_valid, ecmd->payload+1, ecmd->return_size);
		break;
	case ERT_CU_STAT:
	case ERT_SK_START:
		memcpy(ecmd->xcmd->u_execbuf, ecmd->payload+1, ecmd->return_size);
		break;
	default:
		break;
	}
}


static inline bool
ert_pre_process(struct xocl_ert_user *ert_user, struct xrt_ert_command *ecmd)
{
	bool bad_cmd = false;

	switch (cmd_opcode(ecmd)) {
	case ERT_START_CU:
	case ERT_SK_START:
		BUG_ON(ert_user->ctrl_busy);
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
	#if defined(RHEL_RELEASE_CODE)
		#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8, 4)
			__attribute__ ((__fallthrough__));
		#else
		__attribute__ ((fallthrough));
		#endif
	#else
		__attribute__ ((fallthrough));
	#endif
#else
	__attribute__ ((__fallthrough__));
#endif
	case ERT_CLK_CALIB:
	case ERT_SK_CONFIG:
	case ERT_CU_STAT:
	case ERT_MB_VALIDATE:
		BUG_ON(!ert_user->is_configured);
		bad_cmd = false;
		break;
	case ERT_CONFIGURE:
		if (ert_cfg_cmd(ert_user, ecmd))
			bad_cmd = true;
		break;
	case ERT_EXIT:
		bad_cmd = false;
		break;
	default:
		bad_cmd = true;
	}

	return bad_cmd;
}

/**
 * process_ert_cq() - Process cmd witch is completed
 * @ert_user: Target XRT CU
 */
static inline void process_ert_cq(struct xocl_ert_user *ert_user)
{
	struct kds_command *xcmd;
	struct xrt_ert_command *ecmd;

	if (!ert_user->cq.num)
		return;

	ERTUSER_DBG(ert_user, "-> %s\n", __func__);

	while (ert_user->cq.num) {
		ecmd = list_first_entry(&ert_user->cq.head, struct xrt_ert_command, list);
		list_del(&ecmd->list);
		xcmd = ecmd->xcmd;
		if (ert_special_cmd(ecmd))
			ert_user->ctrl_busy = false;

		ert_post_process(ert_user, ecmd);
		if (xcmd) {
			if (cmd_opcode(ecmd) == ERT_START_CU &&
				ecmd->complete_entry.cstate == KDS_COMPLETED) {
				ert_user->cu_stat[xcmd->cu_idx].inflight--;
				ert_user->cu_stat[xcmd->cu_idx].usage++;
			}
			set_xcmd_timestamp(xcmd, ecmd->complete_entry.cstate);
			xcmd->cb.notify_host(xcmd, ecmd->complete_entry.cstate);
			xcmd->cb.free(xcmd);
		}
		ert_free_cmd(ecmd);
		--ert_user->cq.num;
	}

	ERTUSER_DBG(ert_user, "<- %s\n", __func__);
}

/**
 * process_ert_rq() - Process run queue
 * @ert_user: Target XRT ERT
 * @rq: Target running queue
 *
 * Return: return 0 if run queue is empty or no available slot
 *	   Otherwise, return 1
 */
static inline int process_ert_rq(struct xocl_ert_user *ert_user, struct ert_user_queue *rq)
{
	struct xrt_ert_command *ecmd, *next;
	struct ert_packet *epkt = NULL;
	struct kds_client *ev_client = NULL;
	bool bad_cmd = false;

	if (!rq->num)
		return 0;

	ERTUSER_DBG(ert_user, "%s =>\n", __func__);

	ev_client = first_event_client_or_null(ert_user);
	list_for_each_entry_safe(ecmd, next, &rq->head, list) {
		struct kds_command *xcmd = ecmd->xcmd;

		if (unlikely(ert_user->bad_state || (xcmd && ev_client == xcmd->client))) {
			ERTUSER_ERR(ert_user, "%s abort\n", __func__);
			ecmd->complete_entry.cstate = KDS_ERROR;
			bad_cmd = true;
		}

		if (ert_pre_process(ert_user, ecmd)) {
			ERTUSER_ERR(ert_user, "%s bad cmd, opcode: %d\n", __func__, cmd_opcode(ecmd));
			ecmd->complete_entry.cstate = KDS_ABORT;
			bad_cmd = true;
		}

		if (unlikely(bad_cmd)) {
			list_move_tail(&ecmd->list, &ert_user->cq.head);
			--rq->num;
			++ert_user->cq.num;
			continue;
		}

		/* Command is good, try to submit it */
		if (ert_special_cmd(ecmd)) {
			if (ert_user->ctrl_busy) {
				ERTUSER_DBG(ert_user, "ctrl slot is busy\n");
				return 0;
			}
			if (cmd_opcode(ecmd) != ERT_CU_STAT)
				ert_user->ctrl_busy = true;
		}

		if (cmd_opcode(ecmd) == ERT_CONFIGURE) {
			if (ert_cfg_host(ert_user, ecmd)) {
				ert_user->ctrl_busy = false;
				ERTUSER_ERR(ert_user, "%s unable to config queue\n", __func__);
				return 0;
			}
		}

		epkt = (struct ert_packet *)ecmd->payload;
		ERTUSER_DBG(ert_user, "%s op_code %d ecmd->handle %d\n", __func__, cmd_opcode(ecmd), ecmd->handle);

		//sched_debug_packet(epkt, epkt->count+sizeof(epkt->header)/sizeof(u32));

		/* Hardware could be pretty fast, add to sq before touch the CQ_status or cmd queue*/
		list_del(&ecmd->list);
		--rq->num;
		/* Even we don't move it to sq, we still like to track how many command we submitted to the queue entity */
		++ert_user->sq.num;
		if (ert_queue_submit(ert_user, ecmd)) {
			list_add(&ecmd->list, &rq->head);
			++rq->num;
			--ert_user->sq.num;
			return 0;
		}

		if (xcmd) {
			if (cmd_opcode(ecmd) == ERT_START_CU)
				ert_user->cu_stat[ecmd->xcmd->cu_idx].inflight++;

			set_xcmd_timestamp(ecmd->xcmd, KDS_RUNNING);
		}
	}
	ERTUSER_DBG(ert_user, "%s <=\n", __func__);
	return 1;
}

/**
 * process_ert_pq() - Process pending queue
 * @ert_user: Target XRT ERT
 * @pq: Target pending queue
 * @rq: Target running queue
 *
 * Move all of the pending queue commands to the tail of run queue
 * and re-initialized pending queue
 */
static inline void process_ert_pq(struct xocl_ert_user *ert_user, struct ert_user_queue *pq, struct ert_user_queue *rq)
{
	unsigned long flags;

	/* Get pending queue command number without lock.
	 * The idea is to reduce the possibility of conflict on lock.
	 * Need to check pending command number again after lock.
	 */
	if (!pq->num)
		return;

	spin_lock_irqsave(&ert_user->pq_lock, flags);
	if (pq->num) {
		list_splice_tail_init(&pq->head, &rq->head);
		rq->num += pq->num;
		pq->num = 0;
	}
	spin_unlock_irqrestore(&ert_user->pq_lock, flags);
}

static void ert_user_submit(struct kds_ert *kds_ert, struct kds_command *xcmd)
{
	unsigned long flags;
	bool first_command = false;
	struct xocl_ert_user *ert_user = container_of(kds_ert, struct xocl_ert_user, ert);
	struct xrt_ert_command *ecmd = ert_user_alloc_cmd(xcmd, ert_user->slot_size);

	if (!ecmd)
		return;

	ERTUSER_DBG(ert_user, "->%s ecmd %llx\n", __func__, (u64)ecmd);
	spin_lock_irqsave(&ert_user->pq_lock, flags);
	switch (cmd_opcode(ecmd)) {
	case ERT_START_CU:
	case ERT_SK_START:
		list_add_tail(&ecmd->list, &ert_user->pq.head);
		++ert_user->pq.num;
		break;
	case ERT_CLK_CALIB:
	case ERT_SK_CONFIG:
	case ERT_CU_STAT:
	case ERT_MB_VALIDATE:
	case ERT_CONFIGURE:
	default:
		list_add_tail(&ecmd->list, &ert_user->pq_ctrl.head);
		++ert_user->pq_ctrl.num;
		break;
	}
	first_command = ((ert_user->pq.num + ert_user->pq_ctrl.num) == 1);
	spin_unlock_irqrestore(&ert_user->pq_lock, flags);
	/* Add command to pending queue
	 * wakeup service thread if it is the first command
	 */
	if (first_command)
		up(&ert_user->sem);

	ERTUSER_DBG(ert_user, "<-%s\n", __func__);
}


static inline bool ert_user_thread_sleep_condition(struct xocl_ert_user *ert_user)
{
	bool ret = false;
	bool polling_sleep = false, intr_sleep = false, no_event = false
	, no_completed_cmd = false, no_submmited_cmd = false
	, cant_submit = false, cant_submit_start = false, cant_submit_ctrl = false
	, no_need_to_fetch_new_cmd = false, no_need_to_fetch_start_cmd = false, no_need_to_fetch_ctrl_cmd = false;


	/* When ert_thread should go to sleep to save CPU usage
	 * 1. There is no event to be processed
	 * 2. We don't have to process command when
	 *    a. We can't submit cmd if we don't have cmd in running queue or submitted queue is full
	 *    b. There is no cmd in pending queue or we still have cmds in running queue
	 *    c. There is no cmd in completed queue
	 * 3. We are not in polling mode and there is no cmd in submitted queue
	 */

	no_completed_cmd = !ert_user->cq.num;

	cant_submit_start = (!ert_user->rq.num) || (ert_user->sq.num == (ert_user->num_slots-1));
	cant_submit_ctrl = (!ert_user->rq_ctrl.num) || (ert_user->sq.num == 1);
	cant_submit = cant_submit_start && cant_submit_ctrl;

	no_need_to_fetch_start_cmd = ert_user->rq.num != 0 || !ert_user->pq.num;
	no_need_to_fetch_ctrl_cmd = ert_user->rq_ctrl.num != 0 || !ert_user->pq_ctrl.num;
	no_need_to_fetch_new_cmd = no_need_to_fetch_ctrl_cmd && no_need_to_fetch_start_cmd;

	no_submmited_cmd = !ert_user->sq.num;

	polling_sleep = no_completed_cmd && no_need_to_fetch_new_cmd && no_submmited_cmd;
	intr_sleep = no_completed_cmd && no_need_to_fetch_new_cmd && cant_submit;

	no_event = first_event_client_or_null(ert_user) == NULL;


	ret = no_event && ((ert_user->polling_mode && polling_sleep) || (!ert_user->polling_mode && intr_sleep));

	return ret;
}

int ert_user_thread(void *data)
{
	struct xocl_ert_user *ert_user = (struct xocl_ert_user *)data;
	int ret = 0;

	mod_timer(&ert_user->timer, jiffies + ERT_TIMER);

	while (!ert_user->stop) {
		/* Make sure to submit as many commands as possible.
		 * This is why we call continue here. This is important to make
		 * CU busy, especially CU has hardware queue.
		 */
		if (process_ert_rq(ert_user, &ert_user->rq_ctrl))
			continue;

		if (process_ert_rq(ert_user, &ert_user->rq))
			continue;
		/* process completed queue before submitted queue, for
		 * two reasons:
		 * - The last submitted command may be still running
		 * - while handling completed queue, running command might done
		 * - process_ert_sq_polling will check CU status, which is thru slow bus
		 */

		if (ert_user->sq.num != 0)
			ert_queue_poll(ert_user);

		process_ert_cq(ert_user);

		/* If any event occured, we should drain all the related commands ASAP
		 * It only goes to sleep if there is no event
		 */
		if (ert_user_thread_sleep_condition(ert_user)) {
			if (down_interruptible(&ert_user->sem))
				ret = -ERESTARTSYS;
		}

		process_ert_pq(ert_user, &ert_user->pq, &ert_user->rq);
		process_ert_pq(ert_user, &ert_user->pq_ctrl, &ert_user->rq_ctrl);
	}
	del_timer_sync(&ert_user->timer);

	if (!ert_user->bad_state)
		ret = -EBUSY;

	return ret;
}

/**
 * xocl_ert_abort() - Sent an abort event to ERT thread
 * @ert: Target XRT ERT
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
 * xocl_ert_abort() - Get done flag of abort
 * @ert: Target XRT ERT
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

	ert_user = platform_get_drvdata(pdev);
	if (!ert_user) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	ert_intc_config(ert_user, false);

	sysfs_remove_group(&pdev->dev.kobj, &ert_user_attr_group);

	xocl_drvinst_release(ert_user, &hdl);

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
	int err = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_ert_sched_privdata *priv = NULL;
	bool ert = xocl_ert_on(xdev);

	/* If XOCL_DSAFLAG_MB_SCHE_OFF is set, we should not probe ert */
	if (!ert) {
		xocl_warn(&pdev->dev, "Disable ERT flag overwrite, don't probe ert_user");
		return -ENODEV;
	}

	ert_user = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_ert_user));
	if (!ert_user)
		return -ENOMEM;

	ert_user->dev = &pdev->dev;
	ert_user->pdev = pdev;
	/* Initialize pending queue and lock */
	INIT_LIST_HEAD(&ert_user->pq.head);
	INIT_LIST_HEAD(&ert_user->pq_ctrl.head);
	spin_lock_init(&ert_user->pq_lock);
	/* Initialize run queue */
	INIT_LIST_HEAD(&ert_user->rq.head);
	INIT_LIST_HEAD(&ert_user->rq_ctrl.head);

	/* Initialize completed queue */
	INIT_LIST_HEAD(&ert_user->cq.head);

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


	err = sysfs_create_group(&pdev->dev.kobj, &ert_user_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create ert_user sysfs attrs failed: %d", err);
		goto done;
	}

	ert_user->polling_mode = false;

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
