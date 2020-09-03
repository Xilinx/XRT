/*
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@Xilinx.com
 *          Jan Stephan <j.stephan@hzdr.de>
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

#ifndef	_XOCL_DRV_H_
#define	_XOCL_DRV_H_

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 0, 0)
#include <drm/drm_backport.h>
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
#include <drm/drmP.h>
#else
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_drv.h>
#endif
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include "xclbin.h"
#include "xrt_xclbin.h"
#include "xrt_mem.h"
#include "devices.h"
#include "xocl_ioctl.h"
#include "mgmt-ioctl.h"
#include "mailbox_proto.h"
#include <linux/libfdt_env.h>
#include "lib/libfdt/libfdt.h"
#include <linux/firmware.h>
#include "kds_core.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define ioremap_nocache		ioremap
#endif

#ifndef mmiowb
#define mmiowb()		do { } while (0)
#endif

/* The fix for the y2k38 bug was introduced with Linux 3.17 and backported to
 * Red Hat 7.2.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
	#define XOCL_TIMESPEC struct timespec64
	#define XOCL_GETTIME ktime_get_real_ts64
	#define XOCL_USEC tv_nsec / NSEC_PER_USEC
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,2)
		#define XOCL_TIMESPEC struct timespec64
		#define XOCL_GETTIME ktime_get_real_ts64
		#define XOCL_USEC tv_nsec / NSEC_PER_USEC
	#else
		#define XOCL_TIMESPEC struct timeval
		#define XOCL_GETTIME do_gettimeofday
		#define XOCL_USEC tv_usec
	#endif
#else
	#define XOCL_TIMESPEC struct timeval
	#define XOCL_GETTIME do_gettimeofday
	#define XOCL_USEC tv_usec
#endif

/* drm_gem_object_put_unlocked and drm_gem_object_get were introduced with Linux
 * 4.12 and backported to Red Hat 7.5.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
	#define XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_put_unlocked
	#define XOCL_DRM_GEM_OBJECT_GET drm_gem_object_get
#elif defined(RHEL_RELEASE_CODE)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,5)
		#define XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_put_unlocked
		#define XOCL_DRM_GEM_OBJECT_GET drm_gem_object_get
	#else
		#define XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_unreference_unlocked
		#define XOCL_DRM_GEM_OBJECT_GET drm_gem_object_reference
	#endif
#else
	#define XOCL_DRM_GEM_OBJECT_PUT_UNLOCKED drm_gem_object_unreference_unlocked
	#define XOCL_DRM_GEM_OBJECT_GET drm_gem_object_reference
#endif

/* drm_dev_put was introduced with Linux 4.15 and backported to Red Hat 7.6. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)
	#define XOCL_DRM_DEV_PUT drm_dev_put
#elif defined(RHEL_RELEASE_CODE) && !defined(__PPC64__)
	#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,6)
		#define XOCL_DRM_DEV_PUT drm_dev_put
	#else
		#define XOCL_DRM_DEV_PUT drm_dev_unref
	#endif
#else
	#define XOCL_DRM_DEV_PUT drm_dev_unref
#endif

/* access_ok lost its first parameter with Linux 5.0. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,0,0)
	#define XOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(ADDR, SIZE)
#elif defined(RHEL_RELEASE_CODE)
        #if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(8,1)
                #define XOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(ADDR, SIZE)
        #else
                #define XOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(TYPE, ADDR, SIZE)
        #endif
#else
	#define XOCL_ACCESS_OK(TYPE, ADDR, SIZE) access_ok(TYPE, ADDR, SIZE)
#endif

#if defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_CODE <= RHEL_RELEASE_VERSION(7, 4)
#define XOCL_UUID
#endif
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0)
#define XOCL_UUID
#endif
/* UUID helper functions not present in older kernels */
#if defined(XOCL_UUID)
static inline bool uuid_equal(const xuid_t *u1, const xuid_t *u2)
{
	return memcmp(u1, u2, sizeof(xuid_t)) == 0;
}

static inline void uuid_copy(xuid_t *dst, const xuid_t *src)
{
	memcpy(dst, src, sizeof(xuid_t));
}

static inline bool uuid_is_null(const xuid_t *uuid)
{
	xuid_t uuid_null = NULL_UUID_LE;

	return uuid_equal(uuid, &uuid_null);
}
#endif

static inline void xocl_memcpy_fromio(void *buf, void *iomem, u32 size)
{
	int i;

	BUG_ON(size & 0x3);

	for (i = 0; i < size / 4; i++)
		((u32 *)buf)[i] = ioread32((char *)(iomem) + sizeof(u32) * i);
}

static inline void xocl_memcpy_toio(void *iomem, void *buf, u32 size)
{
	int i;

	BUG_ON(size & 0x3);

	for (i = 0; i < size / 4; i++)
		iowrite32(((u32 *)buf)[i], ((char *)(iomem) + sizeof(u32) * i));
}

#define	XOCL_MODULE_NAME	"xocl"
#define	XCLMGMT_MODULE_NAME	"xclmgmt"
#define	ICAP_XCLBIN_V2		"xclbin2"
#define XOCL_CDEV_DIR		"xfpga"

