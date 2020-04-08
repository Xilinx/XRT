// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "linux/device.h"
#include "linux/slab.h"
#include "kds_core.h"

struct kds_command *kds_alloc_command(struct kds_client *client, u32 size)
{
#if PRE_ALLOC
	struct kds_command *xcmds;
#endif
	struct kds_command *xcmd;

	/* TODO: Allocate buffer on critical path is not good
	 * Consider kmem_cache_alloc()
	 */
#if PRE_ALLOC
	xcmds = client->xcmds;
	xcmd = &xcmds[client->xcmd_idx];
#else
	xcmd = kzalloc(sizeof(struct kds_command), GFP_KERNEL);
	if (!xcmd)
		return NULL;
#endif

	xcmd->client = client;
	xcmd->type = 0;

#if PRE_ALLOC
	xcmd->info = client->infos + sizeof(u32) * 128;
	++client->xcmd_idx;
	client->xcmd_idx &= (client->max_xcmd - 1);
#else
	xcmd->info = kzalloc(size, GFP_KERNEL);
	if (!xcmd->info) {
		kfree(xcmd);
		return NULL;
	}
#endif

	return xcmd;
}

void kds_free_command(struct kds_command *xcmd)
{
#if PRE_ALLOC
	return;
#else
	if (xcmd) {
		kfree(xcmd->info);
		kfree(xcmd);
	}
#endif
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
#if PRE_ALLOC
	client->max_xcmd = 0x8000;
	client->xcmd_idx = 0;
	client->xcmds = vzalloc(sizeof(struct kds_command) * client->max_xcmd);
	if (!client->xcmds) {
		kds_err(client, "cound not allocate xcmds");
		return -ENOMEM;
	}
	client->infos = vzalloc(sizeof(u32) * 128 * client->max_xcmd);
	if (!client->infos) {
		kds_err(client, "cound not allocate infos");
		return -ENOMEM;
	}
#endif

	return 0;
}

void kds_fini_client(struct kds_client *client)
{
#if PRE_ALLOC
	vfree(client->xcmds);
	vfree(client->infos);
#endif
	return;
}
