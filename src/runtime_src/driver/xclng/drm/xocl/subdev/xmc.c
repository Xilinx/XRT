/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: chienwei@xilinx.com
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
#include <linux/string.h>
#include <ert.h>
#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

#define MAX_XMC_RETRY       150	//Retry is set to 15s for XMC
#define MAX_ERT_RETRY       10	//Retry is set to 1s for ERT
#define RETRY_INTERVAL  100       //100ms

#define	MAX_IMAGE_LEN	0x20000

#define XMC_MAGIC_REG               0x0
#define XMC_VERSION_REG             0x4
#define XMC_STATUS_REG              0x8
#define XMC_ERROR_REG               0xC
#define XMC_FEATURE_REG             0x10
#define XMC_SENSOR_REG              0x14
#define XMC_CONTROL_REG             0x18
#define XMC_STOP_CONFIRM_REG        0x1C
#define XMC_12V_PEX_REG             0x20
#define XMC_3V3_PEX_REG             0x2C
#define XMC_3V3_AUX_REG             0x38
#define XMC_12V_AUX_REG             0x44
#define XMC_DDR4_VPP_BTM_REG        0x50
#define XMC_SYS_5V5_REG             0x5C
#define XMC_VCC1V2_TOP_REG          0x68
#define XMC_VCC1V8_REG              0x74
#define XMC_VCC0V85_REG             0x80
#define XMC_DDR4_VPP_TOP_REG        0x8C
#define XMC_MGT0V9AVCC_REG          0x98
#define XMC_12V_SW_REG              0xA4
#define XMC_MGTAVTT_REG             0xB0
#define XMC_VCC1V2_BTM_REG          0xBC
#define XMC_12V_PEX_I_IN_REG        0xC8
#define XMC_12V_AUX_I_IN_REG        0xD4
#define XMC_VCCINT_V_REG            0xE0
#define XMC_VCCINT_I_REG            0xEC
#define XMC_FPGA_TEMP               0xF8
#define XMC_FAN_TEMP_REG            0x104
#define XMC_DIMM_TEMP0_REG          0x110
#define XMC_DIMM_TEMP1_REG          0x11C
#define XMC_DIMM_TEMP2_REG          0x128
#define XMC_DIMM_TEMP3_REG          0x134
#define XMC_FAN_SPEED_REG           0x164
#define XMC_SE98_TEMP0_REG          0x140
#define XMC_SE98_TEMP1_REG          0x14C
#define XMC_SE98_TEMP2_REG          0x158
#define XMC_SNSR_CHKSUM_REG         0x1A4
#define XMC_SNSR_FLAGS_REG          0x1A8
#define XMC_HOST_MSG_OFFSET_REG     0x300
#define XMC_HOST_MSG_ERROR_REG      0x304
#define XMC_HOST_MSG_HEADER_REG     0x308


#define	VALID_ID		0x74736574

#define	GPIO_RESET		0x0
#define	GPIO_ENABLED		0x1

#define	SELF_JUMP(ins)		(((ins) & 0xfc00ffff) == 0xb8000000)

enum ctl_mask {
	CTL_MASK_CLEAR_POW	= 0x1,
	CTL_MASK_CLEAR_ERR	= 0x2,
	CTL_MASK_PAUSE		= 0x4,
	CTL_MASK_STOP		= 0x8,
};

enum status_mask {
	STATUS_MASK_INIT_DONE		= 0x1,
	STATUS_MASK_STOPPED		= 0x2,
	STATUS_MASK_PAUSE		= 0x4,
};

enum cap_mask {
	CAP_MASK_PM			= 0x1,
};

enum {
	XMC_STATE_UNKNOWN,
	XMC_STATE_ENABLED,
	XMC_STATE_RESET,
	XMC_STATE_STOPPED,
	XMC_STATE_ERROR
};

enum {
	IO_REG,
	IO_GPIO,
	IO_IMAGE_MGMT,
	IO_IMAGE_SCHED,
	IO_CQ,
	NUM_IOADDR
};

enum {
	VOLTAGE_MAX,
	VOLTAGE_AVG,
	VOLTAGE_INS,
};

#define	READ_REG32(xmc, off)		\
	XOCL_READ_REG32(xmc->base_addrs[IO_REG] + off)
#define	WRITE_REG32(xmc, val, off)	\
	XOCL_WRITE_REG32(val, xmc->base_addrs[IO_REG] + off)

#define	READ_GPIO(xmc, off)		\
	XOCL_READ_REG32(xmc->base_addrs[IO_GPIO] + off)
