// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx CU driver for memory to memory BO copy
 *
 * Copyright (C) 2021-2022 Xilinx, Inc.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 */

#include "xrt_xclbin.h"
#include "../xocl_drv.h"
#include "xgq_cmd_vmr.h"
#include "../xgq_xocl_plat.h"
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

#define	CLK_TYPE_DATA	0
#define	CLK_TYPE_KERNEL	1
#define	CLK_TYPE_SYSTEM	2
#define	CLK_TYPE_MAX	4

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

static DEFINE_IDR(xocl_xgq_vmr_cid_idr);
#define XOCL_VMR_INVALID_CID	0xFFFF

/* cmd timeout in seconds */
#define XOCL_XGQ_FLASH_TIME	msecs_to_jiffies(600 * 1000) 
#define XOCL_XGQ_DOWNLOAD_TIME	msecs_to_jiffies(300 * 1000) 
#define XOCL_XGQ_CONFIG_TIME	msecs_to_jiffies(30 * 1000) 
#define XOCL_XGQ_WAIT_TIMEOUT	msecs_to_jiffies(60 * 1000)
#define XOCL_XGQ_MSLEEP_1S	(1000)      //1 s

#define MAX_WAIT 30
#define WAIT_INTERVAL 1000 //ms
/*
 * reserved shared memory size and number for log page.
 * currently, only 1 resource controlled by sema. Can be extended to n.
 */
#define LOG_PAGE_SIZE	(1024 * 1024)
#define LOG_PAGE_NUM	1

/*
 * Shared memory layout:
 * start                          end
 *   | log page |   data transfer  |
 */
#define XOCL_VMR_LOG_ADDR_OFF 	0x0
#define XOCL_VMR_DATA_ADDR_OFF  (LOG_PAGE_SIZE * LOG_PAGE_NUM)

typedef void (*xocl_vmr_complete_cb)(void *arg, struct xgq_com_queue_entry *ccmd);

struct xocl_xgq_vmr;

struct xocl_xgq_vmr_cmd {
	struct xgq_cmd_sq	xgq_cmd_entry;
	struct list_head	xgq_cmd_list;
	struct completion	xgq_cmd_complete;
	xocl_vmr_complete_cb    xgq_cmd_cb;
	void			*xgq_cmd_arg;
	struct timer_list	xgq_cmd_timer;
	struct xocl_xgq_vmr	*xgq_vmr;
	u64			xgq_cmd_timeout_jiffies; /* timout till */
	int			xgq_cmd_rcode;
	/* xgq complete command can return in-line data via payload */
	struct xgq_cmd_cq_default_payload	xgq_cmd_cq_payload;
};

struct xgq_worker {
	struct task_struct	*complete_thread;
	bool			error;
	bool			stop;
	struct xocl_xgq_vmr	*xgq_vmr;
};

struct xocl_xgq_vmr {
	struct platform_device 	*xgq_pdev;
	struct xgq	 	xgq_queue;
	u64			xgq_io_hdl;
	void __iomem		*xgq_payload_base;
	void __iomem		*xgq_sq_base;
	void __iomem		*xgq_ring_base;
	void __iomem		*xgq_cq_base;
	struct mutex 		xgq_lock;
	struct mutex 		clk_scaling_lock;
	struct vmr_shared_mem	xgq_vmr_shared_mem;
	bool 			xgq_polling;
	bool 			xgq_boot_from_backup;
	bool 			xgq_flash_default_only;
	bool 			xgq_flash_to_legacy;
	u32			xgq_intr_base;
	u32			xgq_intr_num;
	struct list_head	xgq_submitted_cmds;
	struct completion 	xgq_irq_complete;
	struct xgq_worker	xgq_complete_worker;
	struct xgq_worker	xgq_health_worker;
	bool			xgq_halted;
	int 			xgq_cmd_id;
	struct semaphore 	xgq_data_sema;
	struct semaphore 	xgq_log_page_sema;
	struct xgq_cmd_cq_default_payload xgq_cq_payload;
	int 			xgq_vmr_debug_level;
	u8			xgq_vmr_debug_type;
	char			*xgq_vmr_system_dtb;
	size_t			xgq_vmr_system_dtb_size;
	u16			pwr_scaling_ovrd_limit;
	u8			temp_scaling_ovrd_limit;
};

static int vmr_status_query(struct platform_device *pdev);
static void xgq_offline_service(struct xocl_xgq_vmr *xgq);

/*
 * when detect cmd is completed, find xgq_cmd from submitted_cmds list
 * and find cmd by cid; perform callback and remove from submitted_cmds.
 */
static void cmd_complete(struct xocl_xgq_vmr *xgq, struct xgq_com_queue_entry *ccmd)
{
	struct xocl_xgq_vmr_cmd *xgq_cmd = NULL;
	struct list_head *pos = NULL, *next = NULL;

	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_vmr_cmd, xgq_cmd_list);

		if (xgq_cmd->xgq_cmd_entry.hdr.cid == ccmd->hdr.cid) {

			list_del(pos);
			if (xgq_cmd->xgq_cmd_cb)
				xgq_cmd->xgq_cmd_cb(xgq_cmd->xgq_cmd_arg, ccmd);
			return;
		}

	}

	XGQ_WARN(xgq, "unknown cid %d received", ccmd->hdr.cid);
	if (ccmd->hdr.cid == XOCL_VMR_INVALID_CID) {
		XGQ_ERR(xgq, "invalid cid %d, offlineing xgq services...", ccmd->hdr.cid);
		/*
		 * Note: xgq_lock mutex is on, release the lock and offline service.
		 */
		mutex_unlock(&xgq->xgq_lock);
		xgq_offline_service(xgq);
		mutex_lock(&xgq->xgq_lock);
	}
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
	struct xocl_xgq_vmr *xgq = xw->xgq_vmr;

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
			/* Note: We dont support xgq interrupt yet.
			 * Ignore commands killed, the health_worker will set
			 * correct rcode for submitted cmds
			 */
			(void) wait_for_completion_killable(&xgq->xgq_irq_complete);
		}

		if (kthread_should_stop()) {
			xw->stop = true;
		}
	}
	
	return xw->error ? 1 : 0;
}

static bool xgq_submitted_cmd_check(struct xocl_xgq_vmr *xgq)
{
	struct xocl_xgq_vmr_cmd *xgq_cmd = NULL;
	struct list_head *pos = NULL, *next = NULL;
	bool found_timeout = false;

	mutex_lock(&xgq->xgq_lock);
	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_vmr_cmd, xgq_cmd_list);

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

static void xgq_submitted_cmds_drain(struct xocl_xgq_vmr *xgq)
{
	struct xocl_xgq_vmr_cmd *xgq_cmd = NULL;
	struct list_head *pos = NULL, *next = NULL;

	mutex_lock(&xgq->xgq_lock);
	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_vmr_cmd, xgq_cmd_list);

		/* Finding timed out cmds */
		if (xgq_cmd->xgq_cmd_timeout_jiffies < jiffies) {
			list_del(pos);
			
			xgq_cmd->xgq_cmd_rcode = -ETIME;
			complete(&xgq_cmd->xgq_cmd_complete);
			XGQ_ERR(xgq, "cmd id: %d op: 0x%x timed out, hot reset is required!",
				xgq_cmd->xgq_cmd_entry.hdr.cid,
				xgq_cmd->xgq_cmd_entry.hdr.opcode);
		}
	}
	mutex_unlock(&xgq->xgq_lock);
}

