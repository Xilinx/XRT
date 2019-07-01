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
#include "mailbox_proto.h"
#include "xclfeatures.h"

/* Retry is set to 15s for XMC */
#define	MAX_XMC_RETRY			150
/* Retry is set to 1s for ERT */
#define	MAX_ERT_RETRY			10
/* Retry is set to 5s for XMC mailbox */
#define	MAX_XMC_MBX_RETRY		50
/* 100ms */
#define	RETRY_INTERVAL			100

#define	MAX_IMAGE_LEN			0x20000

#define	XMC_MAGIC_REG			0x0
#define	XMC_VERSION_REG			0x4
#define	XMC_STATUS_REG			0x8
#define	XMC_ERROR_REG			0xC
#define	XMC_FEATURE_REG			0x10
#define	XMC_SENSOR_REG			0x14
#define	XMC_CONTROL_REG			0x18
#define	XMC_STOP_CONFIRM_REG		0x1C
#define	XMC_12V_PEX_REG			0x20
#define	XMC_3V3_PEX_REG			0x2C
#define	XMC_3V3_AUX_REG			0x38
#define	XMC_12V_AUX_REG			0x44
#define	XMC_DDR4_VPP_BTM_REG		0x50
#define	XMC_SYS_5V5_REG			0x5C
#define	XMC_VCC1V2_TOP_REG		0x68
#define	XMC_VCC1V8_REG			0x74
#define	XMC_VCC0V85_REG			0x80
#define	XMC_DDR4_VPP_TOP_REG		0x8C
#define	XMC_MGT0V9AVCC_REG		0x98
#define	XMC_12V_SW_REG			0xA4
#define	XMC_MGTAVTT_REG			0xB0
#define	XMC_VCC1V2_BTM_REG		0xBC
#define	XMC_12V_PEX_I_IN_REG		0xC8
#define	XMC_12V_AUX_I_IN_REG		0xD4
#define	XMC_VCCINT_V_REG		0xE0
#define	XMC_VCCINT_I_REG		0xEC
#define	XMC_FPGA_TEMP			0xF8
#define	XMC_FAN_TEMP_REG		0x104
#define	XMC_DIMM_TEMP0_REG		0x110
#define	XMC_DIMM_TEMP1_REG		0x11C
#define	XMC_DIMM_TEMP2_REG		0x128
#define	XMC_DIMM_TEMP3_REG		0x134
#define	XMC_FAN_SPEED_REG		0x164
#define	XMC_SE98_TEMP0_REG		0x140
#define	XMC_SE98_TEMP1_REG		0x14C
#define	XMC_SE98_TEMP2_REG		0x158
#define	XMC_CAGE_TEMP0_REG		0x170
#define	XMC_CAGE_TEMP1_REG		0x17C
#define	XMC_CAGE_TEMP2_REG		0x188
#define	XMC_CAGE_TEMP3_REG		0x194
#define	XMC_SNSR_CHKSUM_REG		0x1A4
#define	XMC_SNSR_FLAGS_REG		0x1A8
#define	XMC_HBM_TEMP_REG		0x260
#define	XMC_HOST_MSG_OFFSET_REG		0x300
#define	XMC_HOST_MSG_ERROR_REG		0x304
#define	XMC_HOST_MSG_HEADER_REG		0x308


#define	VALID_ID			0x74736574

#define	GPIO_RESET			0x0
#define	GPIO_ENABLED			0x1

#define	SELF_JUMP(ins)			(((ins) & 0xfc00ffff) == 0xb8000000)
#define	XMC_PRIVILEGED(xmc)		((xmc)->base_addrs[0] != NULL)

#define	XMC_DEFAULT_EXPIRE_SECS	1

//Clock scaling registers
#define	XMC_CLOCK_CONTROL_REG		0x24
#define	XMC_CLOCK_SCALING_EN		0x1

#define	XMC_CLOCK_SCALING_MODE_REG	0x10
#define	XMC_CLOCK_SCALING_MODE_POWER	0x0
#define	XMC_CLOCK_SCALING_MODE_TEMP	0x1

#define	XMC_CLOCK_SCALING_POWER_REG	0x18
#define	XMC_CLOCK_SCALING_POWER_REG_MASK 0xFFFF
#define	XMC_CLOCK_SCALING_TEMP_REG	0x14
#define	XMC_CLOCK_SCALING_TEMP_REG_MASK	0xFFFF

enum ctl_mask {
	CTL_MASK_CLEAR_POW		= 0x1,
	CTL_MASK_CLEAR_ERR		= 0x2,
	CTL_MASK_PAUSE			= 0x4,
	CTL_MASK_STOP			= 0x8,
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
	IO_CLK_SCALING,
	NUM_IOADDR
};

enum sensor_val_kind {
	SENSOR_MAX,
	SENSOR_AVG,
	SENSOR_INS,
};

#define	READ_REG32(xmc, off)			\
	(xmc->base_addrs[IO_REG] ?		\
	XOCL_READ_REG32(xmc->base_addrs[IO_REG] + off) : 0)
#define	WRITE_REG32(xmc, val, off)		\
	(xmc->base_addrs[IO_REG] ?		\
	XOCL_WRITE_REG32(val, xmc->base_addrs[IO_REG] + off) : ((void)0))

#define	READ_GPIO(xmc, off)			\
	(xmc->base_addrs[IO_GPIO] ?		\
	XOCL_READ_REG32(xmc->base_addrs[IO_GPIO] + off) : 0)
#define	WRITE_GPIO(xmc, val, off)		\
	(xmc->base_addrs[IO_GPIO] ?		\
	XOCL_WRITE_REG32(val, xmc->base_addrs[IO_GPIO] + off) : ((void)0))

#define	READ_IMAGE_MGMT(xmc, off)		\
	(xmc->base_addrs[IO_IMAGE_MGMT] ?	\
	XOCL_READ_REG32(xmc->base_addrs[IO_IMAGE_MGMT] + off) : 0)

#define	READ_IMAGE_SCHED(xmc, off)		\
	(xmc->base_addrs[IO_IMAGE_SCHED] ?	\
	XOCL_READ_REG32(xmc->base_addrs[IO_IMAGE_SCHED] + off) : 0)

#define	COPY_MGMT(xmc, buf, len)		\
	(xmc->base_addrs[IO_IMAGE_MGMT] ?	\
	xocl_memcpy_toio(xmc->base_addrs[IO_IMAGE_MGMT], buf, len) : ((void)0))
#define	COPY_SCHE(xmc, buf, len)		\
	(xmc->base_addrs[IO_IMAGE_SCHED] ?	\
	xocl_memcpy_toio(xmc->base_addrs[IO_IMAGE_SCHED], buf, len) : ((void)0))

#define	READ_RUNTIME_CS(xmc, off)		\
	(xmc->base_addrs[IO_CLK_SCALING] ?	\
	XOCL_READ_REG32(xmc->base_addrs[IO_CLK_SCALING] + off) : 0)
#define	WRITE_RUNTIME_CS(xmc, val, off)		\
	(xmc->base_addrs[IO_CLK_SCALING] ?	\
	XOCL_WRITE_REG32(val, xmc->base_addrs[IO_CLK_SCALING] + off) : ((void)0))

#define	READ_SENSOR(xmc, off, valp, val_kind)	\
	safe_read32(xmc, off + sizeof(u32) * val_kind, valp);

#define	XMC_PKT_SUPPORT_MASK			(1 << 3)
#define	XMC_PKT_OWNER_MASK			(1 << 5)
#define	XMC_PKT_ERR_MASK			(1 << 26)

#define	XMC_HOST_MSG_NO_ERR			0x00
#define	XMC_HOST_MSG_BAD_OPCODE_ERR		0x01
#define	XMC_HOST_MSG_UNKNOWN_ERR		0x02
#define	XMC_HOST_MSG_MSP432_MODE_ERR		0x03
#define	XMC_HOST_MSG_MSP432_FW_LENGTH_ERR	0x04
#define	XMC_HOST_MSG_BRD_INFO_MISSING_ERR	0x05

enum xmc_packet_op {
	XPO_UNKNOWN = 0,
	XPO_MSP432_SEC_START,
	XPO_MSP432_SEC_DATA,
	XPO_MSP432_IMAGE_END,
	XPO_BOARD_INFO,
	XPO_MSP432_ERASE_FW
};

/* Make sure hdr is multiple of u32 */
struct xmc_pkt_hdr {
	u32 payload_sz	: 12;
	u32 reserved	: 12;
	u32 op		: 8;
};

/* We have a 4k buffer for xmc mailbox */
#define	XMC_PKT_MAX_SZ	1024 /* In u32 */
#define	XMC_PKT_MAX_PAYLOAD_SZ	\
	(XMC_PKT_MAX_SZ - sizeof(struct xmc_pkt_hdr) / sizeof(u32)) /* In u32 */
