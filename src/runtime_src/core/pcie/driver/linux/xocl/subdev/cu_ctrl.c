// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo CU Controller Sub-device Driver
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xocl_drv.h"
#include "kds_cu_ctrl.h"
#include "xrt_cu.h"

#define XCUC_INFO(xcuc, fmt, arg...) \
	xocl_info(&xcuc->pdev->dev, fmt "\n", ##arg)
#define XCUC_ERR(xcuc, fmt, arg...) \
	xocl_err(&xcuc->pdev->dev, fmt "\n", ##arg)
#define XCUC_DBG(xcuc, fmt, arg...) \
	xocl_dbg(&xcuc->pdev->dev, fmt "\n", ##arg)

#define TO_XOCL_CU_CTRL(c) ((struct xocl_cu_ctrl *)(c))

struct xocl_cu_ctrl {
	struct kds_cu_ctrl	 core;
	struct platform_device	*pdev;
};

/* sysfs nods */
static ssize_t
cu_ctx_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_cu_ctrl *xcuc = platform_get_drvdata(pdev);

	return show_cu_ctx(TO_CU_CTRL(xcuc), buf);
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
cu_ctrl_config(struct xocl_cu_ctrl *xcuc, struct kds_command *xcmd)
{
	(void) config_ctrl(TO_CU_CTRL(xcuc), xcmd);
}

static void cu_ctrl_dispatch(struct xocl_cu_ctrl *xcuc, struct kds_command *xcmd)
{
	xdev_handle_t xdev = xocl_get_xdev(xcuc->pdev);
	int inst_idx;

	/* This is call echo type 1.
	 * The echo type 2 could be enabled by module param 'kds_echo=1'
	 */
	//xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	//xcmd->cb.free(xcmd);
	//return;

	/* In here we need to know the CU subdevice instance id,
	 * which is determined at load xclbin.
	 * It is different from the CU index defined by config command.
	 */
	inst_idx = acquire_cu_inst_idx(TO_CU_CTRL(xcuc), xcmd);
	if (inst_idx >= 0)
		(void) xocl_cu_submit_xcmd(xdev, inst_idx, xcmd);
}

static void cu_ctrl_submit(struct kds_ctrl *ctrl, struct kds_command *xcmd)
{
	struct xocl_cu_ctrl *xcuc = (struct xocl_cu_ctrl *)ctrl;

	/* Priority from hight to low */
	if (xcmd->opcode != OP_CONFIG_CTRL)
		cu_ctrl_dispatch(xcuc, xcmd);
	else
		cu_ctrl_config(xcuc, xcmd);
}

static int
cu_ctrl_control_ctx(struct kds_ctrl *ctrl, struct kds_client *client,
		    struct kds_ctx_info *info)
{
	return control_ctx(TO_CU_CTRL(ctrl), client, info);
}

static int cu_ctrl_add_cu(struct platform_device *pdev, struct xrt_cu *xcu)
{
	struct xocl_cu_ctrl *xcuc = TO_XOCL_CU_CTRL(platform_get_drvdata(pdev));
	int ret;

	ret = add_cu(TO_CU_CTRL(platform_get_drvdata(pdev)), xcu);
	if (ret < 0) {
		XCUC_ERR(xcuc, "Could not find a slot for CU, ret %d", ret);
		return ret;
	}

	return 0;
}

static int cu_ctrl_remove_cu(struct platform_device *pdev, struct xrt_cu *xcu)
{
	struct xocl_cu_ctrl *xcuc = TO_XOCL_CU_CTRL(platform_get_drvdata(pdev));
	int ret;

	ret = remove_cu(TO_CU_CTRL(platform_get_drvdata(pdev)), xcu);
	if (ret < 0) {
		XCUC_ERR(xcuc, "Could not find CU, ret %d", ret);
		return ret;
	}

	return 0;
}

static int cu_ctrl_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu_ctrl *xcuc;
	void *hdl;

	xcuc = platform_get_drvdata(pdev);
	BUG_ON(xcuc == NULL);

	mutex_destroy(&xcuc->core.lock);
	xocl_drvinst_release(xcuc, &hdl);

	xocl_kds_setctrl(xdev, KDS_CU, NULL);

	sysfs_remove_group(&pdev->dev.kobj, &cu_ctrl_attr_group);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

static int cu_ctrl_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu_ctrl *xcuc;
	struct kds_ctrl *core;
	int ret = 0;

	xcuc = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_cu_ctrl));
	if (!xcuc)
		return -ENOMEM;

	mutex_init(&xcuc->core.lock);
	xcuc->pdev = pdev;

	/* TODO: handle irq resource when we support CU interrupt to host */

	platform_set_drvdata(pdev, xcuc);

	core = TO_KDS_CTRL(&xcuc->core);
	core->control_ctx = cu_ctrl_control_ctx;
	core->submit = cu_ctrl_submit;

	xocl_kds_setctrl(xdev, KDS_CU, core);

	ret = sysfs_create_group(&pdev->dev.kobj, &cu_ctrl_attr_group);
	if (ret) {
		XCUC_ERR(xcuc, "create cu_ctrl attrs failed: 0x%x", ret);
		goto err;
	}

	return 0;

err:
	cu_ctrl_remove(pdev);
	return ret;
}

static struct xocl_kds_ctrl_funcs cu_ctrl_ops = {
	.add_cu		= cu_ctrl_add_cu,
	.remove_cu	= cu_ctrl_remove_cu,
};

static struct xocl_drv_private cu_ctrl_priv = {
	.ops = &cu_ctrl_ops,
};

static struct platform_device_id cu_ctrl_id_table[] = {
	{ XOCL_DEVNAME(XOCL_CU_CTRL), (kernel_ulong_t)&cu_ctrl_priv },
	{ },
};

static struct platform_driver cu_ctrl_driver = {
	.probe		= cu_ctrl_probe,
	.remove		= cu_ctrl_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_CU_CTRL),
	},
	.id_table	= cu_ctrl_id_table,
};

int __init xocl_init_cu_ctrl(void)
{
	return platform_driver_register(&cu_ctrl_driver);
}

void xocl_fini_cu_ctrl(void)
{
	platform_driver_unregister(&cu_ctrl_driver);
}
