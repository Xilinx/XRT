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
#include "xgq_cmd_vmr.h"
#include "../xgq_xocl_plat.h"
#include <linux/time.h>

#define	CLK_TYPE_DATA	0
#define	CLK_TYPE_KERNEL	1
#define	CLK_TYPE_SYSTEM	2
#define	CLK_TYPE_MAX	4

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
 *         | <----------------- xgq_notify_peer_consumed |
 *         |                        [ ...              ] |
 *         |                        [ cmd operations   ] |
 *         |                        [ ...              ] |
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

#define XGQ_SQ_TAIL_POINTER     0x0
#define XGQ_SQ_INTR_REG         0x4
#define XGQ_SQ_INTR_CTRL        0xC
#define XGQ_CQ_TAIL_POINTER     0x100
#define XGQ_CQ_INTR_REG         0x104
#define XGQ_CQ_INTR_CTRL        0x10C

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
#define XOCL_XGQ_FLASH_TIME	msecs_to_jiffies(600 * 1000) 
#define XOCL_XGQ_DOWNLOAD_TIME	msecs_to_jiffies(300 * 1000) 
#define XOCL_XGQ_CONFIG_TIME	msecs_to_jiffies(30 * 1000) 
#define XOCL_XGQ_MSLEEP_1S	(1000)      //1 s

typedef void (*xocl_xgq_complete_cb)(void *arg, struct xgq_com_queue_entry *ccmd);

struct xocl_xgq_cmd {
	struct xgq_cmd_sq	xgq_cmd_entry;
	struct list_head	xgq_cmd_list;
	struct completion	xgq_cmd_complete;
	xocl_xgq_complete_cb    xgq_cmd_cb;
	void			*xgq_cmd_arg;
	struct timer_list	xgq_cmd_timer;
	struct xocl_xgq		*xgq;
	u64			xgq_cmd_timeout_jiffies; /* timout till */
	uint32_t		xgq_cmd_rcode;
	/* xgq complete command can return in-line data via payload */
	struct xgq_cmd_cq_vmr_payload	xgq_cmd_cq_payload;
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
	bool 			xgq_polling;
	bool 			xgq_boot_from_backup;
	u32			xgq_intr_base;
	u32			xgq_intr_num;
	struct list_head	xgq_submitted_cmds;
	struct completion 	xgq_irq_complete;
	struct xgq_worker	xgq_complete_worker;
	struct xgq_worker	xgq_health_worker;
	bool			xgq_halted;
	int 			xgq_cmd_id;
	void			*sensor_data;
	u32			sensor_data_length;
	struct semaphore 	xgq_data_sema;
	/* preserve fpt info for sysfs to display */
	struct xgq_cmd_cq_multiboot_payload xgq_mb_payload;
};

/*
 * when detect cmd is completed, find xgq_cmd from submitted_cmds list
 * and find cmd by cid; perform callback and remove from submitted_cmds.
 */
static void cmd_complete(struct xocl_xgq *xgq, struct xgq_com_queue_entry *ccmd)
{
	struct xocl_xgq_cmd *xgq_cmd = NULL;
	struct list_head *pos = NULL, *next = NULL;

	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_cmd, xgq_cmd_list);

		if (xgq_cmd->xgq_cmd_entry.hdr.cid == ccmd->hdr.cid) {

			list_del(pos);
			if (xgq_cmd->xgq_cmd_cb)
				xgq_cmd->xgq_cmd_cb(xgq_cmd->xgq_cmd_arg, ccmd);
			return;
		}

	}

	XGQ_WARN(xgq, "unknown cid %d received", ccmd->hdr.cid);
	return;
}

/*
 * Read completed cmd based on XGQ protocol.
 */
void read_completion(struct xgq_com_queue_entry *ccmd, u64 addr)
{
	u32 i = 0;
	u32 *buffer = (u32 *)ccmd;

	for (i = 0; i < XGQ_COM_Q1_SLOT_SIZE / sizeof(u32); i++)
		buffer[i] = xgq_reg_read32(0, addr + i * sizeof(u32));

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
		
		while (!list_empty(&xgq->xgq_submitted_cmds)) {
			u64 slot_addr = 0;
			struct xgq_com_queue_entry ccmd;

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
	struct xocl_xgq_cmd *xgq_cmd = NULL;
	struct list_head *pos = NULL, *next = NULL;
	bool found_timeout = false;

	mutex_lock(&xgq->xgq_lock);
	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_cmd, xgq_cmd_list);

		/* Finding timed out cmds */
		if (xgq_cmd->xgq_cmd_timeout_jiffies < jiffies) {
			XGQ_ERR(xgq, "cmd id: %d op: 0x%x timed out, hot reset is required!",
				xgq_cmd->xgq_cmd_entry.hdr.cid,
				xgq_cmd->xgq_cmd_entry.hdr.opcode);
			found_timeout = true;
			break;
		}
	}
	mutex_unlock(&xgq->xgq_lock);

	return found_timeout;
}

