// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx CU driver for memory to memory BO copy
 *
 * Copyright (C) 2021 Xilinx, Inc.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 */

#include "xrt_xclbin.h"
#include "../xocl_drv.h"
#include "xgq_cmd.h"
#include "../xocl_xgq_plat.h"
#include <linux/time.h>

/*
 * XGQ Host management driver design.
 * XGQ resources:
 *	XGQ submission queue (SQ)
 *	XGQ completion queue (CQ)
 * 	XGQ ring buffer
 *
 * XGQ server and client:
 *      XGQ server calls xgq_alloc to allocate SLOTs based on
 *	given slot_size and ring buffer size.
 *	XGQ client calls xgq_attch to get the same configuration
 *	that server has already been allocated.
 *
 * A typical operation:
 *      client                                         server
 *         | generate cmd                                |
 *         | xgq_produce to get SQ slot                  |
 *         | write cmd into SQ slot                      |
 *         | xgq_notify_peer_produced -----------------> |
 *         |                         xgq_consume SQ slot |
 *         |                       read cmd from SQ slot |
 *         |                        [cmd operations ...] |
 *         |                         xgq_produce CQ slot |
 *         |                      write cmd into CQ slot |
 *         | <----------------- xgq_notify_peer_produced |
 *         | xgq_consume CQ slot                         |
 *         | read cmd from CQ slot                       |
 *         | return results                              |
 *
 * The XGQ Host Mgmt driver is a client.
 * The server is running on ARM R5 embedded FreeRTOS.
 * 
 * Note: to minimized error-prone, current version only supports
 *	 synchronized operation, client always wait till server respond.
 */

