/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
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
#ifndef _XOCL_ERRORS_H
#define	_XOCL_ERRORS_H

#include "common.h"
#include "xocl_drv.h"

int
xocl_insert_error_record(struct xocl_dev_core *core, struct xclErrorLast *err_last);

int
xocl_init_errors(struct xocl_dev_core *core);

void
xocl_fini_errors(struct xocl_dev_core *core);

#endif //_XOCL_ERRORS_H