static void xgq_submitted_cmds_drain(struct xocl_xgq *xgq)
{
	struct xocl_xgq_cmd *xgq_cmd = NULL;
	struct list_head *pos = NULL, *next = NULL;

	mutex_lock(&xgq->xgq_lock);
	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_cmd, xgq_cmd_list);

		/* Finding timed out cmds */
		if (xgq_cmd->xgq_cmd_timeout_jiffies < jiffies) {
			list_del(pos);
			
			xgq_cmd->xgq_cmd_rcode = -ETIME;
			complete(&xgq_cmd->xgq_cmd_complete);
			XGQ_ERR(xgq, "cmd id: %d timed out, hot reset is required!",
				xgq_cmd->xgq_cmd_entry.hdr.cid);
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
	xgq->xgq_halted = true;
	mutex_unlock(&xgq->xgq_lock);

#if 0	
	/*TODO: disable interrupts */
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

	/* write xgq cmd to SQ slot */
	memcpy_toio((void __iomem *)slot_addr, &cmd->xgq_cmd_entry,
		sizeof(cmd->xgq_cmd_entry));

	xgq_notify_peer_produced(&xgq->xgq_queue);

	list_add_tail(&cmd->xgq_cmd_list, &xgq->xgq_submitted_cmds);

done:
	mutex_unlock(&xgq->xgq_lock);
	return rval;
}

static void xgq_complete_cb(void *arg, struct xgq_com_queue_entry *ccmd)
{
	struct xocl_xgq_cmd *xgq_cmd = (struct xocl_xgq_cmd *)arg;
	struct xgq_cmd_cq *cmd_cq = (struct xgq_cmd_cq *)ccmd;

	xgq_cmd->xgq_cmd_rcode = ccmd->rcode;
	/* preserve payload prior to free xgq_cmd_cq */
	memcpy(&xgq_cmd->xgq_cmd_cq_payload, &cmd_cq->default_payload,
		sizeof(cmd_cq->default_payload));

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

static void memcpy_from_devices(struct xocl_xgq *xgq, void *dst,
	size_t count)
{
	void __iomem *src = xgq->xgq_ring_base + XOCL_XGQ_DATA_OFFSET;
	memcpy_fromio(dst, src, count);
}

static inline int get_xgq_cid(struct xocl_xgq *xgq)
{
	int id = 0;

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
static ssize_t xgq_transfer_data(struct xocl_xgq *xgq, const void *buf,
	u64 len, enum xgq_cmd_opcode opcode, u32 timer)
{
	struct xocl_xgq_cmd *cmd = NULL;
	struct xgq_cmd_data_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	ssize_t ret = 0;
	int id = 0;

	if (opcode != XGQ_CMD_OP_LOAD_XCLBIN && 
	    opcode != XGQ_CMD_OP_DOWNLOAD_PDI &&
	    opcode != XGQ_CMD_OP_LOAD_APUBIN) {
		XGQ_WARN(xgq, "unsupported opcode %d", opcode);
		return -EINVAL;
	}

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_WARN(xgq, "no enough memory");
		return -ENOMEM;
	}

	/* set up xgq_cmd */
	memset(cmd, 0, sizeof(*cmd));
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq = xgq;

	/* set up payload */
	payload = (opcode == XGQ_CMD_OP_LOAD_XCLBIN) ?
		&(cmd->xgq_cmd_entry.pdi_payload) :
		&(cmd->xgq_cmd_entry.xclbin_payload);

	payload->address = memcpy_to_devices(xgq, buf, len);
	payload->size = len;
	payload->addr_type = XGQ_CMD_ADD_TYPE_AP_OFFSET;

	/* set up hdr */
	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = opcode;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		ret = -ENOMEM;
		goto done;
	}
	hdr->cid = id;

	/* init condition veriable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + timer;

	if (submit_cmd(xgq, cmd)) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d", id);
		goto done;
	}

	/* wait for command completion */
	wait_for_completion_interruptible(&cmd->xgq_cmd_complete);

	/* If return is 0, we set length as return value */
	if (cmd->xgq_cmd_rcode) {
		XGQ_ERR(xgq, "ret %d", cmd->xgq_cmd_rcode);
		ret = cmd->xgq_cmd_rcode;
	} else {
		ret = len;
	}
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
	
	if (down_interruptible(&xgq->xgq_data_sema)) {
		XGQ_ERR(xgq, "XGQ data transfer is interrupted");
		return -EIO;
	}

	ret = xgq_transfer_data(xgq, u_xclbin, xclbin_len,
		XGQ_CMD_OP_LOAD_XCLBIN, XOCL_XGQ_DOWNLOAD_TIME);

	up(&xgq->xgq_data_sema);

	return ret == xclbin_len ? 0 : -EIO;
}