#define	XGQ_ERR(xgq, fmt, arg...)	\
	xocl_err(&(xgq)->xgq_pdev->dev, fmt "\n", ##arg)
#define	XGQ_WARN(xgq, fmt, arg...)	\
	xocl_warn(&(xgq)->xgq_pdev->dev, fmt "\n", ##arg)
#define	XGQ_INFO(xgq, fmt, arg...)	\
	xocl_info(&(xgq)->xgq_pdev->dev, fmt "\n", ##arg)
#define	XGQ_DBG(xgq, fmt, arg...)	\
	xocl_dbg(&(xgq)->xgq_pdev->dev, fmt "\n", ##arg)

#define	XGQ_DEV_NAME "ospi_xgq" SUBDEV_SUFFIX

#define XOCL_XGQ_RING_LEN 0x1000 //4k, must be the same size on device
#define XOCL_XGQ_RESERVE_LEN 0x100 //256, reserved for device status
#define XOCL_XGQ_DATA_OFFSET (XOCL_XGQ_RING_LEN + XOCL_XGQ_RESERVE_LEN)
#define XOCL_XGQ_DEV_STAT_OFFSET (XOCL_XGQ_RING_LEN)

static DEFINE_IDR(xocl_xgq_cid_idr);

/* cmd timeout in seconds */
#define XOCL_XGQ_FLASH_TIME 	msecs_to_jiffies(10 * 60 * 1000) 
#define XOCL_XGQ_DOWNLOAD_TIME 	msecs_to_jiffies(30 * 1000) 
#define XOCL_XGQ_CONFIG_TIME 	msecs_to_jiffies(10 * 1000) 
#define XOCL_XGQ_MSLEEP_1S	(1000)      //1 s

/*
 * Note:
 * We should keep the following flags in-sync with RPU device
 * so that flags passed with XGQ commands(loadxclbin or configure)
 * will be able to be recongized by RPU.
 */
typedef enum xrt_xgq_pkt_type {
	XRT_XGQ_PKT_TYPE_UNKNOWN = 0,
	XRT_XGQ_PKT_TYPE_PDI,
	XRT_XGQ_PKT_TYPE_XCLBIN,
	XRT_XGQ_PKT_TYPE_AF,
} xrt_xgq_pkt_type_t;

typedef union {
	struct {
		u32 version:16;
		u32 type:16;
		u32 cid:16;
		u32 rcode:16;
	};
	u32 head[2];
} xgq_vmr_head_t;

struct xgq_vmr_payload_xclbin {
	u32 address;
	u32 size;
};

struct xgq_vmr_payload_af {
	u32 buffer_addr;
	u32 buffer_size;
};

struct xgq_vmr_pkt {
	xgq_vmr_head_t head;
	union {
		struct xgq_vmr_payload_xclbin payload_xclbin;
		struct xgq_vmr_payload_af payload_af;
	};
};

typedef void (*xocl_xgq_complete_cb)(void *arg, struct xrt_com_queue_entry *ccmd);

struct xocl_sq {
	struct xrt_sub_queue_entry 	xgq_sq_head;
	struct xgq_vmr_pkt 		xgq_sq_pkt;
};

struct xocl_xgq_cmd {
	struct xocl_sq		xgq_sq;
	struct list_head	xgq_cmd_head;
	struct completion	xgq_cmd_complete;
	xocl_xgq_complete_cb    xgq_cmd_cb;
	void			*xgq_cmd_arg;
	int			xgq_cmd_rcode;
	struct timer_list	xgq_cmd_timer;
	struct xocl_xgq		*xgq;
	u64			xgq_cmd_timeout_jiffies; /* timout till */
};

struct xocl_xgq;

struct xgq_worker {
	struct task_struct	*complete_thread;
	bool			error;
	bool			stop;
	struct xocl_xgq		*xgq;
};

struct xocl_xgq {
	struct platform_device 	*xgq_pdev;
	struct xgq	 	xgq_queue;
	u64			xgq_io_hdl;
	void __iomem		*xgq_ring_base;
	u32			xgq_slot_size;
	void __iomem		*xgq_sq_base;
	void __iomem		*xgq_cq_base;
	struct mutex 		xgq_lock;
	int 			xgq_polling;
	u32			xgq_intr_base;
	u32			xgq_intr_num;
	struct list_head	xgq_submitted_cmds;
	struct completion 	xgq_irq_complete;
	struct xgq_worker	xgq_complete_worker;
	struct xgq_worker	xgq_health_worker;
	bool			xgq_halted;
	bool			xgq_ospi_inuse;
	int 			xgq_cmd_id;
};

/*
 * when detect cmd is completed, find xgq_cmd from submitted_cmds list
 * and find cmd by cid; perform callback and remove from submitted_cmds.
 */
static void cmd_complete(struct xocl_xgq *xgq, struct xrt_com_queue_entry *ccmd)
{
	struct xocl_xgq_cmd *xgq_cmd;
	struct list_head *pos, *next;

	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_cmd, xgq_cmd_head);

		if (xgq_cmd->xgq_sq.xgq_sq_head.cid == ccmd->cid) {
			XGQ_INFO(xgq, "xgq cmd %d completed", ccmd->cid);

			list_del(pos);

			if (xgq_cmd->xgq_cmd_cb)
				xgq_cmd->xgq_cmd_cb(xgq_cmd->xgq_cmd_arg, ccmd);
			return;
		}

	}

	XGQ_WARN(xgq, "unknown cid %d received", ccmd->cid);
	return;
}

/*
 * Read completed cmd based on XGQ protocol.
 */
void read_completion(struct xrt_com_queue_entry *ccmd, u64 addr)
{
	u32 i;

	for (i = 0; i < XRT_COM_Q1_SLOT_SIZE / 4; i++)
		ccmd->data[i] = xgq_reg_read32(0, addr + i * 4);

	// Write 0 to first word to make sure the cmd state is not NEW
	xgq_reg_write32(0, addr, 0x0);
}

/*
 * thread to check if completion queue has new command to consume.
 * if there is one, completed it by read CQ entry and performe callback.
 * lastly, notify peer.
 */
static int complete_worker(void *data)
{
	struct xgq_worker *xw = (struct xgq_worker *)data;
	struct xocl_xgq *xgq = xw->xgq;

	while (!xw->stop) {
		
		/* TODO: if we have long stand incomplete cmds ? */
		while (!list_empty(&xgq->xgq_submitted_cmds)) {
			u64 slot_addr = 0;
			struct xrt_com_queue_entry ccmd;

			usleep_range(1000, 2000);

			if (kthread_should_stop()) {
				xw->stop = true;
			}

			mutex_lock(&xgq->xgq_lock);

			if (xgq_consume(&xgq->xgq_queue, &slot_addr)) {
				mutex_unlock(&xgq->xgq_lock);
				continue;
			}

			read_completion(&ccmd, slot_addr);

			cmd_complete(xgq, &ccmd);

			xgq_notify_peer_consumed(&xgq->xgq_queue);

			mutex_unlock(&xgq->xgq_lock);
		}

		if (xgq->xgq_polling) {
			usleep_range(1000, 2000);
		} else {
			wait_for_completion_interruptible(&xgq->xgq_irq_complete);
		}

		if (kthread_should_stop()) {
			xw->stop = true;
		}
	}
	
	return xw->error ? 1 : 0;
}

static bool xgq_submitted_cmd_check(struct xocl_xgq *xgq)
{
	struct xocl_xgq_cmd *xgq_cmd;
	struct list_head *pos, *next;
	bool found_timeout = false;

	mutex_lock(&xgq->xgq_lock);
	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_cmd, xgq_cmd_head);

		XGQ_INFO(xgq, "cmd %llx, jiffies %llx",
			xgq_cmd->xgq_cmd_timeout_jiffies, jiffies);

		/* Finding timed out cmds */
		if (xgq_cmd->xgq_cmd_timeout_jiffies < jiffies) {
			XGQ_ERR(xgq, "cmd id: %d timed out, hot reset is required!",
				xgq_cmd->xgq_sq.xgq_sq_head.cid);
			found_timeout = true;
			break;
		}
	}
	mutex_unlock(&xgq->xgq_lock);

	return found_timeout;
}

