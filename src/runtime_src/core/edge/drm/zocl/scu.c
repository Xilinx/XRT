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

struct zocl_scu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
	spinlock_t		 lock;
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
stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct zocl_scu *scu = platform_get_drvdata(pdev);

	return show_formatted_cu_stat(&scu->base, buf);
}
static DEVICE_ATTR_RO(stat);

static struct attribute *scu_attrs[] = {
	&dev_attr_debug.attr,
	&dev_attr_cu_stat.attr,
	&dev_attr_cu_info.attr,
	&dev_attr_stat.attr,
	NULL,
};

static const struct attribute_group scu_attrgroup = {
	.attrs = scu_attrs,
};

static int configure_soft_kernel(u32 cuidx, char kname[64], unsigned char uuid[16])
{
	struct drm_zocl_dev *zdev = zocl_get_zdev();
	struct soft_krnl *sk = zdev->soft_kernel;
	struct soft_krnl_cmd *scmd = NULL;
	struct config_sk_image *cp = NULL;

	cp = kmalloc(sizeof(struct config_sk_image), GFP_KERNEL);
	cp->start_cuidx = cuidx;
	cp->num_cus = 1;
	strncpy((char *)cp->sk_name,kname,PS_KERNEL_NAME_LENGTH);
	memcpy(cp->sk_uuid,uuid,sizeof(cp->sk_uuid));

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

	info = dev_get_platdata(&pdev->dev);
	memcpy(&zcu->base.info, info, sizeof(struct xrt_cu_info));

	zdev = zocl_get_zdev();
	err = zocl_kds_add_scu(zdev, &zcu->base);
	if (err) {
		DRM_ERROR("Not able to add SCU %p to KDS", zcu);
		goto err;
	}

	err = xrt_cu_scu_init(&zcu->base);
	if (err) {
		DRM_ERROR("Not able to initial SCU %p\n", zcu);
		goto err2;
	}

	platform_set_drvdata(pdev, zcu);

	err = sysfs_create_group(&pdev->dev.kobj, &scu_attrgroup);
	if (err)
		zocl_err(&pdev->dev, "create SCU attrs failed: %d", err);

	err = configure_soft_kernel(info->cu_idx,info->kname,info->uuid);
	if (err)
		zocl_err(&pdev->dev, "configuring SCU failed: %d", err);
	
	zocl_info(&pdev->dev, "SCU[%d] created", info->cu_idx);
	return 0;
err2:
	zocl_kds_del_scu(zdev, &zcu->base);
err:
	kfree(zcu);
	return err;
}

static int scu_remove(struct platform_device *pdev)
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &zcu->base;
	struct xrt_cu_scu *cu_scu = xcu->core;
	struct drm_zocl_dev *zdev = zocl_get_zdev();
	struct xrt_cu_info *info = &zcu->base.info;

	xrt_cu_scu_fini(&zcu->base);

	zocl_kds_del_cu(zdev, &zcu->base);

	if (zcu->base.res)
		vfree(zcu->base.res);

	sysfs_remove_group(&pdev->dev.kobj, &scu_attrgroup);

	zocl_info(&pdev->dev, "SCU[%d] removed", info->cu_idx);
	kfree(zcu);

	return 0;
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
	struct xrt_cu *xcu = &zcu->base;
	struct xrt_cu_scu *cu_scu = xcu->core;
	int ret = 0;

	if(!cu_scu) {
		return -EINVAL;
	}
	cu_scu->sc_pid = pid;
	cu_scu->sc_parent_pid = parent_pid;
	sema_init(&cu_scu->sc_sem, 0);
	ret = drm_gem_handle_create(filp,
				    &cu_scu->sc_bo->cma_base.base, boHandle);
	return(ret);
}
int zocl_scu_wait_cmd_sk(struct platform_device *pdev)
{
	struct zocl_scu *zcu = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &zcu->base;
	struct xrt_cu_scu *cu_scu = xcu->core;
	int ret = 0;
	u32 *vaddr = cu_scu->vaddr;

	/* If the CU is running, mark it as done */
	if (*vaddr & 1)
		/* Clear Bit 0 and set Bit 1 */
		*vaddr = 2 | (*vaddr & ~3);

	if (down_killable(&cu_scu->sc_sem))
		ret = -EINTR;

	if (ret) {
		/* We are interrupted */
		return ret;
	}

	/* Clear Bit 1 and set Bit 0 */
	*vaddr = 1 | (*vaddr & ~3);

	return 0;
}
