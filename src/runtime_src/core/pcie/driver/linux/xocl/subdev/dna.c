/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Chien-Wei Lan <chienwei@xilinx.com>
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

#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/vmalloc.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

/* Registers are defined in pg150-ultrascale-memory-ip.pdf:
 * AXI4-Lite Slave Control/Status Register Map
 */
#define XLNX_DNA_MEMORY_MAP_MAGIC_IS_DEFINED                        (0x3E4D7732)
#define XLNX_DNA_MAJOR_MINOR_VERSION_REGISTER_OFFSET                0x00          //  RO
#define XLNX_DNA_REVISION_REGISTER_OFFSET                           0x04          //  RO
#define XLNX_DNA_CAPABILITY_REGISTER_OFFSET                         0x08          //  RO
//#define XLNX_DNA_SCRATCHPAD_REGISTER_OFFSET                         (0x0C)          //  RO (31-1) + RW (0)
#define XLNX_DNA_STATUS_REGISTER_OFFSET                             0x10            //  RO
#define XLNX_DNA_FSM_DNA_WORD_WRITE_COUNT_REGISTER_OFFSET           (0x14)          //  RO
#define XLNX_DNA_FSM_CERTIFICATE_WORD_WRITE_COUNT_REGISTER_OFFSET   (0x18)          //  RO
#define XLNX_DNA_TIMEOUT_REGISTER_OFFSET                            (0x1C)          //  RO
#define XLNX_DNA_MESSAGE_START_AXI_ONLY_REGISTER_OFFSET             (0x20)          //  RO (31-1) + RW (0)
#define XLNX_DNA_READBACK_REGISTER_2_OFFSET                         0x40            //  RO XLNX_DNA_BOARD_DNA_95_64
#define XLNX_DNA_READBACK_REGISTER_1_OFFSET                         0x44            //  RO XLNX_DNA_BOARD_DNA_63_32
#define XLNX_DNA_READBACK_REGISTER_0_OFFSET                         0x48            //  RO XLNX_DNA_BOARD_DNA_31_0
#define XLNX_DNA_DATA_AXI_ONLY_REGISTER_OFFSET                      (0x80)          //  WO
#define XLNX_DNA_CERTIFICATE_DATA_AXI_ONLY_REGISTER_OFFSET          (0xC0)          //  WO - 512 bit aligned.
#define XLNX_DNA_MAX_ADDRESS_WORDS                                  (0xC4)

#define DEV2XDEV(d) xocl_get_xdev(to_platform_device(d))

#define XLNX_DNA_CAPABILITY_AXI					    0x1
#define XLNX_DNA_CAPABILITY_DRM_ENABLE				    0x100
#define XLNX_DNA_INVALID_CAPABILITY_MASK			    0xFFFFFEEE
#define XLNX_DNA_PRIVILEGED(xlnx_dna)				    ((xlnx_dna)->base != NULL)


#define XLNX_DNA_DEFAULT_EXPIRE_SECS 	1

#define XLNX_DNA_MAX_RES		1

struct xocl_xlnx_dna {
	void __iomem		*base;
	struct device		*xlnx_dna_dev;
	struct mutex		xlnx_dna_lock;
	u64			cache_expire_secs;
	struct xcl_dna		cache;
	ktime_t			cache_expires;
};


enum dna_prop {
	DNA_RAW,
	STATUS,
	CAP,
	VER,
	REVERSION,
};

static void set_xlnx_dna_data(struct xocl_xlnx_dna *xlnx_dna, struct xcl_dna *dna_status)
{
	memcpy(&xlnx_dna->cache, dna_status, sizeof(struct xcl_dna));
	xlnx_dna->cache_expires = ktime_add(ktime_get_boottime(),
		ktime_set(xlnx_dna->cache_expire_secs, 0));
}

static void xlnx_dna_read_from_peer(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna = platform_get_drvdata(pdev);
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	struct xcl_dna dna_status = {0};
	size_t resp_len = sizeof(struct xcl_dna);
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t reqlen = struct_size(mb_req, data, 1) + data_len;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		return;

	mb_req->req = XCL_MAILBOX_REQ_PEER_DATA;
	subdev_peer.size = resp_len;
	subdev_peer.kind = XCL_DNA;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	(void) xocl_peer_request(xdev,
		mb_req, reqlen, &dna_status, &resp_len, NULL, NULL, 0, 0);
	set_xlnx_dna_data(xlnx_dna, &dna_status);

	vfree(mb_req);
}