static int xgq_check_firewall(struct platform_device *pdev)
{
	struct xocl_xgq *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_cmd *cmd = NULL;
	struct xgq_cmd_log_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;
	int id = 0;

	/* skip periodic firewall check when xgq service is halted */
	if (xgq->xgq_halted)
		return 0;

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);	
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return 0;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq = xgq;

	payload = &(cmd->xgq_cmd_entry.log_payload);
	/*TODO: payload is to be filed for retriving log back */

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_GET_LOG_PAGE;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto done;
	}
	hdr->cid = id;

	/* init condition veriable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + XOCL_XGQ_CONFIG_TIME;

	ret = submit_cmd(xgq, cmd);
	if (ret) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d", id);
		/* return 0, because it is not a firewall trip */
		ret = 0;
		goto done;
	}

	/* wait for command completion */
	wait_for_completion_interruptible(&cmd->xgq_cmd_complete);

	ret = cmd->xgq_cmd_rcode == -ETIME ? 0 : cmd->xgq_cmd_rcode;
done:
	if (cmd) {
		remove_xgq_cid(xgq, id);
		kfree(cmd);
	}
	return ret;
}

/* On versal, verify is enforced. */
static int xgq_freq_scaling(struct platform_device *pdev,
	unsigned short *freqs, int num_freqs, int verify)
{
	struct xocl_xgq *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_cmd *cmd = NULL;
	struct xgq_cmd_clock_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;
	int id = 0;
	int i = 0;

	if (num_freqs <= 0 || num_freqs > XGQ_CLOCK_WIZ_MAX_RES) {
		XGQ_ERR(xgq, "num_freqs %d is out of range", num_freqs);
		return -EINVAL;
	}

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return -ENOMEM;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq = xgq;

	payload = &(cmd->xgq_cmd_entry.clock_payload);
	payload->ocl_region = 0;
	payload->ocl_req_type = XGQ_CMD_CLOCK_SCALE;
	payload->ocl_req_num = num_freqs;
	for (i = 0; i < num_freqs; i++)
		payload->ocl_req_freq[i] = freqs[i];

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_CLOCK;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto done;
	}
	hdr->cid = id;

	/* init condition veriable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + XOCL_XGQ_CONFIG_TIME;

	ret = submit_cmd(xgq, cmd);
	if (ret) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d", id);
		goto done;
	}

	/* wait for command completion */
	wait_for_completion_interruptible(&cmd->xgq_cmd_complete);

	ret = cmd->xgq_cmd_rcode;
	if (ret) {
		XGQ_ERR(xgq, "ret %d", cmd->xgq_cmd_rcode);
	} 

done:
	if (cmd) {
		remove_xgq_cid(xgq, id);
		kfree(cmd);
	}

	return ret;
}

static int xgq_freq_scaling_by_topo(struct platform_device *pdev,
	struct clock_freq_topology *topo, int verify)
{
	struct xocl_xgq *xgq = platform_get_drvdata(pdev);
	struct clock_freq *freq = NULL;
	int data_clk_count = 0;
	int kernel_clk_count = 0;
	int system_clk_count = 0;
	int clock_type_count = 0;
	unsigned short target_freqs[4] = {0};
	int i = 0;

	if (!topo)
		return -EINVAL;

	if (topo->m_count > CLK_TYPE_MAX) {
		XGQ_ERR(xgq, "More than 4 clocks found in clock topology");
		return -EDOM;
	}

