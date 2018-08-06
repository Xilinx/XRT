/*
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@Xilinx.com
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
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,0,0)
#include <drm/drm_backport.h>
#endif
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_mm.h>
#include "xclbin.h"
#include "devices.h"
#include "xocl_ioctl.h"

#if defined(RHEL_RELEASE_CODE)
#if RHEL_RELEASE_CODE <= RHEL_RELEASE_VERSION(7,4)
#define XOCL_UUID
#endif
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0)
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

#define	XOCL_DRV_CHANGE		"$Change$"
#define	XOCL_MODULE_NAME	"xocl"
#define	XCLMGMT_MODULE_NAME	"xclmgmt"

#define XOCL_EBUF_LEN           512
#define xocl_sysfs_error(xdev, fmt, args...)     \
        snprintf(((struct xocl_dev_core *)xdev)->ebuf, XOCL_EBUF_LEN,	\
		 fmt, ##args)

#define xocl_err(dev, fmt, args...)			\
	dev_err(dev, "%s: "fmt, __func__, ##args)
#define xocl_info(dev, fmt, args...)			\
	dev_info(dev, "%s: "fmt, __func__, ##args)
#define xocl_dbg(dev, fmt, args...)			\
	dev_dbg(dev, "%s: "fmt, __func__, ##args)

#define	XOCL_READ_REG32(addr)		\
	ioread32(addr)
#define	XOCL_WRITE_REG32(val, addr)	\
	iowrite32(val, addr)
#define	XOCL_COPY2IO(ioaddr, buf, len)	\
	memcpy_toio(ioaddr, buf, len)

#define	XOCL_PL_TO_PCI_DEV(pldev)		\
	to_pci_dev(pldev->dev.parent)

#define	XOCL_QDMA_USER_BAR	2
#define	XOCL_DSA_VERSION(xdev)			\
	(XDEV(xdev)->priv.dsa_ver)

#define	XOCL_DEV_ID(pdev)			\
	PCI_DEVID(pdev->bus->number, pdev->devfn)

#define XOCL_ARE_HOP 0x400000000ull

#define	XOCL_XILINX_VEN		0x10EE
#define	XOCL_CHARDEV_REG_COUNT	16

#ifdef RHEL_RELEASE_VERSION
#if RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,3)
#define RHEL_P2P_SUPPORT  1
#else
#define RHEL_P2P_SUPPORT  0
#endif
#else
#define RHEL_P2P_SUPPORT  0
#endif

extern struct class *xrt_class;

struct xocl_dev;
struct drm_xocl_bo;
struct client_ctx;

struct xocl_subdev {
	struct platform_device 		*pldev;
	void				*ops;
};

struct xocl_mem_topology {
        //TODO : check the first 4 entries - remove unneccessary ones.
        u32                  bank_count;
        struct mem_data     *m_data;
        u32                  m_data_length; /* length of the mem_data section */
        u64                  size;
        struct mem_topology *topology;
};

struct xocl_connectivity {
        u64                     size;
        struct connectivity     *connections;
};

struct xocl_layout {
        u64                     size;
        struct ip_layout        *layout;
};

struct xocl_debug_layout {
        u64                     size;
        struct debug_ip_layout  *layout;
};

typedef	void *	xdev_handle_t;

struct xocl_pci_funcs {
	int (*intr_config)(xdev_handle_t xdev, u32 intr, bool enable);
	int (*intr_register)(xdev_handle_t xdev, u32 intr,
		irq_handler_t handler, void *arg);
	int (*dev_online)(xdev_handle_t xdev);
	int (*dev_offline)(xdev_handle_t xdev);
};

#define	XDEV(dev)	((struct xocl_dev_core *)(dev))
#define	XDEV_PCIOPS(xdev)	(XDEV(xdev)->pci_ops)

#define	xocl_user_interrupt_config(xdev, intr, en)	\
	XDEV_PCIOPS(xdev)->intr_config(xdev, intr, en)
#define	xocl_user_interrupt_reg(xdev, intr, handler, arg)	\
	XDEV_PCIOPS(xdev)->intr_register(xdev, intr, handler, arg)
#define	xocl_user_dev_online(xdev)	\
	XDEV_PCIOPS(xdev)->dev_online(xdev)
#define	xocl_user_dev_offline(xdev)	\
	XDEV_PCIOPS(xdev)->dev_offline(xdev)

