/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *
 *  Utility Functions for AXI firewall IP.
 *  Author: Lizhi.Hou@Xilinx.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/hwmon-sysfs.h>
#include <linux/rtc.h>
#include "../xocl_drv.h"

/* Firewall registers */
#define	FAULT_STATUS				0x0
#define	SOFT_CTRL				0x4
#define	UNBLOCK_CTRL				0x8
// Firewall error bits
#define READ_RESPONSE_BUSY                        BIT(0)
#define RECS_ARREADY_MAX_WAIT                     BIT(1)
#define RECS_CONTINUOUS_RTRANSFERS_MAX_WAIT       BIT(2)
#define ERRS_RDATA_NUM                            BIT(3)
#define ERRS_RID                                  BIT(4)
#define WRITE_RESPONSE_BUSY                       BIT(16)
#define RECS_AWREADY_MAX_WAIT                     BIT(17)
#define RECS_WREADY_MAX_WAIT                      BIT(18)
#define RECS_WRITE_TO_BVALID_MAX_WAIT             BIT(19)
#define ERRS_BRESP                                BIT(20)

// Get the timezone info from the linux kernel
extern struct timezone sys_tz;

#define	FIREWALL_STATUS_BUSY	(READ_RESPONSE_BUSY | WRITE_RESPONSE_BUSY)
#define	CLEAR_RESET_GPIO		0

#define	FW_PRIVILEGED(fw)		((fw)->max_level != -1)
#define	READ_STATUS(fw, id)			\
	XOCL_READ_REG32(fw->base_addrs[id] + FAULT_STATUS)
#define	WRITE_UNBLOCK_CTRL(fw, id, val)			\
	XOCL_WRITE_REG32(val, fw->base_addrs[id] + UNBLOCK_CTRL)

#define	IS_FIRED(fw, id) (READ_STATUS(fw, id) & ~FIREWALL_STATUS_BUSY)

#define	BUSY_RETRY_COUNT		20
#define	BUSY_RETRY_INTERVAL		100		/* ms */
#define	CLEAR_RETRY_COUNT		4
#define	CLEAR_RETRY_INTERVAL		2		/* ms */
#define	FW_DEFAULT_EXPIRE_SECS		1
#define	MAX_LEVEL			16

struct firewall {
	void __iomem		*base_addrs[MAX_LEVEL];
	u32			max_level;
	void __iomem		*gpio_addr;

	u32			curr_status;
	int			curr_level;

	u32			err_detected_status;
	u32			err_detected_level;
	u64			err_detected_time;

	bool			inject_firewall;
	u64			cache_expire_secs;
	struct xcl_firewall	cache;
	ktime_t			cache_expires;
};

static int clear_firewall(struct platform_device *pdev);
static u32 check_firewall(struct platform_device *pdev, int *level);

static void set_fw_data(struct firewall *fw, struct xcl_firewall *fw_status)
{
	memcpy(&fw->cache, fw_status, sizeof(struct xcl_firewall));
	fw->cache_expires = ktime_add(ktime_get_boottime(),
		ktime_set(fw->cache_expire_secs, 0));
}

static void fw_read_from_peer(struct platform_device *pdev)
{
	struct firewall *fw = platform_get_drvdata(pdev);
	struct mailbox_subdev_peer subdev_peer = {0};
	struct xcl_firewall fw_status = {0};
	size_t resp_len = sizeof(struct xcl_firewall);
	size_t data_len = sizeof(struct mailbox_subdev_peer);
	struct mailbox_req *mb_req = NULL;
	size_t reqlen = sizeof(struct mailbox_req) + data_len;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	xocl_info(&pdev->dev, "reading from peer");
	mb_req = vmalloc(reqlen);
	if (!mb_req)
		return;

	mb_req->req = MAILBOX_REQ_PEER_DATA;
	subdev_peer.size = resp_len;
	subdev_peer.kind = FIREWALL;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	(void) xocl_peer_request(xdev,
		mb_req, reqlen, &fw_status, &resp_len, NULL, NULL);
	set_fw_data(fw, &fw_status);

	vfree(mb_req);
}

static void get_fw_status(struct platform_device *pdev)
{
	struct firewall *fw = platform_get_drvdata(pdev);
	ktime_t now = ktime_get_boottime();

	if (ktime_compare(now, fw->cache_expires) > 0)
		fw_read_from_peer(pdev);
}

