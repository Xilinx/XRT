/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * MPSoC based OpenCL accelerators Compute Units.
 *
 * Copyright (C) 2019-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Min Ma      <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */


#include <drm/drm.h>
#include <drm/drm_print.h>
#include <linux/io.h>
#include <linux/vmalloc.h>
#include "zocl_cu.h"

void
zocl_cu_disable_intr(struct zocl_cu *cu, u32 type)
{
	cu->funcs->disable_intr(cu->core, type);
}

void
zocl_cu_enable_intr(struct zocl_cu *cu, u32 type)
{
	cu->funcs->enable_intr(cu->core, type);
}

u32
zocl_cu_clear_intr(struct zocl_cu *cu)
{
	return cu->funcs->clear_intr(cu->core);
}

int
zocl_cu_get_credit(struct zocl_cu *cu)
{
	struct zcu_core *cu_core = cu->core;

	cu_core->credits--;
	return cu_core->credits;
}

void
zocl_cu_refund_credit(struct zocl_cu *cu, u32 count)
{
	struct zcu_core *cu_core = cu->core;

	cu_core->credits += count;
}

void
zocl_cu_configure(struct zocl_cu *cu, u32 *data, size_t sz, int type)
{
	cu->funcs->configure(cu->core, data, sz, type);
}

void
zocl_cu_start(struct zocl_cu *cu)
{
	cu->funcs->start(cu->core);
}

void
zocl_cu_check(struct zocl_cu *cu)
{
	struct zcu_tasks_info tasks;

	cu->funcs->check(cu->core, &tasks);
	cu->done_cnt += tasks.num_tasks_done;
	cu->ready_cnt += tasks.num_tasks_ready;
}

void
zocl_cu_reset(struct zocl_cu *cu)
{
	cu->funcs->reset(cu->core);
}

int
zocl_cu_reset_done(struct zocl_cu *cu)
{
	return cu->funcs->reset_done(cu->core);
}

phys_addr_t
zocl_cu_get_paddr(struct zocl_cu *cu)
{
	struct zcu_core *cu_core = cu->core;

	return cu_core->paddr;
}

void
zocl_cu_status_print(struct zocl_cu *cu)
{
	struct zcu_core *cu_core = cu->core;

	if (!cu_core)
		return;

	DRM_INFO("addr 0x%llx, status 0x%x",
	    (u64)cu_core->paddr, ioread32(cu_core->vaddr));
}

int
zocl_cu_status_get(struct zocl_cu *cu)
{
	struct zcu_core *cu_core = cu->core;

	if (!cu_core)
		return -1;//soft cu has crashed

	return (u32)ioread32(cu_core->vaddr);
}

u32
zocl_cu_get_control(struct zocl_cu *cu)
{
	struct zcu_core *cu_core = cu->core;

	return cu_core->control;
}

/* -- HLS adapter start -- */
/* HLS adapter implementation realted code. */
static void
zocl_hls_enable_intr(void *core, u32 intr_type)
{
	struct zcu_core *cu_core = core;
	u32 intr_mask = intr_type & CU_INTR_DONE;

	/* 0x04 and 0x08 -- Interrupt Enable Registers */
	iowrite32(0x1, cu_core->vaddr + 1);
	/*
	 * bit 0 is ap_done, bit 1 is ap_ready
	 * only enable ap_done before dataflow support, interrupts are handled
	 * in sched_exec_isr, please see dataflow comments for more information.
	 */
	iowrite32(intr_mask, cu_core->vaddr + 2);
}

static void
zocl_hls_disable_intr(void *core, u32 intr_type)
{
	struct zcu_core *cu_core = core;
	u32 intr_mask = intr_type & ioread32(cu_core->vaddr + 2);

	/* 0x04 and 0x08 -- Interrupt Enable Registers */
	iowrite32(0x0, cu_core->vaddr + 1);
	/* bit 0 is ap_done, bit 1 is ap_ready, disable both of interrupts */
	iowrite32(intr_mask, cu_core->vaddr + 2);
}