#define XOCL_MAX_DEVICES	24
#define XOCL_EBUF_LEN           512
#define xocl_sysfs_error(xdev, fmt, args...)     \
		snprintf(((struct xocl_dev_core *)xdev)->ebuf, XOCL_EBUF_LEN,	\
		fmt, ##args)
#define MAX_M_COUNT      	XOCL_SUBDEV_MAX_INST
#define XOCL_MAX_FDT_LEN		1024 * 512

#define	XDEV2DEV(xdev)		(&XDEV(xdev)->pdev->dev)

#define PDEV(dev)	(((dev)->bus == &platform_bus_type && (dev)->parent) ? (dev)->parent : (dev))
#define PNAME(dev)	(((dev)->bus == &pci_bus_type) ? "" : dev_name(dev))

#define xocl_err(dev, fmt, args...)			\
	dev_err(PDEV(dev), "%s %llx %s: "fmt, PNAME(dev), (u64)dev, __func__, ##args)
#define xocl_warn(dev, fmt, args...)			\
	dev_warn(PDEV(dev), "%s %llx %s: "fmt, PNAME(dev), (u64)dev, __func__, ##args)
#define xocl_info(dev, fmt, args...)			\
	dev_info(PDEV(dev), "%s %llx %s: "fmt, PNAME(dev), (u64)dev, __func__, ##args)
#define xocl_dbg(dev, fmt, args...)			\
	dev_dbg(PDEV(dev), "%s %llx %s: "fmt, PNAME(dev), (u64)dev, __func__, ##args)

#define xocl_xdev_info(xdev, fmt, args...)		\
	xocl_info(XDEV2DEV(xdev), fmt, ##args)
#define xocl_xdev_err(xdev, fmt, args...)		\
	xocl_err(XDEV2DEV(xdev), fmt, ##args)
#define xocl_xdev_dbg(xdev, fmt, args...)		\
	xocl_dbg(XDEV2DEV(xdev), fmt, ##args)

#define	XOCL_DRV_VER_NUM(ma, mi, p)		\
	((ma) * 1000 + (mi) * 100 + (p))

#define	XOCL_READ_REG32(addr)		\
	ioread32(addr)
#define	XOCL_WRITE_REG32(val, addr)	\
	iowrite32(val, addr)

/* xclbin helpers */
#define sizeof_sect(sect, data) \
({ \
	size_t ret; \
	size_t data_size; \
	data_size = (sect) ? sect->m_count * sizeof(typeof(sect->data)) : 0; \
	ret = (sect) ? offsetof(typeof(*sect), data) + data_size : 0; \
	(ret); \
})

#define	XOCL_PL_TO_PCI_DEV(pldev)		\
	to_pci_dev(pldev->dev.parent)

#define XOCL_PL_DEV_TO_XDEV(pldev) \
	pci_get_drvdata(XOCL_PL_TO_PCI_DEV(pldev))

#define XOCL_PCI_DEV_TO_XDEV(pcidev) \
	pci_get_drvdata(pcidev)

#define XOCL_PCI_FUNC(xdev_hdl)		\
	PCI_FUNC(XDEV(xdev_hdl)->pdev->devfn)

#define	XOCL_QDMA_USER_BAR	2
#define	XOCL_DSA_VERSION(xdev)			\
	(XDEV(xdev)->priv.dsa_ver)

#define XOCL_DSA_IS_MPSOC(xdev)                \
	(XDEV(xdev)->priv.flags & XOCL_DSAFLAG_MPSOC)

#define XOCL_DSA_IS_SMARTN(xdev)                \
	(XDEV(xdev)->priv.flags & XOCL_DSAFLAG_SMARTN)

#define XOCL_DSA_IS_VERSAL(xdev)                \
	(XDEV(xdev)->priv.flags & XOCL_DSAFLAG_VERSAL)

#define	XOCL_DEV_ID(pdev)			\
	((pci_domain_nr(pdev->bus) << 16) |	\
	PCI_DEVID(pdev->bus->number, pdev->devfn))

#define XOCL_ARE_HOP 0x400000000ull

#define XOCL_XILINX_VEN 0x10EE
#define XOCL_ARISTA_VEN 0x3475

#define	XOCL_CHARDEV_REG_COUNT	16

#define INVALID_SUBDEVICE ~0U

#define XOCL_INVALID_MINOR -1

#define	GB(x)			((uint64_t)(x) * 1024 * 1024 * 1024)

#define XOCL_VSEC_UUID_ROM          0x50
#define XOCL_VSEC_FLASH_CONTROLER   0x51
#define XOCL_VSEC_PLATFORM_INFO     0x52
#define XOCL_VSEC_MAILBOX           0x53

#define XOCL_VSEC_FLASH_TYPE_SPI_IP	0x0
#define XOCL_VSEC_FLASH_TYPE_SPI_REG	0x1
#define XOCL_VSEC_FLASH_TYPE_QSPI	0x2
#define XOCL_VSEC_FLASH_TYPE_VERSAL	0x3

#define XOCL_VSEC_PLAT_RECOVERY     0x0
#define XOCL_VSEC_PLAT_1RP          0x1
#define XOCL_VSEC_PLAT_2RP          0x2

#define XOCL_VSEC_ALF_VSEC_ID       0x20

#define XOCL_MAXNAMELEN	64

#define XOCL_VSEC_XLAT_CTL_REG_ADDR             0x188
#define XOCL_VSEC_XLAT_GPA_LOWER_REG_ADDR       0x18C
#define XOCL_VSEC_XLAT_GPA_BASE_UPPER_REG_ADDR  0x190
#define XOCL_VSEC_XLAT_GPA_LIMIT_UPPER_REG_ADDR 0x194
#define XOCL_VSEC_XLAT_VSEC_ID                  0x40

struct xocl_vsec_header {
	u32		format;
	u32		length;
	u32		entry_sz;
	u32		rsvd;
};

extern struct class *xrt_class;

struct drm_xocl_bo;
struct client_ctx;

enum {
	XOCL_SUBDEV_STATE_UNINIT,
	XOCL_SUBDEV_STATE_INIT,
	XOCL_SUBDEV_STATE_ADDED,
	XOCL_SUBDEV_STATE_ATTACHED,
	XOCL_SUBDEV_STATE_OFFLINE,
	XOCL_SUBDEV_STATE_ACTIVE,
};

struct xocl_subdev {
	struct platform_device		*pldev;
	void				*ops;
	int				state;
	struct xocl_subdev_info		info;
	int				inst;
	int				pf;
	struct cdev			*cdev;
	bool				hold;

	struct resource		res[XOCL_SUBDEV_MAX_RES];
	char	res_name[XOCL_SUBDEV_MAX_RES][XOCL_SUBDEV_RES_NAME_LEN];
	char			bar_idx[XOCL_SUBDEV_MAX_RES];
};

#define XOCL_GET_DRV_PRI(pldev)					\
	(platform_get_device_id(pldev) ?				\
	((struct xocl_drv_private *)				\
	platform_get_device_id(pldev)->driver_data) : NULL)


struct xocl_drv_private {
	void			*ops;
	const struct file_operations	*fops;
	dev_t			dev;
	char			*cdev_name;
};

#define	XOCL_GET_SUBDEV_PRIV(dev)				\
	(dev_get_platdata(dev))

typedef	void *xdev_handle_t;

struct xocl_pci_funcs {
	int (*intr_config)(xdev_handle_t xdev, u32 intr, bool enable);
	int (*intr_register)(xdev_handle_t xdev, u32 intr,
		irq_handler_t handler, void *arg);
	int (*reset)(xdev_handle_t xdev);
};

#define	XDEV(dev)	((struct xocl_dev_core *)(dev))
#define	XDEV_PCIOPS(xdev)	(XDEV(xdev)->pci_ops)

#define	xocl_user_interrupt_config(xdev, intr, en)	\
	XDEV_PCIOPS(xdev)->intr_config(xdev, intr, en)
#define	xocl_user_interrupt_reg(xdev, intr, handler, arg)	\
	XDEV_PCIOPS(xdev)->intr_register(xdev, intr, handler, arg)
#define xocl_reset(xdev)			\
	(XDEV_PCIOPS(xdev)->reset ? XDEV_PCIOPS(xdev)->reset(xdev) : \
	-ENODEV)

struct xocl_thread_arg {
	int (*thread_cb)(void *arg);
	void		*arg;
	u32		interval;    /* ms */
	struct device	*dev;
	char		*name;
};

struct xocl_drvinst_proc {
	struct list_head	link;
	u32			pid;
	u32			count;
};

/*
 * Base structure for platform driver data structures
 */
struct xocl_drvinst {
	struct device		*dev;
	u32			size;
	atomic_t		ref;
	struct completion	comp;
	struct list_head	open_procs;
	void			*file_dev;
	bool			offline;
        /*
	 * The derived object placed inline in field "data"
	 * should be aligned at 8 byte boundary
	 */
        u64			data[1];
};

enum {
	XOCL_WORK_RESET,
	XOCL_WORK_PROGRAM_SHELL,
	XOCL_WORK_REFRESH_SUBDEV,
	XOCL_WORK_SHUTDOWN,
	XOCL_WORK_FORCE_RESET,
	XOCL_WORK_ONLINE,
	XOCL_WORK_NUM,
};

struct xocl_work {
	struct delayed_work	work;
	int			op;
};

struct xocl_dev_core {
	struct pci_dev		*pdev;
	int			dev_minor;
	struct xocl_subdev	*subdevs[XOCL_SUBDEV_NUM];
	struct xocl_subdev	*dyn_subdev_store;
	int			dyn_subdev_num;
	struct xocl_pci_funcs	*pci_ops;

	struct mutex 		lock;

	u32			bar_idx;
	void __iomem		*bar_addr;
	resource_size_t		bar_size;
	resource_size_t		feature_rom_offset;

	u32			intr_bar_idx;
	void __iomem		*intr_bar_addr;
	resource_size_t		intr_bar_size;

	struct task_struct      *poll_thread;
	struct xocl_thread_arg thread_arg;

	struct xocl_drm		*drm;

	char			*fdt_blob;
	char			*blp_blob;
	u32			fdt_blob_sz;
	struct xocl_board_private priv;
	char			vbnv_cache[256];

	rwlock_t		rwlock;

	char			ebuf[XOCL_EBUF_LEN + 1];
	bool			shutdown;

	struct workqueue_struct	*wq;
	struct xocl_work	works[XOCL_WORK_NUM];
	struct mutex		wq_lock;

	struct kds_sched	kds;

	spinlock_t		api_lock;
	struct completion	api_comp;
	int			api_call_cnt;

	struct xocl_xclbin 	*xdev_xclbin;

	/*
	 * To cache user space pass down kernel metadata when load xclbin.
	 * Maybe we would have a better place, like fdt. Before that, keep this.
	 */
	int			ksize;
	char			*kernels;
};

#define XOCL_DRM(xdev_hdl)					\
	(((struct xocl_dev_core *)xdev_hdl)->drm)

#define	XOCL_DSA_PCI_RESET_OFF(xdev_hdl)			\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_PCI_RESET_OFF)
#define	XOCL_DSA_MB_SCHE_OFF(xdev_hdl)				\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_MB_SCHE_OFF)
#define	XOCL_DSA_AXILITE_FLUSH_REQUIRED(xdev_hdl)		\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_AXILITE_FLUSH)
#define	XOCL_DSA_NO_KDMA(xdev_hdl)				\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_NO_KDMA)

#define	XOCL_DSA_XPR_ON(xdev_hdl)		\
	(((struct xocl_dev_core *)xdev_hdl)->priv.xpr)


#define	SUBDEV(xdev, id)	\
	(XDEV(xdev)->subdevs[id][0])
#define	SUBDEV_MULTI(xdev, id, idx)	\
	(XDEV(xdev)->subdevs[id][idx])

struct xocl_subdev_funcs {
	int (*offline)(struct platform_device *pdev);
	int (*online)(struct platform_device *pdev);
};

#define offline_cb common_funcs.offline
#define online_cb common_funcs.online

/* rom callbacks */
struct xocl_rom_funcs {
	struct xocl_subdev_funcs common_funcs;
	bool (*is_unified)(struct platform_device *pdev);
	bool (*mb_mgmt_on)(struct platform_device *pdev);
	bool (*mb_sched_on)(struct platform_device *pdev);
	uint32_t* (*cdma_addr)(struct platform_device *pdev);
	u16 (*get_ddr_channel_count)(struct platform_device *pdev);
	u64 (*get_ddr_channel_size)(struct platform_device *pdev);
	bool (*is_are)(struct platform_device *pdev);
	bool (*is_aws)(struct platform_device *pdev);
	bool (*verify_timestamp)(struct platform_device *pdev, u64 timestamp);
	u64 (*get_timestamp)(struct platform_device *pdev);
	int (*get_raw_header)(struct platform_device *pdev, void *header);
	bool (*runtime_clk_scale_on)(struct platform_device *pdev);
	int (*load_firmware)(struct platform_device *pdev, char **fw,
		size_t *fw_size);
	bool (*passthrough_virtualization_on)(struct platform_device *pdev);
	char *(*get_uuid)(struct platform_device *pdev);
};

#define ROM_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_FEATURE_ROM).pldev
#define	ROM_OPS(xdev)	\
	((struct xocl_rom_funcs *)SUBDEV(xdev, XOCL_SUBDEV_FEATURE_ROM).ops)
#define ROM_CB(xdev, cb)	\
	(ROM_DEV(xdev) && ROM_OPS(xdev) && ROM_OPS(xdev)->cb)
#define	xocl_is_unified(xdev)		\
	(ROM_CB(xdev, is_unified) ? ROM_OPS(xdev)->is_unified(ROM_DEV(xdev)) : true)
#define	xocl_mb_mgmt_on(xdev)		\
	(ROM_CB(xdev, mb_mgmt_on) ? ROM_OPS(xdev)->mb_mgmt_on(ROM_DEV(xdev)) : false)
#define	xocl_mb_sched_on(xdev)		\
	(ROM_CB(xdev, mb_sched_on) ? ROM_OPS(xdev)->mb_sched_on(ROM_DEV(xdev)) : false)
#define	xocl_rom_cdma_addr(xdev)		\
	(ROM_CB(xdev, cdma_addr) ? ROM_OPS(xdev)->cdma_addr(ROM_DEV(xdev)) : 0)
#define xocl_clk_scale_on(xdev)		\
	(ROM_CB(xdev, runtime_clk_scale_on) ? ROM_OPS(xdev)->runtime_clk_scale_on(ROM_DEV(xdev)) : false)
#define	xocl_get_ddr_channel_count(xdev) \
	(ROM_CB(xdev, get_ddr_channel_count) ? ROM_OPS(xdev)->get_ddr_channel_count(ROM_DEV(xdev)) :\
	0)
#define	xocl_get_ddr_channel_size(xdev) \
	(ROM_CB(xdev, get_ddr_channel_size) ? ROM_OPS(xdev)->get_ddr_channel_size(ROM_DEV(xdev)) : 0)
#define	xocl_is_are(xdev)		\
	(ROM_CB(xdev, is_are) ? ROM_OPS(xdev)->is_are(ROM_DEV(xdev)) : false)
#define	xocl_is_aws(xdev)		\
	(ROM_CB(xdev, is_aws) ? ROM_OPS(xdev)->is_aws(ROM_DEV(xdev)) : false)
#define	xocl_verify_timestamp(xdev, ts)	\
	(ROM_CB(xdev, verify_timestamp) ? ROM_OPS(xdev)->verify_timestamp(ROM_DEV(xdev), ts) : \
	false)
#define	xocl_get_timestamp(xdev) \
	(ROM_CB(xdev, get_timestamp) ? ROM_OPS(xdev)->get_timestamp(ROM_DEV(xdev)) : 0)
#define	xocl_get_raw_header(xdev, header) \
	(ROM_CB(xdev, get_raw_header) ? ROM_OPS(xdev)->get_raw_header(ROM_DEV(xdev), header) :\
	-ENODEV)
#define xocl_rom_load_firmware(xdev, fw, len)	\
	(ROM_CB(xdev, load_firmware) ?		\
	ROM_OPS(xdev)->load_firmware(ROM_DEV(xdev), fw, len) : -ENODEV)
#define xocl_passthrough_virtualization_on(xdev)		\
	(ROM_CB(xdev, passthrough_virtualization_on) ?		\
	ROM_OPS(xdev)->passthrough_virtualization_on(ROM_DEV(xdev)) : false)
#define xocl_rom_get_uuid(xdev)				\
	(ROM_CB(xdev, get_uuid) ? ROM_OPS(xdev)->get_uuid(ROM_DEV(xdev)) : NULL)

/* version_ctrl callbacks */
struct xocl_version_ctrl_funcs {
	struct xocl_subdev_funcs common_funcs;
	bool (*flat_shell_check)(struct platform_device *pdev);
	bool (*cmc_in_bitfile)(struct platform_device *pdev);
};

#define VC_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_VERSION_CTRL).pldev
#define	VC_OPS(xdev)	\
	((struct xocl_version_ctrl_funcs *)SUBDEV(xdev, XOCL_SUBDEV_VERSION_CTRL).ops)
#define VC_CB(xdev, cb)	\
	(VC_DEV(xdev) && VC_OPS(xdev) && VC_OPS(xdev)->cb)
#define	xocl_flat_shell_check(xdev)		\
	(VC_CB(xdev, flat_shell_check) ? VC_OPS(xdev)->flat_shell_check(VC_DEV(xdev)) : false)
#define	xocl_cmc_in_bitfile(xdev)		\
	(VC_CB(xdev, cmc_in_bitfile) ? VC_OPS(xdev)->cmc_in_bitfile(VC_DEV(xdev)) : false)

struct xocl_msix_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*user_intr_config)(struct platform_device *pdev, u32 intr,
		bool en);
	int (*user_intr_register)(struct platform_device *pdev, u32 intr,
		irq_handler_t handler, void *arg, int event_fd);
	int (*user_intr_unreg)(struct platform_device *pdev, u32 intr);
};
#define MSIX_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_MSIX).pldev
#define	MSIX_OPS(xdev)	\
	((struct xocl_msix_funcs *)SUBDEV(xdev, XOCL_SUBDEV_MSIX).ops)
#define MSIX_CB(xdev, cb)	\
	(MSIX_DEV(xdev) && MSIX_OPS(xdev) && MSIX_OPS(xdev)->cb)
#define xocl_msix_intr_config(xdev, irq, en)			\
	(MSIX_CB(xdev, user_intr_config) ? MSIX_OPS(xdev)->user_intr_config(MSIX_DEV(xdev), \
	irq, en) : -ENODEV)
#define xocl_msix_intr_register(xdev, irq, handler, arg, event_fd)	\
	(MSIX_CB(xdev, user_intr_register) ? MSIX_OPS(xdev)->user_intr_register(MSIX_DEV(xdev), \
	irq, handler, arg, event_fd) : -ENODEV)
#define xocl_msix_intr_unreg(xdev, irq)				\
	(MSIX_CB(xdev, user_intr_unreg) ? MSIX_OPS(xdev)->user_intr_unreg(MSIX_DEV(xdev),	\
	irq) : -ENODEV)

/* dma callbacks */
struct xocl_dma_funcs {
	struct xocl_subdev_funcs common_funcs;
	ssize_t (*migrate_bo)(struct platform_device *pdev,
		struct sg_table *sgt, u32 dir, u64 paddr, u32 channel, u64 sz);
	ssize_t (*async_migrate_bo)(struct platform_device *pdev,
		struct sg_table *sgt, u32 dir, u64 paddr, u32 channel, u64 sz,
		void (*callback_fn)(unsigned long cb_hndl, int err), void *tx_ctx);
	int (*ac_chan)(struct platform_device *pdev, u32 dir);
	void (*rel_chan)(struct platform_device *pdev, u32 dir, u32 channel);
	u32 (*get_chan_count)(struct platform_device *pdev);
	u64 (*get_chan_stat)(struct platform_device *pdev, u32 channel,
		u32 write);
	u64 (*get_str_stat)(struct platform_device *pdev, u32 q_idx);
	int (*user_intr_config)(struct platform_device *pdev, u32 intr, bool en);
	int (*user_intr_register)(struct platform_device *pdev, u32 intr,
					irq_handler_t handler, void *arg, int event_fd);
	int (*user_intr_unreg)(struct platform_device *pdev, u32 intr);
};

#define DMA_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_DMA).pldev
#define	DMA_OPS(xdev)	\
	((struct xocl_dma_funcs *)SUBDEV(xdev, XOCL_SUBDEV_DMA).ops)
#define DMA_CB(xdev, cb)	\
	(DMA_DEV(xdev) && DMA_OPS(xdev) && DMA_OPS(xdev)->cb)
#define	xocl_migrate_bo(xdev, sgt, to_dev, paddr, chan, len)	\
	(DMA_CB(xdev, migrate_bo) ? DMA_OPS(xdev)->migrate_bo(DMA_DEV(xdev), \
	sgt, to_dev, paddr, chan, len) : 0)
#define	xocl_async_migrate_bo(xdev, sgt, to_dev, paddr, chan, len, cb_fn, ctx_ptr)	\
	(DMA_CB(xdev, async_migrate_bo) ? DMA_OPS(xdev)->async_migrate_bo(DMA_DEV(xdev), \
	sgt, to_dev, paddr, chan, len, cb_fn, ctx_ptr) : 0)
#define	xocl_acquire_channel(xdev, dir)		\
	(DMA_CB(xdev, ac_chan) ? DMA_OPS(xdev)->ac_chan(DMA_DEV(xdev), dir) : \
	-ENODEV)
#define	xocl_release_channel(xdev, dir, chan)	\
	(DMA_CB(xdev, rel_chan) ? DMA_OPS(xdev)->rel_chan(DMA_DEV(xdev), dir, \
	chan) : NULL)
#define	xocl_get_chan_count(xdev)		\
	(DMA_CB(xdev, get_chan_count) ? DMA_OPS(xdev)->get_chan_count(DMA_DEV(xdev)) \
	: 0)
#define	xocl_get_chan_stat(xdev, chan, write)		\
	(DMA_CB(xdev, get_chan_stat) ? DMA_OPS(xdev)->get_chan_stat(DMA_DEV(xdev), \
	chan, write) : 0)
#define xocl_dma_intr_config(xdev, irq, en)			\
	(DMA_CB(xdev, user_intr_config) ? DMA_OPS(xdev)->user_intr_config(DMA_DEV(xdev), \
	irq, en) : -ENODEV)
#define xocl_dma_intr_register(xdev, irq, handler, arg, event_fd)	\
	(DMA_CB(xdev, user_intr_register) ? DMA_OPS(xdev)->user_intr_register(DMA_DEV(xdev), \
	irq, handler, arg, event_fd) : -ENODEV)
#define xocl_dma_intr_unreg(xdev, irq)				\
	(DMA_CB(xdev, user_intr_unreg) ? DMA_OPS(xdev)->user_intr_unreg(DMA_DEV(xdev),	\
	irq) : -ENODEV)

/* mb_scheduler callbacks */
struct xocl_mb_scheduler_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*create_client)(struct platform_device *pdev, void **priv);
	void (*destroy_client)(struct platform_device *pdev, void **priv);
	uint (*poll_client)(struct platform_device *pdev, struct file *filp,
		poll_table *wait, void *priv);
	int (*client_ioctl)(struct platform_device *pdev, int op,
		void *data, void *drm_filp);
	int (*stop)(struct platform_device *pdev);
	int (*reset)(struct platform_device *pdev, const xuid_t *xclbin_id);
	int (*reconfig)(struct platform_device *pdev);
	int (*cu_map_addr)(struct platform_device *pdev, u32 cu_index,
		void *drm_filp, u32 *addrp);
};
#define	MB_SCHEDULER_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_MB_SCHEDULER).pldev
#define	MB_SCHEDULER_OPS(xdev)	\
	((struct xocl_mb_scheduler_funcs *)SUBDEV(xdev,		\
		XOCL_SUBDEV_MB_SCHEDULER).ops)
#define SCHE_CB(xdev, cb)	\
	(MB_SCHEDULER_DEV(xdev) && MB_SCHEDULER_OPS(xdev))
#define	xocl_exec_create_client(xdev, priv)		\
	(SCHE_CB(xdev, create_client) ?			\
	MB_SCHEDULER_OPS(xdev)->create_client(MB_SCHEDULER_DEV(xdev), priv) : \
	-ENODEV)
#define	xocl_exec_destroy_client(xdev, priv)		\
	(SCHE_CB(xdev, destroy_client) ?			\
	MB_SCHEDULER_OPS(xdev)->destroy_client(MB_SCHEDULER_DEV(xdev), priv) : \
	NULL)
#define	xocl_exec_poll_client(xdev, filp, wait, priv)		\
	(SCHE_CB(xdev, poll_client) ?				\
	MB_SCHEDULER_OPS(xdev)->poll_client(MB_SCHEDULER_DEV(xdev), filp, \
	wait, priv) : 0)
#define	xocl_exec_client_ioctl(xdev, op, data, drm_filp)		\
	(SCHE_CB(xdev, client_ioctl) ?				\
	MB_SCHEDULER_OPS(xdev)->client_ioctl(MB_SCHEDULER_DEV(xdev),	\
	op, data, drm_filp) : -ENODEV)
#define	xocl_exec_stop(xdev)		\
	(SCHE_CB(xdev, stop) ?				\
	 MB_SCHEDULER_OPS(xdev)->stop(MB_SCHEDULER_DEV(xdev)) : \
	-ENODEV)
#define	xocl_exec_reset(xdev, xclbin_id)		\
	(SCHE_CB(xdev, reset) ?				\
	 MB_SCHEDULER_OPS(xdev)->reset(MB_SCHEDULER_DEV(xdev), xclbin_id) : \
	-ENODEV)
#define	xocl_exec_reconfig(xdev)		\
	(SCHE_CB(xdev, reconfig) ?				\
	 MB_SCHEDULER_OPS(xdev)->reconfig(MB_SCHEDULER_DEV(xdev)) : \
	-ENODEV)
#define	xocl_exec_cu_map_addr(xdev, cu, filep, addrp)		\
	(SCHE_CB(xdev, cu_map_addr) ?				\
	MB_SCHEDULER_OPS(xdev)->cu_map_addr(			\
	MB_SCHEDULER_DEV(xdev), cu, filep, addrp) :		\
	-ENODEV)

/* sysmon callbacks */
enum {
	XOCL_SYSMON_PROP_TEMP,
	XOCL_SYSMON_PROP_TEMP_MAX,
	XOCL_SYSMON_PROP_TEMP_MIN,
	XOCL_SYSMON_PROP_VCC_INT,
	XOCL_SYSMON_PROP_VCC_INT_MAX,
	XOCL_SYSMON_PROP_VCC_INT_MIN,
	XOCL_SYSMON_PROP_VCC_AUX,
	XOCL_SYSMON_PROP_VCC_AUX_MAX,
	XOCL_SYSMON_PROP_VCC_AUX_MIN,
	XOCL_SYSMON_PROP_VCC_BRAM,
	XOCL_SYSMON_PROP_VCC_BRAM_MAX,
	XOCL_SYSMON_PROP_VCC_BRAM_MIN,
};
struct xocl_sysmon_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*get_prop)(struct platform_device *pdev, u32 prop, void *val);
};
#define	SYSMON_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_SYSMON).pldev
#define	SYSMON_OPS(xdev)	\
	((struct xocl_sysmon_funcs *)SUBDEV(xdev,	\
		XOCL_SUBDEV_SYSMON).ops)
#define SYSMON_CB(xdev, cb)	\
	(SYSMON_DEV(xdev) && SYSMON_OPS(xdev) && SYSMON_OPS(xdev)->cb)
#define	xocl_sysmon_get_prop(xdev, prop, val)		\
	(SYSMON_CB(xdev, get_prop) ? SYSMON_OPS(xdev)->get_prop(SYSMON_DEV(xdev), \
	prop, val) : -ENODEV)

/* firewall callbacks */
enum {
	XOCL_AF_PROP_TOTAL_LEVEL,
	XOCL_AF_PROP_STATUS,
	XOCL_AF_PROP_LEVEL,
	XOCL_AF_PROP_DETECTED_STATUS,
	XOCL_AF_PROP_DETECTED_LEVEL,
	XOCL_AF_PROP_DETECTED_TIME,
};
struct xocl_firewall_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*get_prop)(struct platform_device *pdev, u32 prop, void *val);
	int (*clear_firewall)(struct platform_device *pdev);
	u32 (*check_firewall)(struct platform_device *pdev, int *level);
	void (*get_data)(struct platform_device *pdev, void *buf);
};
#define AF_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_AF).pldev
#define	AF_OPS(xdev)	\
	((struct xocl_firewall_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_AF).ops)
