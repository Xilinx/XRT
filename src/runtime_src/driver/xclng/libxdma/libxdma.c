/*
 * Driver for XDMA for Xilinx XDMA IP core
 *
 * Copyright (C) 2007-2017 Xilinx, Inc.
 *
 * Karen Xie <karen.xie@xilinx.com>
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/errno.h>
//#include <linux/device.h>
#include <linux/sched.h>

#include "libxdma.h"
#include "libxdma_api.h"

/* SECTION: Module licensing */

#ifdef __LIBXDMA_MOD__
#define DRV_MODULE_NAME		"libxdma"
#define DRV_MODULE_DESC		"Xilinx XDMA Base Driver"
#define DRV_MODULE_VERSION	"1.0"
#define DRV_MODULE_RELDATE	"Feb. 2017"

static char version[] =
	DRV_MODULE_DESC " " DRV_MODULE_NAME
	" v" DRV_MODULE_VERSION " (" DRV_MODULE_RELDATE ")\n";

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("GPL v2");

#endif

/*
 * xdma device management
 * maintains a list of the xdma devices
 */
static LIST_HEAD(xdev_list);
static DEFINE_MUTEX(xdev_mutex);

static LIST_HEAD(xdev_rcu_list);
static DEFINE_SPINLOCK(xdev_rcu_lock);

static inline void xdev_list_add(struct xdma_dev *xdev)
{
	mutex_lock(&xdev_mutex);
	if (list_empty(&xdev_list))
		xdev->idx = 0;
	else {
		struct xdma_dev *last;

		last = list_last_entry(&xdev_list, struct xdma_dev, list_head);
		xdev->idx = last->idx + 1;
	}
	list_add_tail(&xdev->list_head, &xdev_list);
	mutex_unlock(&xdev_mutex);

pr_info("xdev 0x%p, idx %d.\n", xdev, xdev->idx);

	spin_lock(&xdev_rcu_lock);
	list_add_tail_rcu(&xdev->rcu_node, &xdev_rcu_list);
	spin_unlock(&xdev_rcu_lock);
}

static inline void xdev_list_remove(struct xdma_dev *xdev)
{
	mutex_lock(&xdev_mutex);
	list_del(&xdev->list_head);
	mutex_unlock(&xdev_mutex);

	spin_lock(&xdev_rcu_lock);
	list_del_rcu(&xdev->rcu_node);
	spin_unlock(&xdev_rcu_lock);
	synchronize_rcu();
}

struct xdma_dev *xdev_find_by_pdev(struct pci_dev *pdev)
{
        struct xdma_dev *xdev, *tmp;

        mutex_lock(&xdev_mutex);
        list_for_each_entry_safe(xdev, tmp, &xdev_list, list_head) {
                if (xdev->pdev == pdev) {
                        mutex_unlock(&xdev_mutex);
                        return xdev;
                }
        }
        mutex_unlock(&xdev_mutex);
        return NULL;
}
EXPORT_SYMBOL_GPL(xdev_find_by_pdev);

static inline int debug_check_dev_hndl(const char *fname, struct pci_dev *pdev,
				 void *hndl)
{
	struct xdma_dev *xdev;

	if (!pdev)
		return -EINVAL;

	xdev = xdev_find_by_pdev(pdev);
	if (!xdev) {
		pr_info("%s pdev 0x%p, hndl 0x%p, NO match found!\n",
			fname, pdev, hndl);
		return -EINVAL;
	}
	BUG_ON(xdev != hndl);
	return 0;
}

#ifdef __LIBXDMA_DEBUG__
/* SECTION: Function definitions */
inline void __write_register(const char *fn, u32 value, void *iomem, unsigned long off)
{
	pr_err("%s: w reg 0x%lx(0x%p), 0x%x.\n", fn, off, iomem, value);
	iowrite32(value, iomem);
}
#define write_register(v,mem,off) __write_register(__func__, v, mem, off)
#else
#define write_register(v,mem,off) iowrite32(v, mem)
#endif

inline u32 read_register(void *iomem)
{
	return ioread32(iomem);
}

static inline u32 build_u32(u32 hi, u32 lo)
{
	return ((hi & 0xFFFFUL) << 16) | (lo & 0xFFFFUL);
}

static inline u64 build_u64(u64 hi, u64 lo)
{
	return ((hi & 0xFFFFFFFULL) << 32) | (lo & 0xFFFFFFFFULL);
}

static u64 find_feature_id(const struct xdma_dev *xdev)
{
	u64 low = 0;
	u64 high = 0;
#define FEATURE_ID 0x031000

	if (xdev->user_bar_idx < 0)
		return 0UL;

	low = ioread32(xdev->bar[xdev->user_bar_idx] + FEATURE_ID);
	high = ioread32(xdev->bar[xdev->user_bar_idx] + FEATURE_ID + 8);
	return low | (high << 32);
}

static void interrupt_status(struct xdma_dev *xdev)
{
	struct interrupt_regs *reg = (struct interrupt_regs *)
		(xdev->bar[xdev->config_bar_idx] + XDMA_OFS_INT_CTRL);
	u32 w;

	dbg_irq("reg = %p\n", reg);
	dbg_irq("&reg->user_int_enable = %p\n", &reg->user_int_enable);

	w = read_register(&reg->user_int_enable);
	dbg_irq("user_int_enable = 0x%08x\n", w);
	w = read_register(&reg->channel_int_enable);
	dbg_irq("channel_int_enable = 0x%08x\n", w);

	w = read_register(&reg->user_int_request);
	dbg_irq("user_int_request = 0x%08x\n", w);
	w = read_register(&reg->channel_int_request);
	dbg_irq("channel_int_request = 0x%08x\n", w);

	w = read_register(&reg->user_int_pending);
	dbg_irq("user_int_pending = 0x%08x\n", w);
	w = read_register(&reg->channel_int_pending);
	dbg_irq("channel_int_pending = 0x%08x\n", w);
}

/* channel_interrupts_enable -- Enable interrupts we are interested in */
static void channel_interrupts_enable(struct xdma_dev *xdev, u32 mask)
{
	struct interrupt_regs *reg = (struct interrupt_regs *)
		(xdev->bar[xdev->config_bar_idx] + XDMA_OFS_INT_CTRL);

	write_register(mask, &reg->channel_int_enable_w1s, XDMA_OFS_INT_CTRL);
}

/* channel_interrupts_disable -- Disable interrupts we not interested in */
static void channel_interrupts_disable(struct xdma_dev *xdev, u32 mask)
{
	struct interrupt_regs *reg = (struct interrupt_regs *)
		(xdev->bar[xdev->config_bar_idx] + XDMA_OFS_INT_CTRL);

	write_register(mask, &reg->channel_int_enable_w1c, XDMA_OFS_INT_CTRL);
}

/* user_interrupts_enable -- Enable interrupts we are interested in */
static void user_interrupts_enable(struct xdma_dev *xdev, u32 mask)
{
	struct interrupt_regs *reg = (struct interrupt_regs *)
		(xdev->bar[xdev->config_bar_idx] + XDMA_OFS_INT_CTRL);

	write_register(mask, &reg->user_int_enable_w1s, XDMA_OFS_INT_CTRL);
}

/* user_interrupts_disable -- Disable interrupts we not interested in */
static void user_interrupts_disable(struct xdma_dev *xdev, u32 mask)
{
	struct interrupt_regs *reg = (struct interrupt_regs *)
		(xdev->bar[xdev->config_bar_idx] + XDMA_OFS_INT_CTRL);

	write_register(mask, &reg->user_int_enable_w1c, XDMA_OFS_INT_CTRL);
}

/* read_interrupts -- Print the interrupt controller status */
static u32 read_interrupts(struct xdma_dev *xdev)
{
	struct interrupt_regs *reg = (struct interrupt_regs *)
		(xdev->bar[xdev->config_bar_idx] + XDMA_OFS_INT_CTRL);
	u32 lo;
	u32 hi;

	/* extra debugging; inspect complete engine set of registers */
	hi = read_register(&reg->user_int_request);
	dbg_io("ioread32(0x%p) returned 0x%08x (user_int_request).\n",
		&reg->user_int_request, hi);
	lo = read_register(&reg->channel_int_request);
	dbg_io("ioread32(0x%p) returned 0x%08x (channel_int_request)\n",
		&reg->channel_int_request, lo);

	/* return interrupts: user in upper 16-bits, channel in lower 16-bits */
	return build_u32(hi, lo);
}

static void engine_reg_dump(struct xdma_engine *engine)
{
	u32 w;

	BUG_ON(!engine);

	w = read_register(&engine->regs->identifier);
	dbg_io("%s: ioread32(0x%p) = 0x%08x (id).\n",
		engine->name, &engine->regs->identifier, w);
	w &= BLOCK_ID_MASK;
	if (w != BLOCK_ID_HEAD) {
		pr_info("%s: engine id missing, 0x%08x exp. 0xad4bXX01.\n",
			 engine->name, w);
		return;
	}
	/* extra debugging; inspect complete engine set of registers */
	w = read_register(&engine->regs->status);
	dbg_io("%s: ioread32(0x%p) = 0x%08x (status).\n",
		engine->name, &engine->regs->status, w);
	w = read_register(&engine->regs->control);
	dbg_io("%s: ioread32(0x%p) = 0x%08x (control)\n",
		engine->name, &engine->regs->control, w);
	w = read_register(&engine->sgdma_regs->first_desc_lo);
	dbg_io("%s: ioread32(0x%p) = 0x%08x (first_desc_lo)\n",
		engine->name, &engine->sgdma_regs->first_desc_lo, w);
	w = read_register(&engine->sgdma_regs->first_desc_hi);
	dbg_io("%s: ioread32(0x%p) = 0x%08x (first_desc_hi)\n",
		engine->name, &engine->sgdma_regs->first_desc_hi, w);
	w = read_register(&engine->sgdma_regs->first_desc_adjacent);
	dbg_io("%s: ioread32(0x%p) = 0x%08x (first_desc_adjacent).\n",
		engine->name, &engine->sgdma_regs->first_desc_adjacent, w);
	w = read_register(&engine->regs->completed_desc_count);
	dbg_io("%s: ioread32(0x%p) = 0x%08x (completed_desc_count).\n",
		engine->name, &engine->regs->completed_desc_count, w);
	w = read_register(&engine->regs->interrupt_enable_mask);
	dbg_io("%s: ioread32(0x%p) = 0x%08x (interrupt_enable_mask)\n",
		engine->name, &engine->regs->interrupt_enable_mask, w);
}

/**
 * engine_status_read() - read status of SG DMA engine (optionally reset)
 *
 * Stores status in engine->status.
 *
 * @return -1 on failure, status register otherwise
 */
static u32 engine_status_read(struct xdma_engine *engine, int clear, int dump)
{
	u32 value;

	BUG_ON(!engine);

	if (dump)
		engine_reg_dump(engine);

	/* read status register */
	if (clear)
		value = engine->status =
			read_register(&engine->regs->status_rc);
	else
		value = engine->status = read_register(&engine->regs->status);

	if (dump)
		pr_info("SG engine %s status: 0x%08x: %s%s%s%s%s%s%s%s%s.\n",
			engine->name, (u32)engine->status,
			(value & XDMA_STAT_BUSY) ? "BUSY " : "IDLE ",
			(value & XDMA_STAT_DESC_STOPPED) ?
					 "DESC_STOPPED " : "",
			(value & XDMA_STAT_DESC_COMPLETED) ?
					"DESC_COMPLETED " : "",
			(value & XDMA_STAT_ALIGN_MISMATCH) ?
					"ALIGN_MISMATCH " : "",
			(value & XDMA_STAT_MAGIC_STOPPED) ?
					"MAGIC_STOPPED " : "",
			(value & XDMA_STAT_FETCH_STOPPED) ?
					"FETCH_STOPPED " : "",
			(value & XDMA_STAT_READ_ERROR) ? "READ_ERROR " : "",
			(value & XDMA_STAT_DESC_ERROR) ? "DESC_ERROR " : "",
			(value & XDMA_STAT_IDLE_STOPPED) ?
					 "IDLE_STOPPED " : "");
	return value;
}