static void xgq_submitted_cmds_drain(struct xocl_xgq *xgq)
{
	struct xocl_xgq_cmd *xgq_cmd;
	struct list_head *pos, *next;

	mutex_lock(&xgq->xgq_lock);
	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_cmd, xgq_cmd_head);

		XGQ_INFO(xgq, "cmd %llx, jiffies %llx",
			xgq_cmd->xgq_cmd_timeout_jiffies, jiffies);

		/* Finding timed out cmds */
		if (xgq_cmd->xgq_cmd_timeout_jiffies < jiffies) {
			list_del(pos);
			
			xgq_cmd->xgq_cmd_rcode = -ETIME;
			complete(&xgq_cmd->xgq_cmd_complete);
			XGQ_ERR(xgq, "cmd id: %d timed out, hot reset is required!",
				xgq_cmd->xgq_sq.xgq_sq_head.cid);
		}
	}
	mutex_unlock(&xgq->xgq_lock);
}

/*
 * When driver detach, we need to wait for all commands to drain.
 * If the one command is already timedout, we can safely recycle it only
 * after disable interrupts and mark device in bad state, a hot_reset
 * is needed to recover the device back to normal.
 */
static bool xgq_submitted_cmds_empty(struct xocl_xgq *xgq)
{
	mutex_lock(&xgq->xgq_lock);
	if (list_empty(&xgq->xgq_submitted_cmds)) {
		mutex_unlock(&xgq->xgq_lock);
		return true;
	}
	mutex_unlock(&xgq->xgq_lock);
	
	return false;
}

/*
 * stop service will be called from driver remove or found timeout cmd from health_worker
 * 3 steps to stop the service:
 *   1) halt any incoming request
 *   2) disable interrupts
 *   3) poll all existing cmds till finish or timeout
 *
 * then, we can safely remove all resources.
 */
static void xgq_stop_services(struct xocl_xgq *xgq)
{
	/* stop receiving incoming commands */
	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_halted = 1;
	mutex_unlock(&xgq->xgq_lock);
#if 0	
	/*TODO disable interrupts */
	if (!xgq->xgq_polling)
		xrt_cu_disable_intr(&xgq->xgq_cu, CU_INTR_DONE);

	/* disable intr */
	for (i = 0; i < xgq->xgq_intr_num; i++) {
		xocl_user_interrupt_config(xdev, xgq->xgq_intr_base + i, false);
		xocl_user_interrupt_reg(xdev, xgq->xgq_intr_base + i, NULL, NULL);
	}

	xrt_cu_hls_fini(&xgq->xgq_cu);
#endif

	/* wait for all commands to drain */
	while (xgq_submitted_cmds_empty(xgq) != true) {
		msleep(XOCL_XGQ_MSLEEP_1S);
		xgq_submitted_cmds_drain(xgq);
	}
}


/*
 * periodically check if there are outstanding timed out commands.
 * if there is any, stop service and drian all timeout cmds
 */