	/* Error checks - we support 1 data clk (reqd), 1 kernel clock(reqd) and
	 * at most 2 system clocks (optional/reqd for aws).
	 * Data clk needs to be the first entry, followed by kernel clock
	 * and then system clocks
	 */
	for (i = 0; i < topo->m_count; i++) {
		freq = &(topo->m_clock_freq[i]);
		if (freq->m_type == CT_DATA)
			data_clk_count++;
		if (freq->m_type == CT_KERNEL)
			kernel_clk_count++;
		if (freq->m_type == CT_SYSTEM)
			system_clk_count++;
	}
	if (data_clk_count != 1) {
		XGQ_ERR(xgq, "Data clock not found in clock topology");
		return -EDOM;
	}
	if (kernel_clk_count != 1) {
		XGQ_ERR(xgq, "Kernel clock not found in clock topology");
		return -EDOM;
	}
	if (system_clk_count > 2) {
		XGQ_ERR(xgq, "More than 2 system clocks found in clock topology");
		return -EDOM;
	}

	for (i = 0; i < topo->m_count; i++) {
		freq = &(topo->m_clock_freq[i]);
		if (freq->m_type == CT_DATA)
			target_freqs[CLK_TYPE_DATA] = freq->m_freq_Mhz;
	}

	for (i = 0; i < topo->m_count; i++) {
		freq = &(topo->m_clock_freq[i]);
		if (freq->m_type == CT_KERNEL)
			target_freqs[CLK_TYPE_KERNEL] = freq->m_freq_Mhz;
	}

	clock_type_count = CLK_TYPE_SYSTEM;
	for (i = 0; i < topo->m_count; i++) {
		freq = &(topo->m_clock_freq[i]);
		if (freq->m_type == CT_SYSTEM)
			target_freqs[clock_type_count++] = freq->m_freq_Mhz;
	}

	XGQ_INFO(xgq, "set %lu freq, data: %d, kernel: %d, sys: %d, sys1: %d",
	    ARRAY_SIZE(target_freqs), target_freqs[0], target_freqs[1],
	    target_freqs[2], target_freqs[3]);

	return xgq_freq_scaling(pdev, target_freqs, ARRAY_SIZE(target_freqs),
		verify);
}

static uint32_t xgq_clock_get_data(struct xocl_xgq *xgq,
	enum xgq_cmd_clock_req_type req_type, int req_id)
{
	struct xocl_xgq_cmd *cmd = NULL;
	struct xgq_cmd_clock_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int id = 0;
	uint32_t ret = 0;

	if (req_id > XGQ_CLOCK_WIZ_MAX_RES) {
		XGQ_ERR(xgq, "req_id %d is out of range", id);
		return 0;
	}

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return 0;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq = xgq;

	payload = &(cmd->xgq_cmd_entry.clock_payload);
	payload->ocl_region = 0;
	payload->ocl_req_type = req_type;
	payload->ocl_req_id = req_id;


	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_CLOCK;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto done;
	}
	hdr->cid = id;

	/* init condition veriable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + XOCL_XGQ_CONFIG_TIME;

	ret = submit_cmd(xgq, cmd);
	if (ret) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d", id);
		ret = 0;
		goto done;
	}

	/* wait for command completion */
	wait_for_completion_interruptible(&cmd->xgq_cmd_complete);

	ret = cmd->xgq_cmd_rcode;
	if (ret) {
		XGQ_ERR(xgq, "ret %d", cmd->xgq_cmd_rcode);
		ret = 0;
	} else {
		/* freq result is in rdata */
		ret = ((struct xgq_cmd_cq_clock_payload *)&cmd->xgq_cmd_cq_payload)->ocl_freq;
	}

done:
	if (cmd) {
		remove_xgq_cid(xgq, id);
		kfree(cmd);
	}

	return ret;
}

static uint64_t xgq_get_data(struct platform_device *pdev,
	enum data_kind kind)
{
	struct xocl_xgq *xgq = platform_get_drvdata(pdev);
	uint64_t target = 0;

