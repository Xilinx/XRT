/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2021-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _KDS_CLIENT_H
#define _KDS_CLIENT_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/device.h>
#include <linux/uuid.h>

#include "xrt_cu.h"
#include "kds_stat.h"

#define EV_ABORT	0x1

/* Multiple CU context can be active under a single KDS client Context.
 */
struct kds_client_cu_ctx {
	u32				cu_idx;
	u32		  		cu_domain;
	u32				flags;
	u32				ref_cnt;
	struct kds_client_ctx		*ctx;
	struct kds_client_hw_ctx	*hw_ctx;
	struct list_head		link;
};

/* KDS CU information. */
struct kds_client_cu_info {
	u32				cu_idx;
	u32		  		cu_domain;
	u32				flags;
	void 				*ctx;
};

/* Multiple xclbin context can be active under a single client.
 * Client should maintain all the active XCLBIN.
 */
struct kds_client_ctx {
	void				*xclbin_id;
	bool				bitstream_locked;

	/* To support multiple CU context */
	struct list_head		cu_ctx_list;

	/* To support zocl multiple PL slot case */
	struct list_head		link;
	u32				slot_idx;
};

/* Multiple xclbin context can be active under a single client.
 * Client should maintain all the active XCLBIN.
 */
struct kds_client_hw_ctx {
	uint32_t 			hw_ctx_idx;
	void				*xclbin_id;
	u32				slot_idx;
	/* To support multiple context for multislot case */
	struct list_head		link;
	/* To support multiple CU context */
	struct list_head		cu_ctx_list;

	/* Per context statistics. Use percpu variable for two reasons
	 * 1. no lock is need while modifying these counters
	 * 2. do not need to worry about cache false share
	 */
	struct client_stats __percpu 	*stats;
};

struct kds_client_cu_refcnt {
	struct mutex	          lock;
	u32                       cu_refs[MAX_CUS];
	u32                       scu_refs[MAX_CUS];
};

/**
 * struct kds_client: Manage user client
 * Whenever user applciation open the device, a client would be created.
 * A client will keep alive util application close the device or being killed.
 * The client could open multiple contexts to access compute resources.
 *
 * @link: Client is added to list in KDS scheduler
 * @dev:  Device
 * @pid:  Client process ID
 * @lock: Mutex to protext context related members
 * @xclbin_id: UUID of xclbin cache
 * @num_ctx: Number of context that opened
 * @num_scu_ctx: Number of soft kernel context that opened
 * @virt_cu_ref: Reference count of virtual CU
 * @cu_bitmap: bitmap of opening CU
 * @scu_bitmap: bitmap of opening SCU
 * @waitq: Wait queue for poll client
 * @event: Events to notify user client
 */
struct kds_client {
	struct list_head	  link;
	struct device	         *dev;
	struct pid	         *pid;
	struct mutex		  lock;

	/* TODO: xocl not suppot multiple xclbin context yet. */
	struct kds_client_ctx    	*ctx;

	/* To suppot ZOCL  multiple PL support */
	struct list_head          	ctx_list;

	/* To suppot multiple hw context */
	struct list_head          	hw_ctx_list;
	uint32_t 		 	next_hw_ctx_id;

	struct list_head          graph_list;
	spinlock_t                graph_list_lock;
	u32                       aie_ctx;
	struct kds_client_cu_refcnt  *refcnt;

	struct list_head	  ev_entry;
	int			  ev_type;

	/*
	 * Below are modified when the other thread is completing commands.
	 * In order to prevent false sharing, they need to be in different
	 * cache lines.
	 */
	wait_queue_head_t	  waitq ____cacheline_aligned_in_smp;
	atomic_t		  event;
};

/* Macros to operates client statistics */
#define client_stat_read(client, hw_ctx, field)				\
({									\
	struct kds_client_hw_ctx *curr_ctx;				\
	typeof(((curr_ctx)->stats)->field) res = 0;			\
	list_for_each_entry(curr_ctx, &client->hw_ctx_list, link)	\
                if (curr_ctx->hw_ctx_idx == hw_ctx)			\
			res = stat_read((curr_ctx)->stats, field);	\
	res;								\
})

#define client_stat_inc(client, hw_ctx, field)				\
({									\
	struct kds_client_hw_ctx *curr_ctx;				\
	list_for_each_entry(curr_ctx, &client->hw_ctx_list, link)	\
		if (curr_ctx->hw_ctx_idx == hw_ctx)			\
			this_stat_inc((curr_ctx)->stats, field);	\
})

#define client_stat_dec(client, hw_ctx, field)				\
({									\
	struct kds_client_hw_ctx *curr_ctx;				\
        list_for_each_entry(curr_ctx, &client->hw_ctx_list, link)	\
                if (curr_ctx->hw_ctx_idx == hw_ctx)			\
			this_stat_dec((curr_ctx)->stats, field);	\
})

#endif
