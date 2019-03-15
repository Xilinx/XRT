/*
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 * 		Lizhi Hou <lizhi.hou@xilinx.com>
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _USERPF_COMMON_H
#define	_USERPF_COMMON_H

#include "../xocl_drv.h"
#include "../lib/libqdma/libqdma_export.h"
#include "xocl_bo.h"
#include "../xocl_drm.h"
#include "xocl_ioctl.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#include <linux/hashtable.h>
#endif

#define XOCL_DRIVER_DESC        "Xilinx PCIe Accelerator Device Manager"
#define XOCL_DRIVER_DATE        "20180612"
#define XOCL_DRIVER_MAJOR       2018
#define XOCL_DRIVER_MINOR       2
#define XOCL_DRIVER_PATCHLEVEL  8

#define XOCL_MAX_CONCURRENT_CLIENTS 32

#define XOCL_DRIVER_VERSION                             \
	__stringify(XOCL_DRIVER_MAJOR) "."              \
	__stringify(XOCL_DRIVER_MINOR) "."              \
	__stringify(XOCL_DRIVER_PATCHLEVEL)

#define XOCL_DRIVER_VERSION_NUMBER                              \
	((XOCL_DRIVER_MAJOR)*1000 + (XOCL_DRIVER_MINOR)*100 +   \
	XOCL_DRIVER_PATCHLEVEL)

#define userpf_err(d, args...)                     \
	xocl_err(&XDEV(d)->pdev->dev, ##args)
#define userpf_info(d, args...)                    \
	xocl_info(&XDEV(d)->pdev->dev, ##args)
#define userpf_dbg(d, args...)                     \
	xocl_dbg(&XDEV(d)->pdev->dev, ##args)

#define xocl_get_root_dev(dev, root)		\
	for (root = dev; root->bus && root->bus->self; root = root->bus->self)

#define	XOCL_USER_PROC_HASH_SZ		256

#define XOCL_U32_MASK 0xFFFFFFFF

#define	MAX_SLOTS	128
#define MAX_CUS		128
#define MAX_U32_SLOT_MASKS (((MAX_SLOTS-1)>>5) + 1)
#define MAX_U32_CU_MASKS (((MAX_CUS-1)>>5) + 1)
#define MAX_DEPS        8

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
#define XOCL_DRM_FREE_MALLOC
#elif defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(7, 4)
#define XOCL_DRM_FREE_MALLOC
#endif
#endif

#define XOCL_PA_SECTION_SHIFT		28

struct xocl_dev	{
	struct xocl_dev_core	core;

	bool			offline;

	/* health thread */
	struct task_struct		*health_thread;
	struct xocl_health_thread_arg	thread_arg;

	u32			p2p_bar_idx;
	resource_size_t		p2p_bar_len;
	void __iomem		*p2p_bar_addr;

	/*should be removed after mailbox is supported */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0) || RHEL_P2P_SUPPORT
	struct percpu_ref ref;
	struct completion cmp;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0) || RHEL_P2P_SUPPORT_76
	struct dev_pagemap pgmap;
#endif
	struct list_head                ctx_list;
	struct mutex			ctx_list_lock;
	unsigned int                    needs_reset; /* bool aligned */
	atomic_t                        outstanding_execs;
	atomic64_t                      total_execs;
	void				*p2p_res_grp;
};

/**
 * struct client_ctx: Manage user space client attached to device
 *
 * @link: Client context is added to list in device
 * @trigger: Poll wait counter for number of completed exec buffers
 * @outstanding_execs: Counter for number outstanding exec buffers
 * @abort: Flag to indicate that this context has detached from user space (ctrl-c)
 * @num_cus: Number of resources (CUs) explcitly aquired
 * @lock: Mutex lock for exclusive access
 * @cu_bitmap: CUs reserved by this context, may contain implicit resources
 * @virt_cu_ref: ref count for implicit resources reserved by this context.
 */
struct client_ctx {
	struct list_head	link;
	unsigned int            abort;
	unsigned int            num_cus;
	atomic_t 		trigger;
	atomic_t                outstanding_execs;
	struct mutex		lock;
	struct xocl_dev        *xdev;
	DECLARE_BITMAP		(cu_bitmap, MAX_CUS);
	struct pid             *pid;
	unsigned int		virt_cu_ref;
};
#define	CLIENT_NUM_CU_CTX(client) ((client)->num_cus + (client)->virt_cu_ref)

struct xocl_mm_wrapper {
  struct drm_mm *mm;
  struct drm_xocl_mm_stat *mm_usage_stat;
  uint64_t start_addr;
  uint64_t size;
  uint32_t ddr;
  struct hlist_node node;
};

/* ioctl functions */
int xocl_info_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_execbuf_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_ctx_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_user_intr_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_read_axlf_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_hot_reset_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_reclock_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_sw_mailbox_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);

/* sysfs functions */
int xocl_init_sysfs(struct device *dev);
void xocl_fini_sysfs(struct device *dev);

/* helper functions */
int xocl_hot_reset(struct xocl_dev *xdev, bool force);
void xocl_p2p_mem_release(struct xocl_dev *xdev, bool recov_bar_sz);
int xocl_p2p_mem_reserve(struct xocl_dev *xdev);
int xocl_get_p2p_bar(struct xocl_dev *xdev, u64 *bar_size);
int xocl_pci_resize_resource(struct pci_dev *dev, int resno, int size);
void xocl_reset_notify(struct pci_dev *pdev, bool prepare);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
void user_pci_reset_prepare(struct pci_dev *pdev);
void user_pci_reset_done(struct pci_dev *pdev);
#endif

uint get_live_client_size(struct xocl_dev *xdev);
void reset_notify_client_ctx(struct xocl_dev *xdev);

void get_pcie_link_info(struct xocl_dev	*xdev,
	unsigned short *link_width, unsigned short *link_speed, bool is_cap);
int xocl_reclock(struct xocl_dev *xdev, void *data);
#endif
