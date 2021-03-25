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

#include "kds_client.h"
#include "kds_command.h"
#include "xrt_cu.h"
#include "kds_stat.h"

#define kds_info(client, fmt, args...)			\
	dev_info(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_err(client, fmt, args...)			\
	dev_err(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_warn(client, fmt, args...)			\
	dev_warn(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_dbg(client, fmt, args...)			\
	dev_dbg(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)

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

/* TODO: PS kernel is very different with FPGA kernel.
 * Let's see if we can unify them later.
 */
struct kds_scu_mgmt {
	struct mutex		  lock;
	int			  num_cus;
	u32			  status[MAX_CUS];
	u32			  usage[MAX_CUS];
	char			  name[MAX_CUS][32];
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
	struct cu_stats __percpu *cu_stats;
	int			  rw_shared;
};

#define cu_stat_read(cu_mgmt, field) \
	stat_read((cu_mgmt)->cu_stats, field)

#define cu_stat_write(cu_mgmt, field, val) \
	stat_write((cu_mgmt)->cu_stats, field, val)

#define cu_stat_inc(cu_mgmt, field) \
	this_stat_inc((cu_mgmt)->cu_stats, field)

#define cu_stat_dec(cu_mgmt, field) \
	this_stat_dec((cu_mgmt)->cu_stats, field)

/* ERT core */
struct kds_ert {
	void (* submit)(struct kds_ert *ert, struct kds_command *xcmd);
	void (* abort)(struct kds_ert *ert, struct kds_client *client, int cu_idx);
	bool (* abort_done)(struct kds_ert *ert, struct kds_client *client, int cu_idx);
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
	struct kds_scu_mgmt	scu_mgmt;
	struct kds_ert	       *ert;
	bool			ini_disable;
	bool			ert_disable;
	u32			cu_intr_cap;
	u32			cu_intr;
	struct plram_info	plram;
	struct completion	comp;
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
struct kds_client *kds_get_client(struct kds_sched *kds, pid_t pid);
int kds_add_cu(struct kds_sched *kds, struct xrt_cu *xcu);
int kds_del_cu(struct kds_sched *kds, struct xrt_cu *xcu);
int kds_get_cu_total(struct kds_sched *kds);
u32 kds_get_cu_addr(struct kds_sched *kds, int idx);
u32 kds_get_cu_proto(struct kds_sched *kds, int idx);
int kds_add_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_ctx_info *info);
int kds_del_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_ctx_info *info);
int kds_map_cu_addr(struct kds_sched *kds, struct kds_client *client,
		    int idx, unsigned long size, u32 *addrp);
int kds_add_command(struct kds_sched *kds, struct kds_command *xcmd);
/* Use this function in xclbin download flow for config commands */
int kds_submit_cmd_and_wait(struct kds_sched *kds, struct kds_command *xcmd);

struct kds_command *kds_alloc_command(struct kds_client *client, u32 size);

void kds_free_command(struct kds_command *xcmd);

/* sysfs */
int store_kds_echo(struct kds_sched *kds, const char *buf, size_t count,
		   int kds_mode, u32 clients, int *echo);
ssize_t show_kds_stat(struct kds_sched *kds, char *buf);
ssize_t show_kds_custat_raw(struct kds_sched *kds, char *buf);
ssize_t show_kds_scustat_raw(struct kds_sched *kds, char *buf);
#endif
