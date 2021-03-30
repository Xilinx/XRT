/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2020-2021 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinc.com>
 *    Himanshu Choudhary <hchoudha@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "zocl_aie.h"
#include "xrt_xclbin.h"
#include "xclbin.h"
#include "sched_exec.h"

#ifndef __NONE_PETALINUX__
#include <linux/xlnx-ai-engine.h>
#endif

#include <linux/workqueue.h>
#include <linux/delay.h>

static inline u8
get_error_module(enum aie_module_type aie_module) {
	switch (aie_module) {
	case AIE_MEM_MOD:
		return XRT_ERROR_MODULE_AIE_MEMORY;
	case AIE_CORE_MOD:
		return XRT_ERROR_MODULE_AIE_CORE;
	case AIE_PL_MOD:
		return XRT_ERROR_MODULE_AIE_PL;
	case AIE_NOC_MOD:
		return XRT_ERROR_MODULE_AIE_NOC;
	default:
		return XRT_ERROR_MODULE_UNKNOWN;
	}
}

static inline u8
get_error_num(u8 aie_category) {
	switch (aie_category) {
	case AIE_ERROR_CATEGORY_SATURATION:
		return XRT_ERROR_NUM_AIE_SATURATION;
	case AIE_ERROR_CATEGORY_FP:
		return XRT_ERROR_NUM_AIE_FP;
	case AIE_ERROR_CATEGORY_STREAM:
		return XRT_ERROR_NUM_AIE_STREAM;
	case AIE_ERROR_CATEGORY_ACCESS:
		return XRT_ERROR_NUM_AIE_ACCESS;
	case AIE_ERROR_CATEGORY_BUS:
		return XRT_ERROR_NUM_AIE_BUS;
	case AIE_ERROR_CATEGORY_INSTRUCTION:
		return XRT_ERROR_NUM_AIE_INSTRUCTION;
	case AIE_ERROR_CATEGORY_ECC:
		return XRT_ERROR_NUM_AIE_ECC;
	case AIE_ERROR_CATEGORY_LOCK:
		return XRT_ERROR_NUM_AIE_LOCK;
	case AIE_ERROR_CATEGORY_DMA:
		return XRT_ERROR_NUM_AIE_DMA;
	case AIE_ERROR_CATEGORY_MEM_PARITY:
		return XRT_ERROR_NUM_AIE_MEM_PARITY;
	default:
		return XRT_ERROR_NUM_UNKNOWN;
	}
}

static int
zocl_aie_cache_error(struct aie_error_cache *zerr, struct aie_error *err)
{
	struct aie_error *temp;

	/* If the current error cache is full, double the cache size */
	if (zerr->num == zerr->cap) {
		temp = vzalloc(sizeof (struct aie_error) * zerr->cap * 2);
		if (!temp)
			return -ENOMEM;

		memcpy(temp, zerr->errors, sizeof (struct aie_error) *
		    zerr->num);
		zerr->cap *= 2;
		vfree(zerr->errors);
		zerr->errors = temp;
	}

	zerr->errors[zerr->num].error_id = err->error_id;
	zerr->errors[zerr->num].module = err->module;
	zerr->errors[zerr->num].loc.col = err->loc.col;
	zerr->errors[zerr->num].loc.row = err->loc.row;
	zerr->num++;

	return 0;
}

static bool
is_cached_error(struct aie_error_cache *zerr, struct aie_error *err)
{
	int i;

	for (i = 0; i < zerr->num; i++) {
		if (zerr->errors[i].error_id == err->error_id &&
		    zerr->errors[i].category == err->category &&
		    zerr->errors[i].module == err->module &&
		    zerr->errors[i].loc.col == err->loc.col &&
		    zerr->errors[i].loc.row == err->loc.row)
			break;
	}

	return i < zerr->num ? true : false;
}

