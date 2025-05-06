/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
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

/* Retry is set to 15s for XMC and also for SC */
#define	MAX_XMC_RETRY			150
/* Retry is set to 1s for ERT */
#define	MAX_ERT_RETRY			10
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
#define	XMC_HBM_TEMP_REG		0x260
#define	XMC_VCC3V3_REG			0x26C
#define	XMC_3V3_PEX_I_REG		0x278
#define	XMC_VCC0V85_I_REG		0x284
#define	XMC_HBM_1V2_REG			0x290
#define	XMC_VPP2V5_REG			0x29C
#define	XMC_VCCINT_BRAM_REG		0x2A8
#define	XMC_HBM_TEMP2_REG		0x2B4
#define	XMC_12V_AUX1_REG                0x2C0
#define	XMC_VCCINT_TEMP_REG             0x2CC
#define	XMC_3V3_AUX_I_REG               0x2F0
#define	XMC_HEARTBEAT_REG               0x2FC
#define	XMC_HOST_MSG_OFFSET_REG		0x300
#define	XMC_HOST_MSG_ERROR_REG		0x304
#define	XMC_HOST_MSG_HEADER_REG		0x308
#define	XMC_STATUS2_REG			0x30C
#define	XMC_HEARTBEAT_ERR_CODE_REG	0x310
#define	XMC_VCC1V2_I_REG                0x314
#define	XMC_V12_IN_I_REG                0x320
#define	XMC_V12_IN_AUX0_I_REG           0x32C
#define	XMC_V12_IN_AUX1_I_REG           0x338
#define	XMC_VCCAUX_REG                  0x344
#define	XMC_VCCAUX_PMC_REG              0x350
#define	XMC_VCCRAM_REG                  0x35C
#define	XMC_POWER_WARN_REG              0x370
#define	XMC_VCCINT_VCU_0V9_REG          0x380
#define	XMC_HOST_NEW_FEATURE_REG1	0xB20
#define	XMC_HOST_NEW_FEATURE_REG1_SC_NO_CS (1 << 30)
#define	XMC_HOST_NEW_FEATURE_REG1_FEATURE_PRESENT (1 << 29)
#define	XMC_HOST_NEW_FEATURE_REG1_FEATURE_ENABLE (1 << 28)
#define	XMC_CLK_THROTTLING_PWR_MGMT_REG		 0xB24
#define	XMC_CLK_THROTTLING_PWR_MGMT_REG_OVRD_MASK 0xFF
#define	XMC_CLK_THROTTLING_PWR_MGMT_REG_PWR_OVRD_EN (1 << 31)
#define	XMC_CLK_THROTTLING_TEMP_MGMT_REG	 0xB28
#define	XMC_CLK_THROTTLING_TEMP_MGMT_REG_OVRD_MASK 0xFF
#define	XMC_CLK_THROTTLING_TEMP_MGMT_REG_TEMP_OVRD_EN (1 << 31)
#define	XMC_QSPI_STATUS_REG		0xC48
#define	XMC_CORE_VERSION_REG		0xC4C
#define	XMC_OEM_ID_REG                  0xC50
#define	XMC_HOST_POWER_THRESHOLD_BASE_REG	0xE68
#define	XMC_HOST_TEMP_THRESHOLD_BASE_REG	0xE90

//Clock scaling registers
#define	XMC_CLOCK_SCALING_CONTROL_REG		0x24
#define	XMC_CLOCK_SCALING_CONTROL_REG_EN	0x1
#define	XMC_CLOCK_SCALING_CONTROL_REG_EN_MASK	0x1
#define	XMC_CLOCK_SCALING_MODE_REG	0x10
#define	XMC_CLOCK_SCALING_MODE_POWER	0x0
#define	XMC_CLOCK_SCALING_MODE_TEMP	0x1
#define	XMC_CLOCK_SCALING_MODE_POWER_TEMP	0x2
#define	XMC_CLOCK_SCALING_POWER_REG	0x18
#define	XMC_CLOCK_SCALING_POWER_TARGET_MASK 0xFF
#define	XMC_CLOCK_SCALING_TEMP_REG	0x14
#define	XMC_CLOCK_SCALING_TEMP_TARGET_MASK	0xFF
#define	XMC_CLOCK_SCALING_THRESHOLD_REG		0x2C
#define	XMC_CLOCK_SCALING_TEMP_THRESHOLD_POS	0
#define	XMC_CLOCK_SCALING_TEMP_THRESHOLD_MASK	0xFF
#define	XMC_CLOCK_SCALING_POWER_THRESHOLD_POS	8
#define	XMC_CLOCK_SCALING_POWER_THRESHOLD_MASK	0xFF
#define	XMC_CLOCK_SCALING_CRIT_TEMP_THRESHOLD_REG	0x3C
#define	XMC_CLOCK_SCALING_CRIT_TEMP_THRESHOLD_REG_MASK	0xFF
#define	XMC_CLOCK_SCALING_CLOCK_STATUS_REG	0x38

//Sensor IDs
#define	SENSOR_12V_AUX0		0x03
#define	SENSOR_12VPEX_I_IN	0x0E
#define	SENSOR_AUX_12V_I_IN	0x0F
#define	SENSOR_VCCINT_I		0x11
#define	SENSOR_FPGA_TEMP	0x12
#define	SENSOR_3V3PEX_I_N	0x32
#define	SENSOR_VCCINT_TEMP	0x39
#define	SENSOR_PEX_12V_POWER	0x3A
#define	SENSOR_PEX_3V3_POWER	0x3B

#define	VALID_ID			0x74736574
#define	XMC_CORE_SUPPORT_NOTUPGRADABLE	0x0c010004
#define	XMC_CORE_SUPPORT_SENSOR_READY	0x0c010002
#define	XMC_CORE_SUPPORT_HEARTBEAT	0x0c01001B
#define	GPIO_RESET			0x0
#define	GPIO_ENABLED			0x1
#define	SENSOR_DATA_READY_MASK 		0x1

#define	SELF_JUMP(ins)			(((ins) & 0xfc00ffff) == 0xb8000000)
#define	XMC_PRIVILEGED(xmc)		((xmc)->base_addrs[0] != NULL)

#define	VALID_MAGIC(val) 		(val == VALID_ID)
#define	VALID_CMC_VERSION(val) 		((val & 0xff000000) == 0x0c000000)
#define	VALID_CORE_VERSION(val) 	((val & 0xff000000) == 0x0c000000)

#define	XMC_DEFAULT_EXPIRE_SECS	1

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
	IO_XMC_GPIO,
	IO_MUTEX,
	NUM_IOADDR
};

static struct xocl_iores_map res_map[] = {
	{ NODE_CMC_REG, IO_REG},
	{ NODE_CMC_RESET, IO_GPIO},
	{ NODE_CMC_FW_MEM, IO_IMAGE_MGMT},
	{ NODE_ERT_FW_MEM, IO_IMAGE_SCHED},
	{ NODE_ERT_CQ_MGMT, IO_CQ},
	{ NODE_CMC_CLK_SCALING_REG, IO_CLK_SCALING},
	{ NODE_CMC_MUTEX, IO_MUTEX},
};


enum sensor_val_kind {
	SENSOR_MAX,
	SENSOR_AVG,
	SENSOR_INS,
};

enum gpio_channel1_mask {
	MUTEX_GRANT_MASK	= 0x1,
};

enum gpio_channel2_mask {
	MUTEX_ACK_MASK		= 0x1,
	REGMAP_READY_MASK	= 0x2,
};

enum sc_mode {
	XMC_SC_UNKNOWN = 0,
	XMC_SC_NORMAL = 1,
	XMC_SC_BSL_MODE_UNSYNCED = 2,
	XMC_SC_BSL_MODE_SYNCED = 3,
	XMC_SC_BSL_MODE_SYNCED_SC_NOT_UPGRADABLE = 4,
	XMC_SC_NORMAL_MODE_SC_NOT_UPGRADABLE = 5
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

#define SCHED_EXIST(xmc)			\
	(xmc->base_addrs[IO_CQ] ? true : false)
#define	READ_CQ(xmc, off)			\
	(xmc->base_addrs[IO_CQ] ?		\
	XOCL_READ_REG32(xmc->base_addrs[IO_CQ] + off) : 0)
#define	WRITE_CQ(xmc, val, off)		\
	(xmc->base_addrs[IO_CQ] ?		\
	XOCL_WRITE_REG32(val, xmc->base_addrs[IO_CQ] + off) : ((void)0))

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

#define	READ_XMC_GPIO(xmc, off)		\
	(xmc->base_addrs[IO_XMC_GPIO] ?		\
	XOCL_READ_REG32(xmc->base_addrs[IO_XMC_GPIO] + off) : 0)
#define	WRITE_XMC_GPIO(xmc, val, off)	\
	(xmc->base_addrs[IO_XMC_GPIO] ?		\
	XOCL_WRITE_REG32(val, xmc->base_addrs[IO_XMC_GPIO] + off) : ((void)0))

#define	XMC_CTRL_ERR_CLR			(1 << 1)

#define	XMC_NO_MAILBOX_MASK			(1 << 3)
#define	XMC_PKT_OWNER_MASK			(1 << 5)
#define	XMC_PKT_ERR_MASK			(1 << 26)

enum xmc_error_code {
	XMC_HOST_MSG_NO_ERR			= 0x0,
	XMC_HOST_MSG_BAD_OPCODE_ERR		= 0x1,
	XMC_HOST_MSG_BRD_INFO_MISSING_ERR	= 0x2,
	XMC_HOST_MSG_LENGTH_ERR			= 0x3,
	XMC_HOST_MSG_SAT_FW_WRITE_FAIL		= 0x4,
	XMC_HOST_MSG_SAT_FW_UPDATE_FAIL		= 0x5,
	XMC_HOST_MSG_SAT_FW_LOAD_FAIL		= 0x6,
	XMC_HOST_MSG_SAT_FW_ERASE_FAIL		= 0x7,
	XMC_HOST_MSG_DR_CMD_FAIL		= 0x8,
	XMC_HOST_MSG_CSDR_FAILED		= 0x9,
	XMC_HOST_QSFP_FAIL			= 0xA,
};

struct xmc_heartbeat_err_code {
	u32 xhe_error		: 16;
	u32 xhe_last_sensor_id	: 8;
	u32 xhe_sensor_no	: 8;
};

enum xmc_xhe_error {
	XHE_NO_ERR = 0,
	XHE_SINGLE_ERR = 1,
	XHE_MULTI_ERR = 2,
};

enum xmc_packet_op {
	XPO_UNKNOWN 			= 0x0,
	XPO_MSP432_SEC_START 		= 0x1,
	XPO_MSP432_SEC_DATA 		= 0x2,
	XPO_MSP432_IMAGE_END 		= 0x3,
	XPO_BOARD_INFO			= 0x4,
	XPO_MSP432_ERASE_FW 		= 0x5,
	XPO_DR_FREEZE			= 0x6,
	XPO_DR_FREE			= 0x7,
	XPO_XCLBIN_DATA			= 0x8,
	XPO_FIRST_CSDR			= 0x9,
	XPO_ADDITIONAL_CSDR		= 0xA,
	XPO_QSFP_DIAG_READ		= 0xB,
	XPO_QSFP_CONTROL_WRITE		= 0xC,
	XPO_QSFP_LOW_SPEED_IO_READ	= 0xD,
	XPO_QSFP_LOW_SPEED_IO_WRITE	= 0xE,
	XPO_QSFP_I2C_READ		= 0xF,
	XPO_QSFP_I2C_WRITE		= 0x10,
};

/* Make sure hdr is multiple of u32 */
struct xmc_pkt_hdr {
	u32 payload_sz	: 12;
	u32 reserved	: 12;
	u32 op		: 8;
};

#define XMC_UNRECOVERABLE_ERR(val) \
		((val == XMC_HOST_MSG_SAT_FW_WRITE_FAIL)  || \
		 (val == XMC_HOST_MSG_SAT_FW_UPDATE_FAIL) || \
		 (val == XMC_HOST_MSG_SAT_FW_LOAD_FAIL)   || \
		 (val == XMC_HOST_MSG_SAT_FW_ERASE_FAIL))

/* We have a 4k buffer for xmc mailbox */
#define	XMC_PKT_MAX_SZ	1024 /* In u32 */
#define	XMC_PKT_MAX_PAYLOAD_SZ	\
	(XMC_PKT_MAX_SZ - sizeof(struct xmc_pkt_hdr) / sizeof(u32)) /* In u32 */
#define	XMC_PKT_SZ(hdr)		\
	((sizeof(struct xmc_pkt_hdr) + (hdr)->payload_sz + sizeof(u32) - 1) / \
	sizeof(u32)) /* In u32 */


#define XMC_BDINFO_ENTRY_LEN_MAX 	256
#define XMC_BDINFO_ENTRY_LEN		32
#define XMC_BDINFO_MAC_LEN		6

#define BDINFO_MAC_DYNAMIC              0x4B

struct xmc_pkt_image_end_op {
	u32 BSL_jump_addr;
};

struct xmc_pkt_sector_start_op {
	u32 addr;
	u32 size;
	u8 data[1];
};

struct xmc_pkt_sector_data_op {
	u8 data[1];
};

struct xmc_pkt_qsfp_diag_read_op {
	u32 port;
	u32 page;
	u32 level;
	u32 data_size;
	u8 data[1];
};

struct xmc_pkt_qsfp_io_rw_op {
	u32 port;
	u8 data[1];
};

struct xmc_pkt_qsfp_byte_rw_op {
	u32 port;
	u32 page;
	u32 level;
	u32 offset;
	u8 data[1];
};

struct xmc_pkt {
	struct xmc_pkt_hdr hdr;
	union {
		u32 data[XMC_PKT_MAX_PAYLOAD_SZ];
		struct xmc_pkt_image_end_op image_end;
		struct xmc_pkt_sector_start_op sector_start;
		struct xmc_pkt_sector_data_op sector_data;
		struct xmc_pkt_qsfp_diag_read_op qsfp_diag_r;
		struct xmc_pkt_qsfp_io_rw_op qsfp_io;
		struct xmc_pkt_qsfp_byte_rw_op qsfp_byte;
	};
};

/* QSFP related macros */
#define CMC_QSFP_PAGE_SIZE		128 /* In bytes */
#define CMC_OP_IO_CONTROL_MAXLEN	1

#define CMC_QSFP_DIAG_DATA_OFFSET \
	(sizeof(struct xmc_pkt_hdr) + offsetof(struct xmc_pkt_qsfp_diag_read_op, data))

#define CMC_QSFP_BYTE_DATA_OFFSET \
	(sizeof(struct xmc_pkt_hdr) + offsetof(struct xmc_pkt_qsfp_byte_rw_op, data))

#define CMC_QSFP_IO_DATA_OFFSET \
	(sizeof(struct xmc_pkt_hdr) + offsetof(struct xmc_pkt_qsfp_io_rw_op, data))

enum board_info_key {
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

struct xmc_status {
	u32 init_done		: 1;
	u32 mb_stopped		: 1;
	u32 reserved0		: 1;
	u32 watchdog_reset	: 1;
	u32 reserved1		: 6;
	u32 power_mode		: 2;
	u32 reserved2		: 12;
	u32 sc_comm_ver		: 4;
	u32 sc_mode		: 3;
	u32 invalid_sc		: 1;
};

struct xocl_xmc {
	struct platform_device	*pdev;
	void __iomem		*base_addrs[NUM_IOADDR];
	size_t			range[NUM_IOADDR];

	struct device		*hwmon_dev;
	bool			enabled;
	u32			state;
	struct mutex		xmc_lock;

	char			*sche_binary;
	u32			sche_binary_length;
	char			*mgmt_binary;
	u32			mgmt_binary_length;

	u64			cache_expire_secs;
	struct xcl_sensor	*cache;
	ktime_t			cache_expires;
	ktime_t			bdinfo_retry;
	u32			sc_presence;
	u32			heartbeat_count;
	u64			heartbeat_time;
	bool			heartbeat_stall;

	/* XMC mailbox support. */
	struct mutex		mbx_lock;
	bool			mbx_enabled;
	u32			mbx_offset;
	struct xmc_pkt		mbx_pkt;
	char			*bdinfo_raw;
	char			serial_num[XMC_BDINFO_ENTRY_LEN_MAX];
	char			mac_addr0[XMC_BDINFO_ENTRY_LEN];
	char			mac_addr1[XMC_BDINFO_ENTRY_LEN];
	char			mac_addr2[XMC_BDINFO_ENTRY_LEN];
	char			mac_addr3[XMC_BDINFO_ENTRY_LEN];
	char			revision[XMC_BDINFO_ENTRY_LEN_MAX];
	char			bd_name[XMC_BDINFO_ENTRY_LEN_MAX];
	char			bmc_ver[XMC_BDINFO_ENTRY_LEN_MAX];
	char			exp_bmc_ver[XMC_BDINFO_ENTRY_LEN_MAX];
	uint32_t		max_power;
	uint32_t		fan_presence;
	uint32_t		config_mode;
	bool			bdinfo_loaded;
	uint32_t		mac_contiguous_num;
	char			mac_addr_first[XMC_BDINFO_MAC_LEN];

	bool			sysfs_created;
	bool			mini_sysfs_created;