#define	WRITE_GPIO(xmc, val, off)	\
	XOCL_WRITE_REG32(val, xmc->base_addrs[IO_GPIO] + off)

#define	READ_IMAGE_MGMT(xmc, off)		\
	XOCL_READ_REG32(xmc->base_addrs[IO_IMAGE_MGMT] + off)

#define	READ_IMAGE_SCHED(xmc, off)		\
	XOCL_READ_REG32(xmc->base_addrs[IO_IMAGE_SCHED] + off)

#define	COPY_MGMT(xmc, buf, len)		\
	XOCL_COPY2IO(xmc->base_addrs[IO_IMAGE_MGMT], buf, len)
#define	COPY_SCHE(xmc, buf, len)		\
	XOCL_COPY2IO(xmc->base_addrs[IO_IMAGE_SCHED], buf, len)

struct xocl_xmc {
	struct platform_device	*pdev;
	void __iomem		*base_addrs[NUM_IOADDR];

	struct device		*hwmon_dev;
	bool			enabled;
	u32			state;
	u32			cap;
	struct mutex		xmc_lock;

	char			*sche_binary;
	u32			sche_binary_length;
	char			*mgmt_binary;
	u32			mgmt_binary_length;
};


static int load_xmc(struct xocl_xmc *xmc);
static int stop_xmc(struct platform_device *pdev);

/* sysfs support */
static void safe_read32(struct xocl_xmc *xmc, u32 reg, u32 *val)
{
	mutex_lock(&xmc->xmc_lock);
	if (xmc->enabled && xmc->state == XMC_STATE_ENABLED) {
		*val = READ_REG32(xmc, reg);
	} else {
		*val = 0;
	}
	mutex_unlock(&xmc->xmc_lock);
}

static void safe_write32(struct xocl_xmc *xmc, u32 reg, u32 val)
{
	mutex_lock(&xmc->xmc_lock);
	if (xmc->enabled && xmc->state == XMC_STATE_ENABLED) {
		WRITE_REG32(xmc, val, reg);
	}
	mutex_unlock(&xmc->xmc_lock);
}

static ssize_t xmc_12v_pex_vol_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 pes_val;

	safe_read32(xmc, XMC_12V_PEX_REG+sizeof(u32)*VOLTAGE_INS, &pes_val);

	return sprintf(buf, "%d\n", pes_val);
}
static DEVICE_ATTR_RO(xmc_12v_pex_vol);

static ssize_t xmc_12v_aux_vol_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_12V_AUX_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_12v_aux_vol);

static ssize_t xmc_12v_pex_curr_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 pes_val;

	safe_read32(xmc, XMC_12V_PEX_I_IN_REG+sizeof(u32)*VOLTAGE_INS, &pes_val);

	return sprintf(buf, "%d\n", pes_val);
}
static DEVICE_ATTR_RO(xmc_12v_pex_curr);

static ssize_t xmc_12v_aux_curr_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_12V_AUX_I_IN_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_12v_aux_curr);

static ssize_t xmc_3v3_pex_vol_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_3V3_PEX_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_3v3_pex_vol);

static ssize_t xmc_3v3_aux_vol_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_3V3_AUX_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_3v3_aux_vol);

static ssize_t xmc_ddr_vpp_btm_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_DDR4_VPP_BTM_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_ddr_vpp_btm);

static ssize_t xmc_sys_5v5_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_SYS_5V5_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_sys_5v5);

static ssize_t xmc_1v2_top_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_VCC1V2_TOP_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_1v2_top);

static ssize_t xmc_1v8_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_VCC1V8_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_1v8);

static ssize_t xmc_0v85_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_VCC0V85_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_0v85);

static ssize_t xmc_ddr_vpp_top_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_DDR4_VPP_TOP_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_ddr_vpp_top);


static ssize_t xmc_mgt0v9avcc_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_MGT0V9AVCC_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_mgt0v9avcc);

static ssize_t xmc_12v_sw_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_12V_SW_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_12v_sw);


static ssize_t xmc_mgtavtt_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_MGTAVTT_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_mgtavtt);

static ssize_t xmc_vcc1v2_btm_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_VCC1V2_BTM_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_vcc1v2_btm);




static ssize_t xmc_vccint_vol_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_VCCINT_V_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_vccint_vol);

static ssize_t xmc_vccint_curr_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_VCCINT_I_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_vccint_curr);

static ssize_t xmc_se98_temp0_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_SE98_TEMP0_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_se98_temp0);

static ssize_t xmc_se98_temp1_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_SE98_TEMP1_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_se98_temp1);