#define	XMC_PKT_SZ(hdr)		\
	((sizeof(struct xmc_pkt_hdr) + (hdr)->payload_sz + sizeof(u32) - 1) / \
	sizeof(u32)) /* In u32 */

struct xmc_pkt {
	struct xmc_pkt_hdr hdr;
	u32 data[XMC_PKT_MAX_PAYLOAD_SZ];
};

enum board_info_key
{
	BDINFO_SN = 0x21,
	BDINFO_MAC0,
	BDINFO_MAC1,
	BDINFO_MAC2,
	BDINFO_MAC3,
	BDINFO_REV,
	BDINFO_NAME,
	BDINFO_BMC_VER,
	BDINFO_MAX_PWR,
	BDINFO_FAN_PRESENCE,
	BDINFO_CONFIG_MODE,
	/* lower and upper limit */
	BDINFO_MIN_KEY = BDINFO_SN,
	BDINFO_MAX_KEY = BDINFO_CONFIG_MODE,
};

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

	u64			cache_expire_secs;
	struct xcl_sensor	cache;
	ktime_t			cache_expires;
	/* Runtime clock scaling enabled status */
	bool			runtime_cs_enabled;

	/* XMC mailbox support. */
	struct mutex		mbx_lock;
	bool			mbx_enabled;
	u32			mbx_offset;
	struct xmc_pkt		mbx_pkt;
	char			*bdinfo_raw;
	u32			bdinfo_raw_sz;
};


static int load_xmc(struct xocl_xmc *xmc);
static int stop_xmc(struct platform_device *pdev);
static void xmc_clk_scale_config(struct platform_device *pdev);

static void set_sensors_data(struct xocl_xmc *xmc, struct xcl_sensor *sensors)
{
	memcpy(&xmc->cache, sensors, sizeof(struct xcl_sensor));
	xmc->cache_expires = ktime_add(ktime_get_boottime(),
		ktime_set(xmc->cache_expire_secs, 0));
}

static void xmc_read_from_peer(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	struct mailbox_subdev_peer subdev_peer = {0};
	struct xcl_sensor xcl_sensor = {0};
	size_t resp_len = sizeof(struct xcl_sensor);
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
	subdev_peer.kind = SENSOR;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	(void) xocl_peer_request(xdev,
		mb_req, reqlen, &xcl_sensor, &resp_len, NULL, NULL);
	set_sensors_data(xmc, &xcl_sensor);

	vfree(mb_req);
}

static void get_sensors_data(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	ktime_t now = ktime_get_boottime();

	if (ktime_compare(now, xmc->cache_expires) > 0)
		xmc_read_from_peer(pdev);
}

/* sysfs support */
static void safe_read32(struct xocl_xmc *xmc, u32 reg, u32 *val)
{
	mutex_lock(&xmc->xmc_lock);
	if (xmc->enabled && xmc->state == XMC_STATE_ENABLED)
		*val = READ_REG32(xmc, reg);
	else
		*val = 0;
	mutex_unlock(&xmc->xmc_lock);
}

static void safe_write32(struct xocl_xmc *xmc, u32 reg, u32 val)
{
	mutex_lock(&xmc->xmc_lock);
	if (xmc->enabled && xmc->state == XMC_STATE_ENABLED)
		WRITE_REG32(xmc, val, reg);
	mutex_unlock(&xmc->xmc_lock);
}

static void safe_read_from_peer(struct xocl_xmc *xmc,
	struct platform_device *pdev)
{
	mutex_lock(&xmc->xmc_lock);
	if (xmc->enabled)
		get_sensors_data(pdev);
	mutex_unlock(&xmc->xmc_lock);
}

static void xmc_sensor(struct platform_device *pdev, enum data_kind kind,
	u32 *val, enum sensor_val_kind val_kind)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);

	if (XMC_PRIVILEGED(xmc)) {
		switch (kind) {
		case DIMM0_TEMP:
			READ_SENSOR(xmc, XMC_DIMM_TEMP0_REG, val, val_kind);
			break;
		case DIMM1_TEMP:
			READ_SENSOR(xmc, XMC_DIMM_TEMP1_REG, val, val_kind);
			break;
		case DIMM2_TEMP:
			READ_SENSOR(xmc, XMC_DIMM_TEMP2_REG, val, val_kind);
			break;
		case DIMM3_TEMP:
			READ_SENSOR(xmc, XMC_DIMM_TEMP3_REG, val, val_kind);
			break;
		case FPGA_TEMP:
			READ_SENSOR(xmc, XMC_FPGA_TEMP, val, val_kind);
			break;
		case VOL_12V_PEX:
			READ_SENSOR(xmc, XMC_12V_PEX_REG, val, val_kind);
			break;
		case VOL_12V_AUX:
			READ_SENSOR(xmc, XMC_12V_AUX_REG, val, val_kind);
			break;
		case CUR_12V_PEX:
			READ_SENSOR(xmc, XMC_12V_PEX_I_IN_REG, val, val_kind);
			break;
		case CUR_12V_AUX:
			READ_SENSOR(xmc, XMC_12V_AUX_I_IN_REG, val, val_kind);
			break;
		case SE98_TEMP0:
			READ_SENSOR(xmc, XMC_SE98_TEMP0_REG, val, val_kind);
			break;
		case SE98_TEMP1:
			READ_SENSOR(xmc, XMC_SE98_TEMP1_REG, val, val_kind);
			break;
		case SE98_TEMP2:
			READ_SENSOR(xmc, XMC_SE98_TEMP2_REG, val, val_kind);
			break;
		case FAN_TEMP:
			READ_SENSOR(xmc, XMC_FAN_TEMP_REG, val, val_kind);
			break;
		case FAN_RPM:
			READ_SENSOR(xmc, XMC_FAN_SPEED_REG, val, val_kind);
			break;
		case VOL_3V3_PEX:
			READ_SENSOR(xmc, XMC_3V3_PEX_REG, val, val_kind);
			break;
		case VOL_3V3_AUX:
			READ_SENSOR(xmc, XMC_3V3_AUX_REG, val, val_kind);
			break;
		case VPP_BTM:
			READ_SENSOR(xmc, XMC_DDR4_VPP_BTM_REG, val, val_kind);
			break;
		case VPP_TOP:
			READ_SENSOR(xmc, XMC_DDR4_VPP_TOP_REG, val, val_kind);
			break;
		case VOL_5V5_SYS:
			READ_SENSOR(xmc, XMC_SYS_5V5_REG, val, val_kind);
			break;
		case VOL_1V2_TOP:
			READ_SENSOR(xmc, XMC_VCC1V2_TOP_REG, val, val_kind);
			break;
		case VOL_1V2_BTM:
			READ_SENSOR(xmc, XMC_VCC1V2_BTM_REG, val, val_kind);
			break;
		case VOL_1V8:
			READ_SENSOR(xmc, XMC_VCC1V8_REG, val, val_kind);
			break;
		case VCC_0V9A:
			READ_SENSOR(xmc, XMC_MGT0V9AVCC_REG, val, val_kind);
			break;
		case VOL_12V_SW:
			READ_SENSOR(xmc, XMC_12V_SW_REG, val, val_kind);
			break;
		case VTT_MGTA:
			READ_SENSOR(xmc, XMC_MGTAVTT_REG, val, val_kind);
			break;
		case VOL_VCC_INT:
			READ_SENSOR(xmc, XMC_VCCINT_V_REG, val, val_kind);
			break;
		case CUR_VCC_INT:
			READ_SENSOR(xmc, XMC_VCCINT_I_REG, val, val_kind);
			break;
		case HBM_TEMP:
			READ_SENSOR(xmc, XMC_HBM_TEMP_REG, val, val_kind);
			break;
		case CAGE_TEMP0:
			READ_SENSOR(xmc, XMC_CAGE_TEMP0_REG, val, val_kind);
			break;
		case CAGE_TEMP1:
			READ_SENSOR(xmc, XMC_CAGE_TEMP1_REG, val, val_kind);
			break;
		case CAGE_TEMP2:
			READ_SENSOR(xmc, XMC_CAGE_TEMP2_REG, val, val_kind);
			break;
		case CAGE_TEMP3:
			READ_SENSOR(xmc, XMC_CAGE_TEMP3_REG, val, val_kind);
			break;
		case VCC_0V85:
			READ_SENSOR(xmc, XMC_VCC0V85_REG, val, val_kind);
			break;
		default:
			break;
		}
	} else {
		safe_read_from_peer(xmc, pdev);

		switch (kind) {
		case DIMM0_TEMP:
			*val = xmc->cache.dimm_temp0;
			break;
		case DIMM1_TEMP:
			*val = xmc->cache.dimm_temp1;
			break;
		case DIMM2_TEMP:
			*val = xmc->cache.dimm_temp2;
			break;
		case DIMM3_TEMP:
			*val = xmc->cache.dimm_temp3;
			break;
		case FPGA_TEMP:
			*val = xmc->cache.fpga_temp;
			break;
		case VOL_12V_PEX:
			*val = xmc->cache.vol_12v_pex;
			break;
		case VOL_12V_AUX:
			*val = xmc->cache.vol_12v_aux;
			break;
		case CUR_12V_PEX:
			*val = xmc->cache.cur_12v_pex;
			break;
		case CUR_12V_AUX:
			*val = xmc->cache.cur_12v_aux;
			break;
		case SE98_TEMP0:
			*val = xmc->cache.se98_temp0;
			break;
		case SE98_TEMP1:
			*val = xmc->cache.se98_temp1;
			break;
		case SE98_TEMP2:
			*val = xmc->cache.se98_temp2;
			break;
		case FAN_TEMP:
			*val = xmc->cache.fan_temp;
			break;
		case FAN_RPM:
			*val = xmc->cache.fan_rpm;
			break;
		case VOL_3V3_PEX:
			*val = xmc->cache.vol_3v3_pex;
			break;
		case VOL_3V3_AUX:
			*val = xmc->cache.vol_3v3_aux;
			break;
		case VPP_BTM:
			*val = xmc->cache.ddr_vpp_btm;
			break;
		case VPP_TOP:
			*val = xmc->cache.ddr_vpp_top;
			break;
		case VOL_5V5_SYS:
			*val = xmc->cache.sys_5v5;
			break;
		case VOL_1V2_TOP:
			*val = xmc->cache.top_1v2;
			break;
		case VOL_1V2_BTM:
			*val = xmc->cache.vcc1v2_btm;
			break;
		case VOL_1V8:
			*val = xmc->cache.vol_1v8;
			break;
		case VCC_0V9A:
			*val = xmc->cache.mgt0v9avcc;
			break;
		case VOL_12V_SW:
			*val = xmc->cache.vol_12v_sw;
			break;
		case VTT_MGTA:
			*val = xmc->cache.mgtavtt;
			break;
		case VOL_VCC_INT:
			*val = xmc->cache.vccint_vol;
			break;
		case CUR_VCC_INT:
			*val = xmc->cache.vccint_curr;
			break;
		case HBM_TEMP:
			*val = xmc->cache.hbm_temp0;
			break;
		case CAGE_TEMP0:
			*val = xmc->cache.cage_temp0;
			break;
		case CAGE_TEMP1:
			*val = xmc->cache.cage_temp1;
			break;
		case CAGE_TEMP2:
			*val = xmc->cache.cage_temp2;
			break;
		case CAGE_TEMP3:
			*val = xmc->cache.cage_temp3;
			break;
		case VCC_0V85:
			*val = xmc->cache.vol_0v85;
			break;
		default:
			break;
		}
	}
}

