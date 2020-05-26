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

#include "zocl_drv.h"
#include "kds_cu_ctrl.h"
#include "xrt_cu.h"

#define TO_ZOCL_CU_CTRL(c) ((struct zocl_cu_ctrl *)(c))

struct zocl_cu_ctrl {
	struct kds_cu_ctrl	 core;
	struct drm_zocl_dev	*zdev;
};

/* sysfs nods */
static ssize_t
cu_ctx_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct drm_zocl_dev *zdev = dev_get_drvdata(dev);

	return show_cu_ctx(TO_CU_CTRL(zocl_kds_getctrl(zdev, KDS_CU)), buf);
}
static DEVICE_ATTR_RO(cu_ctx);

static struct attribute *cu_ctrl_attrs[] = {
	&dev_attr_cu_ctx.attr,
	NULL,
};

static struct attribute_group cu_ctrl_attr_group = {
	.attrs = cu_ctrl_attrs,
};
/* sysfs nods end */

static void
cu_ctrl_config(struct zocl_cu_ctrl *zcuc, struct kds_command *xcmd)
{
	u32 *cus_addr = (u32 *)xcmd->info;
	size_t num_cus = xcmd->isize / sizeof(u32);
	int apt_idx;
	int i;

	if (config_ctrl(TO_CU_CTRL(zcuc), xcmd))
		return;

	/* TODO: replace aperture list.
	 * Before that, keep this to make aperture work.
	 */
	for (i = 0; i < num_cus; i++) {
		apt_idx = get_apt_index_by_addr(zcuc->zdev, cus_addr[i]);
		WARN_ON(apt_idx < 0);
		update_cu_idx_in_apt(zcuc->zdev, apt_idx, i);
	}
}

static inline void
cu_ctrl_dispatch(struct zocl_cu_ctrl *zcuc, struct kds_command *xcmd)
{
	int inst_idx;

	/* This is call echo type 1.
	 * The echo type 2 could be enabled by module param 'kds_echo=1'
	 */
#if 0
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	xcmd->cb.free(xcmd);
	return;
#endif

	/* In here we need to know the CU subdevice instance id,
	 * which is determined at load xclbin.
	 * It is different from the CU index defined by config command.
	 */
	inst_idx = acquire_cu_inst_idx(TO_CU_CTRL(zcuc), xcmd);
	if (inst_idx >= 0)
		(void) zocl_cu_submit_xcmd(zcuc->zdev, inst_idx, xcmd);
}

static void
cu_ctrl_submit(struct kds_ctrl *ctrl, struct kds_command *xcmd)
{
	struct zocl_cu_ctrl *zcuc = TO_ZOCL_CU_CTRL(ctrl);

	/* Priority from hight to low */
	if (xcmd->opcode != OP_CONFIG_CTRL)
		cu_ctrl_dispatch(zcuc, xcmd);
	else
		cu_ctrl_config(zcuc, xcmd);
}

static int
cu_ctrl_control_ctx(struct kds_ctrl *ctrl, struct kds_client *client,
		    struct kds_ctx_info *info)
{
	return control_ctx(TO_CU_CTRL(ctrl), client, info);
}

int cu_ctrl_add_cu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	int ret;

	ret = add_cu(TO_CU_CTRL(zocl_kds_getctrl(zdev, KDS_CU)), xcu);
	if (ret < 0) {
		DRM_ERROR("Could not find a slot for CU, ret %d\n", ret);
		return ret;
	}

	return 0;
}

int cu_ctrl_remove_cu(struct drm_zocl_dev *zdev, struct xrt_cu *xcu)
{
	int ret;

	ret = remove_cu(TO_CU_CTRL(zocl_kds_getctrl(zdev, KDS_CU)), xcu);
	if (ret < 0) {
		DRM_ERROR("Could not find CU, ret %d\n", ret);
		return ret;
	}

	return 0;
}

int cu_ctrl_init(struct drm_zocl_dev *zdev)
{
	struct zocl_cu_ctrl *zcuc;
	struct kds_ctrl *core;
	int ret = 0;

	zcuc = kzalloc(sizeof(*zcuc), GFP_KERNEL);
	if (!zcuc)
		return -ENOMEM;

	mutex_init(&zcuc->core.lock);
	zcuc->zdev = zdev;

	core = TO_KDS_CTRL(&zcuc->core);
	core->control_ctx = cu_ctrl_control_ctx;
	core->submit = cu_ctrl_submit;

	ret = sysfs_create_group(&zdev->ddev->dev->kobj, &cu_ctrl_attr_group);
	if (ret) {
		DRM_ERROR("create cu_ctrl attrs failed: 0x%x", ret);
		goto err;
	}

	zocl_kds_setctrl(zdev, KDS_CU, core);
	return 0;

err:
	kfree(zcuc);
	return ret;
}

void cu_ctrl_fini(struct drm_zocl_dev *zdev)
{
	struct zocl_cu_ctrl *zcuc;

	zcuc = (struct zocl_cu_ctrl *)zocl_kds_getctrl(zdev, KDS_CU);
	BUG_ON(zcuc == NULL);

	mutex_destroy(&zcuc->core.lock);
	kfree(zcuc);

	zocl_kds_setctrl(zdev, KDS_CU, NULL);

	sysfs_remove_group(&zdev->ddev->dev->kobj, &cu_ctrl_attr_group);
}
