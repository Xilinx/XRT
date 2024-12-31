/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019-2022 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>

#include "zocl_drv.h"
#include "xrt_drv.h"
#include "zocl_xclbin.h"
#include "zocl_ospi_versal.h"

#define ov_err(pdev, fmt, args...)  \
	zocl_err(&pdev->dev, fmt"\n", ##args)
#define ov_info(pdev, fmt, args...)  \
	zocl_info(&pdev->dev, fmt"\n", ##args)
#define ov_dbg(pdev, fmt, args...)  \
	zocl_dbg(&pdev->dev, fmt"\n", ##args)

static inline u32 wait_for_status(struct zocl_ov_dev *ov, u8 status)
{
	u32 header;
	struct pdi_packet *pkt = (struct pdi_packet *)&header;

	for (;;) {
		header = ioread32(ov->base);
		if (pkt->pkt_status == status)
			break;
	}

	return header;
}

static inline u8 get_pkt_flags(struct zocl_ov_dev *ov)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	header = ioread32(ov->base);
	return pkt->pkt_flags;
}

static inline bool check_for_status(struct zocl_ov_dev *ov, u8 status)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	header = ioread32(ov->base);
	return (pkt->pkt_status == status);
}

static inline void set_flags(struct zocl_ov_dev *ov, u8 flags)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	header = ioread32(ov->base);
	pkt->pkt_flags = flags;

	iowrite32(pkt->header, ov->base);
}

static inline void set_version(struct zocl_ov_dev *ov)
{
	set_flags(ov, XRT_XFR_VER <<  XRT_XFR_PKT_VER_SHIFT);
}

static inline void set_status(struct zocl_ov_dev *ov, u8 status)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	header = ioread32(ov->base);
	pkt->pkt_status = status;

	iowrite32(pkt->header, ov->base);
}

static inline void read_data(u32 *addr, u32 *data, size_t sz)
{
	int i;

	for (i = 0; i < sz; ++i)
		*(data + i) = ioread32(addr + i);
}

static void zocl_ov_clean(struct zocl_ov_dev *ov)
{
	struct zocl_ov_pkt_node *node;
	struct zocl_ov_pkt_node *pnode;

	node = ov->head;
	while (node != NULL) {
		pnode = node;
		node = pnode->zn_next;
		vfree(pnode->zn_datap);
		vfree(pnode);
	}

	ov->head = NULL;
}

static int zocl_ov_find_parent_dev(struct zocl_ov_dev *ov)
{
	struct platform_device *parent_dev;

	parent_dev = zocl_find_pdev("zyxclmm_drm");
	if (parent_dev) {
		ov_info(ov->pdev, "Found parent pdev zyxclmm_drv: 0x%llx",
		    (uint64_t)(uintptr_t)parent_dev);
		ov->ppdev = parent_dev;
		return 0;
	}

	ov_err(ov->pdev, "Can NOT find parent pdev zyxclmm_drv");

	ov->ppdev = NULL;
	return -ENXIO;
}

static int zocl_ov_copy_xclbin(struct zocl_ov_dev *ov, struct axlf **xclbin)
{
	struct zocl_ov_pkt_node *node = ov->head;
	char *xp;
	size_t len = 0;

	while (node) {
		len += node->zn_size;
		node = node->zn_next;
	}

	if (len == 0) {
		ov_err(ov->pdev, "Load xclbin failed: size is 0");
		return -EINVAL;
	}

	*xclbin = vmalloc(len);
	if (!(*xclbin)) {
		ov_err(ov->pdev, "Load xclbin failed to allocate buf");
		return -ENOMEM;
	}

	xp = (char *)*xclbin;
	node = ov->head;
	while (node) {
		memcpy(xp, node->zn_datap, node->zn_size);
		xp += node->zn_size;
		node = node->zn_next;
	}

	return 0;
}

static int zocl_ov_recieve(struct zocl_ov_dev *ov)
{
	struct zocl_ov_pkt_node *node = ov->head;
	u32 *base = ov->base;
	int len = 0, next = 0;
	int ret;

	for (;;) {
		u32 pkt_header;
		struct pdi_packet *pkt = (struct pdi_packet *)&pkt_header;
		struct zocl_ov_pkt_node *new;

		/* Busy wait here until we get a new packet */
		pkt_header = wait_for_status(ov, XRT_XFR_PKT_STATUS_NEW);

		new = vzalloc(sizeof(struct zocl_ov_pkt_node));
		if (!new) {
			ret = -ENOMEM;
			break;
		}
		new->zn_datap = vmalloc(ov->size);
		if (!new->zn_datap) {
			ret = -ENOMEM;
			break;
		}
		new->zn_size = pkt->pkt_size;

		/* Read packet data payload on a 4 bytes base */
		read_data(base + (sizeof(struct pdi_packet)) / 4,
		    new->zn_datap, (ov->size - sizeof(struct pdi_packet)) / 4);

		/* Notify host that the data has been read */
		set_status(ov, XRT_XFR_PKT_STATUS_IDLE);

		len += ov->size;
		if ((len / 1000000) > next) {
			ov_info(ov->pdev, "%d M", len / 1000000);
			next++;
		}

		/* Add packet data to linked list */
		if (node)
			node->zn_next = new;
		else
			ov->head = new;
		node = new;

		/* Bail out here if this is the last packet */
		if (pkt->pkt_flags & XRT_XFR_PKT_FLAGS_LAST) {
			ret = 0;
			break;
		}
	}

	return ret;
}