static int xmc_get_data(struct platform_device *pdev, void *buf)
{
	struct xcl_sensor *sensors = (struct xcl_sensor *)buf;

	xmc_sensor(pdev, VOL_12V_PEX, &sensors->vol_12v_pex, SENSOR_INS);
	xmc_sensor(pdev, VOL_12V_AUX, &sensors->vol_12v_aux, SENSOR_INS);
	xmc_sensor(pdev, CUR_12V_PEX, &sensors->cur_12v_pex, SENSOR_INS);
	xmc_sensor(pdev, CUR_12V_AUX, &sensors->cur_12v_aux, SENSOR_INS);
	xmc_sensor(pdev, VOL_3V3_PEX, &sensors->vol_3v3_pex, SENSOR_INS);
	xmc_sensor(pdev, VOL_3V3_AUX, &sensors->vol_3v3_aux, SENSOR_INS);
	xmc_sensor(pdev, VPP_BTM, &sensors->ddr_vpp_btm, SENSOR_INS);
	xmc_sensor(pdev, VOL_5V5_SYS, &sensors->sys_5v5, SENSOR_INS);
	xmc_sensor(pdev, VOL_1V2_TOP, &sensors->top_1v2, SENSOR_INS);
	xmc_sensor(pdev, VOL_1V8, &sensors->vol_1v8, SENSOR_INS);
	xmc_sensor(pdev, VCC_0V85, &sensors->vol_0v85, SENSOR_INS);
	xmc_sensor(pdev, VPP_TOP, &sensors->ddr_vpp_top, SENSOR_INS);
	xmc_sensor(pdev, VCC_0V9A, &sensors->mgt0v9avcc, SENSOR_INS);
	xmc_sensor(pdev, VOL_12V_SW, &sensors->vol_12v_sw, SENSOR_INS);
	xmc_sensor(pdev, VTT_MGTA, &sensors->mgtavtt, SENSOR_INS);
	xmc_sensor(pdev, VOL_1V2_BTM, &sensors->vcc1v2_btm, SENSOR_INS);
	xmc_sensor(pdev, FPGA_TEMP, &sensors->fpga_temp, SENSOR_INS);
	xmc_sensor(pdev, FAN_TEMP, &sensors->fan_temp, SENSOR_INS);
	xmc_sensor(pdev, FAN_RPM, &sensors->fan_rpm, SENSOR_INS);
	xmc_sensor(pdev, DIMM0_TEMP, &sensors->dimm_temp0, SENSOR_INS);
	xmc_sensor(pdev, DIMM1_TEMP, &sensors->dimm_temp1, SENSOR_INS);
	xmc_sensor(pdev, DIMM2_TEMP, &sensors->dimm_temp2, SENSOR_INS);
	xmc_sensor(pdev, DIMM3_TEMP, &sensors->dimm_temp3, SENSOR_INS);
	xmc_sensor(pdev, VOL_VCC_INT, &sensors->vccint_vol, SENSOR_INS);
	xmc_sensor(pdev, CUR_VCC_INT, &sensors->vccint_curr, SENSOR_INS);
	xmc_sensor(pdev, SE98_TEMP0, &sensors->se98_temp0, SENSOR_INS);
	xmc_sensor(pdev, SE98_TEMP1, &sensors->se98_temp1, SENSOR_INS);
	xmc_sensor(pdev, SE98_TEMP2, &sensors->se98_temp2, SENSOR_INS);
	xmc_sensor(pdev, CAGE_TEMP0, &sensors->cage_temp0, SENSOR_INS);
	xmc_sensor(pdev, CAGE_TEMP1, &sensors->cage_temp1, SENSOR_INS);
	xmc_sensor(pdev, CAGE_TEMP2, &sensors->cage_temp2, SENSOR_INS);
	xmc_sensor(pdev, CAGE_TEMP3, &sensors->cage_temp3, SENSOR_INS);
	xmc_sensor(pdev, HBM_TEMP, &sensors->hbm_temp0, SENSOR_INS);

	return 0;
}

/*
 * Defining sysfs nodes for all sensor readings.
 */
#define	SENSOR_SYSFS_NODE(node_name, type)				\
	static ssize_t node_name##_show(struct device *dev,		\
		struct device_attribute *attr, char *buf)		\
	{								\
		struct xocl_xmc *xmc = dev_get_drvdata(dev);		\
		u32 val = 0;						\
		xmc_sensor(xmc->pdev, type, &val, SENSOR_INS);		\
		return sprintf(buf, "%d\n", val);			\
	}								\
	static DEVICE_ATTR_RO(node_name)