static void
zocl_aie_error_cb(void *arg)
{
	struct drm_zocl_dev *zdev = arg;
	struct aie_errors *errors;
	int i;

	if (!zdev) {
		DRM_WARN("%s: zdev is not initialized\n", __func__);
		return;
	}

	mutex_lock(&zdev->aie_lock);
	if (!zdev->aie) {
		DRM_WARN("%s: AIE image is not loaded.\n", __func__);
		mutex_unlock(&zdev->aie_lock);
		return;
	}

	if (!zdev->aie->aie_dev) {
		DRM_WARN("%s: No available AIE partition.\n", __func__);
		mutex_unlock(&zdev->aie_lock);
		return;
	}

	errors = aie_get_errors(zdev->aie->aie_dev);
	if (IS_ERR(errors)) {
		DRM_WARN("%s: aie_get_errors failed\n", __func__);
		mutex_unlock(&zdev->aie_lock);
		return;
	}

	for (i = 0; i < errors->num_err; i++) {
		xrtErrorCode err_code;

		DRM_INFO("Get AIE asynchronous Error: "
		    "error_id %d Mod %d, category %d, Col %d, Row %d\n",
		    errors->errors[i].error_id,
		    errors->errors[i].module,
		    errors->errors[i].category,
		    errors->errors[i].loc.col,
		    errors->errors[i].loc.row
		    );

		if (is_cached_error(&zdev->aie->err, &((errors->errors)[i])))
			continue;

		err_code = XRT_ERROR_CODE_BUILD(
		    get_error_num(errors->errors[i].category),
		    XRT_ERROR_DRIVER_AIE,
		    XRT_ERROR_SEVERITY_CRITICAL,
		    get_error_module(errors->errors[i].module),
		    XRT_ERROR_CLASS_AIE
		    );

		zocl_insert_error_record(zdev, err_code);
		zocl_aie_cache_error(&zdev->aie->err, &((errors->errors)[i]));
	}

	aie_free_errors(errors);

	mutex_unlock(&zdev->aie_lock);
}

int
zocl_aie_request_part_fd(struct drm_zocl_dev *zdev, void *data)
{
	struct drm_zocl_aie_fd *args = data;
	int fd;
	int rval = 0;

	mutex_lock(&zdev->aie_lock);

	if (!zdev->aie) {
		DRM_ERROR("AIE image is not loaded.\n");
		rval = -ENODEV;
		goto done;
	}

	if (!zdev->aie->aie_dev) {
		DRM_ERROR("No available AIE partition.\n");
		rval = -ENODEV;
		goto done;
	}

	if (zdev->aie->partition_id != args->partition_id) {
		DRM_ERROR("AIE partition %d does not exist.\n",
		    args->partition_id);
		rval = -ENODEV;
		goto done;
	}

	fd = aie_partition_get_fd(zdev->aie->aie_dev);
	if (fd < 0) {
		DRM_ERROR("Get AIE partition %d fd: %d\n",
		    args->partition_id, fd);
		rval = fd;
		goto done;
	}

	args->fd = fd;

	zdev->aie->fd_cnt++;

done:
	mutex_unlock(&zdev->aie_lock);

	return rval;
}

int
zocl_create_aie(struct drm_zocl_dev *zdev, struct axlf *axlf, void *aie_res)
{
	uint64_t offset;
	uint64_t size;
	struct aie_partition_req req;
	int rval = 0;

	rval = xrt_xclbin_section_info(axlf, AIE_METADATA, &offset, &size);
	if (rval)
		return rval;

	mutex_lock(&zdev->aie_lock);

	/* AIE is reset and no PDI is loaded after reset */
	if (zdev->aie && zdev->aie->aie_reset) {
		rval = -ENODEV;
		DRM_ERROR("PDI is not loaded after AIE reset.\n");
		goto done;
	}

	if (!zdev->aie) {
		zdev->aie = vzalloc(sizeof (struct zocl_aie));
		if (!zdev->aie) {
			rval = -ENOMEM;
			DRM_ERROR("Fail to allocate memory.\n");
			goto done;
		}

		zdev->aie->err.errors = vzalloc(sizeof (struct aie_error) *
		    ZOCL_AIE_ERROR_CACHE_CAP);
		if (!zdev->aie->err.errors) {
			rval = -ENOMEM;
			vfree(zdev->aie);
			DRM_ERROR("Fail to allocate memory.\n");
			goto done;
		}

		zdev->aie->err.num = 0;
		zdev->aie->err.cap = ZOCL_AIE_ERROR_CACHE_CAP;

	}

	if (!zdev->aie->wq) {
		zdev->aie->wq = create_singlethread_workqueue("aie-workq");
		if (!zdev->aie->wq) {
			rval = -ENOMEM;
			DRM_ERROR("Fail to create work queue.\n");
			goto done;
		}
	}

	/* TODO figure out the partition id and uid from xclbin or PDI */
	req.partition_id = 1;
	req.uid = 0;
	req.meta_data = (u64)aie_res;

	if (zdev->aie->aie_dev) {
		DRM_INFO("Partition %d already requested\n",
		    req.partition_id);
		goto done;
	}

	zdev->aie->aie_dev = aie_partition_request(&req);

	if (IS_ERR(zdev->aie->aie_dev)) {
		rval = PTR_ERR(zdev->aie->aie_dev);
		DRM_ERROR("Request AIE partition %d, %d\n",
		    req.partition_id, rval);
		goto done;
	}

	zdev->aie->partition_id = req.partition_id;
	zdev->aie->uid = req.uid;

	/* Register AIE error call back function. */
	rval = aie_register_error_notification(zdev->aie->aie_dev,
	    zocl_aie_error_cb, zdev);

	mutex_unlock(&zdev->aie_lock);

	zocl_init_aie(zdev);

	return 0;

done:
	mutex_unlock(&zdev->aie_lock);

	return rval;
}