/**
 * xdma_engine_stop() - stop an SG DMA engine
 *
 */
static void xdma_engine_stop(struct xdma_engine *engine)
{
	u32 w;

	BUG_ON(!engine);
	dbg_tfr("xdma_engine_stop(engine=%p)\n", engine);

	w = 0;
	w |= (u32)XDMA_CTRL_IE_DESC_ALIGN_MISMATCH;
	w |= (u32)XDMA_CTRL_IE_MAGIC_STOPPED;
	w |= (u32)XDMA_CTRL_IE_READ_ERROR;
	w |= (u32)XDMA_CTRL_IE_DESC_ERROR;
	w |= (u32)XDMA_CTRL_IE_DESC_STOPPED;
	w |= (u32)XDMA_CTRL_IE_DESC_COMPLETED;


	dbg_tfr("Stopping SG DMA %s engine; writing 0x%08x to 0x%p.\n",
			engine->name, w, (u32 *)&engine->regs->control);
	write_register(w, &engine->regs->control,
			(unsigned long)(&engine->regs->control) -
			(unsigned long)(&engine->regs));
	/* dummy read of status register to flush all previous writes */
	dbg_tfr("xdma_engine_stop(%s) done\n", engine->name);
}

static void engine_start_mode_config(struct xdma_engine *engine)
{
	u32 w;

	BUG_ON(!engine);

	/* write control register of SG DMA engine */
	w = (u32)XDMA_CTRL_RUN_STOP;
	w |= (u32)XDMA_CTRL_IE_READ_ERROR;
	w |= (u32)XDMA_CTRL_IE_DESC_ERROR;
	w |= (u32)XDMA_CTRL_IE_DESC_ALIGN_MISMATCH;
	w |= (u32)XDMA_CTRL_IE_MAGIC_STOPPED;

	w |= (u32)XDMA_CTRL_IE_DESC_STOPPED;
	w |= (u32)XDMA_CTRL_IE_DESC_COMPLETED;

	/* set non-incremental addressing mode */
	if (engine->non_incr_addr)
		w |= (u32)XDMA_CTRL_NON_INCR_ADDR;

	dbg_tfr("iowrite32(0x%08x to 0x%p) (control)\n", w,
			(void *)&engine->regs->control);
	/* start the engine */
	write_register(w, &engine->regs->control,
			(unsigned long)(&engine->regs->control) -
			(unsigned long)(&engine->regs));

	/* dummy read of status register to flush all previous writes */
	w = read_register(&engine->regs->status);
	dbg_tfr("ioread32(0x%p) = 0x%08x (dummy read flushes writes).\n",
			&engine->regs->status, w);
}

/**
 * engine_start() - start an idle engine with its first transfer on queue
 *
 * The engine will run and process all transfers that are queued using
 * transfer_queue() and thus have their descriptor lists chained.
 *
 * During the run, new transfers will be processed if transfer_queue() has
 * chained the descriptors before the hardware fetches the last descriptor.
 * A transfer that was chained too late will invoke a new run of the engine
 * initiated from the engine_service() routine.
 *
 * The engine must be idle and at least one transfer must be queued.
 * This function does not take locks; the engine spinlock must already be
 * taken.
 *
 */
static struct xdma_transfer *engine_start(struct xdma_engine *engine)
{
	struct xdma_transfer *transfer;
	u32 w;
	int extra_adj = 0;

	/* engine must be idle */
	BUG_ON(engine->running);
	/* engine transfer queue must not be empty */
	BUG_ON(list_empty(&engine->transfer_list));
	/* inspect first transfer queued on the engine */
	transfer = list_entry(engine->transfer_list.next, struct xdma_transfer,
				entry);
	BUG_ON(!transfer);

	/* engine is no longer shutdown */
	engine->shutdown = ENGINE_SHUTDOWN_NONE;

	dbg_tfr("engine_start(%s): transfer=0x%p.\n", engine->name, transfer);

	/* initialize number of descriptors of dequeued transfers */
	engine->desc_dequeued = 0;

	/* write lower 32-bit of bus address of transfer first descriptor */
	w = cpu_to_le32(PCI_DMA_L(transfer->desc_bus));
	dbg_tfr("iowrite32(0x%08x to 0x%p) (first_desc_lo)\n", w,
			(void *)&engine->sgdma_regs->first_desc_lo);
	write_register(w, &engine->sgdma_regs->first_desc_lo,
			(unsigned long)(&engine->sgdma_regs->first_desc_lo) -
			(unsigned long)(&engine->sgdma_regs));
	/* write upper 32-bit of bus address of transfer first descriptor */
	w = cpu_to_le32(PCI_DMA_H(transfer->desc_bus));
	dbg_tfr("iowrite32(0x%08x to 0x%p) (first_desc_hi)\n", w,
			(void *)&engine->sgdma_regs->first_desc_hi);
	write_register(w, &engine->sgdma_regs->first_desc_hi,
			(unsigned long)(&engine->sgdma_regs->first_desc_hi) -
			(unsigned long)(&engine->sgdma_regs));

	if (transfer->desc_adjacent > 0) {
		extra_adj = transfer->desc_adjacent - 1;
		if (extra_adj > MAX_EXTRA_ADJ)
			extra_adj = MAX_EXTRA_ADJ;
	}
	dbg_tfr("iowrite32(0x%08x to 0x%p) (first_desc_adjacent)\n",
		extra_adj, (void *)&engine->sgdma_regs->first_desc_adjacent);
	write_register(extra_adj, &engine->sgdma_regs->first_desc_adjacent,
			(unsigned long)(&engine->sgdma_regs->first_desc_adjacent) -
			(unsigned long)(&engine->sgdma_regs));

	dbg_tfr("ioread32(0x%p) (dummy read flushes writes).\n",
		&engine->regs->status);
	mmiowb();

	engine_start_mode_config(engine);

	engine_status_read(engine, 0, 0);

	dbg_tfr("%s engine 0x%p now running\n", engine->name, engine);
	/* remember the engine is running */
	engine->running = 1;
	return transfer;
}

/**
 * engine_service() - service an SG DMA engine
 *
 * must be called with engine->lock already acquired
 *
 * @engine pointer to struct xdma_engine
 *
 */
static void engine_service_shutdown(struct xdma_engine *engine)
{
	/* if the engine stopped with RUN still asserted, de-assert RUN now */
	dbg_tfr("engine just went idle, resetting RUN_STOP.\n");
	xdma_engine_stop(engine);
	engine->running = 0;

	/* awake task on engine's shutdown wait queue */
	wake_up_interruptible(&engine->shutdown_wq);
}

struct xdma_transfer *engine_transfer_completion(struct xdma_engine *engine,
		struct xdma_transfer *transfer)
{
	BUG_ON(!engine);
	BUG_ON(!transfer);

	/* synchronous I/O? */
	/* awake task on transfer's wait queue */
	wake_up_interruptible(&transfer->wq);

	return transfer;
}

struct xdma_transfer *engine_service_transfer_list(struct xdma_engine *engine,
		struct xdma_transfer *transfer, u32 *pdesc_completed)
{
	BUG_ON(!engine);
	BUG_ON(!transfer);
	BUG_ON(!pdesc_completed);

	/*
	 * iterate over all the transfers completed by the engine,
	 * except for the last (i.e. use > instead of >=).
	 */
	while (transfer && (*pdesc_completed > transfer->desc_num)) {
		/* remove this transfer from pdesc_completed */
		*pdesc_completed -= transfer->desc_num;
		dbg_tfr("%s engine completed non-cyclic xfer 0x%p (%d desc)\n",
			engine->name, transfer, transfer->desc_num);
		/* remove completed transfer from list */
		list_del(engine->transfer_list.next);
		/* add to dequeued number of descriptors during this run */
		engine->desc_dequeued += transfer->desc_num;
		/* mark transfer as succesfully completed */
		transfer->state = TRANSFER_STATE_COMPLETED;

		/* Complete transfer - sets transfer to NULL if an async
		 * transfer has completed */
		transfer = engine_transfer_completion(engine, transfer);

		/* if exists, get the next transfer on the list */
		if (!list_empty(&engine->transfer_list)) {
			transfer = list_entry(engine->transfer_list.next,
					struct xdma_transfer, entry);
			dbg_tfr("Non-completed transfer %p\n", transfer);
		} else {
			/* no further transfers? */
			transfer = NULL;
		}
	}

	return transfer;
}

static void engine_err_handle(struct xdma_engine *engine,
		struct xdma_transfer *transfer, u32 desc_completed)
{
	u32 value;

	/*
	 * The BUSY bit is expected to be clear now but older HW has a race
	 * condition which could cause it to be still set.  If it's set, re-read
	 * and check again.  If it's still set, log the issue.
	 */
	if (engine->status & XDMA_STAT_BUSY) {
		value = read_register(&engine->regs->status);
		if (value & XDMA_STAT_BUSY)
			pr_info("%s engine has errors but is still BUSY\n",
				engine->name);
	}

	pr_info("Aborted %s engine transfer 0x%p\n", engine->name, transfer);
	pr_info("%s engine was %d descriptors into transfer (with %d desc)\n",
		engine->name, desc_completed, transfer->desc_num);
	pr_info("%s engine status = %d\n", engine->name, engine->status);
	
	/* mark transfer as failed */
	transfer->state = TRANSFER_STATE_FAILED;
	xdma_engine_stop(engine);
}

struct xdma_transfer *engine_service_final_transfer(struct xdma_engine *engine,
			struct xdma_transfer *transfer, u32 *pdesc_completed)
{
	u32 err_flags;
	BUG_ON(!engine);
	BUG_ON(!transfer);
	BUG_ON(!pdesc_completed);

	err_flags = XDMA_STAT_MAGIC_STOPPED;
	err_flags |= XDMA_STAT_ALIGN_MISMATCH;
	err_flags |= XDMA_STAT_READ_ERROR;
	err_flags |= XDMA_STAT_DESC_ERROR;

	/* inspect the current transfer */
	if (transfer) {
		if (engine->status & err_flags) {
			engine_err_handle(engine, transfer, *pdesc_completed);
			return transfer;
		}

		if (engine->status & XDMA_STAT_BUSY)
			dbg_tfr("Engine %s is unexpectedly busy - ignoring\n",
				engine->name);

		/* the engine stopped on current transfer? */
		if (*pdesc_completed < transfer->desc_num) {
			transfer->state = TRANSFER_STATE_FAILED;
			pr_info("%s, xfer 0x%p, stopped half-way, %d/%d.\n",
				engine->name, transfer, *pdesc_completed,
				transfer->desc_num);
		} else {
			dbg_tfr("engine %s completed transfer\n", engine->name);
			dbg_tfr("Completed transfer ID = 0x%p\n", transfer);
			dbg_tfr("*pdesc_completed=%d, transfer->desc_num=%d",
				*pdesc_completed, transfer->desc_num);

			/*
			 * if the engine stopped on this transfer,
			 * it should be the last
			 */
			WARN_ON(*pdesc_completed > transfer->desc_num);
			/* mark transfer as succesfully completed */
			transfer->state = TRANSFER_STATE_COMPLETED;
		}

		/* remove completed transfer from list */
		list_del(engine->transfer_list.next);
		/* add to dequeued number of descriptors during this run */
		engine->desc_dequeued += transfer->desc_num;

		/*
		 * Complete transfer - sets transfer to NULL if an asynchronous
		 * transfer has completed
		 */
		transfer = engine_transfer_completion(engine, transfer);
	}

