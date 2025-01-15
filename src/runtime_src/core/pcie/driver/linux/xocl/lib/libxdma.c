/**
 *  Copyright (C) 2017-2020 Xilinx, Inc. All rights reserved.
 *  Author: Karen Xie <karen.xie@xilinx.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>

#include "libxdma.h"
#include "libxdma_api.h"
#include "cdev_sgdma.h"


extern unsigned int desc_blen_max;

#define xocl_pr_info(fmt, args...)				\
	printk(KERN_DEBUG pr_fmt(fmt), ##args)

/* Module Parameters */
static unsigned int poll_mode;
module_param(poll_mode, uint, 0644);
MODULE_PARM_DESC(poll_mode, "Set 1 for hw polling, default is 0 (interrupts)");

static unsigned int interrupt_mode;
module_param(interrupt_mode, uint, 0644);
MODULE_PARM_DESC(interrupt_mode, "0 - MSI-x , 1 - MSI, 2 - Legacy");

static unsigned int enable_credit_mp = 1;
module_param(enable_credit_mp, uint, 0644);
MODULE_PARM_DESC(enable_credit_mp, "Set 1 to enable creidt feature, default is 0 (no credit control)");

static unsigned int desc_set_depth = 32;
module_param(desc_set_depth, uint, 0644);
MODULE_PARM_DESC(desc_set_depth, "Supported Values 16, 32, 64, 128, default is 32");

unsigned int desc_blen_max = XDMA_DESC_BLEN_MAX;
module_param(desc_blen_max, uint, 0644);
MODULE_PARM_DESC(desc_blen_max,
		 "per descriptor max. buffer length, default is (1 << 28) - 1");



/*
 * xdma device management
 * maintains a list of the xdma devices
 */
static LIST_HEAD(xdev_list);
static DEFINE_SPINLOCK(xdev_lock);

static LIST_HEAD(xdev_rcu_list);
static DEFINE_SPINLOCK(xdev_rcu_lock);

#ifndef list_last_entry
#define list_last_entry(ptr, type, member) \
		list_entry((ptr)->prev, type, member)
#endif

static inline unsigned int incr_ptr_idx(unsigned int cur, unsigned int incr,
			     unsigned int max)
{
	cur += incr;
	if (cur > (max - 1))
		cur -= max;

	return cur;
}


static inline void xdev_list_add(struct xdma_dev *xdev)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev_lock, flags);
	if (list_empty(&xdev_list))
		xdev->idx = 0;
	else {
		struct xdma_dev *last;

		last = list_last_entry(&xdev_list, struct xdma_dev, list_head);
		xdev->idx = last->idx + 1;
	}
	list_add_tail(&xdev->list_head, &xdev_list);
	spin_unlock_irqrestore(&xdev_lock, flags);

	dbg_init("dev %s, xdev 0x%p, xdma idx %d.\n",
		dev_name(&xdev->pdev->dev), xdev, xdev->idx);

	spin_lock(&xdev_rcu_lock);
	list_add_tail_rcu(&xdev->rcu_node, &xdev_rcu_list);
	spin_unlock(&xdev_rcu_lock);
}

#undef list_last_entry

static inline void xdev_list_remove(struct xdma_dev *xdev)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev_lock, flags);
	list_del(&xdev->list_head);
	spin_unlock_irqrestore(&xdev_lock, flags);

	spin_lock(&xdev_rcu_lock);
	list_del_rcu(&xdev->rcu_node);
	spin_unlock(&xdev_rcu_lock);
	synchronize_rcu();
}

static struct xdma_dev *xdev_find_by_pdev(struct pci_dev *pdev)
{
        struct xdma_dev *xdev, *tmp;
	unsigned long flags;

	spin_lock_irqsave(&xdev_lock, flags);
        list_for_each_entry_safe(xdev, tmp, &xdev_list, list_head) {
                if (xdev->pdev == pdev) {
			spin_unlock_irqrestore(&xdev_lock, flags);
                        return xdev;
                }
        }
	spin_unlock_irqrestore(&xdev_lock, flags);
        return NULL;
}

