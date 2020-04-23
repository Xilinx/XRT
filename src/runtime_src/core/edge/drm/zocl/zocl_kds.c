/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/sched/signal.h>
#include "zocl_drv.h"
#include "zocl_util.h"
#include "kds_core.h"

int kds_mode = 0;
module_param(kds_mode, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(kds_mode,
		 "enable new KDS (0 = disable (default), 1 = enable)");

static void notify_execbuf(struct kds_command *xcmd, int status)
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

static inline void cfg_ecmd2xcmd(struct ert_configure_cmd *ecmd,
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

static inline void start_krnl_ecmd2xcmd(struct ert_start_kernel_cmd *ecmd,
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

int zocl_command_ioctl(struct drm_zocl_dev *zdev, void *data,
		       struct drm_file *filp)
{
	struct drm_gem_object *gem_obj;
	struct drm_device *dev = zdev->ddev;
	struct drm_zocl_execbuf *args = data;
	struct kds_client *client = filp->driver_priv;
	struct drm_zocl_bo *zocl_bo;
	struct ert_packet *ecmd;
	struct kds_command *xcmd;
	int ret = 0;

	gem_obj = zocl_gem_object_lookup(dev, filp, args->exec_bo_handle);
	if (!gem_obj) {
		DRM_ERROR("Look up GEM BO %d failed\n", args->exec_bo_handle);
		return -EINVAL;
	}

	zocl_bo = to_zocl_bo(gem_obj);
	if (!zocl_bo_execbuf(zocl_bo)) {
		ret = -EINVAL;
		goto out;
	}

	ecmd = (struct ert_packet *)zocl_bo->cma_base.vaddr;

	/* only the user command knows the real size of the payload.
	 * count is more than enough!
	 */
	xcmd = kds_alloc_command(client, ecmd->count * sizeof(u32));
	if (!xcmd) {
		DRM_INFO("Failed to alloc xcmd\n");
		ret = -ENOMEM;
		goto out;
	}

	/* TODO: one ecmd to one xcmd now. Maybe we will need
	 * one ecmd to multiple xcmds
	 */
	if (ecmd->opcode == ERT_CONFIGURE)
		cfg_ecmd2xcmd(to_cfg_pkg(ecmd), xcmd);
	else if (ecmd->opcode == ERT_START_CU)
		start_krnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);

	/* Now, we could forget execbuf */
	ret = kds_add_command(xcmd);
	if (ret)
		kds_free_command(xcmd);

out:
	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&zocl_bo->cma_base.base);
	return ret;
}

uint zocl_poll_client(struct file *filp, poll_table *wait)
{
	struct drm_file *priv = filp->private_data;
	struct kds_client *client = (struct kds_client *)priv->driver_priv;
	int event;

	poll_wait(filp, &client->waitq, wait);

	event = atomic_dec_if_positive(&client->event);
	if (event == -1)
		return 0;

	/* If only return POLLIN, I could get 100K IOPS more.
	 * With above wait, the IOPS is more unstable (+/-100K).
	 */
	return POLLIN;
}

int zocl_create_client(struct drm_zocl_dev *zdev, void **priv)
{
	struct kds_client *client;
	struct drm_device *ddev;
	int ret = 0;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	ddev = zdev->ddev;

	client->dev  = ddev->dev;
	client->pid  = get_pid(task_pid(current));
	client->ctrl = zdev->kds.ctrl;
	ret = kds_init_client(client);
	if (ret) {
		kfree(client);
		goto out;
	}
	list_add_tail(&client->link, &zdev->ctx_list);
	*priv = client;

out:
	zocl_info(ddev->dev, "created KDS client for pid(%d), ret: %d\n",
		  pid_nr(task_tgid(current)), ret);

	return ret;
}

void zocl_destroy_client(struct drm_zocl_dev *zdev, void **priv)
{
	struct kds_client *client = *priv;
	struct drm_device *ddev;
	int pid = pid_nr(client->pid);

	ddev = zdev->ddev;

	list_del(&client->link);
	kds_fini_client(client);
	kfree(client);
	zocl_info(ddev->dev, "client exits pid(%d)\n", pid);
}

