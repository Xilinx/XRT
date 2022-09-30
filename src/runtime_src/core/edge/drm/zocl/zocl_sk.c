/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu       <yliu@xilinx.com>
 *    Jeff Lin        <jeffli@xilinx.com>
 *    Jan Stephan     <j.stephan@hzdr.de>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "ert.h"
#include "zocl_drv.h"
#include "zocl_cu.h"
#include "zocl_sk.h"

int
zocl_sk_getcmd_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct soft_krnl *sk = zdev->soft_kernel;
	struct soft_krnl_cmd *scmd;
	struct drm_zocl_sk_getcmd *kdata = data;

	/* If no command, the process who calls this ioctl will block here */
	mutex_lock(&sk->sk_lock);
	while (list_empty(&sk->sk_cmd_list)) {
		mutex_unlock(&sk->sk_lock);
		if (wait_event_interruptible(sk->sk_wait_queue,
		    !list_empty(&sk->sk_cmd_list)))
			return -ERESTARTSYS;
		mutex_lock(&sk->sk_lock);
	}

	scmd = list_first_entry(&sk->sk_cmd_list, struct soft_krnl_cmd,
	    skc_list);
	list_del(&scmd->skc_list);
	mutex_unlock(&sk->sk_lock);

	kdata->opcode = scmd->skc_opcode;

	if (kdata->opcode == ERT_SK_CONFIG) {
		struct config_sk_image_uuid *cmd = NULL;
		u32 bohdl = 0xffffffff;
		u32 meta_bohdl = 0xffffffff;
		int i, ret;

		cmd = scmd->skc_packet;

		if (sk->sk_meta_bohdl >= 0) {
			meta_bohdl = sk->sk_meta_bohdl;
		} else {
			ret = drm_gem_handle_create(filp,
						    &sk->sk_meta_bo->cma_base.base, &meta_bohdl);

			if (ret) {
				DRM_WARN("%s Failed create metadata BO handle: %d\n",
					 __func__, ret);
				meta_bohdl = 0xffffffff;
			} else {
				sk->sk_meta_bohdl = meta_bohdl;
				DRM_INFO("sk_meta_data BO handle 0x%x created\n",meta_bohdl);
			}
		}

		for (i = 0; i < sk->sk_nimg; i++) {
			if(strcmp(sk->sk_img[i].scu_name,(char *)cmd->sk_name)!=0) {
				DRM_INFO("SK image name %s not matching sk_name %s\n",
					 sk->sk_img[i].scu_name,(char *)cmd->sk_name);
				continue;
			}
			DRM_INFO("Found SK image name %s matching sk_name %s\n",
				 sk->sk_img[i].scu_name,(char *)cmd->sk_name);

			if (sk->sk_img[i].si_bohdl >= 0) {
				bohdl = sk->sk_img[i].si_bohdl;
				break;
			}

			ret = drm_gem_handle_create(filp,
			    &sk->sk_img[i].si_bo->cma_base.base, &bohdl);

			if (ret) {
				DRM_WARN("%s Failed create BO handle: %d\n",
				    __func__, ret);
				bohdl = 0xffffffff;
			} else {
				sk->sk_img[i].si_bohdl = bohdl;
				DRM_INFO("sk_img[%d] BO handle %d created\n",i,bohdl);
			}

			break;
		}

		/* Copy the command to ioctl caller */
		kdata->start_cuidx = cmd->start_cuidx;
		kdata->cu_nums = cmd->num_cus;
		kdata->bohdl = bohdl;
		kdata->meta_bohdl = meta_bohdl;
		memcpy(kdata->uuid,cmd->sk_uuid,sizeof(kdata->uuid));

		snprintf(kdata->name, ZOCL_MAX_NAME_LENGTH, "%s",
		    (char *)cmd->sk_name);
	} else
		/* We will handle more opcodes */
		DRM_WARN("Unknown soft kernel command: %d\n",
		    kdata->opcode);

	kfree(scmd);
	return 0;
}

