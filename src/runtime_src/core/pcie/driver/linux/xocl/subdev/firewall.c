/**
 *  Copyright (C) 2017-2021 Xilinx, Inc. All rights reserved.
 *
 *  Utility Functions for AXI firewall IP.
 *  Author: Lizhi.Hou@Xilinx.com
 *          Jan Stephan <j.stephan@hzdr.de>
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
#define IP_VERSION				0x10
#define MAX_CONTINUOUS_RTRANSFERS_WAITS	0x30
#define MAX_WRITE_TO_BVALID_WAITS		0x34
#define MAX_ARREADY_WAITS			0x38
#define MAX_AWREADY_WAITS			0x3C
#define MAX_WREADY_WAITS			0x40

/* version 1.1 only registers */
#define SI_FAULT_STATUS				0x100
#define SI_SOFT_CTRL				0x104
#define SI_UNBLOCK_CTRL				0x108
#define MAX_CONTINUOUS_WTRANSFERS_WAITS		0x130
#define MAX_WVALID_TO_AWVALID_WAITS		0x134
#define MAX_RREADY_WAITS			0x138
#define MAX_BREADY_WAITS			0x13c
#define GLOBAL_INTR_ENABLE			0x200
#define MI_INTR_ENABLE				0x204
#define SI_INTR_ENABLE				0x208
#define ARADDR_LO				0x210
#define ARADDR_HI				0x214
#define AWADDR_LO				0x218
#define AWADDR_HI				0x21c
#define ARUSER					0x220
#define AWUSER					0x224
#define TIMEOUT_PRESCALER			0x230
#define TIMEOUT_INITIAL_DELAY			0x234

// Firewall error bits
#define READ_RESPONSE_BUSY                        BIT(0)
#define WRITE_RESPONSE_BUSY                       BIT(16)

static char *af_mi_status[32] = {
	"READ_RESPONSE_BUSY",
	"RECS_ARREADY_MAX_WAIT",
	"RECS_CONTINUOUS_RTRANSFERS_MAX_WAIT",
	"ERRS_RDATA_NUM",
	"ERRS_RID",
	"ERR_RVALID_STABLE",
	"XILINX_RD_SLVERR",
	"XILINX_RD_DECERR",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"WRITE_RESPONSE_BUSY",
	"RECS_AWREADY_MAX_WAIT",
	"RECS_WREADY_MAX_WAIT",
	"RECS_WRITE_TO_BVALID_MAX_WAIT",
	"ERRS_BRESP",
	"ERRS_BVALID_STABLE",
	"XILINX_WR_SLVERR",
	"XILINX_WR_DECERR",
};

static char *af_si_status[32] = {
	"READ_RESPONSE_BUSY",
	"RECM_RREADY_MAX_WAIT",
	"ERRM_ARSIZE",
	"ERRM_ARADDR_BOUNDARY",
	"ERRM_ARVALID_STABLE",
	"XILINX_RD_SLVERR",
	"XILINX_RD_DECERR",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"WRITE_RESPONSE_BUSY",
	"RECM_BREADY_MAX_WAIT",
	"RECM_CONTINUOUS_WTRANSFERS_MAX_WAIT",
	"RECM_WVALID_TO_AWVALID_MAX_WAIT",
	"ERRM_AWSIZE",
	"ERRM_WDATA_NUM",
	"ERRM_AWADDR_BOUNDARY",
	"ERRM_AWVALID_STABLE",
	"ERRM_WVALID_STABLE",
	"XILINX_WR_SLVERR",
	"XILINX_WR_DECERR",
};

#define	FIREWALL_STATUS_BUSY	(READ_RESPONSE_BUSY | WRITE_RESPONSE_BUSY)
#define	CLEAR_RESET_GPIO		0

#define	FW_PRIVILEGED(fw)		((fw)->af[0].base_addr != NULL)
#define	READ_STATUS(fw, id)							\
	((fw->af[id].mode == SI_MODE) ?						\
	XOCL_READ_REG32(fw->af[id].base_addr + SI_FAULT_STATUS) :		\
	XOCL_READ_REG32(fw->af[id].base_addr + FAULT_STATUS))
#define	WRITE_UNBLOCK_CTRL(fw, id, val)						\
	((fw->af[id].mode == SI_MODE) ?						\
	XOCL_WRITE_REG32(val, fw->af[id].base_addr + SI_UNBLOCK_CTRL) :		\
	XOCL_WRITE_REG32(val, fw->af[id].base_addr + UNBLOCK_CTRL))

