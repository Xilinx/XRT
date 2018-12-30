/*
 * FPGA MGR bindings for XRT xocl driver
 *
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Sonal Santan
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/fpga/fpga-mgr.h>

#include "../xocl_drv.h"
#include "xclbin.h"

static int xocl_pr_write_init(struct fpga_manager *mgr,
			      struct fpga_image_info *info, const char *buf, size_t count)
{
	return 0;
}

static int xocl_pr_write(struct fpga_manager *mgr,
			 const char *buf, size_t count)
{
	return 0;
}


static int xocl_pr_write_complete(struct fpga_manager *mgr,
				 struct fpga_image_info *info)
{
	return 0;
}

static enum fpga_mgr_states xocl_pr_state(struct fpga_manager *mgr)
{
	return FPGA_MGR_STATE_UNKNOWN;
}

static const struct fpga_manager_ops xocl_pr_ops = {
	.initial_header_size = sizeof(struct axlf),
	.write_init = xocl_pr_write_init,
	.write = xocl_pr_write,
	.write_complete = xocl_pr_write_complete,
	.state = xocl_pr_state,
};


struct platform_device_id fmgr_id_table[] = {
	{ XOCL_FMGR, 0 },
	{ },
};

static int fmgr_probe(struct platform_device *pdev)
{
	int ret;
	struct xocl_dev *xdev = xocl_get_xdev(pdev);
	ret = fpga_mgr_register(&pdev->dev,
				"Xilinx PCIe FPGA Manager", &xocl_pr_ops, xdev);
	return ret;
}

static int fmgr_remove(struct platform_device *pdev)
{
	fpga_mgr_unregister(&pdev->dev);
	return 0;
}

static struct platform_driver	fmgr_driver = {
	.probe		= fmgr_probe,
	.remove		= fmgr_remove,
	.driver		= {
		.name = "xocl_fmgr",
	},
	.id_table = fmgr_id_table,
};

int __init xocl_init_fmgr(void)
{
	return platform_driver_register(&fmgr_driver);
}

void xocl_fini_fmgr(void)
{
	platform_driver_unregister(&fmgr_driver);
}
