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
#include "xocl_drm.h"

#define	XOCL_XDMA_PCI		"xocl_xdma"
#define	XOCL_QDMA_PCI		"xocl_qdma"

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
#if RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(7,4)
#define XOCL_DRM_FREE_MALLOC
#endif
#endif

struct xocl_dev	{
	struct xocl_dev_core	core;

	void * __iomem		base_addr;
	u64			bar_len;
	u32			bar_idx;
	u64     bypass_bar_len;
	u32     bypass_bar_idx;


	void			*dma_handle;
	u32			max_user_intr;
	u32			start_user_intr;
	struct eventfd_ctx      **user_msix_table;
	struct mutex		user_msix_table_lock;

	bool			offline;

	/* memory management */
	struct drm_device	       *ddev;
	/* Memory manager array, one per DDR channel */
	struct drm_mm		       *mm;
	struct mutex			mm_lock;
	struct drm_xocl_mm_stat	       *mm_usage_stat;
	struct mutex			stat_lock;

	struct xocl_mem_topology	topology;
        struct xocl_layout		layout;
        struct xocl_debug_layout	debug_layout;
        struct xocl_connectivity	connectivity;

	/* context table */
	struct xocl_context_hash	ctx_table;

	/* health thread */
	struct task_struct	       *health_thread;
	struct xocl_health_thread_arg	thread_arg;

	void * __iomem bypass_bar_addr;
	/*should be removed after mailbox is supported */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	struct percpu_ref ref;
#endif
	/*should be removed after mailbox is supported */
	u64			        unique_id_last_bitstream;
	/* remove the previous id after we move to uuid */
	xuid_t                          xclbin_id;
	DECLARE_BITMAP                  (cu_exclusive_bitmap, MAX_CUS);
	DECLARE_BITMAP                  (cu_shared_bitmap, MAX_CUS);
	struct list_head                ctx_list;
	struct mutex			ctx_list_lock;
	atomic_t                        needs_reset;
	atomic_t                        outstanding_execs;
	atomic64_t                      total_execs;
};

/**
 * struct client_ctx: Manage user space client attached to device
 *
 * @link: Client context is added to list in device
 * @xclbin_id: UUID for xclbin loaded by client, or nullid if no xclbin loaded
 * @trigger: Poll wait counter for number of completed exec buffers
 * @outstanding_execs: Counter for number outstanding exec buffers
 * @abort: Flag to indicate that this context has detached from user space (ctrl-c)
 * @lock: Mutex lock for exclusive access
 * @cu_bitmap: CUs reserved by this context
 */
struct client_ctx {
	struct list_head	link;
	xuid_t                  xclbin_id;
	atomic_t		trigger;
	atomic_t                outstanding_execs;
	atomic_t                abort;
	struct mutex		lock;
	struct xocl_dev        *xdev;
	DECLARE_BITMAP(cu_bitmap, MAX_CUS);
	struct pid		*pid;
};

struct xocl_qdma_queue {
	unsigned long		dma_handle;
	unsigned long		handle;
	struct mutex		lock;
	u64			flag;
	u32			q_len;
	struct qdma_queue_conf	*qconf;
	struct qdma_sw_sg       *sgl_cache;
};

/* ioctl functions */
int xocl_info_ioctl(struct drm_device *dev,
        void *data, struct drm_file *filp);
int xocl_execbuf_ioctl(struct drm_device *dev,
        void *data, struct drm_file *filp);
int xocl_ctx_ioctl(struct drm_device *dev, void *data,
                   struct drm_file *filp);
int xocl_user_intr_ioctl(struct drm_device *dev, void *data,
                         struct drm_file *filp);
int xocl_read_axlf_ioctl(struct drm_device *dev,
                        void *data,
                        struct drm_file *filp);

/* sysfs functions */
int xocl_init_sysfs(struct device *dev);
void xocl_fini_sysfs(struct device *dev);

ssize_t xocl_mm_sysfs_stat(struct xocl_dev *xdev, char *buf, bool raw);

/* helper functions */
void xocl_reset_notify(struct pci_dev *pdev, bool prepare);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
void user_pci_reset_prepare(struct pci_dev *pdev);
void user_pci_reset_done(struct pci_dev *pdev);
#endif

uint get_live_client_size(struct xocl_dev *xdev);
void reset_notify_client_ctx(struct xocl_dev *xdev);

struct drm_xocl_bo *xocl_create_bo(struct drm_device *dev,
                                          uint64_t unaligned_size,
                                          unsigned user_flags,
                                          unsigned user_type);

/* QDMA functions */
enum {
	XOCL_QDMA_QUEUE_ADDED	= 0x1,
	XOCL_QDMA_QUEUE_STARTED	= 0x2,
	XOCL_QDMA_QUEUE_DONE	= 0x4,
};

int xocl_qdma_queue_create(struct platform_device *pdev,
        struct qdma_queue_conf *qconf, struct xocl_qdma_queue *queue);
int xocl_qdma_queue_destroy(struct platform_device *pdev,
	struct xocl_qdma_queue *queue);
ssize_t xocl_qdma_post_wr(struct platform_device *pdev,
	struct xocl_qdma_queue * queue,
        struct qdma_request *wr, struct sg_table *sgt, off_t off);

void xocl_dump_sgtable(struct device *dev, struct sg_table *sgt);

#endif
