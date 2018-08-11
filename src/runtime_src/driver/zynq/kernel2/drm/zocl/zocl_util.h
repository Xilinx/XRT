/*
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
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

#ifndef _ZOCL_UTIL_H_
#define _ZOCL_UTIL_H_

#define zocl_err(dev, fmt, args...)     \
	dev_err(dev, "%s: "fmt, __func__, ##args)
#define zocl_info(dev, fmt, args...)    \
	dev_info(dev, "%s: "fmt, __func__, ##args)
#define zocl_dbg(dev, fmt, args...)     \
	dev_dbg(dev, "%s: "fmt, __func__, ##args)

#define CLEAR(x) \
	memset(&x, 0, sizeof(x))

struct zocl_mem_topology {
	u32                  bank_count;
	struct mem_data      *m_data;
	u32                  m_data_length; /* length of the mem_data section */
	/* Bank size in KB. Only fixed sizes are supported. */
	u64                  bank_size;
	u64                  size;
	struct mem_topology  *topology;
};

struct zocl_connectivity {
	u64                   size;
	struct connectivity  *connections;
};

struct zocl_layout {
	u64                   size;
	struct ip_layout     *layout;
};

struct zocl_debug_layout {
	u64                     size;
	struct debug_ip_layout *layout;
};

struct drm_zocl_dev {
	struct drm_device       *ddev;
	struct fpga_manager     *fpga_mgr;
	struct zocl_ert_dev     *ert;
	struct iommu_domain     *domain;
	void __iomem            *regs;
	phys_addr_t              res_start;
	resource_size_t          res_len;
	unsigned int             irq;
	struct sched_exec_core  *exec;

	struct zocl_mem_topology topology;
	struct zocl_layout       layout;
	struct zocl_debug_layout debug_layout;
	struct zocl_connectivity connectivity;
	u64                      unique_id_last_bitstream;
};

#endif