int
zocl_sk_create_ioctl(struct drm_device *dev, void *data, struct drm_file *filp)
{
	struct drm_zocl_sk_create *args = data;
	uint32_t cu_idx = args->cu_idx;
	struct platform_device *zert = zocl_find_pdev("ert_hw");
	struct platform_device *scu_pdev = NULL;
	struct zocl_cu *scu = NULL;
	int boHandle = -1;
	int ret = 0;
	
	if (cu_idx >= MAX_SOFT_KERNEL) {
		DRM_ERROR("Fail to create soft kernel: CU index %d > %d.\n",
			  cu_idx,MAX_SOFT_KERNEL);
		return -EINVAL;
	}

	if(!zert) {
		DRM_ERROR("ERT not found!");
		return -EINVAL;
	}
	scu_pdev = zert_get_scu_pdev(zert, cu_idx);
     	scu = platform_get_drvdata(scu_pdev);
	if (!scu) {
		DRM_ERROR("SCU %d does not exist.\n", cu_idx);
		return -EINVAL;
	}

	ret = zocl_scu_create_sk(scu_pdev,task_pid_nr(current),task_ppid_nr(current), filp, &boHandle);
	if (ret) {
		DRM_WARN("%s Failed to create SK command BO handle: %d\n",
			 __func__, ret);
		boHandle = 0xffffffff;
	}
	
	args->handle = boHandle;
	return ret;
}

int
zocl_sk_report_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	struct drm_zocl_sk_report *args = data;
	struct zocl_cu *scu = NULL;
	uint32_t cu_idx = args->cu_idx;
	enum drm_zocl_scu_state state = args->cu_state;
	int ret = 0;
	struct platform_device *zert = zocl_find_pdev("ert_hw");
	struct platform_device *scu_pdev = NULL;
	
	if(!zert) {
		DRM_ERROR("ERT not found!");
		return -EINVAL;
	}
	scu_pdev = zert_get_scu_pdev(zert, cu_idx);
	if (!scu_pdev) {
		DRM_ERROR("SCU %d does not exist.\n", cu_idx);
		return -EINVAL;
	}

	scu = platform_get_drvdata(scu_pdev);
	if (!scu) {
		DRM_ERROR("SCU %d does not exist.\n", cu_idx);
		return -EINVAL;
	}

	switch (state) {

	case ZOCL_SCU_STATE_DONE:

		ret = zocl_scu_wait_cmd_sk(scu_pdev);
		break;

	case ZOCL_SCU_STATE_READY:

		zocl_scu_sk_ready(scu_pdev);
		break;

	case ZOCL_SCU_STATE_CRASH:

		zocl_scu_sk_crash(scu_pdev);
		break;

	case ZOCL_SCU_STATE_FINI:
		zocl_scu_sk_fini(scu_pdev);
		break;

	default:
		/*
		 * More soft kernel state will be added as the kernel
		 * is not completed but ready to take another run.
		 */
		return -EINVAL;
	}

	return ret;
}

int
zocl_init_soft_kernel(struct drm_zocl_dev *zdev)
{
	struct soft_krnl *sk;

	BUG_ON(!zdev);
	sk = devm_kzalloc(zdev->ddev->dev, sizeof(*sk), GFP_KERNEL);
	if (!sk)
		return -ENOMEM;

	zdev->soft_kernel = sk;
	mutex_init(&sk->sk_lock);
	INIT_LIST_HEAD(&sk->sk_cmd_list);
	init_waitqueue_head(&sk->sk_wait_queue);

	return 0;
}

void
zocl_fini_soft_kernel(struct drm_zocl_dev *zdev)
{
	struct soft_krnl *sk;
	int i;

	BUG_ON(!zdev);
	sk = zdev->soft_kernel;
	mutex_lock(&sk->sk_lock);

	if(!IS_ERR(&sk->sk_meta_bo)) {
		zocl_drm_free_bo(sk->sk_meta_bo);
	}
	
	for (i = 0; i < sk->sk_nimg; i++) {
		if (IS_ERR(&sk->sk_img[i].si_bo))
			continue;
		zocl_drm_free_bo(sk->sk_img[i].si_bo);
	}
	kfree(sk->sk_img);

	mutex_unlock(&sk->sk_lock);
	mutex_destroy(&sk->sk_lock);
}