static void xgq_submitted_cmd_remove(struct xocl_xgq_vmr *xgq, struct xocl_xgq_vmr_cmd *cmd)
{
	struct xocl_xgq_vmr_cmd *xgq_cmd = NULL;
	struct list_head *pos = NULL, *next = NULL;

	mutex_lock(&xgq->xgq_lock);
	list_for_each_safe(pos, next, &xgq->xgq_submitted_cmds) {
		xgq_cmd = list_entry(pos, struct xocl_xgq_vmr_cmd, xgq_cmd_list);

		/* Finding aborted cmds */
		if (xgq_cmd == cmd) {
			list_del(pos);
			
			xgq_cmd->xgq_cmd_rcode = -EIO;

			XGQ_ERR(xgq, "cmd id: %d op: 0x%x reomved.",
				xgq_cmd->xgq_cmd_entry.hdr.cid,
				xgq_cmd->xgq_cmd_entry.hdr.opcode);
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
static bool xgq_submitted_cmds_empty(struct xocl_xgq_vmr *xgq)
{
	mutex_lock(&xgq->xgq_lock);
	if (list_empty(&xgq->xgq_submitted_cmds)) {
		mutex_unlock(&xgq->xgq_lock);
		return true;
	}
	mutex_unlock(&xgq->xgq_lock);
	
	return false;
}

static void xgq_vmr_log_dump(struct xocl_xgq_vmr *xgq, int num_recs, bool dump_to_debug_log)
{
	struct vmr_log log = { 0 };

	if (num_recs > VMR_LOG_MAX_RECS)
		num_recs = VMR_LOG_MAX_RECS;

	xocl_memcpy_fromio(&xgq->xgq_vmr_shared_mem, xgq->xgq_payload_base,
		sizeof(xgq->xgq_vmr_shared_mem));

	/*
	 * log_msg_index which is the oldest log in a ring buffer.
	 * if we want to only dump num_recs, we start from
	 * (log_msg_index + VMR_LOG_MAX_RECS - num_recs) % VMR_LOG_MAX_RECS.
	 */
	if (xgq->xgq_vmr_shared_mem.vmr_magic_no == VMR_MAGIC_NO) {
		u32 idx, log_idx = xgq->xgq_vmr_shared_mem.log_msg_index;

		log_idx = (log_idx + VMR_LOG_MAX_RECS - num_recs) % VMR_LOG_MAX_RECS;

		if (!dump_to_debug_log)
			XGQ_WARN(xgq, "=== start dumping vmr log ===");

		for (idx = 0; idx < num_recs; idx++) {
			xocl_memcpy_fromio(&log.log_buf, xgq->xgq_payload_base +
				xgq->xgq_vmr_shared_mem.log_msg_buf_off +
				sizeof(log) * log_idx,
				sizeof(log));
			log_idx = (log_idx + 1) % VMR_LOG_MAX_RECS;

			if (dump_to_debug_log)
				XGQ_DBG(xgq, "%s", log.log_buf); 
			else
				XGQ_WARN(xgq, "%s", log.log_buf); 
		}

		if (!dump_to_debug_log)
			XGQ_WARN(xgq, "=== end dumping vmr log ===");
	} else {
		XGQ_WARN(xgq, "vmr payload partition table is not available");
	}
}

static void xgq_vmr_log_dump_all(struct xocl_xgq_vmr *xgq)
{
	xgq_vmr_log_dump(xgq, VMR_LOG_MAX_RECS, false);
}

static struct opcode_name {
	const char 	*name;
	int 		opcode;
} opcode_names[] = {
	{"LOAD XCLBIN", XGQ_CMD_OP_LOAD_XCLBIN},
	{"GET LOG PAGE", XGQ_CMD_OP_GET_LOG_PAGE},
	{"DOWNLOAD PDI", XGQ_CMD_OP_DOWNLOAD_PDI},
	{"CLOCK", XGQ_CMD_OP_CLOCK},
	{"SENSOR", XGQ_CMD_OP_SENSOR},
	{"LOAD APUBIN", XGQ_CMD_OP_LOAD_APUBIN},
	{"VMR CONTROL", XGQ_CMD_OP_VMR_CONTROL},
	{"PROGRAM SCFW", XGQ_CMD_OP_PROGRAM_SCFW},
};

const char *get_opcode_name(int opcode)
{
	int i = 0;
	const char *unknown = "UNKNOWN";

	for (i = 0; i < ARRAY_SIZE(opcode_names); i++) {
		if (opcode_names[i].opcode == opcode)
			return opcode_names[i].name;
	}

	return unknown;
}

static void xgq_vmr_log_dump_debug(struct xocl_xgq_vmr *xgq, struct xocl_xgq_vmr_cmd *cmd)
{
	XGQ_WARN(xgq, "opcode: %s(0x%x), rcode: %d, check debug trace for detailed log.",
		get_opcode_name(cmd->xgq_cmd_entry.hdr.opcode),
		cmd->xgq_cmd_entry.hdr.opcode,
		cmd->xgq_cmd_rcode);

	/* Dump VMR logs into xclmgmt debugfs */
	xgq_vmr_log_dump(xgq, 20, true);
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
static void xgq_stop_services(struct xocl_xgq_vmr *xgq)
{
	XGQ_INFO(xgq, "halting xgq services");

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

	XGQ_INFO(xgq, "xgq service is halted");
}

static void xgq_offline_service(struct xocl_xgq_vmr *xgq)
{
	XGQ_INFO(xgq, "xgq service is going offline...");

	/* If we see timeout cmd first time, dump log into dmesg */
	if (!xgq->xgq_halted) {
		xgq_vmr_log_dump_all(xgq);
	}

	/* then we stop service */
	xgq_stop_services(xgq);

	XGQ_INFO(xgq, "xgq service is offline");
}

/*
 * periodically check if there are outstanding timed out commands.
 * if there is any, stop service and drian all timeout cmds
 */
static int health_worker(void *data)
{
	struct xgq_worker *xw = (struct xgq_worker *)data;
	struct xocl_xgq_vmr *xgq = xw->xgq_vmr;

	while (!xw->stop) {
		msleep(XOCL_XGQ_MSLEEP_1S * 10);

		if (xgq_submitted_cmd_check(xgq)) {
			xgq_offline_service(xgq);
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
	struct xocl_xgq_vmr *xgq = (struct xocl_xgq_vmr *)arg;

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
static int submit_cmd(struct xocl_xgq_vmr *xgq, struct xocl_xgq_vmr_cmd *cmd)
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
	xocl_memcpy_toio((void __iomem *)slot_addr, &cmd->xgq_cmd_entry,
		sizeof(cmd->xgq_cmd_entry));

	xgq_notify_peer_produced(&xgq->xgq_queue);

	list_add_tail(&cmd->xgq_cmd_list, &xgq->xgq_submitted_cmds);

done:
	mutex_unlock(&xgq->xgq_lock);
	return rval;
}

static void xgq_complete_cb(void *arg, struct xgq_com_queue_entry *ccmd)
{
	struct xocl_xgq_vmr_cmd *xgq_cmd = (struct xocl_xgq_vmr_cmd *)arg;
	struct xgq_cmd_cq *cmd_cq = (struct xgq_cmd_cq *)ccmd;
	struct xocl_xgq_vmr *xgq_vmr = xgq_cmd->xgq_vmr;

	xgq_cmd->xgq_cmd_rcode = (int)ccmd->rcode;
	/* preserve payload prior to free xgq_cmd_cq */
	memcpy(&xgq_cmd->xgq_cmd_cq_payload, &cmd_cq->cq_default_payload,
		sizeof(cmd_cq->cq_default_payload));

	complete(&xgq_cmd->xgq_cmd_complete);

	if (xgq_cmd->xgq_cmd_rcode)
		xgq_vmr_log_dump_debug(xgq_vmr, xgq_cmd);
}

static size_t inline vmr_shared_mem_size(struct xocl_xgq_vmr *xgq)
{
	return xgq->xgq_vmr_shared_mem.vmr_data_end -
		xgq->xgq_vmr_shared_mem.vmr_data_start + 1;
}

static size_t inline shm_size_log_page(struct xocl_xgq_vmr *xgq)
{
	return (LOG_PAGE_SIZE * LOG_PAGE_NUM);
}

static size_t inline shm_size_data(struct xocl_xgq_vmr *xgq)
{
	return vmr_shared_mem_size(xgq) - shm_size_log_page(xgq);
}

static u32 inline shm_addr_log_page(struct xocl_xgq_vmr *xgq)
{
	return xgq->xgq_vmr_shared_mem.vmr_data_start +
		XOCL_VMR_LOG_ADDR_OFF;
}

static u32 inline shm_addr_data(struct xocl_xgq_vmr *xgq)
{
	return xgq->xgq_vmr_shared_mem.vmr_data_start +
		XOCL_VMR_DATA_ADDR_OFF;
}

/*TODO: enhance to n resources by atomic test_and_clear_bit/set_bit */
static int shm_acquire_log_page(struct xocl_xgq_vmr *xgq, u32 *addr, u32 *len)
{
	if (down_interruptible(&xgq->xgq_log_page_sema)) {
		XGQ_ERR(xgq, "cancelled");
		return -EIO;
	}

	/*TODO: memset shared memory to all zero */
	*addr = shm_addr_log_page(xgq);
	*len = LOG_PAGE_SIZE;
	return 0;
}

static void shm_release_log_page(struct xocl_xgq_vmr *xgq)
{
	up(&xgq->xgq_log_page_sema);
}

static int shm_acquire_data(struct xocl_xgq_vmr *xgq, u32 *addr, u32 *len)
{
	if (down_interruptible(&xgq->xgq_data_sema)) {
		XGQ_ERR(xgq, "cancelled");
		return -EIO;
	}

	*addr = shm_addr_data(xgq);
	*len = shm_size_data(xgq);
	return 0;
}

static void shm_release_data(struct xocl_xgq_vmr *xgq)
{
	up(&xgq->xgq_data_sema);
}

static void memcpy_to_device(struct xocl_xgq_vmr *xgq, u32 offset, const void *data,
	size_t len)
{
	void __iomem *dst = xgq->xgq_payload_base + offset;

	memcpy_toio(dst, data, len);
}

static void memcpy_from_device(struct xocl_xgq_vmr *xgq, u32 offset, void *dst,
	size_t len)
{
	void __iomem *src = xgq->xgq_payload_base + offset;

	memcpy_fromio(dst, src, len);
}

static inline int get_xgq_cid(struct xocl_xgq_vmr *xgq)
{
	int id = 0;

	mutex_lock(&xgq->xgq_lock);
	id = idr_alloc_cyclic(&xocl_xgq_vmr_cid_idr, xgq, 0, 0, GFP_KERNEL);
	mutex_unlock(&xgq->xgq_lock);

	return id;
}

static inline void remove_xgq_cid(struct xocl_xgq_vmr *xgq, int id)
{
	mutex_lock(&xgq->xgq_lock);
	idr_remove(&xocl_xgq_vmr_cid_idr, id);
	mutex_unlock(&xgq->xgq_lock);
}

static enum xgq_cmd_flash_type inline get_flash_type(struct xocl_xgq_vmr *xgq)
{

	if (xgq->xgq_flash_to_legacy)
		return XGQ_CMD_FLASH_TO_LEGACY;
	if (xgq->xgq_flash_default_only)
		return XGQ_CMD_FLASH_NO_BACKUP;

	return XGQ_CMD_FLASH_DEFAULT;
}

static void vmr_cq_result_copy(struct xocl_xgq_vmr *xgq, struct xocl_xgq_vmr_cmd *cmd)
{
	struct xgq_cmd_cq_default_payload *payload =
		(struct xgq_cmd_cq_default_payload *)&cmd->xgq_cmd_cq_payload;

	mutex_lock(&xgq->xgq_lock);
	memcpy(&xgq->xgq_cq_payload, payload, sizeof(*payload));
	mutex_unlock(&xgq->xgq_lock);
}

/*
 * Utilize shared memory between host and device to transfer data.
 */
static ssize_t xgq_transfer_data(struct xocl_xgq_vmr *xgq, const void *buf,
	u64 len, enum xgq_cmd_opcode opcode, u32 timer)
{
	struct xocl_xgq_vmr_cmd *cmd = NULL;
	struct xgq_cmd_data_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	ssize_t ret = 0;
	u32 address = 0;
	u32 length = 0;
	int id = 0;

	if (opcode != XGQ_CMD_OP_LOAD_XCLBIN && 
	    opcode != XGQ_CMD_OP_DOWNLOAD_PDI &&
	    opcode != XGQ_CMD_OP_LOAD_APUBIN &&
	    opcode != XGQ_CMD_OP_PROGRAM_SCFW) {
		XGQ_WARN(xgq, "unsupported opcode %d", opcode);
		return -EINVAL;
	}

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_WARN(xgq, "no enough memory");
		return -ENOMEM;
	}

	/* set up xgq_cmd */
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq_vmr = xgq;

	if (shm_acquire_data(xgq, &address, &length)) {
		ret = -EIO;
		goto acquire_failed;
	}

	if (length < len) {
		ret = -EINVAL;
		XGQ_ERR(xgq, "request %lld is larger than available %d",
			len, length);
		goto cid_alloc_failed;
	}
	/* set up payload */
	payload = (opcode == XGQ_CMD_OP_LOAD_XCLBIN) ?
		&(cmd->xgq_cmd_entry.pdi_payload) :
		&(cmd->xgq_cmd_entry.xclbin_payload);

	/*
	 * copy buf data onto shared memory with device.
	 * Note: if len == 0, it is PROGRAME_SCFW, no payload to copyin
	 */
	if (len > 0)
		memcpy_to_device(xgq, address, buf, len);
	payload->address = address;
	payload->size = len;
	payload->addr_type = XGQ_CMD_ADD_TYPE_AP_OFFSET;
	payload->flash_type = get_flash_type(xgq);

	/* set up hdr */
	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = opcode;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		ret = -ENOMEM;
		goto cid_alloc_failed;
	}
	hdr->cid = id;

	/* init condition variable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timeout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + timer;

	if (submit_cmd(xgq, cmd)) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d", id);
		goto done;
	}

	/*
	 * For pdi/xclbin data transfer, we block any cancellation and
	 * wait till command completed and then release resources safely.
	 * We yield after every timeout to avoid linux kernel warning for
	 * thread hunging too long.
	 */
	while (!wait_for_completion_timeout(&cmd->xgq_cmd_complete, XOCL_XGQ_WAIT_TIMEOUT))
		yield();

	/* If return is 0, we set length as return value */
	if (cmd->xgq_cmd_rcode) {
		ret = cmd->xgq_cmd_rcode;
	} else {
		ret = len;
	}

done:
	remove_xgq_cid(xgq, id);

cid_alloc_failed:
	shm_release_data(xgq);

acquire_failed:
	kfree(cmd);

	return ret;
}

static int xgq_load_xclbin(struct platform_device *pdev,
	const void *u_xclbin)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	struct axlf *xclbin = (struct axlf *)u_xclbin;
	u64 xclbin_len = xclbin->m_header.m_length;
	int ret = 0;
	
	ret = xgq_transfer_data(xgq, u_xclbin, xclbin_len,
		XGQ_CMD_OP_LOAD_XCLBIN, XOCL_XGQ_DOWNLOAD_TIME);

	return ret == xclbin_len ? 0 : -EIO;
}

static int xgq_program_scfw(struct platform_device *pdev)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);

	return xgq_transfer_data(xgq, NULL, 0,
		XGQ_CMD_OP_PROGRAM_SCFW, XOCL_XGQ_DOWNLOAD_TIME);
}

/* Note: caller is responsibe for vfree(*fw) */
static int xgq_log_page_fw(struct platform_device *pdev,
	char **fw, size_t *fw_size, enum xgq_cmd_log_page_type req_pid)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_vmr_cmd *cmd = NULL;
	struct xgq_cmd_log_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;
	int id = 0;
	u32 address = 0;
	u32 len = 0;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return -ENOMEM;
	}

	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq_vmr = xgq;

	if (shm_acquire_log_page(xgq, &address, &len)) {
		ret = -EIO;
		goto acquire_failed;
	}

	payload = &(cmd->xgq_cmd_entry.log_payload);
	payload->address = address;
	payload->size = len;
	payload->offset = 0;
	payload->pid = req_pid;

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_GET_LOG_PAGE;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		ret = -ENOMEM;
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto cid_alloc_failed;
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
	if (wait_for_completion_killable(&cmd->xgq_cmd_complete)) {
		XGQ_ERR(xgq, "submitted cmd killed");
		xgq_submitted_cmd_remove(xgq, cmd);
	}

	ret = cmd->xgq_cmd_rcode;

	if (ret) {
		XGQ_ERR(xgq, "ret %d", ret);
	} else {
		struct xgq_cmd_cq_log_page_payload *fw_result = NULL;

		fw_result = (struct xgq_cmd_cq_log_page_payload *)&cmd->xgq_cmd_cq_payload;

		if (fw_result->count > len) {
			XGQ_ERR(xgq, "need to alloc %d for device data", 
				fw_result->count);
			ret = -ENOSPC;
		} else if (fw_result->count == 0) {
			XGQ_ERR(xgq, "fw size cannot be zero");
			ret = -EINVAL;
		} else {
			*fw_size = fw_result->count;
			*fw = vmalloc(*fw_size);
			if (*fw == NULL) {
				XGQ_ERR(xgq, "vmalloc failed");
				ret = -ENOMEM;
				goto done;
			}
			memcpy_from_device(xgq, address, *fw, *fw_size);
			ret = 0;
			XGQ_INFO(xgq, "loading fw from vmr size %ld", *fw_size);
		}
	}