static ssize_t xmc_se98_temp2_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_SE98_TEMP2_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_se98_temp2);

static ssize_t xmc_fpga_temp_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_FPGA_TEMP, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_fpga_temp);

static ssize_t xmc_fan_temp_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_FAN_TEMP_REG, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_fan_temp);

static ssize_t xmc_fan_rpm_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_FAN_SPEED_REG, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_fan_rpm);


static ssize_t xmc_dimm_temp0_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_DIMM_TEMP0_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_dimm_temp0);

static ssize_t xmc_dimm_temp1_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_DIMM_TEMP1_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_dimm_temp1);

static ssize_t xmc_dimm_temp2_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_DIMM_TEMP2_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_dimm_temp2);

static ssize_t xmc_dimm_temp3_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_DIMM_TEMP3_REG+sizeof(u32)*VOLTAGE_INS, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(xmc_dimm_temp3);

static ssize_t version_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(xmc, XMC_VERSION_REG, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(version);

static ssize_t sensor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(xmc, XMC_SENSOR_REG, &val);

	return sprintf(buf, "0x%04x\n", val);
}
static DEVICE_ATTR_RO(sensor);


static ssize_t id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(xmc, XMC_MAGIC_REG, &val);

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(id);

static ssize_t status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(xmc, XMC_STATUS_REG, &val);

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(status);

static ssize_t error_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(xmc, XMC_ERROR_REG, &val);

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(error);

static ssize_t capability_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(xmc, XMC_FEATURE_REG, &val);

	return sprintf(buf, "%x\n", val);
}
static DEVICE_ATTR_RO(capability);

static ssize_t power_checksum_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(xmc, XMC_SNSR_CHKSUM_REG, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(power_checksum);

static ssize_t pause_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	safe_read32(xmc, XMC_CONTROL_REG, &val);

	return sprintf(buf, "%d\n", !!(val & CTL_MASK_PAUSE));
}

static ssize_t pause_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 1) {
		return -EINVAL;
	}

	val = val ? CTL_MASK_PAUSE : 0;
	safe_write32(xmc, XMC_CONTROL_REG, val);

	return count;
}
static DEVICE_ATTR_RW(pause);

static ssize_t reset_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 1) {
		return -EINVAL;
	}

	if (val) {
		load_xmc(xmc);
	}


	return count;
}
static DEVICE_ATTR_WO(reset);

static ssize_t power_flag_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_SNSR_FLAGS_REG, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(power_flag);

static ssize_t host_msg_offset_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_HOST_MSG_OFFSET_REG, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(host_msg_offset);

static ssize_t host_msg_error_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_HOST_MSG_ERROR_REG, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(host_msg_error);

static ssize_t host_msg_header_show(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_HOST_MSG_HEADER_REG, &val);

	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(host_msg_header);



static int get_temp_by_m_tag(struct xocl_xmc *xmc, char *m_tag)
{

	/**
	 *   m_tag get from xclbin must follow this format
	 *   DDR[0] or bank1
	 *   we check the index in m_tag to decide which temperature
	 *   to get from XMC IP base address
	 */
	char *start = NULL, *left_parentness = NULL, *right_parentness = NULL;
	long idx;
	int ret = 0, digit_len = 0;
	char temp[4];

	if(!xmc)
		return -ENODEV;


	if(!strncmp(m_tag, "bank", 4)) {
		start = m_tag;
		// bank0, no left parentness
		left_parentness = m_tag+3;
		right_parentness = m_tag+strlen(m_tag)+1;
		digit_len = right_parentness-(2+left_parentness);
	} else if (!strncmp(m_tag, "DDR", 3)) {

		start = m_tag;
		left_parentness = strstr(m_tag, "[");
		right_parentness = strstr(m_tag, "]");
		digit_len = right_parentness-(1+left_parentness);
	}

	if(!left_parentness || !right_parentness)
		return ret;

	if(!strncmp(m_tag, "DDR", left_parentness-start) || !strncmp(m_tag, "bank", left_parentness-start)){

		strncpy(temp, left_parentness+1, digit_len);
		//assumption, temperature won't higher than 3 digits, or the temp[digit_len] should be a null character
		temp[digit_len] = '\0';
		//convert to signed long, decimal base 
		if(kstrtol(temp, 10, &idx) == 0 && idx < 4 && idx >=0)
			safe_read32(xmc, XMC_DIMM_TEMP0_REG+ (3*sizeof(int32_t)) * idx +sizeof(u32)*VOLTAGE_INS, &ret);
		else{
			ret = 0;
		}
	}

	return ret;

}