static u32
zocl_hls_clear_intr(void *core)
{
	struct zcu_core *cu_core = core;

	/* Clear all interrupts of the CU
	 *
	 * HLS style kernel has Interrupt Status Register at offset 0x0C
	 * It has two interrupt bits, bit[0] is ap_done, bit[1] is ap_ready.
	 *
	 * The ap_done interrupt means this CU is complete.
	 * The ap_ready interrupt means all inputs have been read.
	 */
	if (cu_core->max_credits == 1) {
		u32 isr;

		/*
		 * The old HLS adapter.
		 *
		 * The Interrupt Status Register is Toggle On Write
		 * RegData = RegData ^ WriteData
		 *
		 * So, the reliable way to clear this register is read
		 * then write the same value back.
		 *
		 * Do not write 1 to this register.  If, somehow, the
		 * status register is 0, write 1 to this register will
		 * trigger interrupt.
		 */
		isr = ioread32(cu_core->vaddr + 3);
		iowrite32(isr, cu_core->vaddr + 3);
		return isr;
	}

	/*
	 * The new HLS adapter with queue.
	 *
	 * The Interrupt Status Register is Clear on Read.
	 *
	 * For debug purpose, This register is toggle on write.
	 * Write 1 to this register will trigger interrupt.
	 */
	return ioread32(cu_core->vaddr + 3);
}

static void
zocl_hls_configure(void *core, u32 *data, size_t sz, int type)
{
	struct zcu_core *cu_core = core;
	u32 *base_addr = cu_core->vaddr;
	u32 i, offset, val;

	switch (type) {
	case CONSECUTIVE:
		/* Write register map, starting at base_addr + 0x10 (byte)
		 * This based on the fact that kernel used
		 *	0x00 -- Control Register
		 *	0x04 and 0x08 -- Interrupt Enable Registers
		 *	0x0C -- Interrupt Status Register
		 * Skip the first 4 words in user regmap.
		 */
		for (i = 4; i < sz; ++i)
			iowrite32(data[i], base_addr + i);
		break;
	case PAIRS:
		/* This is the {offset, value} pairs to configure CU.
		 * It relies on the KDS/ERT command format.
		 * The data in the command is 32 bits.
		 */
		for (i = 0; i < sz - 1; i += 2) {
			 /* The offset is the offset to CU base address */
			offset = *(data + i);
			val = *(data + i + 1);

			iowrite32(val, base_addr + offset / 4);
		}
		break;
	}
}

static void
zocl_hls_start(void *core)
{
	struct zcu_core *cu_core = core;

	/* Bit 0 -- The CU start control bit.
	 * Write 0 to this bit will be ignored.
	 * Until the CU is ready to take next task, this bit will reamin 1.
	 * Once ths CU is ready, it will clear this bit.
	 * So, if this bit is 1, it means the CU is running.
	 */
	iowrite32(0x1, cu_core->vaddr);
}

static void
zocl_hls_check(void *core, struct zcu_tasks_info *tasks_info)
{
	struct zcu_core *cu_core = core;
	u32 version;
	u32 ctrl_reg;
	u32 ready_cnt = 0;
	u32 done_cnt = 0;

	/* done is indicated by AP_DONE(2) alone or by AP_DONE(2) | AP_IDLE(4)
	 * but not by AP_IDLE itself.  Since 0x10 | (0x10 | 0x100) = 0x110
	 * checking for 0x10 is sufficient.
	 */
	ctrl_reg  = ioread32(cu_core->vaddr);
	version   = (ctrl_reg & CU_VERSION_MASK) >> 8;

	/* For the old version of HLS adapter
	 * There is no ready and done counter. If done bit is 1, means CU is
	 * ready for a new command and a command was done.
	 */
	if (version != 0) {
		ready_cnt = (ctrl_reg & CU_READY_CNT_MASK) >> 16;
		done_cnt  = (ctrl_reg & CU_DONE_CNT_MASK) >> 24;
	} else if (ctrl_reg & CU_AP_DONE) {
		ready_cnt = 1;
		done_cnt = 1;

		/*
		 * wrtie AP_CONTINUE to restart CU.
		 * this is safe for all hls/versal kernel
		 */
		iowrite32(CU_AP_CONTINUE, cu_core->vaddr);
		/*
		 * reading AP_DONE is redudent, it should be done in next cycle.
		 */
		ctrl_reg = ioread32(cu_core->vaddr);
		if ((ctrl_reg & CU_AP_DONE) != 0)
			DRM_ERROR("AP_DONE is not zero: 0x%x", ctrl_reg);

	}

	tasks_info->num_tasks_ready = ready_cnt;
	tasks_info->num_tasks_done = done_cnt;
}