done:
	remove_xgq_cid(xgq, id);

cid_alloc_failed:
	shm_release_log_page(xgq);

acquire_failed:
	kfree(cmd);

	return ret;
}

static int xgq_log_page_metadata(struct platform_device *pdev,
	char **fw, size_t *fw_size)
{
	return xgq_log_page_fw(pdev, fw, fw_size, XGQ_CMD_LOG_FW);
}

static int xgq_log_page_system_dtb(struct platform_device *pdev,
	char **fw, size_t *fw_size)
{
	return xgq_log_page_fw(pdev, fw, fw_size, XGQ_CMD_LOG_SYSTEM_DTB);
}

static int xgq_firewall_op(struct platform_device *pdev, enum xgq_cmd_log_page_type type_pid)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_vmr_cmd *cmd = NULL;
	struct xgq_cmd_log_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;
	int id = 0;
	u32 address = 0;
	u32 len = 0;

	/* skip periodic firewall check when xgq service is halted */
	if (xgq->xgq_halted)
		return 0;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry please");
		return 0;
	}

	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq_vmr = xgq;

	if (shm_acquire_log_page(xgq, &address, &len)) {
		ret = 0;
		XGQ_ERR(xgq, "shared memory is busy, retry please");
		goto acquire_failed;
	}

	payload = &(cmd->xgq_cmd_entry.log_payload);
	payload->address = address;
	payload->size = len;
	payload->offset = 0;
	payload->pid = type_pid;

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_GET_LOG_PAGE;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto cid_alloc_failed;
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
	if (wait_for_completion_killable(&cmd->xgq_cmd_complete)) {
		XGQ_ERR(xgq, "submitted cmd killed");
		xgq_submitted_cmd_remove(xgq, cmd);
		/* this is not a firewall trip */
		ret = 0;
		goto done;
	}

	ret = (cmd->xgq_cmd_rcode == -ETIME || cmd->xgq_cmd_rcode == -EINVAL) ?
		0 : cmd->xgq_cmd_rcode;

	if (ret) {
		struct xgq_cmd_cq_log_page_payload *log = NULL;
		u32 log_size = 0;

		log = (struct xgq_cmd_cq_log_page_payload *)&cmd->xgq_cmd_cq_payload;
		log_size = log->count;

		if (log_size > len) {
			XGQ_ERR(xgq, "return log size %d is greater than request %d",
				log->count, len);
			log_size = len;
		} else if (log_size  == 0) {
			XGQ_ERR(xgq, "no error message");
		} else {
			char *log_msg = vmalloc(log_size);
			if (log_msg == NULL) {
				XGQ_ERR(xgq, "vmalloc failed, no msg");
				goto done;
			}
			memcpy_from_device(xgq, address, log_msg, log_size);
			XGQ_ERR(xgq, "%s", log_msg);
			vfree(log_msg);
		}
	} 