	bool			opened;
	bool			sc_fw_erased;
	struct xocl_xmc_privdata *priv_data;
};


static int load_xmc(struct xocl_xmc *xmc);
static int stop_xmc(struct platform_device *pdev);
static void xmc_clk_scale_config(struct platform_device *pdev);
static int xmc_load_board_info(struct xocl_xmc *xmc);
static int xmc_access(struct platform_device *pdev, enum xocl_xmc_flags flags);
static bool scaling_condition_check(struct xocl_xmc *xmc);
static const struct file_operations xmc_fops;
static void clock_status_check(struct platform_device *pdev, bool *latched);
static void xmc_get_serial_num(struct platform_device *pdev);

static ssize_t xmc_qsfp_diag_read(struct xocl_xmc *xmc, int port, char *buffer,
	loff_t off, size_t count);
static ssize_t xmc_qsfp_i2c_read(struct xocl_xmc *xmc, int port, char *buffer,
	loff_t off, size_t count);
static ssize_t xmc_qsfp_i2c_write(struct xocl_xmc *xmc, int port, char *buffer,
	loff_t off, size_t count);
static ssize_t xmc_qsfp_io_read(struct xocl_xmc *xmc, int port, char *buffer,
	loff_t off, size_t count);
static ssize_t xmc_qsfp_io_write(struct xocl_xmc *xmc, int port, char *buffer,
	loff_t off, size_t count);

static void set_sensors_data(struct xocl_xmc *xmc, struct xcl_sensor *sensors)
{
	memcpy(xmc->cache, sensors, sizeof(struct xcl_sensor));
	xmc->cache_expires = ktime_add(ktime_get_boottime(),
		ktime_set(xmc->cache_expire_secs, 0));
}

static void xmc_read_from_peer(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	struct xcl_sensor *xcl_sensor = NULL;
	size_t resp_len = sizeof(struct xcl_sensor);
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t reqlen = struct_size(mb_req, data, 1) + data_len;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	xocl_info(&pdev->dev, "reading from peer");
	mb_req = vmalloc(reqlen);
	if (!mb_req)
		goto done;

	xcl_sensor = vzalloc(resp_len);
	if (!xcl_sensor)
		goto done;

	mb_req->req = XCL_MAILBOX_REQ_PEER_DATA;
	subdev_peer.size = resp_len;
	subdev_peer.kind = XCL_SENSOR;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	(void) xocl_peer_request(xdev,
		mb_req, reqlen, xcl_sensor, &resp_len, NULL, NULL, 0, 0);
	set_sensors_data(xmc, xcl_sensor);

done:
	vfree(xcl_sensor);
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
	void *val, enum sensor_val_kind val_kind)
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
		case CUR_3V3_AUX:
			READ_SENSOR(xmc, XMC_3V3_AUX_I_REG, val, val_kind);
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
		case VOL_VCC_3V3:
			READ_SENSOR(xmc, XMC_VCC3V3_REG, val, val_kind);
			break;
		case CUR_3V3_PEX:
			READ_SENSOR(xmc, XMC_3V3_PEX_I_REG, val, val_kind);
			break;
		case CUR_VCC_0V85:
			READ_SENSOR(xmc, XMC_VCC0V85_I_REG, val, val_kind);
			break;
		case VOL_HBM_1V2:
			READ_SENSOR(xmc, XMC_HBM_1V2_REG, val, val_kind);
			break;
		case VOL_VPP_2V5:
			READ_SENSOR(xmc, XMC_VPP2V5_REG, val, val_kind);
			break;
		case VOL_VCCINT_BRAM:
			READ_SENSOR(xmc, XMC_VCCINT_BRAM_REG, val, val_kind);
			break;
		case XMC_VER:
			safe_read32(xmc, XMC_VERSION_REG, val);
			break;
		case XMC_OEM_ID:
			safe_read32(xmc, XMC_OEM_ID_REG, val);
			break;
		case XMC_VCCINT_TEMP:
			READ_SENSOR(xmc, XMC_VCCINT_TEMP_REG, val, val_kind);
			break;
		case XMC_12V_AUX1:
			READ_SENSOR(xmc, XMC_12V_AUX1_REG, val, val_kind);
			break;
		case XMC_VCC1V2_I:
			READ_SENSOR(xmc, XMC_VCC1V2_I_REG, val, val_kind);
			break;
		case XMC_V12_IN_I:
			READ_SENSOR(xmc, XMC_V12_IN_I_REG, val, val_kind);
			break;
		case XMC_V12_IN_AUX0_I:
			READ_SENSOR(xmc, XMC_V12_IN_AUX0_I_REG, val, val_kind);
			break;
		case XMC_V12_IN_AUX1_I:
			READ_SENSOR(xmc, XMC_V12_IN_AUX1_I_REG, val, val_kind);
			break;
		case XMC_VCCAUX:
			READ_SENSOR(xmc, XMC_VCCAUX_REG, val, val_kind);
			break;
		case XMC_VCCAUX_PMC:
			READ_SENSOR(xmc, XMC_VCCAUX_PMC_REG, val, val_kind);
			break;
		case XMC_VCCRAM:
			READ_SENSOR(xmc, XMC_VCCRAM_REG, val, val_kind);
			break;
		case XMC_POWER_WARN:
			safe_read32(xmc, XMC_POWER_WARN_REG, val);
			break;
		case XMC_QSPI_STATUS:
			safe_read32(xmc, XMC_QSPI_STATUS_REG, val);
			break;
		case XMC_HEARTBEAT_COUNT:
			safe_read32(xmc, XMC_HEARTBEAT_REG, val);
			break;
		case XMC_HEARTBEAT_ERR_TIME:
			*(u64 *)val = xmc->heartbeat_time;	
			break;
		case XMC_HEARTBEAT_ERR_CODE:
			safe_read32(xmc, XMC_HEARTBEAT_ERR_CODE_REG, val);
			break;
		case XMC_VCCINT_VCU_0V9:
			READ_SENSOR(xmc, XMC_VCCINT_VCU_0V9_REG, val, val_kind);
			break;
		default:
			break;
		}
	} else {
		safe_read_from_peer(xmc, pdev);

		switch (kind) {
		case DIMM0_TEMP:
			*(u32 *)val = xmc->cache->dimm_temp0;
			break;
		case DIMM1_TEMP:
			*(u32 *)val = xmc->cache->dimm_temp1;
			break;
		case DIMM2_TEMP:
			*(u32 *)val = xmc->cache->dimm_temp2;
			break;
		case DIMM3_TEMP:
			*(u32 *)val = xmc->cache->dimm_temp3;
			break;
		case FPGA_TEMP:
			*(u32 *)val = xmc->cache->fpga_temp;
			break;
		case VOL_12V_PEX:
			*(u32 *)val = xmc->cache->vol_12v_pex;
			break;
		case VOL_12V_AUX:
			*(u32 *)val = xmc->cache->vol_12v_aux;
			break;
		case CUR_12V_PEX:
			*(u32 *)val = xmc->cache->cur_12v_pex;
			break;
		case CUR_12V_AUX:
			*(u32 *)val = xmc->cache->cur_12v_aux;
			break;
		case SE98_TEMP0:
			*(u32 *)val = xmc->cache->se98_temp0;
			break;
		case SE98_TEMP1:
			*(u32 *)val = xmc->cache->se98_temp1;
			break;
		case SE98_TEMP2:
			*(u32 *)val = xmc->cache->se98_temp2;
			break;
		case FAN_TEMP:
			*(u32 *)val = xmc->cache->fan_temp;
			break;
		case FAN_RPM:
			*(u32 *)val = xmc->cache->fan_rpm;
			break;
		case VOL_3V3_PEX:
			*(u32 *)val = xmc->cache->vol_3v3_pex;
			break;
		case VOL_3V3_AUX:
			*(u32 *)val = xmc->cache->vol_3v3_aux;
			break;
		case CUR_3V3_AUX:
			*(u32 *)val = xmc->cache->cur_3v3_aux;
			break;
		case VPP_BTM:
			*(u32 *)val = xmc->cache->ddr_vpp_btm;
			break;
		case VPP_TOP:
			*(u32 *)val = xmc->cache->ddr_vpp_top;
			break;
		case VOL_5V5_SYS:
			*(u32 *)val = xmc->cache->sys_5v5;
			break;
		case VOL_1V2_TOP:
			*(u32 *)val = xmc->cache->top_1v2;
			break;
		case VOL_1V2_BTM:
			*(u32 *)val = xmc->cache->vcc1v2_btm;
			break;
		case VOL_1V8:
			*(u32 *)val = xmc->cache->vol_1v8;
			break;
		case VCC_0V9A:
			*(u32 *)val = xmc->cache->mgt0v9avcc;
			break;
		case VOL_12V_SW:
			*(u32 *)val = xmc->cache->vol_12v_sw;
			break;
		case VTT_MGTA:
			*(u32 *)val = xmc->cache->mgtavtt;
			break;
		case VOL_VCC_INT:
			*(u32 *)val = xmc->cache->vccint_vol;
			break;
		case CUR_VCC_INT:
			*(u32 *)val = xmc->cache->vccint_curr;
			break;
		case HBM_TEMP:
			*(u32 *)val = xmc->cache->hbm_temp0;
			break;
		case CAGE_TEMP0:
			*(u32 *)val = xmc->cache->cage_temp0;
			break;
		case CAGE_TEMP1:
			*(u32 *)val = xmc->cache->cage_temp1;
			break;
		case CAGE_TEMP2:
			*(u32 *)val = xmc->cache->cage_temp2;
			break;
		case CAGE_TEMP3:
			*(u32 *)val = xmc->cache->cage_temp3;
			break;
		case VCC_0V85:
			*(u32 *)val = xmc->cache->vol_0v85;
			break;
		case VOL_VCC_3V3:
			*(u32 *)val = xmc->cache->vol_3v3_vcc;
			break;
		case CUR_3V3_PEX:
			*(u32 *)val = xmc->cache->cur_3v3_pex;
			break;
		case CUR_VCC_0V85:
			*(u32 *)val = xmc->cache->cur_0v85;
			break;
		case VOL_HBM_1V2:
			*(u32 *)val = xmc->cache->vol_1v2_hbm;
			break;
		case VOL_VPP_2V5:
			*(u32 *)val = xmc->cache->vol_2v5_vpp;
			break;
		case VOL_VCCINT_BRAM:
			*(u32 *)val = xmc->cache->vccint_bram;
			break;
		case XMC_VER:
			*(u32 *)val = xmc->cache->version;
			break;
		case XMC_OEM_ID:
			*(u32 *)val = xmc->cache->oem_id;
			break;
		case XMC_VCCINT_TEMP:
			*(u32 *)val = xmc->cache->vccint_temp;
			break;
		case XMC_12V_AUX1:
			*(u32 *)val = xmc->cache->vol_12v_aux1;
			break;
		case XMC_VCC1V2_I:
			*(u32 *)val = xmc->cache->vol_vcc1v2_i;
			break;
		case XMC_V12_IN_I:
			*(u32 *)val = xmc->cache->vol_v12_in_i;
			break;
		case XMC_V12_IN_AUX0_I:
			*(u32 *)val = xmc->cache->vol_v12_in_aux0_i;
			break;
		case XMC_V12_IN_AUX1_I:
			*(u32 *)val = xmc->cache->vol_v12_in_aux1_i;
			break;
		case XMC_VCCAUX:
			*(u32 *)val = xmc->cache->vol_vccaux;
			break;
		case XMC_VCCAUX_PMC:
			*(u32 *)val = xmc->cache->vol_vccaux_pmc;
			break;
		case XMC_VCCRAM:
			*(u32 *)val = xmc->cache->vol_vccram;
			break;
		case XMC_POWER_WARN:
			*(u32 *)val = xmc->cache->power_warn;
			break;
		case XMC_QSPI_STATUS:
			*(u32 *)val = xmc->cache->qspi_status;
			break;
		case XMC_HEARTBEAT_COUNT:
			*(u32 *)val = xmc->cache->heartbeat_count;
			break;
		case XMC_HEARTBEAT_ERR_TIME:
			*(u64 *)val = xmc->cache->heartbeat_err_time;
			break;
		case XMC_HEARTBEAT_ERR_CODE:
			*(u32 *)val = xmc->cache->heartbeat_err_code;
			break;
		case XMC_VCCINT_VCU_0V9:
			*(u32 *)val = xmc->cache->vccint_vcu_0v9;
			break;
		default:
			break;
		}
	}
}

static void read_bdinfo_from_peer(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	size_t resp_len = sizeof(struct xcl_board_info);
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t reqlen = struct_size(mb_req, data, 1) + data_len;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int ret = 0;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	if (xmc->bdinfo_raw)
		return;

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		goto done;

	xmc->bdinfo_raw = vzalloc(resp_len);
	if (!xmc->bdinfo_raw)
		goto done;

	mb_req->req = XCL_MAILBOX_REQ_PEER_DATA;
	subdev_peer.size = resp_len;
	subdev_peer.kind = XCL_BDINFO;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	ret = xocl_peer_request(xdev,
		mb_req, reqlen, xmc->bdinfo_raw, &resp_len, NULL, NULL, 0, 0);
done:
	if (ret) {
		/* if we failed to get board info from peer, free it and 
		 * try to retrieve next time
		 */
		vfree(xmc->bdinfo_raw);
		xmc->bdinfo_raw = NULL;
	}
	vfree(mb_req);
}
static void xmc_bdinfo(struct platform_device *pdev, enum data_kind kind,
	u32 *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	struct xcl_board_info *bdinfo = NULL;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));
	if (XMC_PRIVILEGED(xmc)) {

		switch (kind) {
		case SER_NUM:
			memcpy(buf, xmc->serial_num, XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case MAC_ADDR0:
			memcpy(buf, xmc->mac_addr0, XMC_BDINFO_ENTRY_LEN);
			break;
		case MAC_ADDR1:
			memcpy(buf, xmc->mac_addr1, XMC_BDINFO_ENTRY_LEN);
			break;
		case MAC_ADDR2:
			memcpy(buf, xmc->mac_addr2, XMC_BDINFO_ENTRY_LEN);
			break;
		case MAC_ADDR3:
			memcpy(buf, xmc->mac_addr3, XMC_BDINFO_ENTRY_LEN);
			break;
		case REVISION:
			memcpy(buf, xmc->revision, XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case CARD_NAME:
			memcpy(buf, xmc->bd_name, XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case BMC_VER:
			memcpy(buf, xmc->bmc_ver, XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case MAX_PWR:
			*buf = xmc->max_power;
			break;
		case FAN_PRESENCE:
			*buf = xmc->fan_presence;
			break;
		case CFG_MODE:
			*buf = xmc->config_mode;
			break;
		case EXP_BMC_VER:
			memcpy(buf, xmc->exp_bmc_ver, XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case MAC_CONT_NUM:
			*buf = xmc->mac_contiguous_num;
			break;
		case MAC_ADDR_FIRST:
			memcpy(buf, xmc->mac_addr_first, XMC_BDINFO_MAC_LEN);
			break;
		default:
			break;
		}

	} else {
		if (!xmc->bdinfo_raw)
			return;

		bdinfo = (struct xcl_board_info *)xmc->bdinfo_raw;

		switch (kind) {
		case SER_NUM:
			memcpy(buf, bdinfo->serial_num,
				XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case MAC_ADDR0:
			memcpy(buf, bdinfo->mac_addr0, XMC_BDINFO_ENTRY_LEN);
			break;
		case MAC_ADDR1:
			memcpy(buf, bdinfo->mac_addr1, XMC_BDINFO_ENTRY_LEN);
			break;
		case MAC_ADDR2:
			memcpy(buf, bdinfo->mac_addr2, XMC_BDINFO_ENTRY_LEN);
			break;
		case MAC_ADDR3:
			memcpy(buf, bdinfo->mac_addr3, XMC_BDINFO_ENTRY_LEN);
			break;
		case REVISION:
			memcpy(buf, bdinfo->revision, XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case CARD_NAME:
			memcpy(buf, bdinfo->bd_name, XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case BMC_VER:
			memcpy(buf, bdinfo->bmc_ver, XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case MAX_PWR:
			*buf = bdinfo->max_power;
			break;
		case FAN_PRESENCE:
			*buf = bdinfo->fan_presence;
			break;
		case CFG_MODE:
			*buf = bdinfo->config_mode;
			break;
		case EXP_BMC_VER:
			memcpy(buf, bdinfo->exp_bmc_ver,
					XMC_BDINFO_ENTRY_LEN_MAX);
			break;
		case MAC_CONT_NUM:
			*buf = bdinfo->mac_contiguous_num;
			break;
		case MAC_ADDR_FIRST:
			memcpy(buf, bdinfo->mac_addr_first, XMC_BDINFO_MAC_LEN);
			break;
		default:
			break;
		}
	}
}

static bool xmc_clk_scale_on(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);

	if (xmc->priv_data && (xmc->priv_data->flags & XOCL_XMC_CLK_SCALING))
		return true;

	return false;
}

static bool nosc_xmc(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);

	if (xmc->priv_data && (xmc->priv_data->flags & XOCL_XMC_NOSC))
		return true;

	return false;
}

static bool xmc_in_bitfile(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);

	/* xmc in bitfile is supported only on SmartSSD U.2 */
	if (xmc->priv_data && (xmc->priv_data->flags & XOCL_XMC_IN_BITFILE))
		return true;

	return false;
}

static bool autonomous_xmc(struct platform_device *pdev)
{
	struct xocl_dev_core *core = xocl_get_xdev(pdev);

	return core->priv.flags & (XOCL_DSAFLAG_SMARTN | XOCL_DSAFLAG_VERSAL
			| XOCL_DSAFLAG_MPSOC);
}

static int xmc_get_data(struct platform_device *pdev, enum xcl_group_kind kind,
	void *buf)
{
	struct xcl_sensor *sensors = NULL;
	struct xcl_board_info *bdinfo = NULL;
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);

	if (XMC_PRIVILEGED(xmc) && !xmc->mgmt_binary && !autonomous_xmc(pdev)) {
		if (!xmc_in_bitfile(xmc->pdev))
			return -ENODEV;
	}

	switch (kind) {
	case XCL_SENSOR:
		sensors = (struct xcl_sensor *)buf;

		xmc_sensor(pdev, VOL_12V_PEX, &sensors->vol_12v_pex, SENSOR_INS);
		xmc_sensor(pdev, VOL_12V_AUX, &sensors->vol_12v_aux, SENSOR_INS);
		xmc_sensor(pdev, CUR_12V_PEX, &sensors->cur_12v_pex, SENSOR_INS);
		xmc_sensor(pdev, CUR_12V_AUX, &sensors->cur_12v_aux, SENSOR_INS);
		xmc_sensor(pdev, VOL_3V3_PEX, &sensors->vol_3v3_pex, SENSOR_INS);
		xmc_sensor(pdev, VOL_3V3_AUX, &sensors->vol_3v3_aux, SENSOR_INS);
		xmc_sensor(pdev, CUR_3V3_AUX, &sensors->cur_3v3_aux, SENSOR_INS);
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
		xmc_sensor(pdev, VOL_VCC_3V3, &sensors->vol_3v3_vcc, SENSOR_INS);
		xmc_sensor(pdev, CUR_3V3_PEX, &sensors->cur_3v3_pex, SENSOR_INS);
		xmc_sensor(pdev, CUR_VCC_0V85, &sensors->cur_0v85, SENSOR_INS);
		xmc_sensor(pdev, VOL_HBM_1V2, &sensors->vol_1v2_hbm, SENSOR_INS);
		xmc_sensor(pdev, VOL_VPP_2V5, &sensors->vol_2v5_vpp, SENSOR_INS);
		xmc_sensor(pdev, VOL_VCCINT_BRAM, &sensors->vccint_bram, SENSOR_INS);
		xmc_sensor(pdev, XMC_VER, &sensors->version, SENSOR_INS);
		xmc_sensor(pdev, XMC_OEM_ID, &sensors->oem_id, SENSOR_INS);
		xmc_sensor(pdev, XMC_VCCINT_TEMP, &sensors->vccint_temp, SENSOR_INS);
		xmc_sensor(pdev, XMC_12V_AUX1, &sensors->vol_12v_aux1, SENSOR_INS);
		xmc_sensor(pdev, XMC_VCC1V2_I, &sensors->vol_vcc1v2_i, SENSOR_INS);
		xmc_sensor(pdev, XMC_V12_IN_I, &sensors->vol_v12_in_i, SENSOR_INS);
		xmc_sensor(pdev, XMC_V12_IN_AUX0_I, &sensors->vol_v12_in_aux0_i, SENSOR_INS);
		xmc_sensor(pdev, XMC_V12_IN_AUX1_I, &sensors->vol_v12_in_aux1_i, SENSOR_INS);
		xmc_sensor(pdev, XMC_VCCAUX, &sensors->vol_vccaux, SENSOR_INS);
		xmc_sensor(pdev, XMC_VCCAUX_PMC, &sensors->vol_vccaux_pmc, SENSOR_INS);
		xmc_sensor(pdev, XMC_VCCRAM, &sensors->vol_vccram, SENSOR_INS);
		xmc_sensor(pdev, XMC_POWER_WARN, &sensors->power_warn, SENSOR_INS);
		xmc_sensor(pdev, XMC_QSPI_STATUS, &sensors->qspi_status, SENSOR_INS);
		xmc_sensor(pdev, XMC_HEARTBEAT_COUNT, &sensors->heartbeat_count, SENSOR_INS);
		xmc_sensor(pdev, XMC_HEARTBEAT_ERR_TIME, &sensors->heartbeat_err_time, SENSOR_INS);
		xmc_sensor(pdev, XMC_HEARTBEAT_ERR_CODE, &sensors->heartbeat_err_code, SENSOR_INS);
		xmc_sensor(pdev, XMC_VCCINT_VCU_0V9, &sensors->vccint_vcu_0v9, SENSOR_INS);
		break;
	case XCL_BDINFO:
		mutex_lock(&xmc->mbx_lock);
		xmc_load_board_info(xmc);

		bdinfo = (struct xcl_board_info *)buf;

		xmc_bdinfo(pdev, SER_NUM, (u32 *)bdinfo->serial_num);
		xmc_bdinfo(pdev, MAC_ADDR0, (u32 *)bdinfo->mac_addr0);
		xmc_bdinfo(pdev, MAC_ADDR1, (u32 *)bdinfo->mac_addr1);
		xmc_bdinfo(pdev, MAC_ADDR2, (u32 *)bdinfo->mac_addr2);
		xmc_bdinfo(pdev, MAC_ADDR3, (u32 *)bdinfo->mac_addr3);
		xmc_bdinfo(pdev, REVISION, (u32 *)bdinfo->revision);
		xmc_bdinfo(pdev, CARD_NAME, (u32 *)bdinfo->bd_name);
		xmc_bdinfo(pdev, BMC_VER, (u32 *)bdinfo->bmc_ver);
		xmc_bdinfo(pdev, MAX_PWR, &bdinfo->max_power);
		xmc_bdinfo(pdev, FAN_PRESENCE, &bdinfo->fan_presence);
		xmc_bdinfo(pdev, CFG_MODE, &bdinfo->config_mode);
		xmc_bdinfo(pdev, EXP_BMC_VER, (u32 *)bdinfo->exp_bmc_ver);
		xmc_bdinfo(pdev, MAC_CONT_NUM, &bdinfo->mac_contiguous_num);
		xmc_bdinfo(pdev, MAC_ADDR_FIRST, (u32 *)bdinfo->mac_addr_first);

	 	if (strcmp(bdinfo->bmc_ver, bdinfo->exp_bmc_ver)) {
			xocl_warn(&xmc->pdev->dev, "installed XSABIN has SC version: "
			    "(%s) mismatch with loaded SC version: (%s).",
			    bdinfo->exp_bmc_ver, bdinfo->bmc_ver);
		}

		mutex_unlock(&xmc->mbx_lock);
		break;
	default:
		break;
	}
	return 0;
}

static uint64_t xmc_get_power(struct platform_device *pdev,
                              enum sensor_val_kind kind, bool instantaneous)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	u32 v_pex, v_aux, v_3v3, c_pex, c_aux, c_3v3;
	u64 val = 0;

	if (instantaneous) {
		xmc_sensor(pdev, VOL_12V_PEX, &v_pex, kind);
		xmc_sensor(pdev, VOL_12V_AUX, &v_aux, kind);
		xmc_sensor(pdev, CUR_12V_PEX, &c_pex, kind);
		xmc_sensor(pdev, CUR_12V_AUX, &c_aux, kind);
		xmc_sensor(pdev, VOL_3V3_PEX, &v_3v3, kind);
		xmc_sensor(pdev, CUR_3V3_PEX, &c_3v3, kind);
	} else {
		v_pex = xmc->cache->vol_12v_pex;
		v_aux = xmc->cache->vol_12v_aux;
		c_pex = xmc->cache->cur_12v_pex;
		c_aux = xmc->cache->cur_12v_aux;
		v_3v3 = xmc->cache->vol_3v3_pex;
		c_3v3 = xmc->cache->cur_3v3_pex;
	}

	xocl_dbg(&xmc->pdev->dev, "v_pex %d, c_pex %d, v_aux %d, "
		"c_aux %d, v_3v3 %d, c_3v3 %d",
		v_pex, c_pex, v_aux, c_aux, v_3v3, c_3v3);

	val = (u64)v_pex * c_pex + (u64)v_aux * c_aux + (u64)v_3v3 * c_3v3;

	return val;
}

static u32 xmc_get_threshold_power(struct xocl_xmc *xmc)
{
	u32 base, max, cntrl;
	u32 c_12v_pex = 0, c_3v3_pex = 0, vccint_c = 0, c_12v_aux = 0;
	u32 v_pex, v_aux, v_3v3;
	u64 power, power_12v_pex;

	/* The thresholds are stored as [Sensor ID, throttle limit] pairs in the shared
	 * XRT/CMC memory map.
	 * Power Thresholds start at 0x0E68 and ends at 0xE8C. This range is
	 * fixed. But, offsets of sensor_id & it's throttle limit pair
	 * are not fixed in this range. Hence, read sensor_id first, and store it's
	 * throttle limit in it's corresponding pair.
	 */

	base = XMC_HOST_POWER_THRESHOLD_BASE_REG;
	max = XMC_HOST_POWER_THRESHOLD_BASE_REG + 14;
	while (base < max) {
		cntrl = READ_REG32(xmc, base);
		if (cntrl == SENSOR_12VPEX_I_IN)
			c_12v_pex = READ_REG32(xmc, base + 4);
		else if (cntrl == SENSOR_3V3PEX_I_N)
			c_3v3_pex = READ_REG32(xmc, base + 4);
		else if (cntrl == SENSOR_VCCINT_I)
			vccint_c = READ_REG32(xmc, base + 4);
		else if (cntrl == SENSOR_AUX_12V_I_IN)
			c_12v_aux = READ_REG32(xmc, base + 4);
		base = base + 8;
	}

	xmc_sensor(xmc->pdev, VOL_12V_PEX, &v_pex, SENSOR_MAX);
	xmc_sensor(xmc->pdev, VOL_12V_AUX, &v_aux, SENSOR_MAX);
	xmc_sensor(xmc->pdev, VOL_3V3_PEX, &v_3v3, SENSOR_MAX);

	//Throttling threshold is 12V_PEX power
	power_12v_pex = (u64)v_pex * c_12v_pex;
	power = power_12v_pex + (u64)v_aux * c_12v_aux + (u64)v_3v3 * c_3v3_pex;

	power_12v_pex = power_12v_pex / 1000000;
	power = power / 1000000;

	return power_12v_pex;
}

static u32 xmc_get_threshold_temp(struct xocl_xmc *xmc)
{
	u32 base, max, cntrl, fpga_temp = 0, vccint_temp = 0;

	base = XMC_HOST_TEMP_THRESHOLD_BASE_REG;
	max = XMC_HOST_TEMP_THRESHOLD_BASE_REG + 0xC;
	while (base < max) {
		cntrl = READ_REG32(xmc, base);
		if (cntrl == SENSOR_FPGA_TEMP)
			fpga_temp = READ_REG32(xmc, base + 4);
		else
			vccint_temp = READ_REG32(xmc, base + 4);
		base = base + 8;
	}

	return fpga_temp;
}

static void runtime_clk_scale_disable(struct xocl_xmc *xmc)
{
	u32 cntrl;
	bool cs_en;

	/* Check if clock scaling feature can be disabled */
	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return;

	cntrl = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_CONTROL_REG);
	cntrl &= ~XMC_CLOCK_SCALING_CONTROL_REG_EN_MASK;
	WRITE_RUNTIME_CS(xmc, cntrl, XMC_CLOCK_SCALING_CONTROL_REG);

	cntrl = READ_REG32(xmc, XMC_HOST_NEW_FEATURE_REG1);
	cntrl &= ~XMC_HOST_NEW_FEATURE_REG1_FEATURE_ENABLE;
	WRITE_REG32(xmc, cntrl, XMC_HOST_NEW_FEATURE_REG1);

	xocl_info(&xmc->pdev->dev, "Runtime clock scaling is disabled\n");
}

static void runtime_clk_scale_enable(struct xocl_xmc *xmc)
{
	u32 cntrl;
	bool cs_en;

	/* Check if clock scaling feature can be enabled */
	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return;

	cntrl = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_CONTROL_REG);
	cntrl |= XMC_CLOCK_SCALING_CONTROL_REG_EN;
	WRITE_RUNTIME_CS(xmc, cntrl, XMC_CLOCK_SCALING_CONTROL_REG);

	cntrl = READ_REG32(xmc, XMC_HOST_NEW_FEATURE_REG1);
	cntrl |= XMC_HOST_NEW_FEATURE_REG1_FEATURE_ENABLE;
	WRITE_REG32(xmc, cntrl, XMC_HOST_NEW_FEATURE_REG1);

	xocl_info(&xmc->pdev->dev, "Runtime clock scaling is enabled\n");
}

/*
 * Defining sysfs nodes for all sensor readings.
 */
#define	SENSOR_SYSFS_NODE_FORMAT(node_name, type, format)		\
	static ssize_t node_name##_show(struct device *dev,		\
		struct device_attribute *attr, char *buf)		\
	{								\
		struct xocl_xmc *xmc = dev_get_drvdata(dev);		\
		u32 val = 0;						\
		xmc_sensor(xmc->pdev, type, &val, SENSOR_INS);		\
		return sprintf(buf, format, val);			\
	}								\
	static DEVICE_ATTR_RO(node_name)