static struct attribute *xmc_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_id.attr,
	&dev_attr_status.attr,
	&dev_attr_sensor.attr,
	&dev_attr_error.attr,
	&dev_attr_capability.attr,
	&dev_attr_power_checksum.attr,
	&dev_attr_xmc_12v_pex_vol.attr,
	&dev_attr_xmc_12v_aux_vol.attr,
	&dev_attr_xmc_12v_pex_curr.attr,
	&dev_attr_xmc_12v_aux_curr.attr,
	&dev_attr_xmc_3v3_pex_vol.attr,
	&dev_attr_xmc_3v3_aux_vol.attr,
	&dev_attr_xmc_ddr_vpp_btm.attr,
	&dev_attr_xmc_sys_5v5.attr,
	&dev_attr_xmc_1v2_top.attr,
	&dev_attr_xmc_1v8.attr,
	&dev_attr_xmc_0v85.attr,
	&dev_attr_xmc_ddr_vpp_top.attr,
	&dev_attr_xmc_mgt0v9avcc.attr,
	&dev_attr_xmc_12v_sw.attr,
	&dev_attr_xmc_mgtavtt.attr,
	&dev_attr_xmc_vcc1v2_btm.attr,
	&dev_attr_xmc_fpga_temp.attr,
	&dev_attr_xmc_fan_temp.attr,
	&dev_attr_xmc_fan_rpm.attr,
	&dev_attr_xmc_dimm_temp0.attr,
	&dev_attr_xmc_dimm_temp1.attr,
	&dev_attr_xmc_dimm_temp2.attr,
	&dev_attr_xmc_dimm_temp3.attr,
	&dev_attr_xmc_vccint_vol.attr,
	&dev_attr_xmc_vccint_curr.attr,
	&dev_attr_xmc_se98_temp0.attr,
	&dev_attr_xmc_se98_temp1.attr,
	&dev_attr_xmc_se98_temp2.attr,
	&dev_attr_pause.attr,
	&dev_attr_reset.attr,
	&dev_attr_power_flag.attr,
	&dev_attr_host_msg_offset.attr,
	&dev_attr_host_msg_error.attr,
	&dev_attr_host_msg_header.attr,
	NULL,
};


static ssize_t read_temp_by_mem_topology(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	u32 nread = 0;
	size_t size = 0;
	u32 i;
	struct mem_topology* memtopo = NULL;
	struct xocl_xmc *xmc;
	uint32_t temp[MAX_M_COUNT] = {0};
	struct xclmgmt_dev *lro;

	lro = (struct xclmgmt_dev *)dev_get_drvdata(container_of(kobj, struct device, kobj)->parent);
	xmc = (struct xocl_xmc *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	memtopo = (struct mem_topology*)xocl_icap_get_axlf_section_data(lro, MEM_TOPOLOGY);

	if(!memtopo)
		return 0;

	size = sizeof(u32)*(memtopo->m_count);

	if (offset >= size)
		return 0;

	for(i=0;i<memtopo->m_count;++i){
		*(temp+i) = get_temp_by_m_tag(xmc, memtopo->m_mem_data[i].m_tag);
	}

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, temp, nread);
	return nread;
}

static struct bin_attribute bin_dimm_temp_by_mem_topology_attr = {
	.attr = {
		.name = "temp_by_mem_topology",
		.mode = 0444
	},
	.read = read_temp_by_mem_topology,
	.write = NULL,
	.size = 0
};

static struct bin_attribute *xmc_bin_attrs[] = {
	&bin_dimm_temp_by_mem_topology_attr,
	NULL,
};

static struct attribute_group xmc_attr_group = {
	.attrs = xmc_attrs,
	.bin_attrs = xmc_bin_attrs,
};
static ssize_t show_mb_pw(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	safe_read32(xmc, XMC_12V_PEX_REG + attr->index * sizeof(u32), &val);

	return sprintf(buf, "%d\n", val);
}