done:
	remove_xgq_cid(xgq, id);

cid_alloc_failed:
	shm_release_log_page(xgq);

acquire_failed:
	kfree(cmd);

	return ret;
}

static int xgq_check_firewall(struct platform_device *pdev)
{
	return xgq_firewall_op(pdev, XGQ_CMD_LOG_AF_CHECK);
}

static int xgq_clear_firewall(struct platform_device *pdev)
{
	return xgq_firewall_op(pdev, XGQ_CMD_LOG_AF_CLEAR);
}

static int vmr_info_query_op(struct platform_device *pdev,
	char *buf, size_t *cnt, enum xgq_cmd_log_page_type type_pid)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_vmr_cmd *cmd = NULL;
	struct xgq_cmd_log_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;
	int id = 0;
	u32 address = 0;
	u32 len = 0;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return -ENOMEM;
	}

	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq_vmr = xgq;

	if (shm_acquire_log_page(xgq, &address, &len)) {
		ret = -EIO;
		goto acquire_failed;
	}

	payload = &(cmd->xgq_cmd_entry.log_payload);
	payload->address = address;
	payload->size = len;
	payload->offset = 0;
	payload->pid = type_pid;

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_GET_LOG_PAGE;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto cid_alloc_failed;
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
	if (wait_for_completion_killable(&cmd->xgq_cmd_complete)) {
		XGQ_ERR(xgq, "submitted cmd killed");
		xgq_submitted_cmd_remove(xgq, cmd);
	}

	ret = cmd->xgq_cmd_rcode;

	if (ret) {
		XGQ_ERR(xgq, "ret %d", ret);
	} else {
		struct xgq_cmd_cq_log_page_payload *info = NULL;
		u32 info_size = 0;

		info = (struct xgq_cmd_cq_log_page_payload *)&cmd->xgq_cmd_cq_payload;
		info_size = info->count;

		if (info_size > len) {
			XGQ_WARN(xgq, "return info size %d is greater than request %d", 
				info->count, len);
			info_size = len;
		} else if (info_size == 0) {
			XGQ_WARN(xgq, "info size is zero");
			ret = -EINVAL;
		} else {
			char *info_data = vmalloc(info_size + 1);
			size_t count = 0;

			if (info_data == NULL) {
				XGQ_ERR(xgq, "vmalloc failed");
				ret = -ENOMEM;
				goto done;
			}
			memcpy_from_device(xgq, address, info_data, info_size);
			info_data[info_size] = '\0'; /* terminate the string */

			/* text buffer for sysfs node should be limited to PAGE_SIZE */
			count = snprintf(buf, PAGE_SIZE, "%s", info_data);
			if (count > PAGE_SIZE) {
				XGQ_WARN(xgq, "message size %d exceeds %ld",
					info_size, PAGE_SIZE);
			}
			*cnt = min(count, PAGE_SIZE);
			vfree(info_data);
		}
	}

done:
	remove_xgq_cid(xgq, id);

cid_alloc_failed:
	shm_release_log_page(xgq);

acquire_failed:
	kfree(cmd);

	return ret;
}

static int vmr_verbose_info_query(struct platform_device *pdev,
	char *buf, size_t *cnt)
{
	return vmr_info_query_op(pdev, buf, cnt, XGQ_CMD_LOG_INFO);
}
static int vmr_endpoint_info_query(struct platform_device *pdev,
	char *buf, size_t *cnt)
{
	return vmr_info_query_op(pdev, buf, cnt, XGQ_CMD_LOG_ENDPOINT);
}

static int vmr_task_info_query(struct platform_device *pdev,
	char *buf, size_t *cnt)
{
	return vmr_info_query_op(pdev, buf, cnt, XGQ_CMD_LOG_TASK_STATS);
}

static int vmr_memory_info_query(struct platform_device *pdev,
	char *buf, size_t *cnt)
{
	return vmr_info_query_op(pdev, buf, cnt, XGQ_CMD_LOG_MEM_STATS);
}

/* On versal, verify is enforced. */
static int xgq_freq_scaling(struct platform_device *pdev,
	unsigned short *freqs, int num_freqs, int verify)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_vmr_cmd *cmd = NULL;
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
	cmd->xgq_vmr = xgq;

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
		goto cid_alloc_failed;
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
	if (wait_for_completion_killable(&cmd->xgq_cmd_complete)) {
		XGQ_ERR(xgq, "submitted cmd killed");
		xgq_submitted_cmd_remove(xgq, cmd);
	}

	ret = cmd->xgq_cmd_rcode;
	if (ret) {
		XGQ_ERR(xgq, "ret %d", cmd->xgq_cmd_rcode);
	} 

done:
	remove_xgq_cid(xgq, id);

cid_alloc_failed:
	kfree(cmd);

	return ret;
}

static int xgq_freq_scaling_by_topo(struct platform_device *pdev,
	struct clock_freq_topology *topo, int verify)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
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

static uint32_t xgq_clock_get_data(struct xocl_xgq_vmr *xgq,
	enum xgq_cmd_clock_req_type req_type, int req_id)
{
	struct xocl_xgq_vmr_cmd *cmd = NULL;
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
		return -ENOMEM;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq_vmr = xgq;

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
		goto cid_alloc_failed;
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
	if (wait_for_completion_killable(&cmd->xgq_cmd_complete)) {
		XGQ_ERR(xgq, "submitted cmd killed");
		xgq_submitted_cmd_remove(xgq, cmd);
	}

	ret = cmd->xgq_cmd_rcode;
	if (ret) {
		XGQ_ERR(xgq, "ret %d", cmd->xgq_cmd_rcode);
		ret = 0;
	} else {
		/* freq result is in rdata */
		ret = ((struct xgq_cmd_cq_clock_payload *)&cmd->xgq_cmd_cq_payload)->ocl_freq;
	}

done:
	remove_xgq_cid(xgq, id);

cid_alloc_failed:
	kfree(cmd);

	return ret;
}

static uint64_t xgq_get_data(struct platform_device *pdev,
	enum data_kind kind)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
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

static bool vmr_check_apu_is_ready(struct xocl_xgq_vmr *xgq)
{
	struct xgq_cmd_cq_vmr_payload *vmr_status =
		(struct xgq_cmd_cq_vmr_payload *)&xgq->xgq_cq_payload;

	if (vmr_status_query(xgq->xgq_pdev))
		return false;

	return vmr_status->ps_is_ready ? true : false;
}

static int vmr_wait_apu_is_ready(struct xocl_xgq_vmr *xgq)
{
	bool is_ready = false;
	int i = 0;

	/*
	 * We wait till the apu is back online or report EBUSY after a
	 * certain time.
	 */
	for (i = 0; i < MAX_WAIT; i++) {
		is_ready = vmr_check_apu_is_ready(xgq);
		if (is_ready)
			break;

		msleep(WAIT_INTERVAL);
	}

	XGQ_INFO(xgq, "wait %d seconds for PS ready value: %d", i, is_ready);
	return is_ready ? 0 : -ETIME;
}

static int xgq_download_apu_bin(struct platform_device *pdev, char *buf,
	size_t len)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	int ret = 0;

	ret = xgq_transfer_data(xgq, buf, len, XGQ_CMD_OP_LOAD_APUBIN,
		XOCL_XGQ_DOWNLOAD_TIME);
	if (ret != len) {
		XGQ_ERR(xgq, "return %d, but request %ld", ret, len);
		return -EIO;
	}

	XGQ_INFO(xgq, "successfully download len %ld", len);
	return 0;
}

/* read firmware from /lib/firmware/xilinx, load via xgq */
static int xgq_download_apu_firmware(struct platform_device *pdev)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(pdev);
	char *apu_bin = "xilinx/xrt-versal-apu.xsabin";
	char *apu_bin_buf = NULL;
	size_t apu_bin_len = 0;
	int ret = 0;

	/* APU is ready, no dup download */
	if (vmr_check_apu_is_ready(xgq)) {
		XGQ_INFO(xgq, "apu is ready, skip download");
		return ret;
	}

	ret = xocl_request_firmware(&pcidev->dev, apu_bin,
			&apu_bin_buf, &apu_bin_len);
	if (ret)
		return ret;

	XGQ_INFO(xgq, "start vmr-downloading apu firmware");
	ret = xgq_download_apu_bin(pdev, apu_bin_buf, apu_bin_len);
	vfree(apu_bin_buf);
	if (ret) 
		return ret;

	XGQ_INFO(xgq, "start waiting apu becomes ready");
	/* wait till apu is ready or return -ETIME */
	return vmr_wait_apu_is_ready(xgq);
}