	return transfer;
}

static void engine_service_resume(struct xdma_engine *engine)
{
	struct xdma_transfer *transfer_started;

	BUG_ON(!engine);

	/* engine stopped? */
	if (!engine->running) {
		/* in the case of shutdown, let it finish what's in the Q */
		if (!list_empty(&engine->transfer_list)) {
			/* (re)start engine */
			transfer_started = engine_start(engine);
			dbg_tfr("re-started %s engine with pending xfer 0x%p\n",
				engine->name, transfer_started);
		/* engine was requested to be shutdown? */
		} else if (engine->shutdown & ENGINE_SHUTDOWN_REQUEST) {
			engine->shutdown |= ENGINE_SHUTDOWN_IDLE;
			/* awake task on engine's shutdown wait queue */
			wake_up_interruptible(&engine->shutdown_wq);
		} else {
			dbg_tfr("no pending transfers, %s engine stays idle.\n",
				engine->name);
		}
	} else {
		/* engine is still running? */
		if (list_empty(&engine->transfer_list)) {
			pr_warn("no queued transfers but %s engine running!\n",
				engine->name);
			WARN_ON(1);
		}
	}
}

/**
 * engine_service() - service an SG DMA engine
 *
 * must be called with engine->lock already acquired
 *
 * @engine pointer to struct xdma_engine
 *
 */
static int engine_service(struct xdma_engine *engine)
{
	struct xdma_transfer *transfer = NULL;
	u32 desc_count;

	BUG_ON(!engine);

	/* Service the engine */
	if (!engine->running) {
		dbg_tfr("Engine was not running!!! Clearing status\n");
		engine_status_read(engine, 1, 0);
		return 0;
	}

	/*
	 * If called by the ISR or polling detected an error, read and clear
	 * engine status. For polled mode descriptor completion, this read is
	 * unnecessary and is skipped to reduce latency
	 */
	engine_status_read(engine, 1, 0);

	/*
	 * engine was running but is no longer busy, or writeback occurred,
	 * shut down
	 */
	if (engine->running && !(engine->status & XDMA_STAT_BUSY))
		engine_service_shutdown(engine);

	/*
	 * If called from the ISR, or if an error occurred, the descriptor
	 * count will be zero.  In this scenario, read the descriptor count
	 * from HW.  In polled mode descriptor completion, this read is
	 * unnecessary and is skipped to reduce latency
	 */
	desc_count = read_register(&engine->regs->completed_desc_count);

	dbg_tfr("desc_count = %d\n", desc_count);

	/* transfers on queue? */
	if (!list_empty(&engine->transfer_list)) {
		/* pick first transfer on queue (was submitted to the engine) */
		transfer = list_entry(engine->transfer_list.next,
				struct xdma_transfer, entry);

		dbg_tfr("head of queue transfer 0x%p has %d descriptors\n",
			transfer, (int)transfer->desc_num);

		dbg_tfr("Engine completed %d desc, %d not yet dequeued\n",
			(int)desc_count,
			(int)desc_count - engine->desc_dequeued);
	}

	/* account for already dequeued transfers during this engine run */
	desc_count -= engine->desc_dequeued;

	/* Process all but the last transfer */
	transfer = engine_service_transfer_list(engine, transfer, &desc_count);

	/*
	 * Process final transfer - includes checks of number of descriptors to
	 * detect faulty completion
	 */
	transfer = engine_service_final_transfer(engine, transfer, &desc_count);

	/* Restart the engine following the servicing */
	engine_service_resume(engine);

	return 0;
}

/* engine_service_work */
static void engine_service_work(struct work_struct *work)
{
	struct xdma_engine *engine;

	engine = container_of(work, struct xdma_engine, work);
	BUG_ON(engine->magic != MAGIC_ENGINE);

	/* lock the engine */
	spin_lock(&engine->lock);

	dbg_tfr("engine_service() for %s engine %p\n",
		engine->name, engine);
	engine_service(engine);

	/* re-enable interrupts for this engine */
	if(engine->xdev->msix_enabled){
		write_register(engine->interrupt_enable_mask_value,
			       &engine->regs->interrupt_enable_mask_w1s,
			(unsigned long)(&engine->regs->interrupt_enable_mask_w1s) -
			(unsigned long)(&engine->regs));
	}else{
		channel_interrupts_enable(engine->xdev, engine->irq_bitmask);
	}
	/* unlock the engine */
	spin_unlock(&engine->lock);
}

static irqreturn_t user_irq_service(int irq, struct xdma_user_irq *user_irq)
{
	unsigned long flags;

	BUG_ON(!user_irq);

	if (user_irq->handler)
		return user_irq->handler(user_irq->user_idx, user_irq->dev);

	spin_lock_irqsave(&(user_irq->events_lock), flags);
	if (!user_irq->events_irq) {
		user_irq->events_irq = 1;
		wake_up_interruptible(&(user_irq->events_wq));
	}
	spin_unlock_irqrestore(&(user_irq->events_lock), flags);

	return IRQ_HANDLED;
}

/*
 * xdma_isr() - Interrupt handler
 *
 * @dev_id pointer to xdma_dev
 */
static irqreturn_t xdma_isr(int irq, void *dev_id)
{
	u32 ch_irq;
	u32 user_irq;
	struct xdma_dev *xdev;
	struct interrupt_regs *irq_regs;
	int user_irq_bit;
	struct xdma_engine *engine;
	int channel;

	dbg_irq("(irq=%d) <<<< INTERRUPT SERVICE ROUTINE\n", irq);
	BUG_ON(!dev_id);
	xdev = (struct xdma_dev *)dev_id;

	if (!xdev) {
		WARN_ON(!xdev);
		dbg_irq("xdma_isr(irq=%d) xdev=%p ??\n", irq, xdev);
		return IRQ_NONE;
	}

	irq_regs = (struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
			XDMA_OFS_INT_CTRL);

	/* read channel interrupt requests */
	ch_irq = read_register(&irq_regs->channel_int_request);
	dbg_irq("ch_irq = 0x%08x\n", ch_irq);

	/*
	 * disable all interrupts that fired; these are re-enabled individually
	 * after the causing module has been fully serviced.
	 */
	channel_interrupts_disable(xdev, ch_irq);

	/* read user interrupts - this read also flushes the above write */
	user_irq = read_register(&irq_regs->user_int_request);
	dbg_irq("user_irq = 0x%08x\n", user_irq);

	for (user_irq_bit = 0; user_irq_bit < xdev->user_max; user_irq_bit++) {
		if (user_irq & (1 << user_irq_bit))
			user_irq_service(irq, &xdev->user_irq[user_irq_bit]);
	}

	/* iterate over H2C (PCIe read) */
	for (channel = 0; channel < xdev->channel_max; channel++) {
		engine = &xdev->engine_h2c[channel];
		/* engine present and its interrupt fired? */
		if ((engine->magic == MAGIC_ENGINE) &&
		    (engine->irq_bitmask & ch_irq)) {
			dbg_tfr("schedule_work(engine=%p)\n", engine);
			schedule_work(&engine->work);
		}
	}

	/* iterate over C2H (PCIe write) */
	for (channel = 0; channel < xdev->channel_max; channel++) {
		engine = &xdev->engine_c2h[channel];
		/* engine present and its interrupt fired? */
		if ((engine->magic == MAGIC_ENGINE) &&
		    (engine->irq_bitmask & ch_irq)) {
			dbg_tfr("schedule_work(engine=%p)\n", engine);
			schedule_work(&engine->work);
		}
	}

	xdev->irq_count++;
	return IRQ_HANDLED;
}

/*
 * xdma_user_irq() - Interrupt handler for user interrupts in MSI-X mode
 *
 * @dev_id pointer to xdma_dev
 */
static irqreturn_t xdma_user_irq(int irq, void *dev_id)
{
	struct xdma_user_irq *user_irq;

	dbg_irq("(irq=%d) <<<< INTERRUPT SERVICE ROUTINE\n", irq);

	BUG_ON(!dev_id);
	user_irq = (struct xdma_user_irq *)dev_id;

	return  user_irq_service(irq, user_irq);
}

/*
 * xdma_channel_irq() - Interrupt handler for channel interrupts in MSI-X mode
 *
 * @dev_id pointer to xdma_dev
 */
static irqreturn_t xdma_channel_irq(int irq, void *dev_id)
{
	struct xdma_dev *xdev;
	struct xdma_engine *engine;
	struct interrupt_regs *irq_regs;

	dbg_irq("(irq=%d) <<<< INTERRUPT service ROUTINE\n", irq);
	BUG_ON(!dev_id);

	engine = (struct xdma_engine *)dev_id;
	xdev = engine->xdev;

	if (!xdev) {
		WARN_ON(!xdev);
		dbg_irq("xdma_channel_irq(irq=%d) xdev=%p ??\n", irq, xdev);
		return IRQ_NONE;
	}

	irq_regs = (struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
			XDMA_OFS_INT_CTRL);

	/* Disable the interrupt for this engine */
	//channel_interrupts_disable(xdev, engine->irq_bitmask);
	engine->interrupt_enable_mask_value = read_register(
			&engine->regs->interrupt_enable_mask);
	write_register(engine->interrupt_enable_mask_value,
			&engine->regs->interrupt_enable_mask_w1c,
			(unsigned long)
			(&engine->regs->interrupt_enable_mask_w1c) -
			(unsigned long)(&engine->regs));
	/* Dummy read to flush the above write */
	read_register(&irq_regs->channel_int_pending);
	/* Schedule the bottom half */
	schedule_work(&engine->work);

	/*
	 * RTO - need to protect access here if multiple MSI-X are used for
	 * user interrupts
	 */
	xdev->irq_count++;
	return IRQ_HANDLED;
}

/*
 * Unmap the BAR regions that had been mapped earlier using map_bars()
 */
static void unmap_bars(struct xdma_dev *xdev, struct pci_dev *dev)
{
	int i;

	for (i = 0; i < XDMA_BAR_NUM; i++) {
		/* is this BAR mapped? */
		if (xdev->bar[i]) {
			/* unmap BAR */
			pci_iounmap(dev, xdev->bar[i]);
			/* mark as unmapped */
			xdev->bar[i] = NULL;
		}
	}
}

static int map_single_bar(struct xdma_dev *xdev, struct pci_dev *dev, int idx)
{
	resource_size_t bar_start;
	resource_size_t bar_len;
	resource_size_t map_len;

	bar_start = pci_resource_start(dev, idx);
	bar_len = pci_resource_len(dev, idx);
	map_len = bar_len;

	xdev->bar[idx] = NULL;

	/* do not map BARs with length 0. Note that start MAY be 0! */
	if (!bar_len) {
		//pr_info("BAR #%d is not present - skipping\n", idx);
		return 0;
	}

	/* BAR size exceeds maximum desired mapping? */
	if (bar_len > INT_MAX) {
		pr_info("Limit BAR %d mapping from %llu to %d bytes\n", idx,
			(u64)bar_len, INT_MAX);
		map_len = (resource_size_t)INT_MAX;
	}
	/*
	 * map the full device memory or IO region into kernel virtual
	 * address space
	 */
	dbg_init("BAR%d: %llu bytes to be mapped.\n", idx, (u64)map_len);
	xdev->bar[idx] = pci_iomap(dev, idx, map_len);

	if (!xdev->bar[idx]) {
		pr_info("Could not map BAR %d.\n", idx);
		return -1;
	}

	pr_info("BAR%d at 0x%llx mapped at 0x%p, length=%llu(/%llu)\n", idx,
		(u64)bar_start, xdev->bar[idx], (u64)map_len, (u64)bar_len);

	return (int)map_len;
}