static int health_worker(void *data)
{
	struct xgq_worker *xw = (struct xgq_worker *)data;
	struct xocl_xgq *xgq = xw->xgq;

	while (!xw->stop) {
		msleep(XOCL_XGQ_MSLEEP_1S * 10);

		if (xgq_submitted_cmd_check(xgq)) {
			xgq_stop_services(xgq);
		}

		if (kthread_should_stop()) {
			xw->stop = true;
		}
	}

	return xw->error ? 1 : 0;
}

static int init_complete_worker(struct xgq_worker *xw)
{
	xw->complete_thread =
		kthread_run(complete_worker, (void *)xw, "complete worker");

	if (IS_ERR(xw->complete_thread)) {
		int ret = PTR_ERR(xw->complete_thread);
		return ret;
	}

	return 0;
}

static int init_health_worker(struct xgq_worker *xw)
{
	xw->complete_thread =
		kthread_run(health_worker, (void *)xw, "health worker");

	if (IS_ERR(xw->complete_thread)) {
		int ret = PTR_ERR(xw->complete_thread);
		return ret;
	}

	return 0;
}

static int fini_worker(struct xgq_worker *xw)
{
	int ret = 0;

	ret = kthread_stop(xw->complete_thread);

	return ret;
}

#if 0
/* TODO: enabe interrupt */
static irqreturn_t xgq_irq_handler(int irq, void *arg)
{
	struct xocl_xgq *xgq = (struct xocl_xgq *)arg;

	if (xgq && !xgq->xgq_polling) {
		/* clear intr for enabling next intr */
		//(void) xrt_cu_clear_intr(&xgq->xgq_cu);
		/* update complete cmd */
		complete(&xgq->xgq_irq_complete);
	} else if (xgq) {
		XGQ_INFO(xgq, "unhandled irq %d", irq);
	}

	return IRQ_HANDLED;
}
#endif

/*
 * submit new cmd into XGQ SQ(submition queue)
 */
static int submit_cmd(struct xocl_xgq *xgq, struct xocl_xgq_cmd *cmd)
{
	u64 slot_addr = 0;
	int rval = 0;
	int i;

	mutex_lock(&xgq->xgq_lock);
	if (xgq->xgq_halted) {
		XGQ_ERR(xgq, "xgq service is halted");
		rval = -EIO;
		goto done;
	}

	rval = xgq_produce(&xgq->xgq_queue, &slot_addr);
	if (rval) {
		XGQ_ERR(xgq, "error: xgq_produce failed: %d", rval);
		goto done;
	}

	/* write head */
	for (i = 0; i < sizeof(cmd->xgq_sq.xgq_sq_head) / 4; i++)
		xgq_reg_write32(0, slot_addr + i * 4, *((u32 *)cmd + i));

	/* write payload */
	memcpy_toio((void __iomem *)(slot_addr + sizeof(cmd->xgq_sq.xgq_sq_head)),
		&cmd->xgq_sq.xgq_sq_pkt, sizeof(cmd->xgq_sq.xgq_sq_pkt));

	xgq_notify_peer_produced(&xgq->xgq_queue);

	list_add_tail(&cmd->xgq_cmd_head, &xgq->xgq_submitted_cmds);

done:
	mutex_unlock(&xgq->xgq_lock);
	return rval;
}

static void xgq_complete_cb(void *arg, struct xrt_com_queue_entry *ccmd)
{
	struct xocl_xgq_cmd *xgq_cmd = (struct xocl_xgq_cmd *)arg;

	/* Note: we only care rcode for now */
	xgq_cmd->xgq_cmd_rcode = ccmd->rcode;

	complete(&xgq_cmd->xgq_cmd_complete);
}

/*
 * Write buffer into shared memory and 
 * return translate host based address to device based address.
 * The 0 ~ XOCL_XGQ_RING_LEN is reserved for ring buffer.
 * The XOCL_XGQ_DATA_OFFSET ~ end is for transferring shared data.
 */
static u64 memcpy_to_devices(struct xocl_xgq *xgq, const void *xclbin_data,
	size_t xclbin_len)
{
	void __iomem *dst = xgq->xgq_ring_base + XOCL_XGQ_DATA_OFFSET;

	memcpy_toio(dst, xclbin_data, xclbin_len);

	/* This is the offset that device start reading data */
	return XOCL_XGQ_DATA_OFFSET;
}

static inline int get_xgq_cid(struct xocl_xgq *xgq)
{
	int id;

	mutex_lock(&xgq->xgq_lock);
	id = idr_alloc_cyclic(&xocl_xgq_cid_idr, xgq, 0, 0, GFP_KERNEL);
	mutex_unlock(&xgq->xgq_lock);

	return id;
}

