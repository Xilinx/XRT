/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General
 Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/string.h>
#include "xocl_drv.h"
#include "xgq_execbuf.h"

/* return the size of the xgq start cu command */
int xgq_exec_convert_start_cu_cmd(struct xgq_cmd_start_cuidx *xgq_cmd,
				   struct ert_start_kernel_cmd *ecmd)
{
	int num_mask = 0;
	int payload_size = 0;

	num_mask = 1 + ecmd->extra_cu_masks;
	payload_size = (ecmd->count - num_mask - 4) * sizeof(u32);
	memcpy(xgq_cmd->data, &ecmd->data[4 + ecmd->extra_cu_masks], payload_size);

	xgq_cmd->hdr.opcode = XGQ_CMD_OP_START_CUIDX;
	xgq_cmd->hdr.state = 1;
	xgq_cmd->hdr.count = payload_size;

	return sizeof(xgq_cmd->hdr) + payload_size;
}

