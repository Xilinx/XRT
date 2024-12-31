/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2022 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *        Jeff Lin <jeffli@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include "zocl_drv.h"
#include "zocl_sk.h"
#include "xrt_cu.h"

#define	SOFT_KERNEL_REG_SIZE	4096

struct zocl_scu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
	struct drm_zocl_bo	*sc_bo;
	/*
	 * This semaphore is used for each soft kernel
	 * CU to wait for next command. When new command
	 * for this CU comes in or we are told to abort
	 * a CU, ert will up this semaphore.
	 */
	struct semaphore	sc_sem;

	/*
	 * soft cu pid and parent pid. This can be used to identify if the
	 * soft cu is still running or not. The parent should never crash
	 */
	u32		sc_pid;
	u32		sc_parent_pid;
	/*
	 * This RW lock is to protect the scu sysfs nodes exported
	 * by zocl driver.
	 */
	rwlock_t	attr_rwlock;

};

static ssize_t debug_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if 0
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_scu *scu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &scu->base;
#endif
	/* Place holder for now. */
	return 0;
}

static ssize_t debug_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
#if 0
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_scu *scu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &scu->base;
#endif

	/* Place holder for now. */
	return count;
}
static DEVICE_ATTR_RW(debug);

static ssize_t
cu_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_scu *scu = platform_get_drvdata(pdev);

	return show_cu_stat(&scu->base, buf);
}
static DEVICE_ATTR_RO(cu_stat);

static ssize_t
cu_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_scu *scu = platform_get_drvdata(pdev);

	return show_cu_info(&scu->base, buf);
}
static DEVICE_ATTR_RO(cu_info);

static ssize_t
stats_begin_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t sz = 0;

	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_scu *scu = platform_get_drvdata(pdev);

	read_lock(&scu->attr_rwlock);
	sz = show_stats_begin(&scu->base, buf);
	read_unlock(&scu->attr_rwlock);

	return sz;
}
static DEVICE_ATTR_RO(stats_begin);

static ssize_t
stats_end_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t sz = 0;

	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_scu *scu = platform_get_drvdata(pdev);

	read_lock(&scu->attr_rwlock);
	sz = show_stats_end(&scu->base, buf);
	read_unlock(&scu->attr_rwlock);

	return sz;
}
static DEVICE_ATTR_RO(stats_end);

static ssize_t
stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t sz = 0;

	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_scu *scu = platform_get_drvdata(pdev);

	read_lock(&scu->attr_rwlock);
	sz = show_formatted_cu_stat(&scu->base, buf);
	read_unlock(&scu->attr_rwlock);

	return sz;
}
static DEVICE_ATTR_RO(stat);

ssize_t show_status(struct zocl_scu *scu, char *buf)
{
	ssize_t sz = 0;

	sz += scnprintf(buf+sz, PAGE_SIZE - sz, "PID:%u\n",
			scu->sc_pid);

	return sz;
}

static ssize_t
status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_scu *scu = platform_get_drvdata(pdev);

	return show_status(scu, buf);
}
static DEVICE_ATTR_RO(status);

static struct attribute *scu_attrs[] = {
	&dev_attr_debug.attr,
	&dev_attr_cu_stat.attr,
	&dev_attr_cu_info.attr,
	&dev_attr_stats_begin.attr,
	&dev_attr_stats_end.attr,
	&dev_attr_stat.attr,
	&dev_attr_status.attr,
	NULL,
};

static const struct attribute_group scu_attrgroup = {
	.attrs = scu_attrs,
};

static int configure_soft_kernel(u32 cuidx, char kname[64], unsigned char uuid[16])
{
	struct drm_zocl_dev *zdev = zocl_get_zdev();
	struct soft_krnl *sk = NULL;
	struct soft_krnl_cmd *scmd = NULL;
	struct config_sk_image_uuid *cp = NULL;

	BUG_ON(!zdev);

	cp = kmalloc(sizeof(struct config_sk_image_uuid), GFP_KERNEL);
	cp->start_cuidx = cuidx;
	cp->num_cus = 1;
	strncpy((char *)cp->sk_name,kname, PS_KERNEL_NAME_LENGTH);
	memcpy(cp->sk_uuid, uuid, sizeof(cp->sk_uuid));

	sk = zdev->soft_kernel;
	// Locking soft kernel data structure
	mutex_lock(&sk->sk_lock);

	/*
	 * Fill up a soft kernel command and add to soft
	 * kernel command list
	 */
	scmd = kmalloc(sizeof(struct soft_krnl_cmd), GFP_KERNEL);
	if (!scmd) {
		DRM_WARN("Config Soft CU failed: no memory.\n");
		mutex_unlock(&sk->sk_lock);
		return -ENOMEM;
	}

	scmd->skc_opcode = ERT_SK_CONFIG;
	scmd->skc_packet = cp;

	list_add_tail(&scmd->skc_list, &sk->sk_cmd_list);
	// Releasing soft kernel data structure lock
	mutex_unlock(&sk->sk_lock);

	/* start CU by waking up PS kernel handler */
	wake_up_interruptible(&sk->sk_wait_queue);

	return 0;
}