static int vmr_control_op(struct platform_device *pdev,
	enum xgq_cmd_vmr_control_type req_type)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_vmr_cmd *cmd = NULL;
	struct xgq_cmd_vmr_control_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;
	int id = 0;

	if (xgq->xgq_halted) {
		XGQ_WARN(xgq, "VMR XGQ service is haulted. skip.");
		return -EIO;
	}
	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return -ENOMEM;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq_vmr = xgq;

	payload = &(cmd->xgq_cmd_entry.vmr_control_payload);
	payload->req_type = req_type;
	payload->debug_level = xgq->xgq_vmr_debug_level;
	payload->debug_type = xgq->xgq_vmr_debug_type;

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_VMR_CONTROL;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto cid_alloc_failed;
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
	if (wait_for_completion_killable(&cmd->xgq_cmd_complete)) {
		XGQ_ERR(xgq, "submitted cmd killed");
		xgq_submitted_cmd_remove(xgq, cmd);
	}

	ret = cmd->xgq_cmd_rcode;

	if (ret) {
		XGQ_ERR(xgq, "Multiboot or reset might not work. ret %d", ret);
	} else if (req_type == XGQ_CMD_VMR_QUERY) {
		vmr_cq_result_copy(xgq, cmd);
	}

done:
	remove_xgq_cid(xgq, id);

cid_alloc_failed:
	kfree(cmd);

	return ret;
}

static int vmr_status_query(struct platform_device *pdev)
{
	return vmr_control_op(pdev, XGQ_CMD_VMR_QUERY);
}

static void clk_scaling_cq_result_copy(struct xocl_xgq_vmr *xgq,
                                       struct xocl_xgq_vmr_cmd *cmd)
{
	struct xgq_cmd_cq_default_payload *payload =
		(struct xgq_cmd_cq_default_payload *)&cmd->xgq_cmd_cq_payload;
	struct xgq_cmd_cq_clk_scaling_payload *cs_payload=
		(struct xgq_cmd_cq_clk_scaling_payload *)&xgq->xgq_cq_payload;

	mutex_lock(&xgq->xgq_lock);
	memcpy(&xgq->xgq_cq_payload, payload, sizeof(*payload));
	xgq->pwr_scaling_ovrd_limit = cs_payload->pwr_scaling_limit;
	xgq->temp_scaling_ovrd_limit = cs_payload->temp_scaling_limit;
	mutex_unlock(&xgq->xgq_lock);
}

static int clk_scaling_configure_op(struct platform_device *pdev,
                                    enum xgq_cmd_clk_scaling_app_id aid,
                                    bool enable, uint16_t pwr_ovrd_limit,
                                    uint8_t temp_ovrd_limit)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_vmr_cmd *cmd = NULL;
	struct xgq_cmd_clk_scaling_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	int ret = 0;
	int id = 0;

	if (xgq->xgq_halted) {
		XGQ_WARN(xgq, "VMR XGQ service is haulted. skip.");
		return -EIO;
	}

	cmd = kmalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return -ENOMEM;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq_vmr = xgq;

	payload = &(cmd->xgq_cmd_entry.clk_scaling_payload);
	payload->aid = aid;
	if (aid == XGQ_CMD_CLK_THROTTLING_AID_CONFIGURE)
	{
		payload->scaling_en = enable ? 1 : 0;
		if (pwr_ovrd_limit)
			payload->pwr_scaling_ovrd_limit = pwr_ovrd_limit;
		if (temp_ovrd_limit)
			payload->temp_scaling_ovrd_limit = temp_ovrd_limit;
	}

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_CLK_THROTTLING;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		goto cid_alloc_failed;
	}
	hdr->cid = id;

	/* init condition veriable */
	init_completion(&cmd->xgq_cmd_complete);

	/* set timout actual jiffies */
	cmd->xgq_cmd_timeout_jiffies = jiffies + XOCL_XGQ_CONFIG_TIME;

	ret = submit_cmd(xgq, cmd);
	if (ret) {
		XGQ_ERR(xgq, "submit cmd failed, cid %d, err: %d", id, ret);
		goto done;
	}

	/* wait for command completion */
	if (wait_for_completion_killable(&cmd->xgq_cmd_complete)) {
		XGQ_ERR(xgq, "submitted cmd killed");
		xgq_submitted_cmd_remove(xgq, cmd);
	}

	ret = cmd->xgq_cmd_rcode;

	if (ret) {
		XGQ_ERR(xgq, "Clock scaling request failed with err: %d", ret);
	} else if (aid == XGQ_CMD_CLK_THROTTLING_AID_READ) {
		clk_scaling_cq_result_copy(xgq, cmd);
	}

done:
	remove_xgq_cid(xgq, id);

cid_alloc_failed:
	kfree(cmd);

	return ret;
}

static int clk_scaling_status_query(struct platform_device *pdev)
{
	return clk_scaling_configure_op(pdev, XGQ_CMD_CLK_THROTTLING_AID_READ, 0, 0, 0);
}

static int vmr_enable_multiboot(struct platform_device *pdev)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);

	return vmr_control_op(pdev,
		xgq->xgq_boot_from_backup ? XGQ_CMD_BOOT_BACKUP : XGQ_CMD_BOOT_DEFAULT);
}

static int xgq_collect_sensors(struct platform_device *pdev, int aid, int sid,
                               char *data_buf, uint32_t len, uint8_t sensor_id)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(pdev);
	struct xocl_xgq_vmr_cmd *cmd = NULL;
	struct xgq_cmd_sensor_payload *payload = NULL;
	struct xgq_cmd_sq_hdr *hdr = NULL;
	u32 address = 0;
	u32 length = 0;
	int ret = 0;
	int id = 0;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		XGQ_ERR(xgq, "kmalloc failed, retry");
		return -ENOMEM;
	}

	cmd->xgq_cmd_cb = xgq_complete_cb;
	cmd->xgq_cmd_arg = cmd;
	cmd->xgq_vmr = xgq;

	if (shm_acquire_log_page(xgq, &address, &length)) {
		ret = -EIO;
		goto acquire_failed;
	}

	if (length < len) {
		XGQ_WARN(xgq, "request %d, but can only have %d available", len, length);
		len = length;
	}
	payload = &(cmd->xgq_cmd_entry.sensor_payload);
	payload->address = address;
	payload->size = len;
	//Sensor API ID
	payload->aid = aid;
	//Sensor request ID
	payload->sid = sid;
	//Sensor ID
	payload->sensor_id = sensor_id;

	hdr = &(cmd->xgq_cmd_entry.hdr);
	hdr->opcode = XGQ_CMD_OP_SENSOR;
	hdr->state = XGQ_SQ_CMD_NEW;
	hdr->count = sizeof(*payload);
	id = get_xgq_cid(xgq);
	if (id < 0) {
		XGQ_ERR(xgq, "alloc cid failed: %d", id);
		ret = id;
		goto cid_alloc_failed;
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
	if (wait_for_completion_killable(&cmd->xgq_cmd_complete)) {
		XGQ_ERR(xgq, "submitted cmd killed");
		xgq_submitted_cmd_remove(xgq, cmd);
	}

	ret = cmd->xgq_cmd_rcode;

	if (ret) {
		XGQ_ERR(xgq, "ret %d", cmd->xgq_cmd_rcode);
	} else {
		memcpy_from_device(xgq, address, data_buf, len);
	}

done:
	remove_xgq_cid(xgq, id);

cid_alloc_failed:
	shm_release_log_page(xgq);

acquire_failed:
	kfree(cmd);

	return ret;
}

static int xgq_collect_sensors_by_repo_id(struct platform_device *pdev, char *buf,
	 uint8_t repo_id, uint32_t len)
{
	return xgq_collect_sensors(pdev, XGQ_CMD_SENSOR_AID_GET_SDR, repo_id, buf, len, 0);
}

static int xgq_collect_sensors_by_sensor_id(struct platform_device *pdev, char *buf,
	 uint8_t repo_id, uint32_t len, uint8_t sensor_id)
{
	return xgq_collect_sensors(pdev, XGQ_CMD_SENSOR_AID_GET_SINGLE_SDR, repo_id, buf, len, sensor_id);
}

static int xgq_collect_all_inst_sensors(struct platform_device *pdev, char *buf,
	 uint8_t repo_id, uint32_t len)
{
	return xgq_collect_sensors(pdev, XGQ_CMD_SENSOR_AID_GET_ALL_SDR, repo_id, buf, len, 0);
}

/* sysfs */
static ssize_t boot_from_backup_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_boot_from_backup = val ? true : false;
	mutex_unlock(&xgq->xgq_lock);

	/*
	 * each time if we change the boot config, we should notify VMR
	 * so that the next hot reset will reset the card correctly
	 * Temporary disable the set due to a warm reboot might cause
	 * the system to hung.
	 * vmr_enable_multiboot(to_platform_device(dev));
	 */
	return count;
}

static ssize_t boot_from_backup_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	mutex_lock(&xgq->xgq_lock);
	cnt += sprintf(buf + cnt, "%d\n", xgq->xgq_boot_from_backup);
	mutex_unlock(&xgq->xgq_lock);

	return cnt;
}
static DEVICE_ATTR(boot_from_backup, 0644, boot_from_backup_show, boot_from_backup_store);

static ssize_t flash_default_only_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_flash_default_only = val ? true : false;
	mutex_unlock(&xgq->xgq_lock);

	return count;
}

