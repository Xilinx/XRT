/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu       <yliu@xilinx.com>
 *    Jan Stephan     <j.stephan@hzdr.de>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "ert.h"
#include "zocl_drv.h"
#include "zocl_sk.h"
#include "sched_exec.h"

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

	kdata->opcode = scmd->skc_packet->opcode;

	if (kdata->opcode == ERT_SK_CONFIG) {
		struct ert_configure_sk_cmd *cmd;

		/* Copy the command to ioctl caller */
		cmd = (struct ert_configure_sk_cmd *)scmd->skc_packet;
		kdata->start_cuidx = cmd->start_cuidx;
		kdata->cu_nums = cmd->num_cus;
		kdata->size = cmd->sk_size;

		/* soft kernel's physical address (little endian) */
		kdata->paddr = cmd->sk_addr;

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
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct soft_krnl *sk = zdev->soft_kernel;
	struct drm_zocl_sk_create *args = data;
	struct drm_gem_object *gem_obj;
	struct drm_zocl_bo *bo;
	uint32_t cu_idx = args->cu_idx;

	mutex_lock(&sk->sk_lock);

	if (sk->sk_cu[cu_idx]) {
		DRM_ERROR("Fail to create soft kernel: CU %d created.\n",
		    cu_idx);
		mutex_unlock(&sk->sk_lock);
		return -EINVAL;
	}

	sk->sk_cu[cu_idx] = kzalloc(sizeof(struct soft_krnl), GFP_KERNEL);
	if (!sk->sk_cu[cu_idx]) {
		DRM_ERROR("Fail to create soft kernel: no memory.\n");
		mutex_unlock(&sk->sk_lock);
		return -ENOMEM;
	}

	mutex_unlock(&sk->sk_lock);

	gem_obj = zocl_gem_object_lookup(dev, filp, args->handle);
	if (!gem_obj) {
		DRM_ERROR("Fail to create soft kernel: BO %d does not exist.\n",
		    args->handle);
		return -ENXIO;
	}
	bo = to_zocl_bo(gem_obj);
	sk->sk_cu[cu_idx]->sc_vregs = bo->cma_base.vaddr;
	sema_init(&sk->sk_cu[cu_idx]->sc_sem, 0);

	ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);

	return 0;
}

int
zocl_sk_report_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct soft_krnl *sk = zdev->soft_kernel;
	struct drm_zocl_sk_report *args = data;
	struct soft_cu *scu;
	uint32_t cu_idx = args->cu_idx;
	uint32_t *vaddr;
	enum drm_zocl_scu_state state = args->cu_state;
	int ret = 0;

	mutex_lock(&sk->sk_lock);

	if (args->cu_idx > sk->sk_ncus) {
		DRM_ERROR("Fail to get cu state: CU %d does not exist.\n",
		    cu_idx);
		mutex_unlock(&sk->sk_lock);
		return -ENXIO;
	}

	scu = sk->sk_cu[args->cu_idx];

	switch (state) {

	case ZOCL_SCU_STATE_DONE:

		vaddr = scu->sc_vregs;

		/* If the CU is running, mark it as done */
		if (*vaddr & 1)
			/* Clear Bit 0 and set Bit 1 */
			*vaddr = 2 | (*vaddr & ~3);

		mutex_unlock(&sk->sk_lock);
		if (down_interruptible(&scu->sc_sem))
			ret = -ERESTARTSYS;
		mutex_lock(&sk->sk_lock);

		if (ret || scu->sc_flags & ZOCL_SCU_FLAGS_RELEASE) {
			/*
			 * If we are interrupted or explictly
			 * told to exit.
			 */
			kfree(sk->sk_cu[args->cu_idx]);
			sk->sk_cu[args->cu_idx] = NULL;

			mutex_unlock(&sk->sk_lock);
			return ret ? ret : -ESRCH;
		}

		/* Clear Bit 1 and set Bit 0 */
		*vaddr = 1 | (*vaddr & ~3);

		mutex_unlock(&sk->sk_lock);

		break;

	default:
		/*
		 * More soft kernel state will be added as the kernel
		 * is not completed but ready to take another run.
		 */
		return -EINVAL;
	}

	return 0;
}

int
zocl_init_soft_kernel(struct drm_device *drm)
{
	struct drm_zocl_dev *zdev = drm->dev_private;
	struct soft_krnl *sk;

	sk = devm_kzalloc(drm->dev, sizeof (*sk), GFP_KERNEL);
	if (!sk)
		return -ENOMEM;

	zdev->soft_kernel = sk;
	mutex_init(&sk->sk_lock);
	INIT_LIST_HEAD(&sk->sk_cmd_list);
	init_waitqueue_head(&sk->sk_wait_queue);

	return 0;
}