static inline int debug_check_dev_hndl(const char *fname, struct pci_dev *pdev,
				 void *hndl)
{
	struct xdma_dev *xdev;

	if (!pdev)
		return -EINVAL;

	xdev = xdev_find_by_pdev(pdev);
	if (!xdev) {
		xocl_pr_info("%s pdev 0x%p, hndl 0x%p, NO match found!\n",
			fname, pdev, hndl);
		return -EINVAL;
	}
	if (xdev != hndl) {
		pr_err("%s pdev 0x%p, hndl 0x%p != 0x%p!\n",
			fname, pdev, hndl, xdev);
		return -EINVAL;
	}

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
#define write_register(v,mem,off) writel(v, mem)
#endif

inline u32 read_register(void *iomem)
{
	return readl(iomem);
}

static inline u32 build_u32(u32 hi, u32 lo)
{
	return ((hi & 0xFFFFUL) << 16) | (lo & 0xFFFFUL);
}

static inline u64 build_u64(u64 hi, u64 lo)
{
	return ((hi & 0xFFFFFFFULL) << 32) | (lo & 0xFFFFFFFFULL);
}

static void check_nonzero_interrupt_status(struct xdma_dev *xdev)
{
	struct interrupt_regs *reg = (struct interrupt_regs *)
		(xdev->bar[xdev->config_bar_idx] + XDMA_OFS_INT_CTRL);
	u32 w;

	w = read_register(&reg->user_int_enable);
	if (w)
		pr_info("%s xdma%d user_int_enable = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);

	w = read_register(&reg->channel_int_enable);
	if (w)
		pr_info("%s xdma%d channel_int_enable = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);

	w = read_register(&reg->user_int_request);
	if (w)
		pr_info("%s xdma%d user_int_request = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);
	w = read_register(&reg->channel_int_request);
	if (w)
		pr_info("%s xdma%d channel_int_request = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);

	w = read_register(&reg->user_int_pending);
	if (w)
		pr_info("%s xdma%d user_int_pending = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);
	w = read_register(&reg->channel_int_pending);
	if (w)
		pr_info("%s xdma%d channel_int_pending = 0x%08x\n",
			dev_name(&xdev->pdev->dev), xdev->idx, w);
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

void enable_perf(struct xdma_engine *engine)
{
	u32 w;

	w = XDMA_PERF_CLEAR;
	write_register(w, &engine->regs->perf_ctrl,
			(unsigned long)(&engine->regs->perf_ctrl) -
			(unsigned long)(&engine->regs));
	read_register(&engine->regs->identifier);
	w = XDMA_PERF_AUTO | XDMA_PERF_RUN;
	write_register(w, &engine->regs->perf_ctrl,
			(unsigned long)(&engine->regs->perf_ctrl) -
			(unsigned long)(&engine->regs));
	read_register(&engine->regs->identifier);

	dbg_perf("IOCTL_XDMA_PERF_START\n");

}

void get_perf_stats(struct xdma_engine *engine)
{
	u32 hi;
	u32 lo;

	BUG_ON(!engine);
	BUG_ON(!engine->xdma_perf);

	hi = 0;
	lo = read_register(&engine->regs->completed_desc_count);
	engine->xdma_perf->iterations = build_u64(hi, lo);

	hi = read_register(&engine->regs->perf_cyc_hi);
	lo = read_register(&engine->regs->perf_cyc_lo);

	engine->xdma_perf->clock_cycle_count = build_u64(hi, lo);

	hi = read_register(&engine->regs->perf_dat_hi);
	lo = read_register(&engine->regs->perf_dat_lo);
	engine->xdma_perf->data_cycle_count = build_u64(hi, lo);

	hi = read_register(&engine->regs->perf_pnd_hi);
	lo = read_register(&engine->regs->perf_pnd_lo);
	engine->xdma_perf->pending_count = build_u64(hi, lo);
}

static void engine_reg_dump(struct xdma_engine *engine)
{
	u32 w;

	BUG_ON(!engine);

	w = read_register(&engine->regs->identifier);
	pr_info("%s: ioread32(0x%p) = 0x%08x (id).\n",
		engine->name, &engine->regs->identifier, w);
	w &= BLOCK_ID_MASK;
	if (w != BLOCK_ID_HEAD) {
		pr_info("%s: engine id missing, 0x%08x exp. & 0x%x = 0x%x\n",
			 engine->name, w, BLOCK_ID_MASK, BLOCK_ID_HEAD);
		return;
	}
	/* extra debugging; inspect complete engine set of registers */
	w = read_register(&engine->regs->status);
	pr_info("%s: ioread32(0x%p) = 0x%08x (status).\n",
		engine->name, &engine->regs->status, w);
	w = read_register(&engine->regs->control);
	pr_info("%s: ioread32(0x%p) = 0x%08x (control)\n",
		engine->name, &engine->regs->control, w);
	w = read_register(&engine->sgdma_regs->first_desc_lo);
	pr_info("%s: ioread32(0x%p) = 0x%08x (first_desc_lo)\n",
		engine->name, &engine->sgdma_regs->first_desc_lo, w);
	w = read_register(&engine->sgdma_regs->first_desc_hi);
	pr_info("%s: ioread32(0x%p) = 0x%08x (first_desc_hi)\n",
		engine->name, &engine->sgdma_regs->first_desc_hi, w);
	w = read_register(&engine->sgdma_regs->first_desc_adjacent);
	pr_info("%s: ioread32(0x%p) = 0x%08x (first_desc_adjacent).\n",
		engine->name, &engine->sgdma_regs->first_desc_adjacent, w);
	w = read_register(&engine->regs->completed_desc_count);
	pr_info("%s: ioread32(0x%p) = 0x%08x (completed_desc_count).\n",
		engine->name, &engine->regs->completed_desc_count, w);
	w = read_register(&engine->regs->interrupt_enable_mask);
	pr_info("%s: ioread32(0x%p) = 0x%08x (interrupt_enable_mask)\n",
		engine->name, &engine->regs->interrupt_enable_mask, w);
}

/**
 * engine_status_read() - read status of SG DMA engine (optionally reset)
 *
 * Stores status in engine->status.
 *
 * @return -1 on failure, status register otherwise
 */
static void engine_status_dump(struct xdma_engine *engine)
{
	u32 v = engine->status;
	char buffer[256];
	char *buf = buffer;
	int len = 0;

	len = sprintf(buf, "SG engine %s status: 0x%08x: ", engine->name, v);

	if ((v & XDMA_STAT_BUSY))
		len += sprintf(buf + len, "BUSY,");
	if ((v & XDMA_STAT_DESC_STOPPED))
		len += sprintf(buf + len, "DESC_STOPPED,");
	if ((v & XDMA_STAT_DESC_COMPLETED))
		len += sprintf(buf + len, "DESC_COMPL,");

	/* common H2C & C2H */
 	if ((v & XDMA_STAT_COMMON_ERR_MASK)) {
		if ((v & XDMA_STAT_ALIGN_MISMATCH))
			len += sprintf(buf + len, "ALIGN_MISMATCH ");
		if ((v & XDMA_STAT_MAGIC_STOPPED))
			len += sprintf(buf + len, "MAGIC_STOPPED ");
		if ((v & XDMA_STAT_INVALID_LEN))
			len += sprintf(buf + len, "INVLIAD_LEN ");
		if ((v & XDMA_STAT_IDLE_STOPPED))
			len += sprintf(buf + len, "IDLE_STOPPED ");
		buf[len - 1] = ',';
	}

	if ((engine->dir == DMA_TO_DEVICE)) {
		/* H2C only */
		if ((v & XDMA_STAT_H2C_R_ERR_MASK)) {
			len += sprintf(buf + len, "R:");
			if ((v & XDMA_STAT_H2C_R_UNSUPP_REQ))
				len += sprintf(buf + len, "UNSUPP_REQ ");
			if ((v & XDMA_STAT_H2C_R_COMPL_ABORT))
				len += sprintf(buf + len, "COMPL_ABORT ");
			if ((v & XDMA_STAT_H2C_R_PARITY_ERR))
				len += sprintf(buf + len, "PARITY ");
			if ((v & XDMA_STAT_H2C_R_HEADER_EP))
				len += sprintf(buf + len, "HEADER_EP ");
			if ((v & XDMA_STAT_H2C_R_UNEXP_COMPL))
				len += sprintf(buf + len, "UNEXP_COMPL ");
			buf[len - 1] = ',';
		}

		if ((v & XDMA_STAT_H2C_W_ERR_MASK)) {
			len += sprintf(buf + len, "W:");
			if ((v & XDMA_STAT_H2C_W_DECODE_ERR))
				len += sprintf(buf + len, "DECODE_ERR ");
			if ((v & XDMA_STAT_H2C_W_SLAVE_ERR))
				len += sprintf(buf + len, "SLAVE_ERR ");
			buf[len - 1] = ',';
		}

	} else {
		/* C2H only */
		if ((v & XDMA_STAT_C2H_R_ERR_MASK)) {
			len += sprintf(buf + len, "R:");
			if ((v & XDMA_STAT_C2H_R_DECODE_ERR))
				len += sprintf(buf + len, "DECODE_ERR ");
			if ((v & XDMA_STAT_C2H_R_SLAVE_ERR))
				len += sprintf(buf + len, "SLAVE_ERR ");
			buf[len - 1] = ',';
		}
	}

	/* common H2C & C2H */
 	if ((v & XDMA_STAT_DESC_ERR_MASK)) {
		len += sprintf(buf + len, "DESC_ERR:");
		if ((v & XDMA_STAT_DESC_UNSUPP_REQ))
			len += sprintf(buf + len, "UNSUPP_REQ ");
		if ((v & XDMA_STAT_DESC_COMPL_ABORT))
			len += sprintf(buf + len, "COMPL_ABORT ");
		if ((v & XDMA_STAT_DESC_PARITY_ERR))
			len += sprintf(buf + len, "PARITY ");
		if ((v & XDMA_STAT_DESC_HEADER_EP))
			len += sprintf(buf + len, "HEADER_EP ");
		if ((v & XDMA_STAT_DESC_UNEXP_COMPL))
			len += sprintf(buf + len, "UNEXP_COMPL ");
		buf[len - 1] = ',';
	}

	buf[len - 1] = '\0';
	pr_info("%s\n", buffer);
}

static u32 engine_status_read(struct xdma_engine *engine, bool clear, bool dump)
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
		engine_status_dump(engine);

	return value;
}

/**
 * xdma_engine_stop() - stop an SG DMA engine
 *
 */
static int xdma_engine_stop(struct xdma_engine *engine)
{
	u32 w;

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}
	dbg_tfr("xdma_engine_stop(engine=%p)\n", engine);


	w = 0;
	w |= (u32)XDMA_CTRL_IE_DESC_ALIGN_MISMATCH;
	w |= (u32)XDMA_CTRL_IE_MAGIC_STOPPED;
	w |= (u32)XDMA_CTRL_IE_READ_ERROR;
	w |= (u32)XDMA_CTRL_IE_DESC_ERROR;

	if (poll_mode) {
		w |= (u32)XDMA_CTRL_POLL_MODE_WB;
	} else {
		w |= (u32)XDMA_CTRL_IE_DESC_STOPPED;
		w |= (u32)XDMA_CTRL_IE_DESC_COMPLETED;

	}

	dbg_tfr("Stopping SG DMA %s engine; writing 0x%08x to 0x%p.\n",
			engine->name, w, (u32 *)&engine->regs->control);
	write_register(w, &engine->regs->control,
		       (unsigned long)(&engine->regs->control) -
		       (unsigned long)(&engine->regs));
	/* dummy read of status register to flush all previous writes */
	dbg_tfr("xdma_engine_stop(%s) done\n", engine->name);
	return 0;
}


static int engine_start_mode_config(struct xdma_engine *engine)
{
	u32 wr;

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}

	/* If a perf test is running, enable the engine interrupts */
	if (engine->xdma_perf) {
		wr = XDMA_CTRL_IE_DESC_STOPPED;
		wr |= XDMA_CTRL_IE_DESC_COMPLETED;
		wr |= XDMA_CTRL_IE_DESC_ALIGN_MISMATCH;
		wr |= XDMA_CTRL_IE_MAGIC_STOPPED;
		wr |= XDMA_CTRL_IE_IDLE_STOPPED;
		wr |= XDMA_CTRL_IE_READ_ERROR;
		wr |= XDMA_CTRL_IE_DESC_ERROR;

		write_register(
			wr, &engine->regs->interrupt_enable_mask,
			(unsigned long)(&engine->regs->interrupt_enable_mask) -
			(unsigned long)(&engine->regs));
	}

	/* write control register of SG DMA engine */
	wr = (u32)XDMA_CTRL_RUN_STOP;
	wr |= (u32)XDMA_CTRL_IE_READ_ERROR;
	wr |= (u32)XDMA_CTRL_IE_DESC_ERROR;
	wr |= (u32)XDMA_CTRL_IE_DESC_ALIGN_MISMATCH;
	wr |= (u32)XDMA_CTRL_IE_MAGIC_STOPPED;

	if (poll_mode) {
		wr |= (u32)XDMA_CTRL_POLL_MODE_WB;
	} else {
		wr |= (u32)XDMA_CTRL_IE_DESC_STOPPED;
		wr |= (u32)XDMA_CTRL_IE_DESC_COMPLETED;
		/* set non-incremental addressing mode */
		if (engine->non_incr_addr)
			wr |= (u32)XDMA_CTRL_NON_INCR_ADDR;
	}

	/* start the engine */
	write_register(wr, &engine->regs->control,
		       (unsigned long)(&engine->regs->control) -
		       (unsigned long)(&engine->regs));

	dbg_tfr("iowrite32(0x%08x to 0x%p) (control)\n", wr,
		(void *)&engine->regs->control);

	return 0;
}


/**
 * engine_service_shutdown() - stop servicing an SG DMA engine
 *
 * must be called with engine->lock already acquired
 *
 * @engine pointer to struct xdma_engine
 *
 */
static int engine_service_shutdown(struct xdma_engine *engine)
{
	int rv;
	/* if the engine stopped with RUN still asserted, de-assert RUN now */
	dbg_tfr("engine just went idle, resetting RUN_STOP.\n");
	rv = xdma_engine_stop(engine);
	if (rv < 0) {
		pr_err("Failed to stop engine\n");
		return rv;
	}
	engine->running = 0;
	engine->desc_dequeued = 0;

	/* awake task on engine's shutdown wait queue */
	wake_up_interruptible(&engine->shutdown_wq);
	return 0;
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

	/* Clear adjacent count for last descriptor in first block
	 * also clear out STOPPED and COMPLETION bit.
	 * TODO - Need to improve this algorithm for setting COMPLETION BIT.
	 */
	u32 control = le32_to_cpu(first->control) & 0xffffc0fcUL;
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

	/* write bytes and next_num */
	first->control = cpu_to_le32(control);
}

/* xdma_desc_adjacent -- Set how many descriptors are adjacent to this one */
static void xdma_desc_adjacent(struct xdma_desc *desc, int next_adjacent)
{
	u32 max_extra_adj = 0x3F;
	/* remember reserved and control bits */
	u32 control = le32_to_cpu(desc->control) & 0xffffc0ffUL;

	if (next_adjacent)
		next_adjacent = next_adjacent - 1;

	if (next_adjacent >= desc_set_depth)
		next_adjacent = desc_set_depth - 1;
	if (next_adjacent > max_extra_adj)
		next_adjacent = max_extra_adj;

	control |= (next_adjacent << 8);
	/* write control and next_adjacent */
	desc->control = cpu_to_le32(control);
}

/* xdma_desc_control -- Set complete control field of a descriptor. */
static int xdma_desc_control_set(struct xdma_desc *first, u32 control_field)
{
	/* remember magic and adjacent number */
	u32 control = le32_to_cpu(first->control) & ~(LS_BYTE_MASK);

	if (control_field & ~(LS_BYTE_MASK)) {
		pr_err("Invalid control field\n");
		return -EINVAL;
	}
	/* merge adjacent and control field */
	control |= control_field;
	/* write control and next_adjacent */
	first->control = cpu_to_le32(control);
	return 0;
}


/* xdma_desc_done - recycle cache-coherent linked list of descriptors.
 *
 * @dev Pointer to pci_dev
 * @number Number of descriptors to be allocated
 * @desc_virt Pointer to (i.e. virtual address of) first descriptor in list
 * @desc_bus Bus address of first descriptor in list
 */
static inline void xdma_desc_done(struct xdma_desc *desc_virt, int count)
{
	memset(desc_virt, 0, count * sizeof(struct xdma_desc));
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
	desc->control = DESC_MAGIC;
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

static inline void enable_interrupts(struct xdma_engine *engine)
{
	/* re-enable interrupts for this engine */
	if (engine->xdev->msix_enabled) {
		write_register(
				engine->interrupt_enable_mask_value,
				&engine->regs->interrupt_enable_mask_w1s,
				(unsigned long)(&engine->regs
					->interrupt_enable_mask_w1s) -
				(unsigned long)(&engine->regs));
	} else
		channel_interrupts_enable(engine->xdev, engine->irq_bitmask);
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
static int engine_start(struct xdma_engine *engine,
		dma_addr_t desc_bus, unsigned int desc_adjacent)
{
	u32 w;
	int extra_adj = desc_adjacent - 1;
	u32 max_extra_adj = 0x3F;
	int rv;

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}

	/* remember the engine is running */
	engine->running = 1;
	engine->desc_dequeued = 0;

	/* engine is no longer shutdown */
	engine->shutdown = ENGINE_SHUTDOWN_NONE;
	if (engine->streaming && (engine->dir == DMA_FROM_DEVICE) &&
			enable_credit_mp) {
		write_register(desc_set_depth,
			       &engine->sgdma_regs->credits, 0);
	}

	/* write lower 32-bit of bus address of transfer first descriptor */
	w = cpu_to_le32(PCI_DMA_L(desc_bus));
	dbg_tfr("iowrite32(0x%08x to 0x%p) (first_desc_lo)\n", w,
		(void *)&engine->sgdma_regs->first_desc_lo);
	write_register(w, &engine->sgdma_regs->first_desc_lo,
		       (unsigned long)(&engine->sgdma_regs->first_desc_lo) -
			       (unsigned long)(&engine->sgdma_regs));
	/* write upper 32-bit of bus address of transfer first descriptor */
	w = cpu_to_le32(PCI_DMA_H(desc_bus));
	dbg_tfr("iowrite32(0x%08x to 0x%p) (first_desc_hi)\n", w,
		(void *)&engine->sgdma_regs->first_desc_hi);
	write_register(w, &engine->sgdma_regs->first_desc_hi,
		       (unsigned long)(&engine->sgdma_regs->first_desc_hi) -
			       (unsigned long)(&engine->sgdma_regs));

	if (extra_adj >= desc_set_depth)
		extra_adj = desc_set_depth - 1;
	if (extra_adj > max_extra_adj)
		extra_adj = max_extra_adj;
	dbg_tfr("iowrite32(0x%08x to 0x%p) (first_desc_adjacent)\n", extra_adj,
		(void *)&engine->sgdma_regs->first_desc_adjacent);
	write_register(
		extra_adj, &engine->sgdma_regs->first_desc_adjacent,
		(unsigned long)(&engine->sgdma_regs->first_desc_adjacent) -
			(unsigned long)(&engine->sgdma_regs));

	dbg_tfr("ioread32(0x%p) (dummy read flushes writes).\n",
		&engine->regs->status);
	mmiowb();

	rv = engine_start_mode_config(engine);

	if (rv < 0) {
		pr_err("Failed to start engine mode config\n");
		return -EINVAL;
	}

	dbg_tfr("%s engine 0x%p now running\n", engine->name, engine);

	return 0;
}

static void xdma_request_free(struct xdma_request_cb *req)
{
	if (((unsigned long)req) >= VMALLOC_START &&
	    ((unsigned long)req) < VMALLOC_END)
		vfree(req);
	else
		kfree(req);
}

static void xdma_request_release(struct xdma_dev *xdev,
			struct xdma_request_cb *req)
{
	struct sg_table *sgt = req->sgt;
	if (!req->dma_mapped) {
		dma_unmap_sg(&xdev->pdev->dev, sgt->sgl, sgt->orig_nents,
			     req->dir);
	}

	xdma_request_free(req);
}

static int free_desc_set(struct xdma_engine *engine,
		unsigned int desc_dequeued) {
	struct desc_sets *s;
	unsigned int desc_cnt = 0;
	unsigned int prev_cidx;
	unsigned int avail_sets = 0;
	int ret = -EBUSY;

	spin_lock(&engine->desc_lock);
	while (desc_dequeued) {
		prev_cidx = engine->cidx;
		s = &engine->sets[prev_cidx];

		desc_cnt = min_t(unsigned int, desc_dequeued,
				 s->desc_set_offset);
		if (!desc_cnt)
			break;
		s->desc_set_offset -= desc_cnt;
		desc_dequeued -= desc_cnt;
		if (s->desc_set_offset == 0) {
			engine->cidx = incr_ptr_idx(prev_cidx, 1,
						       XDMA_DESC_SETS_MAX);
			engine->avail_sets++;
			s->last_set = 0;
			ret = 0;
			dbg_tfr("free desc set cidx = %u/%u/%u/%u",
				s->desc_set_offset, engine->cidx,
				engine->pidx, desc_dequeued);
		}
		avail_sets = engine->avail_sets;
		if (avail_sets > XDMA_DESC_SETS_AVAIL_MAX)
			break;
	}
	spin_unlock(&engine->desc_lock);

	return ret;
}

static int process_completions(struct xdma_engine *engine,
		unsigned int desc_dequeued)
{
	unsigned int desc_count;
	struct xdma_request_cb *req;
	struct list_head *req_list_head = &engine->pend_list;
	unsigned int released_desc = desc_dequeued;
	int i, ret;

	spin_lock(&engine->req_list_lock);
	if (list_empty(&engine->pend_list) && list_empty(&engine->work_list)) {
		spin_unlock(&engine->req_list_lock);
		 /* requests expired */
		return free_desc_set(engine, released_desc);
	}
	spin_unlock(&engine->req_list_lock);
	while (desc_dequeued) {
		req_list_head = &engine->pend_list;
		spin_lock(&engine->req_list_lock);
		if (list_empty(&engine->pend_list)) {
			req_list_head = &engine->work_list;
			if (list_empty(req_list_head)) {
				spin_unlock(&engine->req_list_lock);
				break;
			}
		}
		req = list_first_entry(req_list_head,
				struct xdma_request_cb,	entry);
		desc_count = min_t(unsigned int,
				req->sw_desc_idx - req->desc_completed,
				desc_dequeued);
		if (!desc_count) {
			/*these release descriptors are from some expired
			 * requests*/
			spin_unlock(&engine->req_list_lock);
			break;
		}

		/* Update completed transfer length */
		for (i = req->desc_completed;
				i <(desc_count + req->desc_completed); i++)
			req->done  += req->sdesc[i].len;

		req->desc_completed += desc_count;
		desc_dequeued -= desc_count;

		if (req->sw_desc_cnt == req->desc_completed) {
			list_del(&req->entry);
			if (req->cb && req->cb->io_done) {
				struct xdma_io_cb *cb = req->cb;

				cb->done_bytes = req->done;
				spin_unlock(&engine->req_list_lock);
				cb->io_done((unsigned long)cb->private, 0);
				spin_lock(&engine->req_list_lock);
				xdma_request_release(engine->xdev, req);
			} else
				wake_up(&req->arbtr_wait);
		}
		spin_unlock(&engine->req_list_lock);
	}
	/* at any pint of time only one desc set is given for processing,
	 * so only descriptors related to only 1 desc set can be released */
	ret = free_desc_set(engine, released_desc);

	return ret;
}

static struct xdma_request_cb *xdma_request_alloc(struct sg_table *sgt)
{
	unsigned sdesc_nr = 0;
	struct xdma_request_cb *req;
	unsigned int len;
	int i, extra = 0;
	unsigned int size;

	if (sgt) {
		struct scatterlist *sg = sgt->sgl;

		for (i = 0;  i < sgt->nents ; i++, sg = sg_next(sg)) {
			len = sg_dma_len(sg);

			if (unlikely(len > desc_blen_max))
				extra += len >> XDMA_DESC_BLEN_BITS;
		}

		sdesc_nr = sgt->nents + extra;
	}

	size = sizeof(struct xdma_request_cb) +
			    sdesc_nr * sizeof(struct sw_desc);

	req = kzalloc(size, GFP_KERNEL);
	if (!req) {
		req = vmalloc(size);
		if (req)
			memset(req, 0, size);
	}
	if (!req) {
		xocl_pr_info("OOM, %u sw_desc, %u.\n", sdesc_nr, size);
		return NULL;
	}

	req->sw_desc_cnt = sdesc_nr;

	return req;
}

static int xdma_init_request(struct xdma_request_cb *req)
{
	struct sg_table *sgt = req->sgt;
	struct scatterlist *sg = sgt->sgl;
	int i, j = 0;

	for (i = 0, sg = sgt->sgl; i < sgt->nents; i++, sg = sg_next(sg)) {
		unsigned int tlen = sg_dma_len(sg);
		dma_addr_t addr = sg_dma_address(sg);

		req->total_len += tlen;
		while (tlen) {
			req->sdesc[j].addr = addr;
			if (tlen > desc_blen_max) {
				req->sdesc[j].len = desc_blen_max;
				addr += desc_blen_max;
				tlen -= desc_blen_max;
			} else {
				req->sdesc[j].len = tlen;
				tlen = 0;
			}
			j++;

		}
	}

#ifdef __LIBXDMA_DEBUG__
	xdma_request_cb_dump(req);
#endif
	return 0;
}

static void xdma_add_request(struct xdma_engine *engine,
		struct xdma_request_cb *req)
{
	init_waitqueue_head(&req->arbtr_wait);
	spin_lock(&engine->req_list_lock);
	list_add_tail(&req->entry, &engine->work_list);
	spin_unlock(&engine->req_list_lock);
}

/* returns Num of Descriptors filled for DMA transfer */
/* Fill proper virt and bus address before calling this func */
static void request_build(struct xdma_engine *engine,
			  struct xdma_desc *desc_virt,
			  struct xdma_request_cb *req, unsigned int desc_max)
{
	struct sw_desc *sdesc;
	struct xdma_result *result_virt;
	int i;
	unsigned int result_pidx = engine->result_pidx;
	dma_addr_t result_addr;

	sdesc = &(req->sdesc[req->sw_desc_idx]);

	for (i = 0; i < desc_max; i++, sdesc++) {
		/* fill in descriptor entry j with transfer details */
		xdma_desc_set(desc_virt + i, sdesc->addr, req->ep_addr,
			      sdesc->len, engine->dir);
		if (engine->streaming && engine->dir == DMA_FROM_DEVICE) {
			result_addr = engine->cyclic_result_bus +
					(result_pidx *
						sizeof(struct xdma_result));
			result_virt = engine->cyclic_result + result_pidx;
			result_virt->length = 0;
			result_virt->status = 0;
			desc_virt[i].src_addr_hi =
					cpu_to_le32(PCI_DMA_H(result_addr));
			desc_virt[i].src_addr_lo =
					cpu_to_le32(PCI_DMA_L(result_addr));
			result_pidx = incr_ptr_idx(result_pidx, 1,
						      (XDMA_DESC_SETS_MAX *
						      desc_set_depth));
		}


		/* for non-inc-add mode don't increment ep_addr */
		if (!engine->non_incr_addr)
			req->ep_addr += sdesc->len;
	}
	engine->result_pidx = result_pidx;
	req->sw_desc_idx += desc_max;
}

static void request_desc_init(struct xdma_desc *desc_virt,
		dma_addr_t desc_bus, unsigned int count)
{
	int i;

	if (!count)
		return;
	/* create singly-linked list for SG DMA controller */
	for (i = 0; i < count; i++) {
		/* increment bus address to next in array */
		desc_bus += sizeof(struct xdma_desc);

		desc_virt[i].next_lo = cpu_to_le32(PCI_DMA_L(desc_bus));
		desc_virt[i].next_hi = cpu_to_le32(PCI_DMA_H(desc_bus));
	}
}

static int queue_request(struct xdma_engine *engine,
		dma_addr_t desc_bus, unsigned int desc_count)
{
	int rv = 0;
	struct xdma_dev *xdev;

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}

	if (!engine->xdev) {
		pr_err("Invalid xdev\n");
		return -EINVAL;
	}

	xdev = engine->xdev;
	if (xdma_device_flag_check(xdev, XDEV_FLAG_OFFLINE)) {
		xocl_pr_info("dev 0x%p offline\n", xdev);
		return -EBUSY;
	}

	engine->prev_cpu = get_cpu();
	put_cpu();

	/* engine is being shutdown; do not accept new transfers */
	if (engine->shutdown & ENGINE_SHUTDOWN_REQUEST) {
		xocl_pr_info("engine %s offline\n", engine->name);
		rv = -EBUSY;
		goto shutdown;
	}

	if (!poll_mode)
		enable_interrupts(engine);

	/* start engine */
	rv = engine_start(engine, desc_bus, desc_count);
	if (rv < 0)
		goto shutdown;

	if (poll_mode)
		schedule_work_on(engine->cpu_idx, &engine->poll);

	return rv;
shutdown:
	engine->running = 0;
	dbg_tfr("engine->running = %d\n", engine->running);
	return rv;
}

static inline int xdma_get_desc_set(struct xdma_engine *engine)
{
	int pidx = -EBUSY;

	if (engine->avail_sets)
		pidx = engine->pidx;

	return pidx;
}
/*
 * Fetch the First PENDING REQUEST from running list OR
 * Fetch the First REQUEST FROM waiting list
 */
static struct xdma_request_cb *xdma_fetch_request(struct xdma_engine *engine)
{
	struct xdma_request_cb *req = NULL;

	spin_lock(&engine->req_list_lock);
	req = list_first_entry_or_null(&engine->work_list,
			struct xdma_request_cb,	entry);
	spin_unlock(&engine->req_list_lock);

	return req;
}

static void config_last_desc(struct xdma_engine *engine, struct desc_sets *s,
		struct xdma_desc *last_desc)
{
	if (!s->last_set) {
		last_desc->control |= cpu_to_le32(XDMA_DESC_STOPPED |
				XDMA_DESC_COMPLETED);
		last_desc->next_lo = cpu_to_le32(0);
		last_desc->next_hi = cpu_to_le32(0);
		engine->sets_ready++;
		s->last_set = 1;
	}

	dbg_tfr("%s", __func__);
}

/*
 * Create chained descriptor list by linking two descriptor sets in READY state.
 * Also update COMPLETION bit and STOPPED bit accordingly.
 */
static void xdma_link_sets(struct xdma_engine *engine,
				struct desc_sets *first,
				struct desc_sets *second,
				unsigned int cidx_submit)
{
	struct xdma_desc *last_desc;
	struct xdma_desc *desc_virt;
	dma_addr_t desc_bus;
	unsigned int s_cidx;

	desc_virt = engine->desc + (cidx_submit * desc_set_depth);
	last_desc = &desc_virt[first->desc_set_offset - 1];
	s_cidx = engine->sw_cidx;
	desc_virt = engine->desc + (s_cidx * desc_set_depth);
	desc_bus = engine->desc_bus + ((s_cidx * desc_set_depth) *
				sizeof(struct xdma_desc));

	xdma_desc_link(last_desc, desc_virt, desc_bus);

	return;
}

static int xdma_request_desc_init(struct xdma_engine *engine,
				  unsigned char req_submit)
{
	struct xdma_request_cb *req = NULL;
	struct xdma_desc *desc_virt;
	struct desc_sets *s;
	struct desc_sets *first, *second;
	dma_addr_t desc_bus;
	unsigned int count;
	unsigned int desc_set_offset;
	int pidx = -EBUSY;
	int rv;
	int old_pidx = 0;
	unsigned char desc_setup_yield = req_submit;
	int i;
	unsigned int desc_cnt_submit = 0, submit_cnt = 0;
	unsigned int cidx_submit = 0, cidx_link = 0;
	unsigned char eop = 0;

	/* Loop Runs until REQ are none or Desc Sets consumed */
	req = NULL;
	desc_set_offset = 0;
	do {
		req = xdma_fetch_request(engine);
		if (req == NULL) {
			if (desc_set_offset || req_submit) {
				dbg_tfr("going to submit for pidx = %d", pidx);
				spin_lock(&engine->desc_lock);
				goto submit_req;
			}
			return 0;
		}
		spin_lock(&engine->desc_lock);
		if (!desc_set_offset) {
			pidx = xdma_get_desc_set(engine);
			if (pidx == -EBUSY) {
				if (req_submit)
					goto submit_req;
				spin_unlock(&engine->desc_lock);
				return 0;
			}
			s = &engine->sets[pidx];
			if (s->desc_set_offset) {
				 /* someone already using this desc set,
				  * dont jump the gun, go and wait */
				if (req_submit)
					goto submit_req;
				spin_unlock(&engine->desc_lock);
				return 0;
			}
			 /* if repeatedly same pidx, not going anywhere,
			  * yield */
			if (old_pidx == pidx)
				desc_setup_yield = 1;
			else
				old_pidx = pidx;
		}
		if (req->sw_desc_cnt == req->sw_desc_idx) {
			/* req already setup, do no redo */
			desc_setup_yield = 1;
			goto submit_req;
		}
		dbg_tfr("%u-%u: req desc proced = %u/%u - %u",
			pidx, desc_set_offset,	req->sw_desc_idx,
			req->sw_desc_cnt, engine->avail_sets);

		s = &engine->sets[pidx];
		desc_virt = engine->desc + (pidx * desc_set_depth);
		desc_bus = engine->desc_bus + ((pidx * desc_set_depth) *
				sizeof(struct xdma_desc));
		/* index is to track partially completed desc set */
		count = min_t(unsigned int, req->sw_desc_cnt - req->sw_desc_idx,
			      desc_set_depth - desc_set_offset);
		BUG_ON(!count);
		/* Updates Bus Address of DATA SEGMENTS from SW DESC */
		request_build(engine, desc_virt + desc_set_offset, req, count);
		eop = (req->sw_desc_cnt == req->sw_desc_idx) ? 1 : 0;
		/* Keep Track of Consumed Descriptors per Req */
		request_desc_init(desc_virt + desc_set_offset, desc_bus +
				  (desc_set_offset * sizeof(struct xdma_desc)),
				  count);
		desc_set_offset += count;
		s->desc_set_offset = desc_set_offset;
		if (eop)
			desc_virt[desc_set_offset - 1].control |=
					cpu_to_le32(XDMA_DESC_EOP);

		if (eop) {
			dbg_tfr("EOP desc control = %x",
				desc_virt[s->desc_set_offset - 1].control);
			/* whole request processed */
			spin_lock(&engine->req_list_lock);
			list_del(&req->entry);
			list_add_tail(&req->entry, &engine->pend_list);
			spin_unlock(&engine->req_list_lock);
		} else {
			/* Submit partial request */
			goto submit_req;
		}
		if (desc_set_offset < desc_set_depth) {
			spin_unlock(&engine->desc_lock);
			schedule(); /* time for other threads to do something */
			continue; /* get new request to fill desc set */
		}
submit_req:
		dbg_tfr("pidx = %u, cidx = %u %u - %u", pidx,
			engine->cidx, desc_set_offset, engine->avail_sets);
		if ((pidx >= 0) && desc_set_offset) { /* something new is setup */
			s = &engine->sets[pidx];
			desc_virt = engine->desc + (pidx * desc_set_depth);
			config_last_desc(engine, s,
					 &desc_virt[s->desc_set_offset - 1]);
			dbg_tfr("last desc control = %x/%u/0x%x",
				desc_virt[s->desc_set_offset - 1].control,
				s->desc_set_offset, desc_virt[s->desc_set_offset - 1].next_lo);
			for (i = 0; i < s->desc_set_offset; i++) {
				xdma_desc_adjacent(desc_virt + i,
						   s->desc_set_offset -
						   i - 1);
				dbg_tfr("%s:[%d]desc->control = 0x%x pidx=%u next=0x%x",
					engine->name, i,
					desc_virt[i].control, pidx,
					desc_virt[i].next_lo);
			}
			for (i = s->desc_set_offset; i < desc_set_depth; i++)
				desc_virt[i].control = 0;
			/* fill in adjacent numbers */
			if (engine->avail_sets) {
				engine->pidx = incr_ptr_idx(pidx, 1,
							    XDMA_DESC_SETS_MAX);
				engine->avail_sets--;
			}
			desc_set_offset = 0; /* going for next set */
		}
		if (engine->avail_sets >= XDMA_DESC_SETS_AVAIL_MAX) {
			/* Nothing to submit */
			spin_unlock(&engine->desc_lock);
			break;
		}
		spin_unlock(&engine->desc_lock);

		spin_lock(&engine->lock);
		/* if engine is not free, cannot progress further */
		if (engine->running) {
			spin_unlock(&engine->lock);
			if (desc_setup_yield)
				break;
			else
				continue;
		}
		/* we submit 1 set at a time, the base of set is always 0.
		 * Also, we have produced a set, but submission should always
		 * happen from cidx */
		spin_lock(&engine->desc_lock);
		if (!engine->sets_ready) {
			spin_unlock(&engine->desc_lock);
			spin_unlock(&engine->lock);
			if (desc_setup_yield)
				break;
			else
				continue;
			break;
		}
		cidx_submit = engine->sw_cidx;
		s = &engine->sets[cidx_submit];
		desc_cnt_submit = s->desc_set_offset;
		engine->sets_ready--;
		engine->sw_cidx = incr_ptr_idx(cidx_submit, 1, XDMA_DESC_SETS_MAX);
		cidx_link = cidx_submit;
		submit_cnt = desc_cnt_submit;

		/*
		 * Chain multiple descriptor sets which are ready
		 * to be submitted. Chain as much as possible
		 * for now.
		 * TODO: Make change to increase completion frequency
		 * by using configurable parameter to set COMPLETION bit.
		 */
		while (engine->sets_ready) {
			first = s;
			second = &engine->sets[engine->sw_cidx];
			xdma_link_sets(engine, first, second , cidx_link);
			s = second;
			cidx_link = engine->sw_cidx;
			engine->sw_cidx = incr_ptr_idx(engine->sw_cidx, 1,
						       XDMA_DESC_SETS_MAX);
			submit_cnt += s->desc_set_offset;
			engine->sets_ready--;

		}

		/* TODO: this keeps track of submitted descritors.
		 * Used while processing completions to see if more
		 * completions are expected or not.
		 */
		engine->desc_queued = submit_cnt;


		spin_unlock(&engine->desc_lock);

		rv = queue_request(engine, engine->desc_bus +
				   ((u64)cidx_submit * desc_set_depth * sizeof(struct xdma_desc)),
				   desc_cnt_submit);
		desc_cnt_submit = 0;
		spin_unlock(&engine->lock);
		if (rv < 0) {
			spin_lock(&engine->desc_lock);
			engine->sets_ready++;
			spin_unlock(&engine->desc_lock);
		}
		/* (rv < 0) we can still setup descriptors if resources
		 * available */
		if ((rv < 0) && desc_setup_yield)
			break; /* lingered long enough */
		else
			schedule(); /* time for other threads to do something */
	} while (1);

	return 0;
}

static int xdma_process_requests(struct xdma_engine *engine,
		struct xdma_request_cb *req)
{
	int rv;
	long int timeout;

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}

	if (!req) {
		pr_err("engine %s request NULL\n", engine->name);
		return -EINVAL;
	}

	rv = xdma_request_desc_init(engine, 0);
	if (rv < 0) {
		pr_err("Failed to perform descriptor init\n");
		return rv;
	}
	if ((!req->cb || !req->cb->io_done) && (rv == 0)) {
		timeout = wait_event_timeout(req->arbtr_wait,
				req->sw_desc_cnt == req->desc_completed,
				msecs_to_jiffies(10000)); /* 10sec timeout */

		/* Check for timeouts*/
		if (timeout == 0) {
			pr_err("Request completion timeout\n");
			engine_reg_dump(engine);
			check_nonzero_interrupt_status(engine->xdev);
			rv = -EIO;
		}

	}
	if (req->cb && req->cb->io_done) {
		req->expiry = jiffies + msecs_to_jiffies(10000);
		schedule_work_on(engine->cpu_idx, &engine->aio_mon);
	}

	return rv;
}

static int engine_service_requests(struct xdma_engine *engine,
				   int desc_writeback)
{
	u32 desc_count = desc_writeback & WB_COUNT_MASK;
	u32 err_flag = desc_writeback & WB_ERR_MASK;
	int rv = 0;

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}
	dbg_tfr("Interrupt raised for %s-%s", engine->streaming ? "ST":"MM",
			(engine->dir == DMA_FROM_DEVICE) ? "C2H":"H2C");

	/* If polling detected an error, signal to the caller */
	if (err_flag)
		rv = -1;

	/*
	 * If called by the ISR or polling detected an error, read and clear
	 * engine status. For polled mode descriptor completion, this read is
	 * unnecessary and is skipped to reduce latency
	 */
	if ((desc_count == 0) || (err_flag != 0)) {
		rv = engine_status_read(engine, 1, 0);
		if (rv < 0) {
			pr_err("Failed to read engine status\n");
			return rv;
		}
	}

	if (((engine->dir == DMA_FROM_DEVICE) &&
				(engine->status & XDMA_STAT_C2H_ERR_MASK)) ||
			((engine->dir == DMA_TO_DEVICE) &&
			 (engine->status & XDMA_STAT_H2C_ERR_MASK))) {

		pr_err("engine %s, status error 0x%x.\n", engine->name,
				engine->status);
		engine_status_dump(engine);
		engine_reg_dump(engine);
	}

	if (engine->streaming && (engine->dir == DMA_FROM_DEVICE)) {
		unsigned int result_cidx;
		struct xdma_result *result_virt;
		struct desc_sets *s;
		unsigned long timeout;
		unsigned int desc_max;

		timeout = jiffies + (POLL_TIMEOUT_SECONDS * HZ);
		spin_lock(&engine->desc_lock);
		s = &engine->sets[engine->cidx];
		desc_max = s->desc_set_offset;
		spin_unlock(&engine->desc_lock);
loop_again:
		spin_lock(&engine->desc_lock);
		result_cidx = engine->result_cidx;
		result_virt = engine->cyclic_result + result_cidx;
		while ((result_virt->status & 0xFFFF0000) == 0x52B40000) {
			xocl_pr_info("received packet of length = %u/0x%x",
				result_virt->length, result_virt->status);
			desc_count += (result_virt->length + PAGE_SIZE -1) >>
					PAGE_SHIFT;
			result_cidx = incr_ptr_idx(result_cidx, 1,
						      (XDMA_DESC_SETS_MAX *
						      desc_set_depth));
			result_virt = engine->cyclic_result + result_cidx;
		}
		engine->result_cidx = result_cidx;
		spin_unlock(&engine->desc_lock);
		if ((poll_mode) && (desc_max > desc_count)) {
			if (time_after(jiffies, timeout)) {
				pr_err("Polling timed out");
				pr_err("expected wb = %u, actual = %u",
					 desc_max, desc_count);
			} else {
				schedule();
				goto loop_again;
			}
		}
	}

	spin_lock(&engine->lock);

	/*
	 * If called from the ISR, or if an error occurred, the descriptor
	 * count will be zero.  In this scenario, read the descriptor count
	 * from HW.  In polled mode descriptor completion, this read is
	 * unnecessary and is skipped to reduce latency
	 */
	if (!desc_count)
		desc_count = read_register(&engine->regs->completed_desc_count);

	if (!desc_count) {
		pr_err("desc count is zero\n");
		spin_unlock(&engine->lock);
		goto wait_cmpl;
	}

	desc_count = desc_count - engine->desc_dequeued;
	engine->desc_dequeued += desc_count;
	engine->desc_queued -= desc_count;
	spin_unlock(&engine->lock);

	/* completions need to be processed before engine shutdown */
	rv = process_completions(engine, desc_count);

wait_cmpl:
	/* Continue waiting for more completions if not all are dequeued */
	if (rv < 0 || engine->desc_queued) {
		if (poll_mode)
			schedule_work_on(engine->cpu_idx, &engine->poll);
		else {
			enable_interrupts(engine);
		}
		return 0; /* more descriptors need to be freed*/
	}
	/*
	 * engine was running but is no longer busy, or writeback occurred,
	 * shut down
	 */
	if (!(engine->status & XDMA_STAT_BUSY) ||
			(desc_count != 0)) {
		spin_lock(&engine->lock);
		rv = engine_service_shutdown(engine);
		if (rv < 0) {
			spin_unlock(&engine->lock);
			pr_err("Failed to shutdown engine\n");
			return rv;
		}
		spin_unlock(&engine->lock);
	} else {
		if (engine->status & XDMA_STAT_BUSY)
			pr_warn("engine %s is unexpectedly busy - ignoring\n",
					engine->name);
		if (engine->status & XDMA_STAT_BUSY) {
			u32 value = read_register(&engine->regs->status);
			if ((value & XDMA_STAT_BUSY))
				pr_err("%s has errors but is still BUSY\n",
						engine->name);
			return -EIO;
		}
	}

	spin_lock(&engine->desc_lock);
	if ((engine->avail_sets < XDMA_DESC_SETS_AVAIL_MAX) ||
			!list_empty(&engine->work_list)) {
		spin_unlock(&engine->desc_lock);
		/* something is pending */
		rv = xdma_request_desc_init(engine, 1);
		if (rv < 0)
			pr_err("Failed to perform descriptor init\n");
		return rv;
	}
	spin_unlock(&engine->desc_lock);

	return rv;
}


static void aio_request_monitor(struct work_struct *work)
{
	struct xdma_engine *engine;
	struct list_head *tmp, *entry;
	unsigned char reschedule = 0;
	unsigned char timedout = 0;

	engine = container_of(work, struct xdma_engine, req_proc);
	if (engine->magic != MAGIC_ENGINE) {
		pr_err("%s has invalid magic number %lx\n", engine->name,
		       engine->magic);
		return;
	}

	spin_lock(&engine->req_list_lock);
	list_for_each_safe(entry, tmp, &engine->pend_list) {
		struct xdma_request_cb *req = (struct xdma_request_cb *)entry;
		struct xdma_io_cb *cb = req->cb;

		if (cb && cb->io_done) {
			if (time_after(jiffies, req->expiry)) {
				list_del(&req->entry);
				xdma_request_release(engine->xdev, req);
				cb->io_done((unsigned long)engine->xdev,
						 -EIO);
				timedout = 1;
			} else
				reschedule = 1;
		}
	}
	list_for_each_safe(entry, tmp, &engine->work_list) {
		struct xdma_request_cb *req = (struct xdma_request_cb *)entry;
		struct xdma_io_cb *cb = req->cb;

		if (cb && cb->io_done) {
			if (time_after(jiffies, req->expiry)) {
				list_del(&req->entry);
				xdma_request_release(engine->xdev, req);
				cb->io_done((unsigned long)engine->xdev,
						 -EIO);
				timedout = 1;
			} else
				reschedule = 1;
		}
	}
	spin_unlock(&engine->req_list_lock);
	if (timedout) {
		pr_err("AIO reqs timedout");
		engine_status_dump(engine);
		engine_reg_dump(engine);
	}
	if (reschedule) {
		msleep(100);
		schedule_work_on(engine->cpu_idx, &engine->aio_mon);
	}
}

static void engine_process_requests(struct work_struct *work)
{
	struct xdma_engine *engine;
	int rv;

	engine = container_of(work, struct xdma_engine, req_proc);
	if (engine->magic != MAGIC_ENGINE) {
		pr_err("%s has invalid magic number %lx\n", engine->name,
		       engine->magic);
		return;
	}

	rv = xdma_request_desc_init(engine, 0);
	if (rv < 0)
		pr_err("Failed to perform descriptor init\n");
}


/**
 * engine_service() - service an SG DMA engine
 *
 * must be called with engine->lock already acquired
 *
 * @engine pointer to struct xdma_engine
 *
 */
static void engine_service_work(struct work_struct *work)
{
	struct xdma_engine *engine;
	int rv;

	engine = container_of(work, struct xdma_engine, work);
	if (engine->magic != MAGIC_ENGINE) {
		pr_err("%s has invalid magic number %lx\n", engine->name,
		       engine->magic);
		return;
	}

	engine->wq_serviced++;
	dbg_tfr("engine_service() for %s engine %p\n", engine->name, engine);
		rv = engine_service_requests(engine, 0);
		if (rv < 0)
			pr_err("Failed to service engine\n");
}

static u32 engine_service_wb_monitor(struct xdma_engine *engine,
				     u32 expected_wb)
{
	struct xdma_poll_wb *wb_data;
	u32 desc_wb = 0;
	u32 sched_limit = 0;
	unsigned long timeout;

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}
	wb_data = (struct xdma_poll_wb *)engine->poll_mode_addr_virt;

	/*
	 * Poll the writeback location for the expected number of
	 * descriptors / error events This loop is skipped for cyclic mode,
	 * where the expected_desc_count passed in is zero, since it cannot be
	 * determined before the function is called
	 */

	timeout = jiffies + (POLL_TIMEOUT_SECONDS * HZ);
	while (expected_wb != 0) {
		desc_wb = wb_data->completed_desc_count;

		if (desc_wb & WB_ERR_MASK)
			break;
		else if (desc_wb >= expected_wb)
			break;

		/* RTO - prevent system from hanging in polled mode */
		if (time_after(jiffies, timeout)) {
			pr_err("Polling timeout occurred");
			pr_err("desc_wb = 0x%08x, expected 0x%08x\n", desc_wb,
				expected_wb);
			if ((desc_wb & WB_COUNT_MASK) > expected_wb)
				desc_wb = expected_wb | WB_ERR_MASK;

			break;
		}

		/*
		 * Define NUM_POLLS_PER_SCHED to limit how much time is spent
		 * in the scheduler
		 */

		if (sched_limit != 0) {
			if ((sched_limit % NUM_POLLS_PER_SCHED) == 0)
				schedule();
		}
		sched_limit++;
	}

	return desc_wb;
}

static void engine_service_req_poll(struct work_struct *poll)
{
	struct xdma_engine *engine;
	struct xdma_poll_wb *writeback_data;
	struct desc_sets *s;
	unsigned int expected_desc_count;
	u32 desc_wb = 0;
	unsigned int mon_cidx;

	engine = container_of(poll, struct xdma_engine, poll);
	if (engine->magic != MAGIC_ENGINE) {
		pr_err("%s has invalid magic number %lx\n", engine->name,
				engine->magic);
		return;
	}
	if (engine->streaming && (engine->dir == DMA_FROM_DEVICE))
		goto service_wb;
	writeback_data = (struct xdma_poll_wb *)engine->poll_mode_addr_virt;
	/* only one desc set on ply, so addressing the current cidx is good
	 * enough*/
	spin_lock(&engine->desc_lock);
	mon_cidx = engine->cidx;
	s = &engine->sets[mon_cidx];
	expected_desc_count = s->desc_set_offset;
	spin_unlock(&engine->desc_lock);


	if (!expected_desc_count)
		return;

	if ((expected_desc_count & WB_COUNT_MASK) !=
			expected_desc_count) {
		pr_err("Queued descriptor count is larger than supported\n");
		return;
	}
	desc_wb = engine_service_wb_monitor(engine,
					    expected_desc_count);
	writeback_data->completed_desc_count = 0;
service_wb:
	engine_service_requests(engine, desc_wb);
}


static irqreturn_t user_irq_service(int irq, struct xdma_user_irq *user_irq)
{
	unsigned long flags;

	if (!user_irq) {
		pr_err("Invalid user_irq\n");
		return IRQ_NONE;
	}

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
	u32 mask;
	struct xdma_dev *xdev;
	struct interrupt_regs *irq_regs;
	unsigned long flags;

	dbg_irq("(irq=%d, dev 0x%p) <<<< ISR.\n", irq, dev_id);
	if (!dev_id) {
		pr_err("Invalid dev_id on irq line %d\n", irq);
		return -IRQ_NONE;
	}
	xdev = (struct xdma_dev *)dev_id;

	if (!xdev) {
		WARN_ON(!xdev);
		dbg_irq("%s(irq=%d) xdev=%p ??\n", __func__, irq, xdev);
		return IRQ_NONE;
	}

	spin_lock_irqsave(&xdev->lock, flags);
	irq_regs = (struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					     XDMA_OFS_INT_CTRL);

	/* read channel interrupt requests */
	ch_irq = read_register(&irq_regs->channel_int_request);
	dbg_irq("ch_irq = 0x%08x - mode %u\n", ch_irq, interrupt_mode);

	/*
	 * disable all interrupts that fired; these are re-enabled individually
	 * after the causing module has been fully serviced.
	 */
	if (ch_irq)
		channel_interrupts_disable(xdev, ch_irq);

	/* read user interrupts - this read also flushes the above write */
	user_irq = read_register(&irq_regs->user_int_request);
	dbg_irq("user_irq = 0x%08x\n", user_irq);

	if (user_irq) {
		int user = 0;
		u32 mask = 1;
		int max = xdev->h2c_channel_max;

		for (; user < max && user_irq; user++, mask <<= 1) {
			if (user_irq & mask) {
				user_irq &= ~mask;
				user_irq_service(irq, &xdev->user_irq[user]);
			}
		}
	}

	mask = ch_irq & xdev->mask_irq_h2c;
	if (mask) {
		int channel = 0;
		int max = xdev->h2c_channel_max;

		/* iterate over H2C (PCIe read) */
		for (channel = 0; channel < max && mask; channel++) {
			struct xdma_engine *engine = &xdev->engine_h2c[channel];

			/* engine present and its interrupt fired? */
			if ((engine->irq_bitmask & mask) &&
			    (engine->magic == MAGIC_ENGINE)) {
				mask &= ~engine->irq_bitmask;
				dbg_tfr("schedule_work, %s.\n", engine->name);
				schedule_work_on(engine->cpu_idx, &engine->work);
			}
		}
	}

	mask = ch_irq & xdev->mask_irq_c2h;
	if (mask) {
		int channel = 0;
		int max = xdev->c2h_channel_max;

		/* iterate over C2H (PCIe write) */
		for (channel = 0; channel < max && mask; channel++) {
			struct xdma_engine *engine = &xdev->engine_c2h[channel];

			/* engine present and its interrupt fired? */
			if ((engine->irq_bitmask & mask) &&
			    (engine->magic == MAGIC_ENGINE)) {
				mask &= ~engine->irq_bitmask;
				dbg_tfr("schedule_work, %s.\n", engine->name);
				schedule_work_on(engine->cpu_idx, &engine->work);
			}
		}
	}

	xdev->irq_count++;
	spin_unlock_irqrestore(&xdev->lock, flags);
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

	if (!dev_id) {
		pr_err("Invalid dev_id on irq line %d\n", irq);
		return IRQ_NONE;
	}
	user_irq = (struct xdma_user_irq *)dev_id;

	return user_irq_service(irq, user_irq);
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
	if (!dev_id) {
		pr_err("Invalid dev_id on irq line %d\n", irq);
		return IRQ_NONE;
	}

	engine = (struct xdma_engine *)dev_id;
	xdev = engine->xdev;

	if (!xdev) {
		WARN_ON(!xdev);
		dbg_irq("%s(irq=%d) xdev=%p ??\n", __func__, irq, xdev);
		return IRQ_NONE;
	}

	if (engine->f_fastpath) {
		engine->f_fastpath = false;
		complete(&engine->f_req_compl);
		return IRQ_HANDLED;
	}

	irq_regs = (struct interrupt_regs *)(xdev->bar[xdev->config_bar_idx] +
					     XDMA_OFS_INT_CTRL);
	/* Disable the interrupt for this engine */
	/* TODO: Need to see if this can be removed.
	 */
	write_register(
		engine->interrupt_enable_mask_value,
		&engine->regs->interrupt_enable_mask_w1c,
		(unsigned long)(&engine->regs->interrupt_enable_mask_w1c) -
		(unsigned long)(&engine->regs));

	/* Schedule the bottom half */
	schedule_work_on(engine->cpu_idx, &engine->work);

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
			iounmap(xdev->bar[i]);
			/* mark as unmapped */
			xdev->bar[i] = NULL;
		}
	}
}