SENSOR_SYSFS_NODE(xmc_12v_pex_vol, VOL_12V_PEX);
SENSOR_SYSFS_NODE(xmc_12v_aux_vol, VOL_12V_AUX);
SENSOR_SYSFS_NODE(xmc_12v_pex_curr, CUR_12V_PEX);
SENSOR_SYSFS_NODE(xmc_12v_aux_curr, CUR_12V_AUX);
SENSOR_SYSFS_NODE(xmc_3v3_pex_vol, VOL_3V3_PEX);
SENSOR_SYSFS_NODE(xmc_3v3_aux_vol, VOL_3V3_AUX);
SENSOR_SYSFS_NODE(xmc_ddr_vpp_btm, VPP_BTM);
SENSOR_SYSFS_NODE(xmc_sys_5v5, VOL_5V5_SYS);
SENSOR_SYSFS_NODE(xmc_1v2_top, VOL_1V2_TOP);
SENSOR_SYSFS_NODE(xmc_1v8, VOL_1V8);
SENSOR_SYSFS_NODE(xmc_0v85, VCC_0V85);
SENSOR_SYSFS_NODE(xmc_ddr_vpp_top, VPP_TOP);
SENSOR_SYSFS_NODE(xmc_mgt0v9avcc, VCC_0V9A);
SENSOR_SYSFS_NODE(xmc_12v_sw, VOL_12V_SW);
SENSOR_SYSFS_NODE(xmc_mgtavtt, VTT_MGTA);
SENSOR_SYSFS_NODE(xmc_vcc1v2_btm, VOL_1V2_BTM);
SENSOR_SYSFS_NODE(xmc_vccint_vol, VOL_VCC_INT);
SENSOR_SYSFS_NODE(xmc_vccint_curr, CUR_VCC_INT);
SENSOR_SYSFS_NODE(xmc_se98_temp0, SE98_TEMP0);
SENSOR_SYSFS_NODE(xmc_se98_temp1, SE98_TEMP1);
SENSOR_SYSFS_NODE(xmc_se98_temp2, SE98_TEMP2);
SENSOR_SYSFS_NODE(xmc_fpga_temp, FPGA_TEMP);
SENSOR_SYSFS_NODE(xmc_fan_temp, FAN_TEMP);
SENSOR_SYSFS_NODE(xmc_fan_rpm, FAN_RPM);
SENSOR_SYSFS_NODE(xmc_dimm_temp0, DIMM0_TEMP);
SENSOR_SYSFS_NODE(xmc_dimm_temp1, DIMM1_TEMP);
SENSOR_SYSFS_NODE(xmc_dimm_temp2, DIMM2_TEMP);
SENSOR_SYSFS_NODE(xmc_dimm_temp3, DIMM3_TEMP);
SENSOR_SYSFS_NODE(xmc_cage_temp0, CAGE_TEMP0);
SENSOR_SYSFS_NODE(xmc_cage_temp1, CAGE_TEMP1);
SENSOR_SYSFS_NODE(xmc_cage_temp2, CAGE_TEMP2);
SENSOR_SYSFS_NODE(xmc_cage_temp3, CAGE_TEMP3);
#define	SENSOR_SYSFS_NODE_ATTRS						\
	&dev_attr_xmc_12v_pex_vol.attr,					\
	&dev_attr_xmc_12v_aux_vol.attr,					\
	&dev_attr_xmc_12v_pex_curr.attr,				\
	&dev_attr_xmc_12v_aux_curr.attr,				\
	&dev_attr_xmc_3v3_pex_vol.attr,					\
	&dev_attr_xmc_3v3_aux_vol.attr,					\
	&dev_attr_xmc_ddr_vpp_btm.attr,					\
	&dev_attr_xmc_sys_5v5.attr,					\
	&dev_attr_xmc_1v2_top.attr,					\
	&dev_attr_xmc_1v8.attr,						\
	&dev_attr_xmc_0v85.attr,					\
	&dev_attr_xmc_ddr_vpp_top.attr,					\
	&dev_attr_xmc_mgt0v9avcc.attr,					\
	&dev_attr_xmc_12v_sw.attr,					\
	&dev_attr_xmc_mgtavtt.attr,					\
	&dev_attr_xmc_vcc1v2_btm.attr,					\
	&dev_attr_xmc_fpga_temp.attr,					\
	&dev_attr_xmc_fan_temp.attr,					\
	&dev_attr_xmc_fan_rpm.attr,					\
	&dev_attr_xmc_dimm_temp0.attr,					\
	&dev_attr_xmc_dimm_temp1.attr,					\
	&dev_attr_xmc_dimm_temp2.attr,					\
	&dev_attr_xmc_dimm_temp3.attr,					\
	&dev_attr_xmc_vccint_vol.attr,					\
	&dev_attr_xmc_vccint_curr.attr,					\
	&dev_attr_xmc_se98_temp0.attr,					\
	&dev_attr_xmc_se98_temp1.attr,					\
	&dev_attr_xmc_se98_temp2.attr,					\
	&dev_attr_xmc_cage_temp0.attr,					\
	&dev_attr_xmc_cage_temp1.attr,					\
	&dev_attr_xmc_cage_temp2.attr,					\
	&dev_attr_xmc_cage_temp3.attr

/*
 * Defining sysfs nodes for reading some of xmc regisers.
 */
#define	REG_SYSFS_NODE(node_name, reg, format)				\
	static ssize_t node_name##_show(struct device *dev,		\
		struct device_attribute *attr, char *buf) {		\
		struct xocl_xmc *xmc =					\
			platform_get_drvdata(to_platform_device(dev));	\
		u32 val;						\
		safe_read32(xmc, reg, &val);				\
		return sprintf(buf, format, val);			\
	}								\
	static DEVICE_ATTR_RO(node_name)
REG_SYSFS_NODE(version, XMC_VERSION_REG, "%d\n");
REG_SYSFS_NODE(sensor, XMC_SENSOR_REG, "0x%04x\n");
REG_SYSFS_NODE(id, XMC_MAGIC_REG, "0x%x\n");
REG_SYSFS_NODE(status, XMC_STATUS_REG, "0x%x\n");
REG_SYSFS_NODE(error, XMC_ERROR_REG, "0x%x\n");
REG_SYSFS_NODE(capability, XMC_FEATURE_REG, "0x%x\n");
REG_SYSFS_NODE(power_checksum, XMC_SNSR_CHKSUM_REG, "%d\n");
REG_SYSFS_NODE(power_flag, XMC_SNSR_FLAGS_REG, "%d\n");
REG_SYSFS_NODE(host_msg_offset, XMC_HOST_MSG_OFFSET_REG, "%d\n");
REG_SYSFS_NODE(host_msg_error, XMC_HOST_MSG_ERROR_REG, "0x%x\n");
REG_SYSFS_NODE(host_msg_header, XMC_HOST_MSG_HEADER_REG, "0x%x\n");
#define	REG_SYSFS_NODE_ATTRS						\
	&dev_attr_version.attr,						\
	&dev_attr_sensor.attr,						\
	&dev_attr_id.attr,						\
	&dev_attr_status.attr,						\
	&dev_attr_error.attr,						\
	&dev_attr_capability.attr,					\
	&dev_attr_power_checksum.attr,					\
	&dev_attr_power_flag.attr,					\
	&dev_attr_host_msg_offset.attr,					\
	&dev_attr_host_msg_error.attr,					\
	&dev_attr_host_msg_header.attr


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

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 1)
		return -EINVAL;

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

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 1)
		return -EINVAL;

	if (val)
		load_xmc(xmc);


	return count;
}
static DEVICE_ATTR_WO(reset);


static ssize_t cache_expire_secs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u64 val = 0;

	mutex_lock(&xmc->xmc_lock);
	if (!XMC_PRIVILEGED(xmc))
		val = xmc->cache_expire_secs;

	mutex_unlock(&xmc->xmc_lock);
	return sprintf(buf, "%llu\n", val);
}
static ssize_t cache_expire_secs_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u64 val;

	mutex_lock(&xmc->xmc_lock);
	if (kstrtou64(buf, 10, &val) == -EINVAL || val > 10) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0 ~ 10] > cache_expire_secs");
		return -EINVAL;
	}

	if (!XMC_PRIVILEGED(xmc))
		xmc->cache_expire_secs = val;

	mutex_unlock(&xmc->xmc_lock);
	return count;
}
static DEVICE_ATTR_RW(cache_expire_secs);

static int get_temp_by_m_tag(struct xocl_xmc *xmc, char *m_tag)
{
	/*
	 * m_tag get from xclbin must follow this format
	 * DDR[0] or bank1
	 * we check the index in m_tag to decide which temperature
	 * to get from XMC IP base address
	 */
	char *start = NULL, *left_parentness = NULL, *right_parentness = NULL;
	long idx;
	int ret = 0, digit_len = 0;
	char temp[4];

	if (!xmc)
		return -ENODEV;

	if (!strncmp(m_tag, "HBM", 3)) {
		xmc_sensor(xmc->pdev, HBM_TEMP, &ret, SENSOR_INS);
		return ret;
	}

	if (!strncmp(m_tag, "bank", 4)) {
		start = m_tag;
		/* bank0, no left parentness */
		left_parentness = m_tag+3;
		right_parentness = m_tag+strlen(m_tag)+1;
		digit_len = right_parentness-(2+left_parentness);
	} else if (!strncmp(m_tag, "DDR", 3)) {

		start = m_tag;
		left_parentness = strstr(m_tag, "[");
		right_parentness = strstr(m_tag, "]");
		digit_len = right_parentness-(1+left_parentness);
	}

	if (!left_parentness || !right_parentness)
		return ret;

	if (!strncmp(m_tag, "DDR", left_parentness-start) ||
		!strncmp(m_tag, "bank", left_parentness-start)) {

		strncpy(temp, left_parentness+1, digit_len);
		/* assumption, temperature won't higher than 3 digits, or
		 * the temp[digit_len] should be a null character */
		temp[digit_len] = '\0';
		/* convert to signed long, decimal base */
		if (kstrtol(temp, 10, &idx))
			return ret;

		switch (idx) {
		case 0:
			xmc_sensor(xmc->pdev, DIMM0_TEMP, &ret, SENSOR_INS);
			break;
		case 1:
			xmc_sensor(xmc->pdev, DIMM1_TEMP, &ret, SENSOR_INS);
			break;
		case 2:
			xmc_sensor(xmc->pdev, DIMM2_TEMP, &ret, SENSOR_INS);
			break;
		case 3:
			xmc_sensor(xmc->pdev, DIMM3_TEMP, &ret, SENSOR_INS);
			break;
		}

	}

	return ret;

}