static SENSOR_DEVICE_ATTR(curr1_highest, 0444, show_mb_pw, NULL, 0);
static SENSOR_DEVICE_ATTR(curr1_average, 0444, show_mb_pw, NULL, 1);
static SENSOR_DEVICE_ATTR(curr1_input, 0444, show_mb_pw, NULL, 2);
static SENSOR_DEVICE_ATTR(curr2_highest, 0444, show_mb_pw, NULL, 3);
static SENSOR_DEVICE_ATTR(curr2_average, 0444, show_mb_pw, NULL, 4);
static SENSOR_DEVICE_ATTR(curr2_input, 0444, show_mb_pw, NULL, 5);
static SENSOR_DEVICE_ATTR(curr3_highest, 0444, show_mb_pw, NULL, 6);
static SENSOR_DEVICE_ATTR(curr3_average, 0444, show_mb_pw, NULL, 7);
static SENSOR_DEVICE_ATTR(curr3_input, 0444, show_mb_pw, NULL, 8);
static SENSOR_DEVICE_ATTR(curr4_highest, 0444, show_mb_pw, NULL, 9);
static SENSOR_DEVICE_ATTR(curr4_average, 0444, show_mb_pw, NULL, 10);
static SENSOR_DEVICE_ATTR(curr4_input, 0444, show_mb_pw, NULL, 11);
static SENSOR_DEVICE_ATTR(curr5_highest, 0444, show_mb_pw, NULL, 12);
static SENSOR_DEVICE_ATTR(curr5_average, 0444, show_mb_pw, NULL, 13);
static SENSOR_DEVICE_ATTR(curr5_input, 0444, show_mb_pw, NULL, 14);
static SENSOR_DEVICE_ATTR(curr6_highest, 0444, show_mb_pw, NULL, 15);
static SENSOR_DEVICE_ATTR(curr6_average, 0444, show_mb_pw, NULL, 16);
static SENSOR_DEVICE_ATTR(curr6_input, 0444, show_mb_pw, NULL, 17);

static struct attribute *hwmon_xmc_attributes[] = {
	&sensor_dev_attr_curr1_highest.dev_attr.attr,
	&sensor_dev_attr_curr1_average.dev_attr.attr,
	&sensor_dev_attr_curr1_input.dev_attr.attr,
	&sensor_dev_attr_curr2_highest.dev_attr.attr,
	&sensor_dev_attr_curr2_average.dev_attr.attr,
	&sensor_dev_attr_curr2_input.dev_attr.attr,
	&sensor_dev_attr_curr3_highest.dev_attr.attr,
	&sensor_dev_attr_curr3_average.dev_attr.attr,
	&sensor_dev_attr_curr3_input.dev_attr.attr,
	&sensor_dev_attr_curr4_highest.dev_attr.attr,
	&sensor_dev_attr_curr4_average.dev_attr.attr,
	&sensor_dev_attr_curr4_input.dev_attr.attr,
	&sensor_dev_attr_curr5_highest.dev_attr.attr,
	&sensor_dev_attr_curr5_average.dev_attr.attr,
	&sensor_dev_attr_curr5_input.dev_attr.attr,
	&sensor_dev_attr_curr6_highest.dev_attr.attr,
	&sensor_dev_attr_curr6_average.dev_attr.attr,
	&sensor_dev_attr_curr6_input.dev_attr.attr,
	NULL
};

static const struct attribute_group hwmon_xmc_attrgroup = {
	.attrs = hwmon_xmc_attributes,
};

static ssize_t show_name(struct device *dev, struct device_attribute *da,
        char *buf)
{
        return sprintf(buf, "%s\n", XCLMGMT_MB_HWMON_NAME);
}

static struct sensor_device_attribute name_attr =
        SENSOR_ATTR(name, 0444, show_name, NULL, 0);

static void mgmt_sysfs_destroy_xmc(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;

	xmc = platform_get_drvdata(pdev);

	if (!xmc->enabled) {
		return;
	}

	if (xmc->hwmon_dev) {
		device_remove_file(xmc->hwmon_dev, &name_attr.dev_attr);
		sysfs_remove_group(&xmc->hwmon_dev->kobj,
			&hwmon_xmc_attrgroup);
		hwmon_device_unregister(xmc->hwmon_dev);
		xmc->hwmon_dev = NULL;
	}

  sysfs_remove_group(&pdev->dev.kobj, &xmc_attr_group);
}

