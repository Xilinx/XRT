/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2020-2022 Xilinx, Inc. All rights reserved.
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
	struct drm_zocl_slot *slot = arg;
	struct aie_errors *errors;
	int i;

	if (!slot) {
		DRM_ERROR("%s: slot is not initialized\n", __func__);
		return;
	}

	mutex_lock(&slot->aie_lock);
	if (!slot->aie) {
		DRM_ERROR("%s: AIE image is not loaded.\n", __func__);
		mutex_unlock(&slot->aie_lock);
		return;
	}

	if (!slot->aie->aie_dev) {
		DRM_ERROR("%s: No available AIE partition.\n", __func__);
		mutex_unlock(&slot->aie_lock);
		return;
	}

	errors = aie_get_errors(slot->aie->aie_dev);
	if (IS_ERR(errors)) {
		DRM_ERROR("%s: aie_get_errors failed\n", __func__);
		mutex_unlock(&slot->aie_lock);
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

		if (is_cached_error(&slot->aie->err, &((errors->errors)[i])))
			continue;

		err_code = XRT_ERROR_CODE_BUILD(
		    get_error_num(errors->errors[i].category),
		    XRT_ERROR_DRIVER_AIE,
		    XRT_ERROR_SEVERITY_CRITICAL,
		    get_error_module(errors->errors[i].module),
		    XRT_ERROR_CLASS_AIE
		    );

		//TODO
		//zocl_insert_error_record(zdev, err_code);
		zocl_aie_cache_error(&slot->aie->err, &((errors->errors)[i]));
	}

	aie_free_errors(errors);

	mutex_unlock(&slot->aie_lock);
}

static struct drm_zocl_slot*
get_slot(struct drm_zocl_dev * zdev, struct kds_client* client, int hw_ctx_id)
{
	int slot_idx = 0;
	int i = 0;
	struct drm_zocl_slot* slot = NULL;
	struct kds_client_hw_ctx *kds_hw_ctx = NULL;
	mutex_lock(&client->lock);
	kds_hw_ctx = kds_get_hw_ctx_by_id(client, hw_ctx_id);
	if (kds_hw_ctx) {
		slot_idx = kds_hw_ctx->slot_idx;
	}

	if (slot_idx >= MAX_PR_SLOT_NUM) {
		DRM_ERROR("%s: Invalid client", __func__);
		mutex_unlock(&client->lock);
		return NULL;
	}

	slot = zdev->pr_slot[slot_idx];
	// WorkAround for older flows where there is single slot for AIE
	if (!slot || !slot->aie) {
		for (i = 0; i < MAX_PR_SLOT_NUM; i++) {
			slot = zdev->pr_slot[i];
			if (slot && slot->aie)
				break;
		}
	}

	mutex_unlock(&client->lock);
	return slot;
}

int
zocl_aie_request_part_fd(struct drm_zocl_dev *zdev, void *data,  struct drm_file *filp)
{
	struct drm_zocl_aie_fd *args = data;
	int fd;
	int rval = 0;
	struct drm_zocl_slot *slot = NULL;
	struct kds_client *client = filp->driver_priv;

	if (!client) {
		DRM_ERROR("%s: Invalid client", __func__);
		return -EINVAL;
	}

	slot = get_slot(zdev, client, args->hw_ctx_id);

	if (!slot) {
		DRM_ERROR("%s: Invalid slot", __func__);
		return -EINVAL;
	}

	mutex_lock(&slot->aie_lock);

	if (!slot->aie) {
		DRM_ERROR("%s: AIE image is not loaded.", __func__);
		rval = -ENODEV;
		goto done;
	}

	if (!slot->aie->aie_dev) {
		DRM_ERROR("%s: No available AIE partition.", __func__);
		rval = -ENODEV;
		goto done;
	}

	if (slot->aie->partition_id != args->partition_id) {
		DRM_ERROR("AIE partition %d does not exist.\n",
		    args->partition_id);
		rval = -ENODEV;
		goto done;
	}

	fd = aie_partition_get_fd(slot->aie->aie_dev);
	if (fd < 0) {
		DRM_ERROR("Get AIE partition %d fd: %d\n",
		    args->partition_id, fd);
		rval = fd;
		goto done;
	}

	args->fd = fd;

	slot->aie->fd_cnt++;

done:
	mutex_unlock(&slot->aie_lock);

	return rval;
}


static void
zocl_aie_reset_work(struct work_struct *aie_work)
{
	struct aie_work_data *data = container_of(aie_work,
	    struct aie_work_data, work);
	struct drm_zocl_slot *slot= data->slot;
	if (!slot) {
		DRM_ERROR("%s: Invalid slot", __func__);
		return;
	}
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
	for (i = 0; i < slot->aie->fd_cnt + 1; i++)
		aie_partition_release(slot->aie->aie_dev);

	slot->aie->fd_cnt = 0;
}

