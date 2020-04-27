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

#include "kds_command.h"

struct kds_controller;

#define kds_info(client, fmt, args...)			\
	dev_info(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_err(client, fmt, args...)			\
	dev_err(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)
#define kds_dbg(client, fmt, args...)			\
	dev_dbg(client->dev, " %llx %s: "fmt, (u64)client->dev, __func__, ##args)

#define PRE_ALLOC 0

/**
 * struct kds_client: Manage user space client context attached to device
 *
 * @link: Client context is added to list in device
 * @dev:  Device
 * @pid:  Client process ID
 */
struct kds_client {
	struct list_head	  link;
	struct device	         *dev;
	struct pid	         *pid;
	struct kds_controller   **ctrl;
	wait_queue_head_t	  waitq;
	atomic_t		  event;
#if PRE_ALLOC
	u32			  max_xcmd;
	u32			  xcmd_idx;
	void			 *xcmds;
	void			 *infos;
#endif
};
#define	CLIENT_NUM_CU(client) (0)

struct kds_controller {
	void (* submit)(struct kds_controller *ctrl, struct kds_command *xcmd);
};

int kds_init_client(struct kds_client *client);
void kds_fini_client(struct kds_client *client);
struct kds_command *kds_alloc_command(struct kds_client *client, u32 size);
int kds_add_command(struct kds_command *xcmd);
void kds_free_command(struct kds_command *xcmd);

void notify_execbuf(struct kds_command *xcmd, int status);

#endif