static ssize_t flash_default_only_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	mutex_lock(&xgq->xgq_lock);
	cnt += sprintf(buf + cnt, "%d\n", xgq->xgq_flash_default_only);
	mutex_unlock(&xgq->xgq_lock);

	return cnt;
}
static DEVICE_ATTR(flash_default_only, 0644, flash_default_only_show, flash_default_only_store);

static ssize_t flash_to_legacy_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_flash_to_legacy = val ? true : false;
	mutex_unlock(&xgq->xgq_lock);

	return count;
}

static ssize_t flash_to_legacy_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	mutex_lock(&xgq->xgq_lock);
	cnt += sprintf(buf + cnt, "%d\n", xgq->xgq_flash_to_legacy);
	mutex_unlock(&xgq->xgq_lock);

	return cnt;
}
static DEVICE_ATTR(flash_to_legacy, 0644, flash_to_legacy_show, flash_to_legacy_store);

static ssize_t polling_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
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
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	mutex_lock(&xgq->xgq_lock);
	cnt += sprintf(buf + cnt, "%d\n", xgq->xgq_polling);
	mutex_unlock(&xgq->xgq_lock);

	return cnt;
}
static DEVICE_ATTR(polling, 0644, polling_show, polling_store);

static ssize_t vmr_debug_level_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 3) {
		XGQ_ERR(xgq, "level should be 0 - 3");
		return -EINVAL;
	}

	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_vmr_debug_level = val;
	mutex_unlock(&xgq->xgq_lock);

	/* request debug level change */
	if (vmr_status_query(xgq->xgq_pdev))
		return -EINVAL;

	return count;
}
static DEVICE_ATTR_WO(vmr_debug_level);

static ssize_t program_sc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;
	int ret = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL) {
		return -EINVAL;
	}

	if (val) {
		ret = xgq_program_scfw(to_platform_device(dev));
		if (ret) {
			XGQ_ERR(xgq, "failed: %d", ret);
			return -EINVAL;
		}
	}

	XGQ_INFO(xgq, "done");

	return count;
}
static DEVICE_ATTR_WO(program_sc);

static ssize_t vmr_debug_dump_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL) {
		return -EINVAL;
	}

	xgq_vmr_log_dump(xgq, val, true);

	return count;
}
static DEVICE_ATTR_WO(vmr_debug_dump);

static ssize_t vmr_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	struct xgq_cmd_cq_vmr_payload *vmr_status =
		(struct xgq_cmd_cq_vmr_payload *)&xgq->xgq_cq_payload;
	ssize_t cnt = 0;

	/* update boot status */
	if (vmr_status_query(xgq->xgq_pdev))
		return -EINVAL;

	cnt += sprintf(buf + cnt, "HAS_FPT:%d\n", vmr_status->has_fpt);
	cnt += sprintf(buf + cnt, "HAS_FPT_RECOVERY:%d\n", vmr_status->has_fpt_recovery);
	cnt += sprintf(buf + cnt, "BOOT_ON_DEFAULT:%d\n", vmr_status->boot_on_default);
	cnt += sprintf(buf + cnt, "BOOT_ON_BACKUP:%d\n", vmr_status->boot_on_backup);
	cnt += sprintf(buf + cnt, "BOOT_ON_RECOVERY:%d\n", vmr_status->boot_on_recovery);
	cnt += sprintf(buf + cnt, "CURRENT_MULTI_BOOT_OFFSET:0x%x\n", vmr_status->current_multi_boot_offset);
	cnt += sprintf(buf + cnt, "BOOT_ON_OFFSET:0x%x\n", vmr_status->boot_on_offset);
	cnt += sprintf(buf + cnt, "HAS_EXTFPT:%d\n", vmr_status->has_extfpt);
	cnt += sprintf(buf + cnt, "HAS_EXT_META_XSABIN:%d\n", vmr_status->has_ext_xsabin);
	cnt += sprintf(buf + cnt, "HAS_EXT_SC_FW:%d\n", vmr_status->has_ext_scfw);
	cnt += sprintf(buf + cnt, "HAS_EXT_SYSTEM_DTB:%d\n", vmr_status->has_ext_sysdtb);
	cnt += sprintf(buf + cnt, "DEBUG_LEVEL:%d\n", vmr_status->debug_level);
	cnt += sprintf(buf + cnt, "PROGRAM_PROGRESS:%d\n", vmr_status->program_progress);
	cnt += sprintf(buf + cnt, "PL_IS_READY:%d\n", vmr_status->pl_is_ready);
	cnt += sprintf(buf + cnt, "PS_IS_READY:%d\n", vmr_status->ps_is_ready);

	return cnt;
}
static DEVICE_ATTR_RO(vmr_status);

static ssize_t vmr_verbose_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	if (vmr_verbose_info_query(xgq->xgq_pdev, buf, &cnt))
		return -EINVAL;

	return cnt;
}
static DEVICE_ATTR_RO(vmr_verbose_info);

static ssize_t vmr_endpoint_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	if (vmr_endpoint_info_query(xgq->xgq_pdev, buf, &cnt))
		return -EINVAL;

	return cnt;
}
static DEVICE_ATTR_RO(vmr_endpoint);

static ssize_t vmr_task_stats_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	if (vmr_task_info_query(xgq->xgq_pdev, buf, &cnt))
		return -EINVAL;

	return cnt;
}
static DEVICE_ATTR_RO(vmr_task_stats);

static ssize_t vmr_mem_stats_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	if (vmr_memory_info_query(xgq->xgq_pdev, buf, &cnt))
		return -EINVAL;

	return cnt;
}
static DEVICE_ATTR_RO(vmr_mem_stats);

static ssize_t vmr_debug_type_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 2) {
		XGQ_ERR(xgq, "type should be 0 - 2");
		return -EINVAL;
	}

	mutex_lock(&xgq->xgq_lock);
	xgq->xgq_vmr_debug_type = val;
	mutex_unlock(&xgq->xgq_lock);

	if (vmr_control_op(xgq->xgq_pdev, XGQ_CMD_VMR_DEBUG))
		return -EINVAL;

	return count;
}
static DEVICE_ATTR_WO(vmr_debug_type);

static ssize_t clk_scaling_stat_raw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	struct xgq_cmd_cq_clk_scaling_payload *cs_payload=
		(struct xgq_cmd_cq_clk_scaling_payload *)&xgq->xgq_cq_payload;
	ssize_t cnt = 0;

	//Read clock scaling configuration settings
	cnt += sprintf(buf + cnt, "HAS_CLOCK_THROTTLING:%d\n", cs_payload->has_clk_scaling);
	cnt += sprintf(buf + cnt, "CLOCK_THROTTLING_ENABLED:%d\n", cs_payload->clk_scaling_en);
	cnt += sprintf(buf + cnt, "POWER_SHUTDOWN_LIMIT:%u\n", cs_payload->pwr_shutdown_limit);
	cnt += sprintf(buf + cnt, "TEMP_SHUTDOWN_LIMIT:%u\n", cs_payload->temp_shutdown_limit);
	cnt += sprintf(buf + cnt, "POWER_THROTTLING_LIMIT:%u\n", cs_payload->pwr_scaling_limit);
	cnt += sprintf(buf + cnt, "TEMP_THROTTLING_LIMIT:%u\n", cs_payload->temp_scaling_limit);
	cnt += sprintf(buf + cnt, "POWER_THROTTLING_OVRD_LIMIT:%u\n", xgq->pwr_scaling_ovrd_limit);
	cnt += sprintf(buf + cnt, "TEMP_THROTTLING_OVRD_LIMIT:%u\n", xgq->temp_scaling_ovrd_limit);
	cnt += sprintf(buf + cnt, "POWER_THROTTLING_OVRD_ENABLE:%u\n", xgq->pwr_scaling_ovrd_limit);
	cnt += sprintf(buf + cnt, "TEMP_THROTTLING_OVRD_ENABLE:%u\n", xgq->temp_scaling_ovrd_limit);
	cnt += sprintf(buf + cnt, "CLOCK_THROTTLING_MODE:%u\n", cs_payload->clk_scaling_mode);

	return cnt;
}
static DEVICE_ATTR_RO(clk_scaling_stat_raw);

static ssize_t clk_scaling_configure_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	struct xgq_cmd_cq_clk_scaling_payload *cs_payload=
		(struct xgq_cmd_cq_clk_scaling_payload *)&xgq->xgq_cq_payload;
	ssize_t cnt = 0;

	cnt += sprintf(buf + cnt, "%d,%u,%u\n", cs_payload->clk_scaling_en,
				   xgq->pwr_scaling_ovrd_limit, xgq->temp_scaling_ovrd_limit);

	return cnt;
}

/*
 * clk_scaling_configure_store(): Used to configure clock scaling feature parameters through
 * "clk_scaling_configure" sysfs node.
 * Supporting parameters:
 *   Enable - enable clock scaling feature.
 *   Disable - disable clock scaling feature.
 *   Power override limit - override power threshold value for internal testing.
 *   Temp override limit - override temperature threshold value for internal testing.
 * Arguments to the sysfs node "clk_scaling_configure": It is a string contains 3 values seperated
 * by literal ",". Example: "1,200,80".
 *   Argument 1: tells enable (1) or disable (0) of clock scaling feature
 *   Argument 2: tells Power override limit in Watts
 *   Argument 3: tells Temperature override limit in Celcius
 */