static int is_config_bar(struct xdma_dev *xdev, int idx)
{
	u32 irq_id = 0;
	u32 cfg_id = 0;
	int flag = 0;
	u32 mask = 0xffff0000; /* Compare only XDMA ID's not Version number */
	struct interrupt_regs *irq_regs =
		(struct interrupt_regs *) (xdev->bar[idx] + XDMA_OFS_INT_CTRL);
	struct config_regs *cfg_regs =
		(struct config_regs *)(xdev->bar[idx] + XDMA_OFS_CONFIG);

	irq_id = read_register(&irq_regs->identifier);
	cfg_id = read_register(&cfg_regs->identifier);

	if (((irq_id & mask)== IRQ_BLOCK_ID) && ((cfg_id & mask)== CONFIG_BLOCK_ID)) {
		pr_info("BAR %d is the XDMA config BAR\n", idx);
		flag = 1;
	} else {
		dbg_init("BAR %d is NOT the XDMA config BAR: 0x%x, 0x%x.\n", idx, irq_id, cfg_id);
		flag = 0;
	}

	return flag;
}

static void identify_bars(struct xdma_dev *xdev, int *bar_id_list, int num_bars,
			int config_bar_pos)
{
	/*
	 * The following logic identifies which BARs contain what functionality
	 * based on the position of the XDMA config BAR and the number of BARs
	 * detected. The rules are that the user logic and bypass logic BARs
	 * are optional.  When both are present, the XDMA config BAR will be the
	 * 2nd BAR detected (config_bar_pos = 1), with the user logic being
	 * detected first and the bypass being detected last. When one is
	 * omitted, the type of BAR present can be identified by whether the
	 * XDMA config BAR is detected first or last.  When both are omitted,
	 * only the XDMA config BAR is present.  This somewhat convoluted
	 * approach is used instead of relying on BAR numbers in order to work
	 * correctly with both 32-bit and 64-bit BARs.
	 */

	BUG_ON(!xdev);
	BUG_ON(!bar_id_list);

	dbg_init("xdev 0x%p, bars %d, config at %d.\n",
		xdev, num_bars, config_bar_pos);

	switch (num_bars) {
	case 1:
		/* Only one BAR present - no extra work necessary */
		break;

	case 2:
		if (config_bar_pos == 0) {
			xdev->bypass_bar_idx = bar_id_list[1];
		} else if (config_bar_pos == 1) {
			xdev->user_bar_idx = bar_id_list[0];
		} else {
			pr_info("2, XDMA config BAR unexpected %d.\n",
				config_bar_pos);
		}
		break;

	case 3:
	case 4:
		if ((config_bar_pos == 1) || (config_bar_pos == 2)) {
			/* user bar at bar #0 */
			xdev->user_bar_idx = bar_id_list[0];
			/* bypass bar at the last bar */
			xdev->bypass_bar_idx = bar_id_list[num_bars - 1];
		} else {
			pr_info("3/4, XDMA config BAR unexpected %d.\n",
				config_bar_pos);
		}
		break;

	default:
		/* Should not occur - warn user but safe to continue */
		pr_info("Unexpected number of BARs (%d), XDMA config BAR only.\n", num_bars);
		break;

	}
	pr_info("%d BARs: config %d, user %d, bypass %d.\n",
		num_bars, config_bar_pos, xdev->user_bar_idx,
		xdev->bypass_bar_idx);
}

/* map_bars() -- map device regions into kernel virtual address space
 *
 * Map the device memory regions into kernel virtual address space after
 * verifying their sizes respect the minimum sizes needed
 */
static int map_bars(struct xdma_dev *xdev, struct pci_dev *dev)
{
	int rv;
	int i;
	int bar_id_list[XDMA_BAR_NUM];
	int bar_id_idx = 0;
	int config_bar_pos = 0;

	/* iterate through all the BARs */
	for (i = 0; i < XDMA_BAR_NUM; i++) {
		int bar_len;

		bar_len = map_single_bar(xdev, dev, i);
		if (bar_len == 0) {
			continue;
		} else if (bar_len < 0) {
			rv = -EINVAL;
			goto fail;
		}

		/* Try to identify BAR as XDMA control BAR */
		if ((bar_len >= XDMA_BAR_SIZE) && (xdev->config_bar_idx < 0)) {

			if (is_config_bar(xdev, i)) {
				xdev->config_bar_idx = i;
				config_bar_pos = bar_id_idx;
				pr_info("config bar %d, pos %d.\n",
					xdev->config_bar_idx, config_bar_pos);
			}
		}

		bar_id_list[bar_id_idx] = i;
		bar_id_idx++;
	}

	/* The XDMA config BAR must always be present */
	if (xdev->config_bar_idx < 0) {
		pr_info("Failed to detect XDMA config BAR\n");
		rv = -EINVAL;
		goto fail;
	}

	identify_bars(xdev, bar_id_list, bar_id_idx, config_bar_pos);

	/* successfully mapped all required BAR regions */
	return 0;

fail:
	/* unwind; unmap any BARs that we did map */
	unmap_bars(xdev, dev);
	return rv;
}

/*
 * MSI-X interrupt: 
 *	2*<channel_max> vectors, followed by <user_max> vectors
 */

/*
 * RTO - code to detect if MSI/MSI-X capability exists is derived
 * from linux/pci/msi.c - pci_msi_check_device
 */

#ifndef arch_msi_check_device
int arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
{
	return 0;
}
#endif

/* type = PCI_CAP_ID_MSI or PCI_CAP_ID_MSIX */
static int msi_msix_capable(struct pci_dev *dev, int type)
{
	struct pci_bus *bus;
	int ret;

	if (!dev || dev->no_msi)
		return 0;

	for (bus = dev->bus; bus; bus = bus->parent)
		if (bus->bus_flags & PCI_BUS_FLAGS_NO_MSI)
			return 0;

	ret = arch_msi_check_device(dev, 1, type);
	if (ret)
		return 0;

	if (!pci_find_capability(dev, type))
		return 0;

	return 1;
}
static void disable_msi_msix(struct xdma_dev *xdev, struct pci_dev *pdev)
{
	if (xdev->msix_enabled) {
		pci_disable_msix(pdev);
		xdev->msix_enabled = 0;
	} else if (xdev->msi_enabled) {
		pci_disable_msi(pdev);
		xdev->msi_enabled = 0;
	}
}

static int enable_msi_msix(struct xdma_dev *xdev, struct pci_dev *pdev)
{
	int rv = 0;

	BUG_ON(!xdev);
	BUG_ON(!pdev);

	if (msi_msix_capable(pdev, PCI_CAP_ID_MSIX)) {
		int req_nvec = (xdev->channel_max << 1) + xdev->user_max;
		int i;

		dbg_init("Enabling MSI-X\n");
		for (i = 0; i < req_nvec; i++)
			xdev->entry[i].entry = i;

		rv = pci_enable_msix(pdev, xdev->entry, req_nvec);
		if (rv < 0)
			dbg_init("Couldn't enable MSI-X mode: %d\n", rv);

		xdev->msix_enabled = 1;

	} else if (msi_msix_capable(pdev, PCI_CAP_ID_MSI)) {
		/* enable message signalled interrupts */
		dbg_init("pci_enable_msi()\n");
		rv = pci_enable_msi(pdev);
		if (rv < 0)
			dbg_init("Couldn't enable MSI mode: %d\n", rv);
		xdev->msi_enabled = 1;

	} else {
		dbg_init("MSI/MSI-X not detected - using legacy interrupts\n");
	}

	return rv;
}

static void prog_irq_msix_user(struct xdma_dev *xdev)
{
	/* user */
	struct interrupt_regs *int_regs = (struct interrupt_regs *)
					(xdev->bar[xdev->config_bar_idx] +
					 XDMA_OFS_INT_CTRL);
	u32 max = xdev->user_max;
	u32 i;
	int j;


	for (i = 0, j = 0; i < max; j++) {
		u32 val = 0;
		int k;
		int shift = 0;

		for (k = 0; k < 4 && i < max; i++, k++, shift += 8)
			val |= (i & 0x1f) << shift;
		
		write_register(val, &int_regs->user_msi_vector[j],
			XDMA_OFS_INT_CTRL +
			((unsigned long)&int_regs->user_msi_vector[j] -
			 (unsigned long)int_regs));
	}
}

static void prog_irq_msix_channel(struct xdma_dev *xdev)
{
	struct interrupt_regs *int_regs = (struct interrupt_regs *)
					(xdev->bar[xdev->config_bar_idx] +
					 XDMA_OFS_INT_CTRL);
	u32 max = xdev->channel_max * 2;
	u32 i;
	int j;

	/* engine */
	for (i = 0, j = 0; i < max; j++) {
		u32 val = 0;
		int k;
		int shift = 0;

		for (k = 0; k < 4 && i < max; i++, k++, shift += 8)
			val |= (i & 0x1f) << shift;
		
		write_register(val, &int_regs->channel_msi_vector[j],
			XDMA_OFS_INT_CTRL +
			((unsigned long)&int_regs->channel_msi_vector[j] -
			 (unsigned long)int_regs));
	}
}

static void irq_msix_channel_teardown(struct xdma_dev *xdev)
{
	struct xdma_engine *engine;
	int j = 0;
	int i = 0;

	if (!xdev->msix_enabled)
		return;

 	engine = xdev->engine_h2c;
	for (i = 0; i < xdev->channel_max; i++, j++, engine++) {
		if (!engine->msix_irq_line)
			break;
		dbg_sg("Release IRQ#%d for engine %p\n", engine->msix_irq_line,
			engine);
		free_irq(engine->msix_irq_line, engine);
	}

 	engine = xdev->engine_c2h;
	for (i = 0; i < xdev->channel_max; i++, j++, engine++) {
		if (!engine->msix_irq_line)
			break;
		dbg_sg("Release IRQ#%d for engine %p\n", engine->msix_irq_line,
			engine);
		free_irq(engine->msix_irq_line, engine);
	}
}

static int irq_msix_channel_setup(struct xdma_dev *xdev)
{
	int i;
	int j = xdev->channel_max;
	int rv = 0;
	u32 vector;
	struct xdma_engine *engine;

	BUG_ON(!xdev);
	if (!xdev->msix_enabled)
		return 0;

	engine = xdev->engine_h2c;
	for (i = 0; i < xdev->channel_max; i++, engine++) {
		vector = xdev->entry[i].vector;
		rv = request_irq(vector, xdma_channel_irq, 0, xdev->mod_name,
				 engine);
		if (rv) {
			pr_info("requesti irq#%d failed %d, engine %s.\n",
				vector, rv, engine->name);
			return rv;
		}
		pr_info("engine %s, irq#%d.\n", engine->name, vector);
		engine->msix_irq_line = vector;
	}

	engine = xdev->engine_c2h;
	for (i = 0; i < xdev->channel_max; i++, j++, engine++) {
		vector = xdev->entry[j].vector;
		rv = request_irq(vector, xdma_channel_irq, 0, xdev->mod_name,
				 engine);
		if (rv) {
			pr_info("requesti irq#%d failed %d, engine %s.\n",
				vector, rv, engine->name);
			return rv;
		}
		pr_info("engine %s, irq#%d.\n", engine->name, vector);
		engine->msix_irq_line = vector;
	}

	return 0;
}