	switch (kind) {
	case CLOCK_FREQ_0:
		target = xgq_clock_get_data(xgq, XGQ_CMD_CLOCK_WIZARD, 0);
		break;
	case CLOCK_FREQ_1:
		target = xgq_clock_get_data(xgq, XGQ_CMD_CLOCK_WIZARD, 1);
		break;
	case CLOCK_FREQ_2:
		target = xgq_clock_get_data(xgq, XGQ_CMD_CLOCK_WIZARD, 2);
		break;
	case FREQ_COUNTER_0:
		target = xgq_clock_get_data(xgq, XGQ_CMD_CLOCK_COUNTER, 0);
		break;
	case FREQ_COUNTER_1:
		target = xgq_clock_get_data(xgq, XGQ_CMD_CLOCK_COUNTER, 1);
		break;
	case FREQ_COUNTER_2:
		target = xgq_clock_get_data(xgq, XGQ_CMD_CLOCK_COUNTER, 2);
		break;
	default:
		break;
	}

	return target;
}

static int xgq_download_apu_bin(struct platform_device *pdev, char *buf,
	size_t len)
{
	struct xocl_xgq *xgq = platform_get_drvdata(pdev);
	int ret = 0;


	if (down_interruptible(&xgq->xgq_data_sema)) {
		XGQ_ERR(xgq, "XGQ data transfer is interrupted");
		return -EIO;
	}

	ret = xgq_transfer_data(xgq, buf, len, XGQ_CMD_OP_LOAD_APUBIN,
		XOCL_XGQ_DOWNLOAD_TIME);

	up(&xgq->xgq_data_sema);

	XGQ_DBG(xgq, "ret %d", ret);
	return ret == len ? 0 : -EIO;
}

/* read firmware from /lib/firmware/xilinx, load via xgq */
static int xgq_download_apu_firmware(struct platform_device *pdev)
{
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(pdev);
	char *apu_bin = "xilinx/xrt-versal-apu.xsabin";
	char *apu_bin_buf = NULL;
	size_t apu_bin_len = 0;
	int ret = 0;

	ret = xocl_request_firmware(&pcidev->dev, apu_bin,
			&apu_bin_buf, &apu_bin_len);
	if (ret)
		return ret;
	ret = xgq_download_apu_bin(pdev, apu_bin_buf, apu_bin_len);
	vfree(apu_bin_buf);

	return ret;
}

static void vmr_collect_boot_query(struct xocl_xgq *xgq, struct xocl_xgq_cmd *cmd)
{
	struct xgq_cmd_cq_multiboot_payload *payload =
		(struct xgq_cmd_cq_multiboot_payload *)&cmd->xgq_cmd_cq_payload;

	memcpy(&xgq->xgq_mb_payload, payload, sizeof(*payload));
}

static int vmr_multiboot_op(struct platform_device *pdev,
	enum xgq_cmd_multiboot_req_type req_type)
{
	struct xocl_xgq *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_cmd *cmd = NULL;
	struct xgq_cmd_multiboot_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;
	int id = 0;

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return -ENOMEM;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq = xgq;

	payload = &(cmd->xgq_cmd_entry.multiboot_payload);
	payload->req_type = req_type;

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_MULTIPLE_BOOT;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto done;
	}
	hdr->cid = id;

	/* init condition veriable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + XOCL_XGQ_CONFIG_TIME;

	ret = submit_cmd(xgq, cmd);
	if (ret) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d", id);
		goto done;
	}

	/* wait for command completion */
	wait_for_completion_interruptible(&cmd->xgq_cmd_complete);

	ret = cmd->xgq_cmd_rcode;

	if (ret) {
		XGQ_ERR(xgq, "Multiboot or reset might not work. ret %d",
			cmd->xgq_cmd_rcode);
	} else if (req_type == XGQ_CMD_BOOT_QUERY) {
		vmr_collect_boot_query(xgq, cmd);
	}

done:
	if (cmd) {
		remove_xgq_cid(xgq, id);
		kfree(cmd);
	}

	return ret;
}

static int vmr_fpt_query(struct platform_device *pdev)
{
	return vmr_multiboot_op(pdev, XGQ_CMD_BOOT_QUERY);
}

static int vmr_enable_multiboot(struct platform_device *pdev)
{
	struct xocl_xgq *xgq = platform_get_drvdata(pdev);

	return vmr_multiboot_op(pdev,
		xgq->xgq_boot_from_backup ?  XGQ_CMD_BOOT_BACKUP : XGQ_CMD_BOOT_DEFAULT);
}