static int
zocl_aie_slot_reset(struct drm_zocl_slot* slot)
{
	if (!slot) {
		DRM_ERROR("%s: Invalid slot", __func__);
		return -EINVAL;
	}
	struct aie_partition_req req;
	bool aie_available = false;
	struct aie_work_data *data;
	int count;

	mutex_lock(&slot->aie_lock);

	if (!slot->aie) {
		mutex_unlock(&slot->aie_lock);
		DRM_ERROR("AIE image is not loaded.\n");
		return -ENODEV;
	}

	if (!slot->aie->aie_dev) {
		mutex_unlock(&slot->aie_lock);
		DRM_ERROR("No available AIE partition.\n");
		return -ENODEV;
	}

	data = kmalloc(sizeof(struct aie_work_data), GFP_KERNEL);
	if (!data) {
		mutex_unlock(&slot->aie_lock);
		DRM_ERROR("Can not allocate memory,\n");
		return -ENOMEM;
	}
	data->slot = slot;

	/*
	 * Per the requrirement of current AIE driver, we
	 * need to release the partition in a separate thread.
	 */
	INIT_WORK(&data->work, zocl_aie_reset_work);
	queue_work(slot->aie->wq, &data->work);

	/* Make sure the reset thread is done */
	flush_workqueue(slot->aie->wq);
	kfree(data);

	req.partition_id = slot->aie->partition_id;
	req.uid = slot->aie->uid;

	/* Check if AIE partition is available in a given time */
	count = 0;
	while (1) {
		aie_available = aie_partition_is_available(&req);
		if (aie_available)
			break;
		count++;
		if (count == ZOCL_AIE_RESET_TIMEOUT_NUMBER) {
			mutex_unlock(&slot->aie_lock);
			DRM_ERROR("AIE Reset fail: timeout.");
			return -ETIME;
		}
		msleep(ZOCL_AIE_RESET_TIMEOUT_INTERVAL);
	}

	slot->aie->aie_dev = NULL;
	slot->aie->aie_reset = true;
	slot->aie->err.num = 0;

	mutex_unlock(&slot->aie_lock);

	DRM_INFO("AIE Reset successfully finished.");
	return 0;

}

static void
zocl_destroy_aie(struct drm_zocl_slot* slot)
{
	if (!slot->aie_information)
		return;

	mutex_lock(&slot->aie_lock);
	vfree(slot->aie_information);
	slot->aie_information = NULL;

	if (!slot->aie) {
		mutex_unlock(&slot->aie_lock);
		return;
	}

	if (slot->aie->aie_dev)
		aie_partition_release(slot->aie->aie_dev);

	if (slot->aie->wq)
		destroy_workqueue(slot->aie->wq);

	vfree(slot->aie->err.errors);
	vfree(slot->aie);
	slot->aie = NULL;
	mutex_unlock(&slot->aie_lock);
}

int
zocl_cleanup_aie(struct drm_zocl_slot *slot)
{
	if (!slot)
	{
		DRM_ERROR("%s: Invalid slot", __func__);
		return 0;
	}

	int ret = 0;

	if (slot->aie) {
		/*
		 * Dont reset if aie is already in reset state
		 */
		if( !slot->aie->aie_reset) {
			ret = zocl_aie_slot_reset(slot);
			if (ret)
				return ret;
		}

		zocl_destroy_aie(slot);
	}

	return 0;
}

int
zocl_read_aieresbin(struct drm_zocl_slot *slot, struct axlf* axlf, char __user *xclbin)
{
	struct axlf_section_header *header = NULL;
	header = xrt_xclbin_get_section_hdr_next(axlf, AIE_RESOURCES_BIN, header);
	int ret = 0;
	while (header) {
		int err = 0;
		long start_col = 0, num_col = 0;

		struct aie_resources_bin *aie_p = (struct aie_resources_bin *)(xclbin + header->m_sectionOffset);
		void *data_portion = vmalloc(aie_p->m_image_size);
		if (!data_portion)
			return -ENOMEM;
		err = copy_from_user(data_portion, (char *)aie_p + aie_p->m_image_offset, aie_p->m_image_size);
		if (err) {
			vfree(data_portion);
			return err;
		}

		if (kstrtol((char*)aie_p +aie_p->m_start_column, 10, &start_col) ||
		    kstrtol((char*)aie_p +aie_p->m_num_columns, 10, &num_col)) {
			vfree(data_portion);
			return -EINVAL; 
		}

		//Call the AIE Driver API 
	        if (slot->aie->partition_id == FULL_ARRAY_PARTITION_ID)
		    ret = aie_part_rscmgr_set_static_range(slot->aie->aie_dev, start_col, num_col, data_portion);
		else
		    ret = aie_part_rscmgr_set_static_range(slot->aie->aie_dev, 0, num_col, data_portion);

		if (ret != 0) {
			vfree(data_portion);
			return ret;
		}
		vfree(data_portion);
        	header = xrt_xclbin_get_section_hdr_next(axlf, AIE_RESOURCES_BIN, header);
	}
	return 0;
}