static void get_xlnx_dna_data(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna = platform_get_drvdata(pdev);
	ktime_t now = ktime_get_boottime();

	if (ktime_compare(now, xlnx_dna->cache_expires) > 0)
		xlnx_dna_read_from_peer(pdev);
}

static void xlnx_dna_get_prop(struct device *dev, enum dna_prop prop, void *val)
{
	struct xocl_xlnx_dna *xlnx_dna = dev_get_drvdata(dev);

	BUG_ON(!xlnx_dna);

	if (XLNX_DNA_PRIVILEGED(xlnx_dna)) {
		switch (prop) {
		case DNA_RAW:
			*((u32 *)val+2) = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_READBACK_REGISTER_2_OFFSET);
			*((u32 *)val+1) = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_READBACK_REGISTER_1_OFFSET);
			*(u32 *)val     = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_READBACK_REGISTER_0_OFFSET);
			break;
		case STATUS:
			*(u32 *)val = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_STATUS_REGISTER_OFFSET);
			break;
		case CAP:
			*(u32 *)val = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_CAPABILITY_REGISTER_OFFSET);
			break;
		case VER:
			*(u32 *)val = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_MAJOR_MINOR_VERSION_REGISTER_OFFSET);
			break;
		case REVERSION:
			*(u32 *)val = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_REVISION_REGISTER_OFFSET);
			break;
		default:
			break;
		}
	} else {

		get_xlnx_dna_data(to_platform_device(dev));

		switch (prop) {
		case DNA_RAW:
			memcpy(val, xlnx_dna->cache.dna, sizeof(u32)*4);
			break;
		case STATUS:
			*(u32 *)val = xlnx_dna->cache.status;
			break;
		case CAP:
			*(u32 *)val = xlnx_dna->cache.capability;
			break;
		case VER:
			*(u32 *)val = xlnx_dna->cache.dna_version;
			break;
		case REVERSION:
			*(u32 *)val = xlnx_dna->cache.revision;
			break;
		default:
			break;
		}
	}
}

static ssize_t status_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	u32 status;

	xlnx_dna_get_prop(dev, STATUS, &status);
	return sprintf(buf, "0x%x\n", status);
}
static DEVICE_ATTR_RO(status);


static ssize_t dna_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	uint32_t dna[4];

	xlnx_dna_get_prop(dev, DNA_RAW, dna);
	return sprintf(buf, "%08x%08x%08x\n", dna[2], dna[1], dna[0]);
}
static DEVICE_ATTR_RO(dna);


static ssize_t capability_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	u32 capability;

	xlnx_dna_get_prop(dev, CAP, &capability);
	return sprintf(buf, "0x%x\n", capability);
}
static DEVICE_ATTR_RO(capability);


static ssize_t dna_version_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	u32 version;

	xlnx_dna_get_prop(dev, VER, &version);

	return sprintf(buf, "%d.%d\n", version>>16, version&0xffff);
}
static DEVICE_ATTR_RO(dna_version);

static ssize_t revision_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{

	u32 revision;

	xlnx_dna_get_prop(dev, REVERSION, &revision);
	return sprintf(buf, "%d\n", revision);
}
static DEVICE_ATTR_RO(revision);

static struct attribute *xlnx_dna_attributes[] = {
	&dev_attr_status.attr,
	&dev_attr_dna.attr,
	&dev_attr_capability.attr,
	&dev_attr_dna_version.attr,
	&dev_attr_revision.attr,
	NULL
};

static const struct attribute_group xlnx_dna_attrgroup = {
	.attrs = xlnx_dna_attributes,
};

static uint32_t dna_status(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna = platform_get_drvdata(pdev);
	struct device *dev  = &pdev->dev;
	uint32_t status = 0;
	uint8_t retries = 10;
	bool rsa4096done = false;

	if (!xlnx_dna)
		return status;

	while (!rsa4096done && retries) {
		xlnx_dna_get_prop(dev, STATUS, &status);
		if (status>>8 & 0x1) {
			rsa4096done = true;
			break;
		}
		msleep(1);
		retries--;
	}

	if (retries == 0)
		return -EBUSY;

	xlnx_dna_get_prop(dev, STATUS, &status);

	return status;
}