/* Runtime clock scaling sysfs node */
static ssize_t scaling_governor_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 mode;
	char val[10];

	if (!xmc->runtime_cs_enabled) {
		xocl_err(dev, "runtime clock scaling is not supported\n");
		return -EIO;
	}
	mutex_lock(&xmc->xmc_lock);
	mode = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_MODE_REG);
	mutex_unlock(&xmc->xmc_lock);

	switch (mode) {
	case 0:
		strcpy(val, "power");
		break;
	case 1:
		strcpy(val, "temp");
		break;
	}

	return sprintf(buf, "%s\n", val);
}

static ssize_t scaling_governor_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;

	/* Check if clock scaling feature enabled */
	if (!xmc->runtime_cs_enabled) {
		xocl_err(dev, "runtime clock scaling is not supported\n");
		return -EIO;
	}

	if (strncmp(buf, "power", strlen("power")) == 0)
		val = XMC_CLOCK_SCALING_MODE_POWER;
	else if (strncmp(buf, "temp", strlen("temp")) == 0)
		val = XMC_CLOCK_SCALING_MODE_TEMP;
	else {
		xocl_err(dev, "valid modes [power, temp]\n");
		return -EINVAL;
	}

	mutex_lock(&xmc->xmc_lock);
	WRITE_RUNTIME_CS(xmc, val, XMC_CLOCK_SCALING_MODE_REG);
	mutex_unlock(&xmc->xmc_lock);

	return count;
}
static DEVICE_ATTR_RW(scaling_governor);

static ssize_t scaling_cur_temp_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	u32 board_temp;

	xmc_sensor(pdev, FPGA_TEMP, &board_temp, SENSOR_INS);

	return sprintf(buf, "%d\n", board_temp);
}
static DEVICE_ATTR_RO(scaling_cur_temp);

static ssize_t scaling_cur_power_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	u32 mPexCurr, m12VPex, mAuxCurr, m12VAux, board_power;

	//Measure board power in terms of Watts and store it in register
	xmc_sensor(pdev, VOL_12V_PEX, &m12VPex, SENSOR_INS);
	xmc_sensor(pdev, VOL_12V_AUX, &m12VAux, SENSOR_INS);
	xmc_sensor(pdev, CUR_12V_PEX, &mPexCurr, SENSOR_INS);
	xmc_sensor(pdev, CUR_12V_AUX, &mAuxCurr, SENSOR_INS);

	board_power = ((mPexCurr * m12VPex) + (mAuxCurr * m12VAux)) / 1000000;

	return sprintf(buf, "%d\n", board_power);
}
static DEVICE_ATTR_RO(scaling_cur_power);

static ssize_t scaling_enabled_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	if (!xmc->runtime_cs_enabled) {
		val = 0;
		return sprintf(buf, "%d\n", val);
	}

	mutex_lock(&xmc->xmc_lock);
	val = READ_RUNTIME_CS(xmc, XMC_CLOCK_CONTROL_REG);
	if (val & XMC_CLOCK_SCALING_EN)
		val = 1;
	else
		val = 0;

	mutex_unlock(&xmc->xmc_lock);
	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(scaling_enabled);

static ssize_t scaling_target_power_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	if (!xmc->runtime_cs_enabled) {
		xocl_err(dev, "runtime clock scaling is not supported\n");
		return -EIO;
	}
	mutex_lock(&xmc->xmc_lock);
	val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_POWER_REG);
	val &= XMC_CLOCK_SCALING_POWER_REG_MASK;
	mutex_unlock(&xmc->xmc_lock);

	return sprintf(buf, "%uW\n", val);
}

static ssize_t scaling_target_power_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val, val2;

	if (!xmc->runtime_cs_enabled) {
		xocl_err(dev, "runtime clock scaling is not supported\n");
		return -EIO;
	}

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	//TODO: Check if the threshold power is in board spec limits.
	mutex_lock(&xmc->xmc_lock);
	val2 = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_POWER_REG);
	val2 &= ~XMC_CLOCK_SCALING_POWER_REG_MASK;
	val2 |= (val & XMC_CLOCK_SCALING_POWER_REG_MASK);
	WRITE_RUNTIME_CS(xmc, val2, XMC_CLOCK_SCALING_POWER_REG);
	mutex_unlock(&xmc->xmc_lock);

	return count;
}
static DEVICE_ATTR_RW(scaling_target_power);

static ssize_t scaling_target_temp_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val;

	if (!xmc->runtime_cs_enabled) {
		xocl_err(dev, "runtime clock scaling is not supported\n");
		return -EIO;
	}
	mutex_lock(&xmc->xmc_lock);
	val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_TEMP_REG);
	val &= XMC_CLOCK_SCALING_TEMP_REG_MASK;
	mutex_unlock(&xmc->xmc_lock);

	return sprintf(buf, "%uc\n", val);
}

static ssize_t scaling_target_temp_store(struct device *dev,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val, val2;

	/* Check if clock scaling feature enabled */
	if (!xmc->runtime_cs_enabled) {
		xocl_err(dev, "runtime clock scaling is not supported\n");
		return -EIO;
	}

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	//TODO: Check if the threshold temperature is in board spec limits.
	mutex_lock(&xmc->xmc_lock);
	val2 = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_TEMP_REG);
	val2 &= ~XMC_CLOCK_SCALING_TEMP_REG_MASK;
	val2 |= (val & XMC_CLOCK_SCALING_TEMP_REG_MASK);
	WRITE_RUNTIME_CS(xmc, val2, XMC_CLOCK_SCALING_TEMP_REG);
	mutex_unlock(&xmc->xmc_lock);

	return count;
}
static DEVICE_ATTR_RW(scaling_target_temp);

static ssize_t board_info_show(struct device *dev,
	struct device_attribute *da, char *buf);
static DEVICE_ATTR_RO(board_info);

static struct attribute *xmc_attrs[] = {
	&dev_attr_pause.attr,
	&dev_attr_reset.attr,
	&dev_attr_cache_expire_secs.attr,
	&dev_attr_scaling_enabled.attr,
	&dev_attr_scaling_cur_temp.attr,
	&dev_attr_scaling_cur_power.attr,
	&dev_attr_scaling_target_temp.attr,
	&dev_attr_scaling_target_power.attr,
	&dev_attr_scaling_governor.attr,
	&dev_attr_board_info.attr,
	SENSOR_SYSFS_NODE_ATTRS,
	REG_SYSFS_NODE_ATTRS,
	NULL,
};


static ssize_t read_temp_by_mem_topology(struct file *filp,
	struct kobject *kobj, struct bin_attribute *attr, char *buffer,
	loff_t offset, size_t count)
{
	u32 nread = 0;
	size_t size = 0;
	u32 i;
	struct mem_topology *memtopo = NULL;
	struct xocl_xmc *xmc =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	uint32_t temp[MAX_M_COUNT] = {0};
	xdev_handle_t xdev = xocl_get_xdev(xmc->pdev);

	memtopo = (struct mem_topology *)xocl_icap_get_data(xdev, MEMTOPO_AXLF);

	if (!memtopo)
		return 0;

	size = sizeof(u32)*(memtopo->m_count);

	if (offset >= size)
		return 0;
	for (i = 0; i < memtopo->m_count; ++i)
		*(temp+i) = get_temp_by_m_tag(xmc, memtopo->m_mem_data[i].m_tag);

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, temp, nread);
	/* xocl_icap_unlock_bitstream */
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

/*
 * Defining sysfs nodes for HWMON.
 */
#define	HWMON_INDEX(sensor, val_kind)	(sensor | (val_kind << 24))
#define	HWMON_INDEX2SENSOR(index)	(index & 0xffffff)
#define	HWMON_INDEX2VAL_KIND(index)	((index & ~0xffffff) >> 24)

/* For voltage and current */
static ssize_t hwmon_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	int index = to_sensor_dev_attr(da)->index;
	u32 val;

	xmc_sensor(pdev, HWMON_INDEX2SENSOR(index), &val,
		HWMON_INDEX2VAL_KIND(index));
	return sprintf(buf, "%d\n", val);
}

#define	HWMON_VOLT_CURR_SYSFS_NODE(type, id, name, sensor)		\
	static ssize_t type##id##_label(struct device *dev,		\
		struct device_attribute *attr, char *buf) {		\
		return sprintf(buf, "%s\n", name);			\
	}								\
	static SENSOR_DEVICE_ATTR(type##id##_max, 0444, hwmon_show,	\
		NULL, HWMON_INDEX(sensor, SENSOR_MAX));			\
	static SENSOR_DEVICE_ATTR(type##id##_average, 0444, hwmon_show,	\
		NULL, HWMON_INDEX(sensor, SENSOR_AVG));			\
	static SENSOR_DEVICE_ATTR(type##id##_input, 0444, hwmon_show,	\
		NULL, HWMON_INDEX(sensor, SENSOR_INS));			\
	static SENSOR_DEVICE_ATTR(type##id##_label, 0444, type##id##_label, \
		NULL, HWMON_INDEX(sensor, SENSOR_INS))