int
zocl_create_aie(struct drm_zocl_slot *slot, struct axlf *axlf, char __user *xclbin, void *aie_res, uint8_t hw_gen, uint32_t partition_id)
{
	uint64_t offset;
	uint64_t size;
	struct aie_partition_req req;
	int rval = 0;

	rval = xrt_xclbin_section_info(axlf, AIE_METADATA, &offset, &size);
	if (rval)
		return rval;
	mutex_lock(&slot->aie_lock);

	/* AIE is reset and no PDI is loaded after reset */
	if (slot->aie && slot->aie->aie_reset) {
		rval = -ENODEV;
		DRM_ERROR("PDI is not loaded after AIE reset.\n");
		goto done;
	}

	if (!slot->aie) {
		slot->aie = vzalloc(sizeof (struct zocl_aie));
		if (!slot->aie) {
			rval = -ENOMEM;
			DRM_ERROR("Fail to allocate memory.\n");
			goto done;
		}

		slot->aie->err.errors = vzalloc(sizeof (struct aie_error) *
		    ZOCL_AIE_ERROR_CACHE_CAP);
		if (!slot->aie->err.errors) {
			rval = -ENOMEM;
			vfree(slot->aie);
			DRM_ERROR("Fail to allocate memory.\n");
			goto done;
		}

		slot->aie->err.num = 0;
		slot->aie->err.cap = ZOCL_AIE_ERROR_CACHE_CAP;

	}

	if (!slot->aie->wq) {
		slot->aie->wq = create_singlethread_workqueue("aie-workq");
		if (!slot->aie->wq) {
			rval = -ENOMEM;
			DRM_ERROR("Fail to create work queue.\n");
			goto done;
		}
	}

	/* TODO figure out the partition id and uid from xclbin or PDI */
	req.partition_id = partition_id;
	req.uid = 0;
	req.meta_data = 0;

	if (aie_res)
		req.meta_data = (u64)aie_res;

	if (slot->aie->aie_dev) {
		DRM_INFO("Partition %d already requested\n",
		    req.partition_id);
		goto done;
	}

	slot->aie->aie_dev = aie_partition_request(&req);


	if (IS_ERR(slot->aie->aie_dev)) {
		rval = PTR_ERR(slot->aie->aie_dev);
		DRM_ERROR("Request AIE partition %d, %d\n",
		    req.partition_id, rval);
		goto done;
	}

	slot->aie->partition_id = req.partition_id;
	slot->aie->uid = req.uid;

	if (!aie_res) {
		int res = zocl_read_aieresbin(slot, axlf, xclbin);
		if (res)
			goto done;
	}

	/* Register AIE error call back function. */
	rval = aie_register_error_notification(slot->aie->aie_dev,
					       zocl_aie_error_cb, slot);
	mutex_unlock(&slot->aie_lock);

	zocl_init_aie(slot);

	DRM_INFO("AIE create successfully finished.");
	return 0;

done:
	mutex_unlock(&slot->aie_lock);

	return rval;
}




int
zocl_aie_reset(struct drm_zocl_dev *zdev, void *data, struct drm_file *filp)
{
	struct drm_zocl_aie_reset *args = data;
	struct drm_zocl_slot *slot = NULL;
	struct kds_client *client = filp->driver_priv;
	slot = get_slot(zdev, client, args->hw_ctx_id);
	return zocl_aie_slot_reset(slot);
}

int zocl_aie_freqscale(struct drm_zocl_dev *zdev, void *data, struct drm_file *filp)
{
	struct drm_zocl_aie_freq_scale *args = data;
	struct drm_zocl_slot *slot = NULL;
	struct kds_client *client = filp->driver_priv;
	slot = get_slot(zdev, client, args->hw_ctx_id);
	if (!slot) {
		DRM_ERROR("%s: slot is not initialized\n", __func__);
		return -EINVAL;
	}
	int ret = 0;

	mutex_lock(&slot->aie_lock);

	if (!slot->aie) {
		mutex_unlock(&slot->aie_lock);
		DRM_ERROR("AIE image is not loaded.\n");
		return -ENODEV;
	}

	if (!slot->aie->aie_dev) {
		mutex_unlock(&slot->aie_lock);
		DRM_ERROR("No available AIE partition.\n");
		return -ENODEV;
	}

	if (slot->aie->partition_id != args->partition_id) {
		mutex_unlock(&slot->aie_lock);
		DRM_ERROR("AIE partition %d does not exist.\n",
		    args->partition_id);
		return -ENODEV;
	}

	if(!args->dir) {
		// Read frequency from requested aie partition
		ret = aie_partition_get_freq(slot->aie->aie_dev, &args->freq);
		mutex_unlock(&slot->aie_lock);
		if(ret)
			DRM_ERROR("Reading clock frequency from AIE partition(%d) failed with error %d\n",
				args->partition_id, ret);
		return ret;
	} else {
		// Send Set frequency request for aie partition
		ret = aie_partition_set_freq_req(slot->aie->aie_dev, args->freq);
		mutex_unlock(&slot->aie_lock);
		if(ret)
			DRM_ERROR("Setting clock frequency for AIE partition(%d) failed with error %d\n",
				args->partition_id, ret);
		return ret;

	}
}