static int mgmt_sysfs_create_xmc(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	struct xocl_dev_core *core;
	int err;
	xmc = platform_get_drvdata(pdev);
	core = XDEV(xocl_get_xdev(pdev));

	if (!xmc->enabled) {
		return 0;
	}

	err = sysfs_create_group(&pdev->dev.kobj, &xmc_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create xmc attrs failed: 0x%x", err);
		goto create_attr_failed;
	}
	xmc->hwmon_dev = hwmon_device_register(&core->pdev->dev);
	if (IS_ERR(xmc->hwmon_dev)) {
		err = PTR_ERR(xmc->hwmon_dev);
		xocl_err(&pdev->dev, "register xmc hwmon failed: 0x%x", err);
		goto hwmon_reg_failed;
	}

	dev_set_drvdata(xmc->hwmon_dev, xmc);

	err = device_create_file(xmc->hwmon_dev, &name_attr.dev_attr);
	if (err) {
		xocl_err(&pdev->dev, "create attr name failed: 0x%x", err);
		goto create_name_failed;
	}

	err = sysfs_create_group(&xmc->hwmon_dev->kobj,
		&hwmon_xmc_attrgroup);
	if (err) {
		xocl_err(&pdev->dev, "create pw group failed: 0x%x", err);
		goto create_pw_failed;
	}

	return 0;

create_pw_failed:
	device_remove_file(xmc->hwmon_dev, &name_attr.dev_attr);
create_name_failed:
	hwmon_device_unregister(xmc->hwmon_dev);
	xmc ->hwmon_dev = NULL;
hwmon_reg_failed:
	sysfs_remove_group(&pdev->dev.kobj, &xmc_attr_group);
create_attr_failed:
	return err;
}

static int stop_xmc_nolock(struct platform_device *pdev) {
	struct xocl_xmc *xmc;
	int retry = 0;
	u32 reg_val = 0;
	void *xdev_hdl;

	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return -ENODEV;
	else if (!xmc->enabled)
		return -ENODEV;

	xdev_hdl = xocl_get_xdev(xmc->pdev);

	reg_val = READ_GPIO(xmc, 0);
	xocl_info(&xmc->pdev->dev, "MB Reset GPIO 0x%x", reg_val);

	//Stop XMC and ERT if its currently running
	if (reg_val == GPIO_ENABLED) {
		xocl_info(&xmc->pdev->dev,
			"XMC info, version 0x%x, status 0x%x, id 0x%x",
			READ_REG32(xmc, XMC_VERSION_REG),
			READ_REG32(xmc, XMC_STATUS_REG),
			READ_REG32(xmc, XMC_MAGIC_REG));

		reg_val = READ_REG32(xmc, XMC_STATUS_REG);
		if(!(reg_val & STATUS_MASK_STOPPED)) {
			xocl_info(&xmc->pdev->dev, "Stopping XMC...");
			WRITE_REG32(xmc, CTL_MASK_STOP, XMC_CONTROL_REG);
			WRITE_REG32(xmc, 1, XMC_STOP_CONFIRM_REG);
		}
		//Need to check if ERT is loaded before we attempt to stop it
		if (!SELF_JUMP(READ_IMAGE_SCHED(xmc, 0))) {
			reg_val = XOCL_READ_REG32(xmc->base_addrs[IO_CQ]);
			if(!(reg_val & ERT_STOP_ACK)) {
				xocl_info(&xmc->pdev->dev, "Stopping scheduler...");
				XOCL_WRITE_REG32(ERT_STOP_CMD, xmc->base_addrs[IO_CQ]);
			}
		}

		retry=0;
		while (retry++ < MAX_XMC_RETRY &&
			!(READ_REG32(xmc, XMC_STATUS_REG) & STATUS_MASK_STOPPED))
			msleep(RETRY_INTERVAL);

		//Wait for XMC to stop and then check that ERT has also finished
		if (retry >= MAX_XMC_RETRY) {
			xocl_err(&xmc->pdev->dev,
				"Failed to stop XMC");
			xocl_err(&xmc->pdev->dev,
				"XMC Error Reg 0x%x",
				READ_REG32(xmc, XMC_ERROR_REG));
			xmc->state = XMC_STATE_ERROR;
			return -ETIMEDOUT;
		} else if (!SELF_JUMP(READ_IMAGE_SCHED(xmc, 0)) &&
			 !(XOCL_READ_REG32(xmc->base_addrs[IO_CQ]) & ERT_STOP_ACK)) {
			while (retry++ < MAX_ERT_RETRY &&
				!(XOCL_READ_REG32(xmc->base_addrs[IO_CQ]) & ERT_STOP_ACK))
				msleep(RETRY_INTERVAL);
			if (retry >= MAX_ERT_RETRY) {
				xocl_err(&xmc->pdev->dev,
					"Failed to stop sched");
				xocl_err(&xmc->pdev->dev,
					"Scheduler CQ status 0x%x",
					XOCL_READ_REG32(xmc->base_addrs[IO_CQ]));
				//We don't exit if ERT doesn't stop since it can hang due to bad kernel
				//xmc->state = XMC_STATE_ERROR;
				//return -ETIMEDOUT;
			}
		}

		xocl_info(&xmc->pdev->dev, "XMC/sched Stopped, retry %d",
			retry);
	}

	// Hold XMC in reset now that its safely stopped
	xocl_info(&xmc->pdev->dev,
		"XMC info, version 0x%x, status 0x%x, id 0x%x",
		READ_REG32(xmc, XMC_VERSION_REG),
		READ_REG32(xmc, XMC_STATUS_REG),
		READ_REG32(xmc, XMC_MAGIC_REG));
	WRITE_GPIO(xmc, GPIO_RESET, 0);
	xmc->state = XMC_STATE_RESET;
	reg_val = READ_GPIO(xmc, 0);
	xocl_info(&xmc->pdev->dev, "MB Reset GPIO 0x%x", reg_val);
	if (reg_val != GPIO_RESET) {//Shouldnt make it here but if we do then exit
		xmc->state = XMC_STATE_ERROR;
        	return -EIO;
	}

	return 0;
}
static int stop_xmc(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	int ret = 0;
	void *xdev_hdl;

	xocl_info(&pdev->dev, "Stop Microblaze...");
	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return -ENODEV;
	else if (!xmc->enabled)
		return -ENODEV;

	xdev_hdl = xocl_get_xdev(xmc->pdev);

	mutex_lock(&xmc->xmc_lock);
	ret = stop_xmc_nolock(pdev);
	mutex_unlock(&xmc->xmc_lock);

	return ret;
}