struct xocl_context {
	struct hlist_node	hlist;
	u32			arg_sz;
	char			arg[1];
};

struct xocl_context_hash {
	struct hlist_head	*hash;
	u32			size;
	u32			count;
	spinlock_t		ctx_lock;
	struct device		*dev;
	u32 (*hash_func)(void *arg);
	int (*cmp_func)(void *arg_o, void *arg_n);
};

struct xocl_health_thread_arg {
	int (*health_cb)(void *arg);
	void		*arg;
	u32		interval;    /* ms */
	struct device	*dev;
};

struct xocl_dev_core {
	struct pci_dev		*pdev;
	struct xocl_subdev	subdevs[XOCL_SUBDEV_NUM];
	u32			subdev_num;
	struct xocl_pci_funcs	*pci_ops;

	u32			bar_idx;
        void *__iomem		bar_addr;
	resource_size_t		bar_size;

	u32			intr_bar_idx;
        void *__iomem		intr_bar_addr;
	resource_size_t		intr_bar_size;

	struct xocl_board_private priv;

	char			ebuf[XOCL_EBUF_LEN + 1];
};

#define	XOCL_DSA_PCI_RESET_OFF(xdev_hdl)			\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_PCI_RESET_OFF)
#define	XOCL_DSA_MB_SCHE_OFF(xdev_hdl)			\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_MB_SCHE_OFF)
#define	XOCL_DSA_AXILITE_FLUSH_REQUIRED(xdev_hdl)			\
	(((struct xocl_dev_core *)xdev_hdl)->priv.flags &	\
	XOCL_DSAFLAG_AXILITE_FLUSH)

#define	XOCL_DSA_XPR_ON(xdev_hdl)		\
	(((struct xocl_dev_core *)xdev_hdl)->priv.xpr)


#define	SUBDEV(xdev, id)	\
	(XDEV(xdev)->subdevs[id])

/* rom callbacks */
struct xocl_rom_funcs {
	unsigned int (*dsa_version)(struct platform_device *pdev);
	bool (*is_unified)(struct platform_device *pdev);
	bool (*mb_mgmt_on)(struct platform_device *pdev);
	bool (*mb_sched_on)(struct platform_device *pdev);
	bool (*cdma_on)(struct platform_device *pdev);
	u16 (*get_ddr_channel_count)(struct platform_device *pdev);
	u64 (*get_ddr_channel_size)(struct platform_device *pdev);
	bool (*is_are)(struct platform_device *pdev);
	bool (*is_aws)(struct platform_device *pdev);
	bool (*verify_timestamp)(struct platform_device *pdev, u64 timestamp);
	u64 (*get_timestamp)(struct platform_device *pdev);
	void (*get_raw_header)(struct platform_device *pdev, void *header);
};
#define ROM_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_FEATURE_ROM).pldev
#define	ROM_OPS(xdev)	\
	((struct xocl_rom_funcs *)SUBDEV(xdev, XOCL_SUBDEV_FEATURE_ROM).ops)
#define	xocl_dsa_version(xdev)		\
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->dsa_version(ROM_DEV(xdev)) : 0)
#define	xocl_is_unified(xdev)		\
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->is_unified(ROM_DEV(xdev)) : true)
#define	xocl_mb_mgmt_on(xdev)		\
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->mb_mgmt_on(ROM_DEV(xdev)) : false)
#define	xocl_mb_sched_on(xdev)		\
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->mb_sched_on(ROM_DEV(xdev)) : false)
#define	xocl_cdma_on(xdev)		\
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->cdma_on(ROM_DEV(xdev)) : false)
#define	xocl_get_ddr_channel_count(xdev) \
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->get_ddr_channel_count(ROM_DEV(xdev)) :\
	0)
#define	xocl_get_ddr_channel_size(xdev) \
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->get_ddr_channel_size(ROM_DEV(xdev)) : 0)
#define	xocl_is_are(xdev)		\
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->is_are(ROM_DEV(xdev)) : false)
#define	xocl_is_aws(xdev)		\
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->is_aws(ROM_DEV(xdev)) : false)
#define	xocl_verify_timestamp(xdev, ts)	\
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->verify_timestamp(ROM_DEV(xdev), ts) : \
	false)
