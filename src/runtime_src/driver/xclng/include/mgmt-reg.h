/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _XCL_MGT_REG_H_
#define _XCL_MGT_REG_H_


#define KB(x)   ((unsigned) (x) << 10)
#define MB(x)   ((unsigned) (x) << 20)

enum PFO_BARS {
  USER_BAR = 0,
  DMA_BAR,
  MAX_BAR
};

/**
 * Peripherals on AXI-Lite mapped to PCIe BAR
 */

#define XILINX_VENDOR_ID 	0x10EE
#define OCL_CU_CTRL_RANGE      	KB(4)

#define DDR_BUFFER_ALIGNMENT   	0x40
#define MMAP_SIZE_USER         	MB(32)

//parameters for HWICAP, Flash and APM on PCIe BAR
#define HWICAP_OFFSET           0x020000
#define AXI_GATE_OFFSET         0x030000
#define AXI_GATE_OFFSET_READ    0x030008
#define BPI_FLASH_OFFSET        0x040000

//Base addresses for LAPC
#define LAPC0_BASE            0x00120000  //ocl master00
#define LAPC1_BASE            0x00121000  //ocl master01
#define LAPC2_BASE            0x00122000  //ocl master02
#define LAPC3_BASE            0x00123000  //ocl master03

//Following status registers are available at each base
#define LAPC_OVERALL_STATUS_OFFSET        0x0
#define LAPC_CUMULATIVE_STATUS_0_OFFSET   0x100
#define LAPC_CUMULATIVE_STATUS_1_OFFSET   0x104
#define LAPC_CUMULATIVE_STATUS_2_OFFSET   0x108
#define LAPC_CUMULATIVE_STATUS_3_OFFSET   0x10c

#define LAPC_SNAPSHOT_STATUS_0_OFFSET     0x200
#define LAPC_SNAPSHOT_STATUS_1_OFFSET     0x204
#define LAPC_SNAPSHOT_STATUS_2_OFFSET     0x208
#define LAPC_SNAPSHOT_STATUS_3_OFFSET     0x20c

// NOTE: monitor address offset now defined by PERFMON0_BASE
#define PERFMON0_OFFSET         0x0
#define PERFMON1_OFFSET         0x020000
#define PERFMON2_OFFSET         0x010000

#define PERFMON_START_OFFSET	0x2000
#define PERFMON_RANGE			0x1000

#define FEATURE_ROM_BASE           0x0B0000
#define OCL_CTLR_BASE              0x000000
#define HWICAP_BASE                0x020000
#define AXI_GATE_BASE              0x030000
#define AXI_GATE_BASE_RD_BASE      0x030008
#define FEATURE_ID_BASE            0x031000
#define GENERAL_STATUS_BASE        0x032000
#define AXI_I2C_BASE               0x041000
#define PERFMON0_BASE              0x100000
#define PERFMON0_BASE2             0x1800000
#define OCL_CLKWIZ0_BASE           0x050000
#define OCL_CLKWIZ1_BASE           0x051000
/* Only needed for workaround for 5.0 platforms */
#define GPIO_NULL_BASE             0x1FFF000


#define OCL_CLKWIZ_STATUS_OFFSET      0x4
#define OCL_CLKWIZ_CONFIG_OFFSET(n)   (0x200 + 4 * (n))

/**
 * AXI Firewall Register definition
 */
#define FIREWALL_MGMT_CONTROL_BASE	0xD0000
#define FIREWALL_USER_CONTROL_BASE	0xE0000
#define FIREWALL_DATAPATH_BASE		0xF0000

#define AF_MI_FAULT_STATUS_OFFSET     	       0x0	//MI Fault Status Register
#define AF_MI_SOFT_CTRL_OFFSET		       0x4	//MI Soft Fault Control Register
#define AF_UNBLOCK_CTRL_OFFSET		       0x8	//MI Unblock Control Register

// Currently un-used regs from the Firewall IP.
#define AF_MAX_CONTINUOUS_RTRANSFERS_WAITS     0x30	//MAX_CONTINUOUS_RTRANSFERS_WAITS
#define AF_MAX_WRITE_TO_BVALID_WAITS           0x34	//MAX_WRITE_TO_BVALID_WAITS
#define AF_MAX_ARREADY_WAITS                   0x38	//MAX_ARREADY_WAITS
#define AF_MAX_AWREADY_WAITS                   0x3c	//MAX_AWREADY_WAITS
#define AF_MAX_WREADY_WAITS                    0x40	//MAX_WREADY_WAITS

/**
 * DDR Zero IP Register definition
 */
//#define ENABLE_DDR_ZERO_IP
#define DDR_ZERO_BASE	           	0x0B0000
#define DDR_ZERO_CONFIG_REG_OFFSET 	0x10
#define DDR_ZERO_CTRL_REG_OFFSET	0x0


/**
 * SYSMON Register definition
 */
#define SYSMON_BASE		0x0A0000
#define SYSMON_TEMP 		0x400 		// TEMPOERATURE REGISTER ADDRESS
#define SYSMON_VCCINT		0x404 		// VCCINT REGISTER OFFSET
#define SYSMON_VCCAUX		0x408 		// VCCAUX REGISTER OFFSET
#define SYSMON_VCCBRAM		0x418 		// VCCBRAM REGISTER OFFSET
#define	SYSMON_TEMP_MAX		0x480
#define	SYSMON_VCCINT_MAX	0x484
#define	SYSMON_VCCAUX_MAX	0x488
#define	SYSMON_VCCBRAM_MAX	0x48c
#define	SYSMON_TEMP_MIN		0x490
#define	SYSMON_VCCINT_MIN	0x494
#define	SYSMON_VCCAUX_MIN	0x498
#define	SYSMON_VCCBRAM_MIN	0x49c

