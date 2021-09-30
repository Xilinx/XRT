/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020-2021 Xilinx, Inc. All rights reserved.
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

int kds_mode = 1;
module_param(kds_mode, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(kds_mode,
		 "enable new KDS (0 = disable (default), 1 = enable)");

int kds_echo = 0;

static void zocl_kds_dma_complete(void *arg, int ret)
{
	struct kds_command *xcmd = (struct kds_command *)arg;
	zocl_dma_handle_t *dma_handle = (zocl_dma_handle_t *)xcmd->priv;

	xcmd->status = KDS_COMPLETED;
	if (ret)
		xcmd->status = KDS_ERROR;
	xcmd->cb.notify_host(xcmd, xcmd->status);
	xcmd->cb.free(xcmd);

	kfree(dma_handle);
}

static int copybo_ecmd2xcmd(struct drm_zocl_dev *zdev, struct drm_file *filp,
			    struct ert_start_copybo_cmd *ecmd,
			    struct kds_command *xcmd)
{
	struct drm_device *dev = zdev->ddev;
	zocl_dma_handle_t *dma_handle;
	struct drm_zocl_copy_bo args = {
		.dst_handle = ecmd->dst_bo_hdl,
		.src_handle = ecmd->src_bo_hdl,
		.size = ert_copybo_size(ecmd),
		.dst_offset = ert_copybo_dst_offset(ecmd),
		.src_offset = ert_copybo_src_offset(ecmd),
	};
	int ret = 0;

	dma_handle = kmalloc(sizeof(zocl_dma_handle_t), GFP_KERNEL);
	if (!dma_handle)
		return -ENOMEM;

	memset(dma_handle, 0, sizeof(zocl_dma_handle_t));

	ret = zocl_dma_channel_instance(dma_handle, zdev);
	if (ret)
		return ret;

	/* We must set up callback for async dma operations. */
	dma_handle->dma_func = zocl_kds_dma_complete;
	dma_handle->dma_arg = xcmd;
	xcmd->priv = dma_handle;

	ret = zocl_copy_bo_async(dev, filp, dma_handle, &args);
	return ret;
}

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

static int
zocl_add_graph_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		struct drm_zocl_ctx *args)
{
	void *uuid_ptr = (void *)(uintptr_t)args->uuid_ptr;
	xuid_t *xclbin_id;
	uuid_t *ctx_id;
	u32 gid = args->graph_id;
	u32 flags = args->flags;
	int ret;

	ctx_id = vmalloc(sizeof(uuid_t));
	if (!ctx_id)
		return -ENOMEM;

	ret = copy_from_user(ctx_id, uuid_ptr, sizeof(uuid_t));
	if (ret)
		goto out;

	mutex_lock(&zdev->zdev_xclbin_lock);
	xclbin_id = (xuid_t *)zocl_xclbin_get_uuid(zdev);
	mutex_unlock(&zdev->zdev_xclbin_lock);

	mutex_lock(&client->lock);
	if (!uuid_equal(ctx_id, xclbin_id)) {
		DRM_ERROR("try to allocate Graph CTX with wrong xclbin %pUB",
		    ctx_id);
		ret = -EINVAL;
		goto out;
	}

	ret = zocl_aie_kds_add_graph_context(zdev, gid, flags, client);
out:
	mutex_unlock(&client->lock);
	vfree(ctx_id);
	return ret;
}

static int
zocl_del_graph_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		struct drm_zocl_ctx *args)
{
	u32 gid = args->graph_id;
	int ret;

	mutex_lock(&client->lock);
	ret = zocl_aie_kds_del_graph_context(zdev, gid, client);
	mutex_unlock(&client->lock);

	return 0;
}

static int
zocl_add_aie_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		struct drm_zocl_ctx *args)
{
	u32 flags = args->flags;

	return zocl_aie_kds_add_context(zdev, flags, client);
}

static int
zocl_del_aie_context(struct drm_zocl_dev *zdev, struct kds_client *client,
		struct drm_zocl_ctx *args)
{
	return zocl_aie_kds_del_context(zdev, client);
}

static int
zocl_open_ucu(struct drm_zocl_dev *zdev, struct kds_client *client,
	      struct drm_zocl_ctx *args)
{
	struct kds_sched  *kds;
	u32 cu_idx = args->cu_index;

	kds = &zdev->kds;
	return kds_open_ucu(kds, client, cu_idx);
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
	case ZOCL_CTX_OP_ALLOC_GRAPH_CTX:
		ret = zocl_add_graph_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_FREE_GRAPH_CTX:
		ret = zocl_del_graph_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_ALLOC_AIE_CTX:
		ret = zocl_add_aie_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_FREE_AIE_CTX:
		ret = zocl_del_aie_context(zdev, client, args);
		break;
	case ZOCL_CTX_OP_OPEN_GCU_FD:
		ret = zocl_open_ucu(zdev, client, args);
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