/*
 * This function is called once we detect there is a new XCLBIN packet and it
 * will communicate with host driver to collect all XCLBIN packets and then
 * call zocl driver service to load this xclbin.
 *
 * 1) start receiving XCLBIN packets
 * 2) put all XCLBIN packets into a linked packets list
 * 3) once got all packets, copy XCLBIN packets to a XCLBIN buffer
 * 4) call zocl driver service to load XCLBIN
 * 5) update XCLBIN packet status to notify host
 */
static int zocl_ov_get_xclbin(struct zocl_ov_dev *ov)
{
	struct drm_zocl_dev *zdev = NULL;
	struct drm_zocl_slot *slot = NULL;
	struct axlf *xclbin = NULL;
	void *pdrv = NULL;
	int ret = 0;

	ov_info(ov->pdev, "xclbin is being downloaded...");

	write_lock(&ov->att_rwlock);

	ret = zocl_ov_recieve(ov);
	if (ret) {
		set_status(ov, XRT_XFR_PKT_STATUS_FAIL);
		ov_err(ov->pdev, "Fail to recieve XCLBIN file %d", ret);
		goto out;
	}

	ov_info(ov->pdev, "XCLBIN is transfered");

	ret = zocl_ov_copy_xclbin(ov, &xclbin);
	if (ret) {
		set_status(ov, XRT_XFR_PKT_STATUS_FAIL);
		goto out;
	}

	if (!ov->ppdev) {
		ret = zocl_ov_find_parent_dev(ov);
		if (ret) {
			set_status(ov, XRT_XFR_PKT_STATUS_FAIL);
			goto out;
		}
	}

	pdrv = platform_get_drvdata(ov->ppdev);
	if (!pdrv) {
		ov_err(ov->pdev, "Fail to get parent dev driver data");
		set_status(ov, XRT_XFR_PKT_STATUS_FAIL);
		ret = -ENXIO;
		goto out;
	}

	write_unlock(&ov->att_rwlock);
	zdev = (struct drm_zocl_dev *)pdrv;
	if (!zdev) {
		ret = -ENXIO;
		goto out;
	}

	/* For OSPI device use default slot i.e. 0 */
	slot = zdev->pr_slot[0];
	ret = zocl_xclbin_load_pdi(pdrv, xclbin, slot);
	if (ret) {
		set_status(ov, XRT_XFR_PKT_STATUS_FAIL);
		goto out;
	}
	write_lock(&ov->att_rwlock);

	ov_info(ov->pdev, "xclbin_done: %d", ov->pdi_done);

	set_status(ov, XRT_XFR_PKT_STATUS_DONE);

out:
	zocl_ov_clean(ov);
	write_unlock(&ov->att_rwlock);
	vfree(xclbin);

	wait_for_status(ov, XRT_XFR_PKT_STATUS_IDLE);
	set_version(ov);

	return ret;
}

/*
 * This function is called once we detect there is a new PDI packet and it
 * will communicate with host driver to collect all PDI packets and then
 * communicate with user space daemon to flash the PDI.
 *
 * 1) start receiving PDI packets
 * 2) put all pdi packets into a linked packets list
 * 3) once got all packets, update sysfs node to indicate PDI is ready
 * 4) wait on sysfs node on PDI flash done
 * 5) update PDI packet status to notify host
 */
static int zocl_ov_get_pdi(struct zocl_ov_dev *ov)
{
	int ret;

	ov_info(ov->pdev, "pdi is being downloaded...");

	/* Clear the done flag */
	write_lock(&ov->att_rwlock);
	ov->pdi_done = 0;

	ret = zocl_ov_recieve(ov);
	if (ret) {
		write_unlock(&ov->att_rwlock);
		set_status(ov, XRT_XFR_PKT_STATUS_FAIL);
		ov_err(ov->pdev, "Fail to recieve PDI file %d", ret);
		goto fail;
	}

	ov_info(ov->pdev, "pdi is ready for ospi_daemon");

	/* Set ready flag */
	ov->pdi_ready = 1;
	write_unlock(&ov->att_rwlock);

	/* Wait for done */
	read_lock(&ov->att_rwlock);
	while (true) {
		/*
		 * pdi_done indicates the status of the flashing
		 * 0: in progress
		 * 1: completed successfully
		 * 2: fail
		 */
		if (!ov->pdi_done) {
			read_unlock(&ov->att_rwlock);
			msleep(ZOCL_OV_TIMER_INTERVAL);
			read_lock(&ov->att_rwlock);
			continue;
		}
		if (ov->pdi_done == 1)
			set_status(ov, XRT_XFR_PKT_STATUS_DONE);
		else
			set_status(ov, XRT_XFR_PKT_STATUS_FAIL);
		break;
	}
	read_unlock(&ov->att_rwlock);

	ov_info(ov->pdev, "pdi_done: %d", ov->pdi_done);

	/* Clear ready flag */
	write_lock(&ov->att_rwlock);
	ov->pdi_ready = 0;
	zocl_ov_clean(ov);
	write_unlock(&ov->att_rwlock);

	wait_for_status(ov, XRT_XFR_PKT_STATUS_IDLE);
	set_version(ov);

	return 0;

fail:
	write_lock(&ov->att_rwlock);
	zocl_ov_clean(ov);
	write_unlock(&ov->att_rwlock);

	wait_for_status(ov, XRT_XFR_PKT_STATUS_IDLE);
	set_version(ov);

	return ret;
}

