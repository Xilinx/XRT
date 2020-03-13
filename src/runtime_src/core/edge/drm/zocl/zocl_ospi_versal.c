/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2019-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Larry Liu <yliu@xilinx.com>
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

#include <linux/kthread.h>
#include <linux/platform_device.h>

#include "zocl_drv.h"
#include "xrt_drv.h"
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

static inline bool check_for_status(struct zocl_ov_dev *ov, u8 status)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	header = ioread32(ov->base);
	return (pkt->pkt_status == status);
}

static inline void set_status(struct zocl_ov_dev *ov, u8 status)
{
	struct pdi_packet pkt;

	pkt.pkt_status = status;
	iowrite32(pkt.header, ov->base);
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
	struct zocl_ov_pkt_node *node = ov->head;
	u32 *base = ov->base;
	int ret;

	/* Clear the done flag */
	write_lock(&ov->att_rwlock);
	ov->pdi_done = 0;
	write_unlock(&ov->att_rwlock);

	for (;;) {
		u32 pkt_header;
		struct pdi_packet *pkt = (struct pdi_packet *)&pkt_header;
		struct zocl_ov_pkt_node *new;

		/* Busy wait here until we get a new packet */
		pkt_header = wait_for_status(ov, XRT_PDI_PKT_STATUS_NEW);

		new = vzalloc(sizeof(struct zocl_ov_pkt_node));
		if (!new) {
			set_status(ov, XRT_PDI_PKT_STATUS_FAIL);
			ret = -ENOMEM;
			goto fail;
		}
		new->zn_datap = vmalloc(ov->size);
		if (!new->zn_datap) {
			set_status(ov, XRT_PDI_PKT_STATUS_FAIL);
			ret = -ENOMEM;
			goto fail;
		}
		new->zn_size = pkt->pkt_size;

		/* Read packet data payload on a 4 bytes base */
		read_data(base + (sizeof(struct pdi_packet)) / 4,
		    new->zn_datap, (ov->size - sizeof(struct pdi_packet)) / 4);

		/* Notify host that the data has been read */
		set_status(ov, XRT_PDI_PKT_STATUS_IDLE);

		/* Add packet data to linked list */
		if (node)
			node->zn_next = new;
		else
			ov->head = new;
		node = new;

		/* Bail out here if this is the last packet */
		if (pkt->pkt_flags & XRT_PDI_PKT_FLAGS_LAST)
			break;
	}

	/* Set ready flag */
	write_lock(&ov->att_rwlock);
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
			set_status(ov, XRT_PDI_PKT_STATUS_DONE);
		else
			set_status(ov, XRT_PDI_PKT_STATUS_FAIL);
		break;
	}
	read_unlock(&ov->att_rwlock);

	/* Clear ready flag */
	write_lock(&ov->att_rwlock);
	ov->pdi_ready = 0;
	zocl_ov_clean(ov);
	write_unlock(&ov->att_rwlock);

	wait_for_status(ov, XRT_PDI_PKT_STATUS_IDLE);

	return 0;

fail:
	zocl_ov_clean(ov);
	write_unlock(&ov->att_rwlock);

	return ret;
}

/*
 * This is the main thread in zocl ospi versal subdriver.
 *
 * The thread will wake up every second and check the PDI packet
 * status. If there is a new packet ready, it will start load and
 * flash PDI.
 */
static int zocl_ov_thread(void *data)
{
	struct zocl_ov_dev *ov = (struct zocl_ov_dev *)data;

	set_status(ov, XRT_PDI_PKT_STATUS_IDLE);

	while (1) {
		if (kthread_should_stop())
			break;

		if (check_for_status(ov, XRT_PDI_PKT_STATUS_IDLE)) {
			msleep(ZOCL_OV_TIMER_INTERVAL);
			continue;
		}

		zocl_ov_get_pdi(ov);
	}

	return 0;
}

static const struct of_device_id zocl_ospi_versal_of_match[] = {
	{ .compatible = "xlnx,ospi_versal",
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
		ov_err(pdev, "Unable to map OSPI resource: %0lx.\n",
		    PTR_ERR(map));
		return PTR_ERR(map);
	}

	ov = devm_kzalloc(&pdev->dev, sizeof(*ov), GFP_KERNEL);
	if (!ov)
		return -ENOMEM;

	ov->base = map;
	ov->size = res->end - res->start + 1;
	memset_io(ov->base, 0, ov->size);

	rwlock_init(&ov->att_rwlock);

	ret = zocl_ov_init_sysfs(&pdev->dev);
	if (ret) {
		ov_err(pdev, "Unable to create ospi versal sysfs node.\n");
		kfree(ov);
		return ret;
	}

	/* Start the thread. */
	ov->timer_task = kthread_run(zocl_ov_thread, ov, thread_name);
	if (IS_ERR(ov->timer_task)) {
		ret = PTR_ERR(ov->timer_task);

		ov_err(pdev, "Unable to create ospi versal thread.\n");
		kfree(ov);
		return ret;
	}

	platform_set_drvdata(pdev, ov);

	return 0;
}

static int zocl_ov_remove(struct platform_device *pdev)
{
	struct zocl_ov_dev *ov = platform_get_drvdata(pdev);

	zocl_ov_fini_sysfs(&pdev->dev);

	if (ov && ov->timer_task)
		kthread_stop(ov->timer_task);

	return 0;
}

struct platform_driver zocl_ospi_versal_driver = {
	.driver = {
		.name = ZOCL_OSPI_VERSAL_NAME,
		.of_match_table = zocl_ospi_versal_of_match,
	},
	.probe  = zocl_ov_probe,
	.remove = zocl_ov_remove,
};
