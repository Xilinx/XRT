/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu       <yliu@xilinx.com>
 *    David Zhang     <davidzha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "ert.h"
#include "zocl_drv.h"
#include "zocl_mailbox.h"

static inline const char *
reg2name(struct mailbox *mbx, u32 *reg)
{
	const char *reg_names[] = {
		"wrdata",
		"reserved1",
		"rddata",
		"reserved2",
		"status",
		"error",
		"sit",
		"rit",
		"is",
		"ie",
		"ip",
		"ctrl"
	};

	return reg_names[((uintptr_t)reg -
		(uintptr_t)mbx->mbx_regs) / sizeof(u32)];
}

static inline u32
mailbox_reg_read(u32 *reg)
{
	return ioread32(reg);
}

static inline void
mailbox_reg_write(u32 *reg, u32 val)
{
	iowrite32(val, reg);
}

u32
zocl_mailbox_status(struct mailbox *mbx)
{
	return mailbox_reg_read(&mbx->mbx_regs->mbr_status);
}

u32
zocl_mailbox_get(struct mailbox *mbx)
{
	return mailbox_reg_read(&mbx->mbx_regs->mbr_rddata);
}

void
zocl_mailbox_set(struct mailbox *mbx, u32 val)
{
	mailbox_reg_write(&mbx->mbx_regs->mbr_wrdata, val);
}