#define	xocl_get_timestamp(xdev) \
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->get_timestamp(ROM_DEV(xdev)) : 0)
#define	xocl_get_raw_header(xdev, header) \
	(ROM_DEV(xdev) ? ROM_OPS(xdev)->get_raw_header(ROM_DEV(xdev), header) :\
	NULL)

/* mm_dma callbacks */
struct xocl_mm_dma_funcs {
	ssize_t (*migrate_bo)(struct platform_device *pdev,
		struct sg_table *sgt, u32 dir, u64 paddr, u32 channel, u64 sz);
	int (*ac_chan)(struct platform_device *pdev, u32 dir);
	void (*rel_chan)(struct platform_device *pdev, u32 dir, u32 channel);
	int (*set_max_chan)(struct platform_device *pdev, u32 channel_count);
	u32 (*get_chan_count)(struct platform_device *pdev);
	u64 (*get_chan_stat)(struct platform_device *pdev, u32 channel,
		u32 write);
};
#define MM_DMA_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_MM_DMA).pldev
#define	MM_DMA_OPS(xdev)	\
	((struct xocl_mm_dma_funcs *)SUBDEV(xdev, XOCL_SUBDEV_MM_DMA).ops)
#define	xocl_migrate_bo(xdev, sgt, write, paddr, chan, len)	\
	(MM_DMA_DEV(xdev) ? MM_DMA_OPS(xdev)->migrate_bo(MM_DMA_DEV(xdev), \
	sgt, write, paddr, chan, len) : 0)
#define	xocl_acquire_channel(xdev, dir)		\
	(MM_DMA_DEV(xdev) ? MM_DMA_OPS(xdev)->ac_chan(MM_DMA_DEV(xdev), dir) : \
	-ENODEV)
#define	xocl_release_channel(xdev, dir, chan)	\
	(MM_DMA_DEV(xdev) ? MM_DMA_OPS(xdev)->rel_chan(MM_DMA_DEV(xdev), dir, \
	chan) : NULL)
#define	xocl_set_max_channel(xdev, count)		\
	(MM_DMA_DEV(xdev) ? MM_DMA_OPS(xdev)->set_max_chan(MM_DMA_DEV(xdev), \
	count) : -ENODEV)
#define	xocl_get_chan_count(xdev)		\
	(MM_DMA_DEV(xdev) ? MM_DMA_OPS(xdev)->get_chan_count(MM_DMA_DEV(xdev)) \
	: 0)
#define	xocl_get_chan_stat(xdev, chan, write)		\
	(MM_DMA_DEV(xdev) ? MM_DMA_OPS(xdev)->get_chan_stat(MM_DMA_DEV(xdev), \
	chan, write) : 0)

/* mb_scheduler callbacks */
struct xocl_mb_scheduler_funcs {
	int (*add_exec_buffer)(struct platform_device *pdev, struct client_ctx *client,void *buf, int numdeps, struct drm_xocl_bo **deps);
	int (*create_client)(struct platform_device *pdev, void **priv);
	void (*destroy_client)(struct platform_device *pdev, void **priv);
	uint (*poll_client)(struct platform_device *pdev, struct file *filp,
		poll_table *wait, void *priv);
	int (*reset)(struct platform_device *pdev);
};
#define	MB_SCHEDULER_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_MB_SCHEDULER).pldev
#define	MB_SCHEDULER_OPS(xdev)	\
	((struct xocl_mb_scheduler_funcs *)SUBDEV(xdev, 	\
		XOCL_SUBDEV_MB_SCHEDULER).ops)
#define	xocl_exec_add_buffer(xdev, client, bo, numdeps, deps)	\
	(MB_SCHEDULER_DEV(xdev) ? 				\
	 MB_SCHEDULER_OPS(xdev)->add_exec_buffer(MB_SCHEDULER_DEV(xdev), client, bo,  numdeps, deps) : \
	-ENODEV)
#define	xocl_exec_create_client(xdev, priv)		\
	(MB_SCHEDULER_DEV(xdev) ?			\
	MB_SCHEDULER_OPS(xdev)->create_client(MB_SCHEDULER_DEV(xdev), priv) : \
	-ENODEV)
#define	xocl_exec_destroy_client(xdev, priv)		\
	(MB_SCHEDULER_DEV(xdev) ?			\
	MB_SCHEDULER_OPS(xdev)->destroy_client(MB_SCHEDULER_DEV(xdev), priv) : \
	NULL)
