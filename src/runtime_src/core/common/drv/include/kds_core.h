// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#ifndef _KDS_CORE_H
#define _KDS_CORE_H

#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/device.h>
#include <linux/uuid.h>

#include "kds_command.h"

#define kds_info(client, fmt, args...)			\
	dev_info(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_err(client, fmt, args...)			\
	dev_err(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_dbg(client, fmt, args...)			\
	dev_dbg(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)

#define PRE_ALLOC 0

enum kds_type {
	KDS_CU		= 0,
	KDS_MAX_TYPE, // always the last one
};

/* Context properties */
#define	CU_CTX_PROP_MASK	0x0F
#define	CU_CTX_SHARED		0x00
#define	CU_CTX_EXCLUSIVE	0x01

/* Context operation bits indicated that what operation to perform */
#define	CU_CTX_OP_MASK		0xF0
#define	CU_CTX_OP_INIT		0x10
#define	CU_CTX_OP_FINI		0x20
#define	CU_CTX_OP_ADD		0x30
#define	CU_CTX_OP_DEL		0x40
/* Virtual CU index
 * This is useful when there is no need to open a context on hardware CU,
 * but still need to lockdown the xclbin.
 */
#define	CU_CTX_VIRT_CU		0xffffffff
struct kds_ctx_info {
	u32		  cu_idx;
	u32		  flags;
};

#define TO_KDS_CTRL(core) ((struct kds_ctrl *)(core))

/**
 * struct kds_ctrl: KDS controller core.
 * This is the basic controller that KDS should understand.
 * This should be the first member of a controller implementation.
 * Any members added here should be shared by all controllers.
 *
 * @control_ctx: Delete context from a client
 * @submit: Submit command to controller
 */
struct kds_ctrl {
	int (* control_ctx)(struct kds_ctrl *ctrl, struct kds_client *client,
			    struct kds_ctx_info *info);
	void (* submit)(struct kds_ctrl *ctrl, struct kds_command *xcmd);
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
 * @ctrl_priv: Private data of controller
 * @xclbin_id: UUID of xclbin cache
 * @num_ctx: Number of context that opened
 * @virt_cu_ref: Reference count of virtual CU
 * @ctrl: Pointer to controllers array
 * @waitq: Wait queue for poll client
 * @event: Events to notify user client
 */
struct kds_client {
	struct list_head	  link;
	struct device	         *dev;
	struct pid	         *pid;
	struct mutex		  lock;
	void			 *ctrl_priv[KDS_MAX_TYPE];
	void			 *xclbin_id;
	int			  num_ctx;
	int			  virt_cu_ref;
#if PRE_ALLOC
	u32			  max_xcmd;
	u32			  xcmd_idx;
	void			 *xcmds;
	void			 *infos;
#endif
	/*
	 * "ctrl" is used in thread that is submitting CU cmds.
	 * "waitq" and "event" are modified in thread that is completing them.
	 * In order to prevent false sharing, they need to be in different
	 * cache lines. Hence we add a "padding" in between (assuming 128-byte
	 * is big enough for most CPU architectures).
	 */
	struct kds_ctrl		**ctrl;
	u64			  padding[16];
	wait_queue_head_t	  waitq;
	atomic_t		  event;
};

/**
 * struct kds_sched: KDS scheduler manage controllers and client list.
 *		     There should be only one KDS on a device.
 *
 * @ctrl: Controllers array only one per KDS
 * @list_head: Client list
 * @num_client: Number of clients
 * @lock: Mutex to protect client list
 */
struct kds_sched {
	struct kds_ctrl	       *ctrl[KDS_MAX_TYPE];
	struct list_head	clients;
	int			num_client;
	struct mutex		lock;
};

int kds_init_sched(struct kds_sched *kds);
void kds_fini_sched(struct kds_sched *kds);
int kds_init_client(struct kds_sched *kds, struct kds_client *client);
void kds_fini_client(struct kds_sched *kds, struct kds_client *client);
u32 kds_live_clients(struct kds_sched *kds, pid_t **plist);
struct kds_command *kds_alloc_command(struct kds_client *client, u32 size);
int kds_add_context(struct kds_client *client, struct kds_ctx_info *info);
int kds_del_context(struct kds_client *client, struct kds_ctx_info *info);

int kds_add_command(struct kds_command *xcmd);
void kds_free_command(struct kds_command *xcmd);

#endif