static int load_xmc(struct xocl_xmc *xmc)
{
	int retry = 0;
	u32 reg_val = 0;
	int ret = 0;
	void *xdev_hdl;

	if (!xmc->enabled) {
		return -ENODEV;
	}


	mutex_lock(&xmc->xmc_lock);

    	/* Stop XMC first */
	ret = stop_xmc_nolock(xmc->pdev);
    	if(ret != 0)
		goto out;

	xdev_hdl = xocl_get_xdev(xmc->pdev);

	/* Load XMC and ERT Image */
	if (xocl_mb_mgmt_on(xdev_hdl)) {
		xocl_info(&xmc->pdev->dev, "Copying XMC image len %d",
			xmc->mgmt_binary_length);
		COPY_MGMT(xmc, xmc->mgmt_binary, xmc->mgmt_binary_length);
	}

	if (xocl_mb_sched_on(xdev_hdl)) {
		xocl_info(&xmc->pdev->dev, "Copying scheduler image len %d",
			xmc->sche_binary_length);
		COPY_SCHE(xmc, xmc->sche_binary, xmc->sche_binary_length);
	}

	/* Take XMC and ERT out of reset */
	WRITE_GPIO(xmc, GPIO_ENABLED, 0);
	reg_val = READ_GPIO(xmc, 0);
	xocl_info(&xmc->pdev->dev, "MB Reset GPIO 0x%x", reg_val);
	if (reg_val != GPIO_ENABLED) {//Shouldnt make it here but if we do then exit
		xmc->state = XMC_STATE_ERROR;
		goto out;
	}

	/* Wait for XMC to start
	 * Note that ERT will start long before XMC so we don't check anything */
	reg_val = READ_REG32(xmc, XMC_STATUS_REG);
	if(!(reg_val & STATUS_MASK_INIT_DONE)) {
		xocl_info(&xmc->pdev->dev, "Waiting for XMC to finish init...");
		retry=0;
		while (retry++ < MAX_XMC_RETRY &&
			!(READ_REG32(xmc, XMC_STATUS_REG) & STATUS_MASK_INIT_DONE))
			msleep(RETRY_INTERVAL);
		if (retry >= MAX_XMC_RETRY) {
			xocl_err(&xmc->pdev->dev,
				"XMC did not finish init sequence!");
			xocl_err(&xmc->pdev->dev,
				"Error Reg 0x%x",
				READ_REG32(xmc, XMC_ERROR_REG));
			xocl_err(&xmc->pdev->dev,
				"Status Reg 0x%x",
				READ_REG32(xmc, XMC_STATUS_REG));
			ret = -ETIMEDOUT;
			xmc->state = XMC_STATE_ERROR;
			goto out;
		}
	}
	xocl_info(&xmc->pdev->dev, "XMC and scheduler Enabled, retry %d",
			retry);
	xocl_info(&xmc->pdev->dev,
		"XMC info, version 0x%x, status 0x%x, id 0x%x",
		READ_REG32(xmc, XMC_VERSION_REG),
		READ_REG32(xmc, XMC_STATUS_REG),
		READ_REG32(xmc, XMC_MAGIC_REG));
	xmc->state = XMC_STATE_ENABLED;

	xmc->cap = READ_REG32(xmc, XMC_FEATURE_REG);
out:
	mutex_unlock(&xmc->xmc_lock);

	return ret;
}

