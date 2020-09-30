/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
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
zocl_create_aie(struct drm_zocl_dev *zdev, struct axlf *axlf)
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

	vfree(zdev->aie);
	zdev->aie = NULL;
	mutex_unlock(&zdev->aie_lock);
}

static void
zock_aie_reset_work(struct work_struct *aie_work)
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
	INIT_WORK(&data->work, zock_aie_reset_work);
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

	mutex_unlock(&zdev->aie_lock);

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
	struct aie_info_packet *aiec_packet;

	if(aie == NULL)
		return -EAGAIN;

	mutex_lock(&aie->aie_lock);
	acmd = aie->cmd_inprogress;
	mutex_unlock(&aie->aie_lock);
	if (!acmd) {
		return -ENOMEM;
	}

	struct aie_info_packet *cmd;
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
