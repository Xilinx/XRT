/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Min Ma <min.ma@xilinx.com>
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

#include "linux/device.h"
#include "linux/slab.h"
#include "kds_core.h"

struct kds_command *kds_alloc_command(struct kds_client *client, u32 size)
{
	struct kds_command *xcmd;

	/* TODO: Allocate buffer on critical path is not good
	 * Consider kmem_cache_alloc()
	 */
	xcmd = kzalloc(sizeof(struct kds_command), GFP_KERNEL);
	if (!xcmd)
		return NULL;

	xcmd->client = client;
	xcmd->type = 0;

	xcmd->info = kzalloc(size, GFP_KERNEL);
	if (!xcmd->info) {
		kfree(xcmd);
		return NULL;
	}

	return xcmd;
}

void kds_free_command(struct kds_command *xcmd)
{
	if (xcmd) {
		kfree(xcmd->info);
		kfree(xcmd);
	}
}

int kds_submit_cu(struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	struct kds_controller *ctrl = client->ctrl[KDS_CU];

	if (ctrl) {
		/* NOTE: If still has errors, the controller
		 * should mark command as ERROR and notify host
		 */
		ctrl->submit(ctrl, xcmd);
		return 0;
	}

	kds_err(client, "No CU controller to handle xcmd");
	xcmd->cb.notify_host(xcmd, KDS_ERROR);
	kds_free_command(xcmd);
	return -ENXIO;
}

int kds_add_command(struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	int err = 0;

	if (!xcmd->cb.notify_host) {
		kds_dbg(client, "No call back to notify host");
		return -EINVAL;
	}

	/* TODO: Check if command is blocked */

	/* Command is good to submit */
	switch (xcmd->type) {
	case KDS_CU:
		err = kds_submit_cu(xcmd);
		break;
	default:
		kds_err(client, "Unknown type");
		xcmd->cb.notify_host(xcmd, KDS_ERROR);
		kds_free_command(xcmd);
		err = -EINVAL;
	}

	return err;
}

int kds_init_client(struct kds_client *client)
{
	init_waitqueue_head(&client->waitq);
	atomic_set(&client->event, 0);
	return 0;
}

void kds_fini_client(struct kds_client *client)
{
	return;
}