#define SENSOR_SYSFS_NODE(node_name, type)				\
	SENSOR_SYSFS_NODE_FORMAT(node_name, type, "%d\n")
SENSOR_SYSFS_NODE(xmc_12v_pex_vol, VOL_12V_PEX);
SENSOR_SYSFS_NODE(xmc_12v_aux_vol, VOL_12V_AUX);
SENSOR_SYSFS_NODE(xmc_12v_pex_curr, CUR_12V_PEX);
SENSOR_SYSFS_NODE(xmc_12v_aux_curr, CUR_12V_AUX);
SENSOR_SYSFS_NODE(xmc_3v3_pex_vol, VOL_3V3_PEX);
SENSOR_SYSFS_NODE(xmc_3v3_aux_vol, VOL_3V3_AUX);
SENSOR_SYSFS_NODE(xmc_3v3_aux_cur, CUR_3V3_AUX);
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
SENSOR_SYSFS_NODE(xmc_3v3_vcc_vol, VOL_VCC_3V3);
SENSOR_SYSFS_NODE(xmc_3v3_pex_curr, CUR_3V3_PEX);
SENSOR_SYSFS_NODE(xmc_0v85_curr, CUR_VCC_0V85);
SENSOR_SYSFS_NODE(xmc_hbm_1v2_vol, VOL_HBM_1V2);
SENSOR_SYSFS_NODE(xmc_vpp2v5_vol, VOL_VPP_2V5);
SENSOR_SYSFS_NODE(xmc_vccint_bram_vol, VOL_VCCINT_BRAM);
SENSOR_SYSFS_NODE(xmc_hbm_temp, HBM_TEMP);
SENSOR_SYSFS_NODE(version, XMC_VER);
SENSOR_SYSFS_NODE_FORMAT(xmc_oem_id, XMC_OEM_ID, "0x%x\n");
SENSOR_SYSFS_NODE(xmc_vccint_temp, XMC_VCCINT_TEMP);
SENSOR_SYSFS_NODE(xmc_12v_aux1, XMC_12V_AUX1);
SENSOR_SYSFS_NODE(xmc_vcc1v2_i, XMC_VCC1V2_I);
SENSOR_SYSFS_NODE(xmc_v12_in_i, XMC_V12_IN_I);
SENSOR_SYSFS_NODE(xmc_v12_in_aux0_i, XMC_V12_IN_AUX0_I);
SENSOR_SYSFS_NODE(xmc_v12_in_aux1_i, XMC_V12_IN_AUX1_I);
SENSOR_SYSFS_NODE(xmc_vccaux, XMC_VCCAUX);
SENSOR_SYSFS_NODE(xmc_vccaux_pmc, XMC_VCCAUX_PMC);
SENSOR_SYSFS_NODE(xmc_vccram, XMC_VCCRAM);
SENSOR_SYSFS_NODE(xmc_power_warn, XMC_POWER_WARN);
SENSOR_SYSFS_NODE(xmc_qspi_status, XMC_QSPI_STATUS);
SENSOR_SYSFS_NODE(xmc_heartbeat_count, XMC_HEARTBEAT_COUNT);
SENSOR_SYSFS_NODE_FORMAT(xmc_heartbeat_err_code, XMC_HEARTBEAT_ERR_CODE, "0x%x\n");
SENSOR_SYSFS_NODE(xmc_vccint_vcu_0v9, XMC_VCCINT_VCU_0V9);

static ssize_t xmc_heartbeat_err_time_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u64 val = 0;
	xmc_sensor(xmc->pdev, XMC_HEARTBEAT_ERR_TIME, &val, SENSOR_INS);
	return sprintf(buf, "%llu\n", val);
}
static DEVICE_ATTR_RO(xmc_heartbeat_err_time);

static ssize_t xmc_heartbeat_stall_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", xmc->heartbeat_stall);
}
static DEVICE_ATTR_RO(xmc_heartbeat_stall);

static ssize_t xmc_power_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u64 val = xmc_get_power(xmc->pdev, SENSOR_INS, false);

	return sprintf(buf, "%lld\n", val);
}
static DEVICE_ATTR_RO(xmc_power);

static ssize_t xmc_power_ins_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u64 val = xmc_get_power(xmc->pdev, SENSOR_INS, true);

	return sprintf(buf, "%lld\n", val);
}
static DEVICE_ATTR_RO(xmc_power_ins);

static ssize_t status_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val = 0;

	safe_read32(xmc, XMC_STATUS_REG, &val);

	return sprintf(buf, "0x%x\n", val);
}
static DEVICE_ATTR_RO(status);

static ssize_t core_version_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val = 0;

	safe_read32(xmc, XMC_CORE_VERSION_REG, &val);

	return sprintf(buf, "%u.%u.%u\n",
	    (val & 0xff0000) >> 16, (val & 0xff00) >> 8, (val & 0xff));
}
static DEVICE_ATTR_RO(core_version);

#define	SENSOR_SYSFS_NODE_ATTRS						\
	&dev_attr_xmc_12v_pex_vol.attr,					\
	&dev_attr_xmc_12v_aux_vol.attr,					\
	&dev_attr_xmc_12v_pex_curr.attr,				\
	&dev_attr_xmc_12v_aux_curr.attr,				\
	&dev_attr_xmc_3v3_pex_vol.attr,					\
	&dev_attr_xmc_3v3_aux_vol.attr,					\
	&dev_attr_xmc_3v3_aux_cur.attr,					\
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
	&dev_attr_xmc_cage_temp3.attr,					\
	&dev_attr_xmc_3v3_vcc_vol.attr,					\
	&dev_attr_xmc_3v3_pex_curr.attr,				\
	&dev_attr_xmc_0v85_curr.attr,					\
	&dev_attr_xmc_hbm_1v2_vol.attr,					\
	&dev_attr_xmc_vpp2v5_vol.attr,					\
	&dev_attr_xmc_vccint_bram_vol.attr,				\
	&dev_attr_xmc_hbm_temp.attr,					\
	&dev_attr_xmc_power.attr,					\
	&dev_attr_version.attr,						\
	&dev_attr_xmc_oem_id.attr,					\
	&dev_attr_xmc_vccint_temp.attr,					\
	&dev_attr_xmc_12v_aux1.attr,					\
	&dev_attr_xmc_vcc1v2_i.attr,					\
	&dev_attr_xmc_v12_in_i.attr,					\
	&dev_attr_xmc_v12_in_aux0_i.attr,				\
	&dev_attr_xmc_v12_in_aux1_i.attr,				\
	&dev_attr_xmc_vccaux.attr,					\
	&dev_attr_xmc_vccaux_pmc.attr,					\
	&dev_attr_xmc_vccram.attr,					\
	&dev_attr_xmc_power_warn.attr,					\
	&dev_attr_xmc_vccint_vcu_0v9.attr,					\
	&dev_attr_xmc_heartbeat_count.attr,				\
	&dev_attr_xmc_heartbeat_err_code.attr,				\
	&dev_attr_xmc_heartbeat_err_time.attr,				\
	&dev_attr_xmc_heartbeat_stall.attr,				\
	&dev_attr_xmc_qspi_status.attr,					\
	&dev_attr_xmc_power_ins.attr

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

REG_SYSFS_NODE(sensor, XMC_SENSOR_REG, "0x%04x\n");
REG_SYSFS_NODE(id, XMC_MAGIC_REG, "0x%x\n");
REG_SYSFS_NODE(error, XMC_ERROR_REG, "0x%x\n");
REG_SYSFS_NODE(capability, XMC_FEATURE_REG, "0x%x\n");
REG_SYSFS_NODE(host_msg_offset, XMC_HOST_MSG_OFFSET_REG, "%d\n");
REG_SYSFS_NODE(host_msg_error, XMC_HOST_MSG_ERROR_REG, "0x%x\n");
REG_SYSFS_NODE(host_msg_header, XMC_HOST_MSG_HEADER_REG, "0x%x\n");
#define	REG_SYSFS_NODE_ATTRS						\
	&dev_attr_sensor.attr,						\
	&dev_attr_id.attr,						\
	&dev_attr_error.attr,						\
	&dev_attr_capability.attr,					\
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
	if (!autonomous_xmc(xmc->pdev))
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
		mutex_unlock(&xmc->xmc_lock);
		return -EINVAL;
	}

	if (!XMC_PRIVILEGED(xmc))
		xmc->cache_expire_secs = val;

	mutex_unlock(&xmc->xmc_lock);
	return count;
}
static DEVICE_ATTR_RW(cache_expire_secs);

static ssize_t sensor_update_timestamp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u64 val = 0;

	mutex_lock(&xmc->xmc_lock);
	if (!XMC_PRIVILEGED(xmc))
		val = ktime_to_ms(xmc->cache_expires);

	mutex_unlock(&xmc->xmc_lock);
	return sprintf(buf, "%llu\n", val);
}
static DEVICE_ATTR_RO(sensor_update_timestamp);

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
static bool scaling_condition_check(struct xocl_xmc *xmc)
{
	u32 reg;
	bool cs_on_ptfm = false;
	bool sc_no_cs = false;

	if (!XMC_PRIVILEGED(xmc)) {
		xocl_dbg(&xmc->pdev->dev, "Runtime clock scaling is not supported in non privileged mode\n");
		return false;
	}

	if (!xmc->sc_presence) {
		if (xmc_clk_scale_on(xmc->pdev))
			cs_on_ptfm = true;
	} else {
		//Feature present bit may configured each time an xclbin is downloaded,
		//or following a reset of the CMC Subsystem. So, check for latest
		//status every time.
		reg = READ_REG32(xmc, XMC_HOST_NEW_FEATURE_REG1);
		if (reg & XMC_HOST_NEW_FEATURE_REG1_SC_NO_CS)
			sc_no_cs = true;
		if (reg & XMC_HOST_NEW_FEATURE_REG1_FEATURE_PRESENT)
			cs_on_ptfm = true;
	}

	if (sc_no_cs) {
		xocl_dbg(&xmc->pdev->dev, "Loaded SC fw does not support Runtime clock scalling, cs_on_ptfm: %d\n", cs_on_ptfm);
	} else if (cs_on_ptfm) {
		xocl_dbg(&xmc->pdev->dev, "Runtime clock scaling is supported\n");
		return true;
	} else {
		xocl_dbg(&xmc->pdev->dev, "Runtime clock scaling is not supported\n");
	}

	return false;
}

static bool is_scaling_enabled(struct xocl_xmc *xmc)
{
	u32 reg;

	if (!scaling_condition_check(xmc))
		return false;

	reg = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_CONTROL_REG);
	if (reg & XMC_CLOCK_SCALING_CONTROL_REG_EN)
		return true;

	reg = READ_REG32(xmc, XMC_HOST_NEW_FEATURE_REG1);
	if (reg & XMC_HOST_NEW_FEATURE_REG1_FEATURE_ENABLE)
		return true;

	return false;
}

static bool is_heartbeat_supported(struct xocl_xmc *xmc)
{
	u32 xmc_core_version;

	safe_read32(xmc, XMC_CORE_VERSION_REG, &xmc_core_version);
	return xmc_core_version >= XMC_CORE_SUPPORT_HEARTBEAT;
}

static bool is_sc_fixed(struct xocl_xmc *xmc)
{
	struct xmc_status status;
	u32 xmc_core_version;

	safe_read32(xmc, XMC_CORE_VERSION_REG, &xmc_core_version);
	safe_read32(xmc, XMC_STATUS_REG, (u32 *)&status);

	if (xmc_core_version >= XMC_CORE_SUPPORT_NOTUPGRADABLE &&
	    !status.invalid_sc &&
	    (status.sc_mode == XMC_SC_BSL_MODE_SYNCED_SC_NOT_UPGRADABLE ||
	     status.sc_mode == XMC_SC_NORMAL_MODE_SC_NOT_UPGRADABLE))
		return true;

	return false;
}

static ssize_t scaling_reset_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 buf_val = 0, target, threshold;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return count;

	if (kstrtou32(buf, 10, &buf_val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xmc->xmc_lock);
	//Reset target power settings to default values
	threshold = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
	threshold = (threshold >> XMC_CLOCK_SCALING_POWER_THRESHOLD_POS) &
		XMC_CLOCK_SCALING_POWER_THRESHOLD_MASK;
	target = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_POWER_REG);
	target &= ~XMC_CLOCK_SCALING_POWER_TARGET_MASK;
	target |= (threshold & XMC_CLOCK_SCALING_POWER_TARGET_MASK);
	WRITE_RUNTIME_CS(xmc, target, XMC_CLOCK_SCALING_POWER_REG);

	//Reset target temp settings to default values
	threshold = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
	threshold = (threshold >> XMC_CLOCK_SCALING_TEMP_THRESHOLD_POS) &
		XMC_CLOCK_SCALING_TEMP_THRESHOLD_MASK;
	target = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_TEMP_REG);
	target &= ~XMC_CLOCK_SCALING_TEMP_TARGET_MASK;
	target |= (threshold & XMC_CLOCK_SCALING_TEMP_TARGET_MASK);
	WRITE_RUNTIME_CS(xmc, target, XMC_CLOCK_SCALING_TEMP_REG);

	//Reset power & temp thresold override settings to default values
	target = READ_REG32(xmc, XMC_HOST_NEW_FEATURE_REG1);
	if (target & XMC_HOST_NEW_FEATURE_REG1_FEATURE_PRESENT) {
		WRITE_REG32(xmc, 0x0, XMC_CLK_THROTTLING_PWR_MGMT_REG);
		WRITE_REG32(xmc, 0x0, XMC_CLK_THROTTLING_TEMP_MGMT_REG);
	}

	mutex_unlock(&xmc->xmc_lock);

	return count;
}
static DEVICE_ATTR_WO(scaling_reset);

