/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *	Min Ma <min.ma@xilinx.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

#endif