void
zocl_destroy_aie(struct drm_zocl_dev *zdev)
{
	vfree(zdev->aie_information);
	mutex_lock(&zdev->aie_lock);

	if (!zdev->aie) {
		mutex_unlock(&zdev->aie_lock);
		return;
	}

	if (zdev->aie->aie_dev)
		aie_partition_release(zdev->aie->aie_dev);

	if (zdev->aie->wq)
		destroy_workqueue(zdev->aie->wq);

	vfree(zdev->aie->err.errors);
	vfree(zdev->aie);
	zdev->aie = NULL;
	mutex_unlock(&zdev->aie_lock);
}

static void
zocl_aie_reset_work(struct work_struct *aie_work)
{
	struct aie_work_data *data = container_of(aie_work,
	    struct aie_work_data, work);
	struct drm_zocl_dev *zdev= data->zdev;
	int i;

	/*
	 * Note: we can not hold lock here because this
	 *       work thread is invoked within a lock and
	 *       that lock will not be relased until the
	 *       work is flushed.
	 */

	/*
	 * Reset AIE by releasing AIE partition. The times we
	 * release AIE partition is based on the # of fd is
	 * requested + 1.
	 */
	for (i = 0; i < zdev->aie->fd_cnt + 1; i++)
		aie_partition_release(zdev->aie->aie_dev);

	zdev->aie->fd_cnt = 0;
}

int
zocl_aie_reset(struct drm_zocl_dev *zdev)
{
	struct aie_partition_req req;
	bool aie_available = false;
	struct aie_work_data *data;
	int count;

	mutex_lock(&zdev->aie_lock);

	if (!zdev->aie) {
		mutex_unlock(&zdev->aie_lock);
		DRM_ERROR("AIE image is not loaded.\n");
		return -ENODEV;
	}

	if (!zdev->aie->aie_dev) {
		mutex_unlock(&zdev->aie_lock);
		DRM_ERROR("No available AIE partition.\n");
		return -ENODEV;
	}

	data = kmalloc(sizeof(struct aie_work_data), GFP_KERNEL);
	if (!data) {
		mutex_unlock(&zdev->aie_lock);
		DRM_ERROR("Can not allocate memory,\n");
		return -ENOMEM;
	}
	data->zdev = zdev;

	/*
	 * Per the requrirement of current AIE driver, we
	 * need to release the partition in a separate thread.
	 */
	INIT_WORK(&data->work, zocl_aie_reset_work);
	queue_work(zdev->aie->wq, &data->work);

	/* Make sure the reset thread is done */
	flush_workqueue(zdev->aie->wq);
	kfree(data);

	req.partition_id = zdev->aie->partition_id;
	req.uid = zdev->aie->uid;

	/* Check if AIE partition is available in a given time */
	count = 0;
	while (1) {
		aie_available = aie_partition_is_available(&req);
		if (aie_available)
			break;
		count++;
		if (count == ZOCL_AIE_RESET_TIMEOUT_NUMBER) {
			mutex_unlock(&zdev->aie_lock);
			DRM_ERROR("AIE Reset fail: timeout.");
			return -ETIME;
		}
		msleep(ZOCL_AIE_RESET_TIMEOUT_INTERVAL);
	}

	zdev->aie->aie_dev = NULL;
	zdev->aie->aie_reset = true;
	zdev->aie->err.num = 0;

	mutex_unlock(&zdev->aie_lock);

	return 0;
}

int
zocl_aie_graph_alloc_context(struct drm_zocl_dev *zdev, u32 gid, u32 ctx_code,
	struct sched_client_ctx *client)
{
	struct list_head *cptr, *gptr;
	struct zocl_graph_ctx_node *gnode;
	struct sched_client_ctx *ctx;
	unsigned long ctx_flags, graph_flags;
	int ret;