#define AF_CB(xdev, cb)	\
	(AF_DEV(xdev) && AF_OPS(xdev) && AF_OPS(xdev)->cb)
#define	xocl_af_get_prop(xdev, prop, val)		\
	(AF_CB(xdev, get_prop) ? AF_OPS(xdev)->get_prop(AF_DEV(xdev), prop, val) : \
	-ENODEV)
#define	xocl_af_check(xdev, level)			\
	(AF_CB(xdev, check_firewall) ? AF_OPS(xdev)->check_firewall(AF_DEV(xdev), level) : 0)
#define	xocl_af_clear(xdev)				\
	(AF_CB(xdev, clear_firewall) ? AF_OPS(xdev)->clear_firewall(AF_DEV(xdev)) : -ENODEV)
#define	xocl_af_get_data(xdev, buf)				\
	(AF_CB(xdev, get_data) ? AF_OPS(xdev)->get_data(AF_DEV(xdev), buf) : -ENODEV)

enum xocl_xmc_flags {
	XOCL_MB_XMC = 0,
	XOCL_MB_ERT,
	XOCL_XMC_FREEZE,
	XOCL_XMC_FREE,
};

/* microblaze callbacks */
struct xocl_mb_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*reset)(struct platform_device *pdev);
	int (*stop)(struct platform_device *pdev);
	int (*load_mgmt_image)(struct platform_device *pdev, const char *buf,
		u32 len);
	int (*load_sche_image)(struct platform_device *pdev, const char *buf,
		u32 len);
	int (*get_data)(struct platform_device *pdev, enum xcl_group_kind kind, void *buf);
	int (*xmc_access)(struct platform_device *pdev, enum xocl_xmc_flags flags);
	void (*clock_status)(struct platform_device *pdev, bool *latched);
};

#define	MB_DEV(xdev)		\
	SUBDEV(xdev, XOCL_SUBDEV_MB).pldev
#define	MB_OPS(xdev)		\
	((struct xocl_mb_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_MB).ops)
#define MB_CB(xdev, cb)	\
	(MB_DEV(xdev) && MB_OPS(xdev) && MB_OPS(xdev)->cb)
#define	xocl_xmc_reset(xdev)			\
	(MB_CB(xdev, reset) ? MB_OPS(xdev)->reset(MB_DEV(xdev)) : -ENODEV) \

#define	xocl_xmc_stop(xdev)			\
	(MB_CB(xdev, stop) ? MB_OPS(xdev)->stop(MB_DEV(xdev)) : -ENODEV)

#define xocl_xmc_load_mgmt_image(xdev, buf, len)		\
	(MB_CB(xdev, load_mgmt_image) ? MB_OPS(xdev)->load_mgmt_image(MB_DEV(xdev), buf, len) :\
	-ENODEV)
#define xocl_xmc_load_sche_image(xdev, buf, len)		\
	(MB_CB(xdev, load_sche_image) ? MB_OPS(xdev)->load_sche_image(MB_DEV(xdev), buf, len) :\
	-ENODEV)

#define xocl_xmc_get_data(xdev, kind, buf)			\
	(MB_CB(xdev, get_data) ? MB_OPS(xdev)->get_data(MB_DEV(xdev), kind, buf) : -ENODEV)

#define xocl_xmc_freeze(xdev)		\
	(MB_CB(xdev, xmc_access) ? MB_OPS(xdev)->xmc_access(MB_DEV(xdev), XOCL_XMC_FREEZE) : -ENODEV)
#define xocl_xmc_free(xdev) 		\
	(MB_CB(xdev, xmc_access) ? MB_OPS(xdev)->xmc_access(MB_DEV(xdev), XOCL_XMC_FREE) : -ENODEV)

#define xocl_xmc_clock_status(xdev, latched)		\
	(MB_CB(xdev, clock_status) ? MB_OPS(xdev)->clock_status(MB_DEV(xdev), latched) : -ENODEV)

/* ERT FW callbacks */
#define ERT_DEV(xdev)							\
	SUBDEV_MULTI(xdev, XOCL_SUBDEV_MB, XOCL_MB_ERT).pldev
#define ERT_OPS(xdev)							\
	((struct xocl_mb_funcs *)SUBDEV_MULTI(xdev,			\
	XOCL_SUBDEV_MB, XOCL_MB_ERT).ops)
#define ERT_CB(xdev, cb)						\
	(ERT_DEV(xdev) && ERT_OPS(xdev) && ERT_OPS(xdev)->cb)
#define xocl_ert_reset(xdev)						\
	(ERT_CB(xdev, reset) ? ERT_OPS(xdev)->reset(ERT_DEV(xdev)) : -ENODEV)
#define xocl_ert_stop(xdev)						\
	(ERT_CB(xdev, stop) ? ERT_OPS(xdev)->stop(ERT_DEV(xdev)) : -ENODEV)
#define xocl_ert_load_sche_image(xdev, buf, len)			\
	(ERT_CB(xdev, load_sche_image) ?				\
	ERT_OPS(xdev)->load_sche_image(ERT_DEV(xdev), buf, len) : -ENODEV)

static inline int xocl_mb_stop(xdev_handle_t xdev)
{
	int ret;

	if (ERT_DEV(xdev)) {
		ret = xocl_ert_stop(xdev);
		if (ret)
			return ret;
	}

	return xocl_xmc_stop(xdev);
}
static inline void xocl_mb_reset(xdev_handle_t xdev)
{
	xocl_ert_reset(xdev);
	xocl_xmc_reset(xdev);
}

#define xocl_mb_load_mgmt_image(xdev, buf, len)				\
	xocl_xmc_load_mgmt_image(xdev, buf, len)
