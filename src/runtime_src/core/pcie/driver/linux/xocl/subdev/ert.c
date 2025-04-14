/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: lizhi.hou@xilinx.com
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

#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <ert.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

#define MAX_ERT_RETRY			10
#define RETRY_INTERVAL			100

#define	GPIO_RESET			0x0
#define	GPIO_ENABLED			0x1

#define SELF_JUMP_INS			0xb8000000
#define	SELF_JUMP(ins)			(((ins) & 0xfc00ffff) == SELF_JUMP_INS)

#define READ_GPIO(ert, off)			\
	(ert->reset_addr ? XOCL_READ_REG32(ert->reset_addr + off) : 0)
#define WRITE_GPIO(ert, val, off)			\
	(ert->reset_addr ? XOCL_WRITE_REG32(val, ert->reset_addr + off) : 0)

#define READ_CQ(ert, off)			\
	(ert->cq_addr ? XOCL_READ_REG32(ert->cq_addr + off) : 0)
#define WRITE_CQ(ert, val, off)			\
	(ert->cq_addr ? XOCL_WRITE_REG32(val, ert->cq_addr + off) : 0)

#define	COPY_SCHE(ert, buf, len)		\
	(ert->fw_addr ?			\
	 xocl_memcpy_toio(ert->fw_addr, buf, len) : 0)

/* mutex to handle race conditions when accessing image address */
static DEFINE_MUTEX(ert_mutex);

enum {
	MB_UNINITIALIZED,
	MB_INITIALIZED,
	MB_HOLD_RESET,
	MB_ENABLED,
	MB_RUNNING,
};

struct xocl_ert {
	struct platform_device	*pdev;
	void __iomem		*fw_addr;
	void __iomem		*cq_addr;
	void __iomem		*reset_addr;
	u32			fw_ram_len;

	u32			state;
	bool			sysfs_created;

	char			*sche_binary;
	u32			sche_binary_length;

	struct mutex		ert_lock;

	u32			cq_len;
};

static int stop_ert_nolock(struct xocl_ert *ert)
{
	u32 reg_val = 0;
	u32 retry = 0;
	int ret = 0;

	if (ert->state == MB_UNINITIALIZED)
		return -ENODEV;
	if (ert->state  < MB_RUNNING || !ert->cq_addr)
		return 0;

	if (SELF_JUMP(XOCL_READ_REG32(ert->fw_addr))) {
		xocl_info(&ert->pdev->dev, "MB is self jump");
		return 0;
	}

	xocl_info(&ert->pdev->dev, "Stopping scheduler...");

	reg_val = READ_GPIO(ert, 0);
	if (reg_val != GPIO_ENABLED)
		WRITE_GPIO(ert, GPIO_ENABLED, 0);

	/*
	 * New SSv3 platforms does not have command queue exposed to
	 * mgmtpf anymore. Start/Stop ERT command will happen on
	 * userpf side. In the case that xocl driver is running ERT
	 * and xclmgmt stops ERT at the same time, userpf firewall
	 * will trip. This should not be a normal running case.
	 * And firewall trip makes sense.
	 */
	retry = 0;
	while ((READ_CQ(ert, 0) != (ERT_EXIT_CMD_OP | ERT_EXIT_ACK)) &&
			retry++ < MAX_ERT_RETRY) {
		WRITE_CQ(ert, ERT_EXIT_CMD, 0);
		msleep(RETRY_INTERVAL);
	}
	if (retry >= MAX_ERT_RETRY) {
		xocl_info(&ert->pdev->dev, "Failed to stop ERT");
		ret = EIO;
	}
	xocl_info(&ert->pdev->dev, "ERT is stopped, %d", retry);
	ert->state = MB_ENABLED;

	return ret;
}

static int load_image(struct xocl_ert *ert)
{
	int ret = 0;
	u32 reg_val = 0;
	xdev_handle_t xdev_hdl = xocl_get_xdev(ert->pdev);

	mutex_lock(&ert->ert_lock);
	ret = stop_ert_nolock(ert);
	if (ret)
		goto out;

	WRITE_GPIO(ert, GPIO_RESET, 0);
	reg_val = READ_GPIO(ert, 0);

	xocl_info(&ert->pdev->dev, "ERT Reset GPIO 0x%x", reg_val);
	if (reg_val != GPIO_RESET) {
		xocl_err(&ert->pdev->dev, "Hold reset GPIO Failed");
		ret = EIO;
		goto out;
	}

	ert->state = MB_HOLD_RESET;

	xdev_hdl = xocl_get_xdev(ert->pdev);

	/* load ERT Image */
	if (xocl_mb_sched_on(xdev_hdl) && ert->sche_binary_length) {
		xocl_info(&ert->pdev->dev, "Copying scheduler image len %d",
			ert->sche_binary_length);
		COPY_SCHE(ert, ert->sche_binary, ert->sche_binary_length);
	}

	WRITE_GPIO(ert, GPIO_ENABLED, 0);
	reg_val = READ_GPIO(ert, 0);
	xocl_info(&ert->pdev->dev, "ERT Reset GPIO 0x%x", reg_val);
	if (reg_val != GPIO_ENABLED) {
		xocl_err(&ert->pdev->dev, "Enable GPIO failed");
		ret = EIO;
		goto out;
	}

	/* ert->state = MB_ENABLED; */
	/* write ERT_CU_STAT to check if ERT is up and running */
	ert->state = MB_RUNNING;

out:
	mutex_unlock(&ert->ert_lock);

	return (ret == -ENODEV) ? 0 : ret; 
}

