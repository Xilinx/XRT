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

int kds_echo = 0;
module_param(kds_echo, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(kds_echo,
		 "enable KDS echo (0 = disable (default), 1 = enable)");

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
	xcmd->cb.free = kds_free_command;

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