#define xocl_mb_load_sche_image(xdev, buf, len)				\
	(ERT_DEV(xdev) ? xocl_ert_load_sche_image(xdev, buf, len) :	\
	xocl_xmc_load_sche_image(xdev, buf, len))

/* processor system callbacks */
struct xocl_ps_funcs {
	struct xocl_subdev_funcs common_funcs;
	void (*reset)(struct platform_device *pdev, int type);
	void (*wait)(struct platform_device *pdev);
};

#define	PS_DEV(xdev)		\
	SUBDEV(xdev, XOCL_SUBDEV_PS).pldev
#define	PS_OPS(xdev)		\
	((struct xocl_ps_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_PS).ops)
#define PS_CB(xdev, cb)	\
	(PS_DEV(xdev) && PS_OPS(xdev) && PS_OPS(xdev)->cb)
#define	xocl_ps_sk_reset(xdev)			\
	(PS_CB(xdev, reset) ? PS_OPS(xdev)->reset(PS_DEV(xdev), 1) : NULL)
#define	xocl_ps_reset(xdev)			\
	(PS_CB(xdev, reset) ? PS_OPS(xdev)->reset(PS_DEV(xdev), 2) : NULL)
#define	xocl_ps_sys_reset(xdev)			\
	(PS_CB(xdev, reset) ? PS_OPS(xdev)->reset(PS_DEV(xdev), 3) : NULL)
#define	xocl_ps_wait(xdev)			\
	(PS_CB(xdev, reset) ? PS_OPS(xdev)->wait(PS_DEV(xdev)) : NULL)


/* dna callbacks */
struct xocl_dna_funcs {
	struct xocl_subdev_funcs common_funcs;
	u32 (*status)(struct platform_device *pdev);
	u32 (*capability)(struct platform_device *pdev);
	void (*write_cert)(struct platform_device *pdev, const uint32_t *buf, u32 len);
	void (*get_data)(struct platform_device *pdev, void *buf);
};

#define	DNA_DEV(xdev)		\
	SUBDEV(xdev, XOCL_SUBDEV_DNA).pldev
#define	DNA_OPS(xdev)		\
	((struct xocl_dna_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_DNA).ops)
#define DNA_CB(xdev, cb)	\
	(DNA_DEV(xdev) && DNA_OPS(xdev) && DNA_OPS(xdev)->cb)
#define	xocl_dna_status(xdev)			\
	(DNA_CB(xdev, status) ? DNA_OPS(xdev)->status(DNA_DEV(xdev)) : 0)
#define	xocl_dna_capability(xdev)			\
	(DNA_CB(xdev, capability) ? DNA_OPS(xdev)->capability(DNA_DEV(xdev)) : 2)
#define xocl_dna_write_cert(xdev, data, len)  \
	(DNA_CB(xdev, write_cert) ? DNA_OPS(xdev)->write_cert(DNA_DEV(xdev), data, len) : 0)
#define xocl_dna_get_data(xdev, buf)  \
	(DNA_CB(xdev, get_data) ? DNA_OPS(xdev)->get_data(DNA_DEV(xdev), buf) : 0)


#define	ADDR_TRANSLATOR_DEV(xdev)		\
	SUBDEV(xdev, XOCL_SUBDEV_ADDR_TRANSLATOR).pldev
#define	ADDR_TRANSLATOR_OPS(xdev)		\
	((struct xocl_addr_translator_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_ADDR_TRANSLATOR).ops)
#define ADDR_TRANSLATOR_CB(xdev, cb)	\
	(ADDR_TRANSLATOR_DEV(xdev) && ADDR_TRANSLATOR_OPS(xdev) && ADDR_TRANSLATOR_OPS(xdev)->cb)
#define	xocl_addr_translator_get_entries_num(xdev)			\
	(ADDR_TRANSLATOR_CB(xdev, get_entries_num) ? ADDR_TRANSLATOR_OPS(xdev)->get_entries_num(ADDR_TRANSLATOR_DEV(xdev)) : 0)
#define	xocl_addr_translator_set_page_table(xdev, addrs, sz, num)			\
	(ADDR_TRANSLATOR_CB(xdev, set_page_table) ? ADDR_TRANSLATOR_OPS(xdev)->set_page_table(ADDR_TRANSLATOR_DEV(xdev), addrs, sz, num) : -ENODEV)
#define	xocl_addr_translator_get_range(xdev)			\
	(ADDR_TRANSLATOR_CB(xdev, get_range) ? ADDR_TRANSLATOR_OPS(xdev)->get_range(ADDR_TRANSLATOR_DEV(xdev)) : 0)
#define	xocl_addr_translator_get_host_mem_size(xdev)			\
	(ADDR_TRANSLATOR_CB(xdev, get_host_mem_size) ? ADDR_TRANSLATOR_OPS(xdev)->get_host_mem_size(ADDR_TRANSLATOR_DEV(xdev)) : 0)
#define	xocl_addr_translator_enable_remap(xdev, base_addr, range)			\
	(ADDR_TRANSLATOR_CB(xdev, enable_remap) ? ADDR_TRANSLATOR_OPS(xdev)->enable_remap(ADDR_TRANSLATOR_DEV(xdev), base_addr, range) : -ENODEV)
#define	xocl_addr_translator_disable_remap(xdev)			\
	(ADDR_TRANSLATOR_CB(xdev, disable_remap) ? ADDR_TRANSLATOR_OPS(xdev)->disable_remap(ADDR_TRANSLATOR_DEV(xdev)) : -ENODEV)
#define	xocl_addr_translator_clean(xdev)			\
	(ADDR_TRANSLATOR_CB(xdev, clean) ? ADDR_TRANSLATOR_OPS(xdev)->clean(ADDR_TRANSLATOR_DEV(xdev)) : -ENODEV)
#define	xocl_addr_translator_get_base_addr(xdev)			\
	(ADDR_TRANSLATOR_CB(xdev, get_base_addr) ? ADDR_TRANSLATOR_OPS(xdev)->get_base_addr(ADDR_TRANSLATOR_DEV(xdev)) : 0)

struct xocl_addr_translator_funcs {
	struct xocl_subdev_funcs common_funcs;
	u32 (*get_entries_num)(struct platform_device *pdev);
	u64 (*get_range)(struct platform_device *pdev);
	u64 (*get_host_mem_size)(struct platform_device *pdev);
	int (*set_page_table)(struct platform_device *pdev, uint64_t *phys_addrs, uint64_t entry_sz, uint32_t num);
	int (*enable_remap)(struct platform_device *pdev, uint64_t base_addr, uint64_t range);
	int (*disable_remap)(struct platform_device *pdev);
	int (*clean)(struct platform_device *pdev);
	u64 (*get_base_addr)(struct platform_device *pdev);
};

/**
 *	data_kind
 */

enum data_kind {
	MIG_CALIB,
	DIMM0_TEMP,
	DIMM1_TEMP,
	DIMM2_TEMP,
	DIMM3_TEMP,
	FPGA_TEMP,
	CLOCK_FREQ_0,
	CLOCK_FREQ_1,
	FREQ_COUNTER_0,
	FREQ_COUNTER_1,
	VOL_12V_PEX,
	VOL_12V_AUX,
	CUR_12V_PEX,
	CUR_12V_AUX,
	SE98_TEMP0,
	SE98_TEMP1,
	SE98_TEMP2,
	FAN_TEMP,
	FAN_RPM,
	VOL_3V3_PEX,
	VOL_3V3_AUX,
	CUR_3V3_AUX,
	VPP_BTM,
	VPP_TOP,
	VOL_5V5_SYS,
	VOL_1V2_TOP,
	VOL_1V2_BTM,
	VOL_1V8,
	VCC_0V9A,
	VOL_12V_SW,
	VTT_MGTA,
	VOL_VCC_INT,
	CUR_VCC_INT,
	IDCODE,
	IPLAYOUT_AXLF,
	GROUPTOPO_AXLF,
	MEMTOPO_AXLF,
	GROUPCONNECTIVITY_AXLF,
	CONNECTIVITY_AXLF,
	DEBUG_IPLAYOUT_AXLF,
	PEER_CONN,
	XCLBIN_UUID,
	CLOCK_FREQ_2,
	CLOCK_FREQ_3,
	FREQ_COUNTER_2,
	FREQ_COUNTER_3,
	PEER_UUID,
	HBM_TEMP,
	CAGE_TEMP0,
	CAGE_TEMP1,
	CAGE_TEMP2,
	CAGE_TEMP3,
	VCC_0V85,
	SER_NUM,
	MAC_ADDR0,
	MAC_ADDR1,
	MAC_ADDR2,
	MAC_ADDR3,
	REVISION,
	CARD_NAME,
	BMC_VER,
	MAX_PWR,
	FAN_PRESENCE,
	CFG_MODE,
	VOL_VCC_3V3,
	CUR_3V3_PEX,
	CUR_VCC_0V85,
	VOL_HBM_1V2,
	VOL_VPP_2V5,
	VOL_VCCINT_BRAM,
	XMC_VER,
	EXP_BMC_VER,
	XMC_OEM_ID,
	XMC_VCCINT_TEMP,
	XMC_12V_AUX1,
	XMC_VCC1V2_I,
	XMC_V12_IN_I,
	XMC_V12_IN_AUX0_I,
	XMC_V12_IN_AUX1_I,
	XMC_VCCAUX,
	XMC_VCCAUX_PMC,
	XMC_VCCRAM,
	DATA_RETAIN,
};

enum mb_kind {
	DAEMON_STATE,
	CHAN_STATE,
	CHAN_SWITCH,
	COMM_ID,
	VERSION,
};

typedef	void (*mailbox_msg_cb_t)(void *arg, void *data, size_t len,
	u64 msgid, int err, bool sw_ch);
struct xocl_mailbox_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*request)(struct platform_device *pdev, void *req,
		size_t reqlen, void *resp, size_t *resplen,
		mailbox_msg_cb_t cb, void *cbarg, u32 timeout);
	int (*post_notify)(struct platform_device *pdev, void *req, size_t len);
	int (*post_response)(struct platform_device *pdev,
		enum xcl_mailbox_request req, u64 reqid, void *resp, size_t len);
	int (*listen)(struct platform_device *pdev,
		mailbox_msg_cb_t cb, void *cbarg);
	int (*set)(struct platform_device *pdev, enum mb_kind kind, u64 data);
	int (*get)(struct platform_device *pdev, enum mb_kind kind, u64 *data);
};
#define	MAILBOX_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_MAILBOX).pldev
#define	MAILBOX_OPS(xdev)	\
	((struct xocl_mailbox_funcs *)SUBDEV(xdev, XOCL_SUBDEV_MAILBOX).ops)
#define MAILBOX_READY(xdev, cb)	\
	(MAILBOX_DEV(xdev) && MAILBOX_OPS(xdev) && MAILBOX_OPS(xdev)->cb)
#define	xocl_peer_request(xdev, req, reqlen, resp, resplen, cb, cbarg, timeout)	\
	(MAILBOX_READY(xdev, request) ? MAILBOX_OPS(xdev)->request(MAILBOX_DEV(xdev), \
	req, reqlen, resp, resplen, cb, cbarg, timeout) : -ENODEV)
#define	xocl_peer_response(xdev, req, reqid, buf, len)			\
	(MAILBOX_READY(xdev, post_response) ? MAILBOX_OPS(xdev)->post_response(	\
	MAILBOX_DEV(xdev), req, reqid, buf, len) : -ENODEV)
#define	xocl_peer_notify(xdev, req, reqlen)				\
	(MAILBOX_READY(xdev, post_notify) ? MAILBOX_OPS(xdev)->post_notify(		\
	MAILBOX_DEV(xdev), req, reqlen) : -ENODEV)
#define	xocl_peer_listen(xdev, cb, cbarg)				\
	(MAILBOX_READY(xdev, listen) ? MAILBOX_OPS(xdev)->listen(MAILBOX_DEV(xdev), \
	cb, cbarg) : -ENODEV)
#define	xocl_mailbox_set(xdev, kind, data)				\
	(MAILBOX_READY(xdev, set) ? MAILBOX_OPS(xdev)->set(MAILBOX_DEV(xdev), \
	kind, data) : -ENODEV)
#define	xocl_mailbox_get(xdev, kind, data)				\
	(MAILBOX_READY(xdev, get) ? MAILBOX_OPS(xdev)->get(MAILBOX_DEV(xdev), \
	kind, data) : -ENODEV)

struct xocl_clock_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*get_freq)(struct platform_device *pdev, unsigned int region,
		unsigned short *freqs, int num_freqs);
	int (*get_freq_by_id)(struct platform_device *pdev, unsigned int region,
		unsigned short *freq, int id);
	int (*get_freq_counter_khz)(struct platform_device *pdev,
		unsigned int *value, int id);
	int (*freq_rescaling)(struct platform_device *pdev, bool force);
	int (*freq_scaling_by_request)(struct platform_device *pdev,
		unsigned short *freqs, int num_freqs, int verify);
	int (*freq_scaling_by_topo)(struct platform_device *pdev,
		struct clock_freq_topology *topo, int verify);
	int (*clock_status)(struct platform_device *pdev, bool *latched);
	uint64_t (*get_data)(struct platform_device *pdev, enum data_kind kind);
};
#define CLOCK_DEV_INFO(xdev, idx)					\
	SUBDEV_MULTI(xdev, XOCL_SUBDEV_CLOCK, idx).info