#define	HWMON_VOLT_CURR_ATTRS(type, id)					\
	&sensor_dev_attr_##type##id##_max.dev_attr.attr,		\
	&sensor_dev_attr_##type##id##_average.dev_attr.attr,		\
	&sensor_dev_attr_##type##id##_input.dev_attr.attr,		\
	&sensor_dev_attr_##type##id##_label.dev_attr.attr

/* For fan speed. */
#define	HWMON_FAN_SPEED_SYSFS_NODE(id, name, sensor)			\
	static ssize_t fan##id##_label(struct device *dev,		\
		struct device_attribute *attr, char *buf) {		\
		return sprintf(buf, "%s\n", name);			\
	}								\
	static SENSOR_DEVICE_ATTR(fan##id##_input, 0444, hwmon_show,	\
		NULL, HWMON_INDEX(sensor, SENSOR_INS));			\
	static SENSOR_DEVICE_ATTR(fan##id##_label, 0444, fan##id##_label, \
		NULL, HWMON_INDEX(sensor, SENSOR_INS))
#define	HWMON_FAN_SPEED_ATTRS(id)					\
	&sensor_dev_attr_fan##id##_input.dev_attr.attr,			\
	&sensor_dev_attr_fan##id##_label.dev_attr.attr

/* For temperature */
static ssize_t hwmon_temp_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	int index = to_sensor_dev_attr(da)->index;
	u32 val;

	xmc_sensor(pdev, HWMON_INDEX2SENSOR(index), &val,
		HWMON_INDEX2VAL_KIND(index));
	return sprintf(buf, "%d\n", val * 1000);
}

#define	HWMON_TEMPERATURE_SYSFS_NODE(id, name, sensor)			\
	static ssize_t temp##id##_label(struct device *dev,		\
		struct device_attribute *attr, char *buf) {		\
		return sprintf(buf, "%s\n", name);			\
	}								\
	static SENSOR_DEVICE_ATTR(temp##id##_highest, 0444, hwmon_temp_show,\
		NULL, HWMON_INDEX(sensor, SENSOR_MAX));			\
	static SENSOR_DEVICE_ATTR(temp##id##_input, 0444, hwmon_temp_show, \
		NULL, HWMON_INDEX(sensor, SENSOR_INS));			\
	static SENSOR_DEVICE_ATTR(temp##id##_label, 0444, temp##id##_label, \
		NULL, HWMON_INDEX(sensor, SENSOR_INS))
#define	HWMON_TEMPERATURE_ATTRS(id)					\
	&sensor_dev_attr_temp##id##_highest.dev_attr.attr,		\
	&sensor_dev_attr_temp##id##_input.dev_attr.attr,		\
	&sensor_dev_attr_temp##id##_label.dev_attr.attr

HWMON_VOLT_CURR_SYSFS_NODE(in, 0, "12V PEX", VOL_12V_PEX);
HWMON_VOLT_CURR_SYSFS_NODE(in, 1, "12V AUX", VOL_12V_AUX);
HWMON_VOLT_CURR_SYSFS_NODE(in, 2, "3V3 PEX", VOL_3V3_PEX);
HWMON_VOLT_CURR_SYSFS_NODE(in, 3, "3V3 AUX", VOL_3V3_AUX);
HWMON_VOLT_CURR_SYSFS_NODE(in, 4, "5V5 SYS", VOL_5V5_SYS);
HWMON_VOLT_CURR_SYSFS_NODE(in, 5, "1V2 TOP", VOL_1V2_TOP);
HWMON_VOLT_CURR_SYSFS_NODE(in, 6, "1V2 BTM", VOL_1V2_BTM);
HWMON_VOLT_CURR_SYSFS_NODE(in, 7, "1V8 TOP", VOL_1V8);
HWMON_VOLT_CURR_SYSFS_NODE(in, 8, "12V SW", VOL_12V_SW);
HWMON_VOLT_CURR_SYSFS_NODE(in, 9, "VCC INT", VOL_VCC_INT);
HWMON_VOLT_CURR_SYSFS_NODE(in, 10, "0V9 MGT", VCC_0V9A);
HWMON_VOLT_CURR_SYSFS_NODE(in, 11, "0V85", VCC_0V85);
HWMON_VOLT_CURR_SYSFS_NODE(in, 12, "MGT VTT", VTT_MGTA);
HWMON_VOLT_CURR_SYSFS_NODE(in, 13, "DDR VPP BOTTOM", VPP_BTM);
HWMON_VOLT_CURR_SYSFS_NODE(in, 14, "DDR VPP TOP", VPP_TOP);
HWMON_VOLT_CURR_SYSFS_NODE(curr, 1, "12V PEX Current", CUR_12V_PEX);
HWMON_VOLT_CURR_SYSFS_NODE(curr, 2, "12V AUX Current", CUR_12V_AUX);
HWMON_VOLT_CURR_SYSFS_NODE(curr, 3, "VCC INT Current", CUR_VCC_INT);
HWMON_TEMPERATURE_SYSFS_NODE(1, "PCB TOP FRONT", SE98_TEMP0);
HWMON_TEMPERATURE_SYSFS_NODE(2, "PCB TOP REAR", SE98_TEMP1);
HWMON_TEMPERATURE_SYSFS_NODE(3, "PCB BTM FRONT", SE98_TEMP2);
HWMON_TEMPERATURE_SYSFS_NODE(4, "FPGA TEMP", FPGA_TEMP);
HWMON_TEMPERATURE_SYSFS_NODE(5, "TCRIT TEMP", FAN_TEMP);
HWMON_TEMPERATURE_SYSFS_NODE(6, "DIMM0 TEMP", DIMM0_TEMP);
HWMON_TEMPERATURE_SYSFS_NODE(7, "DIMM1 TEMP", DIMM1_TEMP);
HWMON_TEMPERATURE_SYSFS_NODE(8, "DIMM2 TEMP", DIMM2_TEMP);
HWMON_TEMPERATURE_SYSFS_NODE(9, "DIMM3 TEMP", DIMM3_TEMP);
HWMON_TEMPERATURE_SYSFS_NODE(10, "HBM TEMP", HBM_TEMP);
HWMON_TEMPERATURE_SYSFS_NODE(11, "QSPF 0", CAGE_TEMP0);
HWMON_TEMPERATURE_SYSFS_NODE(12, "QSPF 1", CAGE_TEMP1);
HWMON_TEMPERATURE_SYSFS_NODE(13, "QSPF 2", CAGE_TEMP2);
HWMON_TEMPERATURE_SYSFS_NODE(14, "QSPF 3", CAGE_TEMP3);
HWMON_FAN_SPEED_SYSFS_NODE(1, "FAN SPEED", FAN_RPM);
static struct attribute *hwmon_xmc_attributes[] = {
	HWMON_VOLT_CURR_ATTRS(in, 0),
	HWMON_VOLT_CURR_ATTRS(in, 1),
	HWMON_VOLT_CURR_ATTRS(in, 2),
	HWMON_VOLT_CURR_ATTRS(in, 3),
	HWMON_VOLT_CURR_ATTRS(in, 4),
	HWMON_VOLT_CURR_ATTRS(in, 5),
	HWMON_VOLT_CURR_ATTRS(in, 6),
	HWMON_VOLT_CURR_ATTRS(in, 7),
	HWMON_VOLT_CURR_ATTRS(in, 8),
	HWMON_VOLT_CURR_ATTRS(in, 9),
	HWMON_VOLT_CURR_ATTRS(in, 10),
	HWMON_VOLT_CURR_ATTRS(in, 11),
	HWMON_VOLT_CURR_ATTRS(in, 12),
	HWMON_VOLT_CURR_ATTRS(in, 13),
	HWMON_VOLT_CURR_ATTRS(in, 14),
	HWMON_VOLT_CURR_ATTRS(curr, 1),
	HWMON_VOLT_CURR_ATTRS(curr, 2),
	HWMON_VOLT_CURR_ATTRS(curr, 3),
	HWMON_TEMPERATURE_ATTRS(1),
	HWMON_TEMPERATURE_ATTRS(2),
	HWMON_TEMPERATURE_ATTRS(3),
	HWMON_TEMPERATURE_ATTRS(4),
	HWMON_TEMPERATURE_ATTRS(5),
	HWMON_TEMPERATURE_ATTRS(6),
	HWMON_TEMPERATURE_ATTRS(7),
	HWMON_TEMPERATURE_ATTRS(8),
	HWMON_TEMPERATURE_ATTRS(9),
	HWMON_TEMPERATURE_ATTRS(10),
	HWMON_TEMPERATURE_ATTRS(11),
	HWMON_TEMPERATURE_ATTRS(12),
	HWMON_TEMPERATURE_ATTRS(13),
	HWMON_TEMPERATURE_ATTRS(14),
	HWMON_FAN_SPEED_ATTRS(1),
	NULL
};

