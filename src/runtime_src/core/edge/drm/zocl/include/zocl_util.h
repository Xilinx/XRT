/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_UTIL_H_
#define _ZOCL_UTIL_H_

#include "zocl_lib.h"
#include "kds_core.h"
#include "zocl_error.h"
#include "zynq_ioctl.h"

#define _4KB	0x1000
#define _8KB	0x2000
#define _64KB	0x10000

#define MAX_PR_SLOT_NUM	32
#define MAX_CU_NUM     128
/* Apertures contains both ip and debug ip information */
#define MAX_APT_NUM		2*MAX_CU_NUM
#define EMPTY_APT_VALUE		((phys_addr_t) -1)
#define CU_SIZE        _64KB
#define PR_ISO_SIZE    _4KB

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
#define MEM_BANK_SHIFT_BIT	11
#define	GET_MEM_INDEX(x)	((x) & 0xFFFF)
#define	GET_SLOT_INDEX(x)	(((x) >> MEM_BANK_SHIFT_BIT) & 0x7FF)
#define SET_MEM_INDEX(x, y)	(((x) << MEM_BANK_SHIFT_BIT) | y)

#define ZOCL_GET_ZDEV(ddev) (ddev->dev_private)

struct drm_zocl_mm_stat {
	size_t memory_usage;
	unsigned int bo_count;
};

struct addr_aperture {
	phys_addr_t	addr;
	size_t		size;
	u32		prop;
	int		cu_idx;
	u32		slot_idx;
};

enum zocl_mem_type {
	ZOCL_MEM_TYPE_CMA		= 0,
	ZOCL_MEM_TYPE_RANGE_ALLOC	= 1,
	ZOCL_MEM_TYPE_STREAMING		= 2,
};

/*
 * Memory structure in zocl driver. There will be an array of this
 * structure where each element is representing each section in
 * the memory topology in xclbin.
 */
struct zocl_mem {
	u32			zm_mem_idx;
	enum zocl_mem_type	zm_type;
	unsigned int		zm_used;
	u64			zm_base_addr;
	u64			zm_size;
	struct drm_zocl_mm_stat zm_stat;
	struct list_head	link;
	struct list_head        zm_list;
};

/*
 * zocl dev specific data info, if there are different configs across
 * different compitible device, add their specific data here.
 */
struct zdev_data {
	char fpga_driver_name[64];
};

struct aie_metadata {
	size_t size;
	void *data;
};

struct drm_zocl_slot {
	u32			 slot_idx;
	struct mem_topology	*topology;
	struct ip_layout	*ip;
	struct debug_ip_layout	*debug_ip;
	struct connectivity	*connectivity;
	struct axlf             *axlf;
	size_t                   axlf_size;
	struct aie_metadata	 aie_data;

	u64			 pr_isolation_addr;
	u16			 pr_isolation_freeze;
	u16			 pr_isolation_unfreeze;
	int			 partial_overlay_id;

	int			 ksize;
	char			*kernels;

	struct zocl_xclbin	*slot_xclbin;
	struct mutex		 slot_xclbin_lock;
};

struct drm_zocl_dev {
	struct drm_device       *ddev;
	struct fpga_manager     *fpga_mgr;
	struct zocl_ert_dev     *ert;
	struct iommu_domain	*domain;
	phys_addr_t              host_mem;
	resource_size_t          host_mem_len;
	/* Record start address, this is only for MPSoC as PCIe platform */
	phys_addr_t		 res_start;
	unsigned int		 cu_num;
	unsigned int             irq[MAX_CU_NUM];
	struct sched_exec_core  *exec;
	/* Zocl driver memory list head */
	struct list_head	 zm_list_head;
	struct drm_mm           *zm_drm_mm;    /* DRM MM node for PL-DDR */
	struct mutex		 mm_lock;
	struct mutex		 aie_lock;

	struct list_head	 ctx_list;

	struct addr_aperture	*apertures;
	unsigned int		 num_apts;

	struct kds_sched	 kds;
	struct platform_device	*cu_pldev[MAX_CU_NUM];

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
	struct aie_info		*aie_information;
	struct dma_chan		*zdev_dma_chan;
	struct mailbox		*zdev_mailbox;
	const struct zdev_data	*zdev_data_info;
	struct generic_cu	*generic_cu;
	struct zocl_error	 zdev_error;
	struct zocl_aie		*aie;
	struct zocl_watchdog_dev *watchdog;

	int			 num_pr_slot;
	int			 full_overlay_id;
	struct drm_zocl_slot	*pr_slot[MAX_PR_SLOT_NUM];
};

int zocl_kds_update(struct drm_zocl_dev *zdev, struct drm_zocl_slot *slot,
		    struct drm_zocl_kds *cfg);
#endif
