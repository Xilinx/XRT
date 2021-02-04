/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
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

#define PRE_ALLOC 0

#define EV_ABORT	0x1

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
 * @virt_cu_ref: Reference count of virtual CU
 * @cu_bitmap: bitmap of opening CU
 * @waitq: Wait queue for poll client
 * @event: Events to notify user client
 */
struct kds_client {
	struct list_head	  link;
	struct device	         *dev;
	struct pid	         *pid;
	struct mutex		  lock;
	void			 *xclbin_id;
	int			  num_ctx;
	int			  virt_cu_ref;
	DECLARE_BITMAP(cu_bitmap, MAX_CUS);

	struct list_head	  ev_entry;
	int			  ev_type;

	/* Per CU counter that counts when a command is submitted to CU */
	unsigned long		  s_cnt[MAX_CUS] ____cacheline_aligned_in_smp;
#if PRE_ALLOC
	u32			  max_xcmd;
	u32			  xcmd_idx;
	void			 *xcmds;
	void			 *infos;
#endif
	/*
	 * Below are modified when the other thread is completing commands.
	 * In order to prevent false sharing, they need to be in different
	 * cache lines.
	 */
	/* Per CU counter that counts when a command is completed or error */
	unsigned long		  c_cnt[MAX_CUS] ____cacheline_aligned_in_smp;
	wait_queue_head_t	  waitq;
	atomic_t		  event;
};

#endif