#define	CLOCK_DEV(xdev, idx)						\
	SUBDEV_MULTI(xdev, XOCL_SUBDEV_CLOCK, idx).pldev
#define	CLOCK_OPS(xdev, idx)						\
	((struct xocl_clock_funcs *)SUBDEV_MULTI(xdev, XOCL_SUBDEV_CLOCK, idx).ops)
static inline int xocl_clock_ops_level(xdev_handle_t xdev)
{
	int i;
	for (i = XOCL_SUBDEV_LEVEL_MAX - 1; i >= 0; i--) {
		if (CLOCK_OPS(xdev, i))
			return i;
	}

	return -ENODEV;
}

#define CLOCK_CB(xdev, idx, cb)						\
	(idx >= 0 && CLOCK_DEV(xdev, idx) && CLOCK_OPS(xdev, idx) && CLOCK_OPS(xdev, idx)->cb)

#define CLOCK_DEV_LEVEL(xdev) 						\
({ \
	int __idx = xocl_clock_ops_level(xdev);				\
	(__idx >= 0 ? (CLOCK_DEV_INFO(xdev, __idx).level) : -ENODEV); 	\
})

#define	xocl_clock_freq_rescaling(xdev, force)					\
({ \
	int __idx = xocl_clock_ops_level(xdev);					\
	(CLOCK_CB(xdev, __idx, freq_rescaling) ?				\
	CLOCK_OPS(xdev, __idx)->freq_rescaling(CLOCK_DEV(xdev, __idx), force) :	\
	-ENODEV); \
})
#define	xocl_clock_get_freq(xdev, region, freqs, num_freqs)		\
({ \
	int __idx = xocl_clock_ops_level(xdev);				\
	(CLOCK_CB(xdev, __idx, get_freq) ?				\
	CLOCK_OPS(xdev, __idx)->get_freq(CLOCK_DEV(xdev, __idx), region, freqs, num_freqs) : \
	-ENODEV); \
})
#define	xocl_clock_get_freq_by_id(xdev, region, freq, id)		\
({ \
	int __idx = xocl_clock_ops_level(xdev);				\
	(CLOCK_CB(xdev, __idx, get_freq_by_id) ?			\
	CLOCK_OPS(xdev, __idx)->get_freq_by_id(CLOCK_DEV(xdev, __idx), region, freq, id) : \
	-ENODEV); \
})
#define	xocl_clock_get_freq_counter_khz(xdev, value, id)		\
({ \
	int __idx = xocl_clock_ops_level(xdev);				\
	(CLOCK_CB(xdev, __idx, get_freq_counter_khz) ?			\
	CLOCK_OPS(xdev, __idx)->get_freq_counter_khz(CLOCK_DEV(xdev, __idx), value, id) : \
	-ENODEV); \
})
#define	xocl_clock_freq_scaling_by_request(xdev, freqs, num_freqs, verify) \
({ \
	int __idx = xocl_clock_ops_level(xdev);				\
	(CLOCK_CB(xdev, __idx, freq_scaling_by_request) ?		\
	CLOCK_OPS(xdev, __idx)->freq_scaling_by_request(		\
	    CLOCK_DEV(xdev, __idx), freqs, num_freqs, verify) : -ENODEV); \
})
#define	xocl_clock_freq_scaling_by_topo(xdev, topo, verify) \
({ \
	int __idx = xocl_clock_ops_level(xdev);				\
	(CLOCK_CB(xdev, __idx, freq_scaling_by_topo) ?		\
	CLOCK_OPS(xdev, __idx)->freq_scaling_by_topo(		\
	    CLOCK_DEV(xdev, __idx), topo, verify) : -ENODEV); \
})
#define	xocl_clock_status(xdev, latched)				\
({ \
	int __idx = xocl_clock_ops_level(xdev);				\
	(CLOCK_CB(xdev, __idx, clock_status) ?				\
	CLOCK_OPS(xdev, __idx)->clock_status(CLOCK_DEV(xdev, __idx), latched) : 	\
	-ENODEV); \
})
#define	xocl_clock_get_data(xdev, kind)					\
({ \
	int __idx = xocl_clock_ops_level(xdev);				\
	(CLOCK_CB(xdev, __idx, get_data) ?				\
	CLOCK_OPS(xdev, __idx)->get_data(CLOCK_DEV(xdev, __idx), kind) : 0); 	\
})

/* Not a real SC version to indicate that SC image does not exist. */
#define	NONE_BMC_VERSION	"0.0.0"
struct xocl_icap_funcs {
	struct xocl_subdev_funcs common_funcs;
	void (*reset_axi_gate)(struct platform_device *pdev);
	int (*reset_bitstream)(struct platform_device *pdev);
	int (*download_bitstream_axlf)(struct platform_device *pdev,
		const void __user *arg);
	int (*download_boot_firmware)(struct platform_device *pdev);
	int (*download_rp)(struct platform_device *pdev, int level, int flag);
	int (*post_download_rp)(struct platform_device *pdev);
	int (*ocl_set_freq)(struct platform_device *pdev,
		unsigned int region, unsigned short *freqs, int num_freqs);
	int (*ocl_get_freq)(struct platform_device *pdev,
		unsigned int region, unsigned short *freqs, int num_freqs);
	int (*ocl_update_clock_freq_topology)(struct platform_device *pdev, struct xclmgmt_ioc_freqscaling *freqs);
	int (*xclbin_validate_clock_req)(struct platform_device *pdev, struct drm_xocl_reclock_info *freqs);
	int (*ocl_lock_bitstream)(struct platform_device *pdev,
		const xuid_t *uuid);
	int (*ocl_unlock_bitstream)(struct platform_device *pdev,
		const xuid_t *uuid);
	uint64_t (*get_data)(struct platform_device *pdev,
		enum data_kind kind);
	int (*get_xclbin_metadata)(struct platform_device *pdev,
		enum data_kind kind, void **buf);
	void (*put_xclbin_metadata)(struct platform_device *pdev);
	int (*mig_calibration)(struct platform_device *pdev);
	void (*clean_bitstream)(struct platform_device *pdev);
};
enum {
	RP_DOWNLOAD_NORMAL,
	RP_DOWNLOAD_DRY,
	RP_DOWNLOAD_FORCE,
	RP_DOWNLOAD_CLEAR,
};
#define	ICAP_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_ICAP).pldev
#define	ICAP_OPS(xdev)							\
	((struct xocl_icap_funcs *)SUBDEV(xdev, XOCL_SUBDEV_ICAP).ops)
#define ICAP_CB(xdev, cb)						\
	(ICAP_DEV(xdev) && ICAP_OPS(xdev) && ICAP_OPS(xdev)->cb)
#define	xocl_icap_reset_axi_gate(xdev)					\
	(ICAP_CB(xdev, reset_axi_gate) ?				\
	ICAP_OPS(xdev)->reset_axi_gate(ICAP_DEV(xdev)) :		\
	NULL)
#define	xocl_icap_reset_bitstream(xdev)					\
	(ICAP_CB(xdev, reset_bitstream) ?				\
	ICAP_OPS(xdev)->reset_bitstream(ICAP_DEV(xdev)) :		\
	-ENODEV)
#define	xocl_icap_download_axlf(xdev, xclbin)				\
	(ICAP_CB(xdev, download_bitstream_axlf) ?			\
	ICAP_OPS(xdev)->download_bitstream_axlf(ICAP_DEV(xdev), xclbin) : \
	-ENODEV)
#define	xocl_icap_download_boot_firmware(xdev)				\
	(ICAP_CB(xdev, download_boot_firmware) ?			\
	ICAP_OPS(xdev)->download_boot_firmware(ICAP_DEV(xdev)) :	\
	-ENODEV)
#define xocl_icap_download_rp(xdev, level, flag)			\
	(ICAP_CB(xdev, download_rp) ?					\
	ICAP_OPS(xdev)->download_rp(ICAP_DEV(xdev), level, flag) :	\
	-ENODEV)
#define xocl_icap_post_download_rp(xdev)				\
	(ICAP_CB(xdev, post_download_rp) ?				\
	ICAP_OPS(xdev)->post_download_rp(ICAP_DEV(xdev)) :		\
	-ENODEV)
#define	xocl_icap_ocl_get_freq(xdev, region, freqs, num)		\
	(ICAP_CB(xdev, ocl_get_freq) ?					\
	ICAP_OPS(xdev)->ocl_get_freq(ICAP_DEV(xdev), region, freqs, num) : \
	-ENODEV)
#define	xocl_icap_ocl_update_clock_freq_topology(xdev, freqs)		\
	(ICAP_CB(xdev, ocl_update_clock_freq_topology) ?		\
	ICAP_OPS(xdev)->ocl_update_clock_freq_topology(ICAP_DEV(xdev), freqs) :\
	-ENODEV)
#define	xocl_icap_xclbin_validate_clock_req(xdev, freqs)		\
	(ICAP_CB(xdev, xclbin_validate_clock_req) ?			\
	ICAP_OPS(xdev)->xclbin_validate_clock_req(ICAP_DEV(xdev), freqs) :\
	-ENODEV)
#define	xocl_icap_lock_bitstream(xdev, uuid)				\
	(ICAP_CB(xdev, ocl_lock_bitstream) ?				\
	ICAP_OPS(xdev)->ocl_lock_bitstream(ICAP_DEV(xdev), uuid) :	\
	-ENODEV)
#define	xocl_icap_unlock_bitstream(xdev, uuid)				\
	(ICAP_CB(xdev, ocl_unlock_bitstream) ?				\
	ICAP_OPS(xdev)->ocl_unlock_bitstream(ICAP_DEV(xdev), uuid) :	\
	-ENODEV)
#define xocl_icap_refresh_addrs(xdev)					\
	(ICAP_CB(xdev, refresh_addrs) ?					\
	ICAP_OPS(xdev)->refresh_addrs(ICAP_DEV(xdev)) : NULL)
#define	xocl_icap_get_data(xdev, kind)					\
	(ICAP_CB(xdev, get_data) ?					\
	ICAP_OPS(xdev)->get_data(ICAP_DEV(xdev), kind) : 		\
	0)
#define	xocl_icap_get_xclbin_metadata(xdev, kind, buf)			\
	(ICAP_CB(xdev, get_xclbin_metadata) ?				\
	ICAP_OPS(xdev)->get_xclbin_metadata(ICAP_DEV(xdev), kind, buf) :	\
	-ENODEV)
#define	xocl_icap_put_xclbin_metadata(xdev)			\
	(ICAP_CB(xdev, put_xclbin_metadata) ?			\
	ICAP_OPS(xdev)->put_xclbin_metadata(ICAP_DEV(xdev)) : 	\
	0)
#define	xocl_icap_mig_calibration(xdev)				\
	(ICAP_CB(xdev, mig_calibration) ?			\
	ICAP_OPS(xdev)->mig_calibration(ICAP_DEV(xdev)) : 	\
	-ENODEV)
#define	xocl_icap_clean_bitstream(xdev)				\
	(ICAP_CB(xdev, clean_bitstream) ?			\
	ICAP_OPS(xdev)->clean_bitstream(ICAP_DEV(xdev)) : 	\
	-ENODEV)

#define XOCL_GET_MEM_TOPOLOGY(xdev, mem_topo)						\
	(xocl_icap_get_xclbin_metadata(xdev, MEMTOPO_AXLF, (void **)&mem_topo))
#define XOCL_GET_GROUP_TOPOLOGY(xdev, group_topo)					\
	(xocl_icap_get_xclbin_metadata(xdev, GROUPTOPO_AXLF, (void **)&group_topo))
#define XOCL_GET_IP_LAYOUT(xdev, ip_layout)						\
	(xocl_icap_get_xclbin_metadata(xdev, IPLAYOUT_AXLF, (void **)&ip_layout))
#define XOCL_GET_XCLBIN_ID(xdev, xclbin_id)						\
	(xocl_icap_get_xclbin_metadata(xdev, XCLBIN_UUID, (void **)&xclbin_id))


#define XOCL_PUT_MEM_TOPOLOGY(xdev)						\
	xocl_icap_put_xclbin_metadata(xdev)
#define XOCL_PUT_GROUP_TOPOLOGY(xdev)						\
	xocl_icap_put_xclbin_metadata(xdev)
#define XOCL_PUT_IP_LAYOUT(xdev)						\
	xocl_icap_put_xclbin_metadata(xdev)
#define XOCL_PUT_XCLBIN_ID(xdev)						\
	xocl_icap_put_xclbin_metadata(xdev)

#define XOCL_IS_DDR_USED(topo, ddr) 			\
	(topo->m_mem_data[ddr].m_used == 1)

static inline u32 xocl_ddr_count_unified(xdev_handle_t xdev_hdl)
{
	struct mem_topology *topo = NULL;
	uint32_t ret = 0;
	int err = XOCL_GET_GROUP_TOPOLOGY(xdev_hdl, topo);

	if (err)
		return 0;
	ret = topo ? topo->m_count : 0;
	XOCL_PUT_GROUP_TOPOLOGY(xdev_hdl);

	return ret;
}