static ssize_t clk_scaling_configure_store(struct device *dev,
                                           struct device_attribute *attr,
                                           const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	struct xgq_cmd_cq_clk_scaling_payload *cs_payload=
		(struct xgq_cmd_cq_clk_scaling_payload *)&xgq->xgq_cq_payload;
	uint8_t enable = 0;
	uint16_t pwr = 0;
	uint8_t temp = 0;
	char* args = (char*) buf;
	char* end = (char*) buf;
	int ret = 0;

	if (!cs_payload->has_clk_scaling)
	{
		XGQ_ERR(xgq, "clock scaling feature is not supported");
		return -ENOTSUPP;
	}

	if (args != NULL) {
		args = strsep(&end, ",");
		if (kstrtou8(args, 10, &enable) == -EINVAL || enable > 1) {
			XGQ_ERR(xgq, "value should be 0 (disable) or 1 (enable)");
			return -EINVAL;
		}
		args = end;
	}

	if (args != NULL) {
		args = strsep(&end, ",");
		if ((kstrtou16(args, 10, &pwr) == -EINVAL) ||
			(pwr > cs_payload->pwr_scaling_limit)) {
			XGQ_ERR(xgq, "Invalid power override limit %u provided, whereas max limit is %u",
					pwr, cs_payload->pwr_scaling_limit);
			return -EINVAL;
		}
		args = end;
	}

	if (args != NULL) {
		args = strsep(&end, ",");
		if ((kstrtou8(args, 10, &temp) == -EINVAL) ||
			(temp > cs_payload->temp_scaling_limit)) {
			XGQ_ERR(xgq, "Invalid temp override limit %u provided, wereas max limit is %u",
					temp, cs_payload->temp_scaling_limit);
			return -EINVAL;
		}
		args = end;
	}
	mutex_lock(&xgq->clk_scaling_lock);
	ret = clk_scaling_configure_op(xgq->xgq_pdev,
                                   XGQ_CMD_CLK_THROTTLING_AID_CONFIGURE,
                                   enable, pwr, temp);
	if (!ret) {
		cs_payload->clk_scaling_en = enable;
		if (enable)
			XGQ_INFO(xgq, "clock scaling feature is enabled");
		else
			XGQ_INFO(xgq, "clock scaling feature is disabled");
		if (pwr)
			xgq->pwr_scaling_ovrd_limit = pwr;
		if (temp)
			xgq->temp_scaling_ovrd_limit = temp;
	}
	mutex_unlock(&xgq->clk_scaling_lock);

	return count;
}
static DEVICE_ATTR_RW(clk_scaling_configure);

static ssize_t xgq_scaling_temp_override_show(struct device *dev,
                                              struct device_attribute *attr,
                                              char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	cnt += sprintf(buf + cnt, "%u\n", xgq->temp_scaling_ovrd_limit);

	return cnt;
}

static ssize_t xgq_scaling_temp_override_store(struct device *dev,
                                               struct device_attribute *attr,
                                               const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	struct xgq_cmd_cq_clk_scaling_payload *cs_payload =
		(struct xgq_cmd_cq_clk_scaling_payload *)&xgq->xgq_cq_payload;
	u16 temp = 0;
	int ret = 0;

	if (!cs_payload->has_clk_scaling)
	{
		XGQ_ERR(xgq, "clock scaling feature is not supported");
		return -ENOTSUPP;
	}

	if ((kstrtou16(buf, 10, &temp) == -EINVAL) || (temp > cs_payload->temp_scaling_limit)) {
		XGQ_ERR(xgq, "Invalid temp override limit %u provided, whereas max limit is %u",
				temp, cs_payload->temp_scaling_limit);
		return -EINVAL;
	}

	mutex_lock(&xgq->clk_scaling_lock);
	ret = clk_scaling_configure_op(xgq->xgq_pdev, XGQ_CMD_CLK_THROTTLING_AID_CONFIGURE,
								   cs_payload->clk_scaling_en, 0, temp);
	if (!ret)
		xgq->temp_scaling_ovrd_limit = temp;

	mutex_unlock(&xgq->clk_scaling_lock);

	return count;
}
static DEVICE_ATTR_RW(xgq_scaling_temp_override);

static ssize_t xgq_scaling_power_override_show(struct device *dev,
                                               struct device_attribute *attr,
                                               char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	cnt += sprintf(buf + cnt, "%u\n", xgq->pwr_scaling_ovrd_limit);

	return cnt;
}

static ssize_t xgq_scaling_power_override_store(struct device *dev,
                                                struct device_attribute *attr,
                                                const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	struct xgq_cmd_cq_clk_scaling_payload *cs_payload =
		(struct xgq_cmd_cq_clk_scaling_payload *)&xgq->xgq_cq_payload;
	u16 pwr = 0;
	int ret = 0;

	if (!cs_payload->has_clk_scaling)
	{
		XGQ_ERR(xgq, "clock scaling feature is not supported");
		return -ENOTSUPP;
	}

	if ((kstrtou16(buf, 10, &pwr) == -EINVAL) || (pwr > cs_payload->pwr_scaling_limit)) {
		XGQ_ERR(xgq, "Invalid power override limit %u provided, whereas max limit is %u",
				pwr, cs_payload->pwr_scaling_limit);
		return -EINVAL;
	}

	mutex_lock(&xgq->clk_scaling_lock);
	ret = clk_scaling_configure_op(xgq->xgq_pdev, XGQ_CMD_CLK_THROTTLING_AID_CONFIGURE,
				   cs_payload->clk_scaling_en, pwr, 0);
	if (!ret)
		xgq->pwr_scaling_ovrd_limit = pwr;

	mutex_unlock(&xgq->clk_scaling_lock);

	return count;
}
static DEVICE_ATTR_RW(xgq_scaling_power_override);

static ssize_t xgq_scaling_enable_show(struct device *dev,
                                       struct device_attribute *attr,
                                       char *buf)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	struct xgq_cmd_cq_clk_scaling_payload *cs_payload =
		(struct xgq_cmd_cq_clk_scaling_payload *)&xgq->xgq_cq_payload;
	ssize_t cnt = 0;

	cnt += sprintf(buf + cnt, "%d\n", cs_payload->clk_scaling_en);

	return cnt;
}

static ssize_t xgq_scaling_enable_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
	struct xocl_xgq_vmr *xgq = platform_get_drvdata(to_platform_device(dev));
	struct xgq_cmd_cq_clk_scaling_payload *cs_payload=
		(struct xgq_cmd_cq_clk_scaling_payload *)&xgq->xgq_cq_payload;
	bool enable = false;
	u32 val = 0;
	int ret = 0;

	if (!cs_payload->has_clk_scaling)
	{
		XGQ_ERR(xgq, "clock scaling feature is not supported");
		return -ENOTSUPP;
	}

	if (strncmp(buf, "true", strlen("true")) == 0)
		val = 1;

	enable = val ? true : false;
	mutex_lock(&xgq->clk_scaling_lock);
	ret = clk_scaling_configure_op(xgq->xgq_pdev,
                                   XGQ_CMD_CLK_THROTTLING_AID_CONFIGURE,
                                   enable, 0, 0);
	if (!ret) {
		cs_payload->clk_scaling_en = enable;
		if (enable)
			XGQ_INFO(xgq, "clock scaling feature is enabled");
		else
			XGQ_INFO(xgq, "clock scaling feature is disabled");
	}
	mutex_unlock(&xgq->clk_scaling_lock);

	return count;
}
static DEVICE_ATTR_RW(xgq_scaling_enable);

static ssize_t vmr_system_dtb_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct xocl_xgq_vmr *xgq =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	unsigned char *blob;
	size_t size;
	ssize_t ret = 0;

	mutex_lock(&xgq->xgq_lock);
	blob = xgq->xgq_vmr_system_dtb;
	size = xgq->xgq_vmr_system_dtb_size;
	mutex_unlock(&xgq->xgq_lock);

	if (off >= size)
		goto out;

	if (off + count > size)
		count = size - off;
	memcpy(buf, blob + off, count);

	ret = count;
out:
	return ret;
}
/* Some older linux kernel doesn't support
 * static BIN_ATTR_RO(vmr_system_dtb, 0);
 */
static struct bin_attribute bin_attr_vmr_system_dtb = {
	.attr = {
		.name = "vmr_system_dtb",
		.mode = 0444
	},
	.read = vmr_system_dtb_read,
	.write = NULL,
	.size = 0
};

static struct attribute *vmr_attrs[] = {
	&dev_attr_polling.attr,
	&dev_attr_boot_from_backup.attr,
	&dev_attr_flash_default_only.attr,
	&dev_attr_flash_to_legacy.attr,
	&dev_attr_vmr_status.attr,
	&dev_attr_vmr_verbose_info.attr,
	&dev_attr_vmr_endpoint.attr,
	&dev_attr_vmr_task_stats.attr,
	&dev_attr_vmr_mem_stats.attr,
	&dev_attr_program_sc.attr,
	&dev_attr_vmr_debug_level.attr,
	&dev_attr_vmr_debug_dump.attr,
	&dev_attr_vmr_debug_type.attr,
	&dev_attr_clk_scaling_stat_raw.attr,
	&dev_attr_clk_scaling_configure.attr,
	&dev_attr_xgq_scaling_enable.attr,
	&dev_attr_xgq_scaling_power_override.attr,
	&dev_attr_xgq_scaling_temp_override.attr,
	NULL,
};

