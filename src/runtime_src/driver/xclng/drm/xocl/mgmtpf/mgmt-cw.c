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
	struct clock_freq_topology *topology = 0;
	int num_clocks = 0;
	int i = 0;

	mgmt_info(lro, "ocl_freqscaling_ioctl called");

	if (copy_from_user((void *)&freq_obj, arg,
		sizeof(struct xclmgmt_ioc_freqscaling)))
		return -EFAULT;

	topology = (struct clock_freq_topology*) xocl_icap_ocl_get_clock_freq_topology(lro);
	if(topology) {
		num_clocks = topology->m_count;
		mgmt_info(lro, "Num clocks is %d", num_clocks);
		for(i = 0; i < ARRAY_SIZE(freq_obj.ocl_target_freq); i++) {
			mgmt_info(lro, "requested frequency is : "
				"%d xclbin freq is: %d",
				freq_obj.ocl_target_freq[i],
				topology-> m_clock_freq[i].m_freq_Mhz);
			if(freq_obj.ocl_target_freq[i] >
				topology-> m_clock_freq[i].m_freq_Mhz) {
				mgmt_err(lro, "Unable to set frequency as "
					"requested frequency %d is greater "
					"than set by xclbin %d",
					freq_obj.ocl_target_freq[i],
					topology-> m_clock_freq[i].m_freq_Mhz);
			    return -EDOM;
			}
		}
	}

	mgmt_info(lro, "xocl_icap_ocl_set_freq about to be called");

	return xocl_icap_ocl_set_freq(lro, 0, freq_obj.ocl_target_freq,
		ARRAY_SIZE(freq_obj.ocl_target_freq));
}

void fill_frequency_info(struct xclmgmt_dev *lro, struct xclmgmt_ioc_info *obj)
{
	(void) xocl_icap_ocl_get_freq(lro, 0, obj->ocl_frequency,
		ARRAY_SIZE(obj->ocl_frequency));
}