static ssize_t scaling_threshold_power_override_en_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	mutex_lock(&xmc->xmc_lock);
	if (!xmc->sc_presence) {
		val = 1;
	} else {
		val = READ_REG32(xmc, XMC_CLK_THROTTLING_PWR_MGMT_REG);
		val = (val >> 31) & 0x1;
	}
	mutex_unlock(&xmc->xmc_lock);

	return sprintf(buf, "%u\n", val);
}
static DEVICE_ATTR_RO(scaling_threshold_power_override_en);

static ssize_t scaling_threshold_power_override_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	mutex_lock(&xmc->xmc_lock);
	if (!xmc->sc_presence) {
		val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_POWER_REG);
		val &= XMC_CLOCK_SCALING_POWER_TARGET_MASK;
	} else {
		val = READ_REG32(xmc, XMC_CLK_THROTTLING_PWR_MGMT_REG);
		val = val & XMC_CLK_THROTTLING_PWR_MGMT_REG_OVRD_MASK;
	}
	mutex_unlock(&xmc->xmc_lock);

	return sprintf(buf, "%u\n", val);
}

static ssize_t scaling_threshold_power_override_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val, val2, val3, val4;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return count;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xmc->xmc_lock);
	if (!xmc->sc_presence) {
		val2 = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
		val2 = (val2 >> XMC_CLOCK_SCALING_POWER_THRESHOLD_POS) &
			XMC_CLOCK_SCALING_POWER_THRESHOLD_MASK;
		if (val < val2) {
			val3 = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_POWER_REG);
			val3 &= ~XMC_CLOCK_SCALING_POWER_TARGET_MASK;
			val3 |= (val & XMC_CLOCK_SCALING_POWER_TARGET_MASK);
			WRITE_RUNTIME_CS(xmc, val3, XMC_CLOCK_SCALING_POWER_REG);
			xocl_info(dev, "New power threshold value is = %d W", val);
		} else {
			xocl_info(dev, "Unable to set new power threshold value since value is > %d W", val2);
		}
	} else {
		val2 = READ_REG32(xmc, XMC_CLK_THROTTLING_PWR_MGMT_REG);
		val2 &= ~XMC_CLK_THROTTLING_PWR_MGMT_REG_OVRD_MASK;
		val4 = READ_REG32(xmc, XMC_HOST_NEW_FEATURE_REG1);
		if (val4 & XMC_HOST_NEW_FEATURE_REG1_FEATURE_PRESENT) {
			if (val > 0) { //enable max power override mode
				val2 |= XMC_CLK_THROTTLING_PWR_MGMT_REG_PWR_OVRD_EN;
				val &= XMC_CLK_THROTTLING_PWR_MGMT_REG_OVRD_MASK;
				val2 |= val;
			} else { //disable max power override mode
				val2 &= ~XMC_CLK_THROTTLING_PWR_MGMT_REG_PWR_OVRD_EN;
			}
			WRITE_REG32(xmc, val2, XMC_CLK_THROTTLING_PWR_MGMT_REG);
		}
	}
	mutex_unlock(&xmc->xmc_lock);

	return count;
}
static DEVICE_ATTR_RW(scaling_threshold_power_override);

static ssize_t scaling_critical_power_threshold_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	if (!xmc->sc_presence) {
		//no power threshold defined for clock shutdown
		return sprintf(buf, "N/A\n");
	}

	//no provision given to xrt to retrieve this data on alveo cards
	return sprintf(buf, "N/A\n");
}
static DEVICE_ATTR_RO(scaling_critical_power_threshold);

static ssize_t scaling_critical_temp_threshold_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	if (!xmc->sc_presence) {
		val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_CRIT_TEMP_THRESHOLD_REG);
		val = val & XMC_CLOCK_SCALING_CRIT_TEMP_THRESHOLD_REG_MASK;
	} else {
		//no provision given to xrt to retrieve this data on alveo cards
		return sprintf(buf, "N/A\n");
	}

	return sprintf(buf, "%u\n", val);
}
static DEVICE_ATTR_RO(scaling_critical_temp_threshold);

static ssize_t scaling_threshold_temp_limit_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	if (!xmc->sc_presence) {
		val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
		val = (val >> XMC_CLOCK_SCALING_TEMP_THRESHOLD_POS) &
			XMC_CLOCK_SCALING_TEMP_THRESHOLD_MASK;
	} else {
		val = xmc_get_threshold_temp(xmc);
	}

	return sprintf(buf, "%u\n", val);
}
static DEVICE_ATTR_RO(scaling_threshold_temp_limit);

static ssize_t scaling_threshold_power_limit_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	if (!xmc->sc_presence) {
		val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
		val = (val >> XMC_CLOCK_SCALING_POWER_THRESHOLD_POS) &
			XMC_CLOCK_SCALING_POWER_THRESHOLD_MASK;
	} else {
		val = xmc_get_threshold_power(xmc);
	}

	return sprintf(buf, "%u\n", val);
}
static DEVICE_ATTR_RO(scaling_threshold_power_limit);

static ssize_t scaling_threshold_temp_override_en_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	if (!xmc->sc_presence) {
		val = 1;
	} else {
		val = READ_REG32(xmc, XMC_CLK_THROTTLING_TEMP_MGMT_REG);
		val = (val >> 31) & 0x1;
	}

	return sprintf(buf, "%u\n", val);
}
static DEVICE_ATTR_RO(scaling_threshold_temp_override_en);

static ssize_t scaling_threshold_temp_override_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	mutex_lock(&xmc->xmc_lock);
	if (!xmc->sc_presence) {
		val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_TEMP_REG);
		val &= XMC_CLOCK_SCALING_TEMP_TARGET_MASK;
	} else {
		val = READ_REG32(xmc, XMC_CLK_THROTTLING_TEMP_MGMT_REG);
		val &= XMC_CLK_THROTTLING_TEMP_MGMT_REG_OVRD_MASK;
	}
	mutex_unlock(&xmc->xmc_lock);

	return sprintf(buf, "%u\n", val);
}

static ssize_t scaling_threshold_temp_override_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val, val2, val3, val4;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return count;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xmc->xmc_lock);
	if (!xmc->sc_presence) {
		val2 = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
		val2 = (val2 >> XMC_CLOCK_SCALING_TEMP_THRESHOLD_POS) &
			XMC_CLOCK_SCALING_TEMP_THRESHOLD_MASK;
		if (val <= val2) {
			val3 = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_TEMP_REG);
			val3 &= ~XMC_CLOCK_SCALING_TEMP_TARGET_MASK;
			val3 |= (val & XMC_CLOCK_SCALING_TEMP_TARGET_MASK);
			WRITE_RUNTIME_CS(xmc, val3, XMC_CLOCK_SCALING_TEMP_REG);
			xocl_info(dev, "New temp threshold value is = %d dC", val);
		} else{
			xocl_info(dev, "Unable to set new temp threshold value since value is >= %d dC", val2);
		}
	} else {
		val2 = READ_REG32(xmc, XMC_CLK_THROTTLING_TEMP_MGMT_REG);
		val2 &= ~XMC_CLK_THROTTLING_TEMP_MGMT_REG_OVRD_MASK;
		val4 = READ_REG32(xmc, XMC_HOST_NEW_FEATURE_REG1);
		if (val4 & XMC_HOST_NEW_FEATURE_REG1_FEATURE_PRESENT) {
			if (val > 0) { //enable max temp override mode
				val2 |= XMC_CLK_THROTTLING_TEMP_MGMT_REG_TEMP_OVRD_EN;
				val &= XMC_CLK_THROTTLING_TEMP_MGMT_REG_OVRD_MASK;
				val2 |= val;
			} else { //disable max temp override mode
				val2 &= ~XMC_CLK_THROTTLING_TEMP_MGMT_REG_TEMP_OVRD_EN;
			}
			WRITE_REG32(xmc, val2, XMC_CLK_THROTTLING_TEMP_MGMT_REG);
		}
	}
	mutex_unlock(&xmc->xmc_lock);

	return count;
}
static DEVICE_ATTR_RW(scaling_threshold_temp_override);

static ssize_t scaling_governor_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 mode;
	char val[20] = { 0 };
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en) {
		strcpy(val, "NULL");
		return sprintf(buf, "%s\n", val);
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
	case 2:
		strcpy(val, "power_temp");
		break;
	}

	return sprintf(buf, "%s\n", val);
}

static ssize_t scaling_governor_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val;
	bool cs_en;

	/* Check if clock scaling feature enabled */
	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return count;

	if (strncmp(buf, "power", strlen("power")) == 0)
		val = XMC_CLOCK_SCALING_MODE_POWER;
	else if (strncmp(buf, "temp", strlen("temp")) == 0)
		val = XMC_CLOCK_SCALING_MODE_TEMP;
	else if (strncmp(buf, "power_temp", strlen("power_temp")) == 0)
		val = XMC_CLOCK_SCALING_MODE_POWER_TEMP;
	else {
		xocl_err(dev, "valid modes [power, temp, power_temp]\n");
		return -EINVAL;
	}

	mutex_lock(&xmc->xmc_lock);
	WRITE_RUNTIME_CS(xmc, val, XMC_CLOCK_SCALING_MODE_REG);
	mutex_unlock(&xmc->xmc_lock);

	return count;
}
static DEVICE_ATTR_RW(scaling_governor);

static ssize_t sc_presence_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", xmc->sc_presence);
}
static DEVICE_ATTR_RO(sc_presence);

static ssize_t sc_is_fixed_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", is_sc_fixed(xmc));
}
static DEVICE_ATTR_RO(sc_is_fixed);

static ssize_t scaling_enabled_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));

	if (strncmp(buf, "true", strlen("true")) == 0)
		runtime_clk_scale_enable(xmc);
	else
		runtime_clk_scale_disable(xmc);

	return count;
}

static ssize_t scaling_enabled_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", is_scaling_enabled(xmc));
}
static DEVICE_ATTR_RW(scaling_enabled);

static ssize_t scaling_support_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", scaling_condition_check(xmc));
}
static DEVICE_ATTR_RO(scaling_support);

static ssize_t hwmon_scaling_target_power_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	mutex_lock(&xmc->xmc_lock);
	val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_POWER_REG);
	val &= XMC_CLOCK_SCALING_POWER_TARGET_MASK;
	val = val * 1000000;
	mutex_unlock(&xmc->xmc_lock);

	return sprintf(buf, "%u\n", val);
}

static ssize_t hwmon_scaling_target_power_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val, val2, threshold;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return count;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	val = val / 1000000;

	mutex_lock(&xmc->xmc_lock);
	val2 = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_POWER_REG);
	threshold = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
	threshold = (threshold >> XMC_CLOCK_SCALING_POWER_THRESHOLD_POS) &
		XMC_CLOCK_SCALING_POWER_THRESHOLD_MASK;

	//Check if the threshold power is in board spec limits.
	if (val > threshold) {
		mutex_unlock(&xmc->xmc_lock);
		return -EINVAL;
	}

	val2 &= ~XMC_CLOCK_SCALING_POWER_TARGET_MASK;
	val2 |= (val & XMC_CLOCK_SCALING_POWER_TARGET_MASK);
	WRITE_RUNTIME_CS(xmc, val2, XMC_CLOCK_SCALING_POWER_REG);
	mutex_unlock(&xmc->xmc_lock);

	return count;
}

static ssize_t hwmon_scaling_target_temp_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val = 0;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	mutex_lock(&xmc->xmc_lock);
	val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_TEMP_REG);
	val &= XMC_CLOCK_SCALING_TEMP_TARGET_MASK;
	val = val * 1000;
	mutex_unlock(&xmc->xmc_lock);

	return sprintf(buf, "%u\n", val);
}

static ssize_t hwmon_scaling_target_temp_store(struct device *dev,
		struct device_attribute *da, const char *buf, size_t count)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	u32 val, val2, threshold;
	bool cs_en;

	/* Check if clock scaling feature enabled */
	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return count;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&xmc->xmc_lock);
	val2 = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_TEMP_REG);
	threshold = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
	threshold = (threshold >> XMC_CLOCK_SCALING_TEMP_THRESHOLD_POS) &
		XMC_CLOCK_SCALING_TEMP_THRESHOLD_MASK;

	//Check if the threshold temperature is in board spec limits.
	if (val > threshold) {
		mutex_unlock(&xmc->xmc_lock);
		return -EINVAL;
	}

	val2 &= ~XMC_CLOCK_SCALING_TEMP_TARGET_MASK;
	val2 |= (val & XMC_CLOCK_SCALING_TEMP_TARGET_MASK);
	WRITE_RUNTIME_CS(xmc, val2, XMC_CLOCK_SCALING_TEMP_REG);
	mutex_unlock(&xmc->xmc_lock);

	return count;
}

static ssize_t hwmon_scaling_threshold_temp_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val = 0, val2;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	mutex_lock(&xmc->xmc_lock);
	if (!xmc->sc_presence) {
		val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
		val = (val >> XMC_CLOCK_SCALING_TEMP_THRESHOLD_POS) &
			XMC_CLOCK_SCALING_TEMP_THRESHOLD_MASK;
	} else {
		val2 = READ_REG32(xmc, XMC_CLK_THROTTLING_TEMP_MGMT_REG);
		if (val2 & XMC_CLK_THROTTLING_TEMP_MGMT_REG_TEMP_OVRD_EN) {
			val = val2 & XMC_CLK_THROTTLING_TEMP_MGMT_REG_OVRD_MASK;
		}
	}
	val = val * 1000;
	mutex_unlock(&xmc->xmc_lock);

	return sprintf(buf, "%u\n", val);
}

static ssize_t hwmon_scaling_threshold_power_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct xocl_xmc *xmc = dev_get_drvdata(dev);
	u32 val = 0, val2;
	bool cs_en;

	cs_en = scaling_condition_check(xmc);
	if (!cs_en)
		return sprintf(buf, "%d\n", val);

	mutex_lock(&xmc->xmc_lock);
	if (!xmc->sc_presence) {
		val = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_THRESHOLD_REG);
		val = (val >> XMC_CLOCK_SCALING_POWER_THRESHOLD_POS) &
			XMC_CLOCK_SCALING_POWER_THRESHOLD_MASK;
	} else {
		val2 = READ_REG32(xmc, XMC_CLK_THROTTLING_PWR_MGMT_REG);
		if (val2 & XMC_CLK_THROTTLING_PWR_MGMT_REG_PWR_OVRD_EN) {
			val = val2 & XMC_CLK_THROTTLING_PWR_MGMT_REG_OVRD_MASK;
		}
	}
	val = val * 1000000;
	mutex_unlock(&xmc->xmc_lock);

	return sprintf(buf, "%u\n", val);
}

static ssize_t reg_base_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_xmc *xmc = platform_get_drvdata(to_platform_device(dev));
	xdev_handle_t xdev = xocl_get_xdev(xmc->pdev);
	struct resource *res;
	int ret, bar_idx;
	resource_size_t bar_off;

	res = platform_get_resource(to_platform_device(dev), IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	ret = xocl_ioaddr_to_baroff(xdev, res->start, &bar_idx, &bar_off);
	if (ret)
		return ret;

	return sprintf(buf, "%lld\n", bar_off);
}
static DEVICE_ATTR_RO(reg_base);

#define	XMC_BDINFO_STRING_SYSFS_NODE(name)				\
	static ssize_t name##_show(struct device *dev,			\
		struct device_attribute *attr, char *buf) {		\
		struct xocl_xmc *xmc =					\
			platform_get_drvdata(to_platform_device(dev));	\
		mutex_lock(&xmc->mbx_lock);				\
		xmc_load_board_info(xmc);				\
		mutex_unlock(&xmc->mbx_lock);				\
		return sprintf(buf, "%s\n", xmc->name);			\
	}								\
	static DEVICE_ATTR_RO(name);					\

XMC_BDINFO_STRING_SYSFS_NODE(serial_num)
XMC_BDINFO_STRING_SYSFS_NODE(mac_addr0)
XMC_BDINFO_STRING_SYSFS_NODE(mac_addr1)
XMC_BDINFO_STRING_SYSFS_NODE(mac_addr2)
XMC_BDINFO_STRING_SYSFS_NODE(mac_addr3)
XMC_BDINFO_STRING_SYSFS_NODE(revision)
XMC_BDINFO_STRING_SYSFS_NODE(bd_name)
XMC_BDINFO_STRING_SYSFS_NODE(bmc_ver)
XMC_BDINFO_STRING_SYSFS_NODE(exp_bmc_ver)

static ssize_t mac_addr_first_show(struct device *dev,
	struct device_attribute *attr, char *buf) {
	struct xocl_xmc *xmc =
		platform_get_drvdata(to_platform_device(dev));
	mutex_lock(&xmc->mbx_lock);
	xmc_load_board_info(xmc);
	mutex_unlock(&xmc->mbx_lock);
	return sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X\n",
		(u8)xmc->mac_addr_first[0],
		(u8)xmc->mac_addr_first[1],
		(u8)xmc->mac_addr_first[2],
		(u8)xmc->mac_addr_first[3],
		(u8)xmc->mac_addr_first[4],
		(u8)xmc->mac_addr_first[5]);
}
static DEVICE_ATTR_RO(mac_addr_first);

#define	XMC_BDINFO_STAT_SYSFS_NODE(name)				\
	static ssize_t name##_show(struct device *dev,			\
		struct device_attribute *attr, char *buf) {		\
		struct xocl_xmc *xmc =					\
			platform_get_drvdata(to_platform_device(dev));	\
		mutex_lock(&xmc->mbx_lock);				\
		xmc_load_board_info(xmc);				\
		mutex_unlock(&xmc->mbx_lock);				\
		return sprintf(buf, "%d\n", xmc->name);			\
	}								\
	static DEVICE_ATTR_RO(name);					\

XMC_BDINFO_STAT_SYSFS_NODE(max_power);
XMC_BDINFO_STAT_SYSFS_NODE(config_mode);
XMC_BDINFO_STAT_SYSFS_NODE(mac_contiguous_num);

#define	XMC_BDINFO_CHAR_SYSFS_NODE(name)				\
	static ssize_t name##_show(struct device *dev,			\
		struct device_attribute *attr, char *buf) {		\
		struct xocl_xmc *xmc =					\
			platform_get_drvdata(to_platform_device(dev));	\
		mutex_lock(&xmc->mbx_lock);				\
		xmc_load_board_info(xmc);				\
		mutex_unlock(&xmc->mbx_lock);				\
		return sprintf(buf, "%c\n", xmc->name);			\
	}								\
	static DEVICE_ATTR_RO(name);					\

XMC_BDINFO_CHAR_SYSFS_NODE(fan_presence);

static struct attribute *xmc_attrs[] = {
	&dev_attr_pause.attr,
	&dev_attr_reset.attr,
	&dev_attr_cache_expire_secs.attr,
	&dev_attr_scaling_enabled.attr,
	&dev_attr_scaling_governor.attr,
	&dev_attr_serial_num.attr,
	&dev_attr_mac_addr0.attr,
	&dev_attr_mac_addr1.attr,
	&dev_attr_mac_addr2.attr,
	&dev_attr_mac_addr3.attr,
	&dev_attr_revision.attr,
	&dev_attr_bd_name.attr,
	&dev_attr_bmc_ver.attr,
	&dev_attr_exp_bmc_ver.attr,
	&dev_attr_max_power.attr,
	&dev_attr_fan_presence.attr,
	&dev_attr_config_mode.attr,
	&dev_attr_sensor_update_timestamp.attr,
	&dev_attr_scaling_threshold_power_override.attr,
	&dev_attr_scaling_threshold_power_override_en.attr,
	&dev_attr_scaling_reset.attr,
	&dev_attr_scaling_threshold_temp_override.attr,
	&dev_attr_scaling_threshold_temp_override_en.attr,
	&dev_attr_scaling_support.attr,
	&dev_attr_scaling_threshold_temp_limit.attr,
	&dev_attr_scaling_threshold_power_limit.attr,
	&dev_attr_scaling_critical_temp_threshold.attr,
	&dev_attr_scaling_critical_power_threshold.attr,
	&dev_attr_mac_contiguous_num.attr,
	&dev_attr_mac_addr_first.attr,
	SENSOR_SYSFS_NODE_ATTRS,
	REG_SYSFS_NODE_ATTRS,
	NULL,
};