#define	xocl_exec_poll_client(xdev, filp, wait, priv)		\
	(MB_SCHEDULER_DEV(xdev) ? 				\
	MB_SCHEDULER_OPS(xdev)->poll_client(MB_SCHEDULER_DEV(xdev), filp, \
	wait, priv) : 0)
#define	xocl_exec_reset(xdev)		\
	(MB_SCHEDULER_DEV(xdev) ? 				\
	 MB_SCHEDULER_OPS(xdev)->reset(MB_SCHEDULER_DEV(xdev)) : \
        -ENODEV)
#define	XOCL_IS_DDR_USED(xdev, ddr)		\
	(xdev->topology.m_data[ddr].m_used == 1)
#define	XOCL_DDR_COUNT(xdev)			\
	((xocl_is_unified(xdev) ? xdev->topology.bank_count :	\
	xocl_get_ddr_channel_count(xdev)))

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
	int (*get_prop)(struct platform_device *pdev, u32 prop, void *val);
};
#define	SYSMON_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_SYSMON).pldev
#define	SYSMON_OPS(xdev)	\
	((struct xocl_sysmon_funcs *)SUBDEV(xdev, 	\
		XOCL_SUBDEV_SYSMON).ops)
#define	xocl_sysmon_get_prop(xdev, prop, val)		\
	(SYSMON_DEV(xdev) ? SYSMON_OPS(xdev)->get_prop(SYSMON_DEV(xdev), \
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
	int (*get_prop)(struct platform_device *pdev, u32 prop, void *val);
	int (*clear_firewall)(struct platform_device *pdev);
	u32 (*check_firewall)(struct platform_device *pdev, int *level);
	int (*health_check)(struct platform_device *pdev,
		int (*cb)(void *data), void *cb_arg, u32 interval);
};
#define AF_DEV(xdev)	\
	SUBDEV(xdev, XOCL_SUBDEV_AF).pldev
#define	AF_OPS(xdev)	\
	((struct xocl_firewall_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_AF).ops)
#define	xocl_af_get_prop(xdev, prop, val)		\
	(AF_DEV(xdev) ? AF_OPS(xdev)->get_prop(AF_DEV(xdev), prop, val) : \
	-ENODEV)
#define	xocl_af_check(xdev, level)			\
	(AF_DEV(xdev) ? AF_OPS(xdev)->check_firewall(AF_DEV(xdev), level) : 0)
#define	xocl_af_clear(xdev)				\
	(AF_DEV(xdev) ? AF_OPS(xdev)->clear_firewall(AF_DEV(xdev)) : -ENODEV)
#define xocl_af_start_health_check(xdev, cb, cb_arg, interval)	\
	(AF_DEV(xdev) ? AF_OPS(xdev)->health_check(AF_DEV(xdev), cb, cb_arg, \
	interval) : -ENODEV)

/* microblaze callbacks */
struct xocl_mb_funcs {
	void (*reset)(struct platform_device *pdev);
	int (*load_mgmt_image)(struct platform_device *pdev, const char *buf,
		u32 len);
	int (*load_sche_image)(struct platform_device *pdev, const char *buf,
		u32 len);
};
#define	MB_DEV(xdev)		\
	SUBDEV(xdev, XOCL_SUBDEV_MB).pldev
#define	MB_OPS(xdev)		\
	((struct xocl_mb_funcs *)SUBDEV(xdev,	\
	XOCL_SUBDEV_MB).ops)
#define	xocl_mb_reset(xdev)			\
	(MB_DEV(xdev) ? MB_OPS(xdev)->reset(MB_DEV(xdev)) : NULL)
#define xocl_mb_load_mgmt_image(xdev, buf, len)		\
	(MB_DEV(xdev) ? MB_OPS(xdev)->load_mgmt_image(MB_DEV(xdev), buf, len) :\
	-ENODEV)
#define xocl_mb_load_sche_image(xdev, buf, len)		\
	(MB_DEV(xdev) ? MB_OPS(xdev)->load_sche_image(MB_DEV(xdev), buf, len) :\
	-ENODEV)

/*
 * mailbox callbacks
 */
enum mailbox_request {
	MAILBOX_REQ_UNKNOWN = 0,
	MAILBOX_REQ_TEST_READY,
	MAILBOX_REQ_TEST_READ,
	MAILBOX_REQ_LOCK_BITSTREAM,
	MAILBOX_REQ_UNLOCK_BITSTREAM,
	MAILBOX_REQ_RESET_BEGIN,
	MAILBOX_REQ_RESET_END,
};

struct mailbox_req_bitstream_lock {
	pid_t pid;
	xuid_t uuid;
};

struct mailbox_req {
	enum mailbox_request req;
	union {
		struct mailbox_req_bitstream_lock req_bit_lock;
	} u;
};

typedef	void (*mailbox_msg_cb_t)(void *arg, void *data, size_t len,
	u64 msgid, int err);
struct xocl_mailbox_funcs {
	int (*request)(struct platform_device *pdev, void *req,
		size_t reqlen, void *resp, size_t *resplen,
		mailbox_msg_cb_t cb, void *cbarg);
	int (*post)(struct platform_device *pdev, u64 req_id,
		void *resp, size_t len);
	int (*listen)(struct platform_device *pdev,
		mailbox_msg_cb_t cb, void *cbarg);
	int (*reset)(struct platform_device *pdev, bool end_of_reset);
};
#define	MAILBOX_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_MAILBOX).pldev
#define	MAILBOX_OPS(xdev)	\
	((struct xocl_mailbox_funcs *)SUBDEV(xdev, XOCL_SUBDEV_MAILBOX).ops)
#define MAILBOX_READY(xdev)	(MAILBOX_DEV(xdev) && MAILBOX_OPS(xdev))
#define	xocl_peer_request(xdev, req, resp, resplen, cb, cbarg)		\
	(MAILBOX_READY(xdev) ? MAILBOX_OPS(xdev)->request(MAILBOX_DEV(xdev), \
	req, sizeof(*req), resp, resplen, cb, cbarg) : -ENODEV)
#define	xocl_peer_response(xdev, reqid, buf, len)			\
	(MAILBOX_READY(xdev) ? MAILBOX_OPS(xdev)->post(MAILBOX_DEV(xdev), \
	reqid, buf, len) : -ENODEV)
#define	xocl_peer_notify(xdev, req)					\
	(MAILBOX_READY(xdev) ? MAILBOX_OPS(xdev)->post(MAILBOX_DEV(xdev), 0, \
	req, sizeof (*req)) : -ENODEV)
#define	xocl_peer_listen(xdev, cb, cbarg)				\
	(MAILBOX_READY(xdev) ? MAILBOX_OPS(xdev)->listen(MAILBOX_DEV(xdev), \
	cb, cbarg) : -ENODEV)
#define	xocl_mailbox_reset(xdev, end)				\
	(MAILBOX_READY(xdev) ? MAILBOX_OPS(xdev)->reset(MAILBOX_DEV(xdev), \
	end) : -ENODEV)

struct xocl_icap_funcs {
	int (*freeze_axi_gate)(struct platform_device *pdev);
	int (*free_axi_gate)(struct platform_device *pdev);
	int (*reset_bitstream)(struct platform_device *pdev);
	int (*download_bitstream_axlf)(struct platform_device *pdev,
		const void __user *arg);
	int (*download_boot_firmware)(struct platform_device *pdev);
	int (*ocl_set_freq)(struct platform_device *pdev,
		unsigned int region, unsigned short *freqs, int num_freqs);
	int (*ocl_get_freq)(struct platform_device *pdev,
		unsigned int region, unsigned short *freqs, int num_freqs);
	char* (*ocl_get_clock_freq_topology)(struct platform_device *pdev);
	int (*ocl_lock_bitstream)(struct platform_device *pdev,
		const xuid_t *uuid, pid_t pid);
	int (*ocl_unlock_bitstream)(struct platform_device *pdev,
		const xuid_t *uuid, pid_t pid);
};
#define	ICAP_DEV(xdev)	SUBDEV(xdev, XOCL_SUBDEV_ICAP).pldev
#define	ICAP_OPS(xdev)							\
	((struct xocl_icap_funcs *)SUBDEV(xdev, XOCL_SUBDEV_ICAP).ops)
#define	xocl_icap_freeze_axi_gate(xdev)					\
	ICAP_OPS(xdev)->freeze_axi_gate(ICAP_DEV(xdev))
#define	xocl_icap_free_axi_gate(xdev)					\
	ICAP_OPS(xdev)->free_axi_gate(ICAP_DEV(xdev))
#define	xocl_icap_reset_bitstream(xdev)					\
	ICAP_OPS(xdev)->reset_bitstream(ICAP_DEV(xdev))
#define	xocl_icap_download_axlf(xdev, xclbin)				\
	ICAP_OPS(xdev)->download_bitstream_axlf(ICAP_DEV(xdev), xclbin)
#define	xocl_icap_download_boot_firmware(xdev)				\
	ICAP_OPS(xdev)->download_boot_firmware(ICAP_DEV(xdev))
#define	xocl_icap_ocl_get_freq(xdev, region, freqs, num)		\
	ICAP_OPS(xdev)->ocl_get_freq(ICAP_DEV(xdev), region, freqs, num)
#define	xocl_icap_ocl_get_clock_freq_topology(xdev)		\
	ICAP_OPS(xdev)->ocl_get_clock_freq_topology(ICAP_DEV(xdev))
#define	xocl_icap_ocl_set_freq(xdev, region, freqs, num)		\
	ICAP_OPS(xdev)->ocl_set_freq(ICAP_DEV(xdev), region, freqs, num)
#define	xocl_icap_lock_bitstream(xdev, uuid, pid)			\
	ICAP_OPS(xdev)->ocl_lock_bitstream(ICAP_DEV(xdev), uuid, pid)
#define	xocl_icap_unlock_bitstream(xdev, uuid, pid)			\
	ICAP_OPS(xdev)->ocl_unlock_bitstream(ICAP_DEV(xdev), uuid, pid)

struct xocl_str_dma_funcs  {
	u64 (*get_str_stat)(struct platform_device *pdev, u32 q_idx);
};

/* helper functions */
xdev_handle_t xocl_get_xdev(struct platform_device *pdev);
void xocl_init_dsa_priv(xdev_handle_t xdev_hdl);

/* subdev functions */
int xocl_subdev_create_one(xdev_handle_t xdev_hdl,
	struct xocl_subdev_info *sdev_info);
int xocl_subdev_create_all(xdev_handle_t xdev_hdl,
        struct xocl_subdev_info *sdev_info, u32 subdev_num);
void xocl_subdev_destroy_all(xdev_handle_t xdev_hdl);

void xocl_subdev_register(struct platform_device *pldev, u32 id,
	void *cb_funcs);
void xocl_fill_dsa_priv(xdev_handle_t xdev_hdl, struct xocl_board_private *in);

/* context helpers */
int xocl_ctx_init(struct device *dev, struct xocl_context_hash *ctx_hash,
	u32 hash_sz, u32 (*hash_func)(void *arg),
	int (*cmp_func)(void *arg_o, void *arg_n));
void xocl_ctx_fini(struct device *dev, struct xocl_context_hash *ctx_hash);
int xocl_ctx_remove(struct xocl_context_hash *ctx_hash, void *arg);
int xocl_ctx_add(struct xocl_context_hash *ctx_hash, void *arg, u32 arg_sz);
int xocl_ctx_traverse(struct xocl_context_hash *ctx_hash,
	int (*cb_func)(struct xocl_context_hash *ctx_hash, void *arg));

/* health thread functions */
int health_thread_init(struct device *dev, char *thread_name,
	struct xocl_health_thread_arg *arg, struct task_struct **pthread);
void health_thread_fini(struct device *dev, struct task_struct *pthread);

/* init functions */
int __init xocl_init_drv_user_xdma(void);
void xocl_fini_drv_user_xdma(void);

int __init xocl_init_drv_user_qdma(void);
void xocl_fini_drv_user_qdma(void);

int __init xocl_init_feature_rom(void);
void xocl_fini_feature_rom(void);

int __init xocl_init_mm_xdma(void);
void xocl_fini_mm_xdma(void);

int __init xocl_init_mm_qdma(void);
void xocl_fini_mm_qdma(void);

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

int __init xocl_init_xiic(void);
void xocl_fini_xiic(void);

int __init xocl_init_mailbox(void);
void xocl_fini_mailbox(void);

int __init xocl_init_icap(void);
void xocl_fini_icap(void);

int __init xocl_init_str_qdma(void);
void xocl_fini_str_qdma(void);

int __init xocl_init_mig(void);
void xocl_fini_mig(void);

#endif
