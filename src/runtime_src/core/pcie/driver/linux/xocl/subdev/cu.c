// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo CU Sub-device Driver
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xocl_drv.h"
#include "xrt_cu.h"

#define XCU_INFO(xcu, fmt, arg...) \
	xocl_info(&xcu->pdev->dev, fmt "\n", ##arg)
#define XCU_ERR(xcu, fmt, arg...) \
	xocl_err(&xcu->pdev->dev, fmt "\n", ##arg)
#define XCU_DBG(xcu, fmt, arg...) \
	xocl_dbg(&xcuc->pdev->dev, fmt "\n", ##arg)

struct xocl_cu {
	struct xrt_cu		 base;
	struct platform_device	*pdev;
};

static int cu_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu *xcu;
	struct resource **res;
	struct xrt_cu_info *info;
	int err = 0;
	void *hdl;
	int i;

	xcu = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_cu));
	if (!xcu)
		return -ENOMEM;

	xcu->pdev = pdev;
	xcu->base.dev = XDEV2DEV(xdev);

	info = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	memcpy(&xcu->base.info, info, sizeof(struct xrt_cu_info));

	res = vzalloc(sizeof(struct resource *) * xcu->base.info.num_res);
	if (!res) {
		err = -ENOMEM;
		goto err;
	}

	for (i = 0; i < xcu->base.info.num_res; ++i) {
		res[i] = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res[i]) {
			err = -EINVAL;
			goto err;
		}
	}
	xcu->base.res = res;

	err = xrt_cu_init(&xcu->base);
	if (err) {
		XCU_ERR(xcu, "Not able to initial CU %p", xcu);
		goto err;
	}

	 /* Is time to add this CU to the CU controller's list */
	err = xocl_cu_ctrl_add_cu(xdev, &xcu->base);
	if (err) {
		XCU_ERR(xcu, "Not able to add CU %p to controller", xcu);
		goto err1;
	}

	platform_set_drvdata(pdev, xcu);

	return 0;

err1:
	xrt_cu_fini(&xcu->base);
err:
	xocl_drvinst_release(xcu, &hdl);
	xocl_drvinst_free(hdl);
	return err;
}

static int cu_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu *xcu;
	void *hdl;
	int err = 0;

	xcu = platform_get_drvdata(pdev);
	if (!xcu)
		return -EINVAL;

	err = xocl_cu_ctrl_remove_cu(xdev, &xcu->base);
	if (err)
		XCU_ERR(xcu, "Remove CU failed?");

	xrt_cu_fini(&xcu->base);

	if (xcu->base.res)
		vfree(xcu->base.res);

	xocl_drvinst_release(xcu, &hdl);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

static struct platform_device_id cu_id_table[] = {
	{ XOCL_DEVNAME(XOCL_CU), 0 },
	{ },
};

static struct platform_driver cu_driver = {
	.probe		= cu_probe,
	.remove		= cu_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_CU),
	},
	.id_table	= cu_id_table,
};

int __init xocl_init_cu(void)
{
	return platform_driver_register(&cu_driver);
}

void xocl_fini_cu(void)
{
	platform_driver_unregister(&cu_driver);
}
