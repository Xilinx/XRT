/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: sarabjee@xilinx.com
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
#include <linux/mutex.h>

#include "xocl_drv.h"

static void
xocl_clear_all_error_record(struct xocl_dev *xdev)
{
	struct xocl_error *err = xdev->xocl_errors;
	if (!err)
		return;

	mutex_lock(&xdev->errors_lock);

	memset(err->errors, 0, sizeof(err->errors);
	err->num_err = 0;

	mutex_unlock(&xdev->errors_lock);
}

int
xocl_insert_error_record(struct xocl_dev *xdev, xrtErrorLast *err_last)
{
	struct xocl_error *err = xdev->xocl_errors;
	if (!err)
		return -ENOENT;
	//TODO

	return 0;
}

int
xocl_init_errors(struct xocl_dev *xdev)
{
	//TODO

	return 0;
}

void
xocl_fini_errors(struct xocl_dev *xdev)
{
	struct xocl_error *err = xdev->xocl_errors;
	if (!err)
		return -EINVAL;
	//TODO
}

