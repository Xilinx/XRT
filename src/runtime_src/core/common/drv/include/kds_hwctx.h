/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors: saifuddi@amd.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _KDS_HWCTX_H
#define _KDS_HWCTX_H

#include <linux/list.h>

#include "kds_client.h"

/* Multiple CU context can be active under a single KDS client Context.
 */
struct kds_client_cu_ctx {
	u32				cu_idx;
	u32				cu_domain;
	u32				flags;
	u32				ref_cnt;
	struct kds_client_ctx		*ctx;
	struct kds_client_hw_ctx	*hw_ctx;
	struct list_head		link;
};

/* Multiple xclbin context can be active under a single client.
 * Client should maintain all the active XCLBIN.
 */
struct kds_client_hw_ctx {
	uint32_t			hw_ctx_idx;
	void				*xclbin_id;
	u32				slot_idx;

	/* To support multiple context for multislot case */
	struct list_head		link;

	/* To support multiple CU context */
	struct list_head		cu_ctx_list;

	/* To support multiple graph context */
	struct list_head		graph_ctx_list;

	/* Per context statistics. Use percpu variable for two reasons
	 * 1. no lock is need while modifying these counters
	 * 2. do not need to worry about cache false share
	 */
	struct client_stats __percpu	*stats;
};

struct kds_sched;

struct kds_client_cu_ctx *
kds_get_cu_ctx(struct kds_client *client, struct kds_client_ctx *ctx,
	                       struct kds_client_cu_info *cu_info);
struct kds_client_cu_ctx *
kds_alloc_cu_ctx(struct kds_client *client, struct kds_client_ctx *ctx,
		                 struct kds_client_cu_info *cu_info);
int
kds_free_cu_ctx(struct kds_client *client, struct kds_client_cu_ctx *cu_ctx);

struct kds_client_cu_ctx *
kds_get_cu_hw_ctx(struct kds_client *client, struct kds_client_hw_ctx *hw_ctx,
		                  struct kds_client_cu_info *cu_info);
struct kds_client_cu_ctx *
kds_alloc_cu_hw_ctx(struct kds_client *client, struct kds_client_hw_ctx *hw_ctx,
		                    struct kds_client_cu_info *cu_info);
struct kds_client_hw_ctx *
kds_get_hw_ctx_by_id(struct kds_client *client, uint32_t hw_ctx_id);

struct kds_client_hw_ctx *
kds_alloc_hw_ctx(struct kds_client *client, uuid_t *xclbin_id, uint32_t slot_id);

int kds_free_hw_ctx(struct kds_client *client, struct kds_client_hw_ctx *hw_ctx);

ssize_t
show_kds_cuctx_stat_raw(struct kds_sched *kds, char *buf, size_t buf_size,
			loff_t offset, uint32_t domain);

void kds_fini_hw_ctx_client(struct kds_sched *kds, struct kds_client *client,
                 struct kds_client_hw_ctx *hw_ctx);
#endif
