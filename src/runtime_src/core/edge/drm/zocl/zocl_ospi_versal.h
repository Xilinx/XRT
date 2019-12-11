/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinx.com>
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

#ifndef _ZOCL_OSPI_VERSAL_H_
#define	_ZOCL_OSPI_VERSAL_H_

extern struct platform_driver zocl_ospi_versal_driver;

struct zocl_ov_pkt_node {
	size_t			zn_size;
	u32			*zn_datap;
	struct zocl_ov_pkt_node	*zn_next;
};

/**
 * Main structure of ospi versal subdev.
 * @timer_task:	main thread pointer
 * @base:	PDI packet area base address
 * @size:	PDI packet area size
 * @pdi_ready:	flag to indicate PDI image is ready
 * @pdi_done:	flag to indicate PDI flashing is done
 * @head:	head node of PDI packet linked list
 */
struct zocl_ov_dev {
	struct task_struct	*timer_task;
	void __iomem		*base;
	size_t			size;
	u8			pdi_ready;
	u8			pdi_done;
	rwlock_t		att_rwlock;
	struct zocl_ov_pkt_node	*head;
};

/* Timer thread wake up interval in Millisecond */
#define	ZOCL_OV_TIMER_INTERVAL		(1000)

#define	ZOCL_OSPI_VERSAL_BRAM_RES	0

/* OSPI VERSAL driver name */
#define	ZOCL_OSPI_VERSAL_NAME "zocl_ospi_versal"

int zocl_ov_init_sysfs(struct device *dev);
void zocl_ov_fini_sysfs(struct device *dev);

#endif /* _ZOCL_OSPI_VERSAL_H_ */