static struct attribute *xmc_mini_attrs[] = {
	&dev_attr_reg_base.attr,
	&dev_attr_status.attr,
	&dev_attr_sc_presence.attr,
	&dev_attr_sc_is_fixed.attr,
	&dev_attr_core_version.attr,
	NULL,
};

static ssize_t read_temp_by_mem_topology(struct file *filp,
	struct kobject *kobj, struct bin_attribute *attr, char *buffer,
	loff_t offset, size_t count)
{
	u32 nread = 0;
	size_t size = 0;
	int ret = 0;
	u32 i;
	struct mem_topology *memtopo = NULL;
	struct xocl_xmc *xmc =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	uint32_t *temp = NULL;
	xdev_handle_t xdev = xocl_get_xdev(xmc->pdev);
	struct xocl_drm *drm = XDEV(xdev)->drm;
	uint32_t slot_id = DEFAULT_PL_PS_SLOT;

        if (!drm)
                return 0;

	ret  = XOCL_GET_MEM_TOPOLOGY(xdev, memtopo, slot_id);
	if (ret)
                return ret;

	if (!memtopo)
		goto done;

	size = sizeof(u32)*(memtopo->m_count);

	if (offset >= size)
		goto done;

	temp = vzalloc(size);
	if (!temp)
		goto done;

	for (i = 0; i < memtopo->m_count; ++i)
		*(temp+i) = get_temp_by_m_tag(xmc, memtopo->m_mem_data[i].m_tag);

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, temp, nread);
done:
	XOCL_PUT_MEM_TOPOLOGY(xdev, slot_id);
	vfree(temp);
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

/*
 * preprocessor magic for qsfp name pattern:
 * The following sysfs node will be created.
 *
 * qsfp0_diag			(read only)
 * qsfp0_i2c			(read, write)
 * qsfp0_io_config		(read, write)
 * ...
 * qsfp3_diag			(read only)
 * qsfp3_i2c			(read, write)
 * qsfp3_io_config		(read, write)
 */

#define QSFP_DIAG_READ(PORT) 							\
static ssize_t qsfp##PORT##_diag_read(                          		\
	struct file *filp, struct kobject *kobj, 	                        \
	struct bin_attribute *attr, char *buffer, loff_t off, size_t count)     \
{                                                                               \
	struct xocl_xmc *xmc =                                                  \
		dev_get_drvdata(container_of(kobj, struct device, kobj));       \
	return xmc_qsfp_diag_read(xmc, PORT, buffer, off, count);      		\
}

QSFP_DIAG_READ(0)
QSFP_DIAG_READ(1)
QSFP_DIAG_READ(2)
QSFP_DIAG_READ(3)

#define QSFP_DIAG_ATTR(PORT)                                               	\
static struct bin_attribute bin_attr_qsfp##PORT##_diag = {                 	\
	.attr = {                                                               \
		.name = "qsfp" #PORT "_diag",	                        	\
		.mode = 0444                                                   	\
	},                                                                      \
	.read = qsfp##PORT##_diag_read,                               		\
	.size = 0                                                               \
};
QSFP_DIAG_ATTR(0);
QSFP_DIAG_ATTR(1);
QSFP_DIAG_ATTR(2);
QSFP_DIAG_ATTR(3);

/*
 * qsfp_i2c read,write
 */

#define QSFP_I2C_RW(PORT, RW)							\
static ssize_t qsfp##PORT##_i2c_##RW(                    			\
	struct file *filp, struct kobject *kobj, 	                        \
	struct bin_attribute *attr, char *buffer, loff_t off, size_t count)     \
{                                                                               \
	struct xocl_xmc *xmc =                                                  \
		dev_get_drvdata(container_of(kobj, struct device, kobj));       \
	return xmc_qsfp_i2c_##RW(xmc, PORT, buffer, off, count);		\
}

#define QSFP_I2C_PORT(PORT) 							\
	QSFP_I2C_RW(PORT, read) 						\
	QSFP_I2C_RW(PORT, write)

QSFP_I2C_PORT(0)
QSFP_I2C_PORT(1)
QSFP_I2C_PORT(2)
QSFP_I2C_PORT(3)

#define QSFP_I2C_ATTR(PORT)	                                                \
static struct bin_attribute bin_attr_qsfp##PORT##_i2c = {                	\
	.attr = {                                                               \
		.name = "qsfp" #PORT "_i2c",  					\
		.mode = 0644                                                    \
	},                                                                      \
	.read = qsfp##PORT##_i2c_read,   					\
	.write = qsfp##PORT##_i2c_write, 					\
	.size = 0                        					\
};

QSFP_I2C_ATTR(0);
QSFP_I2C_ATTR(1);
QSFP_I2C_ATTR(2);
QSFP_I2C_ATTR(3);

/* qsfp_io_conifg, e.g. qsfp0_io_config */
#define QSFP_IO_CONFIG_RW(PORT, RW) 						\
static ssize_t qsfp##PORT##_io_config_##RW(   		                        \
	struct file *filp, struct kobject *kobj, 	                        \
	struct bin_attribute *attr, char *buffer, loff_t off, size_t count)     \
{                                                                               \
	struct xocl_xmc *xmc =                                                  \
		dev_get_drvdata(container_of(kobj, struct device, kobj));       \
	return xmc_qsfp_io_##RW(xmc, PORT, buffer, off, count);                 \
}

#define QSFP_IO_CONFIG(PORT) 							\
	QSFP_IO_CONFIG_RW(PORT, read) 						\
	QSFP_IO_CONFIG_RW(PORT, write)

QSFP_IO_CONFIG(0);
QSFP_IO_CONFIG(1);
QSFP_IO_CONFIG(2);
QSFP_IO_CONFIG(3);

#define QSFP_IO_CONFIG_ATTR(PORT)                                               \
static struct bin_attribute bin_attr_qsfp##PORT##_io_config = {                 \
	.attr = {                                                               \
		.name = "qsfp" #PORT "_io_config",	                        \
		.mode = 0644                                                    \
	},                                                                      \
	.read = qsfp##PORT##_io_config_read,                                    \
	.write = qsfp##PORT##_io_config_write,                                  \
	.size = 0                                                               \
};
QSFP_IO_CONFIG_ATTR(0);
QSFP_IO_CONFIG_ATTR(1);
QSFP_IO_CONFIG_ATTR(2);
QSFP_IO_CONFIG_ATTR(3);

static struct bin_attribute *xmc_bin_attrs[] = {
	&bin_dimm_temp_by_mem_topology_attr,
	&bin_attr_qsfp0_diag,
	&bin_attr_qsfp1_diag,
	&bin_attr_qsfp2_diag,
	&bin_attr_qsfp3_diag,
	&bin_attr_qsfp0_i2c,
	&bin_attr_qsfp1_i2c,
	&bin_attr_qsfp2_i2c,
	&bin_attr_qsfp3_i2c,
	&bin_attr_qsfp0_io_config,
	&bin_attr_qsfp1_io_config,
	&bin_attr_qsfp2_io_config,
	&bin_attr_qsfp3_io_config,
	NULL,
};

static struct attribute_group xmc_attr_group = {
	.attrs = xmc_attrs,
	.bin_attrs = xmc_bin_attrs,
};

static ssize_t cmc_image_read(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct xocl_xmc *xmc =
		dev_get_drvdata(container_of(kobj, struct device, kobj));
	ssize_t ret = 0;

	if (xmc->mgmt_binary == NULL)
		goto bail;

	if (off >= xmc->mgmt_binary_length)
		goto bail;

	if (off + count > xmc->mgmt_binary_length)
		count = xmc->mgmt_binary_length - off;

	memcpy(buf, xmc->mgmt_binary + off, count);

	ret = count;
bail:
	return ret;
}