static const struct attribute_group hwmon_xmc_attrgroup = {
	.attrs = hwmon_xmc_attributes,
};

static ssize_t show_hwmon_name(struct device *dev, struct device_attribute *da,
	char *buf)
{
	struct FeatureRomHeader rom = { {0} };
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	void *xdev_hdl = xocl_get_xdev(xmc->pdev);
	char nm[150] = { 0 };
	int n;

	xocl_get_raw_header(xdev_hdl, &rom);
	n = snprintf(nm, sizeof(nm), "%s", rom.VBNVName);
	if (XMC_PRIVILEGED(xmc))
		(void) snprintf(nm + n, sizeof(nm) - n, "%s", "-mgmt");
	else
		(void) snprintf(nm + n, sizeof(nm) - n, "%s", "-user");
	return sprintf(buf, "%s\n", nm);
}
static struct sensor_device_attribute name_attr =
	SENSOR_ATTR(name, 0444, show_hwmon_name, NULL, 0);

static void mgmt_sysfs_destroy_xmc(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;

	xmc = platform_get_drvdata(pdev);

	if (!xmc->enabled)
		return;

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

	if (!xmc->enabled)
		return 0;

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
	xmc->hwmon_dev = NULL;
hwmon_reg_failed:
	sysfs_remove_group(&pdev->dev.kobj, &xmc_attr_group);
create_attr_failed:
	return err;
}

static int stop_xmc_nolock(struct platform_device *pdev)
{
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

	/* Stop XMC and ERT if its currently running */
	if (reg_val == GPIO_ENABLED) {
		xocl_info(&xmc->pdev->dev,
			"XMC info, version 0x%x, status 0x%x, id 0x%x",
			READ_REG32(xmc, XMC_VERSION_REG),
			READ_REG32(xmc, XMC_STATUS_REG),
			READ_REG32(xmc, XMC_MAGIC_REG));

		reg_val = READ_REG32(xmc, XMC_STATUS_REG);
		if (!(reg_val & STATUS_MASK_STOPPED)) {
			xocl_info(&xmc->pdev->dev, "Stopping XMC...");
			WRITE_REG32(xmc, CTL_MASK_STOP, XMC_CONTROL_REG);
			WRITE_REG32(xmc, 1, XMC_STOP_CONFIRM_REG);
		}
		/* Need to check if ERT is loaded before we attempt to stop it */
		if (!SELF_JUMP(READ_IMAGE_SCHED(xmc, 0))) {
			reg_val = XOCL_READ_REG32(xmc->base_addrs[IO_CQ]);
			if (!(reg_val & ERT_EXIT_ACK)) {
				xocl_info(&xmc->pdev->dev, "Stopping scheduler...");
				XOCL_WRITE_REG32(ERT_EXIT_CMD, xmc->base_addrs[IO_CQ]);
			}
		}

		retry = 0;
		while (retry++ < MAX_XMC_RETRY &&
			!(READ_REG32(xmc, XMC_STATUS_REG) & STATUS_MASK_STOPPED))
			msleep(RETRY_INTERVAL);

		/* Wait for XMC to stop and then check that ERT has also finished */
		if (retry >= MAX_XMC_RETRY) {
			xocl_err(&xmc->pdev->dev, "Failed to stop XMC");
			xocl_err(&xmc->pdev->dev, "XMC Error Reg 0x%x",
				READ_REG32(xmc, XMC_ERROR_REG));
			xmc->state = XMC_STATE_ERROR;
			return -ETIMEDOUT;
		} else if (!SELF_JUMP(READ_IMAGE_SCHED(xmc, 0)) &&
			 !(XOCL_READ_REG32(xmc->base_addrs[IO_CQ]) & ERT_EXIT_ACK)) {
			while (retry++ < MAX_ERT_RETRY &&
				!(XOCL_READ_REG32(xmc->base_addrs[IO_CQ]) & ERT_EXIT_ACK))
				msleep(RETRY_INTERVAL);
			if (retry >= MAX_ERT_RETRY) {
				xocl_err(&xmc->pdev->dev,
					"Failed to stop sched");
				xocl_err(&xmc->pdev->dev,
					"Scheduler CQ status 0x%x",
					XOCL_READ_REG32(xmc->base_addrs[IO_CQ]));
				/*
				 * We don't exit if ERT doesn't stop since
				 * it can hang due to bad kernel xmc->state =
				 * XMC_STATE_ERROR; return -ETIMEDOUT;
				 */
			}
		}

		xocl_info(&xmc->pdev->dev, "XMC/sched Stopped, retry %d",
			retry);
	}

	/* Hold XMC in reset now that its safely stopped */
	xocl_info(&xmc->pdev->dev,
		"XMC info, version 0x%x, status 0x%x, id 0x%x",
		READ_REG32(xmc, XMC_VERSION_REG),
		READ_REG32(xmc, XMC_STATUS_REG),
		READ_REG32(xmc, XMC_MAGIC_REG));
	WRITE_GPIO(xmc, GPIO_RESET, 0);
	xmc->state = XMC_STATE_RESET;
	reg_val = READ_GPIO(xmc, 0);
	xocl_info(&xmc->pdev->dev, "MB Reset GPIO 0x%x", reg_val);
	/* Shouldnt make it here but if we do then exit */
	if (reg_val != GPIO_RESET) {
		xmc->state = XMC_STATE_ERROR;
		return -EIO;
	}

	return 0;
}
static int stop_xmc(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	int ret = 0;

	xocl_info(&pdev->dev, "Stop Microblaze...");
	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return -ENODEV;
	else if (!xmc->enabled)
		return -ENODEV;

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

	if (!xmc->enabled)
		return -ENODEV;

	mutex_lock(&xmc->xmc_lock);

	/* Stop XMC first */
	ret = stop_xmc_nolock(xmc->pdev);
	if (ret != 0)
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
	/* Shouldnt make it here but if we do then exit */
	if (reg_val != GPIO_ENABLED) {
		xmc->state = XMC_STATE_ERROR;
		goto out;
	}

	/* Wait for XMC to start
	 * Note that ERT will start long before XMC so we don't check anything
	 */
	reg_val = READ_REG32(xmc, XMC_STATUS_REG);
	if (!(reg_val & STATUS_MASK_INIT_DONE)) {
		xocl_info(&xmc->pdev->dev, "Waiting for XMC to finish init...");
		retry = 0;
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
	if (!xmc->mgmt_binary)
		return -ENOMEM;

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
	if (!xmc->sche_binary)
		return -ENOMEM;

	if (binary)
		devm_kfree(&pdev->dev, binary);
	memcpy(xmc->sche_binary, image, len);
	xmc->sche_binary_length = len;

	return 0;
}

static void xmc_clk_scale_config(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	u32 cntrl;

	xmc = platform_get_drvdata(pdev);
	if (!xmc) {
		xocl_info(&pdev->dev, "failed since xmc handle is null\n");
		return;
	}

	cntrl = READ_RUNTIME_CS(xmc, XMC_CLOCK_CONTROL_REG);
	cntrl |= XMC_CLOCK_SCALING_EN;
	WRITE_RUNTIME_CS(xmc, cntrl, XMC_CLOCK_CONTROL_REG);
}

static struct xocl_mb_funcs xmc_ops = {
	.load_mgmt_image	= load_mgmt_image,
	.load_sche_image	= load_sche_image,
	.reset			= xmc_reset,
	.stop			= stop_xmc,
	.get_data		= xmc_get_data,
};

static void xmc_unload_board_info(struct xocl_xmc *xmc)
{
	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));
	vfree(xmc->bdinfo_raw);
	xmc->bdinfo_raw = NULL;
	xmc->bdinfo_raw_sz = 0;
}

static int xmc_remove(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	int	i;

	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return 0;

	if (xmc->mgmt_binary)
		devm_kfree(&pdev->dev, xmc->mgmt_binary);
	if (xmc->sche_binary)
		devm_kfree(&pdev->dev, xmc->sche_binary);

	mgmt_sysfs_destroy_xmc(pdev);

	mutex_lock(&xmc->mbx_lock);
	xmc_unload_board_info(xmc);
	mutex_unlock(&xmc->mbx_lock);

	for (i = 0; i < NUM_IOADDR; i++) {
		if ((i == IO_CLK_SCALING) && !xmc->runtime_cs_enabled)
			continue;
		if (xmc->base_addrs[i])
			iounmap(xmc->base_addrs[i]);
	}

	mutex_destroy(&xmc->xmc_lock);
	mutex_destroy(&xmc->mbx_lock);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, xmc);
	return 0;
}