#define	IS_FIRED(fw, id) (READ_STATUS(fw, id) & ~FIREWALL_STATUS_BUSY)

#define	BUSY_RETRY_COUNT		20
#define	BUSY_RETRY_INTERVAL		100		/* ms */
#define	CLEAR_RETRY_COUNT		4
#define	CLEAR_RETRY_INTERVAL		2		/* ms */
#define	MAX_LEVEL			16

#define	FW_MAX_WAIT_DEFAULT 		0xffff
#define FW_MAX_WAIT_FIC			0x2000

#define MI_MODE				0
#define SI_MODE				1

#define IP_VER_10			0
#define IP_VER_11			1

#define AF_READ32(fw, id, reg)					\
	XOCL_READ_REG32(fw->af[id].base_addr + reg)
#define AF_WRITE32(fw, id, reg, val)				\
	XOCL_WRITE_REG32(val, fw->af[id].base_addr + reg)

#define READ_ARADDR(fw, id) (((ulong)(AF_READ32(fw, id, ARADDR_HI))) << 32 |	\
		(AF_READ32(fw, id, ARADDR_LO)))
#define READ_AWADDR(fw, id) (((ulong)(AF_READ32(fw, id, AWADDR_HI))) << 32 |	\
		(AF_READ32(fw, id, AWADDR_LO)))
#define READ_ARUSER(fw, id) (AF_READ32(fw, id, ARUSER))
#define READ_AWUSER(fw, id) (AF_READ32(fw, id, AWUSER))

struct firewall_ip {
	void __iomem		*base_addr;
	u32			base_max_wait;
	u32			mode;
	u32			version;
};

struct firewall {
	struct firewall_ip	af[MAX_LEVEL];
	struct xcl_firewall	status;
	char			level_name[MAX_LEVEL][50];

	bool			inject_firewall;
	u64			err_detected_araddr;
	u64			err_detected_awaddr;
	u32			err_detected_aruser;
	u32			err_detected_awuser;
};

static int clear_firewall(struct platform_device *pdev);
static u32 check_firewall(struct platform_device *pdev, int *fw_status);

/*
 * Request the firewall status from the mgmt driver via mailbox.
 * Populates the device firewall status struct with the response
 * from the mgmt driver
 */
static void request_firewall_status(struct platform_device *pdev)
{
	struct firewall *fw = platform_get_drvdata(pdev);
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	size_t resp_len = sizeof(struct xcl_firewall);
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t reqlen = sizeof(struct xcl_mailbox_req) + data_len;
	XOCL_TIMESPEC time;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	xocl_info(&pdev->dev, "reading from peer");
	mb_req = vmalloc(reqlen);
	if (!mb_req)
		return;

	mb_req->req = XCL_MAILBOX_REQ_PEER_DATA;
	subdev_peer.size = resp_len;
	subdev_peer.kind = XCL_FIREWALL;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	/* 
	 * Request the firewall status information from the mgmt driver
	 * Place the response into the firewall status struct
	 */
	(void) xocl_peer_request(xdev,
		mb_req, reqlen, &fw->status, &resp_len, NULL, NULL, 0, 0);
	/* Overwrite mgmt timestamp. Some firmware does not provide a valid time */
	XOCL_GETTIME(&time);
	fw->status.err_detected_time = (u64)time.tv_sec;

	vfree(mb_req);
}