static size_t image_write(char **image, size_t sz,
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

static ssize_t cmc_image_write(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t off, size_t count)
{
	struct xocl_xmc *xmc =
		dev_get_drvdata(container_of(kobj, struct device, kobj));

	xmc->mgmt_binary_length = (u32)image_write(&xmc->mgmt_binary,
			xmc->mgmt_binary_length, buffer, off, count);

	return xmc->mgmt_binary_length ? count : -ENOMEM;
}

static struct bin_attribute cmc_image_attr = {
	.attr = {
		.name = "cmc_image",
		.mode = 0600
	},
	.read = cmc_image_read,
	.write = cmc_image_write,
	.size = 0
};

static struct bin_attribute *xmc_mini_bin_attrs[] = {
	&cmc_image_attr,
	NULL,
};

static struct attribute_group xmc_mini_attr_group = {
	.attrs = xmc_mini_attrs,
	.bin_attrs = xmc_mini_bin_attrs,
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

/* For power */
static ssize_t hwmon_power_show(struct device *dev,
	struct device_attribute *da, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	int index = to_sensor_dev_attr(da)->index;
	u64 val = xmc_get_power(pdev, HWMON_INDEX2VAL_KIND(index), true);

	return sprintf(buf, "%lld\n", val);
}

#define	HWMON_POWER_SYSFS_NODE(id, name)				\
	static ssize_t power##id##_label(struct device *dev,		\
		struct device_attribute *attr, char *buf) {		\
		return sprintf(buf, "%s\n", name);			\
	}								\
	static SENSOR_DEVICE_ATTR(power##id##_average, 0444, hwmon_power_show,\
		NULL, HWMON_INDEX(0, SENSOR_MAX));			\
	static SENSOR_DEVICE_ATTR(power##id##_input, 0444, hwmon_power_show, \
		NULL, HWMON_INDEX(0, SENSOR_INS));			\
	static SENSOR_DEVICE_ATTR(power##id##_label, 0444, power##id##_label, \
		NULL, HWMON_INDEX(0, SENSOR_INS))
#define	HWMON_POWER_ATTRS(id)						\
	&sensor_dev_attr_power##id##_average.dev_attr.attr,		\
	&sensor_dev_attr_power##id##_input.dev_attr.attr,		\
	&sensor_dev_attr_power##id##_label.dev_attr.attr

#define HWMON_CLOCKSCALING_SYSFS_NODE(type, id, name)			\
	static ssize_t type##id##_label(struct device *dev,		\
		struct device_attribute *attr, char *buf) {		\
		return sprintf(buf, "%s\n", name);			\
	}								\
	static SENSOR_DEVICE_ATTR(type##id##_max, 0444,			\
		hwmon_scaling_threshold_##type##_show, NULL, 0);	\
	static SENSOR_DEVICE_ATTR(type##id##_input, 0644,		\
		hwmon_scaling_target_##type##_show,			\
		hwmon_scaling_target_##type##_store, 0);		\
	static SENSOR_DEVICE_ATTR(type##id##_label, 0444,		\
		type##id##_label, NULL, HWMON_INDEX(0, SENSOR_INS))
#define HWMON_CLOCKSCALING_ATTRS(type, id)				\
	&sensor_dev_attr_##type##id##_max.dev_attr.attr,		\
	&sensor_dev_attr_##type##id##_input.dev_attr.attr,		\
	&sensor_dev_attr_##type##id##_label.dev_attr.attr

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
HWMON_VOLT_CURR_SYSFS_NODE(in, 15, "VCC 3V3", VOL_VCC_3V3);
HWMON_VOLT_CURR_SYSFS_NODE(in, 16, "1V2 HBM", VOL_HBM_1V2);
HWMON_VOLT_CURR_SYSFS_NODE(in, 17, "2V5 VPP", VOL_VPP_2V5);
HWMON_VOLT_CURR_SYSFS_NODE(in, 18, "VCC INT BRAM", VOL_VCCINT_BRAM);
HWMON_VOLT_CURR_SYSFS_NODE(curr, 1, "12V PEX Current", CUR_12V_PEX);
HWMON_VOLT_CURR_SYSFS_NODE(curr, 2, "12V AUX Current", CUR_12V_AUX);
HWMON_VOLT_CURR_SYSFS_NODE(curr, 3, "VCC INT Current", CUR_VCC_INT);
HWMON_VOLT_CURR_SYSFS_NODE(curr, 4, "3V3 PEX Current", CUR_3V3_PEX);
HWMON_VOLT_CURR_SYSFS_NODE(curr, 5, "VCC 0V85 Current", CUR_VCC_0V85);
HWMON_VOLT_CURR_SYSFS_NODE(curr, 6, "3V3 AUX Current", CUR_3V3_AUX);
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
HWMON_POWER_SYSFS_NODE(1, "POWER");
HWMON_CLOCKSCALING_SYSFS_NODE(power, 2, "CS_TARGET_POWER");
HWMON_CLOCKSCALING_SYSFS_NODE(temp, 15, "CS_TARGET_TEMP");

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
	HWMON_VOLT_CURR_ATTRS(in, 15),
	HWMON_VOLT_CURR_ATTRS(in, 16),
	HWMON_VOLT_CURR_ATTRS(in, 17),
	HWMON_VOLT_CURR_ATTRS(in, 18),
	HWMON_VOLT_CURR_ATTRS(curr, 1),
	HWMON_VOLT_CURR_ATTRS(curr, 2),
	HWMON_VOLT_CURR_ATTRS(curr, 3),
	HWMON_VOLT_CURR_ATTRS(curr, 4),
	HWMON_VOLT_CURR_ATTRS(curr, 5),
	HWMON_VOLT_CURR_ATTRS(curr, 6),
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
	HWMON_POWER_ATTRS(1),
	HWMON_CLOCKSCALING_ATTRS(power, 2),
	HWMON_CLOCKSCALING_ATTRS(temp, 15),
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
		(void) snprintf(nm + n, sizeof(nm) - n, "%s", "_mgmt");
	else
		(void) snprintf(nm + n, sizeof(nm) - n, "%s", "_user");
	return sprintf(buf, "%s\n", nm);
}
static struct sensor_device_attribute name_attr =
	SENSOR_ATTR(name, 0444, show_hwmon_name, NULL, 0);

static void mgmt_sysfs_destroy_xmc_mini(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;

	xmc = platform_get_drvdata(pdev);
	sysfs_remove_group(&pdev->dev.kobj, &xmc_mini_attr_group);
}

static int mgmt_sysfs_create_xmc_mini(struct platform_device *pdev)
{
	int err;

	err = sysfs_create_group(&pdev->dev.kobj, &xmc_mini_attr_group);
	if (err) {
		xocl_err(&pdev->dev, "create xmc mini attrs failed: 0x%x", err);
	}

	return err;
}

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

static int stop_ert_nolock(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int retry = 0;

	/* MPSOC platforms do not have MB ERT */
	if (XOCL_DSA_IS_MPSOC(xdev))
		return 0;

	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return -ENODEV;
	else if (!xmc->enabled)
		return -ENODEV;

	while ((READ_CQ(xmc, 0) != (ERT_EXIT_CMD_OP | ERT_EXIT_ACK)) &&
		retry++ < MAX_ERT_RETRY) {
		WRITE_CQ(xmc, ERT_EXIT_CMD, 0);
		msleep(RETRY_INTERVAL);
	}
	if (retry >= MAX_ERT_RETRY) {
		xocl_warn(&xmc->pdev->dev, "Failed to stop sched");
		xocl_warn(&xmc->pdev->dev, "Scheduler CQ status 0x%x",
					READ_CQ(xmc, 0));
		return -ETIMEDOUT;
	}

	xocl_info(&xmc->pdev->dev, "ERT stopped, retry %d", retry);
	return 0;
}


static int stop_xmc_nolock(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	int retry = 0;
	u32 reg_val = 0;
	void *xdev_hdl;
	u32 magic = 0;
	int ret;
	bool skip_xmc = false;

	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return -ENODEV;
	else if (!xmc->enabled)
		return -ENODEV;

	/* freeze cmc prior to stop cmc */
	ret = xmc_access(pdev, XOCL_XMC_FREEZE);
	if (ret)
		return ret;

	xdev_hdl = xocl_get_xdev(xmc->pdev);

	magic = READ_REG32(xmc, XMC_MAGIC_REG);
	if (!magic) {
		xocl_info(&xmc->pdev->dev, "Image is not loaded");
		return 0;
	}

	reg_val = READ_GPIO(xmc, 0);

	skip_xmc = xmc_in_bitfile(xmc->pdev);
	if (skip_xmc)
		xocl_info(&xmc->pdev->dev, "MB Reset GPIO 0x%x (ert), 0x%x (xmc)", reg_val,
			  READ_XMC_GPIO(xmc, 0));
	else
		xocl_info(&xmc->pdev->dev, "MB Reset GPIO 0x%x", reg_val);

	/* Stop XMC and ERT if its currently running */
	if (reg_val == GPIO_ENABLED) {
		xocl_info(&xmc->pdev->dev,
			"XMC info, version 0x%x, status 0x%x, id 0x%x",
			READ_REG32(xmc, XMC_VERSION_REG),
			READ_REG32(xmc, XMC_STATUS_REG), magic);

		if (!skip_xmc) {
			reg_val = READ_REG32(xmc, XMC_STATUS_REG);
			if (!(reg_val & STATUS_MASK_STOPPED)) {
				xocl_info(&xmc->pdev->dev, "Stopping XMC...");
				WRITE_REG32(xmc, CTL_MASK_STOP, XMC_CONTROL_REG);
				WRITE_REG32(xmc, 1, XMC_STOP_CONFIRM_REG);
			}
			retry = 0;
			while (retry++ < MAX_XMC_RETRY &&
				   !(READ_REG32(xmc, XMC_STATUS_REG) & STATUS_MASK_STOPPED))
				msleep(RETRY_INTERVAL);

			/* Wait for XMC to stop and then check that ERT
			 * has also finished */
			if (retry >= MAX_XMC_RETRY) {
				xocl_err(&xmc->pdev->dev, "Failed to stop XMC, Error Reg 0x%x",
						 READ_REG32(xmc, XMC_ERROR_REG));
				xmc->state = XMC_STATE_ERROR;
				return -ETIMEDOUT;
			}
			xocl_info(&xmc->pdev->dev, "XMC Stopped, retry %d",	retry);
		} else {
			xocl_info(&xmc->pdev->dev, "Skip XMC stop since XMC is loaded through fpga bitfile");
		}
		if (!SELF_JUMP(READ_IMAGE_SCHED(xmc, 0)) && SCHED_EXIST(xmc)) {
			xocl_info(&xmc->pdev->dev, "Stopping scheduler...");
			/* We try to stop ERT, but based on existing HW design
			 * this can't be done reliably. We will ignore the
			 * error, and if it doesn't stop, system needs to be
			 * cold rebooted to recover from the HW failure.
			 */
			(void) stop_ert_nolock(pdev);
			xocl_info(&xmc->pdev->dev, "Scheduler Stopped");
		}
	}

	/* Hold XMC in reset now that its safely stopped */
	xocl_info(&xmc->pdev->dev,
		"XMC info, version 0x%x, status 0x%x, id 0x%x",
		READ_REG32(xmc, XMC_VERSION_REG),
		READ_REG32(xmc, XMC_STATUS_REG),
		READ_REG32(xmc, XMC_MAGIC_REG));
	return 0;
}

static int stop_xmc(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	int ret = 0;

	if (autonomous_xmc(pdev))
		return ret;

	xocl_info(&pdev->dev, "Stop Microblaze...");
	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return -ENODEV;
	else if (!xmc->enabled)
		return -ENODEV;

	if (xmc->sysfs_created) {
		mgmt_sysfs_destroy_xmc(pdev);
		xmc->sysfs_created = false;
	}

	mutex_lock(&xmc->xmc_lock);
	ret = stop_xmc_nolock(pdev);
	mutex_unlock(&xmc->xmc_lock);

	return ret;
}

static void xmc_enable_mailbox(struct xocl_xmc *xmc)
{
	u32 val = 0;

	xmc->mbx_enabled = false;

	if (!XMC_PRIVILEGED(xmc))
		return;

	if (READ_REG32(xmc, XMC_FEATURE_REG) & XMC_NO_MAILBOX_MASK) {
		xocl_info(&xmc->pdev->dev, "XMC mailbox is not supported");
		return;
	}

	xmc->mbx_enabled = true;
	safe_read32(xmc, XMC_HOST_MSG_OFFSET_REG, &val);
	xmc->mbx_offset = val;
	xocl_info(&xmc->pdev->dev, "XMC mailbox offset read during probe: 0x%x", val);
}

static inline int wait_reg_value(struct xocl_xmc *xmc, void __iomem *base, u32 mask)
{
	u32 val = XOCL_READ_REG32(base);
	int i;

	for (i = 0; !(val & mask) && i < MAX_XMC_RETRY; i++) {
		msleep(RETRY_INTERVAL);
		val = XOCL_READ_REG32(base);
	}

	return (val & mask) ? 0 : -ETIMEDOUT;
}

/*
 * Wait for XMC to start
 * Note that ERT will start long before XMC so we don't check anything
 */
static int xmc_sense_ready(struct xocl_xmc *xmc)
{
	u32 xmc_core_version = 0;
	int ret = 0;

	/*
	 * If dev tree has CMC_MUTEX register defined, we rely on the
	 * regmap_ready bit to check whether cmc is ready, otherwise,
	 * we still use the legacy 'init done' bit in REGMAP
	 */
	if (xmc->base_addrs[IO_MUTEX]) {
		ret = wait_reg_value(xmc,
		    xmc->base_addrs[IO_MUTEX] + XOCL_RES_OFFSET_CHANNEL2,
		    REGMAP_READY_MASK);

		if (ret) {
			xocl_err(&xmc->pdev->dev, "REGMAP not ready.");
			goto errout;
		}
		xocl_info(&xmc->pdev->dev, "REGMAP ready.");

		/*
		 * How to define a cmc_core_version:
		 *   XMC_MAGIC_REG is magjc number 0x74736574
		 *   XMC_VERSION_REG start from 0x0c000000
		 *   XMC_CORE_VERSION_REG starr from 0x0c000000
		 */
		if (VALID_MAGIC(READ_REG32(xmc, XMC_MAGIC_REG)) &&
		    VALID_CMC_VERSION(READ_REG32(xmc, XMC_VERSION_REG)) &&
		    VALID_CORE_VERSION(READ_REG32(xmc, XMC_CORE_VERSION_REG))) {
			xmc_core_version = READ_REG32(xmc, XMC_CORE_VERSION_REG);
		}
		xocl_info(&xmc->pdev->dev, "Core Version 0x%x", xmc_core_version);

		/* early version do not support quick check, fallback to wait */
		if (xmc_core_version >= XMC_CORE_SUPPORT_SENSOR_READY) {
			ret = wait_reg_value(xmc,
			   xmc->base_addrs[IO_REG] + XMC_STATUS2_REG,
			   SENSOR_DATA_READY_MASK);
			if (ret) {
				/* Legacy CMC, rollback to waiting 5 seconds */
				ret = 0;
				xocl_warn(&xmc->pdev->dev, "Sensor Data not ready.");
			} else {
				xocl_info(&xmc->pdev->dev, "Sensor Data ready.");
				/* skip waiting 5 more seconds */
				goto done;
			}
		}
	} else {
		ret = wait_reg_value(xmc,
		   xmc->base_addrs[IO_REG] + XMC_STATUS_REG,
		   STATUS_MASK_INIT_DONE);
		if (ret) {
			xocl_err(&xmc->pdev->dev, "XMC did not finish init.");
			goto errout;
		}
		xocl_info(&xmc->pdev->dev, "XMC init done.");
	}

	/* not support sensor ready, wait 5 more seconds */
	xocl_info(&xmc->pdev->dev,
	    "Wait for 5 seconds to stablize SC connection.");
	ssleep(5);
done:
	return ret;

errout:
	xocl_err(&xmc->pdev->dev, "Error Reg 0x%x", READ_REG32(xmc, XMC_ERROR_REG));
	xocl_err(&xmc->pdev->dev, "Status Reg 0x%x", READ_REG32(xmc, XMC_STATUS_REG));

	return ret;
}

static int load_xmc(struct xocl_xmc *xmc)
{
	int retry = 0;
	u32 reg_val = 0;
	int ret = 0;
	void *xdev_hdl;
	bool skip_xmc = false;

	if (!xmc->enabled)
		return -ENODEV;

	if (autonomous_xmc(xmc->pdev))
		return ret;

	mutex_lock(&xmc->xmc_lock);

	xdev_hdl = xocl_get_xdev(xmc->pdev);
	skip_xmc = xmc_in_bitfile(xmc->pdev);

	if (skip_xmc) {
		xocl_info(&xmc->pdev->dev, "Skip XMC stop/load, since XMC is loaded through fpga bitfile");
		if (READ_XMC_GPIO(xmc, 0) == GPIO_ENABLED)
			xmc->state = XMC_STATE_ENABLED;
		if (xocl_subdev_is_vsec(xdev_hdl))
			goto done;
	}

	/* Stop XMC first */
	ret = stop_xmc_nolock(xmc->pdev);
	if (ret != 0)
		goto out;

	WRITE_GPIO(xmc, GPIO_RESET, 0);
	reg_val = READ_GPIO(xmc, 0);
	xmc->state = XMC_STATE_RESET;
	xocl_info(&xmc->pdev->dev, "MB Reset GPIO 0x%x", reg_val);

	/* Shouldnt make it here but if we do then exit */
	if (reg_val != GPIO_RESET) {
		xocl_err(&xmc->pdev->dev, "Hold reset GPIO Failed");
		xmc->state = XMC_STATE_ERROR;
		ret = -EIO;
		goto out;
	}

	/* Load XMC and ERT Image */
	if (!skip_xmc && xocl_mb_mgmt_on(xdev_hdl) && xmc->mgmt_binary_length) {
		if (xmc->mgmt_binary_length > xmc->range[IO_IMAGE_MGMT]) {
			xocl_err(&xmc->pdev->dev, "XMC image too long %d",
				xmc->mgmt_binary_length);
			goto out;
		} else {
			xocl_info(&xmc->pdev->dev, "Copying XMC image len %d",
				xmc->mgmt_binary_length);
			COPY_MGMT(xmc, xmc->mgmt_binary, xmc->mgmt_binary_length);
		}
	}

	if (xocl_mb_sched_on(xdev_hdl) && xmc->sche_binary_length) {
		if (xmc->sche_binary_length > xmc->range[IO_IMAGE_SCHED]) {
			xocl_info(&xmc->pdev->dev, "scheduler image too long %d",
				xmc->sche_binary_length);
			goto out;
		} else {
			xocl_info(&xmc->pdev->dev, "Copying scheduler image len %d",
				xmc->sche_binary_length);
			COPY_SCHE(xmc, xmc->sche_binary, xmc->sche_binary_length);
		}
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

	ret = xmc_sense_ready(xmc);
	if (ret) {
		xmc->state = XMC_STATE_ERROR;
		goto out;
	}
done:
	if (READ_GPIO(xmc, 0) == GPIO_ENABLED)
		xmc->state = XMC_STATE_ENABLED;

	xocl_info(&xmc->pdev->dev, "XMC and scheduler Enabled, retry %d",
			retry);
	xocl_info(&xmc->pdev->dev,
		"XMC info, version 0x%x, status 0x%x, id 0x%x",
		READ_REG32(xmc, XMC_VERSION_REG),
		READ_REG32(xmc, XMC_STATUS_REG),
		READ_REG32(xmc, XMC_MAGIC_REG));

	if (XMC_PRIVILEGED(xmc) && xmc_clk_scale_on(xmc->pdev))
		xmc_clk_scale_config(xmc->pdev);

	mutex_unlock(&xmc->xmc_lock);

	/* Enabling XMC mailbox support. */
	xmc_enable_mailbox(xmc);

	mutex_lock(&xmc->mbx_lock);
	xmc_load_board_info(xmc);
	mutex_unlock(&xmc->mbx_lock);

	if (!xmc->sysfs_created) {
		ret = mgmt_sysfs_create_xmc(xmc->pdev);
		if (ret) {
			xocl_err(&xmc->pdev->dev, "Create sysfs failed, err %d", ret);
			goto out;
		}

		xmc->sysfs_created = true;
	}

	return 0;
out:
	mutex_unlock(&xmc->xmc_lock);

	return ret;
}

static int xmc_reset(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;

	xocl_info(&pdev->dev, "Reset Microblaze...");
	xmc = platform_get_drvdata(pdev);
	if (!xmc || !xmc->enabled)
		return -EINVAL;

	load_xmc(xmc);
	return 0;
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

	if (autonomous_xmc(pdev))
		return 0;

	binary = xmc->mgmt_binary;
	xmc->mgmt_binary = vmalloc(len);
	if (!xmc->mgmt_binary)
		return -ENOMEM;

	if (binary)
		vfree(binary);
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

	if (autonomous_xmc(pdev))
		return 0;

	binary = xmc->sche_binary;
	xmc->sche_binary = vmalloc(len);
	if (!xmc->sche_binary)
		return -ENOMEM;

	if (binary)
		vfree(binary);
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

	cntrl = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_CONTROL_REG);
	cntrl |= XMC_CLOCK_SCALING_CONTROL_REG_EN;
	WRITE_RUNTIME_CS(xmc, cntrl, XMC_CLOCK_SCALING_CONTROL_REG);
}

static int raptor_cmc_access(struct platform_device *pdev,
	enum xocl_xmc_flags flags)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	u32 val, ack;
	int retry;
	int err = 0;
	u32 grant = 0; /* 0 is disabled, 1 is enabled */

	if (flags == XOCL_XMC_FREE) {
		uint64_t addr;
		u32 pr_gate = 0;

		xocl_axigate_status(xdev, XOCL_SUBDEV_LEVEL_PRP, &pr_gate);
		if (!pr_gate) {
			/* ULP is not connected, return */
			return -ENODEV;
		}
		grant = 1; /* set to 1:enabled */
		/*
		 * for grant (free) access, we are looking for new
		 * features, if no new features, skip the grant operation
		 */
		addr = xocl_iores_get_offset(xdev, IORES_GAPPING);
		if (addr == (uint64_t)-1) {
			xocl_xdev_info(xdev, "No %s resource, skip.",
			    NODE_GAPPING);
			return 0;
		}
		/*
		 * Dancing with CMC here:
		 * 0-24 bit is address read from xclbin
		 * 28 is flag for enable, set to 0x0
		 * 29 is flag for present, set to 0x1
		 * Note: seems that we should write all data at one time.
		 * Apply 24:0 address, set preset bit to 1, and keep other bits to intact.
		 */
		val = READ_REG32(xmc, XMC_HOST_NEW_FEATURE_REG1);
		val &= ~0x1FFFFFF;
		val |= ((addr & 0x01FFFFFF) | XMC_HOST_NEW_FEATURE_REG1_FEATURE_PRESENT);
		WRITE_REG32(xmc, val, XMC_HOST_NEW_FEATURE_REG1);
		xocl_xdev_info(xdev, "%s is 0x%llx, set New Feature Table to 0x%x\n",
		    NODE_GAPPING, addr, val);
	} else if (flags == XOCL_XMC_FREEZE) {
		grant = 0;
	} else {
		xocl_xdev_info(xdev, "invalid flags %d", flags);
		return -EINVAL;
	}

	if (!xmc->base_addrs[IO_MUTEX]) {
		xocl_xdev_info(xdev, "No %s resource, skip.",
		    NODE_CMC_MUTEX);
		return 0;
	}
	XOCL_WRITE_REG32(grant, xmc->base_addrs[IO_MUTEX] +
				XOCL_RES_OFFSET_CHANNEL1);
	for (retry = 0; retry < 100; retry++) {
		ack = XOCL_READ_REG32(xmc->base_addrs[IO_MUTEX] +
					XOCL_RES_OFFSET_CHANNEL2);
		/* Success condition: grant and ack have same value */
		if ((grant & MUTEX_GRANT_MASK) == (ack & MUTEX_ACK_MASK))
				break;
		msleep(100);
	}

	if ((grant & MUTEX_GRANT_MASK) != (ack & MUTEX_ACK_MASK)) {
		xocl_xdev_err(xdev,
		    "Grant falied. The bit 0 in Ack (0x%x) is not the same "
		    "in grant (0x%x)", ack, grant);
		err = -EBUSY;
		goto fail;
	}

	xocl_xdev_info(xdev, "%s CMC succeeded.",
	    flags == XOCL_XMC_FREE ? "Grant" : "Release");
fail:
	return err;
}

static inline char*
xmc_get_heartbeat_reason(enum xmc_xhe_error error_code)
{
	switch (error_code) {
	case XHE_NO_ERR:
		return "NO_ERR";
	case XHE_SINGLE_ERR:
		return "SINGLE_SENSOR_UPDATE_ERR";
	case XHE_MULTI_ERR:
		return "MULTIPLE_SENSOR_UPDATE_ERR";
	default:
		return "UNDEFINED_ERR";
	}
}

static void sensor_status_check(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	struct xmc_heartbeat_err_code err_code = { 0 };
	XOCL_TIMESPEC time;
	u32 val;
	
	if (!is_heartbeat_supported(xmc))
		return;

	safe_read32(xmc, XMC_HEARTBEAT_REG, &val);
	if (val != xmc->heartbeat_count) {
		if (xmc->heartbeat_stall) {
			xmc->heartbeat_stall = false;
			xocl_info(&xmc->pdev->dev, "CMC sensor update resumed.");
		}
		xmc->heartbeat_count = val;
		return;
	}

	/* cmc heartbeat count is stall, reporting */
	if (xmc->heartbeat_stall)
		return;

	xmc->heartbeat_stall = true;
	XOCL_GETTIME(&time);
	xmc->heartbeat_time = (u64)time.tv_sec;
	safe_read32(xmc, XMC_HEARTBEAT_ERR_CODE_REG, (u32 *)&err_code);

	xocl_warn(&xmc->pdev->dev, "%s(0x%x), last sensor id: 0x%x, sensor number: %d",
		xmc_get_heartbeat_reason(err_code.xhe_error), *(u32 *)&err_code,
		err_code.xhe_last_sensor_id,
		err_code.xhe_sensor_no);
}

static int xmc_offline(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);

	if (!xmc)
		return 0;

	if (xmc->sysfs_created) {
		mgmt_sysfs_destroy_xmc(pdev);
		xmc->sysfs_created = false;
	}

	xmc->bdinfo_loaded = false;

	return xmc_access(pdev, XOCL_XMC_FREEZE);
}
static int xmc_online(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	int ret;

	BUG_ON(!xmc);

	if (!xmc->sysfs_created) {
		ret = mgmt_sysfs_create_xmc(xmc->pdev);
		if (ret) {
			xocl_err(&xmc->pdev->dev, "Create sysfs failed, err %d", ret);
			return ret;
		}

		xmc->sysfs_created = true;
	}

	ret = xmc_access(pdev, XOCL_XMC_FREE);
	if (ret && ret != -ENODEV) {
		mgmt_sysfs_destroy_xmc(pdev);
		xmc->sysfs_created = false;
		return ret;
	}

	return 0;
}

static struct xocl_mb_funcs xmc_ops = {
	.offline_cb		= xmc_offline,
	.online_cb		= xmc_online,
	.load_mgmt_image	= load_mgmt_image,
	.load_sche_image	= load_sche_image,
	.reset			= xmc_reset,
	.stop			= stop_xmc,
	.get_data		= xmc_get_data,
	.xmc_access             = xmc_access,
	.clock_status		= clock_status_check,
	.get_serial_num		= xmc_get_serial_num,
	.sensor_status		= sensor_status_check,
};

static void xmc_unload_board_info(struct xocl_xmc *xmc)
{
	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));
	if (xmc->bdinfo_raw)
		vfree(xmc->bdinfo_raw);
	xmc->bdinfo_raw = NULL;
}

static int __xmc_remove(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	void *hdl;
	int	i;

	xmc = platform_get_drvdata(pdev);
	if (!xmc)
		return 0;

	xocl_drvinst_release(xmc, &hdl);

	if (xmc->mgmt_binary)
		vfree(xmc->mgmt_binary);
	if (xmc->sche_binary)
		vfree(xmc->sche_binary);

	if (xmc->mini_sysfs_created)
		mgmt_sysfs_destroy_xmc_mini(pdev);

	if (!xmc->enabled)
		goto end;

	if (xmc->sysfs_created)
		mgmt_sysfs_destroy_xmc(pdev);

	mutex_lock(&xmc->mbx_lock);
	xmc_unload_board_info(xmc);
	mutex_unlock(&xmc->mbx_lock);
end:
	for (i = 0; i < NUM_IOADDR; i++) {
		if (xmc->base_addrs[i]) {
			iounmap(xmc->base_addrs[i]);
			xmc->range[i] = 0;
		}
	}
	if (xmc->cache)
		vfree(xmc->cache);
	mutex_destroy(&xmc->xmc_lock);
	mutex_destroy(&xmc->mbx_lock);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void xmc_remove(struct platform_device *pdev)
{
	__xmc_remove(pdev);
}
#else
#define xmc_remove __xmc_remove
#endif

static const char *xmc_get_board_info(uint32_t *bdinfo_raw,
	uint32_t bdinfo_raw_sz, enum board_info_key key, size_t *len)
{
	char *buf, *p;
	u32 sz;

	if (!bdinfo_raw)
		return NULL;

	buf = (char *)bdinfo_raw;
	sz = bdinfo_raw_sz;
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

	return NULL;
}

static int xmc_mapio_by_name(struct xocl_xmc *xmc, struct resource *res)
{
	int	id;

	id = xocl_res_name2id(res_map, ARRAY_SIZE(res_map), res->name);
	if (id < 0) {
		xocl_info(&xmc->pdev->dev, "resource %s not found", res->name);
		return -EINVAL;
	}
	if (xmc->base_addrs[id]) {
		xocl_err(&xmc->pdev->dev, "resource %s already mapped",
				res->name);
		return -EINVAL;
	}
	xmc->base_addrs[id] = ioremap_nocache(res->start,
			res->end - res->start + 1);
	if (!xmc->base_addrs[id]) {
		xocl_err(&xmc->pdev->dev, "resource %s map failed", res->name);
		return -EIO;
	}
	xmc->range[id] = res->end - res->start + 1;

	return 0;
}

static void inline xmc_enable_sensor_heartbeat(struct xocl_xmc *xmc)
{

	if (!is_heartbeat_supported(xmc))
		return;

	xmc->heartbeat_stall = false;
	safe_read32(xmc, XMC_HEARTBEAT_REG, &xmc->heartbeat_count);
}

static int xmc_probe(struct platform_device *pdev)
{
	struct xocl_xmc *xmc;
	struct resource *res;
	void *xdev_hdl;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int i, err = 0;

	xmc = xocl_drvinst_alloc(&pdev->dev, sizeof(*xmc));
	if (!xmc) {
		xocl_err(&pdev->dev, "out of memory");
		return -ENOMEM;
	}

	xmc->pdev = pdev;
	platform_set_drvdata(pdev, xmc);
	xocl_dbg(&pdev->dev, "fops %lx", (ulong)&xmc_fops);

	mutex_init(&xmc->xmc_lock);
	mutex_init(&xmc->mbx_lock);

	for (i = 0; i < NUM_IOADDR; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res) {
			xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
				res->start, res->end);
			if (res->name) {
			       err = xmc_mapio_by_name(xmc, res);	
			       if (!err)
				       continue;
			}
			/* fall back to legacy */
			xmc->base_addrs[i] = ioremap_nocache(res->start,
				res->end - res->start + 1);
			if (!xmc->base_addrs[i]) {
				err = -EIO;
				xocl_err(&pdev->dev, "Map iomem failed");
				goto failed;
			}
			xmc->range[i] = res->end - res->start + 1;
		} else
			break;
	}

	xmc->priv_data = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
	xdev_hdl = xocl_get_xdev(pdev);

	xmc->sc_presence = nosc_xmc(xmc->pdev) ? 0 : 1;

	if (XMC_PRIVILEGED(xmc)) {
		if (!xmc->priv_data) {
			xmc->priv_data = vzalloc(sizeof(*xmc->priv_data));
			if (!xmc->priv_data) {
				xocl_err(&pdev->dev, "Unable to alloc mem");
				goto failed;
			}
		}
		if (xocl_clk_scale_on(xdev_hdl))
			xmc->priv_data->flags |= XOCL_XMC_CLK_SCALING;
		if (xocl_cmc_in_bitfile(xdev_hdl))
			xmc->priv_data->flags |= XOCL_XMC_IN_BITFILE;
	}

	if (XMC_PRIVILEGED(xmc)) {
		if (xmc->base_addrs[IO_REG]) {
			err = mgmt_sysfs_create_xmc_mini(pdev);
			if (err)
				goto failed;
			xmc->mini_sysfs_created = true;
		} else {
			xocl_err(&pdev->dev, "Empty resources");
			err = -EINVAL;
			goto failed;
		}

		if (XOCL_DSA_IS_VERSAL(xdev)) {
			xmc->enabled = true;
			xmc->state = XMC_STATE_ENABLED;
			xmc_enable_mailbox(xmc);
		} else if (!xmc->base_addrs[IO_GPIO]) {
			xocl_info(&pdev->dev, "minimum mode for SC upgrade");
			/* CMC is always enabled on golden image. */
			xmc->enabled = true;
			xmc->state = XMC_STATE_ENABLED;
			xmc_enable_mailbox(xmc);
			return 0;
		}
	}

	xdev_hdl = xocl_get_xdev(pdev);
	if (xocl_mb_mgmt_on(xdev_hdl) || xocl_mb_sched_on(xdev_hdl) ||
		autonomous_xmc(pdev)) {
		xocl_info(&pdev->dev, "Microblaze is supported.");
		xmc->enabled = true;
	} else {
		xocl_info(&pdev->dev, "Microblaze is not supported.");
		return 0;
	}

	if (READ_GPIO(xmc, 0) == GPIO_ENABLED || autonomous_xmc(pdev))
		xmc->state = XMC_STATE_ENABLED;

	xmc->cache = vzalloc(sizeof(struct xcl_sensor));

	if (!xmc->cache) {
		err = -ENOMEM;
		goto failed;
	}

	xmc->cache_expire_secs = XMC_DEFAULT_EXPIRE_SECS;

	/*
	 * Enabling XMC clock scaling support.
	 * clk scaling can only be enabled on mgmt side, why do we set
	 * the enabled bit in feature ROM on user side at all?
	 */
	if (XMC_PRIVILEGED(xmc)) {
		bool cs_en = scaling_condition_check(xmc);
		if (cs_en)
			xocl_info(&pdev->dev, "Runtime clock scaling is supported.\n");

		if (xmc_in_bitfile(xmc->pdev)) {
			if (READ_XMC_GPIO(xmc, 0) == GPIO_ENABLED)
				xmc->state = XMC_STATE_ENABLED;
		}
	}

	xmc_enable_sensor_heartbeat(xmc);

	err = mgmt_sysfs_create_xmc(pdev);
	if (err) {
		xocl_err(&pdev->dev, "Create sysfs failed, err %d", err);
		goto failed;
	}
	xmc->sysfs_created = true;

	return 0;

