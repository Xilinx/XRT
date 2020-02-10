/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2019-2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: Larry Liu <yliu@xilinx.com>
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

/*
 * This subdriver is used to transfer Firmware Image from host memory
 * to device memory through a dedicated BRAM. This BRAM is mapped to
 * mgmt function and both drivers running on host and device can access
 * this BRAM. The first 4 Bytes of the BRAM is the packet header and
 * others are data payload.
 *
 *                      ------------------
 *                     |    pkt_status    |
 *                     |------------------|
 *                     |    pkt_flags     |
 *                     |------------------|
 *                     |    pkt_size (H)  |
 *                     |------------------|
 *                     |    pkt_size (L)  |
 *                     |------------------|
 *                     |    pkt_data      |
 *                     |      ...         |
 *                     |      ...         |
 *                      ------------------
 *
 * The pkt_status fields are used to communication between host and
 * device driver.
 * 1) The status will be set to IDLE initially
 * 2) The host driver will set it to NEW after it fill the data payload
 *    and flags
 * 3) The device driver will read the data payload and set the status to
 *    IDEL after data has been read so that host driver can write next
 *    data...
 * 4) Once the last packet is sent, host will wait for program done
 *    status or program fail status, setting by device driver according
 *    to the result of ospi flash.
 * 5) Host driver will clear the status to IDLE for next ospi flash
 *
 * The pkt_flags fields are used to indicate the packet attributes.
 * Currently, only one flag is used which will indicate this is the
 * last packet of the Firmware Image.
 *
 * The pkt_size fields are used to indicate the data payload size
 * in bytes.
 *
 * The pkt_data is the actual Firmware image data. The Firmware
 * image data will be split to fragments into pkt_data to fit
 * the size of BRAM.
 */

#include "../xocl_drv.h"
#include "xrt_drv.h"

#define	OSPI_VERSAL_DEV_NAME "ospi_versal" SUBDEV_SUFFIX

/* Timer interval of checking OSPI done */
#define OSPI_VERSAL_TIMER_INTERVAL	(1000)

struct ospi_versal {
	struct platform_device	*ov_pdev;
	void __iomem		*ov_base;
	size_t			ov_size;
	size_t			ov_data_size;
	bool			ov_inuse;

	struct mutex		ov_lock;
};

#define	OV_ERR(ov, fmt, arg...)    \
	xocl_err(&ov->ov_pdev->dev, fmt "\n", ##arg)
#define	OV_INFO(ov, fmt, arg...)    \
	xocl_info(&ov->ov_pdev->dev, fmt "\n", ##arg)

/*
 * If return 0, we get the expected status.
 * If return -1, the status is set to FAIL.
 */
static inline int wait_for_status(struct ospi_versal *ov, u8 status)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	for (;;) {
		header = ioread32(ov->ov_base);
		if (pkt->pkt_status == status)
			return 0;
		if (pkt->pkt_status == XRT_PDI_PKT_STATUS_FAIL)
			return -1;
	}
}

static inline void set_status(struct ospi_versal *ov, u8 status)
{
	struct pdi_packet pkt;

	pkt.pkt_status = status;
	iowrite32(pkt.header, ov->ov_base);
}

/*
 * If return 0, we get the expected status.
 * If return 1, current status is not FAIL and 'status'
 * If return -1, the status is set to FAIL.
 */
static inline int check_for_status(struct ospi_versal *ov, u8 status)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	header = ioread32(ov->ov_base);
	if (pkt->pkt_status == status)
		return 0;
	if (pkt->pkt_status == XRT_PDI_PKT_STATUS_FAIL)
		return -1;
	return 1;
}

static inline void write_data(u32 *addr, u32 *data, size_t sz)
{
	int i;

	for (i = sz - 1; i >= 0; i--)
		iowrite32(data[i], addr + i);
}

