/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *
 *  Code borrowed from Xilinx SDAccel XDMA driver
 *  Author: Umang Parekh
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "mgmt-core.h"

int ocl_freqscaling_ioctl(struct xclmgmt_dev *lro, const void __user *arg)
{
	struct xclmgmt_ioc_freqscaling freq_obj;

	mgmt_info(lro, "ocl_freqscaling_ioctl called");

	if (copy_from_user((void *)&freq_obj, arg,
		sizeof(struct xclmgmt_ioc_freqscaling)))
		return -EFAULT;

	return xocl_icap_ocl_update_clock_freq_topology(lro, &freq_obj);
}

void fill_frequency_info(struct xclmgmt_dev *lro, struct xclmgmt_ioc_info *obj)
{
	(void) xocl_icap_ocl_get_freq(lro, 0, obj->ocl_frequency,
		ARRAY_SIZE(obj->ocl_frequency));
}
