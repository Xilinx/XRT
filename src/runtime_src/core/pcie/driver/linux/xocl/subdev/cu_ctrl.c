// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Alveo CU Controller Sub-device Driver
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: min.ma@xilinx.com
 */

#include "xocl_drv.h"
#include "kds_core.h"
#include "xrt_cu.h"

#define XCUC_INFO(xcuc, fmt, arg...) \
	xocl_info(&xcuc->pdev->dev, fmt "\n", ##arg)
#define XCUC_ERR(xcuc, fmt, arg...) \
	xocl_err(&xcuc->pdev->dev, fmt "\n", ##arg)
#define XCUC_DBG(xcuc, fmt, arg...) \
	xocl_dbg(&xcuc->pdev->dev, fmt "\n", ##arg)

struct xocl_cu_ctrl {
	struct kds_controller    core;
	struct platform_device	*pdev;
	struct xrt_cu		*xcus[MAX_CUS];
	int			 num_cus;

};

static int get_cu_by_addr(struct xocl_cu_ctrl *xcuc, u32 addr)
{
	int i;

	/* Do not use this search in critical path */
	for (i = 0; i < xcuc->num_cus; ++i) {
		if (xcuc->xcus[i]->info.addr == addr)
			break;
	}

	return i;
}

static inline int cu_mask_to_cu_idx(struct kds_command *xcmd)
{
	/* TODO: balance the CU usage if multiple bits are set */

	/* assume there is alwasy one CU */
	return 0;
}

static void cu_ctrl_config(struct xocl_cu_ctrl *xcuc, struct kds_command *xcmd)
{
	u32 *cus_addr = (u32 *)xcmd->info;
	size_t num_cus = xcmd->isize / sizeof(u32);
	struct xrt_cu *tmp;
	int i, j;

	/* I don't care if the configure command claim less number of cus */
	if (num_cus > xcuc->num_cus)
		goto error;

	/* Now we need to make CU index right */
	for (i = 0; i < num_cus; i++) {
		j = get_cu_by_addr(xcuc, cus_addr[i]);
		if (j == xcuc->num_cus)
			goto error;

		/* Ordering CU index */
		if (j != i) {
			tmp = xcuc->xcus[i];
			xcuc->xcus[i] = xcuc->xcus[j];
			xcuc->xcus[j] = tmp;
		}
		xcuc->xcus[i]->info.cu_idx = i;
	}

	/* TODO: Does it need a queue for configure commands? */
	xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	xcmd->cb.free(xcmd);
	return;

error:
	xcmd->cb.notify_host(xcmd, KDS_ERROR);
	xcmd->cb.free(xcmd);
}

static void cu_ctrl_dispatch(struct xocl_cu_ctrl *xcuc, struct kds_command *xcmd)
{
	xdev_handle_t xdev = xocl_get_xdev(xcuc->pdev);
	int cu_idx;
	int inst_idx;

	/* Select CU */
	cu_idx = cu_mask_to_cu_idx(xcmd);

	/* about 850K IOPS, if only do below two lines
	 * The purpose is to show how fast a single user thread
	 * could produce a CU task.
	 */
	//xcmd->cb.notify_host(xcmd, KDS_COMPLETED);
	//xcmd->cb.free(xcmd);
	//return;

	inst_idx = xcuc->xcus[cu_idx]->info.inst_idx;
	(void) xocl_cu_submit_xcmd(xdev, inst_idx, xcmd);
}

static void cu_ctrl_submit(struct kds_controller *ctrl, struct kds_command *xcmd)
{
	struct xocl_cu_ctrl *xcuc = (struct xocl_cu_ctrl *)ctrl;

	/* Priority from hight to low */
	if (xcmd->opcode != OP_CONFIG_CTRL)
		cu_ctrl_dispatch(xcuc, xcmd);
	else
		cu_ctrl_config(xcuc, xcmd);
}

static int cu_ctrl_add_cu(struct platform_device *pdev, struct xrt_cu *xcu)
{
	struct xocl_cu_ctrl *xcuc = platform_get_drvdata(pdev);
	int i;

	if (xcuc->num_cus >= MAX_CUS)
		return -ENOMEM;

	for (i = 0; i < MAX_CUS; i++) {
		if (xcuc->xcus[i] != NULL)
			continue;

		xcuc->xcus[i] = xcu;
		++xcuc->num_cus;
		break;
	}

	if (i == MAX_CUS) {
		XCUC_ERR(xcuc, "Could not find a slot for CU %p", xcu);
		return -ENOSPC;
	}

	return 0;
}

static int cu_ctrl_remove_cu(struct platform_device *pdev, struct xrt_cu *xcu)
{
	struct xocl_cu_ctrl *xcuc = platform_get_drvdata(pdev);
	int i, ret = 0;

	if (xcuc->num_cus == 0)
		return -EINVAL;

	/* The xcus list is not the same as when a CU was added
	 * search the CU..
	 */
	for (i = 0; i < MAX_CUS; i++) {
		if (xcuc->xcus[i] != xcu)
			continue;

		xcuc->xcus[i] = NULL;
		--xcuc->num_cus;
		break;
	}

	if (i == MAX_CUS) {
		XCUC_ERR(xcuc, "Could not find CU %p", xcu);
		return -EINVAL;
	}

	return ret;
}

static int cu_ctrl_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu_ctrl *xcuc;

	xcuc = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xocl_cu_ctrl));
	if (!xcuc)
		return -ENOMEM;

	xcuc->pdev = pdev;

	/* TODO: handle irq resource when we support CU interrupt to host */

	platform_set_drvdata(pdev, xcuc);

	xcuc->core.submit = cu_ctrl_submit;

	xocl_kds_setctrl(xdev, KDS_CU, (struct kds_controller *)xcuc);

	return 0;
}

static int cu_ctrl_remove(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_cu_ctrl *xcuc;
	void *hdl;

	xcuc = platform_get_drvdata(pdev);
	if (!xcuc)
		return -EINVAL;

	xocl_drvinst_release(xcuc, &hdl);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	xocl_kds_setctrl(xdev, KDS_CU, NULL);

	return 0;
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