static inline void remove_xgq_cid(struct xocl_xgq *xgq, int id)
{
	mutex_lock(&xgq->xgq_lock);
	idr_remove(&xocl_xgq_cid_idr, id);
	mutex_unlock(&xgq->xgq_lock);
}

/*
 * Utilize shared memory between host and device to transfer data.
 */
static ssize_t xgq_transfer_data(struct xocl_xgq *xgq, const void *u_xclbin,
	u64 xclbin_len, xrt_xgq_pkt_type_t pkt_type, u32 timer)
{
	struct xocl_xgq_cmd *cmd = NULL;
	struct xrt_sub_queue_entry *cmdp = NULL;
	struct xgq_vmr_pkt *pkt;
	ssize_t ret = 0;
	int id;

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd)
		return ret;
	memset(cmd, 0, sizeof(*cmd));

	/*
	 * xgq mangment driver will leverage XRT_CMD_OP_CONFIGURE cmd
	 * and puts its own message inside the payload. Only RPU code
	 * understand the format and decode it based on VERSION and TYPE.
	 */
	pkt = &(cmd->xgq_sq.xgq_sq_pkt);
	pkt->head.version = 0;
	pkt->head.type = pkt_type;
	pkt->payload_xclbin.address = memcpy_to_devices(xgq, u_xclbin, xclbin_len);
	pkt->payload_xclbin.size = xclbin_len;

	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq = xgq;

	cmdp = &(cmd->xgq_sq.xgq_sq_head);
	cmdp->opcode = XRT_CMD_OP_CONFIGURE;
	cmdp->state = 1;
	cmdp->count = sizeof(struct xgq_vmr_pkt);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto done;
	}
	cmdp->cid = id;

	/* init condition veriable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + timer;

	if (submit_cmd(xgq, cmd)) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d", cmdp->cid);
		goto done;
	}

	/* wait for command completion */
	wait_for_completion_interruptible(&cmd->xgq_cmd_complete);

	XGQ_INFO(xgq, "rcode %d", cmd->xgq_cmd_rcode);

	if (cmd->xgq_cmd_rcode)
		ret = cmd->xgq_cmd_rcode;
	else
		ret = xclbin_len;
done:
	if (cmd) {
		remove_xgq_cid(xgq, id);
		kfree(cmd);
	}

	return ret;
}

static int xgq_load_xclbin(struct platform_device *pdev,
	const void *u_xclbin)
{
	struct xocl_xgq *xgq = platform_get_drvdata(pdev);
	struct axlf *xclbin = (struct axlf *)u_xclbin;
	u64 xclbin_len = xclbin->m_header.m_length;
	int ret = 0;
	
	mutex_lock(&xgq->xgq_lock);
	if (xgq->xgq_ospi_inuse) {
		mutex_unlock(&xgq->xgq_lock);
		XGQ_ERR(xgq, "XGQ OSPI device is busy");
		return -EBUSY;
	}
	xgq->xgq_ospi_inuse = true;
	mutex_unlock(&xgq->xgq_lock);
	
	ret = xgq_transfer_data(xgq, u_xclbin, xclbin_len,
		XRT_XGQ_PKT_TYPE_XCLBIN, XOCL_XGQ_DOWNLOAD_TIME);

	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_ospi_inuse = false;
	mutex_unlock(&xgq->xgq_lock);

	return ret;
}

#if 0
/* A-sync ops is not supported in XGQ now */
static void xgq_af_complete_cb(void *arg, struct xrt_com_queue_entry *ccmd)
{
	struct xocl_xgq_cmd *xgq_cmd = (struct xocl_xgq_cmd *)arg;

	/* Note: we only care rcode for now */
	xgq_cmd->xgq_cmd_rcode = ccmd->rcode;

	/* the func suppose to finish quick and call complete itself */
	if (xgq_cmd->xgq_af_handle && xgq_cmd->xgq_af_handle->func)
		xgq_cmd->xgq_af_handle->func(
			xgq_cmd->xgq_af_handle->arg, ccmd->rcode);

	kfree(xgq_cmd);
}
#endif