static resource_size_t map_single_bar(struct xdma_dev *xdev,
					struct pci_dev *dev, int idx)
{
	resource_size_t bar_start;
	resource_size_t bar_len;
	resource_size_t map_len;

	bar_start = pci_resource_start(dev, idx);
	bar_len = pci_resource_len(dev, idx);
	map_len = bar_len;

	xdev->bar[idx] = NULL;

	/*
	 * do not map
	 * BARs with length 0. Note that start MAY be 0!
	 * USER and P2P bar (size >= 32M)
	 */
	xocl_pr_info("map bar %d, len %lld\n", idx, bar_len);
	/* do not map BARs with length 0. Note that start MAY be 0! */
	if (!bar_len || bar_len >= (1 << 25)) {
		xocl_pr_info("BAR #%d is not present - skipping\n", idx);
		return 0;
	}

	/*
	 * bail out if the bar is mapped
	 */
	if (!request_mem_region(bar_start, bar_len, xdev->mod_name))
		return 0;

	release_mem_region(bar_start, bar_len);

	/* BAR size exceeds maximum desired mapping? */
	if (bar_len > INT_MAX) {
		xocl_pr_info("Limit BAR %d mapping from %llu to %d bytes\n", idx,
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
		xocl_pr_info("Could not map BAR %d.\n", idx);
		return -1;
	}

	xocl_pr_info("BAR%d at 0x%llx mapped at 0x%p, length=%llu(/%llu)\n", idx,
		(u64)bar_start, xdev->bar[idx], (u64)map_len, (u64)bar_len);

	return (resource_size_t)map_len;
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

	if (!xdev->bar[idx])
		return 0;

	irq_id = read_register(&irq_regs->identifier);
	cfg_id = read_register(&cfg_regs->identifier);

	if (((irq_id & mask)== IRQ_BLOCK_ID) &&
	    ((cfg_id & mask)== CONFIG_BLOCK_ID)) {
		dbg_init("BAR %d is the XDMA config BAR\n", idx);
		flag = 1;
	} else {
		dbg_init("BAR %d is NOT the XDMA config BAR: 0x%x, 0x%x.\n",
			idx, irq_id, cfg_id);
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

	xocl_pr_info("xdev 0x%p, bars %d, config at %d.\n",
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
			xocl_pr_info("2, XDMA config BAR unexpected %d.\n",
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
			xocl_pr_info("3/4, XDMA config BAR unexpected %d.\n",
				config_bar_pos);
		}
		break;

	default:
		/* Should not occur - warn user but safe to continue */
		xocl_pr_info("Unexpected # BARs (%d), XDMA config BAR only.\n",
			num_bars);
		break;

	}
	xocl_pr_info("%d BARs: config %d, user %d, bypass %d.\n",
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
		resource_size_t bar_len;

		bar_len = map_single_bar(xdev, dev, i);
		if (bar_len == 0)
			continue;

		/* Try to identify BAR as XDMA control BAR */
		if ((bar_len >= XDMA_BAR_SIZE) && (xdev->config_bar_idx < 0)) {

			if (is_config_bar(xdev, i)) {
				xdev->config_bar_idx = i;
				config_bar_pos = bar_id_idx;
				xocl_pr_info("config bar %d, pos %d.\n",
					xdev->config_bar_idx, config_bar_pos);
			}
		}

		bar_id_list[bar_id_idx] = i;
		bar_id_idx++;
	}

	/* The XDMA config BAR must always be present */
	if (xdev->config_bar_idx < 0) {
		xocl_pr_info("Failed to detect XDMA config BAR\n");
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
 *	<h2c+c2h channel_max> vectors, followed by <user_max> vectors
 */

/*
 * RTO - code to detect if MSI/MSI-X capability exists is derived
 * from linux/pci/msi.c - pci_msi_check_device
 */

#ifndef arch_msi_check_device
static int arch_msi_check_device(struct pci_dev *dev, int nvec, int type)
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

	if (!interrupt_mode && msi_msix_capable(pdev, PCI_CAP_ID_MSIX)) {
		int req_nvec = xdev->c2h_channel_max + xdev->h2c_channel_max +
				 xdev->user_max;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
		dbg_init("Enabling MSI-X\n");
		rv = pci_alloc_irq_vectors(pdev, req_nvec, req_nvec,
					PCI_IRQ_MSIX);
#else
		int i;

		dbg_init("Enabling MSI-X\n");
		for (i = 0; i < req_nvec; i++)
			xdev->entry[i].entry = i;

		rv = pci_enable_msix(pdev, xdev->entry, req_nvec);
#endif
		if (rv < 0)
			dbg_init("Couldn't enable MSI-X mode: %d\n", rv);

		xocl_pr_info("request vectors: h2c %d, c2h %d, user %d\n",
			xdev->h2c_channel_max,
			xdev->c2h_channel_max, xdev->user_max);

		xdev->msix_enabled = 1;

	} else if (interrupt_mode == 1 &&
		   msi_msix_capable(pdev, PCI_CAP_ID_MSI)) {
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

static void pci_check_intr_pend(struct pci_dev *pdev)
{
	u16 v;

	pci_read_config_word(pdev, PCI_STATUS, &v);
	if (v & PCI_STATUS_INTERRUPT) {
		xocl_pr_info("%s PCI STATUS Interrupt pending 0x%x.\n",
                        dev_name(&pdev->dev), v);
		pci_write_config_word(pdev, PCI_STATUS, PCI_STATUS_INTERRUPT);
	}
}

static void pci_keep_intx_enabled(struct pci_dev *pdev)
{
	/* workaround to a h/w bug:
	 * when msix/msi become unavaile, default to legacy.
	 * However the legacy enable was not checked.
	 * If the legacy was disabled, no ack then everything stuck
	 */
	u16 pcmd, pcmd_new;

	pci_read_config_word(pdev, PCI_COMMAND, &pcmd);
	pcmd_new = pcmd & ~PCI_COMMAND_INTX_DISABLE;
	if (pcmd_new != pcmd) {
		xocl_pr_info("%s: clear INTX_DISABLE, 0x%x -> 0x%x.\n",
			dev_name(&pdev->dev), pcmd, pcmd_new);
		pci_write_config_word(pdev, PCI_COMMAND, pcmd_new);
	}
}

static void prog_irq_msix_user(struct xdma_dev *xdev, bool clear)
{
	/* user */
	struct interrupt_regs *int_regs = (struct interrupt_regs *)
					(xdev->bar[xdev->config_bar_idx] +
					 XDMA_OFS_INT_CTRL);
	u32 i = xdev->c2h_channel_max + xdev->h2c_channel_max;
	u32 max = i + xdev->user_max;
	int j;

	for (j = 0; i < max; j++) {
		u32 val = 0;
		int k;
		int shift = 0;

		if (clear)
			i += 4;
		else
			for (k = 0; k < 4 && i < max; i++, k++, shift += 8)
				val |= (i & 0x1f) << shift;

		write_register(val, &int_regs->user_msi_vector[j],
			XDMA_OFS_INT_CTRL +
			((unsigned long)&int_regs->user_msi_vector[j] -
			 (unsigned long)int_regs));

		dbg_init("vector %d, 0x%x.\n", j, val);
	}
}

static void prog_irq_msix_channel(struct xdma_dev *xdev, bool clear)
{
	struct interrupt_regs *int_regs = (struct interrupt_regs *)
					(xdev->bar[xdev->config_bar_idx] +
					 XDMA_OFS_INT_CTRL);
	u32 max = xdev->c2h_channel_max + xdev->h2c_channel_max;
	u32 i;
	int j;

	/* engine */
	for (i = 0, j = 0; i < max; j++) {
		u32 val = 0;
		int k;
		int shift = 0;

		if (clear)
			i += 4;
		else
			for (k = 0; k < 4 && i < max; i++, k++, shift += 8)
				val |= (i & 0x1f) << shift;

		write_register(val, &int_regs->channel_msi_vector[j],
			XDMA_OFS_INT_CTRL +
			((unsigned long)&int_regs->channel_msi_vector[j] -
			 (unsigned long)int_regs));
		dbg_init("vector %d, 0x%x.\n", j, val);
	}
}

static void irq_msix_channel_teardown(struct xdma_dev *xdev)
{
	struct xdma_engine *engine;
	int j = 0;
	int i = 0;

	if (!xdev->msix_enabled)
		return;

	prog_irq_msix_channel(xdev, 1);

 	engine = xdev->engine_h2c;
	for (i = 0; i < xdev->h2c_channel_max; i++, j++, engine++) {
		if (!engine->msix_irq_line)
			break;
		dbg_sg("Release IRQ#%d for engine %p\n", engine->msix_irq_line,
			engine);
		free_irq(engine->msix_irq_line, engine);
	}

 	engine = xdev->engine_c2h;
	for (i = 0; i < xdev->c2h_channel_max; i++, j++, engine++) {
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
	int j = xdev->h2c_channel_max;
	int rv = 0;
	u32 vector;
	struct xdma_engine *engine;

	if (!xdev->msix_enabled || xdev->no_dma)
		return 0;

	engine = xdev->engine_h2c;
	for (i = 0; i < xdev->h2c_channel_max; i++, engine++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
		vector = pci_irq_vector(xdev->pdev, i);
#else
		vector = xdev->entry[i].vector;
#endif
		rv = request_irq(vector, xdma_channel_irq, 0, xdev->mod_name,
				 engine);
		if (rv) {
			pr_err("requesti irq#%d failed %d, engine %s.\n",
				vector, rv, engine->name);
			return rv;
		}
		xocl_pr_info("engine %s, irq#%d.\n", engine->name, vector);
		engine->msix_irq_line = vector;
	}

	engine = xdev->engine_c2h;
	for (i = 0; i < xdev->c2h_channel_max; i++, j++, engine++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
		vector = pci_irq_vector(xdev->pdev, j);
#else
		vector = xdev->entry[j].vector;
#endif
		rv = request_irq(vector, xdma_channel_irq, 0, xdev->mod_name,
				 engine);
		if (rv) {
			xocl_pr_info("requesti irq#%d failed %d, engine %s.\n",
				vector, rv, engine->name);
			return rv;
		}
		xocl_pr_info("engine %s, irq#%d.\n", engine->name, vector);
		engine->msix_irq_line = vector;
	}

	return 0;
}

static void irq_msix_user_teardown(struct xdma_dev *xdev)
{
	int i;
	int j = xdev->h2c_channel_max + xdev->c2h_channel_max;

	if (!xdev->msix_enabled)
		return;

	prog_irq_msix_user(xdev, 1);

	for (i = 0; i < xdev->user_max; i++, j++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
		u32 vector = pci_irq_vector(xdev->pdev, j);
#else
		u32 vector = xdev->entry[j].vector;
#endif
		dbg_init("user %d, releasing IRQ#%d\n", i, vector);
		free_irq(vector, &xdev->user_irq[i]);
	}
}

static int irq_msix_user_setup(struct xdma_dev *xdev)
{
	int i;
	int j = xdev->h2c_channel_max + xdev->c2h_channel_max;
	int rv = 0;

	/*
	 * hard-code the number of dma channels to 2 for no dma mode.
	 * should not rely on the register to get the number of dma channels
	 *
	 * if (xdev->no_dma)
	 *	j = read_register(&reg->user_msi_vector) & 0xf;
	 */	

	/* vectors set in probe_scan_for_msi() */
	for (i = 0; i < xdev->user_max; i++, j++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
		u32 vector = pci_irq_vector(xdev->pdev, j);
#else
		u32 vector = xdev->entry[j].vector;
#endif
		rv = request_irq(vector, xdma_user_irq, 0, xdev->mod_name,
				&xdev->user_irq[i]);
		if (rv) {
			xocl_pr_info("user %d couldn't use IRQ#%d, %d\n",
				i, vector, rv);
			break;
		}
		xocl_pr_info("%d-USR-%d, IRQ#%d with 0x%p\n", xdev->idx, i, vector,
			&xdev->user_irq[i]);
        }

	/* If any errors occur, free IRQs that were successfully requested */
	if (rv) {
		for (i--, j--; i >= 0; i--, j--) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
			u32 vector = pci_irq_vector(xdev->pdev, j);
#else
			u32 vector = xdev->entry[j].vector;
#endif
			free_irq(vector, &xdev->user_irq[i]);
		}
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
	char *reg;
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
	pci_keep_intx_enabled(pdev);

	if (xdev->msix_enabled) {
		int rv = irq_msix_channel_setup(xdev);
		if (rv)
			return rv;
		rv = irq_msix_user_setup(xdev);
		if (rv)
			return rv;
		prog_irq_msix_channel(xdev, 0);
		prog_irq_msix_user(xdev, 0);

		return 0;
	} else if (xdev->msi_enabled)
		return irq_msi_setup(xdev, pdev);

	return irq_legacy_setup(xdev, pdev);
}

#ifdef __LIBXDMA_DEBUG__
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
		xocl_pr_info("0x%08lx/0x%02lx: 0x%08x 0x%08x %s\n",
			 (uintptr_t)p, (uintptr_t)p & 15, (int)*p,
			 le32_to_cpu(*p), field_name[j]);
		p++;
	}
	xocl_pr_info("\n");
}

#endif /* __LIBXDMA_DEBUG__ */



static void engine_alignments(struct xdma_engine *engine)
{
	u32 w;
	u32 align_bytes;
	u32 granularity_bytes;
	u32 address_bits;

	w = read_register(&engine->regs->alignments);
	dbg_init("engine %p name %s alignments=0x%08x\n", engine, engine->name,
		 (int)w);

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

static void engine_fastpath_cleanup(struct xdma_engine *engine)
{
	struct xdma_dev *xdev = engine->xdev;

	if (!engine->f_descs || !engine->f_desc_dma_addr)
		return;

	dma_free_coherent(&xdev->pdev->dev, F_DESC_NUM * sizeof(struct xdma_desc),
			  engine->f_descs, engine->f_desc_dma_addr);
}

static void engine_free_resource(struct xdma_engine *engine)
{
	struct xdma_dev *xdev = engine->xdev;

	/* Release memory use for descriptor writebacks */
	if (engine->poll_mode_addr_virt) {
		dbg_sg("Releasing memory for descriptor writeback\n");
		dma_free_coherent(&xdev->pdev->dev, sizeof(struct xdma_poll_wb),
				  engine->poll_mode_addr_virt,
				  engine->poll_mode_bus);
		dbg_sg("Released memory for descriptor writeback\n");
		engine->poll_mode_addr_virt = NULL;
	}

	if (engine->desc) {
		dbg_init("device %s, engine %s pre-alloc desc 0x%p,0x%llx.\n",
			 dev_name(&xdev->pdev->dev), engine->name, engine->desc,
			 engine->desc_bus);
		dma_free_coherent(&xdev->pdev->dev,
					  XDMA_DESC_SETS_MAX *
					  desc_set_depth *
					  sizeof(struct xdma_desc),
				  engine->desc, engine->desc_bus);
		engine->desc = NULL;
	}

	if (engine->cyclic_result) {
		dma_free_coherent(
			&xdev->pdev->dev,
			XDMA_TRANSFER_MAX_DESC * sizeof(struct xdma_result),
			engine->cyclic_result, engine->cyclic_result_bus);
		engine->cyclic_result = NULL;
	}
}

static int engine_destroy(struct xdma_dev *xdev, struct xdma_engine *engine)
{
	struct list_head *entry, *tmp;

	if (!xdev) {
		pr_err("Invalid xdev\n");
		return -EINVAL;
	}

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}

	dbg_sg("Shutting down engine %s%d", engine->name, engine->channel);

	/* Disable interrupts to stop processing new events during shutdown */
	write_register(0x0, &engine->regs->interrupt_enable_mask,
		       (unsigned long)(&engine->regs->interrupt_enable_mask) -
			       (unsigned long)(&engine->regs));

	spin_lock(&engine->desc_lock);
	list_for_each_safe(entry, tmp, &engine->pend_list) {
		struct xdma_request_cb *req = (struct xdma_request_cb *)entry;

		list_del(&req->entry);
		if (req->cb && req->cb->io_done)
			req->cb->io_done((unsigned long)engine->xdev, -EIO);
	}
	list_for_each_safe(entry, tmp, &engine->work_list) {
		struct xdma_request_cb *req = (struct xdma_request_cb *)entry;

		list_del(&req->entry);
		if (req->cb && req->cb->io_done)
			req->cb->io_done((unsigned long)engine->xdev, -EIO);
	}
	spin_unlock(&engine->desc_lock);
	if (enable_credit_mp && engine->streaming &&
	    engine->dir == DMA_FROM_DEVICE) {
		u32 reg_value = (0x1 << engine->channel) << 16;
		struct sgdma_common_regs *reg =
			(struct sgdma_common_regs
				 *)(xdev->bar[xdev->config_bar_idx] +
				    (0x6 * TARGET_SPACING));
		write_register(reg_value, &reg->credit_mode_enable_w1c, 0);
	}

	/* Release memory use for descriptor writebacks */
	engine_free_resource(engine);

	/* release fast path resources */
	engine_fastpath_cleanup(engine);

	memset(engine, 0, sizeof(struct xdma_engine));
	/* Decrement the number of engines available */
	xdev->engines_num--;
	return 0;
}

/**
 *engine_cyclic_stop() - stop a cyclic transfer running on an SG DMA engine
 *
 *engine->lock must be taken
 */
int engine_cyclic_stop(struct xdma_engine *engine)
{
	int rv;

	if (engine->xdma_perf)
		dbg_perf("Stopping perf transfer on %s\n",
				engine->name);
	else {
		pr_warn("Performance is not running on engine %s\n",
				engine->name);
		return -EINVAL;
	}

	rv = xdma_engine_stop(engine);
	if (rv < 0)
		pr_err("Failed to stop engine\n");
	engine->running = 0;
	rv = engine_status_read(engine, 1, 0);

	return rv;
}

static int engine_writeback_setup(struct xdma_engine *engine)
{
	u32 w;
	struct xdma_dev *xdev;
	struct xdma_poll_wb *writeback;

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}

	xdev = engine->xdev;
	if (!xdev) {
		pr_err("Invalid xdev\n");
		return -EINVAL;
	}

	/*
	 * RTO - doing the allocation per engine is wasteful since a full page
	 * is allocated each time - better to allocate one page for the whole
	 * device during probe() and set per-engine offsets here
	 */
	writeback = (struct xdma_poll_wb *)engine->poll_mode_addr_virt;
	writeback->completed_desc_count = 0;

	dbg_init("Setting writeback location to 0x%llx for engine %p",
		 engine->poll_mode_bus, engine);
	w = cpu_to_le32(PCI_DMA_L(engine->poll_mode_bus));
	write_register(w, &engine->regs->poll_mode_wb_lo,
		       (unsigned long)(&engine->regs->poll_mode_wb_lo) -
			       (unsigned long)(&engine->regs));
	w = cpu_to_le32(PCI_DMA_H(engine->poll_mode_bus));
	write_register(w, &engine->regs->poll_mode_wb_hi,
		       (unsigned long)(&engine->regs->poll_mode_wb_hi) -
			       (unsigned long)(&engine->regs));

	return 0;
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
static int engine_init_regs(struct xdma_engine *engine)
{
	u32 reg_value;
	int rv = 0;

	write_register(XDMA_CTRL_NON_INCR_ADDR, &engine->regs->control_w1c,
		       (unsigned long)(&engine->regs->control_w1c) -
			       (unsigned long)(&engine->regs));

	engine_alignments(engine);

	/* Configure error interrupts by default */
	reg_value = XDMA_CTRL_IE_DESC_ALIGN_MISMATCH;
	reg_value |= XDMA_CTRL_RUN_STOP;
	reg_value |= XDMA_CTRL_IE_MAGIC_STOPPED;
	reg_value |= XDMA_CTRL_IE_READ_ERROR;
	reg_value |= XDMA_CTRL_IE_DESC_ERROR;

	/* if using polled mode, configure writeback address */
	if (poll_mode) {

		rv = engine_writeback_setup(engine);
		if (rv) {
			dbg_init("%s descr writeback setup failed.\n",
				 engine->name);
			goto fail_wb;
		}
	} else {
		/* enable the relevant completion interrupts */
		reg_value |= XDMA_CTRL_IE_DESC_STOPPED;
		reg_value |= XDMA_CTRL_IE_DESC_COMPLETED;
	}

	/* Apply engine configurations */
	write_register(reg_value, &engine->regs->interrupt_enable_mask,
		       (unsigned long)(&engine->regs->interrupt_enable_mask) -
			       (unsigned long)(&engine->regs));

	engine->interrupt_enable_mask_value = reg_value;

	/* only enable credit mode for AXI-ST C2H */
	if (enable_credit_mp && engine->streaming &&
	    engine->dir == DMA_FROM_DEVICE) {
		struct xdma_dev *xdev = engine->xdev;
		u32 reg_value = (0x1 << engine->channel) << 16;
		struct sgdma_common_regs *reg =
			(struct sgdma_common_regs
				 *)(xdev->bar[xdev->config_bar_idx] +
				    (0x6 * TARGET_SPACING));

		write_register(reg_value, &reg->credit_mode_enable_w1s, 0);
	}

	return 0;

fail_wb:
	return rv;
}

static int engine_fastpath_init(struct xdma_engine *engine)
{
	struct xdma_dev *xdev = engine->xdev;
	struct xdma_desc *desc;
	dma_addr_t dma_addr;
	int i, j;

	engine->f_descs = dma_alloc_coherent(&xdev->pdev->dev, F_DESC_NUM * sizeof(*desc),
					     &engine->f_desc_dma_addr, GFP_KERNEL);
	if (!engine->f_descs)
		return -ENOMEM;

	desc = engine->f_descs;
	dma_addr = engine->f_desc_dma_addr;
	for (i = 0; i < F_DESC_BLOCK_NUM; i++) {
		for (j = 0; j < F_DESC_ADJACENT - 1; j++) {
			desc->control = cpu_to_le32(F_DESC_CONTROL(1, 0));
			desc++;
		}
		dma_addr += sizeof(*desc) * F_DESC_ADJACENT;
		desc->control = cpu_to_le32(F_DESC_CONTROL(F_DESC_ADJACENT, 0));
		desc->next_lo = cpu_to_le32(PCI_DMA_L(dma_addr));
		desc->next_hi = cpu_to_le32(PCI_DMA_H(dma_addr));
		desc++;
	}

	init_completion(&engine->f_req_compl);

	return 0;
}

static int engine_alloc_resource(struct xdma_engine *engine)
{
	struct xdma_dev *xdev = engine->xdev;
	int i;
	unsigned int temp_control = DESC_MAGIC;
	struct xdma_desc *desc_virt;
	dma_addr_t desc_bus;

	engine->desc = dma_alloc_coherent(&xdev->pdev->dev,
					  XDMA_DESC_SETS_MAX *
					  desc_set_depth *
					  sizeof(struct xdma_desc),
					  &engine->desc_bus, GFP_KERNEL);
	if (!engine->desc) {
		pr_warn("dev %s, %s pre-alloc desc OOM.\n",
			dev_name(&xdev->pdev->dev), engine->name);
		goto err_out;
	}
	desc_virt = engine->desc;
	desc_bus = engine->desc_bus;
	for (i = 0; i < (XDMA_DESC_SETS_MAX * desc_set_depth); i++) {
		/* increment bus address to next in array */
		desc_bus += sizeof(struct xdma_desc);

		desc_virt[i].next_lo = cpu_to_le32(PCI_DMA_L(desc_bus));
		desc_virt[i].next_hi = cpu_to_le32(PCI_DMA_H(desc_bus));
		desc_virt[i].control = cpu_to_le32(temp_control);
	}
	desc_virt[i - 1].next_lo = cpu_to_le32(0);
	desc_virt[i - 1].next_hi = cpu_to_le32(0);

	if (poll_mode) {

		engine->poll_mode_addr_virt =
			dma_alloc_coherent(&xdev->pdev->dev,
					   sizeof(struct xdma_poll_wb),
					   &engine->poll_mode_bus, GFP_KERNEL);
		if (!engine->poll_mode_addr_virt) {
			pr_warn("%s, %s poll pre-alloc writeback OOM.\n",
				dev_name(&xdev->pdev->dev), engine->name);
			goto err_out;
		}
	}

	if (engine->streaming && engine->dir == DMA_FROM_DEVICE) {
		engine->cyclic_result = dma_alloc_coherent(
			&xdev->pdev->dev,
			((XDMA_DESC_SETS_MAX * desc_set_depth) *
			sizeof(struct xdma_result)),
			&engine->cyclic_result_bus, GFP_KERNEL);

		if (!engine->cyclic_result) {
			pr_warn("%s, %s pre-alloc result OOM.\n",
				dev_name(&xdev->pdev->dev), engine->name);
			goto err_out;
		}
		engine->result_pidx = 0;
		engine->result_cidx = 0;
	}

	return 0;

err_out:
	engine_free_resource(engine);
	return -ENOMEM;
}

static int engine_init(struct xdma_engine *engine, struct xdma_dev *xdev,
		       int offset, enum dma_data_direction dir, int channel)
{
	int rv;
	u32 val;

	dbg_init("channel %d, offset 0x%x, dir %d.\n", channel, offset, dir);

	/* set magic */
	engine->magic = MAGIC_ENGINE;

	engine->channel = channel;

	/* set cpu for engine */
	engine->cpu_idx = channel % num_online_cpus();

	/* engine interrupt request bit */
	engine->irq_bitmask = (1 << XDMA_ENG_IRQ_NUM) - 1;
	engine->irq_bitmask <<= (xdev->engines_num * XDMA_ENG_IRQ_NUM);
	engine->bypass_offset = xdev->engines_num * BYPASS_MODE_SPACING;

	/* parent */
	engine->xdev = xdev;
	/* register address */
	engine->regs = (xdev->bar[xdev->config_bar_idx] + offset);
	engine->sgdma_regs = xdev->bar[xdev->config_bar_idx] + offset +
			     	SGDMA_OFFSET_FROM_CHANNEL;
	val = read_register(&engine->regs->identifier);
	if (val & 0x8000U)
		engine->streaming = 1;

	/* remember SG DMA direction */
	engine->dir = dir;
	sprintf(engine->name, "%d-%s%d-%s", xdev->idx,
		(dir == DMA_TO_DEVICE) ? "H2C" : "C2H", channel,
		engine->streaming ? "ST" : "MM");

	dbg_init("engine %p name %s irq_bitmask=0x%08x\n", engine, engine->name,
		 (int)engine->irq_bitmask);

	/* initialize the deferred work for request/transfer completion */
	if (poll_mode)
		INIT_WORK(&engine->poll, engine_service_req_poll);
	else
		INIT_WORK(&engine->work, engine_service_work);
	INIT_WORK(&engine->aio_mon, aio_request_monitor);
	INIT_WORK(&engine->req_proc, engine_process_requests);

	if (dir == DMA_TO_DEVICE)
		xdev->mask_irq_h2c |= engine->irq_bitmask;
	else
		xdev->mask_irq_c2h |= engine->irq_bitmask;
	xdev->engines_num++;

	engine->wq_serviced = 0;

	rv = engine_alloc_resource(engine);
	if (rv)
		return rv;

	rv = engine_fastpath_init(engine);
	if (rv)
		goto fastpath_init_fail;

	rv = engine_init_regs(engine);
	if (rv)
		goto init_regs_failed;

	return 0;

init_regs_failed:
	engine_fastpath_cleanup(engine);
fastpath_init_fail:
	engine_free_resource(engine);

	return rv;
}



#ifdef __LIBXDMA_DEBUG__
static void sgt_dump(struct sg_table *sgt)
{
	int i;
	struct scatterlist *sg = sgt->sgl;

	xocl_pr_info("sgt 0x%p, sgl 0x%p, nents %u/%u.\n",
		sgt, sgt->sgl, sgt->nents, sgt->orig_nents);

	for (i = 0; i < sgt->orig_nents; i++, sg = sg_next(sg))
		xocl_pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx,%u.\n",
			i, sg, sg_page(sg), sg->offset, sg->length,
			sg_dma_address(sg), sg_dma_len(sg));
}

static void xdma_request_cb_dump(struct xdma_request_cb *req)
{
	int i;

	xocl_pr_info("request 0x%p, total %u, ep 0x%llx, sw_desc %u, sgt 0x%p.\n",
		req, req->total_len, req->ep_addr, req->sw_desc_cnt, req->sgt);
	sgt_dump(req->sgt);
	for (i = 0; i < req->sw_desc_cnt; i++)
		xocl_pr_info("%d/%u, 0x%llx, %u.\n",
			i, req->sw_desc_cnt, req->sdesc[i].addr,
			req->sdesc[i].len);
}
#endif

static inline void fastpath_desc_set(struct xdma_engine *engine, struct xdma_desc *desc,
				     dma_addr_t addr, u64 endpoint_addr, u32 len)
{
	desc->bytes = cpu_to_le32(len);
	if (engine->dir == DMA_TO_DEVICE) {
		desc->src_addr_lo = cpu_to_le32(PCI_DMA_L(addr));
		desc->src_addr_hi = cpu_to_le32(PCI_DMA_H(addr));
		desc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(endpoint_addr));
		desc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(endpoint_addr));
	} else {
		desc->src_addr_lo = cpu_to_le32(PCI_DMA_L(endpoint_addr));
		desc->src_addr_hi = cpu_to_le32(PCI_DMA_H(endpoint_addr));
		desc->dst_addr_lo = cpu_to_le32(PCI_DMA_L(addr));
		desc->dst_addr_hi = cpu_to_le32(PCI_DMA_H(addr));
	}
}

static inline void fastpath_desc_set_last(struct xdma_engine *engine, u32 desc_num)
{
	struct xdma_desc *block_desc = NULL, *last_desc;
	u32 adjacent;

	adjacent = desc_num & (F_DESC_ADJACENT - 1);
	if (desc_num > F_DESC_ADJACENT && adjacent > 0)
		block_desc = engine->f_descs + (desc_num & (~(F_DESC_ADJACENT - 1))) - 1;

	last_desc = engine->f_descs + desc_num - 1;
	if (block_desc)
		block_desc->control = cpu_to_le32(F_DESC_CONTROL(adjacent, 0));
	last_desc->control |= cpu_to_le32(F_DESC_STOPPED | F_DESC_COMPLETED);
}

static inline void fastpath_desc_clear_last(struct xdma_engine *engine, u32 desc_num)
{
	struct xdma_desc *block_desc = NULL, *last_desc;
	u32 adjacent;

	adjacent = desc_num & (F_DESC_ADJACENT - 1);
	if (desc_num > F_DESC_ADJACENT && adjacent > 0)
		block_desc = engine->f_descs + (desc_num & (~(F_DESC_ADJACENT - 1))) - 1;

	last_desc = engine->f_descs + desc_num - 1;
	if (block_desc)
		block_desc->control = cpu_to_le32(F_DESC_CONTROL(F_DESC_ADJACENT, 0));
	last_desc->control &= cpu_to_le32(~(F_DESC_STOPPED | F_DESC_COMPLETED));
}

static ssize_t fastpath_start(struct xdma_engine *engine, u64 endpoint_addr,
			      struct scatterlist **sg, u32 *sg_off, u32 *last_adj)
{
	dma_addr_t addr;
	int i;
	u32 len, rest, adj, desc_num = 0;
	ssize_t total = 0;

