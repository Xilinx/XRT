/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
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

	kdata->opcode = scmd->skc_opcode;

	if (kdata->opcode == ERT_SK_CONFIG) {
		struct config_sk_image *cmd;
		u32 bohdl = 0xffffffff;
		int i, ret;

		cmd = scmd->skc_packet;

		for (i = 0; i < sk->sk_nimg; i++) {
			if (cmd->start_cuidx > sk->sk_img[i].si_end)
				continue;

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
			}

			break;
		}

		/* Copy the command to ioctl caller */
		kdata->start_cuidx = cmd->start_cuidx;
		kdata->cu_nums = cmd->num_cus;
		kdata->bohdl = bohdl;

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

	gem_obj = zocl_gem_object_lookup(dev, filp, args->handle);
	if (!gem_obj) {
		DRM_ERROR("Fail to create soft kernel: BO %d does not exist.\n",
		    args->handle);
		return -ENXIO;
	}
	bo = to_zocl_bo(gem_obj);

	mutex_lock(&sk->sk_lock);

	if (sk->sk_cu[cu_idx]) {
		DRM_ERROR("Fail to create soft kernel: CU %d created.\n",
		    cu_idx);
		mutex_unlock(&sk->sk_lock);
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
		return -EINVAL;
	}

	sk->sk_cu[cu_idx] = kzalloc(sizeof(struct soft_krnl), GFP_KERNEL);
	if (!sk->sk_cu[cu_idx]) {
		DRM_ERROR("Fail to create soft kernel: no memory.\n");
		mutex_unlock(&sk->sk_lock);
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(gem_obj);
		return -ENOMEM;
	}

	sk->sk_cu[cu_idx]->sc_pid = task_pid_nr(current);
	sk->sk_cu[cu_idx]->sc_parent_pid = task_ppid_nr(current);

	sk->sk_cu[cu_idx]->sc_vregs = bo->cma_base.vaddr;
	sk->sk_cu[cu_idx]->gem_obj = gem_obj;
	sema_init(&sk->sk_cu[cu_idx]->sc_sem, 0);

	mutex_unlock(&sk->sk_lock);

	/* Hold refcnt till soft kernel is released or driver unload */

	return 0;
}

/*
 * Update the memory stats for softkernel .
 */
void zocl_sk_mem_stat_incr(struct drm_zocl_dev *zdev,
                unsigned m_stat_type)
{
	struct soft_krnl *sk = zdev->soft_kernel;
	struct sk_mem_stats *mem_stat = &sk->mem_stats;

	write_lock(&zdev->attr_rwlock);
	switch (m_stat_type) {
		case ZOCL_MEM_STAT_TYPE_HBO:
			mem_stat->hbo_cnt++;
			break;

		case ZOCL_MEM_STAT_TYPE_MAPBO:
			mem_stat->mapbo_cnt++;
			break;

		case ZOCL_MEM_STAT_TYPE_UNMAPBO:
			mem_stat->unmapbo_cnt++;
			break;

		case ZOCL_MEM_STAT_TYPE_FREEBO:
			mem_stat->freebo_cnt++;
			break;
	}
	write_unlock(&zdev->attr_rwlock);
}

int
zocl_sk_report_ioctl(struct drm_device *dev, void *data,
		struct drm_file *filp)
{
	struct drm_zocl_dev *zdev = dev->dev_private;
	struct soft_krnl *sk = zdev->soft_kernel;
	struct drm_zocl_sk_report *args = data;
	struct soft_cu *scu = NULL;
	uint32_t cu_idx = args->cu_idx;
	uint32_t *vaddr = NULL;
	struct scu_usages *sc_usage = NULL;
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
	if (!scu) {
		DRM_ERROR("CU %d does not exist.\n", cu_idx);
		mutex_unlock(&sk->sk_lock);
		return -EINVAL;
	}

	sc_usage = &sk->sk_cu[args->cu_idx]->sc_usages;

	switch (state) {

	case ZOCL_SCU_STATE_DONE:

		vaddr = scu->sc_vregs;

		/* If the CU is running, mark it as done */
		if (*vaddr & 1) {
			/* Clear Bit 0 and set Bit 1 */
			*vaddr = 2 | (*vaddr & ~3);

			/* Update stats based on SCU return value */
			if (vaddr[1]) // This stores return value of softkernel. 
				++sc_usage->err_cnt;
			else
				++sc_usage->succ_cnt;
		}

		mutex_unlock(&sk->sk_lock);
		if (down_killable(&scu->sc_sem))
			ret = -EINTR;
		mutex_lock(&sk->sk_lock);
		/* if scu does not equal, it means been killed. */
		if (scu != sk->sk_cu[args->cu_idx]) {
			mutex_unlock(&sk->sk_lock);
			return ret;
		}

		if (ret) {
			/* We are interrupted */
			++sc_usage->crsh_cnt;
			mutex_unlock(&sk->sk_lock);
			return ret;
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

	sk = devm_kzalloc(drm->dev, sizeof(*sk), GFP_KERNEL);
	if (!sk)
		return -ENOMEM;

	zdev->soft_kernel = sk;
	mutex_init(&sk->sk_lock);
	INIT_LIST_HEAD(&sk->sk_cmd_list);
	init_waitqueue_head(&sk->sk_wait_queue);

	return 0;
}

void
zocl_fini_soft_kernel(struct drm_device *drm)
{
	struct drm_zocl_dev *zdev = drm->dev_private;
	struct soft_krnl *sk;
	u32 cu_idx;
	int i;

	sk = zdev->soft_kernel;
	mutex_lock(&sk->sk_lock);
	for (cu_idx = 0; cu_idx < sk->sk_ncus; cu_idx++) {
		if (!sk->sk_cu[cu_idx])
			continue;

		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(sk->sk_cu[cu_idx]->gem_obj);
		kfree(sk->sk_cu[cu_idx]);
		sk->sk_cu[cu_idx] = NULL;
	}

	for (i = 0; i < sk->sk_nimg; i++) {
		if (IS_ERR(&sk->sk_img[i].si_bo))
			continue;
		ZOCL_DRM_GEM_OBJECT_PUT_UNLOCKED(&sk->sk_img[i].si_bo->gem_base);
	}
	kfree(sk->sk_img);

	mutex_unlock(&sk->sk_lock);
	mutex_destroy(&sk->sk_lock);
}
