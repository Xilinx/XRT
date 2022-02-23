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
#include "xrt_cu.h"

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

/* return the size of the xgq start cu command for ps kernel */
int xgq_exec_convert_start_scu_cmd(struct xgq_cmd_start_cuidx *xgq_cmd,
					struct ert_start_kernel_cmd *ecmd)
{
	int num_mask = 0;
	int payload_size = 0;

	num_mask = 1 + ecmd->extra_cu_masks;
	payload_size = (ecmd->count - num_mask) * sizeof(u32);
	memcpy(xgq_cmd->data, &ecmd->data[0], payload_size);

	xgq_cmd->hdr.opcode = XGQ_CMD_OP_START_CUIDX;
	xgq_cmd->hdr.state = 1;
	xgq_cmd->hdr.count = payload_size;
	xgq_cmd->hdr.cu_domain = (SCU_DOMAIN>>16);

	return sizeof(xgq_cmd->hdr) + payload_size;
}

int xgq_exec_convert_start_kv_cu_cmd(struct xgq_cmd_start_cuidx *xgq_cmd,
				     struct ert_start_kernel_cmd *ecmd)
{
	int num_mask = 0;
	int payload_size = 0;

	num_mask = 1 + ecmd->extra_cu_masks;
	payload_size = (ecmd->count - num_mask) * sizeof(u32);
	memcpy(xgq_cmd->data, &ecmd->data[ecmd->extra_cu_masks], payload_size);

	xgq_cmd->hdr.opcode = XGQ_CMD_OP_START_CUIDX_KV;
	xgq_cmd->hdr.state = 1;
	xgq_cmd->hdr.count = payload_size;

	return sizeof(xgq_cmd->hdr) + payload_size;
}

/* return the size of the xgq clock calibration command */
int xgq_exec_convert_clock_calib_cmd(struct xgq_cmd_clock_calib *xgq_cmd,
					struct ert_packet *ecmd)
{

	xgq_cmd->hdr.opcode = XGQ_CMD_OP_CLOCK_CALIB;
	xgq_cmd->hdr.state = 1;
	xgq_cmd->hdr.count = ecmd->count * sizeof(u32);

	return sizeof(xgq_cmd->hdr) + xgq_cmd->hdr.count;
}

/* return the size of the xgq accessibility command */
int xgq_exec_convert_accessible_cmd(struct xgq_cmd_access_valid *xgq_cmd,
					struct ert_packet *ecmd)
{
	xgq_cmd->hdr.opcode = XGQ_CMD_OP_ACCESS_VALID;
	xgq_cmd->hdr.state = 1;
	xgq_cmd->hdr.count = ecmd->count * sizeof(u32);

	return sizeof(xgq_cmd->hdr) + xgq_cmd->hdr.count;
}

/* return the size of the xgq data integrity command */
int xgq_exec_convert_data_integrity_cmd(struct xgq_cmd_data_integrity *xgq_cmd,
					struct ert_packet *ecmd)
{
	xgq_cmd->hdr.opcode = XGQ_CMD_OP_DATA_INTEGRITY;
	xgq_cmd->hdr.state = 1;
	xgq_cmd->hdr.count = ecmd->count * sizeof(u32);

	return sizeof(struct xgq_cmd_data_integrity);	
}