failed:
	xmc_remove(pdev);
	return err;
}

static struct xocl_drv_private	xmc_priv = {
	.ops = &xmc_ops,
#if PF == MGMTPF
	.fops = &xmc_fops,
#endif
	.dev = -1,
};

static struct platform_device_id xmc_id_table[] = {
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
	int err = alloc_chrdev_region(&xmc_priv.dev, 0, XOCL_MAX_DEVICES,
			XOCL_XMC);
	if (err)
		return err;

	err = platform_driver_register(&xmc_driver);
	if (err) {
		unregister_chrdev_region(xmc_priv.dev, XOCL_MAX_DEVICES);
		return err;
	}

	return 0;
}

void xocl_fini_xmc(void)
{
	unregister_chrdev_region(xmc_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&xmc_driver);
}

static int xmc_mailbox_wait(struct xocl_xmc *xmc)
{
	int retry = MAX_XMC_RETRY * 4;
	u32 val, ctrl_val;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	safe_read32(xmc, XMC_CONTROL_REG, &val);
	while ((retry > 0) && (val & XMC_PKT_OWNER_MASK)) {
		msleep(RETRY_INTERVAL);
		safe_read32(xmc, XMC_CONTROL_REG, &val);
		retry--;
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
		/*
		 *  We set the xmc state as XMC_STATE_ERROR only if it's unrecoverable.
		 *  e.g. get error while upgrading the sc firmware.
		 */
		if (XMC_UNRECOVERABLE_ERR(val)) {
			xocl_err(&xmc->pdev->dev, "Card requires pci hot reset\n");
			xmc->state = XMC_STATE_ERROR;
		}
		safe_read32(xmc, XMC_CONTROL_REG, &ctrl_val);
		safe_write32(xmc, XMC_CONTROL_REG, ctrl_val | XMC_CTRL_ERR_CLR);
		return -EIO;
	}

	return 0;
}

static int xmc_send_pkt(struct xocl_xmc *xmc)
{
	u32 *pkt = (u32 *)&xmc->mbx_pkt;
	u32 len = XMC_PKT_SZ(&xmc->mbx_pkt.hdr);
	int ret = 0;
	u32 i;
	u32 val = 0;

	if (!xmc->mbx_enabled) {
		xocl_err(&xmc->pdev->dev, "CMC mailbox is not supported");
		return -ENOTSUPP;
	}

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));
	/*
	 * On card with ps, eg, u30, when xmc subdevice is probed on host,
	 * the cmc running on ps may be not ready yet. so the saved mbx offset
	 * might be 0. The register should be ready when we are here.
	 * we need check and update the mbx offset.
	 */
	safe_read32(xmc, XMC_HOST_MSG_OFFSET_REG, &val);
	if (!val || val == 0xdeadfa11) {
		xocl_err(&xmc->pdev->dev, "CMC mailbox is not ready");
		return -EIO;
	}
	if (val != xmc->mbx_offset) {
		xocl_info(&xmc->pdev->dev, "CMC mailbox offset updated: 0x%x", val);
		xmc->mbx_offset = val;
	}
#ifdef	MBX_PKT_DEBUG
	xocl_info(&xmc->pdev->dev, "Sending XMC packet: %d DWORDS...", len);
	xocl_info(&xmc->pdev->dev, "opcode=%d payload_sz=0x%x (0x%x)",
		xmc->mbx_pkt.hdr.op, xmc->mbx_pkt.hdr.payload_sz, pkt[0]);
	for (i = 0; i < 16; i++)
		printk(KERN_CONT "%02x ", *((u8 *)(xmc->mbx_pkt.data) + i));
#endif
	/* Push pkt data to mailbox on HW. */
	for (i = 0; i < len; i++)
		safe_write32(xmc, xmc->mbx_offset + i * sizeof(u32), pkt[i]);

	/* Notify HW that a pkt is ready for process. */
	safe_read32(xmc, XMC_CONTROL_REG, &val);
	safe_write32(xmc, XMC_CONTROL_REG, val | XMC_PKT_OWNER_MASK);

	/* Make sure HW is done with the mailbox buffer. */
	ret = xmc_mailbox_wait(xmc);

	return ret;
}

static int xmc_recv_pkt(struct xocl_xmc *xmc)
{
	struct xmc_pkt_hdr hdr;
	u32 *pkt;
	u32 len;
	u32 i;
	int ret = 0;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	/* Receive pkt hdr. */
	pkt = (u32 *)&hdr;
	len = sizeof(hdr) / sizeof(u32);
	for (i = 0; i < len; i++)
		safe_read32(xmc, xmc->mbx_offset + i * sizeof(u32), &pkt[i]);

	pkt = (u32 *)&xmc->mbx_pkt;
	len = XMC_PKT_SZ(&hdr);
	if (hdr.payload_sz == 0 || len > XMC_PKT_MAX_SZ) {
		xocl_warn(&xmc->pdev->dev, "read invalid XMC packet\n");
		return -EINVAL;
	}
	for (i = 0; i < len; i++)
		safe_read32(xmc, xmc->mbx_offset + i * sizeof(u32), &pkt[i]);

	/* Make sure HW is done with the mailbox buffer. */
	ret = xmc_mailbox_wait(xmc);
	return ret;
}

static bool is_xmc_ready(struct xocl_xmc *xmc)
{
	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	if (xmc->state == XMC_STATE_ENABLED)
		return true;

	xocl_err(&xmc->pdev->dev, "XMC is not ready, state=%d\n", xmc->state);
	return false;
}

static bool is_sc_ready(struct xocl_xmc *xmc, bool quiet)
{
	struct xmc_status status;
	struct xocl_dev_core *core = xocl_get_xdev(xmc->pdev);

	if (autonomous_xmc(xmc->pdev) &&
	       	(!(core->priv.flags & XOCL_DSAFLAG_MPSOC)))
		return true;

	if (!xmc->sc_presence)
		return false;

	safe_read32(xmc, XMC_STATUS_REG, (u32 *)&status);
	if (status.sc_mode == XMC_SC_NORMAL ||
		status.sc_mode == XMC_SC_NORMAL_MODE_SC_NOT_UPGRADABLE)
		return true;

	if (!quiet) {
		xocl_err(&xmc->pdev->dev, "SC is not ready, state=%d\n",
			status.sc_mode);
	}
	return false;
}

static int smartnic_cmc_access(struct platform_device *pdev,
	enum xocl_xmc_flags flags)
{
	int ret = 0;
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);

	mutex_lock(&xmc->mbx_lock);

	if (!is_xmc_ready(xmc)) {
		ret = -EINVAL;
		goto done;
	}

	/* Load new info from HW. */
	memset(&xmc->mbx_pkt, 0, sizeof(xmc->mbx_pkt));
	if (flags == XOCL_XMC_FREEZE) {
		xmc->mbx_pkt.hdr.op = XPO_DR_FREEZE;
	} else if (flags == XOCL_XMC_FREE) {
		xmc->mbx_pkt.hdr.op = XPO_DR_FREE;
	} else {
		ret = -EINVAL;
		goto done;
	}

	ret = xmc_send_pkt(xmc);
	if (ret)
		goto done;

	xocl_info(&xmc->pdev->dev, "xmc dynamic region %s done.\n",
	    (flags == XOCL_XMC_FREEZE) ? "freeze" : "free");
done:
	mutex_unlock(&xmc->mbx_lock);
	return ret;
}

static int xmc_access(struct platform_device *pdev, enum xocl_xmc_flags flags)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	return XOCL_DSA_IS_SMARTN(xdev) ?
		smartnic_cmc_access(pdev, flags) :
		raptor_cmc_access(pdev, flags);
}

static void clock_status_check(struct platform_device *pdev, bool *latched)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	u32 status = 0;

	if (!xmc->sc_presence) {
		/*
		 * On U2, when board temp is above the critical threshold value for 0.5 sec
		 * continuously, CMC firmware turns off the kernel clocks, and sets 0th bit in
		 * XMC_CLOCK_SCALING_CLOCK_STATUS_REG to 1.
		 * So, check if kernel clocks have been stopped.
		 */
		status = READ_RUNTIME_CS(xmc, XMC_CLOCK_SCALING_CLOCK_STATUS_REG);
		if (status & 0x1) {
			xocl_err(&pdev->dev, "Critical temperature event, "
					"kernel clocks have been stopped.");
			/* explicitly indicate reset should be latched */
			*latched = true;
		}
	}
}

static void xmc_get_serial_num(struct platform_device *pdev)
{
	struct xocl_xmc *xmc = platform_get_drvdata(pdev);
	mutex_lock(&xmc->mbx_lock);
	xmc_load_board_info(xmc);
	mutex_unlock(&xmc->mbx_lock);
	if (strlen(xmc->serial_num))
		memcpy(XDEV(xocl_get_xdev(pdev))->serial_num, xmc->serial_num,
			SERIAL_NUM_LEN);
}

static bool xmc_has_dynamic_mac(uint32_t *bdinfo_raw, uint32_t bd_info_sz)
{
	size_t len;
	const char *iomem;

	iomem = xmc_get_board_info(bdinfo_raw, bd_info_sz, BDINFO_MAC_DYNAMIC, &len);

	return iomem != NULL && len == 8;
}

static void xmc_set_dynamic_mac(struct xocl_xmc *xmc, uint32_t *bdinfo_raw,
	uint32_t bd_info_sz)
{
	size_t len = 0;
	const char *iomem;
	u16 num = 0;

	iomem = xmc_get_board_info(bdinfo_raw, bd_info_sz, BDINFO_MAC_DYNAMIC, &len);
	if (len != 8) {
		xocl_err(&xmc->pdev->dev, "dynamic mac data is corrupted.");
		return;
	}

	/*
	 * Byte 0:1 is contiguous mac addresses number in LSB.
	 * Byte 2:7 is first mac address.
	 */
	memcpy(&num, iomem, 2);
	xmc->mac_contiguous_num = le16_to_cpu(num);

	memcpy(xmc->mac_addr_first, iomem+2, 6);
}
	
static void xmc_set_board_info(uint32_t *bdinfo_raw, uint32_t bd_info_sz,
	enum board_info_key key, char *target)
{
	size_t len;
	const char *info;

	info = xmc_get_board_info(bdinfo_raw, bd_info_sz, key, &len);
	if (!info)
		return;

	memcpy(target, info, len);
}

static bool bd_info_valid(char *ser_num)
{
	if (ser_num[0] != 0)
		return true;

	return false;
}

static int xmc_load_board_info(struct xocl_xmc *xmc)
{
	int ret = 0;
	uint32_t bd_info_sz = 0;
	uint32_t *bdinfo_raw;
	xdev_handle_t xdev = xocl_get_xdev(xmc->pdev);
	char *tmp_str = NULL;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));
	if (xmc->bdinfo_loaded)
		return 0;

	if (XMC_PRIVILEGED(xmc)) {

		tmp_str = (char *)xocl_icap_get_data(xdev, EXP_BMC_VER);
		if (tmp_str) {
			strncpy(xmc->exp_bmc_ver, tmp_str,
				sizeof(xmc->exp_bmc_ver) - 1);
		}

		if ((!is_xmc_ready(xmc) || !is_sc_ready(xmc, false)))
			return -EINVAL;

		/* Load new info from HW. */
		memset(&xmc->mbx_pkt, 0, sizeof(xmc->mbx_pkt));
		xmc->mbx_pkt.hdr.op = XPO_BOARD_INFO;
		ret = xmc_send_pkt(xmc);
		if (ret)
			return ret;
		ret = xmc_recv_pkt(xmc);
		if (ret)
			return ret;

		bd_info_sz = xmc->mbx_pkt.hdr.payload_sz;
		bdinfo_raw = vzalloc(bd_info_sz);
		if (bdinfo_raw == NULL)
			return -ENOMEM;
		memcpy(bdinfo_raw, xmc->mbx_pkt.data, bd_info_sz);

		if (xmc_has_dynamic_mac(bdinfo_raw, bd_info_sz)) {
			xmc_set_dynamic_mac(xmc, bdinfo_raw, bd_info_sz);

		} else {
			xmc_set_board_info(bdinfo_raw, bd_info_sz,
				BDINFO_MAC0, xmc->mac_addr0);
			xmc_set_board_info(bdinfo_raw, bd_info_sz,
				BDINFO_MAC1, xmc->mac_addr1);
			xmc_set_board_info(bdinfo_raw, bd_info_sz,
				BDINFO_MAC2, xmc->mac_addr2);
			xmc_set_board_info(bdinfo_raw, bd_info_sz,
				BDINFO_MAC3, xmc->mac_addr3);
		}

		xmc_set_board_info(bdinfo_raw, bd_info_sz,
			BDINFO_SN, xmc->serial_num);
		xmc_set_board_info(bdinfo_raw, bd_info_sz,
			BDINFO_REV, xmc->revision);
		xmc_set_board_info(bdinfo_raw, bd_info_sz,
			BDINFO_NAME, xmc->bd_name);
		xmc_set_board_info(bdinfo_raw, bd_info_sz,
			BDINFO_BMC_VER, xmc->bmc_ver);
		if (!strcmp(xmc->exp_bmc_ver, NONE_BMC_VERSION) ||
                is_sc_fixed(xmc)) {
			/*
			 * No SC image is needed, or SC is beyond control of XRT, 
			 * set expect to be the same as current.
			 */
			xmc_set_board_info(bdinfo_raw, bd_info_sz,
				BDINFO_BMC_VER, xmc->exp_bmc_ver);
		}
		xmc_set_board_info(bdinfo_raw, bd_info_sz,
			BDINFO_MAX_PWR, (char *)&xmc->max_power);
		xmc_set_board_info(bdinfo_raw, bd_info_sz,
			BDINFO_FAN_PRESENCE, (char *)&xmc->fan_presence);
		xmc_set_board_info(bdinfo_raw, bd_info_sz,
			BDINFO_CONFIG_MODE, (char *)&xmc->config_mode);

		if (bd_info_valid(xmc->serial_num) &&
			!strcmp(xmc->bmc_ver, xmc->exp_bmc_ver)) {
			xmc->bdinfo_loaded = true;
			xocl_info(&xmc->pdev->dev, "board info reloaded\n");
		}
		vfree(bdinfo_raw);
	} else {

		ktime_t now = ktime_get_boottime();
		if (xmc->bdinfo_loaded &&
			!strcmp(xmc->bmc_ver, xmc->exp_bmc_ver)) {
			xocl_info(&xmc->pdev->dev, "board info loaded, skip\n");
			return 0;
		}

		/*
		 * If bdinfo is not loaded, probably something uncommon has
		 * happened. keep trying load boardinfo through mailbox doesn't
		 * make sense. In this case, we limit the interval of load
		 * boardinfo to 1s by default
		 */
		if (ktime_compare(now, xmc->bdinfo_retry) < 0)
			return 0;
		xmc->bdinfo_retry = ktime_add(now,
			ktime_set(xmc->cache_expire_secs, 0));

		vfree(xmc->bdinfo_raw);
		xmc->bdinfo_raw = NULL;

		/*
		 * If boardinfo can't be read out, it is likely either PF
		 * mailbox or the CMC mailbox timed out, don't try to read
		 * it again and again in this same context, othereise, the
		 * xocl driver may be considered as hang (120s no response)
		 * defined by
		 * /proc/sys/kernel/hung_task_timeout_secs"
		 */
		read_bdinfo_from_peer(xmc->pdev);
		if (!xmc->bdinfo_raw)
			return -EINVAL;

		xmc_bdinfo(xmc->pdev, SER_NUM, (u32 *)xmc->serial_num);
		xmc_bdinfo(xmc->pdev, MAC_ADDR0, (u32 *)xmc->mac_addr0);
		xmc_bdinfo(xmc->pdev, MAC_ADDR1, (u32 *)xmc->mac_addr1);
		xmc_bdinfo(xmc->pdev, MAC_ADDR2, (u32 *)xmc->mac_addr2);
		xmc_bdinfo(xmc->pdev, MAC_ADDR3, (u32 *)xmc->mac_addr3);
		xmc_bdinfo(xmc->pdev, REVISION, (u32 *)xmc->revision);
		xmc_bdinfo(xmc->pdev, CARD_NAME, (u32 *)xmc->bd_name);
		xmc_bdinfo(xmc->pdev, BMC_VER, (u32 *)xmc->bmc_ver);
		xmc_bdinfo(xmc->pdev, MAX_PWR, &xmc->max_power);
		xmc_bdinfo(xmc->pdev, FAN_PRESENCE, &xmc->fan_presence);
		xmc_bdinfo(xmc->pdev, CFG_MODE, &xmc->config_mode);
		xmc_bdinfo(xmc->pdev, EXP_BMC_VER, (u32 *)xmc->exp_bmc_ver);
		xmc_bdinfo(xmc->pdev, MAC_CONT_NUM, &xmc->mac_contiguous_num);
		xmc_bdinfo(xmc->pdev, MAC_ADDR_FIRST, (u32 *)xmc->mac_addr_first);

		if (bd_info_valid(xmc->serial_num) &&
			!strcmp(xmc->bmc_ver, xmc->exp_bmc_ver)) {
			xmc->bdinfo_loaded = true;
			xocl_info(&xmc->pdev->dev, "board info reloaded\n");
		}
	}
	return 0;
}

