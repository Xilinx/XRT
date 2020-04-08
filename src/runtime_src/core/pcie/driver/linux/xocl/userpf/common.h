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
#include "kds_core.h"

#define XOCL_DRIVER_DESC        "Xilinx PCIe Accelerator Device Manager"
#define XOCL_DRIVER_DATE        "20180612"
#define XOCL_DRIVER_MAJOR       2018
#define XOCL_DRIVER_MINOR       2
#define XOCL_DRIVER_PATCHLEVEL  8

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

#define	XOCL_RESET_DELAY		2000
#define	XOCL_PROGRAM_SHELL_DELAY	2000

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

/*
 * P2P Linux kernel API has gone through changes over the time. We are trying
 * to maintain our driver compabile w/ all kernels we support here.
 */
#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#define P2P_API_V0
#elif KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
#define P2P_API_V1
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
#define P2P_API_V2
#elif defined(RHEL_RELEASE_VERSION) /* CentOS/RedHat specific check */

#if RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(7, 3) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 6)
#define P2P_API_V1
#elif RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 6)
#define P2P_API_V2
#endif

#endif

/* Each P2P chunk we set up must be at least 256MB */
#define XOCL_P2P_CHUNK_SHIFT		28
#define XOCL_P2P_CHUNK_SIZE		(1ULL << XOCL_P2P_CHUNK_SHIFT)

struct xocl_p2p_mem_chunk {
	struct xocl_dev		*xpmc_xdev;
	void			*xpmc_res_grp;
	void __iomem		*xpmc_va;
	resource_size_t		xpmc_pa;
	resource_size_t		xpmc_size;
	int			xpmc_ref;

	/* Used by kernel API */
	struct percpu_ref	xpmc_percpu_ref;
	struct completion	xpmc_comp;
#ifdef	P2P_API_V2
	struct dev_pagemap	xpmc_pgmap;
#endif
};

enum {
	XOCL_FLAGS_SYSFS_INITIALIZED = (1 << 0),
	XOCL_FLAGS_PERSIST_SYSFS_INITIALIZED = (1 << 1),
};

struct xocl_dev	{
	struct xocl_dev_core	core;

	bool			offline;

	int			p2p_bar_idx;
	u64			p2p_bar_sz_cached;
	resource_size_t		p2p_bar_len;
	struct mutex		p2p_mem_chunk_lock;
	int			p2p_mem_chunk_num;
	struct xocl_p2p_mem_chunk *p2p_mem_chunks;

	struct list_head	ctx_list;
	struct workqueue_struct	*wq;
	struct xocl_work	works[XOCL_WORK_NUM];
	struct mutex		wq_lock;

	/*
	 * Per xdev lock protecting client list and all client contexts in the
	 * list. Any operation which requires client status, such as xclbin
	 * downloading or validating exec buf, should hold this lock.
	 */
	struct mutex		dev_lock;
	unsigned int		needs_reset; /* bool aligned */
	atomic_t		outstanding_execs;
	atomic64_t		total_execs;

	struct xocl_subdev	*dyn_subdev_store;
	int			dyn_subdev_num;

	void			*ulp_blob;

	unsigned int		mbx_offset;

	uint64_t		mig_cache_expire_secs;
	ktime_t			mig_cache_expires;

	u32			flags;
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
	atomic_t		trigger;
	atomic_t                outstanding_execs;
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
int xocl_alloc_cma_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);
int xocl_free_cma_ioctl(struct drm_device *dev, void *data,
	struct drm_file *filp);

/* sysfs functions */
int xocl_init_sysfs(struct xocl_dev *xdev);
void xocl_fini_sysfs(struct xocl_dev *xdev);
int xocl_init_persist_sysfs(struct xocl_dev *xdev);
void xocl_fini_persist_sysfs(struct xocl_dev *xdev);

/* helper functions */
enum {
	XOCL_RESET_FORCE = 1,
	XOCL_RESET_SHUTDOWN = 2,
};
int xocl_hot_reset(struct xocl_dev *xdev, u32 flag);
void xocl_p2p_fini(struct xocl_dev *xdev, bool recov_bar_sz);
int xocl_p2p_init(struct xocl_dev *xdev);
int xocl_p2p_reserve_release_range(struct xocl_dev *xdev,
	resource_size_t off, resource_size_t sz, bool reserve);
int xocl_get_p2p_bar(struct xocl_dev *xdev, u64 *bar_size);
int xocl_pci_resize_resource(struct pci_dev *dev, int resno, int size);
int xocl_pci_rbar_refresh(struct pci_dev *dev, int resno);
void xocl_reset_notify(struct pci_dev *pdev, bool prepare);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
void user_pci_reset_prepare(struct pci_dev *pdev);
void user_pci_reset_done(struct pci_dev *pdev);
#endif

int xocl_refresh_subdevs(struct xocl_dev *xdev);

u32 get_live_clients(struct xocl_dev *xdev, pid_t **pid_list);
void reset_notify_client_ctx(struct xocl_dev *xdev);

void get_pcie_link_info(struct xocl_dev	*xdev,
	unsigned short *link_width, unsigned short *link_speed, bool is_cap);
uint64_t xocl_get_data(struct xocl_dev *xdev, enum data_kind kind);
int xocl_reclock(struct xocl_dev *xdev, void *data);

void xocl_update_mig_cache(struct xocl_dev *xdev);

int xocl_config_pci(struct xocl_dev *xdev);

static inline u64 xocl_pci_rebar_size_to_bytes(int size)
{
	return 1ULL << (size + 20);
}

/* KDS functions */
int xocl_create_client(struct xocl_dev *xdev, void **priv);
void xocl_destroy_client(struct xocl_dev *xdev, void **priv);
int xocl_poll_client(struct xocl_dev *xdev, struct file *filp,
		     poll_table *wait, void *priv);
int xocl_client_ioctl(struct xocl_dev *xdev, int op, void *data,
		      struct drm_file *filp);
int xocl_kds_stop(struct xocl_dev *xdev);
int xocl_kds_reset(struct xocl_dev *xdev, const xuid_t *xclbin_id);
int xocl_kds_reconfig(struct xocl_dev *xdev);
int xocl_cu_map_addr(struct xocl_dev *xdev, u32 cu_idx,
		     void *drm_filp, u32 *addrp);

#endif