static int xgq_check_firewall(struct platform_device *pdev)
{
	struct xocl_xgq *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_cmd *cmd = NULL;
	struct xrt_sub_queue_entry *cmdp = NULL;
	struct xgq_vmr_pkt *pkt;
	int ret = 0;
	int id;

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);	
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return 0;
	}
	memset(cmd, 0, sizeof(*cmd));

	pkt = &(cmd->xgq_sq.xgq_sq_pkt);
	pkt->head.version = 0;
	pkt->head.type = XRT_XGQ_PKT_TYPE_AF;

	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq = xgq;

	cmdp = &(cmd->xgq_sq.xgq_sq_head);
	cmdp->opcode = XRT_CMD_OP_CONFIGURE;
	cmdp->state = 1;
	cmdp->count = sizeof(struct xgq_vmr_pkt);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto done;
	}
	cmdp->cid = id;

	/* init condition veriable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + XOCL_XGQ_CONFIG_TIME;

	XGQ_INFO(xgq, "submit cid: %d", cmdp->cid);
	ret = submit_cmd(xgq, cmd);
	if (ret) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d", cmdp->cid);
		/* return 0, because it is not a firewall trip */
		ret = 0;
		goto done;
	}

	XGQ_INFO(xgq, "before wait cid: %d", cmdp->cid);
	/* wait for command completion */
	wait_for_completion_interruptible(&cmd->xgq_cmd_complete);
	XGQ_INFO(xgq, "after wait cid: %d", cmdp->cid);

	XGQ_INFO(xgq, "rcode: %d", cmd->xgq_cmd_rcode);

	ret = cmd->xgq_cmd_rcode == -ETIME ? 0 : cmd->xgq_cmd_rcode;
done:
	if (cmd) {
		remove_xgq_cid(xgq, id);
		kfree(cmd);
	}

	return ret;
}

/* sysfs */
static ssize_t polling_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_polling = val ? 1 : 0;
	mutex_unlock(&xgq->xgq_lock);

	return count;
}

static ssize_t polling_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	mutex_lock(&xgq->xgq_lock);
	cnt += sprintf(buf + cnt, "%d\n", xgq->xgq_polling);
	mutex_unlock(&xgq->xgq_lock);

	return cnt;
}
static DEVICE_ATTR(polling, 0644, polling_show, polling_store);

static struct attribute *xgq_attrs[] = {
	&dev_attr_polling.attr,
	NULL,
};

static struct attribute_group xgq_attr_group = {
	.attrs = xgq_attrs,
};

static ssize_t xgq_ospi_write(struct file *filp, const char __user *udata,
	size_t data_len, loff_t *off)
{
	struct xocl_xgq *xgq = filp->private_data;
	ssize_t ret;
	char *kdata = NULL;

	if (*off != 0) {
		XGQ_ERR(xgq, "OSPI offset non-zero is not supported");
		return -EINVAL;
	}

	mutex_lock(&xgq->xgq_lock);
	if (xgq->xgq_ospi_inuse) {
		mutex_unlock(&xgq->xgq_lock);
		XGQ_ERR(xgq, "OSPI device is busy");
		return -EBUSY;
	}
	xgq->xgq_ospi_inuse = true;
	mutex_unlock(&xgq->xgq_lock);

	kdata = vmalloc(data_len);
	if (!kdata) {
		XGQ_ERR(xgq, "Cannot create xgq transfer buffer");
		ret = -ENOMEM;
		goto done;
	}

	ret = copy_from_user(kdata, udata, data_len);
	if (ret) {
		XGQ_ERR(xgq, "copy data failed %ld", ret);
		goto done;
	}
	
	ret = xgq_transfer_data(xgq, kdata, data_len,
		XRT_XGQ_PKT_TYPE_PDI, XOCL_XGQ_FLASH_TIME);

done:
	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_ospi_inuse = false;
	mutex_unlock(&xgq->xgq_lock);
	vfree(kdata);

	return ret;
}

static int xgq_ospi_open(struct inode *inode, struct file *file)
{
	struct xocl_xgq *xgq = NULL;

	xgq = xocl_drvinst_open(inode->i_cdev);
	if (!xgq)
		return -ENXIO;

	file->private_data = xgq;
	return 0;
}

static int xgq_ospi_close(struct inode *inode, struct file *file)
{
	struct xocl_xgq *xgq = file->private_data;

	xocl_drvinst_close(xgq);
	return 0;
}