static ssize_t ospi_versal_write(struct file *filp, const char __user *data,
	size_t data_len, loff_t *off)
{
	struct ospi_versal *ov = filp->private_data;
	ssize_t len = 0, ret;
	ssize_t remain = data_len;
	u32 *pkt_data, pkt_size, tran_size;
	u32 *base_addr = ov->ov_base;
	struct pdi_packet pkt;

	/* We don't support program partial of the ospi flash */
	if (*off != 0) {
		OV_ERR(ov, "OSPI offset is not 0: %lld", *off);
		return -EINVAL;
	}

	mutex_lock(&ov->ov_lock);
	if (ov->ov_inuse) {
		mutex_unlock(&ov->ov_lock);
		OV_ERR(ov, "OSPI device is busy");
		return -EBUSY;
	}

	ov->ov_inuse = true;
	mutex_unlock(&ov->ov_lock);

	if (wait_for_status(ov, XRT_PDI_PKT_STATUS_IDLE)) {
		OV_ERR(ov, "OSPI device is not in proper state");
		return -EIO;
	}

	pkt_size = ov->ov_data_size;
	pkt_data = vmalloc(pkt_size);

	while (len < data_len) {
		tran_size = (remain > pkt_size) ? pkt_size : remain;
		ret = copy_from_user(pkt_data, data + len, tran_size);
		if (ret) {
			OV_ERR(ov, "copy data failed %ld", ret);
			goto done;
		}

		write_data(base_addr + (sizeof(struct pdi_packet)) / 4,
		    pkt_data, pkt_size / 4);

		pkt.pkt_status = XRT_PDI_PKT_STATUS_NEW;
		pkt.pkt_flags = tran_size < pkt_size ?
		    XRT_PDI_PKT_FLAGS_LAST : 0;
		pkt.pkt_size = tran_size;

		/* Write data on 4 bytes base */
		write_data(base_addr, &pkt.header,
		    sizeof(struct pdi_packet) / 4);

		len += tran_size;
		remain -= tran_size;

		/* Give up CPU to avoid taking too much CPU cycles */
		schedule();

		/* wait until the data is fetched by device side */
		if (wait_for_status(ov, XRT_PDI_PKT_STATUS_IDLE)) {
			ret = -EIO;
			OV_ERR(ov, "OSPI program error");
			goto done;
		}
	}

	/* wait until the ospi flash is done */
	while (true) {
		int status;
		status = check_for_status(ov, XRT_PDI_PKT_STATUS_DONE);
		if (status == -1) {
			OV_ERR(ov, "OSPI program error");
			ret = -EIO;
			goto done;
		}

		if (status == 0)
			break;

		msleep(OSPI_VERSAL_TIMER_INTERVAL);
	}

	OV_INFO(ov, "OSPI program is completed");
	ret = len;
done:
	set_status(ov, XRT_PDI_PKT_STATUS_IDLE);

	vfree(pkt_data);
	mutex_lock(&ov->ov_lock);
	ov->ov_inuse = false;
	mutex_unlock(&ov->ov_lock);

	return ret;
}

static int ospi_versal_open(struct inode *inode, struct file *file)
{
	struct ospi_versal *ov = NULL;

	ov = xocl_drvinst_open(inode->i_cdev);
	if (!ov)
		return -ENXIO;

	file->private_data = ov;
	return 0;
}

static int ospi_versal_close(struct inode *inode, struct file *file)
{
	struct ospi_versal *ov = file->private_data;

	xocl_drvinst_close(ov);
	return 0;
}

static int ospi_versal_remove(struct platform_device *pdev)
{
	struct ospi_versal *ov = platform_get_drvdata(pdev);

	if (!ov) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	if (ov->ov_base)
		iounmap(ov->ov_base);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(ov);

	return 0;
}

static int ospi_versal_probe(struct platform_device *pdev)
{
	struct ospi_versal *ov = NULL;
	struct resource *res;
	int ret;

	ov = xocl_drvinst_alloc(&pdev->dev, sizeof(struct ospi_versal));
	if (!ov)
		return -ENOMEM;
	platform_set_drvdata(pdev, ov);
	ov->ov_pdev = pdev;

	mutex_init(&ov->ov_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	ov->ov_base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!ov->ov_base) {
		OV_ERR(ov, "failed to map in BRAM");
		ret = -EIO;
		goto failed;
	}
	ov->ov_size = res->end - res->start + 1;
	ov->ov_data_size = ov->ov_size - sizeof(struct pdi_packet);
	if (ov->ov_size % 4 || ov->ov_data_size % 4) {
		OV_ERR(ov, "BRAM size is not 4 Bytes aligned");
		ret = -EINVAL;
		goto failed;
	}

	return 0;

failed:
	if (ov->ov_base) {
		iounmap(ov->ov_base);
		ov->ov_base = NULL;
	}
	ospi_versal_remove(pdev);

	return ret;
}

static const struct file_operations ospi_versal_fops = {
	.owner = THIS_MODULE,
	.open = ospi_versal_open,
	.release = ospi_versal_close,
	.write = ospi_versal_write,
};

struct xocl_drv_private ospi_versal_priv = {
	.fops = &ospi_versal_fops,
	.dev = -1,
};

struct platform_device_id ospi_versal_id_table[] = {
	{ XOCL_DEVNAME(XOCL_OSPI_VERSAL),
	    (kernel_ulong_t)&ospi_versal_priv },
	{ },
};

static struct platform_driver	ospi_versal_driver = {
	.probe		= ospi_versal_probe,
	.remove		= ospi_versal_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_OSPI_VERSAL),
	},
	.id_table = ospi_versal_id_table,
};

int __init xocl_init_ospi_versal(void)
{
	int err = 0;

	err = alloc_chrdev_region(&ospi_versal_priv.dev, 0, XOCL_MAX_DEVICES,
	    OSPI_VERSAL_DEV_NAME);
	if (err < 0)
		return err;

	err = platform_driver_register(&ospi_versal_driver);
	if (err) {
		unregister_chrdev_region(ospi_versal_priv.dev,
		    XOCL_MAX_DEVICES);
		return err;
	}

	return 0;
}

void xocl_fini_ospi_versal(void)
{
	unregister_chrdev_region(ospi_versal_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&ospi_versal_driver);
}
