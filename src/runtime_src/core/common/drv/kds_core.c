// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
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
	xcmd->cb.free(xcmd);
	return -ENXIO;
}

int kds_add_command(struct kds_command *xcmd)
{
	struct kds_client *client = xcmd->client;
	int err = 0;

	if (!xcmd->cb.notify_host || !xcmd->cb.free) {
		kds_dbg(client, "Command callback empty");
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
		xcmd->cb.free(xcmd);
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

/* General notify client function */
void notify_execbuf(struct kds_command *xcmd, int status)
{
	struct kds_client *client = xcmd->client;
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->execbuf;

	if (status == KDS_COMPLETED)
		ecmd->state = ERT_CMD_STATE_COMPLETED;
	else if (status == KDS_ERROR)
		ecmd->state = ERT_CMD_STATE_ERROR;

	atomic_inc(&client->event);
	wake_up_interruptible(&client->waitq);
}

void cfg_ecmd2xcmd(struct ert_configure_cmd *ecmd,
		   struct kds_command *xcmd)
{
	xcmd->type = KDS_CU;
	xcmd->opcode = OP_CONFIG_CTRL;

	xcmd->cb.notify_host = notify_execbuf;
	xcmd->execbuf = (u32 *)ecmd;

	xcmd->isize = ecmd->num_cus * sizeof(u32);
	/* Expect a ordered list of CU address */
	memcpy(xcmd->info, ecmd->data, xcmd->isize);
}

void start_krnl_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
			  struct kds_command *xcmd)
{
	xcmd->type = KDS_CU;
	xcmd->opcode = OP_START;

	xcmd->cb.notify_host = notify_execbuf;
	xcmd->execbuf = (u32 *)ecmd;

	xcmd->cu_mask[0] = ecmd->cu_mask;
	memcpy(&xcmd->cu_mask[1], ecmd->data, ecmd->extra_cu_masks);
	xcmd->num_mask = 1 + ecmd->extra_cu_masks;

	/* Skip first 4 control registers */
	xcmd->isize = (ecmd->count - xcmd->num_mask - 4) * sizeof(u32);
	memcpy(xcmd->info, &ecmd->data[4], xcmd->isize);
}