static int xgq_remove(struct platform_device *pdev)
{
	struct xocl_xgq	*xgq;
	void *hdl;

	xgq = platform_get_drvdata(pdev);
	if (!xgq) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	XGQ_INFO(xgq, "->");

	xgq_stop_services(xgq);

	fini_worker(&xgq->xgq_complete_worker);
	fini_worker(&xgq->xgq_health_worker);

	if (xgq->xgq_ring_base)
		iounmap(xgq->xgq_ring_base);
	if (xgq->xgq_sq_base)
		iounmap(xgq->xgq_sq_base);
	if (xgq->xgq_cq_base)
		iounmap(xgq->xgq_cq_base);

	sysfs_remove_group(&pdev->dev.kobj, &xgq_attr_group);
	mutex_destroy(&xgq->xgq_lock);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_release(xgq, &hdl);
	xocl_drvinst_free(hdl);

	XGQ_INFO(xgq, "successfully removed xgq subdev");
	return 0;
}

static inline bool xgq_device_is_ready(struct xocl_xgq *xgq)
{
	int rval;

	rval = ioread32(xgq->xgq_ring_base + XOCL_XGQ_DEV_STAT_OFFSET);
	
	return rval > 0;
}

static int xgq_probe(struct platform_device *pdev)
{
	struct xocl_xgq *xgq = NULL;
	struct resource *res;
	u64 flags = 0;
	int ret = 0, i = 0;

	xgq = xocl_drvinst_alloc(&pdev->dev, sizeof (*xgq));
	if (!xgq)
		return -ENOMEM;
	platform_set_drvdata(pdev, xgq);
	xgq->xgq_pdev = pdev;
	xgq->xgq_cmd_id = 0;

	mutex_init(&xgq->xgq_lock);

	for (res = platform_get_resource(pdev, IORESOURCE_MEM, i); res;
	    res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {
		XGQ_INFO(xgq, "res : %s %pR", res->name, res);
		if (!strncmp(res->name, NODE_XGQ_SQ_BASE, strlen(NODE_XGQ_SQ_BASE))) {
			xgq->xgq_sq_base = ioremap_nocache(res->start,
				res->end - res->start + 1);
		}
		if (!strncmp(res->name, NODE_XGQ_CQ_BASE, strlen(NODE_XGQ_CQ_BASE))) {
			xgq->xgq_cq_base = ioremap_nocache(res->start,
				res->end - res->start + 1);
		}
		if (!strncmp(res->name, NODE_XGQ_RING_BASE, strlen(NODE_XGQ_RING_BASE))) {
			xgq->xgq_ring_base = ioremap_nocache(res->start,
				res->end - res->start + 1);
		}
	}

	if (!xgq->xgq_sq_base || !xgq->xgq_cq_base || !xgq->xgq_ring_base) {
		ret = -EIO;
		XGQ_ERR(xgq, "platform get resource failed");
		goto attach_failed;
	}

	/* check device is ready */
	if (xgq_device_is_ready(xgq)) {
		ret = -ENODEV;
		XGQ_ERR(xgq, "device is not ready, please reset device.");
		goto attach_failed;
	}

	/* server to reset SQ tail pointer status */
	//iowrite32(0x0, xgq->xgq_sq_base);
	
	flags &= ~XGQ_SERVER;
	ret = xgq_attach(&xgq->xgq_queue, flags, (u64)xgq->xgq_ring_base,
		(u64)xgq->xgq_sq_base, (u64)xgq->xgq_cq_base);
	if (ret != 0) {
		XGQ_ERR(xgq, "xgq_attache failed: %d, please reset device", ret);
		ret = -ENODEV;
		goto attach_failed;
	}

	XGQ_DBG(xgq, "sq_slot_size 0x%lx\n", xgq->xgq_queue.xq_sq.xr_slot_sz);
	XGQ_DBG(xgq, "cq_slot_size 0x%lx\n", xgq->xgq_queue.xq_cq.xr_slot_sz);
	XGQ_DBG(xgq, "sq_num_slots %d\n", xgq->xgq_queue.xq_sq.xr_slot_num);
	XGQ_DBG(xgq, "cq_num_slots %d\n", xgq->xgq_queue.xq_cq.xr_slot_num);
	XGQ_DBG(xgq, "SQ 0x%lx off: 0x%llx\n", xgq->xgq_queue.xq_sq.xr_slot_addr);
	XGQ_DBG(xgq, "CQ 0x%lx off: 0x%llx\n", xgq->xgq_queue.xq_cq.xr_slot_addr);
	XGQ_DBG(xgq, "SQ xr_produced_addr 0x%lx off: 0x%llx\n",
		xgq->xgq_queue.xq_sq.xr_produced_addr,
		xgq->xgq_queue.xq_sq.xr_produced_addr - (u64)xgq->xgq_ring_base);
	XGQ_DBG(xgq, "SQ xr_consumed_addr 0x%lx off: 0x%llx\n",
		xgq->xgq_queue.xq_sq.xr_consumed_addr,
		xgq->xgq_queue.xq_sq.xr_consumed_addr - (u64)xgq->xgq_ring_base);
	XGQ_DBG(xgq, "CQ xr_produced_addr 0x%lx off: 0x%llx\n",
		xgq->xgq_queue.xq_cq.xr_produced_addr,
		xgq->xgq_queue.xq_cq.xr_produced_addr - (u64)xgq->xgq_ring_base);
	XGQ_DBG(xgq, "CQ xr_consumed_addr 0x%lx off: 0x%llx\n",
		xgq->xgq_queue.xq_cq.xr_consumed_addr,
		xgq->xgq_queue.xq_cq.xr_consumed_addr - (u64)xgq->xgq_ring_base);

	/* init condition veriable */
	init_completion(&xgq->xgq_irq_complete);

	xgq->xgq_polling = 1;

	INIT_LIST_HEAD(&xgq->xgq_submitted_cmds);

	xgq->xgq_complete_worker.xgq = xgq;
	xgq->xgq_health_worker.xgq = xgq;
	init_complete_worker(&xgq->xgq_complete_worker);
	init_health_worker(&xgq->xgq_health_worker);
#if 0
	/*TODO: enable interrupts */

	/* init interrupt vector number based on iores of kdma */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res) {
		xgq->xgq_intr_base = res->start;
		xgq->xgq_intr_num = res->end - res->start + 1;
		xgq->xgq_polling = 0;
	}

	for (i = 0; i < xgq->xgq_intr_num; i++) {
		xocl_user_interrupt_reg(xdev, xgq->xgq_intr_base + i, xgq_irq_handler, xgq);
		xocl_user_interrupt_config(xdev, xgq->xgq_intr_base + i, true);
	}

	if (xgq->xgq_polling)
		xrt_cu_disable_intr(&xgq->xgq_cu, CU_INTR_DONE);
	else
		xrt_cu_enable_intr(&xgq->xgq_cu, CU_INTR_DONE);
#endif
	ret = sysfs_create_group(&pdev->dev.kobj, &xgq_attr_group);
	if (ret) {
		XGQ_ERR(xgq, "create xgq attrs failed: %d", ret);
		/* Gracefully remove xgq resources */
		(void) xgq_remove(pdev);
		return ret;
	}

	XGQ_INFO(xgq, "Initialized xgq subdev, polling (%d)", xgq->xgq_polling);
	return ret;

attach_failed:
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static struct xocl_xgq_funcs xgq_ops = {
	.xgq_load_xclbin = xgq_load_xclbin,
	.xgq_check_firewall = xgq_check_firewall,
};

static const struct file_operations xgq_fops = {
	.owner = THIS_MODULE,
	.open = xgq_ospi_open,
	.release = xgq_ospi_close,
	.write = xgq_ospi_write,
};

struct xocl_drv_private xgq_priv = {
	.ops = &xgq_ops,
	.fops = &xgq_fops,
	.dev = -1,
};

struct platform_device_id xgq_id_table[] = {
	{ XOCL_DEVNAME(XOCL_XGQ), (kernel_ulong_t)&xgq_priv },
	{ },
};

static struct platform_driver	xgq_driver = {
	.probe		= xgq_probe,
	.remove		= xgq_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_XGQ),
	},
	.id_table = xgq_id_table,
};

int __init xocl_init_xgq(void)
{
	int err = 0;

	err = alloc_chrdev_region(&xgq_priv.dev, 0, XOCL_MAX_DEVICES,
	    XGQ_DEV_NAME);
	if (err < 0)
		return err;

	err = platform_driver_register(&xgq_driver);
	if (err) {
		unregister_chrdev_region(xgq_priv.dev, XOCL_MAX_DEVICES);
		return err;
	}

	return 0;
}

void xocl_fini_xgq(void)
{
	unregister_chrdev_region(xgq_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&xgq_driver);
}