static ssize_t reset_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_ert *ert = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 1)
		return -EINVAL;

	if (val)
		load_image(ert);

	return count;
}
static DEVICE_ATTR_WO(reset);

static ssize_t image_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct xocl_ert *ert =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	ssize_t ret = 0;

	if (!ert->sche_binary)
		goto bail;

	if (off >= ert->sche_binary_length)
		goto bail;

	if (off + count > ert->sche_binary_length)
		count = ert->sche_binary_length - off;

	memcpy(buf, ert->sche_binary + off, count);

	ret = count;
bail:
	return ret;
}

static size_t _image_write(char **image, size_t sz,
		char *buffer, loff_t off, size_t count)
{
	char *tmp_buf;
	size_t total;

	if (off == 0) {
		if (*image)
			vfree(*image);
		*image = vmalloc(count);
		if (!*image)
			return 0;

		memcpy(*image, buffer, count);
		return count;
	}

	total = off + count;
	if (total > sz) {
		tmp_buf = vmalloc(total);
		if (!tmp_buf) {
			vfree(*image);
			*image = NULL;
			return 0;
		}
		memcpy(tmp_buf, *image, sz);
		vfree(*image);
		sz = total;
	} else {
		tmp_buf = *image;
	}

	memcpy(tmp_buf + off, buffer, count);
	*image = tmp_buf;

	return sz;
}

static ssize_t image_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t off, size_t count)
{
	struct xocl_ert *ert =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	mutex_lock(&ert_mutex);
	ert->sche_binary_length = (u32)_image_write(&ert->sche_binary,
			ert->sche_binary_length, buffer, off, count);
	mutex_unlock(&ert_mutex);

	return ert->sche_binary_length ? count : -ENOMEM;
}

static struct bin_attribute ert_image_attr = {
	.attr = {
		.name = "image",
		.mode = 0600
	},
	.read = image_read,
	.write = image_write,
	.size = 0
};

static struct bin_attribute *ert_bin_attrs[] = {
	&ert_image_attr,
	NULL,
};

static struct attribute *ert_attrs[] = {
	&dev_attr_reset.attr,
	NULL,
};

static struct attribute_group ert_attr_group = {
	.attrs = ert_attrs,
	.bin_attrs = ert_bin_attrs,
};

static void ert_sysfs_destroy(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &ert_attr_group);
}

static int ert_sysfs_create(struct platform_device *pdev)
{
	int err;

	err = sysfs_create_group(&pdev->dev.kobj, &ert_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create ert attrs failed: 0x%x", err);
	}

	return err;
}

static int stop_ert(struct platform_device *pdev)
{
	struct xocl_ert *ert;
	int ret = 0;

	xocl_info(&pdev->dev, "Stop Microblaze...");
	ert = platform_get_drvdata(pdev);
	if (!ert)
		return -ENODEV;

	mutex_lock(&ert->ert_lock);
	ret = stop_ert_nolock(ert);
	mutex_unlock(&ert->ert_lock);

	return ret;
}

static int load_sche_image(struct platform_device *pdev, const char *image,
	u32 len)
{
	struct xocl_ert *ert;
	char *binary = NULL;

	ert = platform_get_drvdata(pdev);
	if (!ert)
		return -EINVAL;

	if (len > ert->fw_ram_len) {
		xocl_err(&pdev->dev, "image is too big %d, ram size %d",
				len, ert->fw_ram_len);
		return -EINVAL;
	}

	binary = ert->sche_binary;
	ert->sche_binary = vmalloc(len);
	if (!ert->sche_binary)
		return -ENOMEM;

	if (binary)
		vfree(binary);
	memcpy(ert->sche_binary, image, len);
	ert->sche_binary_length = len;

	return 0;
}