	for (i = 0; i < F_DESC_NUM && *sg; i++) {
		addr = sg_dma_address(*sg) + *sg_off;
		rest = sg_dma_len(*sg) - *sg_off;
		if (XDMA_DESC_BLEN_MAX < rest) {
			len = XDMA_DESC_BLEN_MAX;
			*sg_off += XDMA_DESC_BLEN_MAX;
		} else {
			len = rest;
			*sg_off = 0;
			*sg = sg_next(*sg);
		}

		if (len) {
			fastpath_desc_set(engine, engine->f_descs + desc_num, addr, endpoint_addr, len);
			endpoint_addr += len;
			total += len;
			desc_num++;
		}
	}
	if (!total)
		return 0;
	fastpath_desc_set_last(engine, desc_num);
	engine->f_submitted_desc_cnt = desc_num;

	enable_interrupts(engine);
	if (desc_num >= F_DESC_ADJACENT)
		adj = F_DESC_ADJACENT;
	else
		adj = desc_num;
	if (adj != *last_adj) {
		write_register(adj - 1, &engine->sgdma_regs->first_desc_adjacent,
			       (unsigned long)(&engine->sgdma_regs->first_desc_adjacent) -
			       (unsigned long)(&engine->sgdma_regs));
		mmiowb();
		*last_adj = adj;
	}
	engine_start_mode_config(engine);