static int get_prop(struct platform_device *pdev, u32 prop, void *val)
{
	struct firewall *fw;
	int ret = 0;

	fw = platform_get_drvdata(pdev);
	BUG_ON(!fw);

	if (FW_PRIVILEGED(fw)) {

		check_firewall(pdev, NULL);

		switch (prop) {
		case XOCL_AF_PROP_TOTAL_LEVEL:
			*(u64 *)val = fw->max_level;
			break;
		case XOCL_AF_PROP_STATUS:
			*(u64 *)val = fw->curr_status;
			break;
		case XOCL_AF_PROP_LEVEL:
			*(int64_t *)val = fw->curr_level;
			break;
		case XOCL_AF_PROP_DETECTED_STATUS:
			*(u64 *)val = fw->err_detected_status;
			break;
		case XOCL_AF_PROP_DETECTED_LEVEL:
			*(u64 *)val = fw->err_detected_level;
			break;
		case XOCL_AF_PROP_DETECTED_TIME:
			*(u64 *)val = fw->err_detected_time;
			break;
		default:
			xocl_err(&pdev->dev, "Invalid prop %d", prop);
			ret = -EINVAL;
		}
	} else {

		get_fw_status(pdev);

		switch (prop) {
		case XOCL_AF_PROP_TOTAL_LEVEL:
			*(u64 *)val = fw->cache.max_level;
			break;
		case XOCL_AF_PROP_STATUS:
			*(u64 *)val = fw->cache.curr_status;
			break;
		case XOCL_AF_PROP_LEVEL:
			*(int *)val = fw->cache.curr_level;
			break;
		case XOCL_AF_PROP_DETECTED_STATUS:
			*(u64 *)val = fw->cache.err_detected_status;
			break;
		case XOCL_AF_PROP_DETECTED_LEVEL:
			*(u64 *)val = fw->cache.err_detected_level;
			break;
		case XOCL_AF_PROP_DETECTED_TIME:
			*(u64 *)val = fw->cache.err_detected_time;
			break;
		default:
			xocl_err(&pdev->dev, "Invalid prop %d", prop);
			ret = -EINVAL;
		}
	}
	return ret;
}

/* sysfs support */
static ssize_t show_firewall(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct platform_device *pdev = to_platform_device(dev);
	struct firewall *fw;
	u64 t;
	int ret;

	fw = platform_get_drvdata(pdev);
	BUG_ON(!fw);

	ret = get_prop(pdev, attr->index, &t);
	if (ret)
		return 0;

	return sprintf(buf, "%llu\n", t);
}

static SENSOR_DEVICE_ATTR(status, 0444, show_firewall, NULL,
	XOCL_AF_PROP_STATUS);
static SENSOR_DEVICE_ATTR(level, 0444, show_firewall, NULL,
	XOCL_AF_PROP_LEVEL);
static SENSOR_DEVICE_ATTR(detected_status, 0444, show_firewall, NULL,
	XOCL_AF_PROP_DETECTED_STATUS);
static SENSOR_DEVICE_ATTR(detected_level, 0444, show_firewall, NULL,
	XOCL_AF_PROP_DETECTED_LEVEL);
static SENSOR_DEVICE_ATTR(detected_time, 0444, show_firewall, NULL,
	XOCL_AF_PROP_DETECTED_TIME);

static ssize_t clear_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	u32 val = 0;
	struct firewall *fw = platform_get_drvdata(to_platform_device(dev));

	if (!FW_PRIVILEGED(fw))
		return 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL || val != 1)
		return -EINVAL;

	clear_firewall(pdev);

	return count;
}
static DEVICE_ATTR_WO(clear);

static ssize_t inject_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct firewall *fw = platform_get_drvdata(to_platform_device(dev));

	if (!FW_PRIVILEGED(fw))
		return 0;

	fw->inject_firewall = true;
	return count;
}
static DEVICE_ATTR_WO(inject);

static struct attribute *firewall_attributes[] = {
	&sensor_dev_attr_status.dev_attr.attr,
	&sensor_dev_attr_level.dev_attr.attr,
	&sensor_dev_attr_detected_status.dev_attr.attr,
	&sensor_dev_attr_detected_level.dev_attr.attr,
	&sensor_dev_attr_detected_time.dev_attr.attr,
	&dev_attr_clear.attr,
	&dev_attr_inject.attr,
	NULL
};

static const struct attribute_group firewall_attrgroup = {
	.attrs = firewall_attributes,
};

static u32 check_firewall(struct platform_device *pdev, int *level)
{
	struct firewall	*fw;
	struct timeval	time;
	int	i;
	u32	val = 0;

	fw = platform_get_drvdata(pdev);
	BUG_ON(!fw);

	if (!FW_PRIVILEGED(fw))
		return 0;

	for (i = 0; i < fw->max_level; i++) {
		val = IS_FIRED(fw, i);
		if (val) {
			xocl_info(&pdev->dev, "AXI Firewall %d tripped, status: 0x%x", i, val);
			if (!fw->curr_status) {
				fw->err_detected_status = val;
				fw->err_detected_level = i;
				do_gettimeofday(&time);
				fw->err_detected_time = (u64)(time.tv_sec -
					(sys_tz.tz_minuteswest * 60));
			}
			fw->curr_level = i;

			if (level)
				*level = i;
			break;
		}
	}

	fw->curr_status = val;
	fw->curr_level = i >= fw->max_level ? -1 : i;

	/* Inject firewall for testing. */
	if (fw->curr_level == -1 && fw->inject_firewall) {
		fw->inject_firewall = false;
		fw->curr_level = 0;
		fw->curr_status = 0x1;
	}

	return fw->curr_status;
}

