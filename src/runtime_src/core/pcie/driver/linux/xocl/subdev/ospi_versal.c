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
 * This subdriver is used to transfer data, e.g. firmware image, XCLBIN
 * from host memory to device memory through a dedicated BRAM. This BRAM
 * is mapped to mgmt function and both drivers running on host and device
 * can access this BRAM. The first 4 Bytes of the BRAM is the packet
 * header and others are data payload.
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
 *
 * Layout of packet header
 * 31 - 16   15 - 14   13 - 12   11 - 9    8    7 - 0
 * -----------------------------------------------------
 * |    |    |    |    |    |    |    |    |    |----| pkt_status
 * |    |    |    |    |    |    |    |    |---------- pkt_flags: last packet
 * |    |    |    |    |    |    |----|--------------- pkt_flags: pkt type
 * |    |    |    |    |----|------------------------- pkt_flags: version
 * |    |    |----|----------------------------------- pkt_flags: reserved
 * |----|--------------------------------------------- pkt_size
 *
 * The pkt_status fields are used to communication between host and
 * device driver.
 * 1) The status will be set to IDLE initially
 * 2) The host driver will set it to NEW after it fill the data payload
 *    and flags
 * 3) The device driver will read the data payload and set the status to
 *    IDEL after data has been read so that host driver can write next
 *    data...
 * 4) Once the last packet is sent, host will wait for the done
 *    status or fail status, setting by device driver according
 *    to the result of whole package handling, where PDI image will
 *    be handled by ospi_flash daemon; XCLBIN image will be handled
 *    by zocl driver xclbin service.
 * 5) Host driver will clear the status to IDLE for next data transfer
 *
 * The pkt_flags fields are used to indicate the packet attributes.
 *	last packet flag : set if the last packet of the Data Image.
 *	type flag        : we currently support load PDI and XCLBIN
 *	version flag     : set by zocl to indicate the protocol version.
 *	                   the current version is set to 1. we need this
 *	                   field to support old shell (including golden
 *	                   image) where there is no version field and thus
 *	                   these fields were set to 0.
 *
 * The pkt_size fields are used to indicate the data payload size
 * in bytes.
 *
 * The pkt_data is the actual data image. The data image will be split
 * to fragments into pkt_data to fit the size of BRAM.
 */

#include "../xocl_drv.h"
#include "xrt_drv.h"

#define	XFER_VERSAL_DEV_NAME "xfer_versal" SUBDEV_SUFFIX

/* Timer interval of checking OSPI done */
#define XFER_VERSAL_TIMER_INTERVAL	(1000)

struct xfer_versal {
	struct platform_device	*xv_pdev;
	void __iomem		*xv_base;
	size_t			xv_size;
	size_t			xv_data_size;
	bool			xv_inuse;

	struct mutex		xv_lock;
};

