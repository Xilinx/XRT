/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _XGQ_EXECBUF_H
#define _XGQ_EXECBUF_H

#include "ert.h"
#include "xgq_cmd_ert.h"

int xgq_exec_convert_start_cu_cmd(struct xgq_cmd_start_cuidx *xgq_cmd,
					struct ert_start_kernel_cmd *ecmd);
int xgq_exec_convert_start_scu_cmd(struct xgq_cmd_start_cuidx *xgq_cmd,
					struct ert_start_kernel_cmd *ecmd);
int xgq_exec_convert_start_kv_cu_cmd(struct xgq_cmd_start_cuidx *xgq_cmd,
				     struct ert_start_kernel_cmd *ecmd);
int xgq_exec_convert_clock_calib_cmd(struct xgq_cmd_clock_calib *xgq_cmd,
					struct ert_packet *ecmd);
int xgq_exec_convert_accessible_cmd(struct xgq_cmd_access_valid *xgq_cmd,
					struct ert_packet *ecmd);
int xgq_exec_convert_data_integrity_cmd(struct xgq_cmd_data_integrity *xgq_cmd,
					struct ert_packet *ecmd);
#endif /* _XGQ_EXECBUF_H */
