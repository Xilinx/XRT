// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#ifndef _KDS_CU_CTRL_H
#define _KDS_CU_CTRL_H

#include "kds_core.h"
#include "xrt_cu.h"

#define CU_EXCLU_MASK		0x80000000

struct kds_cu_ctrl {
	struct kds_ctrl		  core;
	struct xrt_cu		 *xcus[MAX_CUS];
	struct mutex		  lock;
	u32			  cu_refs[MAX_CUS];
	u64			  cu_usage[MAX_CUS];
	int			  num_cus;
	int			  num_clients;
	int			  configured;
};

#define TO_CU_CTRL(core) ((struct kds_cu_ctrl *)(core))

struct client_cu_priv {
	DECLARE_BITMAP(cu_bitmap, MAX_CUS);
};

int config_ctrl(struct kds_cu_ctrl *kcuc, struct kds_command *xcmd);
int acquire_cu_inst_idx(struct kds_cu_ctrl *kcuc, struct kds_command *xcmd);
int control_ctx(struct kds_cu_ctrl *kcuc, struct kds_client *client,
		struct kds_ctx_info *info);
int add_cu(struct kds_cu_ctrl *kcuc, struct xrt_cu *xcu);
int remove_cu(struct kds_cu_ctrl *kcuc, struct xrt_cu *xcu);

ssize_t show_cu_ctx(struct kds_cu_ctrl *kcuc, char *buf);
ssize_t show_cu_ctrl_stat(struct kds_cu_ctrl *kcuc, char *buf);

#endif
