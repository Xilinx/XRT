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
#include "zocl_xclbin.h"
#include "kds_core.h"
#include "xclbin.h"

#define print_ecmd_info(ecmd) \
do {\
	int i;\
	printk("%s: ecmd header 0x%x\n", __func__, ecmd->header);\
	for (i = 0; i < ecmd->count; i++) {\
		printk("%s: ecmd data[%d] 0x%x\n", __func__, i, ecmd->data[i]);\
	}\
} while(0)

int kds_mode = 0;
module_param(kds_mode, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(kds_mode,
		 "enable new KDS (0 = disable (default), 1 = enable)");

int kds_echo = 0;

static inline void
zocl_ctx_to_info(struct drm_zocl_ctx *args, struct kds_ctx_info *info)
{
	if (args->cu_index == ZOCL_CTX_VIRT_CU_INDEX)
		info->cu_idx = CU_CTX_VIRT_CU;
	else
		info->cu_idx = args->cu_index;

	/* Ignore ZOCL_CTX_SHARED bit if ZOCL_CTX_EXCLUSIVE bit is set */
	if (args->flags & ZOCL_CTX_EXCLUSIVE)
		info->flags = CU_CTX_EXCLUSIVE;
	else
		info->flags = CU_CTX_SHARED;
}

static int
zocl_add_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		 struct drm_zocl_ctx *args)
{
	struct kds_ctx_info info;
	void *uuid_ptr = (void *)(uintptr_t)args->uuid_ptr;
	uuid_t *id;
	int ret;

	id = vmalloc(sizeof(uuid_t));
	if (!id)
		return -ENOMEM;

	ret = copy_from_user(id, uuid_ptr, sizeof(uuid_t));
	if (ret) {
		vfree(id);
		return ret;
	}

	mutex_lock(&client->lock);
	if (!client->num_ctx) {
		ret = zocl_lock_bitstream(zdev, id);
		if (ret)
			goto out;
		client->xclbin_id = vzalloc(sizeof(*id));
		if (!client->xclbin_id) {
			ret = -ENOMEM;
			goto out1;
		}
		uuid_copy(client->xclbin_id, id);
	}

	/* Bitstream is locked. No one could load a new one
	 * until this client close all of the contexts.
	 */
	zocl_ctx_to_info(args, &info);
	ret = kds_add_context(&zdev->kds, client, &info);

out1:
	if (!client->num_ctx) {
		vfree(client->xclbin_id);
		client->xclbin_id = NULL;
		(void) zocl_unlock_bitstream(zdev, id);
	}
out:
	mutex_unlock(&client->lock);
	vfree(id);
	return ret;
}

static int
zocl_del_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		 struct drm_zocl_ctx *args)
{
	struct kds_ctx_info info;
	void *uuid_ptr = (void *)(uintptr_t)args->uuid_ptr;
	uuid_t *id;
	uuid_t *uuid;
	int ret;

	id = vmalloc(sizeof(uuid_t));
	if (!id)
		return -ENOMEM;

	ret = copy_from_user(id, uuid_ptr, sizeof(uuid_t));
	if (ret) {
		vfree(id);
		return ret;
	}

	mutex_lock(&client->lock);
	uuid = client->xclbin_id;
	/* xclCloseContext() would send xclbin_id and cu_idx.
	 * Be more cautious while delete. Do sanity check
	 */
	if (!uuid) {
		DRM_ERROR("No context was opened");
		ret = -EINVAL;
		goto out;
	}

	/* If xclbin id looks good, unlock bitstream should not fail. */
	if (!uuid_equal(uuid, id)) {
		DRM_ERROR("Try to delete CTX on wrong xclbin");
		ret = -EBUSY;
		goto out;
	}

	zocl_ctx_to_info(args, &info);
	ret = kds_del_context(&zdev->kds, client, &info);
	if (ret)
		goto out;

	if (!client->num_ctx) {
		vfree(client->xclbin_id);
		client->xclbin_id = NULL;
		(void) zocl_unlock_bitstream(zdev, id);
	}

out:
	mutex_unlock(&client->lock);
	vfree(id);
	return ret;
}

int zocl_context_ioctl(struct drm_zocl_dev *zdev, void *data,
		       struct drm_file *filp)
{
	struct drm_zocl_ctx *args = data;
	struct kds_client *client = filp->driver_priv;
	int ret = 0;