static uint32_t dna_capability(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna = platform_get_drvdata(pdev);
	struct device *dev  = &pdev->dev;
	u32 capability = 0;

	if (!xlnx_dna)
		return capability;

	xlnx_dna_get_prop(dev, CAP, &capability);

	return capability;
}

static void dna_write_cert(struct platform_device *pdev, const uint32_t *cert, uint32_t len)
{
	struct xocl_xlnx_dna *xlnx_dna = platform_get_drvdata(pdev);
	struct device *dev  = &pdev->dev;
	int i, j, k;
	u32 status = 0, words;
	uint8_t retries = 100;
	bool sha256done = false;
	uint32_t convert;
	uint32_t sign_start, message_words = (len-512)>>2;

	sign_start = message_words;
	if (!xlnx_dna)
		return;

	if (!XLNX_DNA_PRIVILEGED(xlnx_dna))
		return;

	xocl_dr_reg_write32(DEV2XDEV(dev), 0x1, xlnx_dna->base+XLNX_DNA_MESSAGE_START_AXI_ONLY_REGISTER_OFFSET);
	status = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_STATUS_REGISTER_OFFSET);
	xocl_info(&pdev->dev, "Start: status %08x", status);

	for (i = 0; i < message_words; i += 16) {

		retries = 100;
		sha256done = false;

		while (!sha256done && retries) {
			status = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_STATUS_REGISTER_OFFSET);
			if (!(status>>4 & 0x1)) {
				sha256done = true;
				break;
			}
			msleep(10);
			retries--;
		}
		for (j = 0; j < 16; ++j) {
			convert = (*(cert+i+j)>>24 & 0xff) | (*(cert+i+j)>>8 & 0xff00) | (*(cert+i+j)<<8 & 0xff0000) | ((*(cert+i+j) & 0xff)<<24);
			xocl_dr_reg_write32(DEV2XDEV(dev), convert, xlnx_dna->base+XLNX_DNA_DATA_AXI_ONLY_REGISTER_OFFSET+j*4);
		}
	}
	retries = 100;
	sha256done = false;
	while (!sha256done && retries) {
		status = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_STATUS_REGISTER_OFFSET);
		if (!(status>>4 & 0x1)) {
			sha256done = true;
			break;
		}
		msleep(10);
		retries--;
	}

	status = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_STATUS_REGISTER_OFFSET);
	words  = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_FSM_DNA_WORD_WRITE_COUNT_REGISTER_OFFSET);
	xocl_info(&pdev->dev, "Message: status %08x dna words %d", status, words);

	for (k = 0; k < 128; k += 16) {
		for (i = 0; i < 16; i++) {
			j = k+i+sign_start;
			convert = (*(cert+j)>>24 & 0xff) | (*(cert+j)>>8 & 0xff00) | (*(cert+j)<<8 & 0xff0000) | ((*(cert+j) & 0xff)<<24);
			xocl_dr_reg_write32(DEV2XDEV(dev), convert, xlnx_dna->base+XLNX_DNA_CERTIFICATE_DATA_AXI_ONLY_REGISTER_OFFSET+i*4);
		}
	}

	status = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_STATUS_REGISTER_OFFSET);
	words  = xocl_dr_reg_read32(DEV2XDEV(dev), xlnx_dna->base+XLNX_DNA_FSM_CERTIFICATE_WORD_WRITE_COUNT_REGISTER_OFFSET);
	xocl_info(&pdev->dev, "Signature: status %08x certificate words %d", status, words);
}

static void dna_get_data(struct platform_device *pdev, void *buf)
{
	struct xocl_xlnx_dna *xlnx_dna = platform_get_drvdata(pdev);
	struct xcl_dna dna_status = {0};

	if (!XLNX_DNA_PRIVILEGED(xlnx_dna))
		return;

	xlnx_dna_get_prop(&pdev->dev, STATUS, &dna_status.status);
	xlnx_dna_get_prop(&pdev->dev, CAP, &dna_status.capability);
	xlnx_dna_get_prop(&pdev->dev, DNA_RAW, dna_status.dna);
	xlnx_dna_get_prop(&pdev->dev, VER, &dna_status.dna_version);
	xlnx_dna_get_prop(&pdev->dev, REVERSION, &dna_status.revision);

	memcpy(buf, &dna_status, sizeof(struct xcl_dna));
}