#define	XOCL_DDR_COUNT(xdev)			\
	(xocl_is_unified(xdev) ? xocl_ddr_count_unified(xdev) :	\
	xocl_get_ddr_channel_count(xdev))
#define XOCL_IS_STREAM(topo, idx)					\
	(topo->m_mem_data[idx].m_type == MEM_STREAMING || \
	 topo->m_mem_data[idx].m_type == MEM_STREAMING_CONNECTION)
#define XOCL_IS_P2P_MEM(topo, idx)					\
	(topo->m_mem_data[idx].m_type == MEM_DDR3 ||			\
	 topo->m_mem_data[idx].m_type == MEM_DDR4 ||			\
	 topo->m_mem_data[idx].m_type == MEM_DRAM ||			\
	 topo->m_mem_data[idx].m_type == MEM_HBM)

struct xocl_mig_label {
	unsigned char		tag[16];
	uint64_t		mem_idx;
	enum MEM_TYPE		mem_type;
};

struct xocl_mig_funcs {
	struct xocl_subdev_funcs common_funcs;
	void (*get_data)(struct platform_device *pdev, void *buf, size_t entry_sz);
	void (*set_data)(struct platform_device *pdev, void *buf);
	uint32_t (*get_id)(struct platform_device *pdev);
};

#define	MIG_DEV(xdev, idx)	SUBDEV_MULTI(xdev, XOCL_SUBDEV_MIG, idx).pldev
#define	MIG_OPS(xdev, idx)							\
	((struct xocl_mig_funcs *)SUBDEV_MULTI(xdev, XOCL_SUBDEV_MIG, idx).ops)
#define	MIG_CB(xdev, idx)	\
	(MIG_DEV(xdev, idx) && MIG_OPS(xdev, idx))
#define	xocl_mig_get_data(xdev, idx, buf, entry_sz)				\
	(MIG_CB(xdev, idx) ?						\
	MIG_OPS(xdev, idx)->get_data(MIG_DEV(xdev, idx), buf, entry_sz) : \
	0)
#define	xocl_mig_set_data(xdev, idx, buf)				\
	(MIG_CB(xdev, idx) ?						\
	MIG_OPS(xdev, idx)->set_data(MIG_DEV(xdev, idx), buf) : \
	0)
#define	xocl_mig_get_id(xdev, idx)				\
	(MIG_CB(xdev, idx) ?						\
	MIG_OPS(xdev, idx)->get_id(MIG_DEV(xdev, idx)) : \
	0)

struct xocl_iores_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*read32)(struct platform_device *pdev, u32 id, u32 off, u32 *val);
	int (*write32)(struct platform_device *pdev, u32 id, u32 off, u32 val);
	void __iomem *(*get_base)(struct platform_device *pdev, u32 id);
	uint64_t (*get_offset)(struct platform_device *pdev, u32 id);
};

#define IORES_DEV(xdev, idx)  SUBDEV_MULTI(xdev, XOCL_SUBDEV_IORES, idx).pldev
#define	IORES_OPS(xdev, idx)						\
	((struct xocl_iores_funcs *)SUBDEV_MULTI(xdev, XOCL_SUBDEV_IORES, idx).ops)
#define IORES_CB(xdev, idx, cb)		\
	(IORES_DEV(xdev, idx) && IORES_OPS(xdev, idx) &&		\
	IORES_OPS(xdev, idx)->cb)
#define	xocl_iores_read32(xdev, level, id, off, val)			\
	(IORES_CB(xdev, level, read32) ?				\
	IORES_OPS(xdev, level)->read32(IORES_DEV(xdev, level), id, off, val) :\
	-ENODEV)
#define	xocl_iores_write32(xdev, level, id, off, val)			\
	(IORES_CB(xdev, level, write32) ?				\
	IORES_OPS(xdev, level)->write32(IORES_DEV(xdev, level), id, off, val) :\
	-ENODEV)
#define __get_base(xdev, level, id)				\
	(IORES_CB(xdev, level, get_base) ?				\
	IORES_OPS(xdev, level)->get_base(IORES_DEV(xdev, level), id) : NULL)
static inline void __iomem *xocl_iores_get_base(xdev_handle_t xdev, int id)
{
	void __iomem *base;
	int i;

	for (i = XOCL_SUBDEV_LEVEL_MAX - 1; i >= 0; i--) {
		base = __get_base(xdev, i, id);
		if (base)
			return base;
	}

	return NULL;
}
#define __get_offset(xdev, level, id)				\
	(IORES_CB(xdev, level, get_offset) ?				\
	IORES_OPS(xdev, level)->get_offset(IORES_DEV(xdev, level), id) : -1)
static inline uint64_t xocl_iores_get_offset(xdev_handle_t xdev, int id)
{
	uint64_t offset;
	int i;

	for (i = XOCL_SUBDEV_LEVEL_MAX - 1; i >= 0; i--) {
		offset = __get_offset(xdev, i, id);
		if (offset != (uint64_t)-1)
			return offset;
	}

	return -1;
}


struct xocl_axigate_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*freeze)(struct platform_device *pdev);
	int (*free)(struct platform_device *pdev);
	int (*reset)(struct platform_device *pdev);
	int (*get_status)(struct platform_device *pdev, u32 *status);
};

#define AXIGATE_DEV(xdev, idx)			\
	SUBDEV_MULTI(xdev, XOCL_SUBDEV_AXIGATE, idx).pldev
#define AXIGATE_OPS(xdev, idx)			\
	((struct xocl_axigate_funcs *)SUBDEV_MULTI(xdev, XOCL_SUBDEV_AXIGATE, \
	idx).ops)
#define AXIGATE_CB(xdev, idx, cb)		\
	(AXIGATE_DEV(xdev, idx) && AXIGATE_OPS(xdev, idx) &&		\
	AXIGATE_OPS(xdev, idx)->cb)
#define xocl_axigate_freeze(xdev, level)		\
	(AXIGATE_CB(xdev, level, freeze) ?		\
	AXIGATE_OPS(xdev, level)->freeze(AXIGATE_DEV(xdev, level)) :	\
	-ENODEV)
#define xocl_axigate_free(xdev, level)		\
	(AXIGATE_CB(xdev, level, free) ?		\
	AXIGATE_OPS(xdev, level)->free(AXIGATE_DEV(xdev, level)) :	\
	-ENODEV)
#define xocl_axigate_reset(xdev, level)		\
	(AXIGATE_CB(xdev, level, reset) ?		\
	AXIGATE_OPS(xdev, level)->reset(AXIGATE_DEV(xdev, level)) :	\
	-ENODEV)
#define xocl_axigate_status(xdev, level, status)		\
	(AXIGATE_CB(xdev, level, get_status) ?		\
	AXIGATE_OPS(xdev, level)->get_status(AXIGATE_DEV(xdev, level), status) :\
	-ENODEV)

struct xocl_mailbox_versal_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*set)(struct platform_device *pdev, u32 data);
	int (*get)(struct platform_device *pdev, u32 *data);
	int (*enable_intr)(struct platform_device *pdev);
	int (*disable_intr)(struct platform_device *pdev);
	int (*handle_intr)(struct platform_device *pdev);
};
#define	MAILBOX_VERSAL_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_MAILBOX_VERSAL).pldev
#define	MAILBOX_VERSAL_OPS(xdev)	\
	((struct xocl_mailbox_versal_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_MAILBOX_VERSAL).ops)
#define MAILBOX_VERSAL_READY(xdev, cb)	\
	(MAILBOX_VERSAL_DEV(xdev) && MAILBOX_VERSAL_OPS(xdev) &&	\
	 MAILBOX_VERSAL_OPS(xdev)->cb)
#define	xocl_mailbox_versal_set(xdev, data)	\
	(MAILBOX_VERSAL_READY(xdev, set) ?	\
	MAILBOX_VERSAL_OPS(xdev)->set(MAILBOX_VERSAL_DEV(xdev), \
	data) : -ENODEV)
#define	xocl_mailbox_versal_get(xdev, data)	\
	(MAILBOX_VERSAL_READY(xdev, get)	\
	? MAILBOX_VERSAL_OPS(xdev)->get(MAILBOX_VERSAL_DEV(xdev), \
	data) : -ENODEV)
#define	xocl_mailbox_versal_enable_intr(xdev)	\
	(MAILBOX_VERSAL_READY(xdev, enable_intr)	\
	? MAILBOX_VERSAL_OPS(xdev)->enable_intr(MAILBOX_VERSAL_DEV(xdev)) : -ENODEV)
#define	xocl_mailbox_versal_disable_intr(xdev)	\
	(MAILBOX_VERSAL_READY(xdev, disable_intr)	\
	? MAILBOX_VERSAL_OPS(xdev)->disable_intr(MAILBOX_VERSAL_DEV(xdev)) : -ENODEV)
#define	xocl_mailbox_versal_handle_intr(xdev)	\
	(MAILBOX_VERSAL_READY(xdev, handle_intr)	\
	? MAILBOX_VERSAL_OPS(xdev)->handle_intr(MAILBOX_VERSAL_DEV(xdev)) : -ENODEV)

/* srsr callbacks */
struct xocl_srsr_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*save_calib)(struct platform_device *pdev);
	int (*calib)(struct platform_device *pdev, bool retain);
	int (*write_calib)(struct platform_device *pdev, const void *calib_cache, uint32_t size);
	int (*read_calib)(struct platform_device *pdev, void *calib_cache, uint32_t size);
	uint32_t (*cache_size)(struct platform_device *pdev);
};

#define	SRSR_DEV(xdev, idx)	SUBDEV_MULTI(xdev, XOCL_SUBDEV_SRSR, idx).pldev
#define	SRSR_OPS(xdev, idx)							\
	((struct xocl_srsr_funcs *)SUBDEV_MULTI(xdev, XOCL_SUBDEV_SRSR, idx).ops)
#define	SRSR_CB(xdev, idx)	\
	(SRSR_DEV(xdev, idx) && SRSR_OPS(xdev, idx))
#define	xocl_srsr_reset(xdev, idx)				\
	(SRSR_CB(xdev, idx) ?						\
	SRSR_OPS(xdev, idx)->reset(SRSR_DEV(xdev, idx)) : \
	-ENODEV)
#define	xocl_srsr_save_calib(xdev, idx)				\
	(SRSR_CB(xdev, idx) ?						\
	SRSR_OPS(xdev, idx)->save_calib(SRSR_DEV(xdev, idx)) : \
	-ENODEV)
#define	xocl_srsr_calib(xdev, idx, retain)				\
	(SRSR_CB(xdev, idx) ?						\
	SRSR_OPS(xdev, idx)->calib(SRSR_DEV(xdev, idx), retain) : \
	-ENODEV)
#define	xocl_srsr_write_calib(xdev, idx, calib_cache, size)				\
	(SRSR_CB(xdev, idx) ?						\
	SRSR_OPS(xdev, idx)->write_calib(SRSR_DEV(xdev, idx), calib_cache, size) : \
	-ENODEV)
#define	xocl_srsr_read_calib(xdev, idx, calib_cache, size)				\
	(SRSR_CB(xdev, idx) ?						\
	SRSR_OPS(xdev, idx)->read_calib(SRSR_DEV(xdev, idx), calib_cache, size) : \
	-ENODEV)
#define	xocl_srsr_cache_size(xdev, idx)				\
	(SRSR_CB(xdev, idx) ?						\
	SRSR_OPS(xdev, idx)->cache_size(SRSR_DEV(xdev, idx)) : 0)

struct calib_storage_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*save)(struct platform_device *pdev);
	int (*restore)(struct platform_device *pdev);
};

#define	CALIB_STORAGE_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_CALIB_STORAGE).pldev
#define	CALIB_STORAGE_OPS(xdev)							\
	((struct calib_storage_funcs *)SUBDEV(xdev, XOCL_SUBDEV_CALIB_STORAGE).ops)
#define	CALIB_STORAGE_CB(xdev)	\
	(CALIB_STORAGE_DEV(xdev) && CALIB_STORAGE_OPS(xdev))
#define	xocl_calib_storage_save(xdev)				\
	(CALIB_STORAGE_CB(xdev) ?						\
	CALIB_STORAGE_OPS(xdev)->save(CALIB_STORAGE_DEV(xdev)) : \
	-ENODEV)
#define	xocl_calib_storage_restore(xdev)				\
	(CALIB_STORAGE_CB(xdev) ?						\
	CALIB_STORAGE_OPS(xdev)->restore(CALIB_STORAGE_DEV(xdev)) : \
	-ENODEV)

/* CU callback */
struct xocl_cu_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*submit)(struct platform_device *pdev, struct kds_command *xcmd);
};
#define CU_DEV(xdev, idx) \
	SUBDEV_MULTI(xdev, XOCL_SUBDEV_CU, idx).pldev
#define CU_OPS(xdev, idx) \
	((struct xocl_cu_funcs *)SUBDEV_MULTI(xdev, XOCL_SUBDEV_CU, idx).ops)
#define CU_CB(xdev, idx, cb) \
	(CU_DEV(xdev, idx) && CU_OPS(xdev, idx) && CU_OPS(xdev, idx)->cb)

/* INTC call back */
enum intc_mode {
	ERT_INTR,
	CU_INTR,
};

