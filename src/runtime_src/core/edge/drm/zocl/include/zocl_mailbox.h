/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    David Zhang <davidzha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_MAILBOX_H_
#define _ZOCL_MAILBOX_H_

#define	MBX_STATUS_EMPTY	(1 << 0)
#define	MBX_STATUS_FULL		(1 << 1)
#define	MBX_STATUS_STA		(1 << 2)
#define	MBX_STATUS_RTA		(1 << 3)

/*
 * Mailbox IP register layout
 */
struct mailbox_reg {
	u32			mbr_wrdata;
	u32			mbr_resv1;
	u32			mbr_rddata;
	u32			mbr_resv2;
	u32			mbr_status;
	u32			mbr_error;
	u32			mbr_sit;
	u32			mbr_rit;
	u32			mbr_is;
	u32			mbr_ie;
	u32			mbr_ip;
	u32			mbr_ctrl;
} __packed;

struct mailbox {
	struct mailbox_reg	*mbx_regs;
};

u32 zocl_mailbox_status(struct mailbox *mbx);
u32 zocl_mailbox_get(struct mailbox *mbx);
void zocl_mailbox_set(struct mailbox *mbx, u32 val);

#endif