static struct xocl_dna_funcs dna_ops = {
	.status = dna_status,
	.capability = dna_capability,
	.write_cert = dna_write_cert,
	.get_data = dna_get_data,
};


static void mgmt_sysfs_destroy_xlnx_dna(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna;

	xlnx_dna = platform_get_drvdata(pdev);

	sysfs_remove_group(&pdev->dev.kobj, &xlnx_dna_attrgroup);

}

static int mgmt_sysfs_create_xlnx_dna(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna;
	struct xocl_dev_core *core;
	int err;

	xlnx_dna = platform_get_drvdata(pdev);
	core = XDEV(xocl_get_xdev(pdev));

	err = sysfs_create_group(&pdev->dev.kobj, &xlnx_dna_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "create pw group failed: 0x%x", err);
		goto create_grp_failed;
	}

	return 0;

create_grp_failed:
	return err;
}

static int xlnx_dna_probe(struct platform_device *pdev)
{
	struct xocl_xlnx_dna *xlnx_dna;
	struct resource *res;
	int i, err;
	uint32_t capability;

	xlnx_dna = devm_kzalloc(&pdev->dev, sizeof(*xlnx_dna), GFP_KERNEL);
	if (!xlnx_dna)
		return -ENOMEM;


	for (i = 0; i < XLNX_DNA_MAX_RES; ++i) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!res)
			break;

		xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
			res->start, res->end);

		xlnx_dna->base = ioremap_nocache(res->start, res->end - res->start + 1);
		if (!xlnx_dna->base) {
			err = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto failed;
		}
	}
	platform_set_drvdata(pdev, xlnx_dna);

	capability = dna_capability(pdev);
	if (capability & XLNX_DNA_INVALID_CAPABILITY_MASK) {
		xocl_err(&pdev->dev, "DNA IP not detected");
		err = -EINVAL;
		goto create_xlnx_dna_failed;
	} else if (capability & XLNX_DNA_CAPABILITY_DRM_ENABLE &&
		   !(capability & XLNX_DNA_CAPABILITY_AXI)) {
		xocl_err(&pdev->dev, "BRAM version DRM IP is obsoleted, please update xclbin");
		err = -EINVAL;
		goto create_xlnx_dna_failed;
	}

	err = mgmt_sysfs_create_xlnx_dna(pdev);
	if (err)
		goto create_xlnx_dna_failed;

	xlnx_dna->cache_expire_secs = XLNX_DNA_DEFAULT_EXPIRE_SECS;

	return 0;

create_xlnx_dna_failed:
	platform_set_drvdata(pdev, NULL);
failed:
	return err;
}


static int __xlnx_dna_remove(struct platform_device *pdev)
{
	struct xocl_xlnx_dna	*xlnx_dna;

	xlnx_dna = platform_get_drvdata(pdev);
	if (!xlnx_dna) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	mgmt_sysfs_destroy_xlnx_dna(pdev);

	if (xlnx_dna->base)
		iounmap(xlnx_dna->base);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, xlnx_dna);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void xlnx_dna_remove(struct platform_device *pdev)
{
	__xlnx_dna_remove(pdev);
}
#else
#define xlnx_dna_remove __xlnx_dna_remove
#endif

struct xocl_drv_private dna_priv = {
	.ops = &dna_ops,
};

struct platform_device_id xlnx_dna_id_table[] = {
	{ XOCL_DEVNAME(XOCL_DNA), (kernel_ulong_t)&dna_priv },
	{ },
};

static struct platform_driver	xlnx_dna_driver = {
	.probe		= xlnx_dna_probe,
	.remove		= xlnx_dna_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_DNA),
	},
	.id_table = xlnx_dna_id_table,
};

int __init xocl_init_dna(void)
{
	return platform_driver_register(&xlnx_dna_driver);
}

void xocl_fini_dna(void)
{
	platform_driver_unregister(&xlnx_dna_driver);
}
