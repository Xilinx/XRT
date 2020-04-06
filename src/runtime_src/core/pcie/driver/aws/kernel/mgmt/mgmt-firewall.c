/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *
 *  Utility Functions for AXI firewall IP.
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

bool check_axi_firewall(struct awsmgmt_dev *lro) {
	u32 value;
	value = ioread32(lro->bar[AWSMGMT_MAIN_BAR] + FIREWALL_COUNT);
	if (value != lro->firewall_count) {
		printk(KERN_INFO "firewall count increased by %d",
		    value - lro->firewall_count);
		lro->firewall_count = value;
	}

	return true;
}

void init_firewall(struct awsmgmt_dev *lro) {
	lro->firewall_count = ioread32(lro->bar[AWSMGMT_MAIN_BAR] +
	    FIREWALL_COUNT);
}
