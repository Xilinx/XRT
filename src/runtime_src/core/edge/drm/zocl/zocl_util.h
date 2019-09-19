/*
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
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

#define _4KB	0x1000
#define _8KB	0x2000
#define _64KB	0x10000

#define MAX_CU_NUM 128
#define CU_SIZE _64KB

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

/*
 * Get the bank index from BO creation flags.
 * bits  0 ~ 15: DDR BANK index
 */
#define	GET_MEM_BANK(x)		((x) & 0xFFFF)

struct drm_zocl_mm_stat {
	size_t memory_usage;
	unsigned int bo_count;
};

struct addr_aperture {
	phys_addr_t	addr;
	size_t		size;
};

enum zocl_mem_type {
	ZOCL_MEM_TYPE_CMA	= 0,
	ZOCL_MEM_TYPE_PLDDR	= 1,
	ZOCL_MEM_TYPE_STREAMING = 2,
};

/*
 * Memory structure in zocl driver. There will be an array of this
 * structure where each element is representing each section in
 * the memory topology in xclbin.
 */
struct zocl_mem {
	enum zocl_mem_type	zm_type;
	unsigned int		zm_used;
	u64			zm_base_addr;
	u64			zm_size;
	struct drm_zocl_mm_stat zm_stat;
	struct drm_mm          *zm_mm;    /* DRM MM node for PL-DDR */
};

/*
 * zocl dev specific data info, if there are different configs across
 * different compitible device, add their specific data here.
 */
struct zdev_data {
	char fpga_driver_name[64];
};

struct drm_zocl_dev {
	struct drm_device       *ddev;
	struct fpga_manager     *fpga_mgr;
	struct zocl_ert_dev     *ert;
	struct iommu_domain     *domain;
	phys_addr_t              host_mem;
	resource_size_t          host_mem_len;
	/* Record start address, this is only for MPSoC as PCIe platform */
	phys_addr_t		 res_start;
	unsigned int		 cu_num;
	unsigned int             irq[MAX_CU_NUM];
	struct sched_exec_core  *exec;
	unsigned int		 num_mem;
	struct zocl_mem		*mem;
	struct mutex		 mm_lock;

	struct mem_topology	*topology;
	struct ip_layout	*ip;
	struct debug_ip_layout	*debug_ip;
	struct connectivity	*connectivity;
	struct addr_aperture	*apertures;
	unsigned int		 num_apts;
	u64			 unique_id_last_bitstream;

	/*
	 * This RW lock is to protect the sysfs nodes exported
	 * by zocl driver. Currently, all zocl attributes exported
	 * to sysfs nodes are protected by a single lock. Any read
	 * functions which not atomically touch those attributes should
	 * hold read lock; And all write functions which not atomically
	 * touch those attributes should hold write lock.
	 */
	rwlock_t		attr_rwlock;

	struct soft_krnl	*soft_kernel;
	struct dma_chan		*zdev_dma_chan;
	struct mailbox 		*zdev_mailbox;
	const struct zdev_data 	*zdev_data_info;
	u32			pr_isolation_addr;
};

#endif