struct xocl_intc_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*request_intr)(struct platform_device *pdev, int id,
			    irqreturn_t (*handler)(int irq, void *arg),
			    void *arg, int mode);
	int (*config_intr)(struct platform_device *pdev, int id, bool en, int mode);
	int (*sel_ert_intr)(struct platform_device *pdev, int mode);
	int (*csr_read32)(struct platform_device *pdev, u32 off);
	void (*csr_write32)(struct platform_device *pdev, u32 val, u32 off);
};
#define	INTC_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_INTC).pldev
#define INTC_OPS(xdev)  \
	((struct xocl_intc_funcs *)SUBDEV(xdev, XOCL_SUBDEV_INTC).ops)
#define INTC_CB(xdev, cb) \
	(INTC_DEV(xdev) && INTC_OPS(xdev) && INTC_OPS(xdev)->cb)
#define xocl_intc_ert_request(xdev, id, handler, arg) \
	(INTC_CB(xdev, request_intr) ? \
	 INTC_OPS(xdev)->request_intr(INTC_DEV(xdev), id, handler, arg, ERT_INTR) : \
	 -ENODEV)
#define xocl_intc_ert_config(xdev, id, en) \
	(INTC_CB(xdev, config_intr) ? \
	 INTC_OPS(xdev)->config_intr(INTC_DEV(xdev), id, en, ERT_INTR) : \
	 -ENODEV)
#define xocl_intc_cu_request(xdev, id, handler, arg) \
	(INTC_CB(xdev, request_intr) ? \
	 INTC_OPS(xdev)->request_intr(INTC_DEV(xdev), id, handler, arg, CU_INTR) : \
	 -ENODEV)
#define xocl_intc_cu_config(xdev, id, en) \
	(INTC_CB(xdev, config_intr) ? \
	 INTC_OPS(xdev)->config_intr(INTC_DEV(xdev), id, en, CU_INTR) : \
	 -ENODEV)
#define xocl_intc_set_mode(xdev, mode) \
	(INTC_CB(xdev, sel_ert_intr) ? \
	 INTC_OPS(xdev)->sel_ert_intr(INTC_DEV(xdev), mode) : \
	 -ENODEV)
/* Only used in ERT sub-device polling mode */
#define xocl_intc_ert_read32(xdev, off) \
	(INTC_CB(xdev, csr_read32) ? \
	 INTC_OPS(xdev)->csr_read32(INTC_DEV(xdev), off) : \
	 -ENODEV)
#define xocl_intc_ert_write32(xdev, val, off) \
	(INTC_CB(xdev, csr_write32) ? \
	 INTC_OPS(xdev)->csr_write32(INTC_DEV(xdev), val, off) : \
	 -ENODEV)

enum ert_gpio_cfg {
	INTR_TO_ERT,
	INTR_TO_CU,
	MB_WAKEUP,
	MB_SLEEP,
	MB_STATUS,
};

struct xocl_ert_user_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (* configured)(struct platform_device *pdev);
};
#define	ERT_USER_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_ERT_USER).pldev
#define ERT_USER_OPS(xdev)  \
	((struct xocl_ert_user_funcs *)SUBDEV(xdev, XOCL_SUBDEV_ERT_USER).ops)
#define ERT_USER_CB(xdev, cb)  \
	(ERT_USER_DEV(xdev) && ERT_USER_OPS(xdev) && ERT_USER_OPS(xdev)->cb)

#define xocl_ert_user_configured(xdev) \
	(ERT_USER_CB(xdev, configured) ? \
	 ERT_USER_OPS(xdev)->configured(ERT_USER_DEV(xdev)) : \
	 -ENODEV)

struct xocl_ert_30_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (* configured)(struct platform_device *pdev);
	uint32_t (* gpio_cfg)(struct platform_device *pdev, enum ert_gpio_cfg type);
};
#define	ERT_30_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_ERT_30).pldev
#define ERT_30_OPS(xdev)  \
	((struct xocl_ert_30_funcs *)SUBDEV(xdev, XOCL_SUBDEV_ERT_30).ops)
#define ERT_30_CB(xdev, cb)  \
	(ERT_30_DEV(xdev) && ERT_30_OPS(xdev) && ERT_30_OPS(xdev)->cb)

#define xocl_ert_30_configured(xdev) \
	(ERT_30_CB(xdev, configured) ? \
	 ERT_30_OPS(xdev)->configured(ERT_30_DEV(xdev)) : \
	 -ENODEV)
#define xocl_ert_30_mb_wakeup(xdev) \
	(ERT_30_CB(xdev, gpio_cfg) ? \
	 ERT_30_OPS(xdev)->gpio_cfg(ERT_30_DEV(xdev), MB_WAKEUP) : \
	 -ENODEV)
#define xocl_ert_30_mb_sleep(xdev) \
	(ERT_30_CB(xdev, gpio_cfg) ? \
	 ERT_30_OPS(xdev)->gpio_cfg(ERT_30_DEV(xdev), MB_SLEEP) : \
	 -ENODEV)
#define xocl_ert_30_cu_intr_cfg(xdev) \
	(ERT_30_CB(xdev, gpio_cfg) ? \
	 ERT_30_OPS(xdev)->gpio_cfg(ERT_30_DEV(xdev), INTR_TO_CU) : \
	 -ENODEV)
#define xocl_ert_30_ert_intr_cfg(xdev) \
	(ERT_30_CB(xdev, gpio_cfg) ? \
	 ERT_30_OPS(xdev)->gpio_cfg(ERT_30_DEV(xdev), INTR_TO_ERT) : \
	 -ENODEV)


/* helper functions */
xdev_handle_t xocl_get_xdev(struct platform_device *pdev);
void xocl_init_dsa_priv(xdev_handle_t xdev_hdl);

static inline int xocl_queue_work(xdev_handle_t xdev_hdl, int op, int delay)
{
	struct xocl_dev_core *xdev = XDEV(xdev_hdl);
	int ret = 0;

	mutex_lock(&xdev->wq_lock);
	if (xdev->wq) {
		ret = queue_delayed_work(xdev->wq,
			&xdev->works[op].work, msecs_to_jiffies(delay));
	}
	mutex_unlock(&xdev->wq_lock);

	return ret;
}

static inline void xocl_queue_destroy(xdev_handle_t xdev_hdl)
{
	struct xocl_dev_core *xdev = XDEV(xdev_hdl);
	int i;

	mutex_lock(&xdev->wq_lock);
	if (xdev->wq) {
		for (i = 0; i < XOCL_WORK_NUM; i++) {
			cancel_delayed_work_sync(&xdev->works[i].work);
			flush_delayed_work(&xdev->works[i].work);
		}
		flush_workqueue(xdev->wq);
		destroy_workqueue(xdev->wq);
		xdev->wq = NULL;
	}
	mutex_unlock(&xdev->wq_lock);
}

static inline struct kernel_info *
xocl_query_kernel(xdev_handle_t xdev_hdl, const char *name)
{
	struct xocl_dev_core *xdev = XDEV(xdev_hdl);
	struct kernel_info *kernel;
	int off = 0;

	while (off < xdev->ksize) {
		kernel = (struct kernel_info *)(xdev->kernels + off);
		if (!strcmp(kernel->name, name))
			break;
		off += sizeof(struct kernel_info);
		off += sizeof(struct argument_info) * kernel->anums;
	}

	if (off < xdev->ksize)
		return kernel;

	return NULL;
}

struct xocl_flash_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*read)(struct platform_device *pdev,
		char *buf, size_t n, loff_t off);
	int (*get_size)(struct platform_device *pdev, size_t *size);
};
#define	FLASH_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_FLASH).pldev
#define	FLASH_OPS(xdev)				\
	((struct xocl_flash_funcs *)SUBDEV(xdev, XOCL_SUBDEV_FLASH).ops)
#define	FLASH_CB(xdev)	(FLASH_DEV(xdev) && FLASH_OPS(xdev))
#define	xocl_flash_read(xdev, buf, n, off)	\
	(FLASH_CB(xdev) ?			\
	FLASH_OPS(xdev)->read(FLASH_DEV(xdev), buf, n, off) : -ENODEV)
#define	xocl_flash_get_size(xdev, size)		\
	(FLASH_CB(xdev) ?			\
	FLASH_OPS(xdev)->get_size(FLASH_DEV(xdev), size) : -ENODEV)

struct xocl_xfer_versal_funcs {
	int (*download_axlf)(struct platform_device *pdev,
		const void __user *arg);
};
#define	XFER_VERSAL_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_XFER_VERSAL).pldev
#define	XFER_VERSAL_OPS(xdev)					\
	((struct xocl_xfer_versal_funcs *)SUBDEV(xdev, XOCL_SUBDEV_XFER_VERSAL).ops)
#define	XFER_VERSAL_CB(xdev)	(XFER_VERSAL_DEV(xdev) && XFER_VERSAL_OPS(xdev))
#define	xocl_xfer_versal_download_axlf(xdev, xclbin)	\
	(XFER_VERSAL_CB(xdev) ?					\
	XFER_VERSAL_OPS(xdev)->download_axlf(XFER_VERSAL_DEV(xdev), xclbin) : -ENODEV)

struct xocl_pmc_funcs {
	int (*enable_reset)(struct platform_device *pdev);
};
#define	PMC_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_PMC).pldev
#define	PMC_OPS(xdev) \
	((struct xocl_pmc_funcs *)SUBDEV(xdev, XOCL_SUBDEV_PMC).ops)
#define	PMC_CB(xdev)	(PMC_DEV(xdev) && PMC_OPS(xdev))
#define	xocl_pmc_enable_reset(xdev) \
	(PMC_CB(xdev) ? PMC_OPS(xdev)->enable_reset(PMC_DEV(xdev)) : -ENODEV)

/* subdev mbx messages */
#define XOCL_MSG_SUBDEV_VER	1
#define XOCL_MSG_SUBDEV_DATA_LEN	(512 * 1024)

enum {
	XOCL_MSG_SUBDEV_RTN_UNCHANGED = 1,
	XOCL_MSG_SUBDEV_RTN_PARTIAL,
	XOCL_MSG_SUBDEV_RTN_COMPLETE,
	XOCL_MSG_SUBDEV_RTN_PENDINGPLP,
};

struct xocl_p2p_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*mem_map)(struct platform_device *pdev, ulong bank_addr,
			ulong bank_size, ulong offset, ulong len,
			ulong *bar_off);
	int (*mem_unmap)(struct platform_device *pdev, ulong bar_off,
			ulong len);
	int (*mem_init)(struct platform_device *pdev);
	int (*mem_cleanup)(struct platform_device *pdev);
	int (*mem_get_pages)(struct platform_device *pdev,
			ulong bar_off, ulong size,
			struct page **pages, ulong npages);
	int (*remap_resource)(struct platform_device *pdev, int bar_idx,
			struct resource *res, int level);
	int (*release_resource)(struct platform_device *pdev,
			struct resource *res);
};
#define	P2P_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_P2P).pldev
#define	P2P_OPS(xdev)				\
	((struct xocl_p2p_funcs *)SUBDEV(xdev, XOCL_SUBDEV_P2P).ops)
#define	P2P_CB(xdev)	(P2P_DEV(xdev) && P2P_OPS(xdev))
#define	xocl_p2p_mem_map(xdev, ba, bs, off, len, bar_off)	\
	(P2P_CB(xdev) ?			\
	P2P_OPS(xdev)->mem_map(P2P_DEV(xdev), ba, bs, off, len, bar_off) : \
	-ENODEV)
#define xocl_p2p_mem_unmap(xdev, bar_off, len)				\
	(P2P_CB(xdev) ?							\
	 P2P_OPS(xdev)->mem_unmap(P2P_DEV(xdev), bar_off, len) : -ENODEV)
#define xocl_p2p_mem_init(xdev)						\
	(P2P_CB(xdev) ?							\
	 P2P_OPS(xdev)->mem_init(P2P_DEV(xdev)) : -ENODEV)
#define xocl_p2p_mem_cleanup(xdev)						\
	(P2P_CB(xdev) ?							\
	 P2P_OPS(xdev)->mem_cleanup(P2P_DEV(xdev)) : -ENODEV)
#define xocl_p2p_mem_get_pages(xdev, bar_off, len, pages, npages)	\
	(P2P_CB(xdev) ?							\
	 P2P_OPS(xdev)->mem_get_pages(P2P_DEV(xdev), bar_off, len,	\
	 pages, npages) : -ENODEV)
#define xocl_p2p_remap_resource(xdev, bar, res, level)			\
	(P2P_CB(xdev) ?							\
	 P2P_OPS(xdev)->remap_resource(P2P_DEV(xdev), bar, res, level) : \
	 -ENODEV)
#define xocl_p2p_release_resource(xdev, res)				\
	(P2P_CB(xdev) ?							\
	 P2P_OPS(xdev)->release_resource(P2P_DEV(xdev), res) : -ENODEV)

/* Each P2P chunk we set up must be at least 256MB */
#define XOCL_P2P_CHUNK_SHIFT		28
#define XOCL_P2P_CHUNK_SIZE		(1UL << XOCL_P2P_CHUNK_SHIFT)