static void irq_msix_user_teardown(struct xdma_dev *xdev)
{
	int i;
	int j = xdev->channel_max << 1;

	BUG_ON(!xdev);

	if (!xdev->msix_enabled)
		return;

	for (i = 0; i < xdev->user_max; i++, j++) {
		dbg_init("user %d, releasing IRQ#%d\n",
			i, xdev->entry[j].vector);
		free_irq(xdev->entry[j].vector, &xdev->user_irq[i]);
	}
}

static int irq_msix_user_setup(struct xdma_dev *xdev)
{
	int i;
	int j = xdev->channel_max << 1;
	int rv = 0;	

	/* vectors set in probe_scan_for_msi() */
	for (i = 0; i < xdev->user_max; i++, j++) {
		rv = request_irq(xdev->entry[j].vector, xdma_user_irq, 0,
				xdev->mod_name, &xdev->user_irq[i]);
		if (rv) {
			dbg_init("user %d couldn't use IRQ#%d, %d\n",
				i, xdev->entry[j].vector, rv);
			break;
		}
		dbg_init("user %d, IRQ#%d with 0x%p\n",
			i, xdev->entry[j].vector, &xdev->user_irq[i]);
        }

	/* If any errors occur, free IRQs that were successfully requested */
	if (rv) {
		for (i--, j--; i >= 0; i--, j--) 
			free_irq(xdev->entry[j].vector, &xdev->user_irq[i]);
	}

	return rv;
}

static int irq_msi_setup(struct xdma_dev *xdev, struct pci_dev *pdev)
{
	int rv;

	xdev->irq_line = (int)pdev->irq;
	rv = request_irq(pdev->irq, xdma_isr, 0, xdev->mod_name, xdev);
	if (rv)
		dbg_init("Couldn't use IRQ#%d, %d\n", pdev->irq, rv);
	else
		dbg_init("Using IRQ#%d with 0x%p\n", pdev->irq, xdev);

	return rv;
}

static int irq_legacy_setup(struct xdma_dev *xdev, struct pci_dev *pdev)
{
	u32 w;
	u8 val;
	void *reg;
	int rv;

	pci_read_config_byte(pdev, PCI_INTERRUPT_PIN, &val);
	dbg_init("Legacy Interrupt register value = %d\n", val);
	if (val > 1) {
		val--;
		w = (val<<24) | (val<<16) | (val<<8)| val;
		/* Program IRQ Block Channel vactor and IRQ Block User vector
		 * with Legacy interrupt value */
		reg = xdev->bar[xdev->config_bar_idx] + 0x2080;   // IRQ user
		write_register(w, reg, 0x2080);
		write_register(w, reg+0x4, 0x2084);
		write_register(w, reg+0x8, 0x2088);
		write_register(w, reg+0xC, 0x208C);
		reg = xdev->bar[xdev->config_bar_idx] + 0x20A0;   // IRQ Block
		write_register(w, reg, 0x20A0);
		write_register(w, reg+0x4, 0x20A4);
	}

	xdev->irq_line = (int)pdev->irq;
	rv = request_irq(pdev->irq, xdma_isr, IRQF_SHARED, xdev->mod_name,
			xdev);
	if (rv)
		dbg_init("Couldn't use IRQ#%d, %d\n", pdev->irq, rv);
	else
		dbg_init("Using IRQ#%d with 0x%p\n", pdev->irq, xdev);

	return rv;
}

static void irq_teardown(struct xdma_dev *xdev)
{
	if (xdev->msix_enabled) {
		irq_msix_channel_teardown(xdev);
		irq_msix_user_teardown(xdev);
	} else if (xdev->irq_line != -1) {
		dbg_init("Releasing IRQ#%d\n", xdev->irq_line);
		free_irq(xdev->irq_line, xdev);
	}
}

static int irq_setup(struct xdma_dev *xdev, struct pci_dev *pdev)
{
	if (xdev->msix_enabled) {
		int rv = irq_msix_channel_setup(xdev);
		if (rv)
			return rv;
		rv = irq_msix_user_setup(xdev);
		if (rv)
			return rv;
		prog_irq_msix_channel(xdev);
		prog_irq_msix_user(xdev);

		return 0;
	} else if (xdev->msi_enabled)
		return irq_msi_setup(xdev, pdev);

	return irq_legacy_setup(xdev, pdev);
}

static void dump_desc(struct xdma_desc *desc_virt)
{
	int j;
	u32 *p = (u32 *)desc_virt;
	static char * const field_name[] = {
		"magic|extra_adjacent|control", "bytes", "src_addr_lo",
		"src_addr_hi", "dst_addr_lo", "dst_addr_hi", "next_addr",
		"next_addr_pad"};
	char *dummy;

	/* remove warning about unused variable when debug printing is off */
	dummy = field_name[0];

	for (j = 0; j < 8; j += 1) {
		dbg_desc("0x%08lx/0x%02lx: 0x%08x 0x%08x %s\n",
			 (uintptr_t)p, (uintptr_t)p & 15, (int)*p,
			 le32_to_cpu(*p), field_name[j]);
		p++;
	}
	dbg_desc("\n");
}

static void transfer_dump(struct xdma_transfer *transfer)
{
	int i;
	struct xdma_desc *desc_virt = transfer->desc_virt;

	dbg_desc("Descriptor Entry (Pre-Transfer)\n");
	for (i = 0; i < transfer->desc_num; i += 1)
		dump_desc(desc_virt + i);
}

/* xdma_desc_alloc() - Allocate cache-coherent array of N descriptors.
 *
 * Allocates an array of 'number' descriptors in contiguous PCI bus addressable
 * memory. Chains the descriptors as a singly-linked list; the descriptor's
 * next * pointer specifies the bus address of the next descriptor.
 *
 *
 * @dev Pointer to pci_dev
 * @number Number of descriptors to be allocated
 * @desc_bus_p Pointer where to store the first descriptor bus address
 *
 * @return Virtual address of the first descriptor
 *
 */
static struct xdma_desc *xdma_desc_alloc(struct pci_dev *pdev, int number,
		dma_addr_t *desc_bus_p)
{
	struct xdma_desc *desc_virt;	/* virtual address */
	dma_addr_t desc_bus;		/* bus address */
	int i;
	int adj = number - 1;
	int extra_adj;
	u32 temp_control;

	BUG_ON(number < 1);

	/* allocate a set of cache-coherent contiguous pages */
	desc_virt = (struct xdma_desc *)dma_alloc_coherent(&pdev->dev,
				number * sizeof(struct xdma_desc), desc_bus_p,
				GFP_KERNEL);
	if (!desc_virt) {
		pr_err("dma_alloc_coherent failed, pdev 0x%p, %d*%ld.\n",
			pdev, number, sizeof(struct xdma_desc)); 
		return NULL;
	}
	/* get bus address of the first descriptor */
	desc_bus = *desc_bus_p;

	/* create singly-linked list for SG DMA controller */
	for (i = 0; i < number - 1; i++) {
		/* increment bus address to next in array */
		desc_bus += sizeof(struct xdma_desc);

		/* singly-linked list uses bus addresses */
		desc_virt[i].next_lo = cpu_to_le32(PCI_DMA_L(desc_bus));
		desc_virt[i].next_hi = cpu_to_le32(PCI_DMA_H(desc_bus));
		desc_virt[i].bytes = cpu_to_le32(0);

		/* any adjacent descriptors? */
		if (adj > 0) {
			extra_adj = adj - 1;
			if (extra_adj > MAX_EXTRA_ADJ)
				extra_adj = MAX_EXTRA_ADJ;

			adj--;
		} else {
			extra_adj = 0;
		}

		temp_control = DESC_MAGIC | (extra_adj << 8);

		desc_virt[i].control = cpu_to_le32(temp_control);
	}
	/* { i = number - 1 } */
	/* zero the last descriptor next pointer */
	desc_virt[i].next_lo = cpu_to_le32(0);
	desc_virt[i].next_hi = cpu_to_le32(0);
	desc_virt[i].bytes = cpu_to_le32(0);

	temp_control = DESC_MAGIC;

	desc_virt[i].control = cpu_to_le32(temp_control);

	/* return the virtual address of the first descriptor */
	return desc_virt;
}

/* xdma_desc_link() - Link two descriptors
 *
 * Link the first descriptor to a second descriptor, or terminate the first.
 *
 * @first first descriptor
 * @second second descriptor, or NULL if first descriptor must be set as last.
 * @second_bus bus address of second descriptor
 */
static void xdma_desc_link(struct xdma_desc *first, struct xdma_desc *second,
		dma_addr_t second_bus)
{
	/*
	 * remember reserved control in first descriptor, but zero
	 * extra_adjacent!
	 */
	 /* RTO - what's this about?  Shouldn't it be 0x0000c0ffUL? */
	u32 control = le32_to_cpu(first->control) & 0x0000f0ffUL;
	/* second descriptor given? */
	if (second) {
		/*
		 * link last descriptor of 1st array to first descriptor of
		 * 2nd array
		 */
		first->next_lo = cpu_to_le32(PCI_DMA_L(second_bus));
		first->next_hi = cpu_to_le32(PCI_DMA_H(second_bus));
		WARN_ON(first->next_hi);
		/* no second descriptor given */
	} else {
		/* first descriptor is the last */
		first->next_lo = 0;
		first->next_hi = 0;
	}
	/* merge magic, extra_adjacent and control field */
	control |= DESC_MAGIC;

	/* write bytes and next_num */
	first->control = cpu_to_le32(control);
}

/* xdma_desc_adjacent -- Set how many descriptors are adjacent to this one */
static void xdma_desc_adjacent(struct xdma_desc *desc, int next_adjacent)
{
	int extra_adj = 0;
	/* remember reserved and control bits */
	u32 control = le32_to_cpu(desc->control) & 0x0000f0ffUL;
	u32 max_adj_4k = 0;

	if (next_adjacent > 0) {
		extra_adj =  next_adjacent - 1;
		if (extra_adj > MAX_EXTRA_ADJ){
			extra_adj = MAX_EXTRA_ADJ;
		}
		max_adj_4k = (0x1000 - ((le32_to_cpu(desc->next_lo))&0xFFF))/32 - 1;
		if (extra_adj>max_adj_4k) {
			extra_adj = max_adj_4k;
		}
		if(extra_adj<0){
			printk("Warning: extra_adj<0, converting it to 0\n");
			extra_adj = 0;
		}
	}
	/* merge adjacent and control field */
	control |= 0xAD4B0000UL | (extra_adj << 8);
	/* write control and next_adjacent */
	desc->control = cpu_to_le32(control);
}

/* xdma_desc_control -- Set complete control field of a descriptor. */
static void xdma_desc_control(struct xdma_desc *first, u32 control_field)
{
	/* remember magic and adjacent number */
	u32 control = le32_to_cpu(first->control) & ~(LS_BYTE_MASK);

	BUG_ON(control_field & ~(LS_BYTE_MASK));
	/* merge adjacent and control field */
	control |= control_field;
	/* write control and next_adjacent */
	first->control = cpu_to_le32(control);
}

/* xdma_desc_free - Free cache-coherent linked list of N descriptors.
 *
 * @dev Pointer to pci_dev
 * @number Number of descriptors to be allocated
 * @desc_virt Pointer to (i.e. virtual address of) first descriptor in list
 * @desc_bus Bus address of first descriptor in list
 */
