/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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
#include <linux/kthread.h>

#include "kds_client.h"
#include "kds_command.h"
#include "xrt_cu.h"
#include "kds_stat.h"
#include "xclbin.h"

#define kds_info(client, fmt, args...)			\
	dev_info(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_err(client, fmt, args...)			\
	dev_err(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_warn(client, fmt, args...)			\
	dev_warn(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_dbg(client, fmt, args...)			\
	dev_dbg(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)

/*
 * A CU domain can contain the same type of CUs.
 * CUs from different domain can have different implementation details.
 * Typical domains: PL kernel domain, PS kernel domain
 *
 * The user space passed down cu index is encoding into
 * domain + index of a domain in below format.
 * +-------------------+-------------------+
 * | 31    ...      16 | 15     ...      0 |
 * +-------------------+-------------------+
 * |     Domain        |   Domain index    |
 * +-------------------+-------------------+
 *
 * Use below helper macros to handle user space cu index.
 */
enum kds_cu_domain {
	/* Virtual CU index
	 * This is useful when there is no need to open a context on hardware CU,
	 * but still need to lockdown the xclbin.
	 */
	DOMAIN_VIRT = 0xFFFF,
	DOMAIN_PL   = 0x0,
	DOMAIN_PS,
	MAX_DOMAIN /* always the last one */
};
#define DOMAIN_MASK  0xFFFF0000
#define DOMAIN_INDEX_MASK  0x0000FFFF
#define get_domain(idx) ((idx & DOMAIN_MASK) >> 16)
#define get_domain_idx(idx) (idx & DOMAIN_INDEX_MASK)
#define set_domain(domain, idx) ((domain << 16) + idx)

/* MAX CUs per domain */
#define MAX_CUS 128
/* TODO: This is only used in print custat and scustat
 * Is slot index in the range of 0 to 31 ??
 */
#define MAX_SLOT 32
#define MAX_CU_STAT_LINE_LENGTH  128

enum kds_type {
	KDS_CU		= 0,
	KDS_SCU,
	KDS_ERT,
	KDS_MAX_TYPE, // always the last one
};

/* Context properties */
#define	CU_CTX_PROP_MASK	0x0F
#define	CU_CTX_SHARED		0x00
#define	CU_CTX_EXCLUSIVE	0x01

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
	u32			slot_size;
	void (* submit)(struct kds_ert *ert, struct kds_command *xcmd);
	void (* abort)(struct kds_ert *ert, struct kds_client *client, int cu_idx);
	bool (* abort_done)(struct kds_ert *ert, struct kds_client *client, int cu_idx);
	bool (* abort_sync)(struct kds_ert *ert, struct kds_client *client, int cu_idx);
};

/* Fast adapter memory info */
#define FA_MEM_MAX_SIZE 128 * 1024
struct cmdmem_info {
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
 * @xgq_enable: remote scheduler supports XGQ
 * @cu_intr_cap: capbility of CU interrupt support
 * @cu_intr: CU or ERT interrupt. 1 for CU, 0 for ERT.
 * @anon_client: driver own kds client used with driver generated command
 * @polling_thread: poll CUs when ERT is disabled
 */
#define KDS_SYSFS_SETTING_BIT	(1 << 31)
#define KDS_SET_SYSFS_BIT(val)	(val | KDS_SYSFS_SETTING_BIT)
#define KDS_SYSFS_SETTING(val)	(val & KDS_SYSFS_SETTING_BIT)
#define KDS_SETTING(val)	(val & ~KDS_SYSFS_SETTING_BIT)
struct kds_sched {
	struct list_head	clients;
	int			num_client;
	struct mutex		lock;
	bool			bad_state;
	struct kds_cu_mgmt	cu_mgmt;
	struct kds_cu_mgmt	scu_mgmt;
	struct kds_ert	       *ert;
	bool			xgq_enable;
	u32			cu_intr_cap;
	struct cmdmem_info	cmdmem;
	struct completion	comp;
	struct kds_client      *anon_client;

	/* Settings */
	bool			ini_disable;
	bool			ert_disable;
	u32			cu_intr;

	/* KDS polling thread */
	struct task_struct     *polling_thread;
	struct list_head	alive_cus; /* alive CU list */
	wait_queue_head_t	wait_queue;
	int			polling_start;
	int			polling_stop;
	u32			interval;
};

int kds_init_sched(struct kds_sched *kds);
int kds_init_ert(struct kds_sched *kds, struct kds_ert *ert);
int kds_init_client(struct kds_sched *kds, struct kds_client *client);
void kds_fini_sched(struct kds_sched *kds);
int kds_fini_ert(struct kds_sched *kds);
void kds_fini_client(struct kds_sched *kds, struct kds_client *client);
void kds_reset(struct kds_sched *kds);
int kds_cfg_update(struct kds_sched *kds);
void kds_cus_irq_enable(struct kds_sched *kds, bool enable);
int is_bad_state(struct kds_sched *kds);
u32 kds_live_clients(struct kds_sched *kds, pid_t **plist);
u32 kds_live_clients_nolock(struct kds_sched *kds, pid_t **plist);
int kds_add_cu(struct kds_sched *kds, struct xrt_cu *xcu);
int kds_del_cu(struct kds_sched *kds, struct xrt_cu *xcu);
int kds_add_scu(struct kds_sched *kds, struct xrt_cu *xcu);
int kds_del_scu(struct kds_sched *kds, struct xrt_cu *xcu);
int kds_get_cu_total(struct kds_sched *kds);
u32 kds_get_cu_addr(struct kds_sched *kds, int idx);
u32 kds_get_cu_proto(struct kds_sched *kds, int idx);
int kds_get_max_regmap_size(struct kds_sched *kds);
/* Start of legacy context functions */
struct kds_client_cu_ctx *
kds_get_cu_ctx(struct kds_client *client, struct kds_client_ctx *ctx,
		struct kds_client_cu_info *cu_info);
struct kds_client_cu_ctx *
kds_alloc_cu_ctx(struct kds_client *client, struct kds_client_ctx *ctx,
		struct kds_client_cu_info *cu_info);
int kds_free_cu_ctx(struct kds_client *client, struct kds_client_cu_ctx *cu_ctx);
int kds_add_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_client_cu_ctx *cu_ctx);
int kds_del_context(struct kds_sched *kds, struct kds_client *client,
		    struct kds_client_cu_ctx *cu_ctx);
/* End of legacy context functions */

/* Start of hw context functions */
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
/* End of hw context functions */

int kds_open_ucu(struct kds_sched *kds, struct kds_client *client, u32 cu_idx);
int kds_map_cu_addr(struct kds_sched *kds, struct kds_client *client,
		    int idx, unsigned long size, u32 *addrp);
int kds_add_command(struct kds_sched *kds, struct kds_command *xcmd);
/* Use this function in xclbin download flow for config commands */
int kds_submit_cmd_and_wait(struct kds_sched *kds, struct kds_command *xcmd);
int kds_set_cu_read_range(struct kds_sched *kds, u32 cu_idx, u32 start, u32 size);

struct kds_command *kds_alloc_command(struct kds_client *client, u32 size);

void kds_free_command(struct kds_command *xcmd);
int kds_ip_layout2cu_info(struct ip_layout *ip_layout, struct xrt_cu_info cu_info[], int num_info);
int kds_ip_layout2scu_info(struct ip_layout *ip_layout, struct xrt_cu_info cu_info[], int num_info);

/* sysfs */
int store_kds_echo(struct kds_sched *kds, const char *buf, size_t count,
		   int *echo);
ssize_t show_kds_stat(struct kds_sched *kds, char *buf);
ssize_t show_kds_custat_raw(struct kds_sched *kds, char *buf, size_t buf_size, loff_t offset);
ssize_t show_kds_scustat_raw(struct kds_sched *kds, char *buf, size_t buf_size, loff_t offset);
ssize_t show_kds_cuctx_stat_raw(struct kds_sched *kds, char *buf, size_t buf_size, loff_t offset,
		uint32_t domain);
#endif
