/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020-2021 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _KDS_COMMAND_H
#define _KDS_COMMAND_H

#include "xgq_cmd_ert.h"
/* Userspace command format */
#include "ert.h"

#define REGMAP 0
#define KEY_VAL 1
#define XGQ_CMD 2

enum kds_opcode {
	OP_NONE = 0,
	OP_CONFIG,
	OP_START,
	OP_ABORT,
	OP_CONFIG_SK, /* TODO: There is a plan to remove softkernel config and unconfig command */
	OP_START_SK,
	OP_CLK_CALIB,
	OP_VALIDATE,
	OP_GET_STAT,
};

/* KDS_NEW:		Command is validated
 * KDS_QUEUED:		Command is sent to pending queue
 * KDS_RUNNING:		Command is sent to hardware (CU/ERT)
 * KDS_COMPLETED:	Command is completed
 * KDS_ERROR:		Command is error out
 * KDS_ABORT:		Command is abort
 * KDS_TIMEOUT:		Command is timeout
 */
enum kds_status {
	KDS_NEW = 0,
	KDS_QUEUED,
	KDS_RUNNING,
	KDS_COMPLETED,
	KDS_ERROR,
	KDS_ABORT,
	KDS_TIMEOUT,
	KDS_STAT_MAX,
};

struct kds_command;

struct kds_cmd_ops {
	void (*notify_host)(struct kds_command *xcmd, int status);
	void (*free)(struct kds_command *xcmd);
};

struct in_kernel_cb {
	void (*func)(unsigned long cb_data, int err);
	void *data;
};

/* Default cu index of a command */
#define NO_INDEX -1

/**
 * struct kds_command: KDS command struct
 * @client: the client that the command belongs to
 * @type:   type of the command. Use this to determin controller
 */
struct kds_command {
	struct kds_client	*client;
	int			 status;
	u32			 rcode;
	int			 cu_idx;
	u32			 type;
	u32			 opcode;
	struct list_head	 list;
	u32			 payload_alloc;
	u32			 payload_type;
	void			*info;
	size_t			 isize;
	void                    *response;
	size_t                   response_size;
	struct kds_cmd_ops	 cb;
	void			*priv;

	unsigned int		 tick;
	u32			 timestamp_enabled;
	u64			 timestamp[KDS_STAT_MAX];

	/* TODO: may rethink if we should have cu bit mask here
	 * or move it to info.
	 * Since NO all types of command have cu_mask
	 */
	u32			 cu_mask[4];
	u32			 num_mask;
	u64			 start;

	/* execbuf is used to update the header
	 * of execbuf when notifying host
	 */
	u32			*execbuf;
	u32			*u_execbuf;
	void			*gem_obj;
	u32			 exec_bo_handle;
	/* to notify inkernel exec completion */
	struct in_kernel_cb	*inkern_cb;
};

void set_xcmd_timestamp(struct kds_command *xcmd, enum kds_status s);

/* execbuf command related funtions */
void cfg_ecmd2xcmd(struct ert_configure_cmd *ecmd,
		   struct kds_command *xcmd);
void start_krnl_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			  struct kds_command *xcmd);
void start_skrnl_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			  struct kds_command *xcmd);
void start_krnl_kv_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			     struct kds_command *xcmd);
void start_fa_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			struct kds_command *xcmd);
void abort_ecmd2xcmd(struct ert_abort_cmd *ecmd,
		     struct kds_command *xcmd);
int cu_mask_to_cu_idx(struct kds_command *xcmd, uint8_t *cus);

#endif