#define	SYSMON_TO_MILLDEGREE(val)		\
	(((int64_t)(val) * 501374 >> 16) - 273678)
#define	SYSMON_TO_MILLVOLT(val)			\
	((val) * 1000 * 3 >> 16)


/**
 * ICAP Register definition
 */

#define XHWICAP_GIER            HWICAP_BASE+0x1c
#define XHWICAP_ISR             HWICAP_BASE+0x20
#define XHWICAP_IER             HWICAP_BASE+0x28
#define XHWICAP_WF              HWICAP_BASE+0x100
#define XHWICAP_RF              HWICAP_BASE+0x104
#define XHWICAP_SZ              HWICAP_BASE+0x108
#define XHWICAP_CR              HWICAP_BASE+0x10c
#define XHWICAP_SR              HWICAP_BASE+0x110
#define XHWICAP_WFV             HWICAP_BASE+0x114
#define XHWICAP_RFO             HWICAP_BASE+0x118
#define XHWICAP_ASR             HWICAP_BASE+0x11c

/* Used for parsing bitstream header */
#define XHI_EVEN_MAGIC_BYTE     0x0f
#define XHI_ODD_MAGIC_BYTE      0xf0

/* Extra mode for IDLE */
#define XHI_OP_IDLE  -1

#define XHI_BIT_HEADER_FAILURE -1

/* The imaginary module length register */
#define XHI_MLR                  15

#define DMA_HWICAP_BITFILE_BUFFER_SIZE 1024

/*
 * Flash programming constants
 * XAPP 518
 * http://www.xilinx.com/support/documentation/application_notes/xapp518-isp-bpi-prom-virtex-6-pcie.pdf
 * Table 1
 */

#define START_ADDR_HI_CMD   0x53420000
#define START_ADDR_CMD      0x53410000
#define END_ADDR_CMD        0x45000000
#define END_ADDR_HI_CMD     0x45420000
#define UNLOCK_CMD          0x556E6C6B
#define ERASE_CMD           0x45726173
#define PROGRAM_CMD         0x50726F67
#define VERSION_CMD         0x55726F73

#define READY_STAT          0x00008000
#define ERASE_STAT          0x00000000
#define PROGRAM_STAT        0x00000080

/*
 * Booting FPGA from PROM
 * http://www.xilinx.com/support/documentation/user_guides/ug470_7Series_Config.pdf
 * Table 7.1
 */

#define DUMMY_WORD         0xFFFFFFFF
#define SYNC_WORD          0xAA995566
#define TYPE1_NOOP         0x20000000
#define TYPE1_WRITE_WBSTAR 0x30020001
#define WBSTAR_ADD10       0x00000000
#define WBSTAR_ADD11       0x01000000
#define TYPE1_WRITE_CMD    0x30008001
#define IPROG_CMD          0x0000000F

/*
 * MicroBlaze definition
 */

#define	MB_REG_BASE		0x120000
#define	MB_GPIO			0x131000
#define	MB_IMAGE_MGMT		0x140000
#define	MB_IMAGE_SCHE		0x160000

#define	MB_REG_VERSION		(MB_REG_BASE)
#define	MB_REG_ID		(MB_REG_BASE + 0x4)
#define	MB_REG_STATUS		(MB_REG_BASE + 0x8)
#define	MB_REG_ERR		(MB_REG_BASE + 0xC)
#define	MB_REG_CAP		(MB_REG_BASE + 0x10)
#define	MB_REG_CTL		(MB_REG_BASE + 0x18)
#define	MB_REG_STOP_CONFIRM	(MB_REG_BASE + 0x1C)
#define	MB_REG_CURR_BASE	(MB_REG_BASE + 0x20)
#define	MB_REG_POW_CHK		(MB_REG_BASE + 0x1A4)

#define	MB_CTL_MASK_STOP		0x8
#define	MB_CTL_MASK_PAUSE		0x4
#define	MB_CTL_MASK_CLEAR_ERR		0x2
#define MB_CTL_MASK_CLEAR_POW		0x1

#define	MB_STATUS_MASK_INIT_DONE	0x1
#define	MB_STATUS_MASK_STOPPED		0x2
#define	MB_STATUS_MASK_PAUSED		0x4

#define	MB_CAP_MASK_PM			0x1

#define	MB_VALID_ID			0x74736574

#define	MB_GPIO_RESET			0x0
#define	MB_GPIO_ENABLED			0x1

#define	MB_SELF_JUMP(ins)		(((ins) & 0xfc00ffff) == 0xb8000000)

/*
 * Interrupt controls
 */
#define XCLMGMT_MAX_INTR_NUM            32
#define XCLMGMT_MAX_USER_INTR           16
#define XCLMGMT_INTR_CTRL_BASE          (0x2000UL)
#define XCLMGMT_INTR_USER_ENABLE        (XCLMGMT_INTR_CTRL_BASE + 0x08)
#define XCLMGMT_INTR_USER_DISABLE       (XCLMGMT_INTR_CTRL_BASE + 0x0C)
#define XCLMGMT_INTR_USER_VECTOR        (XCLMGMT_INTR_CTRL_BASE + 0x80)
#define XCLMGMT_MAILBOX_INTR            11

#endif