	return total;
}

ssize_t xdma_xfer_fastpath(void *dev_hndl, int channel, bool write, u64 ep_addr,
			   struct sg_table *sgt, bool dma_mapped, int timeout_ms)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;
	struct scatterlist *sg = sgt->sgl;
	struct xdma_engine *engine;
	u32 val, sg_off = 0, last_adj = ~0;
	u64 done_bytes = 0;
	ssize_t ret = 0;
	int nents;

	if (poll_mode) {
		return xdma_xfer_submit(dev_hndl, channel, write, ep_addr, sgt, dma_mapped,
					timeout_ms, NULL);
	}

	if (write == 1)
		engine = &xdev->engine_h2c[channel];
	else
		engine = &xdev->engine_c2h[channel];

	if (!dma_mapped) {
		nents = dma_map_sg(&xdev->pdev->dev, sg, sgt->orig_nents,
				   engine->dir);
		if (!nents) {
			xocl_pr_info("map sgl failed, sgt 0x%p.\n", sgt);
			return -EIO;
		}
		sgt->nents = nents;
	}

	if (!sgt->nents) {
		pr_err("empty sg table");
		return -EINVAL;
	}

	write_register(PCI_DMA_H(engine->f_desc_dma_addr), &engine->sgdma_regs->first_desc_hi,
		       (unsigned long)(&engine->sgdma_regs->first_desc_hi) -
		       (unsigned long)(&engine->sgdma_regs));
	write_register(PCI_DMA_L(engine->f_desc_dma_addr), &engine->sgdma_regs->first_desc_lo,
		       (unsigned long)(&engine->sgdma_regs->first_desc_lo) -
		       (unsigned long)(&engine->sgdma_regs));
	sg = sgt->sgl;
	while (sg && ret >= 0) {
		engine->f_fastpath = true;
		ret = fastpath_start(engine, ep_addr + done_bytes,&sg, &sg_off, &last_adj);
		if (!ret)
			continue;

		done_bytes += ret;
		if (!wait_for_completion_timeout(&engine->f_req_compl,
						 msecs_to_jiffies(10000))) {
			pr_err("Wait for request timed out");
			engine_reg_dump(engine);
			check_nonzero_interrupt_status(engine->xdev);
			ret = -EIO;
		} else {
			val = read_register(&engine->regs->completed_desc_count);
			if (val != engine->f_submitted_desc_cnt) {
				pr_err("Invalid completed count %d, expected %d",
					    val, engine->f_submitted_desc_cnt);
				ret = -EINVAL;
			}
		}
		fastpath_desc_clear_last(engine, engine->f_submitted_desc_cnt);
		val = read_register(&engine->regs->status_rc);
		if (((engine->dir == DMA_FROM_DEVICE) &&
		    (val & XDMA_STAT_C2H_ERR_MASK)) ||
		    ((engine->dir == DMA_TO_DEVICE) &&
		    (val & XDMA_STAT_H2C_ERR_MASK))) {
			pr_err("engine %s, status error 0x%x.\n", engine->name,
			        val);
			engine_status_dump(engine);
			engine_reg_dump(engine);
		}
		write_register(XDMA_CTRL_RUN_STOP, &engine->regs->control_w1c,
			       (unsigned long)(&engine->regs->control_w1c) -
			       (unsigned long)(&engine->regs));
	}
	if (!dma_mapped) {
                dma_unmap_sg(&xdev->pdev->dev, sgt->sgl, sgt->orig_nents,
			     engine->dir);
        }