static struct bin_attribute *vmr_bin_attrs[] = {
	&bin_attr_vmr_system_dtb,
	NULL,
};
static struct attribute_group xgq_attr_group = {
	.attrs = vmr_attrs,
	.bin_attrs = vmr_bin_attrs,
};

static ssize_t xgq_ospi_write(struct file *filp, const char __user *udata,
	size_t data_len, loff_t *off)
{
	struct xocl_xgq_vmr *xgq = filp->private_data;
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

	ret = xgq_transfer_data(xgq, kdata, data_len,
		XGQ_CMD_OP_DOWNLOAD_PDI, XOCL_XGQ_FLASH_TIME);
done:
	vfree(kdata);

	return ret;
}

static int xgq_ospi_open(struct inode *inode, struct file *file)
{
	struct xocl_xgq_vmr *xgq = NULL;

	xgq = xocl_drvinst_open(inode->i_cdev);
	if (!xgq)
		return -ENXIO;

	file->private_data = xgq;
	return 0;
}

static int xgq_ospi_close(struct inode *inode, struct file *file)
{
	struct xocl_xgq_vmr *xgq = file->private_data;

	xocl_drvinst_close(xgq);
	return 0;
}

static int xgq_vmr_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_xgq_vmr	*xgq;
	void *hdl;

	xgq = platform_get_drvdata(pdev);
	if (!xgq) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xgq_stop_services(xgq);

	fini_worker(&xgq->xgq_complete_worker);
	fini_worker(&xgq->xgq_health_worker);

	if (xgq->xgq_payload_base)
		iounmap(xgq->xgq_payload_base);
	if (xgq->xgq_sq_base)
		iounmap(xgq->xgq_sq_base);

	xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_HWMON_SDM);

	sysfs_remove_group(&pdev->dev.kobj, &xgq_attr_group);

	/* make sure cached system_dtb blob is freed */
	if (xgq->xgq_vmr_system_dtb)
		vfree(xgq->xgq_vmr_system_dtb);

	mutex_destroy(&xgq->clk_scaling_lock);
	mutex_destroy(&xgq->xgq_lock);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_release(xgq, &hdl);
	xocl_drvinst_free(hdl);

	XGQ_INFO(xgq, "successfully removed xgq subdev");
	return 0;
}

/* Wait for xgq service is fully ready after a reset. */
static inline bool xgq_device_is_ready(struct xocl_xgq_vmr *xgq)
{
	u32 rval = 0;
	int i = 0, retry = 50;

	for (i = 0; i < retry; i++) {
		msleep(100);

		memcpy_fromio(&xgq->xgq_vmr_shared_mem, xgq->xgq_payload_base,
			sizeof(xgq->xgq_vmr_shared_mem));
		if (xgq->xgq_vmr_shared_mem.vmr_magic_no == VMR_MAGIC_NO) {
			rval = ioread32(xgq->xgq_payload_base +
				xgq->xgq_vmr_shared_mem.vmr_status_off);
			if (rval)
				return true;
		}
	}
	
	return false;
}

static int xgq_vmr_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_xgq_vmr *xgq = NULL;
	struct resource *res = NULL;
	struct xocl_subdev_info subdev_info = XOCL_DEVINFO_HWMON_SDM;
	u64 flags = 0;
	int ret = 0, i = 0;
	void *hdl;

	xgq = xocl_drvinst_alloc(&pdev->dev, sizeof (*xgq));
	if (!xgq)
		return -ENOMEM;
	platform_set_drvdata(pdev, xgq);
	xgq->xgq_pdev = pdev;
	xgq->xgq_cmd_id = 0;
	xgq->xgq_vmr_system_dtb = NULL;
	xgq->xgq_vmr_system_dtb_size = 0;

	mutex_init(&xgq->xgq_lock);
	mutex_init(&xgq->clk_scaling_lock);
	sema_init(&xgq->xgq_data_sema, 1);
	sema_init(&xgq->xgq_log_page_sema, 1); /*TODO: improve to n based on availabity */

	for (res = platform_get_resource(pdev, IORESOURCE_MEM, i); res;
	    res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {
		XGQ_INFO(xgq, "res : %s %pR", res->name, res);
		if (!strncmp(res->name, NODE_XGQ_SQ_BASE, strlen(NODE_XGQ_SQ_BASE))) {
			xgq->xgq_sq_base = ioremap_nocache(res->start,
				res->end - res->start + 1);
		}
		if (!strncmp(res->name, NODE_XGQ_VMR_PAYLOAD_BASE,
			strlen(NODE_XGQ_VMR_PAYLOAD_BASE))) {
			xgq->xgq_payload_base = ioremap_nocache(res->start,
				res->end - res->start + 1);
		}
	}

	if (!xgq->xgq_sq_base || !xgq->xgq_payload_base) {
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

	xgq->xgq_ring_base = xgq->xgq_payload_base + xgq->xgq_vmr_shared_mem.ring_buffer_off;
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

	xgq->xgq_complete_worker.xgq_vmr = xgq;
	xgq->xgq_health_worker.xgq_vmr = xgq;
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
		(void) xgq_vmr_remove(pdev);
		return ret;
	}

	ret = xgq_log_page_system_dtb(pdev, &xgq->xgq_vmr_system_dtb,
			&xgq->xgq_vmr_system_dtb_size);
	if (ret) {
		XGQ_WARN(xgq, "cannot load system dtb from shell, ret: %d", ret);
		ret = 0;
	}
	ret = xgq_download_apu_firmware(pdev);
	if (ret) {
		XGQ_WARN(xgq, "unable to download APU, ret: %d", ret);
		ret = 0;
	}

	//Read clock scaling configuration settings
	ret = clk_scaling_status_query(pdev);
	if (ret) {
		XGQ_WARN(xgq, "Failed to receive clock scaling default settings, ret: %d", ret);
		ret = 0;
	} else {
		struct xgq_cmd_cq_clk_scaling_payload *cs_payload=
			(struct xgq_cmd_cq_clk_scaling_payload *)&xgq->xgq_cq_payload;
		if (cs_payload->has_clk_scaling)
			XGQ_INFO(xgq, "clock scaling feature is supported, and enable status: %d", cs_payload->clk_scaling_en);
		else
			XGQ_INFO(xgq, "clock scaling feature is not supported");
	}

	ret = xocl_subdev_create(xdev, &subdev_info);
	if (ret) {
		XGQ_WARN(xgq, "unable to create HWMON_SDM subdev, ret: %d", ret);
		ret = 0;
	}

	XGQ_INFO(xgq, "Initialized xgq subdev, polling (%d)", xgq->xgq_polling);

	return ret;

attach_failed:
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_release(xgq, &hdl);
	xocl_drvinst_free(hdl);

	return ret;
}

static struct xocl_xgq_vmr_funcs xgq_vmr_ops = {
	.xgq_load_xclbin = xgq_load_xclbin,
	.xgq_check_firewall = xgq_check_firewall,
	.xgq_clear_firewall = xgq_clear_firewall,
	.xgq_freq_scaling = xgq_freq_scaling,
	.xgq_freq_scaling_by_topo = xgq_freq_scaling_by_topo,
	.xgq_get_data = xgq_get_data,
	.xgq_download_apu_firmware = xgq_download_apu_firmware,
	.vmr_enable_multiboot = vmr_enable_multiboot,
	.xgq_collect_sensors_by_repo_id = xgq_collect_sensors_by_repo_id,
	.xgq_collect_sensors_by_sensor_id = xgq_collect_sensors_by_sensor_id,
	.xgq_collect_all_inst_sensors = xgq_collect_all_inst_sensors,
	.vmr_load_firmware = xgq_log_page_metadata,
};

static const struct file_operations xgq_vmr_fops = {
	.owner = THIS_MODULE,
	.open = xgq_ospi_open,
	.release = xgq_ospi_close,
	.write = xgq_ospi_write,
};

struct xocl_drv_private xgq_vmr_priv = {
	.ops = &xgq_vmr_ops,
	.fops = &xgq_vmr_fops,
	.dev = -1,
};

struct platform_device_id xgq_vmr_id_table[] = {
	{ XOCL_DEVNAME(XOCL_XGQ_VMR), (kernel_ulong_t)&xgq_vmr_priv },
	{ },
};

static struct platform_driver	xgq_vmr_driver = {
	.probe		= xgq_vmr_probe,
	.remove		= xgq_vmr_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_XGQ_VMR),
	},
	.id_table = xgq_vmr_id_table,
};

int __init xocl_init_xgq(void)
{
	int err = 0;

	err = alloc_chrdev_region(&xgq_vmr_priv.dev, 0, XOCL_MAX_DEVICES,
	    XGQ_DEV_NAME);
	if (err < 0)
		return err;

	err = platform_driver_register(&xgq_vmr_driver);
	if (err) {
		unregister_chrdev_region(xgq_vmr_priv.dev, XOCL_MAX_DEVICES);
		return err;
	}

	return 0;
}

void xocl_fini_xgq(void)
{
	unregister_chrdev_region(xgq_vmr_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&xgq_vmr_driver);
}
