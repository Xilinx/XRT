/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: min.ma@xilinx.com
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _KDS_CORE_H
#define _KDS_CORE_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/device.h>
#include <linux/uuid.h>

#include "kds_command.h"
#include "xrt_cu.h"

#define kds_info(client, fmt, args...)			\
	dev_info(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_err(client, fmt, args...)			\
	dev_err(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_dbg(client, fmt, args...)			\
	dev_dbg(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)

#define PRE_ALLOC 0

enum kds_type {
	KDS_CU		= 0,
	KDS_ERT,
	KDS_MAX_TYPE, // always the last one
};

/* Context properties */
#define	CU_CTX_PROP_MASK	0x0F
#define	CU_CTX_SHARED		0x00
#define	CU_CTX_EXCLUSIVE	0x01

/* Virtual CU index
 * This is useful when there is no need to open a context on hardware CU,
 * but still need to lockdown the xclbin.
 */
#define	CU_CTX_VIRT_CU		0xffffffff
struct kds_ctx_info {
	u32		  cu_idx;
	u32		  flags;
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
#if PRE_ALLOC
	u32			  max_xcmd;
	u32			  xcmd_idx;
	void			 *xcmds;
	void			 *infos;
#endif
	/*
	 * "waitq" and "event" are modified in thread that is completing them.
	 * In order to prevent false sharing, they need to be in different
	 * cache lines. Hence we add a "padding" in between (assuming 128-byte
	 * is big enough for most CPU architectures).
	 */
	u64			  padding[16];
	wait_queue_head_t	  waitq;
	atomic_t		  event;
};

/* the MSB of cu_refs is used for exclusive flag */
#define CU_EXCLU_MASK		0x80000000
struct kds_cu_mgmt {
	struct xrt_cu		 *xcus[MAX_CUS];
	struct mutex		  lock;
	int			  num_cus;
	int			  num_cdma;
	u32			  cu_intr[MAX_CUS];
	u32			  cu_refs[MAX_CUS];
	u64			  cu_usage[MAX_CUS];
	int			  configured;
};

/* ERT core */
struct kds_ert {
	void (* submit)(struct kds_ert *ert, struct kds_command *xcmd);
};

struct plram_info {
	/* This is use for free bo, do not use it in shared code */
	void		       *bo;
	u64			bar_paddr;
	u64			dev_paddr;
	void __iomem		*vaddr;
	u32			size;
};

/**
 * struct kds_sched: KDS scheduler manage CUs and client list.
 *		     There should be only one KDS per device.
 *
 * @list_head: Client list
 * @num_client: Number of clients
 * @lock: Mutex to protect client list
 * @cu_mgmt: hardware CUs management data structure
 * @ert: remote scheduler
 * @ert_disable: remote scheduler is disabled or not
 * @cu_intr_cap: capbility of CU interrupt support
 * @cu_intr: CU or ERT interrupt. 1 for CU, 0 for ERT.
 */
struct kds_sched {
	struct list_head	clients;
	int			num_client;
	struct mutex		lock;
	bool			bad_state;
	struct kds_cu_mgmt	cu_mgmt;
	struct kds_ert	       *ert;
	bool			ert_disable;
	u32			cu_intr_cap;
	u32			cu_intr;
	struct plram_info	plram;
};

int kds_init_sched(struct kds_sched *kds);
int kds_init_ert(struct kds_sched *kds, struct kds_ert *ert);
int kds_init_client(struct kds_sched *kds, struct kds_client *client);
void kds_fini_sched(struct kds_sched *kds);
int kds_fini_ert(struct kds_sched *kds);
void kds_fini_client(struct kds_sched *kds, struct kds_client *client);
void kds_reset(struct kds_sched *kds);
int kds_cfg_update(struct kds_sched *kds);
int is_bad_state(struct kds_sched *kds);
u32 kds_live_clients(struct kds_sched *kds, pid_t **plist);
u32 kds_live_clients_nolock(struct kds_sched *kds, pid_t **plist);
int kds_add_cu(struct kds_sched *kds, struct xrt_cu *xcu);
int kds_del_cu(struct kds_sched *kds, struct xrt_cu *xcu);
int kds_add_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_ctx_info *info);
int kds_del_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_ctx_info *info);
int kds_add_command(struct kds_sched *kds, struct kds_command *xcmd);

struct kds_command *kds_alloc_command(struct kds_client *client, u32 size);

void kds_free_command(struct kds_command *xcmd);

/* sysfs */
int store_kds_echo(struct kds_sched *kds, const char *buf, size_t count,
		   int kds_mode, u32 clients, int *echo);
ssize_t show_kds_stat(struct kds_sched *kds, char *buf);
ssize_t show_kds_custat_raw(struct kds_sched *kds, char *buf);
#endif