#define	XV_ERR(xv, fmt, arg...)    \
	xocl_err(&xv->xv_pdev->dev, fmt "\n", ##arg)
#define	XV_INFO(xv, fmt, arg...)    \
	xocl_info(&xv->xv_pdev->dev, fmt "\n", ##arg)
#define	XV_DEBUG(xv, fmt, arg...)    \
	xocl_dbg(&xv->xv_pdev->dev, fmt "\n", ##arg)

/*
 * If return 0, we get the expected status.
 * If return -1, the status is set to FAIL.
 */
static inline int wait_for_status(struct xfer_versal *xv, u8 status)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	for (;;) {
		header = ioread32(xv->xv_base);
		if (pkt->pkt_status == status)
			return 0;
		if (pkt->pkt_status == XRT_XFR_PKT_STATUS_FAIL)
			return -1;
	}
}

static inline void set_status(struct xfer_versal *xv, u8 status)
{
	struct pdi_packet pkt;

	pkt.pkt_status = status;
	iowrite32(pkt.header, xv->xv_base);
}

/*
 * If return 0, we get the expected status.
 * If return 1, current status is not FAIL and 'status'
 * If return -1, the status is set to FAIL.
 */
static inline int check_for_status(struct xfer_versal *xv, u8 status)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	header = ioread32(xv->xv_base);
	if (pkt->pkt_status == status)
		return 0;
	if (pkt->pkt_status == XRT_XFR_PKT_STATUS_FAIL)
		return -1;
	return 1;
}

static inline u8 get_pkt_flags(struct xfer_versal *xv)
{
	struct pdi_packet *pkt;
	u32 header;

	pkt = (struct pdi_packet *)&header;
	header = ioread32(xv->xv_base);
	return pkt->pkt_flags;
}

static inline void write_data(u32 *addr, u32 *data, size_t sz)
{
	int i;

	for (i = sz - 1; i >= 0; i--)
		iowrite32(data[i], addr + i);
}

static ssize_t xfer_versal_transfer(struct xfer_versal *xv, const char *data,
		size_t data_len, u8 flags)
{
	ssize_t len = 0, ret;
	ssize_t remain = data_len;
	u32 *pkt_data, pkt_size, tran_size;
	u32 *base_addr = xv->xv_base;
	struct pdi_packet pkt;
	int mod;
	int next = 0;

	pkt_size = xv->xv_data_size;

	XV_INFO(xv, "start writting data_len: %lu", data_len);

	while (len < data_len) {
		tran_size = (remain > pkt_size) ? pkt_size : remain;
		pkt_data = (u32 *)(data + len);
		mod = tran_size % 4;

		if (mod == 0) {
			write_data(base_addr + (sizeof(struct pdi_packet)) / 4,
			    pkt_data, tran_size / 4);
		} else {
			u32 resid;

			write_data(base_addr + (sizeof(struct pdi_packet)) / 4,
			    pkt_data, (tran_size - mod) / 4);
			memcpy(&resid, data + data_len - mod, mod);
			write_data(base_addr + (sizeof(struct pdi_packet)) / 4 +
			    (tran_size / 4), &resid, 1);
		}

		pkt.pkt_status = XRT_XFR_PKT_STATUS_NEW;
		pkt.pkt_flags = (tran_size < pkt_size ?
		    XRT_XFR_PKT_FLAGS_LAST : 0) | flags;
		pkt.pkt_size = tran_size;

		/* Write data on 4 bytes base */
		write_data(base_addr, &pkt.header,
		    sizeof(struct pdi_packet) / 4);

		len += tran_size;
		remain -= tran_size;

		if ((len / 1000000) > next) {
			XV_INFO(xv, "%lu M write %lu, remain %lu",
			    (len / 1000000), len, remain);
			next++;
		}

		/* Give up CPU to avoid taking too much CPU cycles */
		schedule();

		/* wait until the data is fetched by device side */
		if (wait_for_status(xv, XRT_XFR_PKT_STATUS_IDLE)) {
			ret = -EIO;
			XV_ERR(xv, "Data transfer error");
			goto done;
		}
	}

	XV_INFO(xv, "copy file to device done");

	/* wait until the data is done */
	while (true) {
		int status;

		status = check_for_status(xv, XRT_XFR_PKT_STATUS_DONE);
		if (status == -1) {
			XV_ERR(xv, "Data handle error");
			ret = -EIO;
			goto done;
		}

		if (status == 0)
			break;

		msleep(XFER_VERSAL_TIMER_INTERVAL);
	}

	XV_INFO(xv, "Data transfer is completed");
	ret = len;

done:
	set_status(xv, XRT_XFR_PKT_STATUS_IDLE);

	return ret;
}

static ssize_t xfer_versal_write(struct file *filp, const char __user *udata,
	size_t data_len, loff_t *off)
{
	struct xfer_versal *xv = filp->private_data;
	ssize_t ret;
	char *kdata = NULL;

	/* We don't support program partial of the ospi flash */
	if (*off != 0) {
		XV_ERR(xv, "OSPI offset is not 0: %lld", *off);
		return -EINVAL;
	}

	mutex_lock(&xv->xv_lock);
	if (xv->xv_inuse) {
		mutex_unlock(&xv->xv_lock);
		XV_ERR(xv, "OSPI device is busy");
		return -EBUSY;
	}

	xv->xv_inuse = true;
	mutex_unlock(&xv->xv_lock);

	if (wait_for_status(xv, XRT_XFR_PKT_STATUS_IDLE)) {
		XV_ERR(xv, "OSPI device is not in proper state");
		ret = -EIO;
		goto done;
	}

	kdata = vmalloc(data_len);
	if (!kdata) {
		XV_ERR(xv, "Can't create xfer buffer");
		ret = -ENOMEM;
		goto done;
	}

	ret = copy_from_user(kdata, udata, data_len);
	if (ret) {
		XV_ERR(xv, "copy data failed %ld", ret);
		goto done;
	}

	ret = xfer_versal_transfer(xv, kdata, data_len, XRT_XFR_PKT_FLAGS_PDI);

done:
	mutex_lock(&xv->xv_lock);
	xv->xv_inuse = false;
	mutex_unlock(&xv->xv_lock);
	vfree(kdata);

	return ret;
}

static int xfer_versal_download_axlf(struct platform_device *pdev,
		const void *u_xclbin)
{
	struct xfer_versal *xv = platform_get_drvdata(pdev);
	struct axlf *xclbin = (struct axlf *)u_xclbin;
	uint64_t xclbin_len = xclbin->m_header.m_length;
	u8 pkt_flags, pkt_ver;
	int ret;

	mutex_lock(&xv->xv_lock);
	if (xv->xv_inuse) {
		mutex_unlock(&xv->xv_lock);
		XV_ERR(xv, "XFER device is busy");
		return -EBUSY;
	}

	xv->xv_inuse = true;
	mutex_unlock(&xv->xv_lock);

	pkt_flags = get_pkt_flags(xv);
	pkt_ver = pkt_flags >> XRT_XFR_PKT_VER_SHIFT & XRT_XFR_PKT_VER_MASK;
	if (pkt_ver != XRT_XFR_VER) {
		XV_ERR(xv, "Platform does not support load xclbin");
		ret = -ENOTSUPP;
		goto done;
	}

	ret = xfer_versal_transfer(xv, u_xclbin, xclbin_len,
	    XRT_XFR_PKT_FLAGS_XCLBIN);
	ret = ret == xclbin_len ? 0 : -EIO;

done:
	mutex_lock(&xv->xv_lock);
	xv->xv_inuse = false;
	mutex_unlock(&xv->xv_lock);

	return ret;
}

static const struct axlf_section_header *get_axlf_section_hdr(
	struct xfer_versal *xv, const struct axlf *top, enum axlf_section_kind kind)
{
	int i;
	const struct axlf_section_header *hdr = NULL;

	for (i = 0; i < top->m_header.m_numSections; i++) {
		if (top->m_sections[i].m_sectionKind == kind) {
			hdr = &top->m_sections[i];
			break;
		}
	}

	if (hdr) {
		if ((hdr->m_sectionOffset + hdr->m_sectionSize) >
			top->m_header.m_length) {
			XV_ERR(xv, "found section %d is invalid", kind);
			hdr = NULL;
		} else {
			XV_INFO(xv, "section %d offset: %llu, size: %llu",
				kind, hdr->m_sectionOffset, hdr->m_sectionSize);
		}
	} else {
		XV_INFO(xv, "could not find section header %d", kind);
	}

	return hdr;
}

static void parse_partition_metadata(struct xfer_versal *xv,
	const struct axlf *top, void **addr, uint64_t *size)
{
	void *section = NULL;
	const struct axlf_section_header *hdr =
		get_axlf_section_hdr(xv, top, PARTITION_METADATA);

	if (hdr == NULL)
		return;

	section = vmalloc(hdr->m_sectionSize);
	if (section == NULL)
		return;

	memcpy(section, ((const char *)top) + hdr->m_sectionOffset,
		hdr->m_sectionSize);

	*addr = section;
	*size = hdr->m_sectionSize;
}

/* Note: this is a workaround for enabling ULP level clock after xclbin
 * download. We will have new-code to replace this api. For fast fix, just
 * enable it temporarily.
 */
static int xclbin_load_axlf(struct platform_device *pdev, const void *buf)
{
	struct xfer_versal *xv = platform_get_drvdata(pdev);
	struct axlf *xclbin = (struct axlf *)buf;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	void *metadata = NULL;	
	uint64_t size;
	struct xocl_subdev *urpdevs;
	int i, ret = 0, num_dev = 0;
	
	parse_partition_metadata(xv, xclbin, &metadata, &size);
	if (metadata) {
		num_dev = xocl_fdt_parse_blob(xdev, metadata,
		    size, &urpdevs);
		vfree(metadata);
	}
	xocl_subdev_destroy_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);

	/* download bitstream */
	ret = xfer_versal_download_axlf(pdev, buf);

	if (num_dev) {
		for (i = 0; i < num_dev; i++)
			(void) xocl_subdev_create(xdev, &urpdevs[i].info);
		xocl_subdev_create_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);
	}

	return ret;
}