/*
 * This is the main thread in zocl ospi versal subdriver.
 *
 * The thread will wake up every second and check the packet
 * status. If there is a new packet ready, it will start load and
 * handle the packets.
 */
static int zocl_ov_thread(void *data)
{
	struct zocl_ov_dev *ov = (struct zocl_ov_dev *)data;
	u8 pkt_flags, pkt_type;

	set_status(ov, XRT_XFR_PKT_STATUS_IDLE);

	while (1) {
		if (kthread_should_stop())
			break;

		if (check_for_status(ov, XRT_XFR_PKT_STATUS_IDLE)) {
			msleep(ZOCL_OV_TIMER_INTERVAL);
			continue;
		}

		pkt_flags = get_pkt_flags(ov);
		pkt_type = pkt_flags >> XRT_XFR_PKT_TYPE_SHIFT &
		    XRT_XFR_PKT_TYPE_MASK;
		switch (pkt_type) {
		case XRT_XFR_PKT_TYPE_PDI:
			zocl_ov_get_pdi(ov);
			break;
		case XRT_XFR_PKT_TYPE_XCLBIN:
			zocl_ov_get_xclbin(ov);
			break;
		default:
			ov_err(ov->pdev, "Unknow packet type: %d", pkt_type);
			break;
		}
	}

	return 0;
}

static const struct of_device_id zocl_ospi_versal_of_match[] = {
	{ .compatible = "xlnx,ospi_versal",
	},
	{ .compatible = "xlnx,mpsoc_ocm",
	},
	{ /* end of table */ },
};

MODULE_DEVICE_TABLE(of, zocl_ospi_versal_of_match);

static int zocl_ov_probe(struct platform_device  *pdev)
{
	struct zocl_ov_dev *ov;
	const struct of_device_id *id;
	struct resource *res;
	void __iomem *map;
	char thread_name[256] = "zocl-ov-thread";
	int ret;

	id = of_match_node(zocl_ospi_versal_of_match, pdev->dev.of_node);
	ov_info(pdev, "Probing for %s", id->compatible);

	res = platform_get_resource(pdev, IORESOURCE_MEM,
	    ZOCL_OSPI_VERSAL_BRAM_RES);
	map = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(map)) {
		ov_err(pdev, "Unable to map OSPI resource: %0lx",
		    PTR_ERR(map));
		return PTR_ERR(map);
	}

	ov = devm_kzalloc(&pdev->dev, sizeof(*ov), GFP_KERNEL);
	if (!ov)
		return -ENOMEM;

	ov->base = map;
	ov->size = res->end - res->start + 1;
	memset_io(ov->base, 0, ov->size);
	ov->pdev = pdev;

	rwlock_init(&ov->att_rwlock);

	ret = zocl_ov_init_sysfs(&pdev->dev);
	if (ret) {
		ov_err(pdev, "Unable to create ospi versal sysfs node");
		kfree(ov);
		return ret;
	}

	set_version(ov);

	/* Start the thread. */
	ov->timer_task = kthread_run(zocl_ov_thread, ov, thread_name);
	if (IS_ERR(ov->timer_task)) {
		ret = PTR_ERR(ov->timer_task);

		ov_err(pdev, "Unable to create ospi versal thread");
		kfree(ov);
		return ret;
	}

	platform_set_drvdata(pdev, ov);

	return 0;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void zocl_ov_remove(struct platform_device *pdev)
#else
static int zocl_ov_remove(struct platform_device *pdev)
#endif
{
	struct zocl_ov_dev *ov = platform_get_drvdata(pdev);

	zocl_ov_fini_sysfs(&pdev->dev);

	if (ov && ov->timer_task)
		kthread_stop(ov->timer_task);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	return 0;
#endif
}

struct platform_driver zocl_ospi_versal_driver = {
	.driver = {
		.name = ZOCL_OSPI_VERSAL_NAME,
		.of_match_table = zocl_ospi_versal_of_match,
	},
	.probe  = zocl_ov_probe,
	.remove = zocl_ov_remove,
};