static void xdma_desc_free(struct pci_dev *pdev, int number,
		struct xdma_desc *desc_virt, dma_addr_t desc_bus)
{
	BUG_ON(!desc_virt);
	BUG_ON(number < 0);
	/* free contiguous list */
	dma_free_coherent(&pdev->dev, number * sizeof(struct xdma_desc),
			desc_virt, desc_bus);
}

/* xdma_desc() - Fill a descriptor with the transfer details
 *
 * @desc pointer to descriptor to be filled
 * @addr root complex address
 * @ep_addr end point address
 * @len number of bytes, must be a (non-negative) multiple of 4.
 * @dir, dma direction
 * is the end point address. If zero, vice versa.
 *
 * Does not modify the next pointer
 */
static void xdma_desc_set(struct xdma_desc *desc, dma_addr_t rc_bus_addr,
		u64 ep_addr, int len, int dir)
{
	/* transfer length */
	desc->bytes = cpu_to_le32(len);
	if (dir == DMA_TO_DEVICE) {
		/* read from root complex memory (source address) */
		desc->src_addr_lo = cpu_to_le32(PCI_DMA_L(rc_bus_addr));
		desc->src_addr_hi = cpu_to_le32(PCI_DMA_H(rc_bus_addr));
		/* write to end point address (destination address) */
		desc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(ep_addr));
		desc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(ep_addr));
	} else {
		/* read from end point address (source address) */
		desc->src_addr_lo = cpu_to_le32(PCI_DMA_L(ep_addr));
		desc->src_addr_hi = cpu_to_le32(PCI_DMA_H(ep_addr));
		/* write to root complex memory (destination address) */
		desc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(rc_bus_addr));
		desc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(rc_bus_addr));
	}
}

/* transfer_queue() - Queue a DMA transfer on the engine
 *
 * @engine DMA engine doing the transfer
 * @transfer DMA transfer submitted to the engine
 *
 * Takes and releases the engine spinlock
 */
static int transfer_queue(struct xdma_engine *engine,
		struct xdma_transfer *transfer)
{
	int rv = 0;
	struct xdma_transfer *transfer_started;
	struct xdma_dev *xdev;

	BUG_ON(!engine);
	BUG_ON(!engine->xdev);
	BUG_ON(!transfer);
	BUG_ON(transfer->desc_num == 0);
	dbg_tfr("transfer_queue(transfer=0x%p).\n", transfer);

	xdev = engine->xdev;
	if (xdma_device_flag_check(xdev, XDEV_FLAG_OFFLINE)) {
		pr_info("dev 0x%p offline, transfer 0x%p not queued.\n",
			xdev, transfer);
		return -EBUSY;
	}

	/* lock the engine state */
	spin_lock(&engine->lock);
	engine->prev_cpu = get_cpu();
	put_cpu();

	/* engine is being shutdown; do not accept new transfers */
	if (engine->shutdown & ENGINE_SHUTDOWN_REQUEST) {
		pr_info("engine %s offline, transfer 0x%p not queued.\n",
			engine->name, transfer);
		rv = -EBUSY;
		goto shutdown;
	}

	/* mark the transfer as submitted */
	transfer->state = TRANSFER_STATE_SUBMITTED;
	/* add transfer to the tail of the engine transfer queue */
	list_add_tail(&transfer->entry, &engine->transfer_list);

	/* engine is idle? */
	if (!engine->running) {
		/* start engine */
		dbg_tfr("transfer_queue(): starting %s engine.\n",
			engine->name);
		transfer_started = engine_start(engine);
		dbg_tfr("transfer=0x%p started %s engine with transfer 0x%p.\n",
			transfer, engine->name, transfer_started);
	} else {
		dbg_tfr("transfer=0x%p queued, with %s engine running.\n",
			transfer, engine->name);
	}

shutdown:
	/* unlock the engine state */
	dbg_tfr("engine->running = %d\n", engine->running);
	spin_unlock(&engine->lock);
	return rv;
}

static void engine_alignments(struct xdma_engine *engine)
{
	u32 w;
	u32 align_bytes;
	u32 granularity_bytes;
	u32 address_bits;

	w = read_register(&engine->regs->alignments);
	dbg_init("engine %p name %s alignments=0x%08x\n", engine,
		engine->name, (int)w);

	/* RTO  - add some macros to extract these fields */
	align_bytes = (w & 0x00ff0000U) >> 16;
	granularity_bytes = (w & 0x0000ff00U) >> 8;
	address_bits = (w & 0x000000ffU);

	dbg_init("align_bytes = %d\n", align_bytes);
	dbg_init("granularity_bytes = %d\n", granularity_bytes);
	dbg_init("address_bits = %d\n", address_bits);

	if (w) {
		engine->addr_align = align_bytes;
		engine->len_granularity = granularity_bytes;
		engine->addr_bits = address_bits;
	} else {
		/* Some default values if alignments are unspecified */
		engine->addr_align = 1;
		engine->len_granularity = 1;
		engine->addr_bits = 64;
	}
}

static void engine_destroy(struct xdma_dev *xdev, struct xdma_engine *engine)
{
	BUG_ON(!xdev);
	BUG_ON(!engine);

	dbg_sg("Shutting down engine %s%d", engine->name, engine->channel);

	/* Disable interrupts to stop processing new events during shutdown */
	write_register(0x0, &engine->regs->interrupt_enable_mask,
			(unsigned long)(&engine->regs->interrupt_enable_mask) -
			(unsigned long)(&engine->regs));

	memset(engine, 0, sizeof(struct xdma_engine));
	/* Decrement the number of engines available */
	xdev->engines_num--;
}

/* engine_create() - Create an SG DMA engine bookkeeping data structure
 *
 * An SG DMA engine consists of the resources for a single-direction transfer
 * queue; the SG DMA hardware, the software queue and interrupt handling.
 *
 * @dev Pointer to pci_dev
 * @offset byte address offset in BAR[xdev->config_bar_idx] resource for the
 * SG DMA * controller registers.
 * @dir: DMA_TO/FROM_DEVICE
 * @streaming Whether the engine is attached to AXI ST (rather than MM)
 */
static void engine_init_regs(struct xdma_engine *engine)
{
	u32 reg_value;

	write_register(XDMA_CTRL_NON_INCR_ADDR, &engine->regs->control_w1c,
			(unsigned long)(&engine->regs->control_w1c) -
			(unsigned long)(&engine->regs));

	engine_alignments(engine);

	/* Configure error interrupts by default */
	reg_value = XDMA_CTRL_IE_DESC_ALIGN_MISMATCH;
	reg_value |= XDMA_CTRL_IE_MAGIC_STOPPED;
	reg_value |= XDMA_CTRL_IE_MAGIC_STOPPED;
	reg_value |= XDMA_CTRL_IE_READ_ERROR;
	reg_value |= XDMA_CTRL_IE_DESC_ERROR;

	/* enable the relevant completion interrupts */
	reg_value |= XDMA_CTRL_IE_DESC_STOPPED;
	reg_value |= XDMA_CTRL_IE_DESC_COMPLETED;

	/* Apply engine configurations */
	write_register(reg_value, &engine->regs->interrupt_enable_mask,
			(unsigned long)(&engine->regs->interrupt_enable_mask) -
			(unsigned long)(&engine->regs));
}

static int engine_init(struct xdma_engine *engine, struct xdma_dev *xdev,
			int offset, enum dma_data_direction dir, int channel)
{
	dbg_init("channel %d, offset 0x%x, dir %d.\n", channel, offset, dir);

	/* set magic */
	engine->magic = MAGIC_ENGINE;

	engine->channel = channel;

	/* engine interrupt request bit */
	engine->irq_bitmask = (1 << XDMA_ENG_IRQ_NUM) - 1;
	engine->irq_bitmask <<= (xdev->engines_num * XDMA_ENG_IRQ_NUM);
	engine->bypass_offset = xdev->engines_num * BYPASS_MODE_SPACING;

	/* initialize spinlock */
	spin_lock_init(&engine->lock);
	/* initialize transfer_list */
	INIT_LIST_HEAD(&engine->transfer_list);
	/* parent */
	engine->xdev = xdev;
	/* register address */
	engine->regs = (xdev->bar[xdev->config_bar_idx] + offset);
	engine->sgdma_regs = xdev->bar[xdev->config_bar_idx] + offset +
				SGDMA_OFFSET_FROM_CHANNEL;
	/* remember SG DMA direction */
	engine->dir = dir;
	sprintf(engine->name, "%s%d",
		(dir == DMA_TO_DEVICE) ? "H2C" : "C2H", channel);

	dbg_init("engine %p name %s irq_bitmask=0x%08x\n", engine, engine->name,
		(int)engine->irq_bitmask);

	/* initialize the deferred work for transfer completion */
	INIT_WORK(&engine->work, engine_service_work);

	xdev->engines_num++;

	/* initialize wait queue */
	init_waitqueue_head(&engine->shutdown_wq);

	engine_init_regs(engine);

	/* all engine setup completed successfully */
	return 0;
}

/* transfer_destroy() - free transfer */
static void transfer_destroy(struct xdma_dev *xdev, struct xdma_transfer *transfer)
{
	/* free descriptors */
	xdma_desc_free(xdev->pdev, transfer->desc_num, transfer->desc_virt,
			transfer->desc_bus);

	if (transfer->last_in_request &&
		(transfer->flags & XFER_FLAG_NEED_UNMAP)) {
        	struct sg_table *sgt = transfer->sgt;
		pci_unmap_sg(xdev->pdev, sgt->sgl, sgt->nents, transfer->dir);
	}

	/* free transfer */
	kfree(transfer);
}

static int transfer_build(struct xdma_engine *engine,
			struct xdma_transfer *transfer, u64 ep_addr,
			struct scatterlist **sgl_p, unsigned int nents)
{
	struct scatterlist *sg = *sgl_p;
	int i = 0;
	int j = 0;
	dma_addr_t cont_addr = sg_dma_address(sg);
	unsigned int cont_len = sg_dma_len(sg);
	unsigned int next_len = 0;

	dbg_desc("sg %d/%u: addr=0x%llx, len=0x%x\n",
		i, nents, cont_addr, cont_len);
	for (i = 1, sg = sg_next(sg); i < nents; i++, sg = sg_next(sg)) {
		dma_addr_t next_addr = sg_dma_address(sg);
		next_len = sg_dma_len(sg);

		dbg_desc("sg %d/%u: addr=0x%llx, len=0x%x, cont 0x%llx,0x%x.\n",
			i, nents, next_addr, next_len, cont_addr, cont_len);
		/* contiguous ? */
		if (next_addr == (cont_addr + cont_len)) {
			cont_len += next_len;
			continue;
		}

	dbg_desc("DESC %d: addr=0x%llx, 0x%x, ep_addr=0x%llx\n",
		j, (u64)cont_addr, cont_len, (u64)ep_addr);
		/* fill in descriptor entry j with transfer details */
		xdma_desc_set(transfer->desc_virt + j, cont_addr, ep_addr,
				 cont_len, transfer->dir);
		transfer->xfer_len += cont_len;

		/* for non-inc-add mode don't increment ep_addr */
		if (!engine->non_incr_addr)
			ep_addr += cont_len;

		/* start new contiguous block */
		cont_addr = next_addr;
		cont_len = next_len;
		j++;
	}
	BUG_ON(j > nents);