/* Kernel APIs exported from this sub-device driver */
static struct xocl_xfer_versal_funcs xfer_versal_ops = {
	.download_axlf = xfer_versal_download_axlf,
	.xclbin_load_axlf = xclbin_load_axlf,
};

static int xfer_versal_open(struct inode *inode, struct file *file)
{
	struct xfer_versal *xv = NULL;

	xv = xocl_drvinst_open(inode->i_cdev);
	if (!xv)
		return -ENXIO;

	file->private_data = xv;
	return 0;
}

static int xfer_versal_close(struct inode *inode, struct file *file)
{
	struct xfer_versal *xv = file->private_data;

	xocl_drvinst_close(xv);
	return 0;
}

static int xfer_versal_remove(struct platform_device *pdev)
{
	struct xfer_versal *xv = platform_get_drvdata(pdev);
	void *hdl;
	int ret = 0;

	if (!xv) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	xocl_drvinst_release(xv, &hdl);
	if (xv->xv_base)
		iounmap(xv->xv_base);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	XV_INFO(xv, "return: %d", ret);
	return ret;
}

static int xfer_versal_probe(struct platform_device *pdev)
{
	struct xfer_versal *xv = NULL;
	struct resource *res;
	int ret = 0;

	xv = xocl_drvinst_alloc(&pdev->dev, sizeof(struct xfer_versal));
	if (!xv)
		return -ENOMEM;
	platform_set_drvdata(pdev, xv);
	xv->xv_pdev = pdev;

	mutex_init(&xv->xv_lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		XV_ERR(xv, "failed to get resource");
		return ret;
	}

	xv->xv_base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!xv->xv_base) {
		XV_ERR(xv, "failed to map in BRAM");
		ret = -EIO;
		goto failed;
	}
	xv->xv_size = res->end - res->start + 1;
	xv->xv_data_size = xv->xv_size - sizeof(struct pdi_packet);
	if (xv->xv_size % 4 || xv->xv_data_size % 4) {
		XV_ERR(xv, "BRAM size is not 4 Bytes aligned");
		ret = -EINVAL;
		goto failed;
	}

	XV_INFO(xv, "return: %d", ret);
	return 0;

