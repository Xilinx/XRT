// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver Scheduler
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include "kds_core.h"

/* for sysfs */
int store_kds_echo(struct kds_sched *kds, const char *buf, size_t count,
		   int kds_mode, u32 clients, int *echo)
{
	u32 enable;
	u32 live_clients;

	if (kds)
		live_clients = kds_live_clients(kds, NULL);
	else
		live_clients = clients;

	/* Ideally, KDS should be locked to reject new client.
	 * But, this node is hidden for internal test purpose.
	 * Let's refine it after new KDS is the default and
	 * user is allow to configure it through xbutil.
	 */
	if (live_clients > 0)
		return -EBUSY;

	if (kstrtou32(buf, 10, &enable) == -EINVAL || enable > 1)
		return -EINVAL;

	*echo = enable;

	return count;
}
/* sysfs end */

int kds_init_sched(struct kds_sched *kds)
{
	INIT_LIST_HEAD(&kds->clients);
	mutex_init(&kds->lock);
	kds->num_client = 0;

	return 0;
}

void kds_fini_sched(struct kds_sched *kds)
{
	mutex_destroy(&kds->lock);
}

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
	struct kds_ctrl *ctrl = client->ctrl[KDS_CU];

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

int kds_init_client(struct kds_sched *kds, struct kds_client *client)
{
	struct kds_ctx_info info;
	struct kds_ctrl *ctrl;

	client->pid = get_pid(task_pid(current));
	client->ctrl = kds->ctrl;
	mutex_init(&client->lock);

	/* Initial controller context private data */
	ctrl = client->ctrl[KDS_CU];
	info.flags = CU_CTX_OP_INIT;
	if (ctrl)
		ctrl->control_ctx(ctrl, client, &info);

	init_waitqueue_head(&client->waitq);
	atomic_set(&client->event, 0);

	mutex_lock(&kds->lock);
	list_add_tail(&client->link, &kds->clients);
	kds->num_client++;
	mutex_unlock(&kds->lock);

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

void kds_fini_client(struct kds_sched *kds, struct kds_client *client)
{
	struct kds_ctx_info info;
	struct kds_ctrl *ctrl;

#if PRE_ALLOC
	vfree(client->xcmds);
	vfree(client->infos);
#endif
	ctrl = client->ctrl[KDS_CU];
	info.flags = CU_CTX_OP_FINI;
	if (ctrl)
		ctrl->control_ctx(ctrl, client, &info);

	put_pid(client->pid);
	mutex_destroy(&client->lock);

	mutex_lock(&kds->lock);
	list_del(&client->link);
	kds->num_client--;
	mutex_unlock(&kds->lock);
}

int kds_add_context(struct kds_client *client, struct kds_ctx_info *info)
{
	u32 cu_idx = info->cu_idx;
	bool shared = (info->flags != CU_CTX_EXCLUSIVE);
	struct kds_ctrl *ctrl = client->ctrl[KDS_CU];

	BUG_ON(!mutex_is_locked(&client->lock));

	/* TODO: In lagcy KDS, there is a concept of implicit CUs.
	 * It looks like that part is related to cdma. But it use the same
	 * cu bit map and it relies on to how user open context.
	 * Let's consider that kind of CUs later.
	 */
	if (cu_idx == CU_CTX_VIRT_CU) {
		if (!shared) {
			kds_err(client, "Only allow share virtual CU");
			return -EINVAL;
		}
		++client->virt_cu_ref;
	} else {
		if (!ctrl)
			return -ENODEV;
		info->flags &= ~CU_CTX_OP_MASK;
		info->flags |= CU_CTX_OP_ADD;
		if (ctrl->control_ctx(ctrl, client, info))
			return -EINVAL;
	}

	++client->num_ctx;
	kds_info(client, "Client pid(%d) add context CU(0x%x) shared(%s)",
		 pid_nr(client->pid), cu_idx, shared? "true" : "false");
	return 0;
}

int kds_del_context(struct kds_client *client, struct kds_ctx_info *info)
{
	u32 cu_idx = info->cu_idx;
	struct kds_ctrl *ctrl = client->ctrl[KDS_CU];

	BUG_ON(!mutex_is_locked(&client->lock));

	if (cu_idx == CU_CTX_VIRT_CU) {
		if (!client->virt_cu_ref) {
			kds_err(client, "No opening virtual CU");
			return -EINVAL;
		}
		--client->virt_cu_ref;
	} else {
		if (!ctrl)
			return -ENODEV;
		info->flags &= ~CU_CTX_OP_MASK;
		info->flags |= CU_CTX_OP_DEL;
		if (ctrl->control_ctx(ctrl, client, info))
			return -EINVAL;
	}

	--client->num_ctx;
	kds_info(client, "Client pid(%d) del context CU(0x%x)",
		 pid_nr(client->pid), cu_idx);
	return 0;
}

u32 kds_live_clients(struct kds_sched *kds, pid_t **plist)
{
	const struct list_head *ptr;
	struct kds_client *client;
	pid_t *pl = NULL;
	u32 count = 0;
	u32 i = 0;

	mutex_lock(&kds->lock);
	/* Find out number of active client */
	list_for_each(ptr, &kds->clients) {
		client = list_entry(ptr, struct kds_client, link);
		if (client->num_ctx > 0)
			count++;
	}
	if (count == 0 || plist == NULL)
		goto out;

	/* Collect list of PIDs of active client */
	pl = (pid_t *)vmalloc(sizeof(pid_t) * count);
	if (pl == NULL)
		goto out;

	list_for_each(ptr, &kds->clients) {
		client = list_entry(ptr, struct kds_client, link);
		if (client->num_ctx > 0) {
			pl[i] = pid_nr(client->pid);
			i++;
		}
	}

out:
	mutex_unlock(&kds->lock);
	return count;
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

/* User space execbuf command related functions below */
void cfg_ecmd2xcmd(struct ert_configure_cmd *ecmd,
		   struct kds_command *xcmd)
{
	int i;

	xcmd->type = KDS_CU;
	xcmd->opcode = OP_CONFIG_CTRL;

	xcmd->cb.notify_host = notify_execbuf;
	xcmd->execbuf = (u32 *)ecmd;

	xcmd->isize = ecmd->num_cus * sizeof(u32);
	/* Remove encoding at the low bits
	 * The same information is stored in CU already.
	 */
	for (i = 0; i < ecmd->num_cus; i++)
		ecmd->data[i] &= ~0x000000FF;

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

/**
 * cu_mask_to_cu_idx - Convert CU mask to CU index list
 *
 * @xcmd: Command
 * @cus:  CU index list
 *
 * Returns: Number of CUs
 *
 */
inline int
cu_mask_to_cu_idx(struct kds_command *xcmd, uint8_t *cus)
{
	int num_cu = 0;
	u32 mask;
	/* i for iterate masks, j for iterate bits */
	int i, j;

	for (i = 0; i < xcmd->num_mask; ++i) {
		if (xcmd->cu_mask[i] == 0)
			continue;

		mask = xcmd->cu_mask[i];
		for (j = 0; mask > 0; ++j) {
			if (!(mask & 0x1)) {
				mask >>= 1;
				continue;
			}

			cus[num_cu] = i * 32 + j;
			num_cu++;
			mask >>= 1;
		}
	}

	return num_cu;
}