	spin_lock_irqsave(&zdev->exec->ctx_list_lock, ctx_flags);

	list_for_each(cptr, &zdev->exec->ctx_list) {
		ctx = list_entry(cptr, struct sched_client_ctx, link);

		spin_lock_irqsave(&ctx->graph_list_lock, graph_flags);

		list_for_each(gptr, &ctx->graph_list) {
			gnode = list_entry(gptr, struct zocl_graph_ctx_node,
			    link);

			if (gnode->gid != gid)
				continue;

			if (ctx == client) {
				/*
				 * This graph has been opened by same
				 * context.
				 */
				DRM_ERROR("Graph %d has been opened.\n", gid);
				ret = -EINVAL;
				goto out;
			}

			if (gnode->ctx_code == ZOCL_CTX_EXCLUSIVE ||
			    ctx_code == ZOCL_CTX_EXCLUSIVE) {
				/*
				 * This graph has been opened with
				 * exclusive context or;
				 * We request exclusive context but
				 * this graph has been opened with
				 * non-exclusive context
				 */
				DRM_ERROR("Graph %d only one exclusive context can be opened.\n",
				    gid);
				ret = -EBUSY;
				goto out;
			}

			if (gnode->ctx_code == ZOCL_CTX_PRIMARY &&
			    ctx_code != ZOCL_CTX_SHARED) {
				/*
				 * This graph has been opened with
				 * primary context but the request
				 * is not shared context
				 */
				DRM_ERROR("Graph %d has been opened with primary context.\n",
				    gid);
				ret = -EBUSY;
				goto out;
			}
		}
		spin_unlock_irqrestore(&ctx->graph_list_lock, graph_flags);
	}

	gnode = kzalloc(sizeof(*gnode), GFP_KERNEL);
	gnode->ctx_code = ctx_code;
	gnode->gid = gid;
	list_add_tail(&gnode->link, &client->graph_list);

	spin_unlock_irqrestore(&zdev->exec->ctx_list_lock, ctx_flags);
	return 0;

out:
	spin_unlock_irqrestore(&ctx->graph_list_lock, graph_flags);
	spin_unlock_irqrestore(&zdev->exec->ctx_list_lock, ctx_flags);

	return ret;
}

int
zocl_aie_graph_free_context(struct drm_zocl_dev *zdev, u32 gid,
	struct sched_client_ctx *client)
{
	struct list_head *gptr, *next;
	struct zocl_graph_ctx_node *gnode;
	unsigned long flags = 0;
	int ret;

	spin_lock_irqsave(&client->graph_list_lock, flags);

	list_for_each_safe(gptr, next, &client->graph_list) {
		gnode = list_entry(gptr, struct zocl_graph_ctx_node, link);

		if (gnode->gid == gid) {
			list_del(gptr);
			kfree(gnode);
			ret = 0;
			goto out;
		}
	}

	DRM_ERROR("Fail to close graph context: Graph %d does not exist.\n",
	    gid);
	ret = -EINVAL;

out:
	spin_unlock_irqrestore(&client->graph_list_lock, flags);
	return ret;
}

void
zocl_aie_graph_free_context_all(struct drm_zocl_dev *zdev,
	struct sched_client_ctx *client)
{
	struct list_head *gptr, *next;
	struct zocl_graph_ctx_node *gnode;
	unsigned long flags = 0;

	spin_lock_irqsave(&client->graph_list_lock, flags);

	list_for_each_safe(gptr, next, &client->graph_list) {
		gnode = list_entry(gptr, struct zocl_graph_ctx_node, link);

		list_del(gptr);
		kfree(gnode);
	}

	spin_unlock_irqrestore(&client->graph_list_lock, flags);
}

int
zocl_aie_alloc_context(struct drm_zocl_dev *zdev, u32 ctx_code,
	struct sched_client_ctx *client)
{
	struct list_head *cptr;
	struct sched_client_ctx *ctx;
	unsigned long ctx_flags;
	int ret;

	spin_lock_irqsave(&zdev->exec->ctx_list_lock, ctx_flags);

	if (client->aie_ctx != ZOCL_CTX_NOOPS) {
		DRM_ERROR("Changing AIE context is not supported.\n");
		ret = -EBUSY;
		goto out;
	}