	if (ret < 0)
		return ret;

	return (ssize_t)done_bytes;
}

ssize_t xdma_xfer_submit(void *dev_hndl, int channel, bool write, u64 ep_addr,
			 struct sg_table *sgt, bool dma_mapped, int timeout_ms,
			 struct xdma_io_cb *cb)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;
	struct xdma_engine *engine;
	int rv = 0;
	ssize_t done = 0;
	struct scatterlist *sg = sgt->sgl;
	int nents;
	enum dma_data_direction dir = write ? DMA_TO_DEVICE : DMA_FROM_DEVICE;
	struct xdma_request_cb *req = NULL;

	if (!dev_hndl)
		return -EINVAL;

	if (debug_check_dev_hndl(__func__, xdev->pdev, dev_hndl) < 0)
		return -EINVAL;

	if (write == 1) {
		if (channel >= xdev->h2c_channel_max) {
			pr_err("H2C channel %d >= %d.\n", channel,
				xdev->h2c_channel_max);
			return -EINVAL;
		}
		engine = &xdev->engine_h2c[channel];
	} else if (write == 0) {
		if (channel >= xdev->c2h_channel_max) {
			pr_err("C2H channel %d >= %d.\n", channel,
				xdev->c2h_channel_max);
			return -EINVAL;
		}
		engine = &xdev->engine_c2h[channel];
	}

	if (!engine) {
		pr_err("dma engine NULL\n");
		return -EINVAL;
	}

	if (engine->magic != MAGIC_ENGINE) {
		pr_err("%s has invalid magic number %lx\n", engine->name,
		       engine->magic);
		return -EINVAL;
	}

	xdev = engine->xdev;
	if (xdma_device_flag_check(xdev, XDEV_FLAG_OFFLINE)) {
		pr_err("xdev 0x%p, offline.\n", xdev);
		return -EBUSY;
	}

	/* check the direction */
	if (engine->dir != dir) {
		pr_err("0x%p, %s, %d, W %d, 0x%x/0x%x mismatch.\n", engine,
			engine->name, channel, write, engine->dir, dir);
		return -EINVAL;
	}

	if (!dma_mapped) {
		nents = dma_map_sg(&xdev->pdev->dev, sg, sgt->orig_nents, dir);
		if (!nents) {
			xocl_pr_info("map sgl failed, sgt 0x%p.\n", sgt);
			return -EIO;
		}
		sgt->nents = nents;
	} else {
		if (!sgt->nents) {
			pr_err("sg table has invalid number of entries 0x%p.\n",
			       sgt);
			return -EIO;
		}
	}

	req = xdma_request_alloc(sgt);
	if (!req)
		return -ENOMEM;
	req->dma_mapped = dma_mapped;
	req->cb = cb;
	req->dir = dir;
	req->sgt = sgt;
	req->ep_addr = ep_addr;


	rv = xdma_init_request(req);
	if (rv < 0)
		goto unmap_sgl;
	xdma_add_request(engine, req);

	dbg_tfr("%s, len %u sg cnt %u.\n", engine->name, req->total_len,
		req->sw_desc_cnt);

	/* If this is async request return immediately. */
	if (req->cb && req->cb->io_done)
	{
		/* Kickoff processing of asynchronous IOs */
		schedule_work_on(engine->cpu_idx, &engine->req_proc);
		return 0;

	}
	rv = xdma_process_requests(engine, req);

	spin_lock(&engine->req_list_lock);
	/* Read length of completed transfer */
	done = req->done;
	if (rv < 0) {
		if (req->sw_desc_cnt > req->desc_completed)
			list_del(&req->entry);
		pr_err("Request Processing failed, :%u/%u/%u\n",
		       req->sw_desc_cnt, req->sw_desc_idx, req->desc_completed);
		goto unmap_sgl;
	}