	if (cont_len) {
		dbg_desc("DESC %d: addr=0x%llx, 0x%x, ep_addr=0x%llx\n",
			j, (u64)cont_addr, cont_len, (u64)ep_addr);
		xdma_desc_set(transfer->desc_virt + j, cont_addr, ep_addr,
			 cont_len, transfer->dir);
		transfer->xfer_len += cont_len;
	}

	*sgl_p = sg;
	return j;
}

static struct xdma_transfer *transfer_create(struct xdma_engine *engine,
			u64 ep_addr, struct scatterlist **sgl_p, int nents)
{
	struct xdma_dev *xdev = engine->xdev;
	struct xdma_transfer *transfer = NULL;
	int i = 0;
	int last = 0;
	u32 control;
	int rv;

	transfer = kzalloc(sizeof(struct xdma_transfer), GFP_KERNEL);
	if (!transfer) {
		pr_info("OOM.\n");
		return NULL;
	}

	/* remember direction of transfer */
	transfer->dir = engine->dir;

	/* allocate descriptor list */
	transfer->desc_virt = xdma_desc_alloc(xdev->pdev, nents,
				&transfer->desc_bus);
	if (!transfer->desc_virt) 
		goto err_out;
	
	dbg_sg("transfer->desc_bus = 0x%llx.\n", (u64)transfer->desc_bus);

	rv = transfer_build(engine, transfer, ep_addr, sgl_p, nents);
	if (rv < 0)
		goto err_out;
	last = rv;

	/* terminate last descriptor */
	xdma_desc_link(transfer->desc_virt + last, 0, 0);
	/* stop engine, EOP for AXI ST, req IRQ on last descriptor */
	control = XDMA_DESC_STOPPED;
	control |= XDMA_DESC_EOP;
	control |= XDMA_DESC_COMPLETED;
	xdma_desc_control(transfer->desc_virt + last, control);

	last++;
	/* last is the number of descriptors */
	transfer->desc_num = transfer->desc_adjacent = last;

	dbg_sg("transfer 0x%p has %d descriptors\n", transfer,
		transfer->desc_num);
	/* fill in adjacent numbers */
	for (i = 0; i < transfer->desc_num; i++) {
		xdma_desc_adjacent(transfer->desc_virt + i,
			transfer->desc_num - i - 1);
	}

	/* initialize wait queue */
	init_waitqueue_head(&transfer->wq);

	return transfer;

err_out:
	if (transfer)
		kfree(transfer);
	return NULL;	
}

static void sgt_dump(struct sg_table *sgt)
{
	int i;
	struct scatterlist *sg = sgt->sgl;

	pr_info("sgt 0x%p, sgl 0x%p, nents %u/%u.\n",
		sgt, sgt->sgl, sgt->nents, sgt->orig_nents);

	for (i = 0; i < sgt->orig_nents; i++, sg = sg_next(sg))
		pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx,%u.\n",
			i, sg, sg_page(sg), sg->offset, sg->length,
			sg_dma_address(sg), sg_dma_len(sg)); 
}

int xdma_xfer_submit(void *dev_hndl, int channel, enum dma_data_direction dir,
			u64 ep_addr, struct sg_table *sgt, int dma_mapped,
			int timeout_ms)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;
	struct xdma_engine *engine;
	int rv = 0;
	ssize_t done = 0;
	struct scatterlist *sg = sgt->sgl;
	int nents;

	if (!dev_hndl)
		return -EINVAL;

	if (debug_check_dev_hndl(__func__, xdev->pdev, dev_hndl) < 0)
		return -EINVAL;

	if (channel >= xdev->channel_max) {
		pr_warn("channel %d >= %d.\n", channel, xdev->channel_max);
		return -EINVAL;
	}

	if (dir == DMA_TO_DEVICE)
		engine = &xdev->engine_h2c[channel];
	else
		engine = &xdev->engine_c2h[channel];

        BUG_ON(!engine);
        BUG_ON(engine->magic != MAGIC_ENGINE);

	xdev = engine->xdev;
	if (xdma_device_flag_check(xdev, XDEV_FLAG_OFFLINE)) {
		pr_info("xdev 0x%p, offline.\n", xdev);
		return -EBUSY;
	}

	/* check the direction */
	if (engine->dir != dir) {
		pr_info("channel 0x%p, %s, %d, dir 0x%x/0x%x mismatch.\n",
			engine, engine->name, channel, engine->dir, dir);
		return -EINVAL;
	}

	if (!dma_mapped) {
		nents = pci_map_sg(xdev->pdev, sg, sgt->orig_nents, dir);
		if (!nents) {
			pr_info("map sgl failed, sgt 0x%p.\n", sgt);
			return -EIO;
		}
		sgt->nents = nents;
	} else {
		BUG_ON(!sgt->nents);
		nents = sgt->nents;
	}

	while (nents) {
		unsigned int xfer_nents = min_t(unsigned int, 
					nents, XDMA_TRANSFER_MAX_DESC);
		struct xdma_transfer *transfer;

		/* build transfer */	
		transfer = transfer_create(engine, ep_addr, &sg, xfer_nents);
		if (!transfer) {
			pr_info("OOM.\n");
			rv = -ENOMEM;
			goto unmap;
		}

		if (!dma_mapped)
			transfer->flags = XFER_FLAG_NEED_UNMAP;

		/* last transfer for the given request? */
		nents -= xfer_nents;
		if (!nents) {
			transfer->last_in_request = 1;
			transfer->sgt = sgt;
		}

		transfer_dump(transfer);

		rv = transfer_queue(engine, transfer);
		if (rv < 0) {
			pr_info("unable to submit %s, %d.\n", engine->name, rv);
			transfer_destroy(xdev, transfer);
			goto unmap;
		}

		rv = wait_event_interruptible_timeout(transfer->wq,
                        (transfer->state != TRANSFER_STATE_SUBMITTED),
			msecs_to_jiffies(timeout_ms));

		switch(transfer->state) {
		case TRANSFER_STATE_COMPLETED:
			dbg_tfr("transfer %p, %u completed.\n", transfer,
				transfer->xfer_len);
			done += transfer->xfer_len;
			ep_addr += transfer->xfer_len;
			transfer_destroy(xdev, transfer);
			break;
		case TRANSFER_STATE_FAILED:
			dbg_tfr("transfer %p, %u failed.\n", transfer,
				transfer->xfer_len);
			rv = -EIO;
			goto unmap;
		default:
			/* transfer can still be in-flight */
			pr_info("xfer 0x%p,%u, state 0x%x.\n",
				 transfer, transfer->xfer_len,
				transfer->state);
			engine_status_read(engine, 0, 1);
			read_interrupts(xdev);
			interrupt_status(xdev);
			rv = -ERESTARTSYS;
			goto unmap;
		}
	} /* while (sg) */

	return done;

unmap:
	//sgt_dump(sgt);
	if (!dma_mapped) {
		pci_unmap_sg(xdev->pdev, sgt->sgl, sgt->orig_nents, dir);
		sgt->nents = 0;
	}
	return rv;
}
EXPORT_SYMBOL_GPL(xdma_xfer_submit);

static struct xdma_dev *alloc_dev_instance(struct pci_dev *pdev)
{
	int i;
	struct xdma_dev *xdev;

	BUG_ON(!pdev);

	/* allocate zeroed device book keeping structure */
	xdev = kzalloc(sizeof(struct xdma_dev), GFP_KERNEL);
	if (!xdev) {
		pr_info("OOM, xdma_dev.\n");
		return NULL;
	}
	spin_lock_init(&xdev->lock);

	xdev->magic = MAGIC_DEVICE;
	xdev->config_bar_idx = -1;
	xdev->user_bar_idx = -1;
	xdev->bypass_bar_idx = -1;
	xdev->irq_line = -1;

	/* create a driver to device reference */
	xdev->pdev = pdev;
	dbg_init("xdev = 0x%p\n", xdev);

	/* Set up data user IRQ data structures */
	for (i = 0; i < xdev->user_max; i++) {
		xdev->user_irq[i].xdev = xdev;
		spin_lock_init(&xdev->user_irq[i].events_lock);
		init_waitqueue_head(&xdev->user_irq[i].events_wq);
		xdev->user_irq[i].handler = NULL;
		xdev->user_irq[i].user_idx = i + 1;
	}

	return xdev;
}

static int request_regions(struct xdma_dev *xdev, struct pci_dev *pdev)
{
	int rv;

	BUG_ON(!xdev);
	BUG_ON(!pdev);

	dbg_init("pci_request_regions()\n");
	rv = pci_request_regions(pdev, xdev->mod_name);
	/* could not request all regions? */
	if (rv) {
		dbg_init("pci_request_regions() = %d, device in use?\n", rv);
		/* assume device is in use so do not disable it later */
		xdev->regions_in_use = 1;
	} else {
		xdev->got_regions = 1;
	}

	return rv;
}

static int set_dma_mask(struct pci_dev *pdev)
{
	BUG_ON(!pdev);

	dbg_init("sizeof(dma_addr_t) == %ld\n", sizeof(dma_addr_t));
	/* 64-bit addressing capability for XDMA? */
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		/* query for DMA transfer */
		/* @see Documentation/DMA-mapping.txt */
		dbg_init("pci_set_dma_mask()\n");
		/* use 64-bit DMA */
		dbg_init("Using a 64-bit DMA mask.\n");
		/* use 32-bit DMA for descriptors */
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		/* use 64-bit DMA, 32-bit for consistent */
	} else if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		dbg_init("Could not set 64-bit DMA mask.\n");
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		/* use 32-bit DMA */
		dbg_init("Using a 32-bit DMA mask.\n");
	} else {
		dbg_init("No suitable DMA possible.\n");
		return -EINVAL;
	}

	return 0;
}

static u32 get_engine_channel_id(struct engine_regs *regs)
{
	u32 value;

	BUG_ON(!regs);

	value = read_register(&regs->identifier);

	return (value & 0x00000f00U) >> 8;
}

static u32 get_engine_id(struct engine_regs *regs)
{
	u32 value;

	BUG_ON(!regs);

	value = read_register(&regs->identifier);
	return (value & 0xffff0000U) >> 16;
}

static void remove_engines(struct xdma_dev *xdev)
{
	struct xdma_engine *engine;
	int i;

	BUG_ON(!xdev);

	/* iterate over channels */
	for (i = 0; i < xdev->channel_max; i++) {
		engine = &xdev->engine_h2c[i];
		if (engine->magic == MAGIC_ENGINE) {
			dbg_sg("Remove %s, %d", engine->name, i);
			engine_destroy(xdev, engine);
			dbg_sg("%s, %d removed", engine->name, i);
		}

		engine = &xdev->engine_c2h[i];
		if (engine->magic == MAGIC_ENGINE) {
			dbg_sg("Remove %s, %d", engine->name, i);
			engine_destroy(xdev, engine);
			dbg_sg("%s, %d removed", engine->name, i);
		}
	}
}