static int get_prop(struct platform_device *pdev, u32 prop, void *val)
{
	struct firewall *fw;
	int ret = 0;

	fw = platform_get_drvdata(pdev);
	BUG_ON(!fw);
	/* Get the requested property */
	switch (prop) {
		case XOCL_AF_PROP_TOTAL_LEVEL:
			*(u64 *)val = fw->status.max_level;
			break;
		case XOCL_AF_PROP_STATUS:
			*(u64 *)val = fw->status.curr_status;
			break;
		case XOCL_AF_PROP_LEVEL:
			*(int64_t *)val = fw->status.curr_level;
			break;
		case XOCL_AF_PROP_DETECTED_STATUS:
			*(u64 *)val = fw->status.err_detected_status;
			break;
		case XOCL_AF_PROP_DETECTED_LEVEL:
			*(u64 *)val = fw->status.err_detected_level;
			break;
		case XOCL_AF_PROP_DETECTED_TIME:
			*(u64 *)val = fw->status.err_detected_time;
			break;
		case XOCL_AF_PROP_DETECTED_LEVEL_NAME:
			strcpy((char *)val, fw->status.err_detected_level_name);
			break;
		default:
			xocl_err(&pdev->dev, "Invalid prop %d", prop);
			ret = -EINVAL;
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
	char name[50];
	int ret;

	fw = platform_get_drvdata(pdev);
	BUG_ON(!fw);

	if (attr->index == XOCL_AF_PROP_DETECTED_LEVEL_NAME) {
		ret = get_prop(pdev, attr->index, &name);
		if (ret)
			return 0;

		return sprintf(buf, "%s\n", name);
	}

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
static SENSOR_DEVICE_ATTR(detected_level_name, 0444, show_firewall, NULL,
	XOCL_AF_PROP_DETECTED_LEVEL_NAME);

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

static ssize_t detected_trip_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct firewall *fw = platform_get_drvdata(to_platform_device(dev));
	int i;
	char **status;
	ssize_t count = 0;

	status = (fw->af[fw->status.err_detected_level].mode == SI_MODE) ?
		af_si_status: af_mi_status;
	for (i = 0; i < 32; i++) {
		if (fw->status.err_detected_status & BIT(i)) {
			count += sprintf(buf + count, "status_bit%d:%s\n",
				i, status[i]);
		}
	}

	count += sprintf(buf + count, "level_name:%s\n", fw->status.err_detected_level_name);
	count += sprintf(buf + count, "araddr:0x%llx\n", fw->err_detected_araddr);
	count += sprintf(buf + count, "awaddr:0x%llx\n", fw->err_detected_awaddr);
	count += sprintf(buf + count, "aruser:0x%x\n", fw->err_detected_aruser);
	count += sprintf(buf + count, "awuser:0x%x\n", fw->err_detected_awuser);

	return count;
}
static DEVICE_ATTR_RO(detected_trip);

static struct attribute *firewall_attributes[] = {
	&sensor_dev_attr_status.dev_attr.attr,
	&sensor_dev_attr_level.dev_attr.attr,
	&sensor_dev_attr_detected_status.dev_attr.attr,
	&sensor_dev_attr_detected_level.dev_attr.attr,
	&sensor_dev_attr_detected_time.dev_attr.attr,
	&sensor_dev_attr_detected_level_name.dev_attr.attr,
	&dev_attr_clear.attr,
	&dev_attr_inject.attr,
	&dev_attr_detected_trip.attr,
	NULL
};

static const struct attribute_group firewall_attrgroup = {
	.attrs = firewall_attributes,
};

static u32 check_firewall(struct platform_device *pdev, int *level)
{
	struct firewall	*fw;
	XOCL_TIMESPEC time;
	int	i, bar_idx;
	u32	val = 0;
	resource_size_t bar_off = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct resource *res;

	fw = platform_get_drvdata(pdev);
	BUG_ON(!fw);

	/* Force xocl driver to request data from xclmgmt */
	if (!FW_PRIVILEGED(fw)) {
		request_firewall_status(pdev);
		return 0;
	}

	/* Check for any tripped firewall events */
	for (i = 0; i < fw->status.max_level; i++) {
		val = IS_FIRED(fw, i);
		if (val) {
			res = platform_get_resource(pdev, IORESOURCE_MEM, i);
			if (res) {
				(void) xocl_ioaddr_to_baroff(xdev, res->start,
					&bar_idx, &bar_off);
			}
			XOCL_GETTIME(&time);
			xocl_info(&pdev->dev,
				"AXI Firewall %d tripped, status: 0x%x, bar offset 0x%llx, resource %s",
				i, val, bar_off, (res && res->name) ? res->name : "N/A");
			if (fw->af[i].version >= IP_VER_11) {
				xocl_info(&pdev->dev, "ARADDR 0x%lx, AWADDR 0x%lx, ARUSER 0x%x, AWUSER 0x%x",
					READ_ARADDR(fw, i), READ_AWADDR(fw, i),
					READ_ARUSER(fw, i), READ_AWUSER(fw, i));
			}

			/* 
			 * Only update the firewall status if a there is a firewall event
			 * Otherwise latch the previous firewall event
			 */
			if (!fw->status.curr_status) {
				fw->status.err_detected_status = val;
				fw->status.err_detected_level = i;
				strcpy(fw->status.err_detected_level_name, fw->level_name[i]);
				fw->status.err_detected_time = (u64)time.tv_sec;
				fw->err_detected_araddr = READ_ARADDR(fw, i);
				fw->err_detected_awaddr = READ_AWADDR(fw, i);
				fw->err_detected_aruser = READ_ARUSER(fw, i);
				fw->err_detected_awuser = READ_AWUSER(fw, i);
			}
			fw->status.curr_level = i;

			if (level)
				*level = i;
			break;
		}
	}

	fw->status.curr_status = val;
	fw->status.curr_level = i >= fw->status.max_level ? -1 : i;

	/* Print out all firewall status information if the firewall is tripped */
	if (val) {
		for (i = 0; i < fw->status.max_level; i++) {
			res = platform_get_resource(pdev, IORESOURCE_MEM, i);
			if (res) {
				(void) xocl_ioaddr_to_baroff(xdev, res->start,
					&bar_idx, &bar_off);
			}
			xocl_info(&pdev->dev,
				"Firewall %d, ep %s, status: 0x%x, bar offset 0x%llx",
				i, (res && res->name) ? res->name : "N/A",
				READ_STATUS(fw, i), bar_off);
			if (fw->af[i].version >= IP_VER_11) {
				xocl_info(&pdev->dev, "ARADDR 0x%lx, AWADDR 0x%lx, ARUSER 0x%x, AWUSER 0x%x",
				    READ_ARADDR(fw, i), READ_AWADDR(fw, i),
				    READ_ARUSER(fw, i), READ_AWUSER(fw, i));
			}
		}
	}

	/* Inject firewall for testing. */
	if (fw->status.curr_level == -1 && fw->inject_firewall) {
		fw->inject_firewall = false;
		fw->status.curr_level = 0;
		fw->status.curr_status = 0x1;
	}

	return fw->status.curr_status;
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
	for (i = 0; i < fw->status.max_level; i++) {
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

	if (!check_firewall(pdev, NULL)) {
		xocl_info(&pdev->dev, "firewall cleared level 2");
		return 0;
	}

	xocl_info(&pdev->dev, "failed clear firewall, level %llu, status 0x%llx",
		fw->status.curr_level, fw->status.curr_status);

	ret = -EIO;

failed:
	return ret;
}

static void af_get_data(struct platform_device *pdev, void *buf)
{
	struct xcl_firewall *af_status = (struct xcl_firewall *)buf;
	get_prop(pdev, XOCL_AF_PROP_TOTAL_LEVEL, &af_status->max_level);
	get_prop(pdev, XOCL_AF_PROP_STATUS, &af_status->curr_status);
	get_prop(pdev, XOCL_AF_PROP_LEVEL, &af_status->curr_level);
	get_prop(pdev, XOCL_AF_PROP_DETECTED_STATUS, &af_status->err_detected_status);
	get_prop(pdev, XOCL_AF_PROP_DETECTED_LEVEL, &af_status->err_detected_level);
	get_prop(pdev, XOCL_AF_PROP_DETECTED_TIME, &af_status->err_detected_time);
	get_prop(pdev, XOCL_AF_PROP_DETECTED_LEVEL_NAME, &af_status->err_detected_level_name);
}

static void inline reset_max_wait(struct firewall *fw, int idx)
{
	u32 value = fw->af[idx].base_max_wait;
	void __iomem *addr = fw->af[idx].base_addr;

	if (value == 0 || addr == NULL)
		return;

	XOCL_WRITE_REG32(value, addr + MAX_CONTINUOUS_RTRANSFERS_WAITS);
	XOCL_WRITE_REG32(value, addr + MAX_WRITE_TO_BVALID_WAITS);
	XOCL_WRITE_REG32(value, addr + MAX_ARREADY_WAITS);
	XOCL_WRITE_REG32(value, addr + MAX_AWREADY_WAITS);
	XOCL_WRITE_REG32(value, addr + MAX_WREADY_WAITS);
}

static void inline update_max_wait(struct firewall *fw, int idx, u32 value)
{
	fw->af[idx].base_max_wait = value;
}

static void
resource_max_wait_set(struct resource *res, struct firewall *fw, int idx)
{
	const char *res_name = res->name;

	if (!res_name)
		return;

	if (!strncmp(res_name, NODE_AF_CTRL_MGMT, strlen(NODE_AF_CTRL_MGMT)) ||
	    !strncmp(res_name, NODE_AF_CTRL_USER, strlen(NODE_AF_CTRL_USER)) ||
	    !strncmp(res_name, NODE_AF_CTRL_DEBUG, strlen(NODE_AF_CTRL_DEBUG))) {
		update_max_wait(fw, idx, FW_MAX_WAIT_FIC);
		reset_max_wait(fw, idx);
	}
}

static int firewall_offline(struct platform_device *pdev)
{
	/* so far nothing to do */
	return 0;
}

static int firewall_online(struct platform_device *pdev)
{
	struct firewall *fw;
	int i;

	fw = platform_get_drvdata(pdev);
	if (!fw) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	/* reset max_wait settings */
	for (i = 0; i < MAX_LEVEL; i++)
		reset_max_wait(fw, i);

	return 0;
}

static struct xocl_firewall_funcs fw_ops = {
	.offline_cb	= firewall_offline,
	.online_cb	= firewall_online,
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

	for (i = 0; i <= fw->status.max_level; i++) {
		if (fw->af[i].base_addr)
			iounmap(fw->af[i].base_addr);
	}
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, fw);
	return 0;
}

static void get_fw_ep_name(const char *res_name, char *result)
{
	if (!strncmp(res_name, NODE_AF_CTRL_MGMT, strlen(NODE_AF_CTRL_MGMT)))
		strcpy(result, "CTRL_MGMT");
	else if (!strncmp(res_name, NODE_AF_CTRL_USER, strlen(NODE_AF_CTRL_USER)))
		strcpy(result, "CTRL_USER");
	else if (!strncmp(res_name, NODE_AF_CTRL_DEBUG, strlen(NODE_AF_CTRL_DEBUG)))
		strcpy(result, "CTRL_DEBUG");
	else if (!strncmp(res_name, NODE_AF_BLP_CTRL_MGMT, strlen(NODE_AF_BLP_CTRL_MGMT)))
		strcpy(result, "BLP_CTRL_MGMT");
	else if (!strncmp(res_name, NODE_AF_BLP_CTRL_USER, strlen(NODE_AF_BLP_CTRL_USER)))
		strcpy(result, "BLP_CTRL_USER");
	else if (!strncmp(res_name, NODE_AF_DATA_H2C, strlen(NODE_AF_DATA_H2C)))
		strcpy(result, "DATA_H2C");
	else if (!strncmp(res_name, NODE_AF_DATA_C2H, strlen(NODE_AF_DATA_C2H)))
		strcpy(result, "DATA_C2H");
	else if (!strncmp(res_name, NODE_AF_DATA_P2P, strlen(NODE_AF_DATA_P2P)))
		strcpy(result, "DATA_P2P");
	else if (!strncmp(res_name, NODE_AF_DATA_M2M, strlen(NODE_AF_DATA_M2M)))
		strcpy(result, "DATA_M2M");
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


	fw->status.curr_level = -1;
	for (i = 0; i < MAX_LEVEL; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			fw->status.max_level = i;
			break;
		}
		get_fw_ep_name(res->name, fw->level_name[i]);
		fw->af[i].base_addr =
			ioremap_nocache(res->start, res->end - res->start + 1);
		if (!fw->af[i].base_addr) {
			ret = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto failed;
		}

		fw->af[i].version = AF_READ32(fw, i, IP_VERSION);
		if (fw->af[i].version >= IP_VER_11 &&
		    AF_READ32(fw, i, MAX_CONTINUOUS_WTRANSFERS_WAITS) != 0)
			fw->af[i].mode = SI_MODE;

		/* additional check after res mapped */
		resource_max_wait_set(res, fw, i);
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &firewall_attrgroup);
	if (ret) {
		xocl_err(&pdev->dev, "create attr group failed: %d", ret);
		goto failed;
	}

	return 0;

failed:
	firewall_remove(pdev);
	return ret;
}

struct xocl_drv_private firewall_priv = {
	.ops = &fw_ops,
};

struct platform_device_id firewall_id_table[] = {
	{ XOCL_DEVNAME(XOCL_FIREWALL), (kernel_ulong_t)&firewall_priv },
	{ },
};

static struct platform_driver	firewall_driver = {
	.probe		= firewall_probe,
	.remove		= firewall_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_FIREWALL),
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