unmap_sgl:
	xdma_request_release(engine->xdev, req);
	spin_unlock(&engine->req_list_lock);
	if (rv < 0)
		return rv;

	return done;
}

void xdma_proc_aio_requests(void *dev_hndl, int channel, bool write)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;
	struct xdma_engine *engine;

	if (write) {
		if (channel >= xdev->h2c_channel_max) {
			pr_err("H2C channel %d >= %d.\n", channel,
				xdev->h2c_channel_max);
			return;
		}
		engine = &xdev->engine_h2c[channel];
	} else {
		if (channel >= xdev->c2h_channel_max) {
			pr_err("C2H channel %d >= %d.\n", channel,
				xdev->c2h_channel_max);
			return;
		}
		engine = &xdev->engine_c2h[channel];
	}
	if (!engine) {
		pr_err("dma engine NULL\n");
		return;
	}

	if (engine->magic != MAGIC_ENGINE) {
		pr_err("%s has invalid magic number %lx\n", engine->name,
		       engine->magic);
		return;
	}

	if (xdma_device_flag_check(xdev, XDEV_FLAG_OFFLINE)) {
		pr_err("xdev 0x%p, offline.\n", xdev);
		return;
	}

	schedule_work_on(engine->cpu_idx, &engine->req_proc);
}

int xdma_performance_submit(struct xdma_dev *xdev, struct xdma_engine *engine)
{
	u8 *buffer_virt;
	u32 max_consistent_size = 128 * 32 * 1024; /* 1024 pages, 4MB */
	dma_addr_t buffer_bus; /* bus address */
	dma_addr_t rc_bus_addr;
	struct xdma_request_cb *req;
	struct xdma_desc *desc_virt;
	dma_addr_t desc_bus;
	u64 ep_addr = 0;
	int num_desc_in_a_loop = 128;
	int size_in_desc = engine->xdma_perf->transfer_size;
	int size = size_in_desc * num_desc_in_a_loop;
	int i;
	int rv = -ENOMEM;


	if (size_in_desc > max_consistent_size) {
		pr_err("%s max consistent size %d is more than supported %d\n",
		       engine->name, size_in_desc, max_consistent_size);
		return -EINVAL;
	}

	if (size > max_consistent_size) {
		size = max_consistent_size;
		num_desc_in_a_loop = size / size_in_desc;
	}

	if (!engine->desc) {
		pr_err("DMA engine %s has void descriptor buffers\n",
			     engine->name);
		return -EINVAL;
	}


	buffer_virt = NULL;
	/* allocates request without sgt entries */
	req = xdma_request_alloc(0);
	if (!req)
		return -ENOMEM;
	spin_lock(&engine->lock);
	if (engine->running) {
		pr_warn("Dma Engine is busy\n");
		rv = -EBUSY;
		goto err_dma_desc;
	}
	//xdma_add_request(engine, req);
	req->desc_virt = engine->desc;
	req->desc_bus = engine->desc_bus;

	buffer_virt = dma_alloc_coherent(&xdev->pdev->dev, size, &buffer_bus,
					 GFP_KERNEL);
	if (!buffer_virt) {
		pr_err("dev %s, %s DMA allocation OOM.\n",
		       dev_name(&xdev->pdev->dev), engine->name);
		rv = -ENOMEM;
		goto err_dma_desc;
	}

	engine->perf_buffer = buffer_virt;
	engine->perf_bus = buffer_bus;
	engine->perf_size = size;

	desc_bus = req->desc_bus;
	for (i = 0; i < num_desc_in_a_loop; i++) {
		desc_virt = req->desc_virt + i;
		desc_bus += sizeof(struct xdma_desc);
		/* Actual Data to perform DMA */
		rc_bus_addr = buffer_bus + (u64)size_in_desc * i;

		/* fill in descriptor entry with transfer details */
		xdma_desc_set(desc_virt, rc_bus_addr, ep_addr, size_in_desc,
			      engine->dir);
		desc_virt[i].next_lo = cpu_to_le32(PCI_DMA_L(desc_bus));
		desc_virt[i].next_hi = cpu_to_le32(PCI_DMA_H(desc_bus));
	}

	/* stop engine and request interrupt on last descriptor */
	rv = xdma_desc_control_set(req->desc_virt, 0);
	if (rv < 0) {
		pr_err("Failed to set desc control\n");
		goto err_dma_desc;
	}
	/* create a linked loop */
	xdma_desc_link(req->desc_virt + num_desc_in_a_loop - 1,
		       req->desc_virt, req->desc_bus);

	dbg_perf("Queueing XDMA I/O %s request for performance measurement.\n",
		 engine->dir ? "write (to dev)" : "read (from dev)");
	rv = engine_start(engine, req->desc_bus, num_desc_in_a_loop);
	if (rv < 0) {
		pr_err("Failed to queue transfer\n");
		goto err_dma_desc;
	}
	engine->running = 1;
	spin_unlock(&engine->lock);
	kfree(req);
	return 0;

err_dma_desc:
	spin_unlock(&engine->lock);
	kfree(req);
	req = NULL;
	if (buffer_virt)
		dma_free_coherent(&xdev->pdev->dev, size, buffer_virt,
				  buffer_bus);
	buffer_virt = NULL;
	return rv;
}

static struct xdma_dev *alloc_dev_instance(struct pci_dev *pdev)
{
	int i;
	struct xdma_dev *xdev;
	struct xdma_engine *engine;

	if (!pdev) {
		pr_err("Invalid pdev\n");
		return NULL;
	}

	/* allocate zeroed device book keeping structure */
	xdev = kzalloc(sizeof(struct xdma_dev), GFP_KERNEL);
	if (!xdev) {
		xocl_pr_info("OOM, xdma_dev.\n");
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
	for (i = 0; i < 16; i++) {
		xdev->user_irq[i].xdev = xdev;
		spin_lock_init(&xdev->user_irq[i].events_lock);
		init_waitqueue_head(&xdev->user_irq[i].events_wq);
		xdev->user_irq[i].handler = NULL;
		xdev->user_irq[i].user_idx = i; /* 0 based */
	}

	engine = xdev->engine_h2c;
	for (i = 0; i < XDMA_CHANNEL_NUM_MAX; i++, engine++) {
		spin_lock_init(&engine->lock);
		spin_lock_init(&engine->desc_lock);
		spin_lock_init(&engine->req_list_lock);
		INIT_LIST_HEAD(&engine->work_list);
		INIT_LIST_HEAD(&engine->pend_list);
		engine->avail_sets = XDMA_DESC_SETS_AVAIL_MAX;
	//	INIT_LIST_HEAD(&engine->transfer_list);
		init_waitqueue_head(&engine->shutdown_wq);
		init_waitqueue_head(&engine->xdma_perf_wq);
	}

	engine = xdev->engine_c2h;
	for (i = 0; i < XDMA_CHANNEL_NUM_MAX; i++, engine++) {
		spin_lock_init(&engine->lock);
		spin_lock_init(&engine->desc_lock);
		spin_lock_init(&engine->req_list_lock);
		INIT_LIST_HEAD(&engine->work_list);
		INIT_LIST_HEAD(&engine->pend_list);
		engine->avail_sets = XDMA_DESC_SETS_AVAIL_MAX;
	//	INIT_LIST_HEAD(&engine->transfer_list);
		init_waitqueue_head(&engine->shutdown_wq);
		init_waitqueue_head(&engine->xdma_perf_wq);
	}

	return xdev;
}


static int set_dma_mask(struct pci_dev *pdev)
{
	BUG_ON(!pdev);

	dbg_init("sizeof(dma_addr_t) == %ld\n", sizeof(dma_addr_t));
	/* 64-bit addressing capability for XDMA? */
	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(64))) {
		/* query for DMA transfer */
		/* @see Documentation/DMA-mapping.txt */
		dbg_init("pci_set_dma_mask()\n");
		/* use 64-bit DMA */
		dbg_init("Using a 64-bit DMA mask.\n");
		/* use 32-bit DMA for descriptors */
		dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
		/* use 64-bit DMA, 32-bit for consistent */
	} else if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(32))) {
		dbg_init("Could not set 64-bit DMA mask.\n");
		dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
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

	if (xdev->no_dma)
		return;

	/* iterate over channels */
	for (i = 0; i < xdev->h2c_channel_max; i++) {
		engine = &xdev->engine_h2c[i];
		if (engine->magic == MAGIC_ENGINE) {
			dbg_sg("Remove %s, %d", engine->name, i);
			engine_destroy(xdev, engine);
			dbg_sg("%s, %d removed", engine->name, i);
		}
	}

	for (i = 0; i < xdev->c2h_channel_max; i++) {
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
		dbg_init("%s %d engine, reg off 0x%x, id mismatch 0x%x,0x%x,"
			"exp 0x%x,0x%x, SKIP.\n",
		 	dir == DMA_TO_DEVICE ? "H2C" : "C2H",
			 channel, offset, engine_id, channel_id,
			engine_id_expected, channel_id != channel);
		return -EINVAL;
	}

	dbg_init("found AXI %s %d engine, reg. off 0x%x, id 0x%x,0x%x.\n",
		 dir == DMA_TO_DEVICE ? "H2C" : "C2H", channel,
		 offset, engine_id, channel_id);

	/* allocate and initialize engine */
	rv = engine_init(engine, xdev, offset, dir, channel);
	if (rv != 0) {
		xocl_pr_info("failed to create AXI %s %d engine.\n",
			dir == DMA_TO_DEVICE ? "H2C" : "C2H",
			channel);
		return rv;
	}

	return 0;
}