static int xgq_collect_sensor_data(struct xocl_xgq *xgq)
{
	struct xocl_xgq_cmd *cmd = NULL;
	struct xgq_cmd_log_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;
	int id = 0;

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return -ENOMEM;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq = xgq;

	/* reset to all 0 first */
	memset(xgq->sensor_data, 0, xgq->sensor_data_length);
	payload = &(cmd->xgq_cmd_entry.sensor_payload);
	/* set address offset, so that device will write data start from this offset */
	payload->address = memcpy_to_devices(xgq,
		xgq->sensor_data, xgq->sensor_data_length);
	payload->size = xgq->sensor_data_length;
	payload->pid = XGQ_CMD_SENSOR_PID_BDINFO;

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_SENSOR;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto done;
	}
	hdr->cid = id;

	/* init condition veriable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + XOCL_XGQ_CONFIG_TIME;

	ret = submit_cmd(xgq, cmd);
	if (ret) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d", id);
		goto done;
	}

	/* wait for command completion */
	wait_for_completion_interruptible(&cmd->xgq_cmd_complete);

	ret = cmd->xgq_cmd_rcode;

	if (ret) {
		XGQ_ERR(xgq, "ret %d", cmd->xgq_cmd_rcode);
	} else {
		memcpy_from_devices(xgq, xgq->sensor_data,
			xgq->sensor_data_length);
	}

done:
	if (cmd) {
		remove_xgq_cid(xgq, id);
		kfree(cmd);
	}

	return ret;
}

/* sysfs */
static ssize_t boot_from_backup_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_boot_from_backup = val ? true : false;
	mutex_unlock(&xgq->xgq_lock);

	return count;
}

static ssize_t boot_from_backup_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	mutex_lock(&xgq->xgq_lock);
	cnt += sprintf(buf + cnt, "%d\n", xgq->xgq_boot_from_backup);
	mutex_unlock(&xgq->xgq_lock);

	return cnt;
}
static DEVICE_ATTR(boot_from_backup, 0644, boot_from_backup_show, boot_from_backup_store);


static ssize_t polling_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_polling = val ? true : false;
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

static ssize_t boot_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	/* update boot status */
	vmr_fpt_query(xgq->xgq_pdev);

	mutex_lock(&xgq->xgq_lock);
	cnt += sprintf(buf + cnt, "HAS_FPT:%d\n",
		xgq->xgq_mb_payload.has_fpt);
	cnt += sprintf(buf + cnt, "HAS_FPT_RECOVERY:%d\n",
		xgq->xgq_mb_payload.has_fpt_recovery);
	cnt += sprintf(buf + cnt, "BOOT_ON_DEFAULT:%d\n",
		xgq->xgq_mb_payload.boot_on_default);
	cnt += sprintf(buf + cnt, "BOOT_ON_BACKUP:%d\n",
		xgq->xgq_mb_payload.boot_on_backup);
	cnt += sprintf(buf + cnt, "BOOT_ON_RECOVERY:%d\n",
		xgq->xgq_mb_payload.boot_on_recovery);
	cnt += sprintf(buf + cnt, "MULTI_BOOT_OFFSET:0x%x\n",
		xgq->xgq_mb_payload.multi_boot_offset);
	mutex_unlock(&xgq->xgq_lock);

	return cnt;
}
static DEVICE_ATTR_RO(boot_status);

static struct attribute *xgq_attrs[] = {
	&dev_attr_polling.attr,
	&dev_attr_boot_from_backup.attr,
	&dev_attr_boot_status.attr,
	NULL,
};

static ssize_t sensor_data_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct xocl_xgq *xgq =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	ssize_t ret = 0;

	/* if off == 0, read data */
	if (off == 0)
		xgq_collect_sensor_data(xgq);

	if (xgq->sensor_data == NULL)
		goto bail;

	if (off >= xgq->sensor_data_length)
		goto bail;

	if (off + count > xgq->sensor_data_length)
		count = xgq->sensor_data_length - off;

	memcpy(buf, xgq->sensor_data + off, count);

	ret = count;
bail:
	return ret;
}

static struct bin_attribute bin_attr_sensor_data = {
	.attr = {
		.name = "sensor_data",
		.mode = 0444
	},
	.read = sensor_data_read,
	.size = 0
};

static struct bin_attribute *xgq_bin_attrs[] = {
	&bin_attr_sensor_data,
	NULL,
};

static struct attribute_group xgq_attr_group = {
	.attrs = xgq_attrs,
	.bin_attrs = xgq_bin_attrs,
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

	if (down_interruptible(&xgq->xgq_data_sema)) {
		XGQ_ERR(xgq, "XGQ data transfer is interrupted");
		ret = -EIO;
		goto done;
	}