struct xocl_m2m_funcs {
	struct xocl_subdev_funcs common_funcs;
	int (*copy_bo)(struct platform_device *pdev, uint64_t src_paddr,
		uint64_t dst_paddr, uint32_t src_handle, uint32_t dst_handle,
		uint32_t size);
};
#define	M2M_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_M2M).pldev
#define	M2M_OPS(xdev)	\
	((struct xocl_m2m_funcs *)SUBDEV(xdev, XOCL_SUBDEV_M2M).ops)
#define	M2M_CB(xdev)	(M2M_DEV(xdev) && M2M_OPS(xdev))
#define	xocl_m2m_copy_bo(xdev, src_paddr, dst_paddr, src_handle, dst_handle, size) \
	(M2M_CB(xdev) ? M2M_OPS(xdev)->copy_bo(M2M_DEV(xdev), src_paddr, dst_paddr, \
	src_handle, dst_handle, size) : -ENODEV)

/* subdev functions */
int xocl_subdev_init(xdev_handle_t xdev_hdl, struct pci_dev *pdev,
	struct xocl_pci_funcs *pci_ops);
void xocl_subdev_fini(xdev_handle_t xdev_hdl);
int xocl_subdev_create(xdev_handle_t xdev_hdl,
	struct xocl_subdev_info *sdev_info);
int xocl_subdev_create_by_id(xdev_handle_t xdev_hdl, int id);
int xocl_subdev_create_by_level(xdev_handle_t xdev_hdl, int level);
int xocl_subdev_create_all(xdev_handle_t xdev_hdl);
void xocl_subdev_destroy_all(xdev_handle_t xdev_hdl);
int xocl_subdev_offline_all(xdev_handle_t xdev_hdl);
int xocl_subdev_offline_by_id(xdev_handle_t xdev_hdl, u32 id);
int xocl_subdev_offline_by_level(xdev_handle_t xdev_hdl, int level);
int xocl_subdev_online_all(xdev_handle_t xdev_hdl);
int xocl_subdev_online_by_id(xdev_handle_t xdev_hdl, u32 id);
int xocl_subdev_online_by_id_and_inst(xdev_handle_t xdev_hdl, u32 id, u32 inst_id);
int xocl_subdev_online_by_level(xdev_handle_t xdev_hdl, int level);
void xocl_subdev_destroy_by_id(xdev_handle_t xdev_hdl, u32 id);
void xocl_subdev_destroy_by_level(xdev_handle_t xdev_hdl, int level);

int xocl_subdev_create_by_name(xdev_handle_t xdev_hdl, char *name);
int xocl_subdev_destroy_by_name(xdev_handle_t xdev_hdl, char *name);

int xocl_subdev_create_by_baridx(xdev_handle_t xdev_hdl, int bar_idx);
void xocl_subdev_destroy_by_baridx(xdev_handle_t xdev_hdl, int bar_idx);

int xocl_subdev_destroy_prp(xdev_handle_t xdev);
int xocl_subdev_create_prp(xdev_handle_t xdev);

int xocl_subdev_vsec(xdev_handle_t xdev, u32 type, int *bar_idx, u64 *offset,
	u32 *verType);
u32 xocl_subdev_vsec_read32(xdev_handle_t xdev, int bar, u64 offset);
int xocl_subdev_create_vsec_devs(xdev_handle_t xdev);
bool xocl_subdev_is_vsec(xdev_handle_t xdev);
int xocl_subdev_get_level(struct platform_device *pdev);

void xocl_subdev_register(struct platform_device *pldev, void *ops);
void xocl_subdev_unregister(struct platform_device *pldev);

int xocl_subdev_get_resource(xdev_handle_t xdev_hdl,
		char *res_name, u32 type, struct resource *res);

void xocl_fill_dsa_priv(xdev_handle_t xdev_hdl, struct xocl_board_private *in);
int xocl_xrt_version_check(xdev_handle_t xdev_hdl,
	struct axlf *bin_obj, bool major_only);
int xocl_alloc_dev_minor(xdev_handle_t xdev_hdl);
void xocl_free_dev_minor(xdev_handle_t xdev_hdl);

struct resource *xocl_get_iores_byname(struct platform_device *pdev,
				       char *name);
int xocl_get_irq_byname(struct platform_device *pdev, char *name);
void __iomem *xocl_devm_ioremap_res(struct platform_device *pdev, int index);
void __iomem *xocl_devm_ioremap_res_byname(struct platform_device *pdev,
					   const char *name);

int xocl_ioaddr_to_baroff(xdev_handle_t xdev_hdl, resource_size_t io_addr,
	int *bar_idx, resource_size_t *bar_off);
int xocl_wait_pci_status(struct pci_dev *pdev, u16 mask, u16 val, int timeout);

static inline void xocl_lock_xdev(xdev_handle_t xdev)
{
	mutex_lock(&XDEV(xdev)->lock);
}

static inline void xocl_unlock_xdev(xdev_handle_t xdev)
{
	mutex_unlock(&XDEV(xdev)->lock);
}

static inline uint32_t xocl_dr_reg_read32(xdev_handle_t xdev, void __iomem *addr)
{
	u32 val;

	read_lock(&XDEV(xdev)->rwlock);
	val = ioread32(addr);
	read_unlock(&XDEV(xdev)->rwlock);

	return val;
}

static inline void xocl_dr_reg_write32(xdev_handle_t xdev, u32 value, void __iomem *addr)
{
	read_lock(&XDEV(xdev)->rwlock);
	iowrite32(value, addr);
	read_unlock(&XDEV(xdev)->rwlock);
}

/* Unify KDS wrappers */
static inline int xocl_kds_add_cu(xdev_handle_t xdev, struct xrt_cu *xcu)
{
	return kds_add_cu(&XDEV(xdev)->kds, xcu);
}

static inline int xocl_kds_del_cu(xdev_handle_t xdev, struct xrt_cu *xcu)
{
	return kds_del_cu(&XDEV(xdev)->kds, xcu);
}

static inline int xocl_kds_init_ert(xdev_handle_t xdev, struct kds_ert *ert)
{
	return kds_init_ert(&XDEV(xdev)->kds, ert);
}

static inline int xocl_kds_fini_ert(xdev_handle_t xdev)
{
	return kds_fini_ert(&XDEV(xdev)->kds);
}

/* context helpers */
extern struct mutex xocl_drvinst_mutex;
extern struct xocl_drvinst *xocl_drvinst_array[XOCL_MAX_DEVICES * 10];

void *xocl_drvinst_alloc(struct device *dev, u32 size);
void xocl_drvinst_release(void *data, void **hdl);
static inline void xocl_drvinst_free(void *hdl) {
	kfree(hdl);
}
void *xocl_drvinst_open(void *file_dev);
void *xocl_drvinst_open_single(void *file_dev);
void xocl_drvinst_close(void *data);
void xocl_drvinst_set_filedev(void *data, void *file_dev);
void xocl_drvinst_offline(xdev_handle_t xdev_hdl, bool offline);
int xocl_drvinst_set_offline(void *data, bool offline);
int xocl_drvinst_get_offline(void *data, bool *offline);
int xocl_drvinst_kill_proc(void *data);

/* health thread functions */
int xocl_thread_start(xdev_handle_t xdev);
int xocl_thread_stop(xdev_handle_t xdev);

/* subdev blob functions */
int xocl_fdt_blob_input(xdev_handle_t xdev_hdl, char *blob, u32 blob_sz,
		int part_level, char *vbnv);
int xocl_fdt_remove_subdevs(xdev_handle_t xdev_hdl, struct list_head *devlist);
int xocl_fdt_unlink_node(xdev_handle_t xdev_hdl, void *node);
int xocl_fdt_overlay(void *fdt, int target, void *fdto, int node, int pf,
		int part_level);
int xocl_fdt_build_priv_data(xdev_handle_t xdev_hdl, struct xocl_subdev *subdev,
		void **priv_data,  size_t *data_len);
int xocl_fdt_get_userpf(xdev_handle_t xdev_hdl, void *blob);
int xocl_fdt_get_p2pbar(xdev_handle_t xdev_hdl, void *blob);
long xocl_fdt_get_p2pbar_len(xdev_handle_t xdev_hdl, void *blob);
int xocl_fdt_add_pair(xdev_handle_t xdev_hdl, void *blob, char *name,
		void *val, int size);
int xocl_fdt_get_next_prop_by_name(xdev_handle_t xdev_hdl, void *blob,
    int offset, char *name, const void **prop, int *prop_len);
int xocl_fdt_check_uuids(xdev_handle_t xdev_hdl, const void *blob,
		        const void *subset_blob);
int xocl_fdt_parse_blob(xdev_handle_t xdev_hdl, char *blob, u32 blob_sz,
		struct xocl_subdev **subdevs);
const struct axlf_section_header *xocl_axlf_section_header(
	xdev_handle_t xdev_hdl, const struct axlf *top,
	enum axlf_section_kind kind);
int xocl_fdt_path_offset(xdev_handle_t xdev_hdl, void *blob, const char *path);
int xocl_fdt_setprop(xdev_handle_t xdev_hdl, void *blob, int off,
		     const char *name, const void *val, int size);
const void *xocl_fdt_getprop(xdev_handle_t xdev_hdl, void *blob, int off,
			     char *name, int *lenp);


/* init functions */
int __init xocl_init_userpf(void);
void xocl_fini_fini_userpf(void);

int __init xocl_init_drv_user_qdma(void);
void xocl_fini_drv_user_qdma(void);

int __init xocl_init_feature_rom(void);
void xocl_fini_feature_rom(void);

int __init xocl_init_xdma(void);
void xocl_fini_xdma(void);

int __init xocl_init_qdma(void);
void xocl_fini_qdma(void);

int __init xocl_init_qdma4(void);
void xocl_fini_qdma4(void);

int __init xocl_init_mb_scheduler(void);
void xocl_fini_mb_scheduler(void);

int __init xocl_init_xvc(void);
void xocl_fini_xvc(void);

int __init xocl_init_firewall(void);
void xocl_fini_firewall(void);

int __init xocl_init_sysmon(void);
void xocl_fini_sysmon(void);

int __init xocl_init_mb(void);
void xocl_fini_mb(void);

int __init xocl_init_ps(void);
void xocl_fini_ps(void);

int __init xocl_init_xiic(void);
void xocl_fini_xiic(void);

int __init xocl_init_mailbox(void);
void xocl_fini_mailbox(void);

int __init xocl_init_icap(void);
void xocl_fini_icap(void);

int __init xocl_init_clock(void);
void xocl_fini_clock(void);

int __init xocl_init_mig(void);
void xocl_fini_mig(void);

int __init xocl_init_ert(void);
void xocl_fini_ert(void);

int __init xocl_init_xmc(void);
void xocl_fini_xmc(void);

int __init xocl_init_dna(void);
void xocl_fini_dna(void);

int __init xocl_init_fmgr(void);
void xocl_fini_fmgr(void);

int __init xocl_init_mgmt_msix(void);
void xocl_fini_mgmt_msix(void);

int __init xocl_init_flash(void);
void xocl_fini_flash(void);

int __init xocl_init_axigate(void);
void xocl_fini_axigate(void);

int __init xocl_init_iores(void);
void xocl_fini_iores(void);

int __init xocl_init_mailbox_versal(void);
void xocl_fini_mailbox_versal(void);

int __init xocl_init_xfer_versal(void);
void xocl_fini_xfer_versal(void);

int __init xocl_init_aim(void);
void xocl_fini_aim(void);

int __init xocl_init_am(void);
void xocl_fini_am(void);

int __init xocl_init_asm(void);
void xocl_fini_asm(void);

int __init xocl_init_trace_fifo_lite(void);
void xocl_fini_trace_fifo_lite(void);

int __init xocl_init_trace_fifo_full(void);
void xocl_fini_trace_fifo_full(void);

int __init xocl_init_trace_funnel(void);
void xocl_fini_trace_funnel(void);

int __init xocl_init_trace_s2mm(void);
void xocl_fini_trace_s2mm(void);

int __init xocl_init_mem_hbm(void);
void xocl_fini_mem_hbm(void);

int __init xocl_init_srsr(void);
void xocl_fini_srsr(void);

int __init xocl_init_ulite(void);
void xocl_fini_ulite(void);

int __init xocl_init_calib_storage(void);
void xocl_fini_calib_storage(void);

int __init xocl_init_kds(void);
void xocl_fini_kds(void);

int __init xocl_init_cu(void);
void xocl_fini_cu(void);

int __init xocl_init_addr_translator(void);
void xocl_fini_addr_translator(void);

int __init xocl_init_p2p(void);
void xocl_fini_p2p(void);

int __init xocl_init_spc(void);
void xocl_fini_spc(void);

int __init xocl_init_lapc(void);
void xocl_fini_lapc(void);

int __init xocl_init_pmc(void);
void xocl_fini_pmc(void);

int __init xocl_init_intc(void);
void xocl_fini_intc(void);

int __init xocl_init_icap_controller(void);
void xocl_fini_icap_controller(void);

int __init xocl_init_m2m(void);
void xocl_fini_m2m(void);

int __init xocl_init_version_control(void);
void xocl_fini_version_control(void);

int __init xocl_init_msix_xdma(void);
void xocl_fini_msix_xdma(void);

int __init xocl_init_ert_user(void);
void xocl_fini_ert_user(void);

int __init xocl_init_ert_30(void);
void xocl_fini_ert_30(void);

#endif