int
zocl_aie_kds_add_graph_context(struct drm_zocl_dev *zdev, u32 gid,
	u32 ctx_code, struct kds_client *client)
{
	struct list_head *cptr, *gptr;
	struct zocl_graph_ctx_node *gnode;
	struct kds_client *ctx;
	unsigned long graph_flags;
	struct kds_sched *kds = &zdev->kds;
	int ret;

	mutex_lock(&kds->lock);

	list_for_each(cptr, &kds->clients) {
		ctx = list_entry(cptr, struct kds_client, link);

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

	mutex_unlock(&kds->lock);
	return 0;

out:
	spin_unlock_irqrestore(&ctx->graph_list_lock, graph_flags);
	mutex_unlock(&kds->lock);

	return ret;
}

int
zocl_aie_kds_del_graph_context(struct drm_zocl_dev *zdev, u32 gid,
	struct kds_client *client)
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
zocl_aie_kds_del_graph_context_all(struct kds_client *client)
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
zocl_aie_kds_add_context(struct drm_zocl_dev *zdev, u32 ctx_code,
	struct kds_client *client)
{
	struct list_head *cptr;
	struct kds_client *ctx;
	struct kds_sched *kds = &zdev->kds;
	int ret = 0;

	mutex_lock(&kds->lock);

	if (client->aie_ctx != ZOCL_CTX_NOOPS) {
		DRM_ERROR("Changing AIE context is not supported.\n");
		ret = -EBUSY;
		goto out;
	}

	list_for_each(cptr, &kds->clients) {
		ctx = list_entry(cptr, struct kds_client, link);

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

	client->aie_ctx = ZOCL_CTX_NOOPS;

out:
	mutex_unlock(&kds->lock);
	return ret;
}

int
zocl_aie_kds_del_context(struct drm_zocl_dev *zdev, struct kds_client *client)
{
	struct kds_sched *kds = &zdev->kds;

	mutex_lock(&kds->lock);

	if (client->aie_ctx == ZOCL_CTX_NOOPS) {
		DRM_ERROR("No AIE context has been allocated.\n");
		mutex_unlock(&kds->lock);
		return -EINVAL;
	}

	client->aie_ctx = ZOCL_CTX_NOOPS;

	mutex_unlock(&kds->lock);
	return 0;

}

int
zocl_aie_getcmd_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	int ret;
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct drm_zocl_slot *slot = NULL;
	struct kds_client *client = filp->driver_priv;
	struct aie_info_cmd *acmd;
	struct drm_zocl_aie_cmd *kdata = data;
	slot = get_slot(zdev, client, kdata->hw_ctx_id);
	if (!slot) {
		DRM_ERROR("%s: slot is not initialized\n", __func__);
		return -EINVAL;
	}
	struct aie_info *aie = slot->aie_information;

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
	struct drm_zocl_slot *slot = NULL;
	struct kds_client *client = filp->driver_priv;
	struct aie_info_cmd *acmd;
	struct drm_zocl_aie_cmd *kdata = data;
	slot = get_slot(zdev, client, kdata->hw_ctx_id);
	struct aie_info_packet *cmd;

	if (!slot) {
		DRM_ERROR("%s: slot is not initialized\n", __func__);
		return -EINVAL;
	}
	struct aie_info *aie = slot->aie_information;

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
zocl_init_aie(struct drm_zocl_slot *slot)
{
	struct aie_info *aie;
	aie = vzalloc(sizeof(struct aie_info));
	if (!aie) {
		slot->aie_information = NULL;
		return -ENOMEM;
	}

	mutex_init(&aie->aie_lock);
	INIT_LIST_HEAD(&aie->aie_cmd_list);
	init_waitqueue_head(&aie->aie_wait_queue);
	aie->cmd_inprogress = NULL;
	slot->aie_information = aie;

	return 0;
}