	ret = xgq_transfer_data(xgq, kdata, data_len,
		XGQ_CMD_OP_DOWNLOAD_PDI, XOCL_XGQ_FLASH_TIME);

	up(&xgq->xgq_data_sema);

done:
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

	xgq_stop_services(xgq);

	fini_worker(&xgq->xgq_complete_worker);
	fini_worker(&xgq->xgq_health_worker);

	kfree(xgq->sensor_data);

	if (xgq->xgq_ring_base)
		iounmap(xgq->xgq_ring_base);
	if (xgq->xgq_sq_base)
		iounmap(xgq->xgq_sq_base);

	sysfs_remove_group(&pdev->dev.kobj, &xgq_attr_group);
	mutex_destroy(&xgq->xgq_lock);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_release(xgq, &hdl);
	xocl_drvinst_free(hdl);

	XGQ_INFO(xgq, "successfully removed xgq subdev");
	return 0;
}

/* Wait for xgq service is fully ready after a reset. */
static inline bool xgq_device_is_ready(struct xocl_xgq *xgq)
{
	u32 rval = 0;
	int i = 0, retry = 50;

	for (i = 0; i < retry; i++) {
		msleep(100);
		rval = ioread32(xgq->xgq_ring_base + XOCL_XGQ_DEV_STAT_OFFSET);
		if (rval)
			return true;
	}
	
	return false;
}

static int xgq_probe(struct platform_device *pdev)
{
	struct xocl_xgq *xgq = NULL;
	struct resource *res = NULL;
	u64 flags = 0;
	int ret = 0, i = 0;
	void *hdl;

	xgq = xocl_drvinst_alloc(&pdev->dev, sizeof (*xgq));
	if (!xgq)
		return -ENOMEM;
	platform_set_drvdata(pdev, xgq);
	xgq->xgq_pdev = pdev;
	xgq->xgq_cmd_id = 0;

	mutex_init(&xgq->xgq_lock);
	sema_init(&xgq->xgq_data_sema, 1); /*TODO: improve to n based on availabity */

	/*TODO: after real sensor data enabled, redefine this size */
	xgq->sensor_data_length = 8 * 512;
	xgq->sensor_data = kmalloc(xgq->sensor_data_length, GFP_KERNEL);

	for (res = platform_get_resource(pdev, IORESOURCE_MEM, i); res;
	    res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {
		XGQ_INFO(xgq, "res : %s %pR", res->name, res);
		if (!strncmp(res->name, NODE_XGQ_SQ_BASE, strlen(NODE_XGQ_SQ_BASE))) {
			xgq->xgq_sq_base = ioremap_nocache(res->start,
				res->end - res->start + 1);
		}
		if (!strncmp(res->name, NODE_XGQ_RING_BASE, strlen(NODE_XGQ_RING_BASE))) {
			xgq->xgq_ring_base = ioremap_nocache(res->start,
				res->end - res->start + 1);
		}
	}

	if (!xgq->xgq_sq_base || !xgq->xgq_ring_base) {
		ret = -EIO;
		XGQ_ERR(xgq, "platform get resource failed");
		goto attach_failed;
	}

	xgq->xgq_sq_base = xgq->xgq_sq_base + XGQ_SQ_TAIL_POINTER;
	xgq->xgq_cq_base = xgq->xgq_sq_base + XGQ_CQ_TAIL_POINTER;

	/* check device is ready */
	if (!xgq_device_is_ready(xgq)) {
		ret = -ENODEV;
		XGQ_ERR(xgq, "device is not ready, please reset device.");
		goto attach_failed;
	}

	ret = xgq_attach(&xgq->xgq_queue, flags, 0, (u64)xgq->xgq_ring_base,
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

	xgq->xgq_polling = true;

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
	kfree(xgq->sensor_data);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_release(xgq, &hdl);
	xocl_drvinst_free(hdl);

	return ret;
}

static struct xocl_xgq_funcs xgq_ops = {
	.xgq_load_xclbin = xgq_load_xclbin,
	.xgq_check_firewall = xgq_check_firewall,
	.xgq_freq_scaling = xgq_freq_scaling,
	.xgq_freq_scaling_by_topo = xgq_freq_scaling_by_topo,
	.xgq_get_data = xgq_get_data,
	.xgq_download_apu_firmware = xgq_download_apu_firmware,
	.vmr_enable_multiboot = vmr_enable_multiboot,
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