failed:
	if (xv->xv_base) {
		iounmap(xv->xv_base);
		xv->xv_base = NULL;
	}
	xfer_versal_remove(pdev);

	XV_INFO(xv, "return: %d", ret);
	return ret;
}

static const struct file_operations xfer_versal_fops = {
	.owner = THIS_MODULE,
	.open = xfer_versal_open,
	.release = xfer_versal_close,
	.write = xfer_versal_write,
};

struct xocl_drv_private xfer_versal_priv = {
	.ops = &xfer_versal_ops,
	.fops = &xfer_versal_fops,
	.dev = -1,
};

struct platform_device_id xfer_versal_id_table[] = {
	{ XOCL_DEVNAME(XOCL_XFER_VERSAL),
	    (kernel_ulong_t)&xfer_versal_priv },
	{ },
};

static struct platform_driver	xfer_versal_driver = {
	.probe		= xfer_versal_probe,
	.remove		= xfer_versal_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_XFER_VERSAL),
	},
	.id_table = xfer_versal_id_table,
};

int __init xocl_init_xfer_versal(void)
{
	int err = 0;

	err = alloc_chrdev_region(&xfer_versal_priv.dev, 0, XOCL_MAX_DEVICES,
	    XFER_VERSAL_DEV_NAME);
	if (err < 0)
		return err;

	err = platform_driver_register(&xfer_versal_driver);
	if (err) {
		unregister_chrdev_region(xfer_versal_priv.dev,
		    XOCL_MAX_DEVICES);
		return err;
	}

	return 0;
}

void xocl_fini_xfer_versal(void)
{
	unregister_chrdev_region(xfer_versal_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&xfer_versal_driver);
}
