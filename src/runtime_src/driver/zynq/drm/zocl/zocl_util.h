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

#define MAX_CU_NUM 128

#define CLEAR(x) \
	memset(&x, 0, sizeof(x))

#define sizeof_section(sect, data) \
({ \
	size_t ret; \
	size_t data_size; \
	data_size = sect->m_count * sizeof(typeof(sect->data)); \
	ret = (sect) ? offsetof(typeof(*sect), data) + data_size : 0; \
	(ret); \
})

struct drm_zocl_dev {
	struct drm_device       *ddev;
	struct fpga_manager     *fpga_mgr;
	struct zocl_ert_dev     *ert;
	struct iommu_domain     *domain;
	void __iomem            *regs;
	phys_addr_t              res_start;
	resource_size_t          res_len;
	phys_addr_t              host_mem;
	resource_size_t          host_mem_len;
	unsigned int		 cu_num;
	unsigned int             irq[MAX_CU_NUM];
	struct sched_exec_core  *exec;

	struct mem_topology	*topology;
	struct ip_layout	*ip;
	struct debug_ip_layout	*debug_ip;
	struct connectivity	*connectivity;
	u64			 unique_id_last_bitstream;
};

#endif