static int scu_probe(struct platform_device *pdev)
{
	struct zocl_scu *zcu = NULL;
	struct xrt_cu_info *info = NULL;
	struct drm_zocl_dev *zdev = NULL;
	int err = 0;

	zcu = kzalloc(sizeof(*zcu), GFP_KERNEL);
	if (!zcu)
		return -ENOMEM;

	zcu->pdev = pdev;
	zcu->base.dev = &pdev->dev;
	sema_init(&zcu->sc_sem, 0);

	info = dev_get_platdata(&pdev->dev);
	memcpy(&zcu->base.info, info, sizeof(struct xrt_cu_info));

	zdev = zocl_get_zdev();
	BUG_ON(!zdev);
	zcu->sc_bo = zocl_drm_create_bo(zdev->ddev, SOFT_KERNEL_REG_SIZE, ZOCL_BO_FLAGS_CMA);
	if (IS_ERR(zcu->sc_bo)) {
		return -ENOMEM;
		goto err;
	}
	zcu->sc_bo->flags = ZOCL_BO_FLAGS_CMA;
	err = xrt_cu_scu_init(&zcu->base, zcu->sc_bo->cma_base.vaddr, &zcu->sc_sem);
	if (err) {
		DRM_ERROR("Not able to initial SCU %p\n", zcu);
		goto err;
	}

	platform_set_drvdata(pdev, zcu);

	rwlock_init(&zcu->attr_rwlock);
	err = sysfs_create_group(&pdev->dev.kobj, &scu_attrgroup);
	if (err)
		zocl_err(&pdev->dev, "create SCU attrs failed: %d", err);

	err = configure_soft_kernel(info->cu_idx, info->kname,info->uuid);
	if (err)
		zocl_err(&pdev->dev, "configuring SCU failed: %d", err);

	zocl_info(&pdev->dev, "SCU[%d] created", info->cu_idx);
	return 0;
err:
	kfree(zcu);
	return err;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void scu_remove(struct platform_device *pdev)
#else
static int scu_remove(struct platform_device *pdev)
#endif
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);
	struct drm_zocl_dev *zdev = zocl_get_zdev();
	struct xrt_cu_info *info = &zcu->base.info;

	xrt_cu_scu_fini(&zcu->base);

	zocl_kds_del_scu(zdev, &zcu->base);

	if (zcu->base.res)
		vfree(zcu->base.res);

	// Free Command Buffer BO
	zocl_drm_free_bo(zcu->sc_bo);
	write_lock(&zcu->attr_rwlock);
	sysfs_remove_group(&pdev->dev.kobj, &scu_attrgroup);
	write_unlock(&zcu->attr_rwlock);

	zocl_info(&pdev->dev, "SCU[%d] removed", info->cu_idx);
	kfree(zcu);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	return 0;
#endif
}

static struct platform_device_id scu_id_table[] = {
	{"SCU", 0 },
	{ /* end of table */ },
};

struct platform_driver scu_driver = {
	.probe		= scu_probe,
	.remove		= scu_remove,
	.driver		= {
		.name = "scu_drv",
	},
	.id_table	= scu_id_table,
};

u32 zocl_scu_get_status(struct platform_device *pdev)
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);

	BUG_ON(!zcu);
	return xrt_cu_get_status(&zcu->base);
}

int zocl_scu_create_sk(struct platform_device *pdev, u32 pid, u32 parent_pid, struct drm_file *filp, int *boHandle)
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);
	int ret = 0;

	zcu->sc_pid = pid;
	zcu->sc_parent_pid = parent_pid;
	ret = drm_gem_handle_create(filp,
				    &zcu->sc_bo->cma_base.base, boHandle);
	return(ret);
}

int zocl_scu_wait_cmd_sk(struct platform_device *pdev)
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);
	int ret = 0;
	u32 *vaddr = zcu->sc_bo->cma_base.vaddr;

	/* If the CU is running, mark it as done */
	if (*vaddr == CU_AP_START)
		/* Set CU to AP_DONE */
		*vaddr = CU_AP_DONE;

	if (down_interruptible(&zcu->sc_sem)) {
		ret = -EINTR;
	}

	if (ret) {
		/* We are interrupted */
		return ret;
	}

	/* set CU AP_START */
	*vaddr = CU_AP_START;

	return 0;
}

int zocl_scu_wait_ready(struct platform_device *pdev)
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &zcu->base;
	int ret = 0;

	// Wait for PS kernel initizliation complete
	if(down_timeout(&zcu->sc_sem, msecs_to_jiffies(1000))) {
		zocl_err(&pdev->dev, "PS kernel initialization timed out!");
		return -ETIME;
	}
	ret = zocl_kds_add_scu(zocl_get_zdev(), xcu);
	if (ret) {
		zocl_err(&pdev->dev, "Not able to add SCU %p to KDS", zcu);
		return ret;
	}
	return 0;
}

// Signal SKD Ready
void zocl_scu_sk_ready(struct platform_device *pdev)
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);

	BUG_ON(!zcu);
	up(&zcu->sc_sem);
}

// Signal PS kernel crashed
void zocl_scu_sk_crash(struct platform_device *pdev)
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &zcu->base;

	xrt_cu_scu_crashed(xcu);
}

void zocl_scu_sk_shutdown(struct platform_device *pdev)
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);
	struct pid *p = NULL;
	struct task_struct *task = NULL;
	int ret = 0;

	// Wait for PS Kernel Process to finish
	p = find_get_pid(zcu->sc_pid);
	if(!p) {
		// Process already gone
		goto skip_kill;
	}

	task = pid_task(p, PIDTYPE_PID);
	if(!task) {
		DRM_WARN("Failed to get task for pid %d\n", zcu->sc_pid);
		put_pid(p);
		goto skip_kill;
	}

	if(zcu->sc_parent_pid != task_ppid_nr(task)) {
		DRM_WARN("Parent pid does not match\n");
		put_pid(p);
		goto skip_kill;
	}

	ret = kill_pid(p, SIGTERM, 1);
	if (ret) {
		DRM_WARN("Failed to terminate SCU pid %d.  Performing SIGKILL.\n", zcu->sc_pid);
		kill_pid(p, SIGKILL, 1);
	}
	put_pid(p);

 skip_kill:
	return;
}