	if (xcmd->timestamp_enabled) {
		/* Only start kernel command supports timestamps */
		struct ert_start_kernel_cmd *scmd;
		struct cu_cmd_state_timestamps *ts;

		scmd = (struct ert_start_kernel_cmd *)ecmd;
		ts = ert_start_kernel_timestamps(scmd);
		ts->skc_timestamps[ERT_CMD_STATE_NEW] = xcmd->timestamp[KDS_NEW];
		ts->skc_timestamps[ERT_CMD_STATE_QUEUED] = xcmd->timestamp[KDS_QUEUED];
		ts->skc_timestamps[ERT_CMD_STATE_RUNNING] = xcmd->timestamp[KDS_RUNNING];
		ts->skc_timestamps[ecmd->state] = xcmd->timestamp[status];
	}

	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(xcmd->gem_obj);

	if (xcmd->cu_idx >= 0)
		client_stat_inc(client, c_cnt[xcmd->cu_idx]);

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
		DRM_ERROR("Command buffer is not exec buf\n");
		return -EINVAL;
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
	xcmd->cb.notify_host = notify_execbuf;
	xcmd->execbuf = (u32 *)ecmd;
	xcmd->gem_obj = gem_obj;

	//print_ecmd_info(ecmd);

	switch(ecmd->opcode) {
	case ERT_CONFIGURE:
		xcmd->status = KDS_COMPLETED;
		xcmd->cb.notify_host(xcmd, xcmd->status);
		goto out1;
	case ERT_START_CU:
		start_krnl_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_EXEC_WRITE:
		DRM_WARN_ONCE("ERT_EXEC_WRITE is obsoleted, use ERT_START_KEY_VAL\n");
#if KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE
		__attribute__ ((fallthrough));
#else
		__attribute__ ((__fallthrough__));
#endif
		/* pass through */
	case ERT_START_KEY_VAL:
		start_krnl_kv_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_START_FA:
		start_fa_ecmd2xcmd(to_start_krnl_pkg(ecmd), xcmd);
		break;
	case ERT_START_COPYBO:
		ret = copybo_ecmd2xcmd(zdev, filp, to_copybo_pkg(ecmd), xcmd);
		if (ret)
			goto out1;
		goto out;
	default:
		DRM_ERROR("Unsupport command\n");
		ret = -EINVAL;
		goto out1;
	}

	/* Now, we could forget execbuf */
	ret = kds_add_command(&zdev->kds, xcmd);
	return ret;

out1:
	xcmd->cb.free(xcmd);
out:
	/* Don't forget to put gem object if error happen */
	if (ret < 0) {
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
	}
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
	INIT_LIST_HEAD(&client->graph_list);
	spin_lock_init(&client->graph_list_lock);
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
	zocl_aie_kds_del_graph_context_all(client);
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

	bo = zdev->kds.cmdmem.bo;
	if (bo)
		zocl_drm_free_bo(bo);
	zdev->kds.cmdmem.bo = NULL;

	kds_fini_sched(&zdev->kds);
}

static void zocl_detect_fa_cmdmem(struct drm_zocl_dev *zdev)
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
	if (!ip_layout)
		return;

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

	zdev->kds.cmdmem.bo = bo;
	zdev->kds.cmdmem.bar_paddr = bar_paddr;
	zdev->kds.cmdmem.dev_paddr = base_addr;
	zdev->kds.cmdmem.vaddr = vaddr;
	zdev->kds.cmdmem.size = size;
}

int zocl_kds_update(struct drm_zocl_dev *zdev, struct drm_zocl_kds *cfg)
{
	struct drm_zocl_bo *bo = NULL;
	int i;

	if (zdev->kds.cmdmem.bo) {
		bo = zdev->kds.cmdmem.bo;
		zocl_drm_free_bo(bo);
		zdev->kds.cmdmem.bo = NULL;
		zdev->kds.cmdmem.bar_paddr = 0;
		zdev->kds.cmdmem.dev_paddr = 0;
		zdev->kds.cmdmem.vaddr = 0;
		zdev->kds.cmdmem.size = 0;
	}

	zocl_detect_fa_cmdmem(zdev);
	
	// Default supporting interrupt mode
	zdev->kds.cu_intr_cap = 1;	

	for (i = 0; i < zdev->kds.cu_mgmt.num_cus; i++) {
		struct xrt_cu *xcu;
		int apt_idx;

		xcu = zdev->kds.cu_mgmt.xcus[i];
		apt_idx = get_apt_index_by_addr(zdev, xcu->info.addr); 
		if (apt_idx < 0) {
			DRM_ERROR("CU address %llx is not found in XCLBIN\n",
			    xcu->info.addr);
			return apt_idx;
		}
		update_cu_idx_in_apt(zdev, apt_idx, i);
	}

	// Check for polling mode and enable CU interrupt if polling_mode is false
	if (cfg->polling)
		zdev->kds.cu_intr = 0;
	else
		zdev->kds.cu_intr = 1;


	return kds_cfg_update(&zdev->kds);
}