	switch (args->op) {
	case ZOCL_CTX_OP_ALLOC_CTX:
		ret = zocl_add_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_FREE_CTX:
		ret = zocl_del_context(zdev, client, args);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void notify_execbuf(struct kds_command *xcmd, int status)
{
	struct kds_client *client = xcmd->client;
	struct ert_packet *ecmd = (struct ert_packet *)xcmd->execbuf;

	if (status == KDS_COMPLETED)
		ecmd->state = ERT_CMD_STATE_COMPLETED;
	else if (status == KDS_ERROR)
		ecmd->state = ERT_CMD_STATE_ERROR;
	else if (status == KDS_TIMEOUT)
		ecmd->state = ERT_CMD_STATE_TIMEOUT;
	else if (status == KDS_ABORT)
		ecmd->state = ERT_CMD_STATE_ABORT;

	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(xcmd->gem_obj);

	if (xcmd->cu_idx >= 0)
		client->c_cnt[xcmd->cu_idx]++;

	atomic_inc(&client->event);
	wake_up_interruptible(&client->waitq);
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

	if (!client->xclbin_id) {
		DRM_ERROR("The client has no opening context\n");
		return -EINVAL;
	}

	if (zdev->kds.bad_state) {
		DRM_ERROR("KDS is in bad state\n");
		return -EDEADLK;
	}

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

	ecmd->state = ERT_CMD_STATE_NEW;
	/* only the user command knows the real size of the payload.
	 * count is more than enough!
	 */
	xcmd = kds_alloc_command(client, ecmd->count * sizeof(u32));
	if (!xcmd) {
		DRM_ERROR("Failed to alloc xcmd\n");
		ret = -ENOMEM;
		goto out;
	}
	xcmd->cb.free = kds_free_command;

	//print_ecmd_info(ecmd);

	/* TODO: one ecmd to one xcmd now. Maybe we will need
	 * one ecmd to multiple xcmds
	 */
	if (ecmd->opcode == ERT_CONFIGURE)
		cfg_ecmd2xcmd(to_cfg_pkg(ecmd), xcmd);
	else if (ecmd->opcode == ERT_START_CU)
		start_krnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
	else if (ecmd->opcode == ERT_START_FA)
		start_fa_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
	xcmd->cb.notify_host = notify_execbuf;
	xcmd->gem_obj = gem_obj;

	/* Now, we could forget execbuf */
	ret = kds_add_command(&zdev->kds, xcmd);

out:
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
	struct kds_sched  *kds;
	struct drm_device *ddev;
	int ret = 0;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	ddev = zdev->ddev;

	kds = &zdev->kds;
	client->dev = ddev->dev;
	ret = kds_init_client(kds, client);
	if (ret) {
		kfree(client);
		goto out;
	}
	*priv = client;

out:
	zocl_info(ddev->dev, "created KDS client for pid(%d), ret: %d\n",
		  pid_nr(task_tgid(current)), ret);

	return ret;
}

void zocl_destroy_client(struct drm_zocl_dev *zdev, void **priv)
{
	struct kds_client *client = *priv;
	struct kds_sched  *kds;
	struct drm_device *ddev;
	int pid = pid_nr(client->pid);

	ddev = zdev->ddev;

	kds = &zdev->kds;
	/* kds_fini_client should released resources hold by the client.
	 * release xclbin_id and unlock bitstream if needed.
	 */
	kds_fini_client(kds, client);
	if (client->xclbin_id) {
		(void) zocl_unlock_bitstream(zdev, client->xclbin_id);
		vfree(client->xclbin_id);
	}

	/* Make sure all resources of the client are released */
	kfree(client);
	zocl_info(ddev->dev, "client exits pid(%d)\n", pid);
}

int zocl_init_sched(struct drm_zocl_dev *zdev)
{
	return kds_init_sched(&zdev->kds);
}

void zocl_fini_sched(struct drm_zocl_dev *zdev)
{
	struct drm_zocl_bo *bo = NULL;

	bo = zdev->kds.plram.bo;
	if (bo)
		zocl_drm_free_bo(bo);
	zdev->kds.plram.bo = NULL;

	kds_fini_sched(&zdev->kds);
}

static void zocl_detect_fa_plram(struct drm_zocl_dev *zdev)
{
	struct ip_layout    *ip_layout = NULL;
	struct drm_zocl_bo *bo = NULL;
	struct drm_zocl_create_bo args;
	int i;
	uint64_t size;
	uint64_t base_addr;
	void __iomem *vaddr;
	ulong bar_paddr = 0;

	/* Detect Fast adapter */
	ip_layout = zdev->ip;

	for (i = 0; i < ip_layout->m_count; ++i) {
		struct ip_data *ip = &ip_layout->m_ip_data[i];
		u32 prot;

		if (ip->m_type != IP_KERNEL)
			continue;

		prot = (ip->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT;
		if (prot != FAST_ADAPTER)
			continue;

		break;
	}

	if (i == ip_layout->m_count)
		return;

	/* TODO: logic to dynamicly select size */
	size = 4096;

	args.size = size;
	args.flags = ZOCL_BO_FLAGS_CMA;
	bo = zocl_drm_create_bo(zdev->ddev, size, args.flags);
	if (IS_ERR(bo))
		return;

	
	bar_paddr = (uint64_t)bo->cma_base.paddr;	
	base_addr = (uint64_t)bo->cma_base.paddr;	
	vaddr = bo->cma_base.vaddr;	

	zdev->kds.plram.bo = bo;
	zdev->kds.plram.bar_paddr = bar_paddr;
	zdev->kds.plram.dev_paddr = base_addr;
	zdev->kds.plram.vaddr = vaddr;
	zdev->kds.plram.size = size;
}

int zocl_kds_update(struct drm_zocl_dev *zdev)
{
	struct drm_zocl_bo *bo = NULL;

	if (zdev->kds.plram.bo) {
		bo = zdev->kds.plram.bo;
		zocl_drm_free_bo(bo);
		zdev->kds.plram.bo = NULL;
		zdev->kds.plram.bar_paddr = 0;
		zdev->kds.plram.dev_paddr = 0;
		zdev->kds.plram.vaddr = 0;
		zdev->kds.plram.size = 0;
	}

	zocl_detect_fa_plram(zdev);
	zdev->kds.cu_intr = 0;
	return kds_cfg_update(&zdev->kds);
}