static void
zocl_hls_reset(void *core)
{
	struct zcu_core *cu_core = core;

	iowrite32(CU_AP_RESET, cu_core->vaddr);
}

static int
zocl_hls_reset_done(void *core)
{
	struct zcu_core *cu_core = core;
	u32 status;

	status = ioread32(cu_core->vaddr);

	/* reset done is indicated by AP_RESET_DONE, bit 6 */
	if (status & (1 << 6))
		return true;

	return false;
}

static struct zcu_funcs hls_adapter_ops = {
	.enable_intr	= zocl_hls_enable_intr,
	.disable_intr	= zocl_hls_disable_intr,
	.clear_intr	= zocl_hls_clear_intr,
	.configure	= zocl_hls_configure,
	.start		= zocl_hls_start,
	.check		= zocl_hls_check,
	.reset		= zocl_hls_reset,
	.reset_done	= zocl_hls_reset_done,
};

static int
zocl_hls_cu_init(struct zocl_cu *cu, phys_addr_t paddr)
{
	struct zcu_core *core;
	u32 ctrl_reg;
	u32 version;
	u32 max_cap;

	core = vzalloc(sizeof(struct zcu_core));
	if (!core) {
		DRM_ERROR("Could not allocate CU core object\n");
		return -ENOMEM;
	}

	core->control = paddr & 0x7;
	paddr = paddr & ZOCL_KDS_MASK;
	core->paddr = paddr;
	core->vaddr = ioremap(paddr, CU_SIZE);
	if (!core->vaddr) {
		DRM_ERROR("Mapping CU failed\n");
		vfree(core);
		return -ENOMEM;
	}

	DRM_DEBUG("CU 0x%llx map to 0x%p\n", (u64)core->paddr, core->vaddr);
	ctrl_reg = ioread32(core->vaddr);
	version = (ctrl_reg & CU_VERSION_MASK) >> 8;
	max_cap = (ctrl_reg & CU_MAX_CAP_MASK) >> 12;
	switch (version) {
	case 1:
		core->max_credits = 1 << max_cap;
		break;
	default:
		core->max_credits = 1;
	}
	core->credits = core->max_credits;

	core->intr_type = CU_INTR_DONE | CU_INTR_READY;

	/* In case CU object is not initialled as 0 */
	cu->done_cnt = 0;
	cu->ready_cnt = 0;
	cu->usage = 0;
	cu->core = core;
	cu->funcs = &hls_adapter_ops;

	INIT_LIST_HEAD(&cu->running_queue);

	return 0;
}

static int
zocl_hls_cu_fini(struct zocl_cu *cu)
{
	struct zcu_core *core = cu->core;

	if (!core)
		return 0;

	if (core->vaddr)
		iounmap(core->vaddr);
	vfree(core);

	return 0;
}

/* -- HLS adapter end -- */