static void xmc_reset(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;

	xocl_info(&pdev->dev, "Reset Microblaze...");
	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return;

	load_xmc(xmc);
}

static int load_mgmt_image(struct platform_device *pdev, const char *image,
	u32 len)
{
	struct xocl_xmc *xmc;
	char *binary;

	if (len > MAX_IMAGE_LEN)
		return -EINVAL;

	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return -EINVAL;

	binary = xmc->mgmt_binary;
	xmc->mgmt_binary = devm_kzalloc(&pdev->dev, len, GFP_KERNEL);
	if (!xmc->mgmt_binary) {
		return -ENOMEM;
	}

	if (binary)
		devm_kfree(&pdev->dev, binary);
	memcpy(xmc->mgmt_binary, image, len);
	xmc->mgmt_binary_length = len;

	return 0;
}

static int load_sche_image(struct platform_device *pdev, const char *image,
	u32 len)
{
	struct xocl_xmc *xmc;
	char *binary = NULL;

	if (len > MAX_IMAGE_LEN)
		return -EINVAL;

	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return -EINVAL;

	binary = xmc->sche_binary;
	xmc->sche_binary = devm_kzalloc(&pdev->dev, len, GFP_KERNEL);
	if (!xmc->sche_binary) {
		return -ENOMEM;
	}

	if (binary)
		devm_kfree(&pdev->dev, binary);
	memcpy(xmc->sche_binary, image, len);
	xmc->sche_binary_length = len;

	return 0;
}

static struct xocl_mb_funcs xmc_ops = {
	.load_mgmt_image	= load_mgmt_image,
	.load_sche_image	= load_sche_image,
	.reset			= xmc_reset,
	.stop			= stop_xmc,
};

static int xmc_remove(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	int	i;

	xmc = platform_get_drvdata(pdev);
	if (!xmc) {
		return 0;
	}

	if (xmc->mgmt_binary)
		devm_kfree(&pdev->dev, xmc->mgmt_binary);
	if (xmc->sche_binary)
		devm_kfree(&pdev->dev, xmc->sche_binary);

	mgmt_sysfs_destroy_xmc(pdev);

	for (i = 0; i < NUM_IOADDR; i++) {
		if (xmc->base_addrs[i])
			iounmap(xmc->base_addrs[i]);
	}

	mutex_destroy(&xmc->xmc_lock);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, xmc);

	return 0;
}

static int xmc_probe(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	struct resource *res;
	void	*xdev_hdl;
	int i, err;

	xmc = devm_kzalloc(&pdev->dev, sizeof(*xmc), GFP_KERNEL);
	if (!xmc) {
		xocl_err(&pdev->dev, "out of memory");
		return -ENOMEM;
	}

	xmc->pdev = pdev;
	platform_set_drvdata(pdev, xmc);

	xdev_hdl = xocl_get_xdev(pdev);
	if (xocl_mb_mgmt_on(xdev_hdl) || xocl_mb_sched_on(xdev_hdl)) {
		xocl_info(&pdev->dev, "Microblaze is supported.");
		xmc->enabled = true;
	} else {
		xocl_info(&pdev->dev, "Microblaze is not supported.");
		devm_kfree(&pdev->dev, xmc);
		platform_set_drvdata(pdev, NULL);
		return 0;
	}

	for (i = 0; i < NUM_IOADDR; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
			res->start, res->end);
		xmc->base_addrs[i] =
			ioremap_nocache(res->start, res->end - res->start + 1);
		if (!xmc->base_addrs[i]) {
			err = -EIO;
			xocl_err(&pdev->dev, "Map iomem failed");
			goto failed;
		}
	}

	err = mgmt_sysfs_create_xmc(pdev);
	if (err) {
		xocl_err(&pdev->dev, "Create sysfs failed, err %d", err);
		goto failed;
	}

	xocl_subdev_register(pdev, XOCL_SUBDEV_XMC, &xmc_ops);

	mutex_init(&xmc->xmc_lock);

	return 0;

failed:
	xmc_remove(pdev);
	return err;
}

struct platform_device_id xmc_id_table[] = {
	{ XOCL_XMC, 0 },
	{ },
};

static struct platform_driver	xmc_driver = {
	.probe		= xmc_probe,
	.remove		= xmc_remove,
	.driver		= {
		.name = "xocl_xmc",
	},
	.id_table = xmc_id_table,
};

int __init xocl_init_xmc(void)
{
	return platform_driver_register(&xmc_driver);
}

void xocl_fini_xmc(void)
{
	platform_driver_unregister(&xmc_driver);
}