static int clear_firewall(struct platform_device *pdev)
{
	struct firewall	*fw = platform_get_drvdata(pdev);
	int	i, retry = 0, clear_retry = 0;
	u32	val;
	int	ret = 0;

	BUG_ON(!fw);

	if (!check_firewall(pdev, NULL)) {
		/* firewall is not tripped */
		return 0;
	}

retry_level1:
	for (i = 0; i < fw->max_level; i++) {
		for (val = READ_STATUS(fw, i);
			(val & FIREWALL_STATUS_BUSY) &&
			retry++ < BUSY_RETRY_COUNT;
			val = READ_STATUS(fw, i)) {
			msleep(BUSY_RETRY_INTERVAL);
		}
		if (val & FIREWALL_STATUS_BUSY) {
			xocl_err(&pdev->dev, "firewall %d busy", i);
			ret = -EBUSY;
			goto failed;
		}
		WRITE_UNBLOCK_CTRL(fw, i, 1);
	}

	if (check_firewall(pdev, NULL) && clear_retry++ < CLEAR_RETRY_COUNT) {
		msleep(CLEAR_RETRY_INTERVAL);
		goto retry_level1;
	}

	if (!check_firewall(pdev, NULL)) {
		xocl_info(&pdev->dev, "firewall cleared level 1");
		return 0;
	}

	clear_retry = 0;

retry_level2:
	XOCL_WRITE_REG32(CLEAR_RESET_GPIO, fw->gpio_addr);

	if (check_firewall(pdev, NULL) && clear_retry++ < CLEAR_RETRY_COUNT) {
		msleep(CLEAR_RETRY_INTERVAL);
		goto retry_level2;
	}

	if (!check_firewall(pdev, NULL)) {
		xocl_info(&pdev->dev, "firewall cleared level 2");
		return 0;
	}

	xocl_info(&pdev->dev, "failed clear firewall, level %d, status 0x%x",
		fw->curr_level, fw->curr_status);

	ret = -EIO;

failed:
	return ret;
}

static void af_get_data(struct platform_device *pdev, void *buf)
{
	struct firewall	*fw = platform_get_drvdata(pdev);
	struct xcl_firewall *af_status = (struct xcl_firewall *)buf;

	if (FW_PRIVILEGED(fw)) {
		get_prop(pdev, XOCL_AF_PROP_TOTAL_LEVEL, &af_status->max_level);
		get_prop(pdev, XOCL_AF_PROP_STATUS, &af_status->curr_status);
		get_prop(pdev, XOCL_AF_PROP_LEVEL, &af_status->curr_level);
		get_prop(pdev, XOCL_AF_PROP_DETECTED_STATUS, &af_status->err_detected_status);
		get_prop(pdev, XOCL_AF_PROP_DETECTED_LEVEL, &af_status->err_detected_level);
		get_prop(pdev, XOCL_AF_PROP_DETECTED_TIME, &af_status->err_detected_time);
	}
}

static struct xocl_firewall_funcs fw_ops = {
	.clear_firewall	= clear_firewall,
	.check_firewall = check_firewall,
	.get_prop = get_prop,
	.get_data = af_get_data,
};

static int firewall_remove(struct platform_device *pdev)
{
	struct firewall *fw;
	int     i;

	fw = platform_get_drvdata(pdev);
	if (!fw) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &firewall_attrgroup);

	for (i = 0; i < MAX_LEVEL; i++) {
		if (fw->base_addrs[i])
			iounmap(fw->base_addrs[i]);
	}
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, fw);
	return 0;
}

static int firewall_probe(struct platform_device *pdev)
{
	struct firewall	*fw;
	struct resource	*res;
	int	i, ret = 0;

	xocl_info(&pdev->dev, "probe");
	fw = devm_kzalloc(&pdev->dev, sizeof(*fw), GFP_KERNEL);
	if (!fw)
		return -ENOMEM;
	platform_set_drvdata(pdev, fw);


	fw->curr_level = -1;
	for (i = 0; i < MAX_LEVEL; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			fw->max_level = i - 1;
			fw->gpio_addr = fw->base_addrs[i - 1];
			break;
		}
		fw->base_addrs[i] =
			ioremap_nocache(res->start, res->end - res->start + 1);
		if (!fw->base_addrs[i]) {
			ret = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto failed;
		}
	}
	ret = sysfs_create_group(&pdev->dev.kobj, &firewall_attrgroup);
	if (ret) {
		xocl_err(&pdev->dev, "create attr group failed: %d", ret);
		goto failed;
	}

	xocl_subdev_register(pdev, XOCL_SUBDEV_AF, &fw_ops);
	fw->cache_expire_secs = FW_DEFAULT_EXPIRE_SECS;

	return 0;

failed:
	firewall_remove(pdev);
	return ret;
}

struct platform_device_id firewall_id_table[] = {
	{ XOCL_FIREWALL, 0 },
	{ },
};

static struct platform_driver	firewall_driver = {
	.probe		= firewall_probe,
	.remove		= firewall_remove,
	.driver		= {
		.name = XOCL_FIREWALL,
	},
	.id_table = firewall_id_table,
};

int __init xocl_init_firewall(void)
{
	return platform_driver_register(&firewall_driver);
}

void xocl_fini_firewall(void)
{
	return platform_driver_unregister(&firewall_driver);
}
