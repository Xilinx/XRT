/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx ERT Model
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Authors: chienwei@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _XRT_ERT_H
#define _XRT_ERT_H

#include "ert.h"
#include "kds_command.h"
#include "xgq_cmd_common.h"

struct xrt_ert_command;

struct ert_cmd_ops {
	void (*complete)(struct xrt_ert_command *ecmd, void *core);
	void (*notify)(void *core);
	void (*free_payload)(void *payload);
};

struct xrt_ert_command {
	struct list_head		list;
	struct kds_command		*xcmd;

	uint32_t			handle;

	void 				*client;
	struct ert_cmd_ops		cb;
	uint32_t			*payload;
	// payload size in words
	uint32_t			payload_size;
	uint32_t			cu_idx;
	struct xgq_com_queue_entry	complete_entry;
	uint32_t			response_size;

	uint32_t			response[0];
};

struct ert_queue {
	void				*handle;
	struct xrt_ert_queue_funcs	*func;
	uint64_t			size;
};

struct xrt_ert_queue_funcs {

	void (*poll)(void *queue_handle);

	int (*submit)(struct xrt_ert_command *ecmd, void *queue_handle);

	int  (*queue_config)(uint32_t slot_size, bool polling, void *ert_handle, void *queue_handle);

	uint32_t (*max_slot_num)(void *queue_handle);

	void (*abort)(void *client, void *queue_handle);

	void (*intc_config)(bool enable, void *queue_handle);

};

#endif /* _XRT_ERT_H */