static int probe_engines(struct xdma_dev *xdev)
{
	int i;
	int rv = 0;

	BUG_ON(!xdev);

	if (xdev->no_dma) {
		xdev->h2c_channel_max = 2;
		xdev->c2h_channel_max = 2;
		return 0;
	}

	/* iterate over channels */
	for (i = 0; i < xdev->h2c_channel_max; i++) {
		rv = probe_for_engine(xdev, DMA_TO_DEVICE, i);
		if (rv)
			break;
	}
	xdev->h2c_channel_max = i;

	for (i = 0; i < xdev->c2h_channel_max; i++) {
		rv = probe_for_engine(xdev, DMA_FROM_DEVICE, i);
		if (rv)
			break;
	}
	xdev->c2h_channel_max = i;

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
static void pci_enable_capability(struct pci_dev *pdev, int cap)
{
	pcie_capability_set_word(pdev, PCI_EXP_DEVCTL, cap);
}
#else
static void pci_enable_capability(struct pci_dev *pdev, int cap)
{
	u16 v;
	int pos;

	pos = pci_pcie_cap(pdev);
	if (pos > 0) {
		pci_read_config_word(pdev, pos + PCI_EXP_DEVCTL, &v);
		v |= cap;
		pci_write_config_word(pdev, pos + PCI_EXP_DEVCTL, v);
	}
}
#endif

static int pci_check_extended_tag(struct xdma_dev *xdev, struct pci_dev *pdev)
{
	u16 cap;
#if 0
	u32 v;
	void *__iomem reg;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
	pcie_capability_read_word(pdev, PCI_EXP_DEVCTL, &cap);
#else
	int pos;

	pos = pci_pcie_cap(pdev);
	if (pos > 0)
		pci_read_config_word(pdev, pos + PCI_EXP_DEVCTL, &cap);
	else {
		xocl_pr_info("pdev 0x%p, unable to access pcie cap.\n", pdev);
		return -EACCES;
	}
#endif

	if ((cap & PCI_EXP_DEVCTL_EXT_TAG))
		return 0;

	/* extended tag not enabled */
	xocl_pr_info("0x%p EXT_TAG disabled.\n", pdev);

#if 0
	/* Confirmed with Karen. This code is needed when ExtTag is disabled.
	 * Reason to disable below code:
	 * We observed that the ExtTag was cleared on some system. The SSD-
	 * FPGA board will not work on that system (DMA failed). The solution
	 * is that XDMA driver should enable ExtTag in that case.
	 *
	 * If ExtTag need to be disabled for your system, please enable this.
	 */
	if (xdev->config_bar_idx < 0) {
		xocl_pr_info("pdev 0x%p, xdev 0x%p, config bar UNKNOWN.\n",
				pdev, xdev);
		return -EINVAL;
	}

	reg = xdev->bar[xdev->config_bar_idx] + XDMA_OFS_CONFIG + 0x4C;
	v =  read_register(reg);
	v = (v & 0xFF) | (((u32)32) << 8);
	write_register(v, reg, XDMA_OFS_CONFIG + 0x4C);
	return 0;
#else
	/* Return 1 will go to enable ExtTag */
	return 1;
#endif
}

void *xdma_device_open(const char *mname, struct pci_dev *pdev, int *user_max,
			int *h2c_channel_max, int *c2h_channel_max,
			bool no_dma)
{
	struct xdma_dev *xdev = NULL;
	int rv = 0;

	xocl_pr_info("%s device %s, 0x%p.\n", mname, dev_name(&pdev->dev), pdev);

	/* allocate zeroed device book keeping structure */
	xdev = alloc_dev_instance(pdev);
	if (!xdev)
		return NULL;
	xdev->no_dma = no_dma;
	xdev->mod_name = mname;
	xdev->user_max = *user_max;
	if (h2c_channel_max)
		xdev->h2c_channel_max = *h2c_channel_max;
	if (c2h_channel_max)
		xdev->c2h_channel_max = *c2h_channel_max;

	xdma_device_flag_set(xdev, XDEV_FLAG_OFFLINE);
	xdev_list_add(xdev);

	if (xdev->user_max == 0 || xdev->user_max > MAX_USER_IRQ)
		xdev->user_max = MAX_USER_IRQ;
	if (xdev->h2c_channel_max == 0 ||
	    xdev->h2c_channel_max > XDMA_CHANNEL_NUM_MAX)
		xdev->h2c_channel_max = XDMA_CHANNEL_NUM_MAX;
	if (xdev->c2h_channel_max == 0 ||
	    xdev->c2h_channel_max > XDMA_CHANNEL_NUM_MAX)
		xdev->c2h_channel_max = XDMA_CHANNEL_NUM_MAX;

	/* keep INTx enabled */
	pci_check_intr_pend(pdev);

	/* enable relaxed ordering */
	pci_enable_capability(pdev, PCI_EXP_DEVCTL_RELAX_EN);

	/* if extended tag check failed, enable it */
	if (pci_check_extended_tag(xdev, pdev)) {
		xocl_pr_info("ExtTag is disabled, try enable it.\n");
		pci_enable_capability(pdev, PCI_EXP_DEVCTL_EXT_TAG);
	}

	/* force MRRS to be 512 */
	rv = pcie_get_readrq(pdev);
	if (rv < 0) {
		dev_err(&pdev->dev, "failed to read mrrs %d\n", rv);
		goto err_map;
	}
	if (rv > 512) {
		rv = pcie_set_readrq(pdev, 512);
		if (rv) {
			dev_err(&pdev->dev, "failed to force mrrs %d\n", rv);
			goto err_map;
		}
	}

	/* enable bus master capability */
	pci_set_master(pdev);

	rv = map_bars(xdev, pdev);
	if (rv)
		goto err_map;

	rv = set_dma_mask(pdev);
	if (rv)
		goto err_mask;

	check_nonzero_interrupt_status(xdev);

	/* explicitely zero all interrupt enable masks */
	channel_interrupts_disable(xdev, ~0);
	user_interrupts_disable(xdev, ~0);
	read_interrupts(xdev);

	rv = probe_engines(xdev);
	if (rv)
		goto err_engines;

	/* re-determine user_max */
	xdev->user_max = min(xdev->user_max, pci_msix_vec_count(pdev) -
				xdev->c2h_channel_max - xdev->h2c_channel_max);
	if (xdev->user_max < 0) {
		rv = -EINVAL;
		pr_err("Invalid number of interrupts. "
			"pci %d, h2c %d, c2h %d",
			pci_msix_vec_count(pdev), xdev->h2c_channel_max,
			xdev->c2h_channel_max);
		goto err_enable_msix;
	}

	rv = enable_msi_msix(xdev, pdev);
	if (rv < 0)
		goto err_enable_msix;

	rv = irq_setup(xdev, pdev);
	if (rv < 0)
		goto err_interrupts;

	if (!poll_mode)
		channel_interrupts_enable(xdev, ~0);

	/* Flush writes */
	read_interrupts(xdev);


	*user_max = xdev->user_max;
	if (h2c_channel_max)
		*h2c_channel_max = xdev->h2c_channel_max;
	if (c2h_channel_max)
		*c2h_channel_max = xdev->c2h_channel_max;

	xdma_device_flag_clear(xdev, XDEV_FLAG_OFFLINE);
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
	xdev_list_remove(xdev);
	kfree(xdev);
	return NULL;
}

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

	xdev_list_remove(xdev);

	kfree(xdev);
}

void xdma_device_offline(struct pci_dev *pdev, void *dev_hndl)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;
	struct xdma_engine *engine;
	int i;

	if (!dev_hndl)
		return;

	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return;

	xocl_pr_info("pdev 0x%p, xdev 0x%p.\n", pdev, xdev);
	xdma_device_flag_set(xdev, XDEV_FLAG_OFFLINE);

	/* wait for all engines to be idle */
	for (i  = 0; i < xdev->h2c_channel_max; i++) {
		unsigned long flags;

		engine = &xdev->engine_h2c[i];

		if (engine->magic == MAGIC_ENGINE) {
			spin_lock_irqsave(&engine->lock, flags);
			engine->shutdown |= ENGINE_SHUTDOWN_REQUEST;

			xdma_engine_stop(engine);
			engine->running = 0;
			spin_unlock_irqrestore(&engine->lock, flags);
		}
	}

	for (i  = 0; i < xdev->c2h_channel_max; i++) {
		unsigned long flags;

		engine = &xdev->engine_c2h[i];
		if (engine->magic == MAGIC_ENGINE) {
			spin_lock_irqsave(&engine->lock, flags);
			engine->shutdown |= ENGINE_SHUTDOWN_REQUEST;

			xdma_engine_stop(engine);
			engine->running = 0;
			spin_unlock_irqrestore(&engine->lock, flags);
		}
	}

	/* turn off interrupts */
	channel_interrupts_disable(xdev, ~0);
	user_interrupts_disable(xdev, ~0);
	read_interrupts(xdev);
	irq_teardown(xdev);

	xocl_pr_info("xdev 0x%p, done.\n", xdev);
}

void xdma_device_online(struct pci_dev *pdev, void *dev_hndl)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;
	struct xdma_engine *engine;
	unsigned long flags;
	int i;

	if (!dev_hndl)
		return;

	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return;

	xocl_pr_info("pdev 0x%p, xdev 0x%p.\n", pdev, xdev);

	for (i  = 0; i < xdev->h2c_channel_max; i++) {
		engine = &xdev->engine_h2c[i];
		if (engine->magic == MAGIC_ENGINE) {
			engine_init_regs(engine);
			spin_lock_irqsave(&engine->lock, flags);
			engine->shutdown &= ~ENGINE_SHUTDOWN_REQUEST;
			spin_unlock_irqrestore(&engine->lock, flags);
		}
	}

	for (i  = 0; i < xdev->c2h_channel_max; i++) {
		engine = &xdev->engine_c2h[i];
		if (engine->magic == MAGIC_ENGINE) {
			engine_init_regs(engine);
			spin_lock_irqsave(&engine->lock, flags);
			engine->shutdown &= ~ENGINE_SHUTDOWN_REQUEST;
			spin_unlock_irqrestore(&engine->lock, flags);
		}
	}

	/* re-write the interrupt table */
	if (!poll_mode) {
		irq_setup(xdev, pdev);

		channel_interrupts_enable(xdev, ~0);
		user_interrupts_enable(xdev, xdev->mask_irq_user);
		read_interrupts(xdev);
	}

	xdma_device_flag_clear(xdev, XDEV_FLAG_OFFLINE);
	xocl_pr_info("xdev 0x%p, done.\n", xdev);
}

int xdma_device_restart(struct pci_dev *pdev, void *dev_hndl)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;

	if (!dev_hndl)
		return -EINVAL;

	if (debug_check_dev_hndl(__func__, pdev, dev_hndl) < 0)
		return -EINVAL;

	xocl_pr_info("NOT implemented, 0x%p.\n", xdev);
	return -EINVAL;
}

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

int xdma_get_userio(void *dev_hndl, void * __iomem *base_addr,
	u64 *len, u32 *bar_idx)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;

	if (xdev->user_bar_idx < 0)
		return -ENOENT;

	*base_addr = xdev->bar[xdev->user_bar_idx];
	*len = pci_resource_len(xdev->pdev, xdev->user_bar_idx);
	*bar_idx = xdev->user_bar_idx;

	return (0);
}

int xdma_get_bypassio(void *dev_hndl, u64 *len, u32 *bar_idx)
{
	struct xdma_dev *xdev = (struct xdma_dev *)dev_hndl;

	/* not necessary have bypass bar*/
	if (xdev->bypass_bar_idx < 0)
		return 0;

	*len = pci_resource_len(xdev->pdev, xdev->bypass_bar_idx);
	*bar_idx = xdev->bypass_bar_idx;

	return (0);
}

/*
static struct scatterlist *sglist_index(struct sg_table *sgt, unsigned int idx)
{
	struct scatterlist *sg = sgt->sgl;
	int i;

	if (idx >= sgt->orig_nents)
		return NULL;

	if (!idx)
		return sg;

	for (i = 0; i < idx; i++, sg = sg_next(sg))
		;

	return sg;
}
*/

/* ********************* static function definitions ************************ */

/* SECTION: Module licensing */
#ifdef __LIBXDMA_MOD__
#include "version.h"
#define DRV_MODULE_NAME		"libxdma"
#define DRV_MODULE_DESC		"Xilinx XDMA Base Driver"
#define DRV_MODULE_RELDATE	"Feb. 2017"

static char version[] =
        DRV_MODULE_DESC " " DRV_MODULE_NAME " v" DRV_MODULE_VERSION "\n";

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION(DRV_MODULE_DESC);
MODULE_VERSION(DRV_MODULE_VERSION);
MODULE_LICENSE("GPL v2");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0) || defined(RHEL_9_0_GE)
MODULE_IMPORT_NS(DMA_BUF);
#endif

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

EXPORT_SYMBOL_GPL(xdma_get_bypassio);
EXPORT_SYMBOL_GPL(xdma_get_userio);
EXPORT_SYMBOL_GPL(xdma_user_isr_disable);
EXPORT_SYMBOL_GPL(xdma_user_isr_enable);
EXPORT_SYMBOL_GPL(xdma_user_isr_register);
EXPORT_SYMBOL_GPL(xdma_device_restart);
EXPORT_SYMBOL_GPL(xdma_device_online);
EXPORT_SYMBOL_GPL(xdma_device_offline);
EXPORT_SYMBOL_GPL(xdma_device_close);
EXPORT_SYMBOL_GPL(xdma_device_open);
EXPORT_SYMBOL_GPL(xdma_performance_submit);
EXPORT_SYMBOL_GPL(xdma_proc_aio_requests);
EXPORT_SYMBOL_GPL(xdma_xfer_submit);
EXPORT_SYMBOL_GPL(engine_cyclic_stop);
EXPORT_SYMBOL_GPL(get_perf_stats);
EXPORT_SYMBOL_GPL(enable_perf);
#endif