static int
xmc_erase_sc_firmware(struct xocl_xmc *xmc)
{
	int ret = 0;

	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));

	if (xmc->sc_fw_erased)
		return 0;

	xocl_info(&xmc->pdev->dev, "erasing SC firmware...");
	memset(&xmc->mbx_pkt, 0, sizeof(xmc->mbx_pkt));
	xmc->mbx_pkt.hdr.op = XPO_MSP432_ERASE_FW;
	ret = xmc_send_pkt(xmc);
	if (ret == 0)
		xmc->sc_fw_erased = true;
	return ret;
}

static int
xmc_write_sc_firmware_section(struct xocl_xmc *xmc, loff_t start,
	size_t n, const char *buf)
{
	int ret = 0;
	size_t sz, thissz;

	xocl_info(&xmc->pdev->dev, "writing %ld bytes @0x%llx", n, start);
	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));
	if (n == 0)
		return 0;

	BUG_ON(!xmc->sc_fw_erased);
	for (sz = 0; ret == 0 && sz < n; sz += thissz) {
		if (sz == 0) {
			/* First packet for the section. */
			xmc->mbx_pkt.hdr.op = XPO_MSP432_SEC_START;
			xmc->mbx_pkt.sector_start.addr = start;
			xmc->mbx_pkt.sector_start.size = n;
			thissz = XMC_PKT_MAX_PAYLOAD_SZ * sizeof(u32) -
				offsetof(struct xmc_pkt_sector_start_op, data);
			thissz = min(thissz, n - sz);
			xmc->mbx_pkt.hdr.payload_sz = thissz +
				offsetof(struct xmc_pkt_sector_start_op, data);
			memcpy(xmc->mbx_pkt.sector_start.data, buf, thissz);
		} else {
			xmc->mbx_pkt.hdr.op = XPO_MSP432_SEC_DATA;
			thissz = XMC_PKT_MAX_PAYLOAD_SZ * sizeof(u32);
			thissz = min(thissz, n - sz);
			xmc->mbx_pkt.hdr.payload_sz = thissz;
			memcpy(xmc->mbx_pkt.sector_data.data, buf + sz, thissz);
		}
		ret = xmc_send_pkt(xmc);
	}

	return ret;
}

static int
xmc_boot_sc(struct xocl_xmc *xmc, u32 jump_addr)
{
	int retry = 0;
	int ret = 0;

	xocl_info(&xmc->pdev->dev, "rebooting SC @0x%x", jump_addr);
	BUG_ON(!mutex_is_locked(&xmc->mbx_lock));
	BUG_ON(!xmc->sc_fw_erased);

	/* Mark new SC firmware is installed. */
	xmc->sc_fw_erased = false;

	/* Try booting it up. */
	xmc->mbx_pkt.hdr.op = XPO_MSP432_IMAGE_END;
	xmc->mbx_pkt.hdr.payload_sz = sizeof(struct xmc_pkt_image_end_op);
	xmc->mbx_pkt.image_end.BSL_jump_addr = jump_addr;
	ret = xmc_send_pkt(xmc);
	if (ret)
		return ret;

	/* Wait for SC to reboot */
	while (retry++ < MAX_XMC_RETRY * 2 && !is_sc_ready(xmc, true))
		msleep(RETRY_INTERVAL);
	if (!is_sc_ready(xmc, false))
		ret = -ETIMEDOUT;

	return ret;
}

static int inline
xmc_qsfp_supported(struct xocl_xmc *xmc)
{
	struct xmc_status status;

	safe_read32(xmc, XMC_STATUS_REG, (u32 *)&status);
	if (status.sc_comm_ver < 6) {
		xocl_info(&xmc->pdev->dev,
			"not supported ver %d", status.sc_comm_ver);
		return 0;
	}

	return 1;
}

static ssize_t
xmc_qsfp_io_read(struct xocl_xmc *xmc, int port, char *buffer, loff_t off, size_t count)
{
	int ret = 0;

	xocl_dbg(&xmc->pdev->dev, "off %lld count %lld", off, count);

	if (!xmc_qsfp_supported(xmc))
		return -EINVAL;

	/* read done, exit */
	if (off >= CMC_OP_IO_CONTROL_MAXLEN) {
		xocl_err(&xmc->pdev->dev, "cannot read more than %d bytes",
			CMC_OP_IO_CONTROL_MAXLEN);
		return -EINVAL;
	}

	mutex_lock(&xmc->mbx_lock);

	memset(&xmc->mbx_pkt, 0, sizeof(xmc->mbx_pkt));
	xmc->mbx_pkt.hdr.op = XPO_QSFP_LOW_SPEED_IO_READ;
	xmc->mbx_pkt.hdr.payload_sz = sizeof(struct xmc_pkt_qsfp_io_rw_op);
	xmc->mbx_pkt.qsfp_io.port = port;
	ret = xmc_send_pkt(xmc);
	if (ret) {
		xocl_info(&xmc->pdev->dev, "send pkt ret %d", ret);
		goto out;
	}
	ret = xmc_recv_pkt(xmc);
	if (ret) {
		xocl_info(&xmc->pdev->dev, "recv pkt ret %d", ret);
		goto out;
	}

	if (xmc->base_addrs[IO_REG]) {
		((u8 *)buffer)[0] = ioread8(xmc->base_addrs[IO_REG] +
			xmc->mbx_offset + CMC_QSFP_IO_DATA_OFFSET);
	}
	mutex_unlock(&xmc->mbx_lock);

	return CMC_OP_IO_CONTROL_MAXLEN;
out:
	mutex_unlock(&xmc->mbx_lock);
	return 0;

}

static ssize_t
xmc_qsfp_io_write(struct xocl_xmc *xmc, int port, char *buffer, loff_t off, size_t count)
{
	int ret = 0;

	xocl_dbg(&xmc->pdev->dev, "off %lld count %lld", off, count);

	if (!xmc_qsfp_supported(xmc) || off != 0)
		return -EINVAL;

	if (off != 0 || count > CMC_OP_IO_CONTROL_MAXLEN) {
		xocl_err(&xmc->pdev->dev, "cannot write more than %d bytes",
			CMC_OP_IO_CONTROL_MAXLEN);
		return -EINVAL;
	}

	mutex_lock(&xmc->mbx_lock);

	memset(&xmc->mbx_pkt, 0, sizeof(xmc->mbx_pkt));
	xmc->mbx_pkt.hdr.op = XPO_QSFP_LOW_SPEED_IO_WRITE;
	xmc->mbx_pkt.hdr.payload_sz = sizeof(struct xmc_pkt_qsfp_io_rw_op);
	xmc->mbx_pkt.qsfp_io.port = port;
	memcpy(xmc->mbx_pkt.qsfp_io.data, buffer, count);
	ret = xmc_send_pkt(xmc);
	if (ret)
		xocl_info(&xmc->pdev->dev, "send pkt ret %d", ret);
	else
		ret = count;

	mutex_unlock(&xmc->mbx_lock);

	return ret;
}

/* read one page into buf */
static int
xmc_qsfp_diag_read_page(struct xocl_xmc *xmc, int port, int page, int level,
	char *buf, int buf_len)
{
	u32 data_size = 0;
	int ret = 0;

	xocl_dbg(&xmc->pdev->dev, "page %d, level %d", page, level);

	if (buf_len > CMC_QSFP_PAGE_SIZE) {
		xocl_err(&xmc->pdev->dev,
			"request size %d is greater than page size %d",
			buf_len, CMC_QSFP_PAGE_SIZE);
		return -EINVAL;
	}
	if (buf_len & 0x3) {
		xocl_err(&xmc->pdev->dev,
			"request size %d is not 4 byte aligned", buf_len);
		return -EINVAL;
	}

	mutex_lock(&xmc->mbx_lock);

	memset(&xmc->mbx_pkt, 0, sizeof(xmc->mbx_pkt));
	xmc->mbx_pkt.hdr.op = XPO_QSFP_DIAG_READ;
	xmc->mbx_pkt.hdr.payload_sz = sizeof(struct xmc_pkt_qsfp_diag_read_op);
	xmc->mbx_pkt.qsfp_diag_r.port = port;
	xmc->mbx_pkt.qsfp_diag_r.page = page;
	xmc->mbx_pkt.qsfp_diag_r.level = level;
	ret = xmc_send_pkt(xmc);
	if (ret) {
		xocl_info(&xmc->pdev->dev, "send pkt ret %d", ret);
		goto out;
	}

	xmc->mbx_pkt.hdr.payload_sz = sizeof(struct xmc_pkt_qsfp_diag_read_op);
	ret = xmc_recv_pkt(xmc);
	if (ret) {
		xocl_info(&xmc->pdev->dev, "recv pkt ret %d", ret);
		goto out;
	}

	data_size = xmc->mbx_pkt.qsfp_diag_r.data_size;
	if (data_size != CMC_QSFP_PAGE_SIZE) {
		xocl_err(&xmc->pdev->dev, "data_size %d is not %d page aligned",
			data_size, CMC_QSFP_PAGE_SIZE);
		ret = -ENODATA;
		goto out;
	}

	if (xmc->base_addrs[IO_REG]) {
		/* only copy request buf_len back to buf */
		xocl_memcpy_fromio(buf, xmc->base_addrs[IO_REG] +
			xmc->mbx_offset + CMC_QSFP_DIAG_DATA_OFFSET, buf_len);
	}

out:
	mutex_unlock(&xmc->mbx_lock);
	return ret;
}

static ssize_t
xmc_qsfp_diag_read(struct xocl_xmc *xmc, int port, char *buffer, loff_t off, size_t count)
{
	int buf_len = CMC_QSFP_PAGE_SIZE;
	char buf[CMC_QSFP_PAGE_SIZE];
	int i, start_page, end_page, global_idx, local_idx, local_len;
	int ret = 0;

	if (!xmc_qsfp_supported(xmc))
		return 0;

	xocl_dbg(&xmc->pdev->dev, "off %lld count %ld\n", off, count);

	/*
	 * Assemble cmc pkg into request buffer.
	 * Spec:
	 *   each request is mapped to a page size 128 bits [0 - 127]
	 *   page layout is: lower-0, upper-0, upper-1, etc... max is up to 255 pages.
	 *   the request buffer starts from off to off+count.
	 */

	/* start includes itself */
	start_page = off / buf_len; 
	end_page = (off + count - 1) / buf_len;
	global_idx = 0;
	local_idx = off % buf_len;
	local_len = min((int)count, (int)(buf_len - off % buf_len));

	xocl_dbg(&xmc->pdev->dev,
		"start_page %d end_page %d local_idx %d local_len %d\n",
		start_page, end_page, local_idx, local_len);

	for (i = start_page; i <= end_page; i++) {
		/* lower page is level 0, upper page is level 1 */
		int level = i == 0 ? 0 : 1;
		/* lower page is page 0, upper page 0 starts from page index 1 */
		int page = i == 0 ? 0 : i - 1;
		int remain = 0;

		xocl_dbg(&xmc->pdev->dev, "index %d", i);

		/* read the whole page */
		ret = xmc_qsfp_diag_read_page(xmc, port, page, level, buf, buf_len); 
		if (ret) 
			return ret;
		/* copy the exact buffer back */
		memcpy(buffer + global_idx, buf + local_idx, local_len);

		global_idx += local_len;
		remain = count - global_idx;
		if (remain >= buf_len) {
			local_idx = 0;
			local_len = buf_len;
		} else {
			local_idx = 0;
			local_len = remain;
		}
		xocl_dbg(&xmc->pdev->dev,
			"global_idx %d remain %d local_idx %d local_len %d\n",
			global_idx, remain, local_idx, local_len);
	}

	return count;
}

/*
 * The CMC_OP_QSFP_I2C_BYTE_READ|WRITE always fill first 256 bytes if
 * page and level set to 0, however if we request offset beyond lower page 0
 * we want exact data at offset location, thus we should pass correct
 * page,level and offset instead of always set page,level to 0.
 */
static void
xmc_qsfp_i2c_count_offset(struct xocl_xmc *xmc, loff_t off,
	u32 *page, u32 *level, u32 *offset)
{
	if (off < 128) {
		*page = 0;
		*level = 0;
		*offset = off;
	} else {
		/* set page, level and skip first 0-127 bytes */
		*page = (off >> 7) - 1;
		*level = 1;
		*offset = (off % 128) + 128;
	}

	xocl_dbg(&xmc->pdev->dev, "page %d, level %d, offset %d",
		*page, *level, *offset);
}

static ssize_t
xmc_qsfp_i2c_read(struct xocl_xmc *xmc, int port, char *buffer, loff_t off, size_t count)
{
	/* current protocl only support one byte */
	u32 page, level, offset;
	/* current protocl only support one byte */
	u32 data_size = sizeof(u8);
	int ret = 0;

	if (!xmc_qsfp_supported(xmc))
		return 0;

	/* we don't limit the offset for reading */
	xocl_dbg(&xmc->pdev->dev, "off %lld count %ld port %d", off, count, port);
	xmc_qsfp_i2c_count_offset(xmc, off, &page, &level, &offset);

	mutex_lock(&xmc->mbx_lock);

	memset(&xmc->mbx_pkt, 0, sizeof(xmc->mbx_pkt));
	xmc->mbx_pkt.hdr.op = XPO_QSFP_I2C_READ;
	xmc->mbx_pkt.hdr.payload_sz = sizeof(struct xmc_pkt_qsfp_byte_rw_op);
	xmc->mbx_pkt.qsfp_byte.port = port;
	xmc->mbx_pkt.qsfp_byte.page = page;
	xmc->mbx_pkt.qsfp_byte.level = level;
	xmc->mbx_pkt.qsfp_byte.offset = offset;
	ret = xmc_send_pkt(xmc);
	if (ret) {
		xocl_info(&xmc->pdev->dev, "send pkt ret %d", ret);
		goto out;
	}

	xmc->mbx_pkt.hdr.payload_sz = sizeof(struct xmc_pkt_qsfp_byte_rw_op);
	ret = xmc_recv_pkt(xmc);
	if (ret) {
		xocl_info(&xmc->pdev->dev, "recv pkt ret %d", ret);
		goto out;
	}

	if (xmc->base_addrs[IO_REG]) {
		((u8 *)buffer)[0] = ioread8(xmc->base_addrs[IO_REG] +
			xmc->mbx_offset + CMC_QSFP_BYTE_DATA_OFFSET);

		xocl_dbg(&xmc->pdev->dev, "buffer 0x%x", buffer[0]);
	}

	mutex_unlock(&xmc->mbx_lock);
	return data_size;
out:
	mutex_unlock(&xmc->mbx_lock);
	return 0;
}

static inline ssize_t
xmc_qsfp_i2c_write(struct xocl_xmc *xmc, int port, char *buffer, loff_t off, size_t count)
{
	u32 page, level, offset;
	int ret = 0;
	size_t max_data_size;
	/* current protocl only support one byte */
	u32 data_size = sizeof(u8);

	if (!xmc_qsfp_supported(xmc))
		return 0;

	/* we don't limit the offset for reading */
	xocl_dbg(&xmc->pdev->dev, "off %lld count %ld port %d", off, count, port);
	xmc_qsfp_i2c_count_offset(xmc, off, &page, &level, &offset);

	max_data_size = XMC_PKT_MAX_PAYLOAD_SZ * sizeof(u32) -
		offsetof(struct xmc_pkt_qsfp_byte_rw_op, data);

	/* Sanity check payload size */
	if (data_size > max_data_size) {
		xocl_err(&xmc->pdev->dev, "cannot write more then %ld bytes",
			max_data_size);
		return -EINVAL;
	}

	mutex_lock(&xmc->mbx_lock);

	memset(&xmc->mbx_pkt, 0, sizeof(xmc->mbx_pkt));
	xmc->mbx_pkt.hdr.op = XPO_QSFP_I2C_WRITE;
	xmc->mbx_pkt.hdr.payload_sz =
		offsetof(struct xmc_pkt_qsfp_byte_rw_op, data) + count;
	xmc->mbx_pkt.qsfp_byte.port = port;
	xmc->mbx_pkt.qsfp_byte.page = page;
	xmc->mbx_pkt.qsfp_byte.level = level;
	xmc->mbx_pkt.qsfp_byte.offset = offset;
	memcpy(xmc->mbx_pkt.qsfp_byte.data, buffer, data_size);

	xocl_dbg(&xmc->pdev->dev, "write 0x%x\n", buffer[0]);

	ret = xmc_send_pkt(xmc);
	if (ret)
		xocl_info(&xmc->pdev->dev, "send pkt ret %d", ret);
	else
		ret = data_size;

	mutex_unlock(&xmc->mbx_lock);

	return ret;
}

/*
 * Write SC firmware image data at specified location.
 */
static ssize_t
xmc_update_sc_firmware(struct file *file, const char __user *ubuf,
	size_t n, loff_t *off)
{
	u32 jump_addr = 0;
	struct xocl_xmc *xmc = file->private_data;
	/* Special offset for writing SC's BSL jump address. */
	const loff_t jump_offset = 0xffffffff;
	ssize_t ret = 0;
	u8 *kbuf;

	mutex_lock(&xmc->xmc_lock);
	if (xmc->state != XMC_STATE_ENABLED) {
		xocl_err(&xmc->pdev->dev, "Card requires pci hot reset\n");
		ret = -EACCES;
	}
	mutex_unlock(&xmc->xmc_lock);

	if (ret)
		return ret;
	/* Sanity check input 'n' */
	if (n == 0 || n > jump_offset || n > 100 * 1024 * 1024)
		return -EINVAL;

	kbuf = vmalloc(n);
	if (kbuf == NULL)
		return -ENOMEM;
	if (copy_from_user(kbuf, ubuf, n)) {
		vfree(kbuf);
		return -EFAULT;
	}

	mutex_lock(&xmc->mbx_lock);

	ret = xmc_erase_sc_firmware(xmc);
	if (ret) {
		xocl_err(&xmc->pdev->dev, "can't erase SC firmware");
	} else if (*off == jump_offset) {
		/*
		 * Write to jump_offset will cause a reboot of SC and jump
		 * to address that is passed in.
		 */
		if (n != sizeof(jump_addr)) {
			xocl_err(&xmc->pdev->dev, "invalid jump addr size");
			ret = -EINVAL;
		} else {
			jump_addr = *(u32 *)kbuf;
			ret = xmc_boot_sc(xmc, jump_addr);
			/* Invalid board info cache after new SC is installed */
			xmc->bdinfo_loaded = false;
		}
	} else {
		ret = xmc_write_sc_firmware_section(xmc, *off, n, kbuf);
	}

	mutex_unlock(&xmc->mbx_lock);
	vfree(kbuf);
	if (ret) {
		xmc->sc_fw_erased = false;
		return ret;
	}

	*off += n;
	return n;
}

/*
 * Only allow one client at a time.
 */
static int xmc_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	struct xocl_xmc *xmc = xocl_drvinst_open(inode->i_cdev);

	if (!xmc)
		return -ENXIO;

	mutex_lock(&xmc->mbx_lock);
	if (xmc->opened) {
		ret = -EBUSY;
	} else {
		file->private_data = xmc;
		xmc->opened = true;
	}
	mutex_unlock(&xmc->mbx_lock);

	if (ret)
		xocl_drvinst_close(xmc);
	return ret;
}

static int xmc_close(struct inode *inode, struct file *file)
{
	struct xocl_xmc *xmc = file->private_data;

	if (!xmc)
		return -EINVAL;

	mutex_lock(&xmc->mbx_lock);
	xmc->opened = false;
	file->private_data = NULL;
	mutex_unlock(&xmc->mbx_lock);

	xocl_drvinst_close(xmc);
	return 0;
}

static loff_t
xmc_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t npos;

	switch(whence) {
	case 0: /* SEEK_SET */
		npos = off;
		break;
	case 1: /* SEEK_CUR */
		npos = filp->f_pos + off;
		break;
	case 2: /* SEEK_END: no need to support */
		return -EINVAL;
	default: /* should not happen */
		return -EINVAL;
	}
	if (npos < 0)
		return -EINVAL;

	filp->f_pos = npos;
	return npos;
}

static const struct file_operations xmc_fops = {
	.owner = THIS_MODULE,
	.open = xmc_open,
	.release = xmc_close,
	.llseek = xmc_llseek,
	.write = xmc_update_sc_firmware,
};