static int xmc_probe(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	struct resource *res;
	void *xdev_hdl;
	int i, err;
	u32 val;

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
		xocl_err(&pdev->dev, "Microblaze is not supported.");
		devm_kfree(&pdev->dev, xmc);
		platform_set_drvdata(pdev, NULL);
		return 0;
	}

	for (i = 0; i < NUM_IOADDR; i++) {
		if ((i == IO_CLK_SCALING) && !xmc->runtime_cs_enabled)
			continue;
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res) {
			xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
				res->start, res->end);
			xmc->base_addrs[i] = ioremap_nocache(res->start,
				res->end - res->start + 1);
			if (!xmc->base_addrs[i]) {
				err = -EIO;
				xocl_err(&pdev->dev, "Map iomem failed");
				goto failed;
			}
		} else
			break;
	}

	if (READ_GPIO(xmc, 0) == GPIO_ENABLED)
		xmc->state = XMC_STATE_ENABLED;

	err = mgmt_sysfs_create_xmc(pdev);
	if (err) {
		xocl_err(&pdev->dev, "Create sysfs failed, err %d", err);
		goto failed;
	}

	mutex_init(&xmc->xmc_lock);
	xmc->cache_expire_secs = XMC_DEFAULT_EXPIRE_SECS;

	/*
	 * Enabling XMC clock scaling support.
	 * clk scaling can only be enabled on mgmt side, why do we set
	 * the enabled bit in feature ROM on user side at all?
	 */
	if (XMC_PRIVILEGED(xmc) && xocl_clk_scale_on(xdev_hdl)) {
		xmc->runtime_cs_enabled = true;
		xmc_clk_scale_config(pdev);
		xocl_info(&pdev->dev, "Runtime clock scaling is supported.\n");
	}

	/* Enabling XMC mailbox support. */
	mutex_init(&xmc->mbx_lock);
	if (XMC_PRIVILEGED(xmc)) {
		xmc->mbx_enabled = true;
		safe_read32(xmc, XMC_HOST_MSG_OFFSET_REG, &val);
		xmc->mbx_offset = val;
		xocl_info(&pdev->dev, "XMC mailbox offset: 0x%x.\n", val);
	}

	return 0;

failed:
	xmc_remove(pdev);
	return err;
}

struct xocl_drv_private	xmc_priv = {
	.ops = &xmc_ops,
};

struct platform_device_id xmc_id_table[] = {
	{ XOCL_DEVNAME(XOCL_XMC), (kernel_ulong_t)&xmc_priv },
	{ },
};

static struct platform_driver	xmc_driver = {
	.probe		= xmc_probe,
	.remove		= xmc_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_XMC),
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

static int xmc_mailbox_wait(struct xocl_xmc *xmc)
{
	int retry = MAX_XMC_MBX_RETRY;
	u32 val;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	safe_read32(xmc, XMC_CONTROL_REG, &val);
	while ((retry-- > 0) && (val & XMC_PKT_OWNER_MASK)){
		msleep(RETRY_INTERVAL);
		safe_read32(xmc, XMC_CONTROL_REG, &val);
	}

	if (retry == 0) {
		xocl_err(&xmc->pdev->dev, "XMC packet error: time'd out\n");
		return -ETIMEDOUT;
	}

	safe_read32(xmc, XMC_ERROR_REG, &val);
	if (val & XMC_PKT_ERR_MASK)
		safe_read32(xmc, XMC_HOST_MSG_ERROR_REG, &val);

	if (val) {
		xocl_err(&xmc->pdev->dev, "XMC packet error: %d\n", val);
		return -EINVAL;
	}

	return 0;
}

static int xmc_send_pkt(struct xocl_xmc *xmc)
{
	u32 *pkt = (u32 *)&xmc->mbx_pkt;
	u32 len = XMC_PKT_SZ(&xmc->mbx_pkt.hdr);
	int ret;
	u32 i;
	u32 val;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	/* Make sure HW is done with the mailbox buffer. */
	ret = xmc_mailbox_wait(xmc);
	if (ret != 0)
		return ret;

	/* Push pkt data to mailbox on HW. */
	for (i = 0; i < len; i++)
		safe_write32(xmc, xmc->mbx_offset + i * sizeof(u32), pkt[i]);
	
	/* Notify HW that a pkt is ready for process. */
	safe_read32(xmc, XMC_CONTROL_REG, &val);
	safe_write32(xmc, XMC_CONTROL_REG, val | XMC_PKT_OWNER_MASK);
	return 0;
}

static int xmc_recv_pkt(struct xocl_xmc *xmc)
{
	struct xmc_pkt_hdr hdr;
	u32 *pkt;
	u32 len;
	u32 i;
	int ret;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	/* Make sure HW is done with the mailbox buffer. */
	ret = xmc_mailbox_wait(xmc);
	if (ret != 0)
		return ret;

	/* Receive pkt hdr. */
	pkt = (u32 *)&hdr;
	len = sizeof(hdr) / sizeof(u32);
	for (i = 0; i < len; i++)
		safe_read32(xmc, xmc->mbx_offset + i * sizeof(u32), &pkt[i]);

	pkt = (u32 *)&xmc->mbx_pkt;
	len = XMC_PKT_SZ(&hdr);
	if (hdr.payload_sz == 0 || len > XMC_PKT_MAX_SZ) {
		xocl_err(&xmc->pdev->dev, "read invalid XMC packet\n");
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		safe_read32(xmc, xmc->mbx_offset + i * sizeof(u32), &pkt[i]);
	return 0;
}

static bool is_xmc_ready(struct xocl_xmc *xmc)
{
	if (xmc->state == XMC_STATE_ENABLED)
		return true;

	xocl_err(&xmc->pdev->dev, "XMC is not ready, state=%d\n", xmc->state);
	return false;
}

static bool is_sc_ready(struct xocl_xmc *xmc)
{
	u32 val;

	safe_read32(xmc, XMC_STATUS_REG, &val);
	val >>= 28;
	if (val == 0x1)
		return true;

	xocl_err(&xmc->pdev->dev, "SC is not ready, state=%d\n", val);
	return false;
}

static int xmc_load_board_info(struct xocl_xmc *xmc)
{
	int ret = 0;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	if (!is_xmc_ready(xmc) || !is_sc_ready(xmc))
		return -EINVAL;

	/* Release current info. */
	xmc_unload_board_info(xmc);

	/* Load new info from HW. */
	memset(&xmc->mbx_pkt, 0, sizeof(xmc->mbx_pkt));
	xmc->mbx_pkt.hdr.op = XPO_BOARD_INFO;
	ret = xmc_send_pkt(xmc);
	if (ret)
		return ret;

	ret = xmc_recv_pkt(xmc);
	if (ret)
		return ret;

	xmc->bdinfo_raw = vmalloc(xmc->mbx_pkt.hdr.payload_sz);
	if (xmc->bdinfo_raw == NULL)
		return -ENOMEM;

	memcpy(xmc->bdinfo_raw, xmc->mbx_pkt.data, xmc->mbx_pkt.hdr.payload_sz);
	xmc->bdinfo_raw_sz = xmc->mbx_pkt.hdr.payload_sz;
	xocl_info(&xmc->pdev->dev, "board info reloaded\n");
	return 0;
}

static const char *xmc_get_board_info(struct xocl_xmc *xmc,
	enum board_info_key key, size_t *len)
{
	char *buf, *p;
	u32 sz;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	if (xmc->bdinfo_raw == NULL) {
		if (xmc_load_board_info(xmc) != 0) {		
			xocl_err(&xmc->pdev->dev, "board info not available\n");
			return NULL;
		}
	}

	buf = xmc->bdinfo_raw;
	sz = xmc->bdinfo_raw_sz;
	for (p = buf; p < buf + sz;) {
		char k = *(p++);
		u8 l = *(p++);
		if (k == key) {
			if (len)
				*len = l;
			return p;
		}
		p += l;
	}

	xocl_err(&xmc->pdev->dev, "can't find key ID %d in board info\n", key);
	return NULL;
}

static u64 xmc_get_board_info_int(struct xocl_xmc *xmc, enum board_info_key key)
{
	u64 rval = 0;
	size_t len;
	const char *p;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	p = xmc_get_board_info(xmc, key, &len);
	if (p == NULL)
		return rval;
	if (len > sizeof(rval)) {
		xocl_err(&xmc->pdev->dev, "content too big for key %d\n", key);
		return rval;
	}
	memcpy(&rval, p, len);
	return rval;
}

static ssize_t board_info_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	ssize_t cnt = 0;
	int key;

	mutex_lock(&xmc->mbx_lock);

	for (key = BDINFO_MIN_KEY; key <= BDINFO_MAX_KEY; key++) {
		ssize_t sz;

		if (key == BDINFO_MAX_PWR		||
			key == BDINFO_FAN_PRESENCE	||
			key == BDINFO_CONFIG_MODE) {
			sz = sprintf(buf, "%d=%llu\n", key,
				xmc_get_board_info_int(xmc, key)); 
		} else {
			const char *info = xmc_get_board_info(xmc, key, NULL);
			if (info == NULL)
				continue;
			sz = sprintf(buf, "%d=%s\n", key, info);
		}
		buf += sz;
		cnt += sz;
	}

	mutex_unlock(&xmc->mbx_lock);

	return cnt;
}