static int probe_for_engine(struct xdma_dev *xdev, enum dma_data_direction dir,
			int channel)
{
	struct engine_regs *regs;
	int offset = channel * CHANNEL_SPACING;
	u32 engine_id;
	u32 engine_id_expected;
	u32 channel_id;
	struct xdma_engine *engine;
	int rv;

	/* register offset for the engine */
	/* read channels at 0x0000, write channels at 0x1000,
	 * channels at 0x100 interval */
	if (dir == DMA_TO_DEVICE) {
		engine_id_expected = XDMA_ID_H2C;
		engine = &xdev->engine_h2c[channel];
	} else {
		offset += H2C_CHANNEL_OFFSET;
		engine_id_expected = XDMA_ID_C2H;
		engine = &xdev->engine_c2h[channel];
	}

	regs = xdev->bar[xdev->config_bar_idx] + offset;
	engine_id = get_engine_id(regs);
	channel_id = get_engine_channel_id(regs);

	if ((engine_id != engine_id_expected) || (channel_id != channel)) {
		pr_info("%s %d engine, reg off 0x%x, id mismatch 0x%x,0x%x,"
			"exp 0x%x,0x%x, SKIP.\n",
		 	dir == DMA_TO_DEVICE ? "H2C" : "C2H",
			 channel, offset, engine_id, channel_id,
			engine_id_expected, channel_id != channel);
		return -EINVAL;
	}

	pr_info("found AXI %s %d engine, reg. off 0x%x, id 0x%x,0x%x.\n",
		 dir == DMA_TO_DEVICE ? "H2C" : "C2H", channel,
		 offset, engine_id, channel_id);

	/* allocate and initialize engine */
	rv = engine_init(engine, xdev, offset, dir, channel);
	if (rv != 0) {
		pr_info("failed to create AXI %s %d engine.\n",
			dir == DMA_TO_DEVICE ? "H2C" : "C2H",
			channel);
		return rv;
	}

	return 0;
}

static int probe_engines(struct xdma_dev *xdev)
{
	int i, j;
	int rv = 0;

	BUG_ON(!xdev);

	/* iterate over channels */
	for (i = 0; i < xdev->channel_max; i++) {
		rv = probe_for_engine(xdev, DMA_TO_DEVICE, i);
		if (rv)
			break;
	}

	for (j = 0; j < xdev->channel_max; j++) {
		rv = probe_for_engine(xdev, DMA_FROM_DEVICE, j);
		if (rv)
			break;
	}

	/* h2c  & c2h are always a pair */
	if (i != j) {
		pr_err("unmatched H2C %d vs C2H %d.\n", i, j);
		rv = -EINVAL;
		goto fail;
	}
	xdev->channel_max = i;

	return 0;

fail:
	dbg_init("Engine probing failed - unwinding\n");
	remove_engines(xdev);

	return rv;
}

void *xdma_device_open(const char *mod_name, struct pci_dev *pdev,
			int *user_max, int *channel_max)
{
	struct xdma_dev *xdev = NULL;
	int rv = 0;

	pr_info("%s device %s, 0x%p.\n", mod_name, dev_name(&pdev->dev), pdev);


	/* allocate zeroed device book keeping structure */
	xdev = alloc_dev_instance(pdev);
	if (!xdev)
		return NULL;
	xdev->mod_name = mod_name;
	xdev->user_max = *user_max;
	xdev->channel_max = *channel_max;

	if (xdev->user_max == 0 || xdev->user_max > MAX_USER_IRQ)
		xdev->user_max = MAX_USER_IRQ;
	if (xdev->channel_max == 0 || xdev->channel_max > XDMA_CHANNEL_NUM_MAX) 
		xdev->channel_max = XDMA_CHANNEL_NUM_MAX;
		
	rv = pci_enable_device(pdev);
	if (rv) {
		dbg_init("pci_enable_device() failed, %d.\n", rv);
		goto err_enable;
	}

	/* enable bus master capability */
	pci_set_master(pdev);

	rv = request_regions(xdev, pdev);
	if (rv)
		goto err_regions;

	rv = map_bars(xdev, pdev);
	if (rv)
		goto err_map;

	rv = set_dma_mask(pdev);
	if (rv)
		goto err_mask;

	rv = probe_engines(xdev);
	if (rv)
		goto err_engines;

	rv = enable_msi_msix(xdev, pdev);
	if (rv < 0)
		goto err_enable_msix;

	rv = irq_setup(xdev, pdev);
	if (rv < 0)
		goto err_interrupts;

	channel_interrupts_enable(xdev, ~0);

	/* Flush writes */
	read_interrupts(xdev);

//	xdev->feature_id = find_feature_id(xdev);
	
	xdev_list_add(xdev);

	*user_max = xdev->user_max;
	*channel_max = xdev->channel_max;
	return (void *)xdev;

err_interrupts:
	irq_teardown(xdev);
err_enable_msix:
	disable_msi_msix(xdev, pdev);
err_engines:
	remove_engines(xdev);
err_mask:
	unmap_bars(xdev, pdev);
err_map:
	if (xdev->got_regions)
		pci_release_regions(pdev);
err_regions:
	if (!xdev->regions_in_use)
		pci_disable_device(pdev);
err_enable:
	kfree(xdev);
	return NULL;
}
EXPORT_SYMBOL_GPL(xdma_device_open);

void xdma_device_close(struct pci_dev *pdev, void *dev_hndl)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;

	dbg_init("pdev 0x%p, xdev 0x%p.\n", pdev, dev_hndl);

	if (!dev_hndl)
		return;

	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return;

	dbg_sg("remove(dev = 0x%p) where pdev->dev.driver_data = 0x%p\n",
		   pdev, xdev);
	if (xdev->pdev != pdev) {
		dbg_sg("pci_dev(0x%lx) != pdev(0x%lx)\n",
			(unsigned long)xdev->pdev, (unsigned long)pdev);
	}

	channel_interrupts_disable(xdev, ~0);
	user_interrupts_disable(xdev, ~0);
	read_interrupts(xdev);

	irq_teardown(xdev);
	disable_msi_msix(xdev, pdev);

	remove_engines(xdev);
	unmap_bars(xdev, pdev);

	if (xdev->got_regions) {
		dbg_init("pci_release_regions 0x%p.\n", pdev);
		pci_release_regions(pdev);
	}

	if (!xdev->regions_in_use) {
		dbg_init("pci_disable_device 0x%p.\n", pdev);
		pci_disable_device(pdev);
	}

	xdev_list_remove(xdev);

	kfree(xdev);
}
EXPORT_SYMBOL_GPL(xdma_device_close);

void xdma_device_offline(struct pci_dev *pdev, void *dev_hndl)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;
	struct xdma_engine *engine;
	int i, rv;

	if (!dev_hndl)
		return;

	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return;

pr_info("pdev 0x%p, xdev 0x%p.\n", pdev, xdev);
	xdma_device_flag_set(xdev, XDEV_FLAG_OFFLINE);

	/* wait for all engines to be idle */
	for (i  = 0; i < xdev->channel_max; i++) {
		engine = &xdev->engine_h2c[i];
		
		if (engine->magic == MAGIC_ENGINE) {
			spin_lock(&engine->lock);
			engine->shutdown |= ENGINE_SHUTDOWN_REQUEST;
			spin_unlock(&engine->lock);

			xdma_engine_stop(engine);
			pr_info("xdev 0x%p, h2c %d, wait for idle.\n", xdev, i);
			rv = wait_event_interruptible(engine->shutdown_wq,
						!engine->running);
			pr_info("xdev 0x%p, h2c %d, wait done %d.\n", xdev, i, rv);
			if (engine->running)
				pr_warn("xdev 0x%p, h2c %d, NOT idle.\n", xdev, i);
		}
		engine = &xdev->engine_c2h[i];
		if (engine->magic == MAGIC_ENGINE) {
			spin_lock(&engine->lock);
			engine->shutdown |= ENGINE_SHUTDOWN_REQUEST;
			spin_unlock(&engine->lock);

			xdma_engine_stop(engine);
			pr_info("xdev 0x%p, c2h %d, wait for idle.\n", xdev, i);
			rv = wait_event_interruptible(engine->shutdown_wq,
						!engine->running);
			pr_info("xdev 0x%p, c2h %d, wait done %d.\n", xdev, i, rv);
			if (engine->running)
				pr_warn("xdev 0x%p, c2h %d, NOT idle.\n", xdev, i);
		}
	}

	/* turn off interrupts */
	channel_interrupts_disable(xdev, ~0);
	user_interrupts_disable(xdev, ~0);
	read_interrupts(xdev);

	pr_info("xdev 0x%p, done.\n", xdev);
}
EXPORT_SYMBOL_GPL(xdma_device_offline);

void xdma_device_online(struct pci_dev *pdev, void *dev_hndl)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;
	struct xdma_engine *engine;
	int i;

	if (!dev_hndl)
		return;

	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return;

pr_info("pdev 0x%p, xdev 0x%p.\n", pdev, xdev);

	for (i  = 0; i < xdev->channel_max; i++) {
		engine = &xdev->engine_h2c[i];
		if (engine->magic == MAGIC_ENGINE) {
			engine_init_regs(engine);
			spin_lock(&engine->lock);
			engine->shutdown &= ~ENGINE_SHUTDOWN_REQUEST;
			spin_unlock(&engine->lock);
		}
		engine = &xdev->engine_c2h[i];
		if (engine->magic == MAGIC_ENGINE) {
			engine_init_regs(engine);
			spin_lock(&engine->lock);
			engine->shutdown &= ~ENGINE_SHUTDOWN_REQUEST;
			spin_unlock(&engine->lock);
		}
	}

	/* re-write the interrupt table */
	prog_irq_msix_channel(xdev);
	prog_irq_msix_user(xdev);
	
	xdma_device_flag_clear(xdev, XDEV_FLAG_OFFLINE);

	channel_interrupts_enable(xdev, ~0);
	user_interrupts_enable(xdev, xdev->mask_irq_user);
	read_interrupts(xdev);
pr_info("xdev 0x%p, done.\n", xdev);
}
EXPORT_SYMBOL_GPL(xdma_device_online);

int xdma_device_restart(struct pci_dev *pdev, void *dev_hndl)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;

	if (!dev_hndl)
		return -EINVAL;

	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return -EINVAL;

	pr_info("NOT implemented, 0x%p.\n", xdev);
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(xdma_device_restart);

int xdma_user_isr_register(void *dev_hndl, unsigned int mask,
			irq_handler_t handler, void *dev)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;
	int i;

	if (!dev_hndl)
		return -EINVAL;

	if (debug_check_dev_hndl(__func__, xdev->pdev, dev_hndl) < 0)
		return -EINVAL;

	for (i = 0; i < xdev->user_max && mask; i++) {
		unsigned int bit = (1 << i);

		if ((bit & mask) == 0)
			continue;

		mask &= ~bit;
		xdev->user_irq[i].handler = handler;
		xdev->user_irq[i].dev = dev;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(xdma_user_isr_register);

int xdma_user_isr_enable(void *dev_hndl, unsigned int mask)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;

	if (!dev_hndl)
		return -EINVAL;

	if (debug_check_dev_hndl(__func__, xdev->pdev, dev_hndl) < 0)
		return -EINVAL;

	xdev->mask_irq_user |= mask;
	/* enable user interrupts */
	user_interrupts_enable(xdev, mask);
	read_interrupts(xdev);

	return 0;
}
EXPORT_SYMBOL_GPL(xdma_user_isr_enable);

int xdma_user_isr_disable(void *dev_hndl, unsigned int mask)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;

	if (!dev_hndl)
		return -EINVAL;

	if (debug_check_dev_hndl(__func__, xdev->pdev, dev_hndl) < 0)
		return -EINVAL;
	
	xdev->mask_irq_user &= ~mask;
	user_interrupts_disable(xdev, mask);
	read_interrupts(xdev);

	return 0;
}
EXPORT_SYMBOL_GPL(xdma_user_isr_disable);

#ifdef __LIBXDMA_MOD__
static int __init xdma_base_init(void)
{
	printk(KERN_INFO "%s", version);
	return 0;
}

static void __exit xdma_base_exit(void)
{
	return;
}

module_init(xdma_base_init);
module_exit(xdma_base_exit);
#endif