/* -- ACC adapter start -- */
static void
zocl_acc_configure(void *core, u32 *data, size_t sz, int type)
{
	struct zcu_core *cu_core = core;
	u32 *base_addr = cu_core->vaddr;
	u32 i, offset, val;

	if (type != PAIRS)
		return;

	/*
	 * Same open issue like HLS adapter
	 * Skip 6 data,this is how user layer construct the command.
	 */
	for (i = 6; i < sz - 1; i += 2) {
		offset = *(data + i) - cu_core->paddr;
		val = *(data + i + 1);

		iowrite32(val, base_addr + offset/4);
	}
}

static void
zocl_acc_start(void *core)
{
	struct zcu_core *cu_core = core;
	/* 0x08 -- Command register
	 * Write a zero to the command queue to indicate the start of a command
	 */
	iowrite32(0x0, cu_core->vaddr + 2);
}

static void
zocl_acc_check(void *core, struct zcu_tasks_info *tasks_info)
{
	struct zcu_core *cu_core = core;
	u32 status;

	tasks_info->num_tasks_ready = 0;
	tasks_info->num_tasks_done = 0;
	/* 0x04 -- Status register. If the CU is idle, the value is 0x6 or 0x4.
	 * Bit 5, O means a task is finished. 1 means done queue empty
	 * So, if the CU is running or idle, this bit would be 1.
	 */
	status = ioread32(cu_core->vaddr + 1);
	if ((status & 0x20) == 0) {
		/* One command is done.
		 * This adapter didn't support CU ready signal.
		 * So think the CU alway ready and done at the same time.
		 */
		tasks_info->num_tasks_ready = 1;
		tasks_info->num_tasks_done = 1;
	}
}

static struct zcu_funcs acc_adapter_ops = {
	.configure	= zocl_acc_configure,
	.start		= zocl_acc_start,
	.check		= zocl_acc_check,
};

static int
zocl_acc_cu_init(struct zocl_cu *cu, phys_addr_t paddr)
{
	struct zcu_core *core;

	core = vzalloc(sizeof(struct zcu_core));
	if (!core) {
		DRM_ERROR("Could not allocate CU core object\n");
		return -ENOMEM;
	}

	core->paddr = paddr;
	core->vaddr = ioremap(paddr, CU_SIZE);
	if (!core->vaddr) {
		DRM_ERROR("Mapping CU failed\n");
		vfree(core);
		return -ENOMEM;
	}

	DRM_DEBUG("CU 0x%llx map to 0x%p\n", (u64)core->paddr, core->vaddr);
	/* Unless otherwise configured, the adapter IP has space for 16
	 * outstanding computations.
	 */
	core->max_credits = 16;
	core->credits = core->max_credits;

	core->intr_type = 0;
	core->control = 0;

	cu->done_cnt = 0;
	cu->ready_cnt = 0;
	cu->usage = 0;
	cu->core = core;
	cu->funcs = &acc_adapter_ops;

	INIT_LIST_HEAD(&cu->running_queue);

	return 0;
}

static int
zocl_acc_cu_fini(struct zocl_cu *cu)
{
	struct zcu_core *core = cu->core;

	if (!core)
		return 0;

	if (core->vaddr)
		iounmap(core->vaddr);
	vfree(core);

	return 0;
}
/* -- ACC adapter end -- */

int zocl_cu_init(struct zocl_cu *cu, enum zcu_model m, phys_addr_t paddr)
{
	cu->model = m;
	switch (cu->model) {
	case MODEL_HLS:
		return zocl_hls_cu_init(cu, paddr);
	case MODEL_ACC:
		return zocl_acc_cu_init(cu, paddr);
	default:
		DRM_ERROR("Unknown CU model\n");
		return -EINVAL;
	}
}

int zocl_cu_fini(struct zocl_cu *cu)
{
	if (!cu)
		return 0;

	switch (cu->model) {
	case MODEL_HLS:
		return zocl_hls_cu_fini(cu);
	case MODEL_ACC:
		return zocl_acc_cu_fini(cu);
	default:
		DRM_ERROR("Unknown CU model\n");
		return -EINVAL;
	}
}