	list_for_each(cptr, &zdev->exec->ctx_list) {
		ctx = list_entry(cptr, struct sched_client_ctx, link);

		if (ctx == client || ctx->aie_ctx == ZOCL_CTX_NOOPS)
			continue;

		if (ctx->aie_ctx == ZOCL_CTX_EXCLUSIVE ||
		    ctx_code == ZOCL_CTX_EXCLUSIVE) {
			/*
			 * This AIE array has been allocated exclusive
			 * context or;
			 * We request exclusive context but this AIE array
			 * has been allocated with non-exclusive context
			 */
			DRM_ERROR("Only one exclusive AIE context can be allocated.\n");
			ret = -EBUSY;
			goto out;
		}

		if (ctx->aie_ctx == ZOCL_CTX_PRIMARY &&
		    ctx_code != ZOCL_CTX_SHARED) {
			/*
			 * This AIE array has been allocated primary context
			 * but the request is not shared context
			 */
			DRM_ERROR("Primary AIE context has been allocated.\n");
			ret = -EBUSY;
			goto out;
		}
	}

	client->aie_ctx = ctx_code;

	spin_unlock_irqrestore(&zdev->exec->ctx_list_lock, ctx_flags);
	return 0;

out:
	spin_unlock_irqrestore(&zdev->exec->ctx_list_lock, ctx_flags);
	return ret;
}

int
zocl_aie_free_context(struct drm_zocl_dev *zdev,
	struct sched_client_ctx *client)
{
	unsigned long ctx_flags;

	spin_lock_irqsave(&zdev->exec->ctx_list_lock, ctx_flags);

	if (client->aie_ctx == ZOCL_CTX_NOOPS) {
		DRM_ERROR("No AIE context has been allocated.\n");
		spin_unlock_irqrestore(&zdev->exec->ctx_list_lock, ctx_flags);
		return -EINVAL;
	}

	client->aie_ctx = ZOCL_CTX_NOOPS;

	spin_unlock_irqrestore(&zdev->exec->ctx_list_lock, ctx_flags);

	return 0;
}

int
zocl_aie_getcmd_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	int ret;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct aie_info *aie = zdev->aie_information;
	struct aie_info_cmd *acmd;
	struct drm_zocl_aie_cmd *kdata = data;

	if(aie == NULL)
		return -EAGAIN;

	/* If no command, the process who calls this ioctl will block here */
	mutex_lock(&aie->aie_lock);
	while (list_empty(&aie->aie_cmd_list)) {
		mutex_unlock(&aie->aie_lock);
		/*
 		 * Return greater than 0 if condition true before timeout,
 		 * 0 when time out, else -ERESTARTSYS.
 		 */
		ret = wait_event_interruptible_timeout(aie->aie_wait_queue,
		    !list_empty(&aie->aie_cmd_list), msecs_to_jiffies(500));
		if (ret <= 0) {
			return -ERESTARTSYS;
                }
		mutex_lock(&aie->aie_lock);
	}

	acmd = list_first_entry(&aie->aie_cmd_list, struct aie_info_cmd,
	    aiec_list);
	list_del(&acmd->aiec_list);

	/* Only one aied thread */
	aie->cmd_inprogress = acmd;
	mutex_unlock(&aie->aie_lock);

	kdata->opcode = acmd->aiec_packet->opcode;

	return 0;
}
int
zocl_aie_putcmd_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct aie_info *aie = zdev->aie_information;
	struct aie_info_cmd *acmd;
	struct drm_zocl_aie_cmd *kdata = data;
	struct aie_info_packet *cmd;

	if(aie == NULL)
		return -EAGAIN;

	mutex_lock(&aie->aie_lock);
	acmd = aie->cmd_inprogress;
	mutex_unlock(&aie->aie_lock);
	if (!acmd) {
		return -ENOMEM;
	}

	cmd = acmd->aiec_packet;
	cmd->size = (kdata->size < AIE_INFO_SIZE) ? kdata->size : AIE_INFO_SIZE;
	snprintf(cmd->info, cmd->size, "%s", (char *)kdata->info);

	up(&acmd->aiec_sem);

	return 0;
}

int
zocl_init_aie(struct drm_zocl_dev *zdev)
{
	struct aie_info *aie;
	aie = vzalloc(sizeof(struct aie_info));
	if (!aie) {
		zdev->aie_information = NULL;
		return -ENOMEM;
	}

	mutex_init(&aie->aie_lock);
	INIT_LIST_HEAD(&aie->aie_cmd_list);
	init_waitqueue_head(&aie->aie_wait_queue);
	aie->cmd_inprogress = NULL;
	zdev->aie_information = aie;

	return 0;
}