static int ert_reset(struct platform_device *pdev)
{
	struct xocl_ert *ert;

	xocl_info(&pdev->dev, "Reset ERT...");
	ert = platform_get_drvdata(pdev);

	load_image(ert);

	return 0;
}

static struct xocl_mb_funcs ert_ops = {
	.load_sche_image	= load_sche_image,
	.reset			= ert_reset,
	.stop			= stop_ert,
};

static int __ert_remove(struct platform_device *pdev)
{
	struct xocl_ert *ert;
	void *hdl;

	ert = platform_get_drvdata(pdev);
	if (!ert)
		return 0;

	xocl_drvinst_release(ert, &hdl);

	stop_ert(pdev);

	if (ert->sche_binary)
		vfree(ert->sche_binary);

	if (ert->sysfs_created)
		ert_sysfs_destroy(pdev);

	if (ert->fw_addr)
		iounmap(ert->fw_addr);

	if (ert->cq_addr)
		iounmap(ert->cq_addr);

	if (ert->reset_addr)
		iounmap(ert->reset_addr);

	mutex_destroy(&ert->ert_lock);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void ert_remove(struct platform_device *pdev)
{
	__ert_remove(pdev);
}
#else
#define ert_remove __ert_remove
#endif

static int ert_probe(struct platform_device *pdev)
{
	struct xocl_ert *ert;
	struct resource *res;
	void *xdev_hdl;
	int err, i;

	ert = xocl_drvinst_alloc(&pdev->dev, sizeof(*ert));
	if (!ert) {
		xocl_err(&pdev->dev, "out of memory");
		return -ENOMEM;
	}

	ert->pdev = pdev;
	platform_set_drvdata(pdev, ert);

	mutex_init(&ert->ert_lock);

	res = xocl_get_iores_byname(pdev, RESNAME_ERT_FW_MEM);
	if (!res) {
		xocl_err(&pdev->dev, "Did not find %s", RESNAME_ERT_FW_MEM);
		err = -EINVAL;
		goto failed;
	}
	ert->fw_addr = ioremap_nocache(res->start, res->end - res->start + 1);
	ert->fw_ram_len = res->end - res->start + 1;

	res = xocl_get_iores_byname(pdev, RESNAME_ERT_CQ_MGMT);
	if (res) {
		xocl_info(&pdev->dev, "Found mgmtpf CQ %s", RESNAME_ERT_CQ_MGMT);
		ert->cq_addr = ioremap_nocache(res->start, res->end - res->start + 1);
		ert->cq_len = (u32)(res->end - res->start + 1);
	}

	res = xocl_get_iores_byname(pdev, RESNAME_ERT_RESET);
	if (!res) {
		xocl_err(&pdev->dev, "Did not find %s", RESNAME_ERT_RESET);
		err = -EINVAL;
		goto failed;
	}
	ert->reset_addr = ioremap_nocache(res->start, res->end - res->start + 1);

	xdev_hdl = xocl_get_xdev(pdev);
	if (!xocl_mb_sched_on(xdev_hdl)) {
		xocl_info(&pdev->dev, "Microblaze is not supported.");
		return 0;
	}

	/* GPIO is set to 0 by default. needs to
	 * 1) replace ERT image with a self jump instruction
	 * 2) cleanup command queue
	 * 3) start MB. otherwise any touching of ERT subsystem trips firewall
	 */

	if (READ_GPIO(ert, 0) == GPIO_RESET) {
		XOCL_WRITE_REG32(SELF_JUMP_INS, ert->fw_addr);
		WRITE_GPIO(ert, GPIO_ENABLED, 0);
		for (i = 0; i < ert->cq_len; i += 4)
			XOCL_WRITE_REG32(0, ert->cq_addr + i);
	}

	err = ert_sysfs_create(pdev);
	if (err) {
		xocl_err(&pdev->dev, "Create sysfs failed, err %d", err);
		goto failed;
	}

	ert->sysfs_created = true;
	ert->state = MB_INITIALIZED;

	return 0;

failed:
	ert_remove(pdev);
	return err;
}

struct xocl_drv_private	ert_priv = {
	.ops = &ert_ops,
	.dev = -1,
};

struct platform_device_id ert_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ERT), (kernel_ulong_t)&ert_priv },
	{ },
};

static struct platform_driver	ert_driver = {
	.probe		= ert_probe,
	.remove		= ert_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ERT),
	},
	.id_table = ert_id_table,
};

int __init xocl_init_ert(void)
{
	int err;

	err = platform_driver_register(&ert_driver);
	if (err) {
		return err;
	}

	return 0;
}

void xocl_fini_ert(void)
{
	platform_driver_unregister(&ert_driver);
}
