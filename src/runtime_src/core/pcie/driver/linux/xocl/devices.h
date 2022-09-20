/*
 *  Copyright (C) 2018-2022, Xilinx Inc
 *
 *  This file is dual licensed.  It may be redistributed and/or modified
 *  under the terms of the Apache 2.0 License OR version 2 of the GNU
 *  General Public License.
 *
 *  Apache License Verbiage
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  GPL license Verbiage:
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by the Free Software Foundation;
 *  either version 2 of the License, or (at your option) any later version.
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License along with this program;
 *  if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#ifndef	_XCL_DEVICES_H_
#define	_XCL_DEVICES_H_

#include <ert.h>
#include "xocl_fdt.h"
/* board flags */
enum {
	XOCL_DSAFLAG_PCI_RESET_OFF		= (1 << 0),
	XOCL_DSAFLAG_MB_SCHE_OFF		= (1 << 1),
	XOCL_DSAFLAG_AXILITE_FLUSH		= (1 << 2),
	XOCL_DSAFLAG_SET_DSA_VER		= (1 << 3),
	XOCL_DSAFLAG_SET_XPR			= (1 << 4),
	XOCL_DSAFLAG_MFG			= (1 << 5),
	XOCL_DSAFLAG_FIXED_INTR			= (1 << 6),
	XOCL_DSAFLAG_NO_KDMA			= (1 << 7),
	XOCL_DSAFLAG_CUDMA_OFF			= (1 << 8),
	XOCL_DSAFLAG_DYNAMIC_IP			= (1 << 9),
	XOCL_DSAFLAG_SMARTN			= (1 << 10),
	XOCL_DSAFLAG_VERSAL			= (1 << 11),
	XOCL_DSAFLAG_MPSOC			= (1 << 12),
	XOCL_DSAFLAG_CUSTOM_DTB                 = (1 << 13),
	XOCL_DSAFLAG_VERSAL_ES3			= (1 << 14),
};

/* sysmon flags */
enum {
	XOCL_SYSMON_OT_OVERRIDE		= (1 << 0),
};

/* xmc flags */
enum {
	XOCL_XMC_NOSC		= (1 << 0),
	XOCL_XMC_IN_BITFILE	= (1 << 1),
	XOCL_XMC_CLK_SCALING	= (1 << 2),
};

/* icap controller flags */
enum {
	XOCL_IC_FLAT_SHELL	= (1 << 0),
};

#define	FLASH_TYPE_SPI	"spi"
#define	FLASH_TYPE_QSPIPS	"qspi_ps"
#define	FLASH_TYPE_QSPIPS_X2_SINGLE	"qspi_ps_x2_single"
#define	FLASH_TYPE_QSPIPS_X4_SINGLE	"qspi_ps_x4_single"
#define	FLASH_TYPE_OSPI_VERSAL	"ospi_versal"
#define	FLASH_TYPE_QSPI_VERSAL	"qspi_versal"
#define	FLASH_TYPE_OSPI_XGQ	"ospi_xgq"

#define XOCL_SUBDEV_MAX_RES		32
#define XOCL_SUBDEV_RES_NAME_LEN	64
#define XOCL_SUBDEV_MAX_INST		64

enum {
	XOCL_SUBDEV_LEVEL_STATIC = 0,
	XOCL_SUBDEV_LEVEL_BLD,
	XOCL_SUBDEV_LEVEL_PRP,
	XOCL_SUBDEV_LEVEL_URP,
	XOCL_SUBDEV_LEVEL_MAX,
};
struct xocl_subdev_info {
	uint32_t		id;
	const char		*name;
	struct resource		*res;
	int			num_res;
	void			*priv_data;
	int			data_len;
	bool			multi_inst;
	int			level;
	char			*bar_idx;
	int			dyn_ip;
	const char		*override_name;
	int			override_idx;
	int 			dev_idx;
};

struct xocl_board_private {
	uint64_t		flags;
	struct xocl_subdev_info	*subdev_info;
	uint32_t		subdev_num;
	uint32_t		dsa_ver;
	bool			xpr;
	char			*flash_type; /* used by xbflash */
	char			*board_name; /* used by xbflash */
	uint64_t		p2p_bar_sz;
	const char		*vbnv;
	const char		*sched_bin;
};

struct xocl_flash_privdata {
	u32			properties;
	char			flash_type[128];
};

struct xocl_msix_privdata {
	u32			start;
	u32			total;
};

struct xocl_ert_sched_privdata {
	char			dsa;
	int			major;
};

struct xocl_ert_cq_privdata {
	void __iomem            *cq_base;
	uint32_t                 cq_range;
};

struct xocl_sysmon_privdata {
	uint16_t		flags;
};

struct xocl_xmc_privdata {
	uint16_t		flags;
};

struct xocl_icap_cntrl_privdata {
	uint16_t		flags;
};

#define XOCL_P2P_FLAG_SIBASE_NEEDED	1
struct xocl_p2p_privdata {
	u32			flags;
};

#ifdef __KERNEL__
#define XOCL_PCI_DEVID(ven, dev, subsysid, priv)	\
	 .vendor = ven, .device=dev, .subvendor = PCI_ANY_ID, \
	 .subdevice = subsysid, .driver_data =	  \
	 (kernel_ulong_t) &XOCL_BOARD_##priv

enum {
	XOCL_DSAMAP_VBNV,
	XOCL_DSAMAP_DYNAMIC,
	XOCL_DSAMAP_RAPTOR2,
};

struct xocl_dsa_map {
	uint16_t		vendor;
	uint16_t		device;
	uint16_t		subdevice;
	const char			*vbnv;
	struct xocl_board_private	*priv_data;
	uint32_t		type;
};

#else
struct xocl_board_info {
	uint16_t		vendor;
	uint16_t		device;
	uint16_t		subdevice;
	struct xocl_board_private	*priv_data;
};

#define XOCL_PCI_DEVID(ven, dev, subsysid, priv)	\
	 .vendor = ven, .device=dev, \
	 .subdevice = subsysid, .priv_data = &XOCL_BOARD_##priv

struct resource {
	size_t		start;
	size_t		end;
	unsigned long	flags;
};

enum {
	IORESOURCE_MEM,
	IORESOURCE_IRQ,
};

#define	PCI_ANY_ID	-1
#define SUBDEV_SUFFIX
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

#endif

#define	MGMTPF		0
#define	USERPF		1

#if PF == MGMTPF
#define SUBDEV_SUFFIX	".m"
#elif PF == USERPF
#define SUBDEV_SUFFIX	".u"
#endif

#define	XOCL_FEATURE_ROM	"rom"
#define	XOCL_IORES0		"iores0"
#define	XOCL_IORES1		"iores1"
#define	XOCL_IORES2		"iores2"
#define	XOCL_IORES3		"iores3"
#define	XOCL_XDMA		"dma.xdma"
#define	XOCL_QDMA4		"dma.qdma4"
#define	XOCL_QDMA		"dma.qdma"
#define	XOCL_XVC_PUB		"xvc_pub"
#define	XOCL_XVC_PRI		"xvc_pri"
#define	XOCL_NIFD_PRI		"nifd_pri"
#define	XOCL_SYSMON		"sysmon"
#define	XOCL_FIREWALL		"firewall"
#define	XOCL_MB			"microblaze"
#define	XOCL_PS			"processor_system"
#define	XOCL_XIIC		"xiic"
#define	XOCL_MAILBOX		"mailbox"
#define	XOCL_ICAP		"icap"
#define	XOCL_CLOCK_WIZ		"clock_wizard"
#define	XOCL_CLOCK_COUNTER	"clock_freq_counter"
#define	XOCL_AXIGATE		"axigate"
#define	XOCL_MIG		"mig"
#define	XOCL_XMC		"xmc"
#define	XOCL_XMC_U2		"xmc.u2"
#define	XOCL_DNA		"dna"
#define	XOCL_FMGR		"fmgr"
#define	XOCL_FLASH		"flash"
#define	XOCL_DMA_MSIX		"dma_msix"
#define	XOCL_MAILBOX_VERSAL	"mailbox_versal"
#define	XOCL_ERT		"ert"
#define	XOCL_XFER_VERSAL	"xfer_versal"
#define	XOCL_AIM		"aximm_mon"
#define	XOCL_AM			"accel_mon"
#define	XOCL_ASM		"axistream_mon"
#define	XOCL_TRACE_FIFO_LITE	"trace_fifo_lite"
#define	XOCL_TRACE_FIFO_FULL	"trace_fifo_full"
#define	XOCL_TRACE_FUNNEL	"trace_funnel"
#define	XOCL_TRACE_S2MM		"trace_s2mm"
#define	XOCL_LAPC		"lapc"
#define	XOCL_SPC		"spc"
#define	XOCL_MIG_HBM		"mig.hbm"
#define	XOCL_SRSR		"srsr"
#define	XOCL_UARTLITE		"ulite"
#define	XOCL_CALIB_STORAGE	"calib_storage"
#define	XOCL_ADDR_TRANSLATOR	"address_translator"
#define	XOCL_CU			"cu"
#define	XOCL_SCU		"scu"
#define	XOCL_P2P		"p2p"
#define	XOCL_PMC		"pmc"
#define	XOCL_INTC		"intc"
#define	XOCL_ICAP_CNTRL		"icap_controller"
#define	XOCL_VERSION_CTRL	"version_control"
#define	XOCL_MSIX_XDMA		"msix_xdma"
#define	XOCL_ERT_USER		"ert_user"
#define	XOCL_M2M		"m2m"
#define	XOCL_PCIE_FIREWALL	"pcie_firewall"
#define	XOCL_ACCEL_DEADLOCK_DETECTOR	"accel_deadlock"
#define	XOCL_CFG_GPIO		"ert_cfg_gpio"
#define	XOCL_COMMAND_QUEUE	"command_queue"
#define	XOCL_XGQ_VMR		"xgq_vmr"
#define	XOCL_HWMON_SDM		"hwmon_sdm"
#define XOCL_ERT_CTRL           "ert_ctrl"
#define XOCL_ERT_CTRL_VERSAL    "ert_ctrl.versal"

#define XOCL_DEVNAME(str)	str SUBDEV_SUFFIX

enum subdev_id {
	XOCL_SUBDEV_FEATURE_ROM,
	XOCL_SUBDEV_VERSION_CTRL,
	XOCL_SUBDEV_PCIE_FIREWALL,
	XOCL_SUBDEV_AXIGATE,
	XOCL_SUBDEV_MSIX,
	XOCL_SUBDEV_DMA,
	XOCL_SUBDEV_M2M,
	XOCL_SUBDEV_IORES,
	XOCL_SUBDEV_FLASH,
	XOCL_SUBDEV_MAILBOX,
	XOCL_SUBDEV_MAILBOX_VERSAL,
	XOCL_SUBDEV_P2P,
	XOCL_SUBDEV_XVC_PUB,
	XOCL_SUBDEV_XVC_PRI,
	XOCL_SUBDEV_NIFD_PRI,
	XOCL_SUBDEV_SYSMON,
	XOCL_SUBDEV_AF,
	XOCL_SUBDEV_MIG,
	XOCL_SUBDEV_MB,
	XOCL_SUBDEV_PS,
	XOCL_SUBDEV_XIIC,
	XOCL_SUBDEV_ICAP,
	XOCL_SUBDEV_DNA,
	XOCL_SUBDEV_FMGR,
	XOCL_SUBDEV_MIG_HBM,
	XOCL_SUBDEV_XFER_VERSAL,
	XOCL_SUBDEV_CLOCK_WIZ,
	XOCL_SUBDEV_CLOCK_COUNTER,
	XOCL_SUBDEV_AIM,
	XOCL_SUBDEV_AM,
	XOCL_SUBDEV_ASM,
	XOCL_SUBDEV_TRACE_FIFO_LITE,
	XOCL_SUBDEV_TRACE_FIFO_FULL,
	XOCL_SUBDEV_TRACE_FUNNEL,
	XOCL_SUBDEV_TRACE_S2MM,
	XOCL_SUBDEV_SRSR,
	XOCL_SUBDEV_UARTLITE,
	XOCL_SUBDEV_UARTLITE_01,
	XOCL_SUBDEV_CALIB_STORAGE,
	XOCL_SUBDEV_ADDR_TRANSLATOR,
	XOCL_SUBDEV_INTC,
	XOCL_SUBDEV_CU,
	XOCL_SUBDEV_SCU,
	XOCL_SUBDEV_LAPC,
	XOCL_SUBDEV_SPC,
	XOCL_SUBDEV_PMC,
	XOCL_SUBDEV_ICAP_CNTRL,
	XOCL_SUBDEV_ERT_USER,
	XOCL_SUBDEV_ERT_VERSAL,
	XOCL_SUBDEV_ACCEL_DEADLOCK_DETECTOR,
	XOCL_SUBDEV_CFG_GPIO,
	XOCL_SUBDEV_COMMAND_QUEUE,
	XOCL_SUBDEV_XGQ_VMR,
	XOCL_SUBDEV_HWMON_SDM,
	XOCL_SUBDEV_ERT_CTRL,
	XOCL_SUBDEV_NUM
};

#define	XOCL_SUBDEV_MAP_USERPF_ONLY		0x1
struct xocl_subdev_res {
	const char *res_name; 		/* resource ep name, e.g. ep_xdma_00 */
	const char *regmap_name;	/* compatible ip, e.g. axi_hwicap */
};

struct xocl_subdev_map {
	int	id;
	const char *dev_name;
	struct xocl_subdev_res *res_array;
	u32	required_ip;
	u32	flags;
	void	*(*build_priv_data)(void *dev_hdl, void *subdev, size_t *len);
	void	(*devinfo_cb)(void *dev_hdl, void *subdevs, int num);
	u32	min_level;
	u32	max_level;
};

#define	XOCL_RES_FEATURE_ROM				\
		((struct resource []) {			\
			{				\
			.start	= 0xB0000,		\
			.end	= 0xB0FFF,		\
			.flags	= IORESOURCE_MEM,	\
			}				\
		})

#define	XOCL_DEVINFO_FEATURE_ROM			\
	{						\
		XOCL_SUBDEV_FEATURE_ROM,		\
		XOCL_FEATURE_ROM,			\
		XOCL_RES_FEATURE_ROM,			\
		ARRAY_SIZE(XOCL_RES_FEATURE_ROM),	\
		.override_idx = -1,			\
	}

#define	XOCL_RES_VERSION_CTRL			\
		((struct resource []) {			\
			{				\
			.start	= 0x0330000,		\
			.end	= 0x0330010,		\
			.flags	= IORESOURCE_MEM,	\
			}				\
		})

#define	XOCL_DEVINFO_VERSION_CTRL		\
	{						\
		XOCL_SUBDEV_VERSION_CTRL,		\
		XOCL_VERSION_CTRL,			\
		XOCL_RES_VERSION_CTRL,		\
		ARRAY_SIZE(XOCL_RES_VERSION_CTRL),	\
		.override_idx = -1,			\
	}

#define	XOCL_RES_FEATURE_ROM_VERSAL			\
		((struct resource []) {			\
			{				\
			.start	= 0x6000000,		\
			.end	= 0x600FFFF,		\
			.flags	= IORESOURCE_MEM,	\
			}				\
		})

#define	XOCL_DEVINFO_FEATURE_ROM_VERSAL			\
	{						\
		XOCL_SUBDEV_FEATURE_ROM,		\
		XOCL_FEATURE_ROM,			\
		XOCL_RES_FEATURE_ROM_VERSAL,		\
		ARRAY_SIZE(XOCL_RES_FEATURE_ROM_VERSAL),	\
		.override_idx = -1,			\
	}

#define	XOCL_RES_FEATURE_ROM_SMARTN			\
		((struct resource []) {			\
			{				\
			.start	= 0x122000,		\
			.end	= 0x122FFF,		\
			.flags	= IORESOURCE_MEM,	\
			}				\
		})


#define	XOCL_DEVINFO_FEATURE_ROM_SMARTN			\
	{						\
		XOCL_SUBDEV_FEATURE_ROM,		\
		XOCL_FEATURE_ROM,			\
		XOCL_RES_FEATURE_ROM_SMARTN,		\
		ARRAY_SIZE(XOCL_RES_FEATURE_ROM_SMARTN),\
		.override_idx = -1,			\
	}


#define	XOCL_RES_SYSMON					\
		((struct resource []) {			\
			{				\
			.start	= 0xA0000,		\
			.end 	= 0xAFFFF,		\
			.flags  = IORESOURCE_MEM,	\
			}				\
		})

#define	XOCL_DEVINFO_SYSMON				\
	{						\
		XOCL_SUBDEV_SYSMON,			\
		XOCL_SYSMON,				\
		XOCL_RES_SYSMON,			\
		ARRAY_SIZE(XOCL_RES_SYSMON),		\
		.override_idx = -1,			\
	}

#define XOCL_PRIV_SYSMON_U2				\
	((struct xocl_sysmon_privdata){			\
		XOCL_SYSMON_OT_OVERRIDE,		\
	 })

#define	XOCL_DEVINFO_SYSMON_U2		\
	{						\
		XOCL_SUBDEV_SYSMON,			\
		XOCL_SYSMON,				\
		XOCL_RES_SYSMON,			\
		ARRAY_SIZE(XOCL_RES_SYSMON),		\
		.override_idx = -1,			\
		.priv_data = &XOCL_PRIV_SYSMON_U2,	\
		.data_len = sizeof(struct xocl_sysmon_privdata), \
	}

/* Will be populated dynamically */
#define	XOCL_RES_MIG					\
		((struct resource []) {			\
			{				\
			.start	= 0x0,			\
			.end 	= 0x3FF,		\
			.flags  = IORESOURCE_MEM,	\
			}				\
		})

#define	XOCL_DEVINFO_MIG				\
	{						\
		XOCL_SUBDEV_MIG,			\
		XOCL_MIG,				\
		XOCL_RES_MIG,				\
		ARRAY_SIZE(XOCL_RES_MIG),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_RES_MIG_HBM				\
		((struct resource []) {			\
			{				\
			.start	= 0x3C00,		\
			.end 	= 0x58FF,		\
			.flags  = IORESOURCE_MEM,	\
			}				\
		})

#define	XOCL_DEVINFO_MIG_HBM				\
	{						\
		XOCL_SUBDEV_MIG,			\
		XOCL_MIG_HBM,				\
		XOCL_RES_MIG_HBM,			\
		ARRAY_SIZE(XOCL_RES_MIG_HBM),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_RES_AF					\
		((struct resource []) {			\
			{				\
			.start	= 0xD0000,		\
			.end 	= 0xDFFFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0xE0000,		\
			.end 	= 0xEFFFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0xF0000,		\
			.end 	= 0xFFFFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
		})

#define	XOCL_DEVINFO_AF					\
	{						\
		XOCL_SUBDEV_AF,				\
		XOCL_FIREWALL,				\
		XOCL_RES_AF,				\
		ARRAY_SIZE(XOCL_RES_AF),		\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_AF_USER				\
	{						\
		XOCL_SUBDEV_AF,				\
		XOCL_FIREWALL,				\
		NULL,					\
		0,					\
		.override_idx = -1,			\
	}

#define	XOCL_RES_AF_DSA52				\
		((struct resource []) {			\
			{				\
			.start	= 0xD0000,		\
			.end 	= 0xDFFFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0xE0000,		\
			.end 	= 0xE0FFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0xE1000,		\
			.end 	= 0xE1FFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0xF0000,		\
			.end 	= 0xFFFFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
		})

#define	XOCL_DEVINFO_AF_DSA52				\
	{						\
		XOCL_SUBDEV_AF,				\
		XOCL_FIREWALL,				\
		XOCL_RES_AF_DSA52,			\
		ARRAY_SIZE(XOCL_RES_AF_DSA52),		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_XVC_PUB				\
	((struct resource []) {				\
		{					\
			.start	= 0xC0000,		\
			.end	= 0xCFFFF,		\
			.flags	= IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_XVC_PUB				\
	{						\
		XOCL_SUBDEV_XVC_PUB,			\
		XOCL_XVC_PUB,				\
		XOCL_RES_XVC_PUB,			\
		ARRAY_SIZE(XOCL_RES_XVC_PUB),		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_XVC_PRI				\
	((struct resource []) {				\
		{					\
			.start	= 0x1C0000,		\
			.end	= 0x1CFFFF,		\
			.flags	= IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_XVC_PRI				\
	{						\
		XOCL_SUBDEV_XVC_PRI,			\
		XOCL_XVC_PRI,				\
		XOCL_RES_XVC_PRI,			\
		ARRAY_SIZE(XOCL_RES_XVC_PRI),		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_NIFD_PRI				\
	((struct resource []) {				\
		{					\
			.start	= 0x28000,		\
			.end	= 0x2cfff,		\
			.flags	= IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_NIFD_PRI				\
	{						\
		XOCL_SUBDEV_NIFD_PRI,			\
		XOCL_NIFD_PRI,				\
		XOCL_RES_NIFD_PRI,			\
		ARRAY_SIZE(XOCL_RES_NIFD_PRI),		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_XIIC					\
	((struct resource []) {				\
		{					\
			.start	= 0x41000,		\
			.end	= 0x41FFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_XIIC				\
	{						\
		XOCL_SUBDEV_XIIC,			\
		XOCL_XIIC,				\
		XOCL_RES_XIIC,				\
		ARRAY_SIZE(XOCL_RES_XIIC),		\
		.override_idx = -1,			\
	}


/* Will be populated dynamically */
#define	XOCL_RES_DNA					\
	((struct resource []) {				\
		{					\
			.start	= 0x0,			\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		}					\
	})

#define	XOCL_DEVINFO_DNA				\
	{						\
		XOCL_SUBDEV_DNA,			\
		XOCL_DNA,				\
		XOCL_RES_DNA,				\
		ARRAY_SIZE(XOCL_RES_DNA),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_AIM					\
	((struct resource []) {				\
		{					\
			.start	= 0x0,			\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_AIM				\
	{						\
		XOCL_SUBDEV_AIM,			\
		XOCL_AIM,				\
		XOCL_RES_AIM,			\
		ARRAY_SIZE(XOCL_RES_AIM),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_RES_AM					\
	((struct resource []) {				\
		{					\
			.name   = "ACCEL_MONITOR",	\
			.start	= 0x0,			\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_AM				\
	{						\
		XOCL_SUBDEV_AM,			\
		XOCL_AM,				\
		XOCL_RES_AM,			\
		ARRAY_SIZE(XOCL_RES_AM),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}


#define	XOCL_RES_ASM					\
	((struct resource []) {				\
		{					\
			.name   = "AXI_STREAM_MONITOR",	\
			.start	= 0x0,			\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_ASM				\
	{						\
		XOCL_SUBDEV_ASM,			\
		XOCL_ASM,				\
		XOCL_RES_ASM,			\
		ARRAY_SIZE(XOCL_RES_ASM),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_RES_TRACE_FIFO_LITE			\
	((struct resource []) {				\
		{					\
			.name   = "TRACE_FIFO_LITE",	\
			.start	= 0x0,			\
			.end	= 0x1FFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_TRACE_FIFO_LITE				\
	{						\
		XOCL_SUBDEV_TRACE_FIFO_LITE,			\
		XOCL_TRACE_FIFO_LITE,				\
		XOCL_RES_TRACE_FIFO_LITE,			\
		ARRAY_SIZE(XOCL_RES_TRACE_FIFO_LITE),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_TRACE_FIFO_FULL				\
	{						\
		XOCL_SUBDEV_TRACE_FIFO_FULL,			\
		XOCL_TRACE_FIFO_FULL,				\
		NULL,			\
		0,		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

/* Fake resource for HLS CU
 * Res 0: CU registers on AXI-Lite
 */
#define XOCL_RES_CU					\
	((struct resource []) {				\
		{					\
			.start	= 0x0,			\
			.end	= 0x00FFF,		\
			.flags	= IORESOURCE_MEM,	\
		}					\
	})

#define XOCL_DEVINFO_CU					\
	{						\
		XOCL_SUBDEV_CU,				\
		XOCL_CU,				\
		XOCL_RES_CU,				\
		ARRAY_SIZE(XOCL_RES_CU),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

/* Fake resource for FIFO base CU
 * Res 0: CU registers on AXI-Lite
 * Res 1: CU argument ram on AXI-full (hard code for U.2)
 */
#define XOCL_RES_CU_PLRAM				\
	((struct resource []) {				\
		{					\
			.start	= 0x0,			\
			.end	= 0x0FFFF,		\
			.flags	= IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 0x100000000,		\
			.end	= 0x1000FFFFF,		\
			.flags	= IORESOURCE_MEM,	\
		}					\
	})

#define XOCL_DEVINFO_CU_PLRAM				\
	{						\
		XOCL_SUBDEV_CU,				\
		XOCL_CU,				\
		XOCL_RES_CU_PLRAM,			\
		ARRAY_SIZE(XOCL_RES_CU_PLRAM),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
		.bar_idx = (char []){ 0, 4 },		\
	}

/* Fake resource for PS Kernels
 */
#define XOCL_DEVINFO_SCU					\
	{						\
		XOCL_SUBDEV_SCU,				\
		XOCL_SCU,				\
		NULL,				\
		0,		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_RES_TRACE_FUNNEL			\
	((struct resource []) {				\
		{					\
			.name   = "TRACE_FUNNEL",	\
			.start	= 0x0,			\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_TRACE_FUNNEL				\
	{						\
		XOCL_SUBDEV_TRACE_FUNNEL,			\
		XOCL_TRACE_FUNNEL,				\
		XOCL_RES_TRACE_FUNNEL,			\
		ARRAY_SIZE(XOCL_RES_TRACE_FUNNEL),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_RES_TRACE_S2MM			\
	((struct resource []) {				\
		{					\
			.name   = "TRACE_S2MM",	\
			.start	= 0x0,			\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_TRACE_S2MM				\
	{						\
		XOCL_SUBDEV_TRACE_S2MM,			\
		XOCL_TRACE_S2MM,				\
		XOCL_RES_TRACE_S2MM,			\
		ARRAY_SIZE(XOCL_RES_TRACE_S2MM),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_RES_ACCEL_DEADLOCK_DETECTOR			\
	((struct resource []) {				\
		{					\
			.name   = "ACCEL_DEADLOCK_DETECTOR",	\
			.start	= 0x0,			\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_ACCEL_DEADLOCK_DETECTOR				\
	{						\
		XOCL_SUBDEV_ACCEL_DEADLOCK_DETECTOR,			\
		XOCL_ACCEL_DEADLOCK_DETECTOR,				\
		XOCL_RES_ACCEL_DEADLOCK_DETECTOR,			\
		ARRAY_SIZE(XOCL_RES_ACCEL_DEADLOCK_DETECTOR),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}


#define	XOCL_RES_LAPC			\
	((struct resource []) {				\
		{					\
			.name   = "LAPC",	\
			.start	= 0x0,			\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_LAPC				\
	{						\
		XOCL_SUBDEV_LAPC,			\
		XOCL_LAPC,				\
		XOCL_RES_LAPC,			\
		ARRAY_SIZE(XOCL_RES_LAPC),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_RES_SPC			\
	((struct resource []) {				\
		{					\
			.name   = "SPC",	\
			.start	= 0x0,			\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_SPC				\
	{						\
		XOCL_SUBDEV_SPC,			\
		XOCL_SPC,				\
		XOCL_RES_SPC,			\
		ARRAY_SIZE(XOCL_RES_SPC),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
		.override_idx = -1,			\
	}

#define	XOCL_RES_ADDR_TRANSLATOR					\
	((struct resource []) {				\
		{					\
			.start	= 0,		\
			.end	= 0xFFF,		\
			.flags  = IORESOURCE_MEM,	\
		}					\
	})

#define	XOCL_DEVINFO_ADDR_TRANSLATOR				\
	{						\
		XOCL_SUBDEV_ADDR_TRANSLATOR,			\
		XOCL_ADDR_TRANSLATOR,				\
		XOCL_RES_ADDR_TRANSLATOR,				\
		ARRAY_SIZE(XOCL_RES_ADDR_TRANSLATOR),		\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_SRSR					\
	((struct resource []) {				\
		{					\
			.start	= 0x0,			\
			.end	= 0x7FFF,		\
			.flags  = IORESOURCE_MEM,	\
		}					\
	})

#define	XOCL_DEVINFO_SRSR				\
	{						\
		XOCL_SUBDEV_SRSR,			\
		XOCL_SRSR,				\
		XOCL_RES_SRSR,				\
		ARRAY_SIZE(XOCL_RES_SRSR),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.override_idx = -1,			\
	}


#define	XOCL_DEVINFO_CALIB_STORAGE			\
	{						\
		XOCL_SUBDEV_CALIB_STORAGE,		\
		XOCL_CALIB_STORAGE,			\
		NULL,					\
		0,					\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = -1,			\
	}


#define	XOCL_MAILBOX_OFFSET_MGMT	0x210000
#define	XOCL_RES_MAILBOX_MGMT				\
	((struct resource []) {				\
		{					\
			.start	= XOCL_MAILBOX_OFFSET_MGMT, \
			.end	= 0x21002F,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 11,			\
			.end	= 11,			\
			.flags  = IORESOURCE_IRQ,	\
		},					\
	})

#define	XOCL_DEVINFO_MAILBOX_MGMT			\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_MAILBOX_MGMT,			\
		ARRAY_SIZE(XOCL_RES_MAILBOX_MGMT),	\
		.override_idx = -1,			\
	}

#define	XOCL_MAILBOX_OFFSET_MGMT_VERSAL		0x6050000
#define	XOCL_RES_MAILBOX_MGMT_VERSAL				\
	((struct resource []) {				\
		{					\
			.start	= XOCL_MAILBOX_OFFSET_MGMT_VERSAL, \
			.end	= 0x605002F,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 3,			\
			.end	= 3,			\
			.flags  = IORESOURCE_IRQ,	\
		},					\
	})

#define	XOCL_DEVINFO_MAILBOX_MGMT_VERSAL		\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_MAILBOX_MGMT_VERSAL,		\
		ARRAY_SIZE(XOCL_RES_MAILBOX_MGMT_VERSAL),	\
		.override_idx = -1,			\
	}

#define	XOCL_RES_MAILBOX_MGMT_QDMA				\
	((struct resource []) {				\
		{					\
			.start	= XOCL_MAILBOX_OFFSET_MGMT, \
			.end	= 0x21002F,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 1,			\
			.end	= 1,			\
			.flags  = IORESOURCE_IRQ,	\
		},					\
	})

#define	XOCL_DEVINFO_MAILBOX_MGMT_QDMA			\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_MAILBOX_MGMT_QDMA,		\
		ARRAY_SIZE(XOCL_RES_MAILBOX_MGMT_QDMA),	\
		.override_idx = -1,			\
	}

#define	XOCL_MAILBOX_OFFSET_USER	0x200000
#define	XOCL_RES_MAILBOX_USER				\
	((struct resource []) {				\
		{					\
			.start	= XOCL_MAILBOX_OFFSET_USER, \
			.end	= 0x20002F,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 4,			\
			.end	= 4,			\
			.flags  = IORESOURCE_IRQ,	\
		},					\
	})

#define	XOCL_DEVINFO_MAILBOX_USER			\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_MAILBOX_USER,			\
		ARRAY_SIZE(XOCL_RES_MAILBOX_USER),	\
		.override_idx = -1,			\
	}

#define	XOCL_RES_MAILBOX_USER_QDMA			\
	((struct resource []) {				\
		{					\
			.start	= XOCL_MAILBOX_OFFSET_USER, \
			.end	= 0x20002F,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 1,			\
			.end	= 1,			\
			.flags  = IORESOURCE_IRQ,	\
		},					\
	})

#define	XOCL_DEVINFO_MAILBOX_USER_QDMA			\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_MAILBOX_USER_QDMA,		\
		ARRAY_SIZE(XOCL_RES_MAILBOX_USER_QDMA),	\
		.override_idx = -1,			\
	}

#define	XOCL_PF_MAILBOX_OFFSET_USER_VERSAL	0x6010000
#define	XOCL_RES_PF_MAILBOX_USER_VERSAL				\
	((struct resource []) {				\
		{					\
			.start	= XOCL_PF_MAILBOX_OFFSET_USER_VERSAL, \
			.end	= 0x601002F,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 4,			\
			.end	= 4,			\
			.flags  = IORESOURCE_IRQ,	\
		},					\
	})

#define	XOCL_DEVINFO_PF_MAILBOX_USER_VERSAL		\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_PF_MAILBOX_USER_VERSAL,	\
		ARRAY_SIZE(XOCL_RES_PF_MAILBOX_USER_VERSAL),	\
		.override_idx = -1,			\
	}

#define	XOCL_MAILBOX_OFFSET_USER_VERSAL	0x6040000
#define	XOCL_MAILBOX_USER_VERSAL_SIZE	0x2f
#define	XOCL_RES_MAILBOX_USER_VERSAL			\
	((struct resource []) {				\
		{					\
			.start	= XOCL_MAILBOX_OFFSET_USER_VERSAL, \
			.end	= XOCL_MAILBOX_OFFSET_USER_VERSAL +	\
	 			XOCL_MAILBOX_USER_VERSAL_SIZE,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_MAILBOX_USER_VERSAL		\
	{						\
		XOCL_SUBDEV_MAILBOX_VERSAL,		\
		XOCL_MAILBOX_VERSAL,			\
		XOCL_RES_MAILBOX_USER_VERSAL,		\
		ARRAY_SIZE(XOCL_RES_MAILBOX_USER_VERSAL),	\
		.override_idx = -1,			\
	}

#define	XOCL_RES_ICAP_MGMT				\
	((struct resource []) {				\
		{					\
			.start	= 0x020000,		\
			.end	= 0x020119,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_ICAP_MGMT				\
	{						\
		XOCL_SUBDEV_ICAP,			\
		XOCL_ICAP,				\
		XOCL_RES_ICAP_MGMT,			\
		ARRAY_SIZE(XOCL_RES_ICAP_MGMT),		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_ICAP_MGMT_SMARTN				\
	((struct resource []) {				\
		{					\
			.start	= 0x100000,		\
			.end	= 0x100119,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_ICAP_MGMT_SMARTN			\
	{						\
		XOCL_SUBDEV_ICAP,			\
		XOCL_ICAP,				\
		XOCL_RES_ICAP_MGMT_SMARTN,		\
		ARRAY_SIZE(XOCL_RES_ICAP_MGMT_SMARTN),	\
		.override_idx = -1,			\
	}

#define __RES_CLOCK_K1					\
		{					\
			.name	= RESNAME_CLKWIZKERNEL1,\
			.start	= 0x050000,		\
			.end	= 0x050fff,		\
			.flags  = IORESOURCE_MEM,	\
		}

#define __RES_CLOCK_K2					\
		{					\
			.name	= RESNAME_CLKWIZKERNEL2,\
			.start	= 0x051000,		\
			.end	= 0x051fff,		\
			.flags  = IORESOURCE_MEM,	\
		}

#define __RES_CLOCK_HBM					\
		{					\
			.name	= RESNAME_CLKWIZKERNEL3,\
			.start	= 0x053000,		\
			.end	= 0x053fff,		\
			.flags  = IORESOURCE_MEM,	\
		}

#define __RES_CLKFREQ_K1_K2				\
		/* OCL_CLKFREQ_BASE */			\
		{					\
			.name	= RESNAME_CLKFREQ_K1_K2,\
			.start	= 0x052000,		\
			.end	= 0x052fff,		\
			.flags  = IORESOURCE_MEM,	\
		}

#define __RES_CLKFREQ_HBM				\
		/* OCL_CLKFREQ_BASE */			\
		{					\
			.name	= RESNAME_CLKFREQ_HBM,	\
			.start	= 0x055000,		\
			.end	= 0x055fff,		\
			.flags  = IORESOURCE_MEM,	\
		}

#define __RES_CLKFREQ_K1_K2_STATIC			\
		/* OCL_CLKFREQ_BASE */			\
		{					\
			.name	= RESNAME_CLKFREQ_K1_K2,\
			.start	= 0x1000000,		\
			.end	= 0x1000fff,		\
			.flags  = IORESOURCE_MEM,	\
		}

#define __RES_CLKFREQ_HBM_STATIC			\
		/* OCL_CLKFREQ_BASE */			\
		{					\
			.name	= RESNAME_CLKFREQ_HBM,	\
			.start	= 0x1001000,		\
			.end	= 0x1001fff,		\
			.flags  = IORESOURCE_MEM,	\
		}


#define XOCL_RES_CLOCK_WIZ_LEGACY			\
	((struct resource []) {				\
		__RES_CLOCK_K1,				\
		__RES_CLOCK_K2,				\
	 })

#define XOCL_RES_CLOCK_COUNTER_LEGACY			\
	((struct resource []) {				\
		__RES_CLKFREQ_K1_K2,			\
	 })

#define XOCL_DEVINFO_CLOCK_WIZ_LEGACY			\
	{						\
		XOCL_SUBDEV_CLOCK_WIZ,			\
		XOCL_CLOCK_WIZ,				\
		XOCL_RES_CLOCK_WIZ_LEGACY,		\
		ARRAY_SIZE(XOCL_RES_CLOCK_WIZ_LEGACY),	\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = XOCL_SUBDEV_LEVEL_PRP,	\
	}

#define XOCL_DEVINFO_CLOCK_COUNTER_LEGACY		\
	{						\
		XOCL_SUBDEV_CLOCK_COUNTER,		\
		XOCL_CLOCK_COUNTER,			\
		XOCL_RES_CLOCK_COUNTER_LEGACY,		\
		ARRAY_SIZE(XOCL_RES_CLOCK_COUNTER_LEGACY),\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = XOCL_SUBDEV_LEVEL_PRP,	\
	}

#define XOCL_DEVINFO_CLOCK_LEGACY			\
		XOCL_DEVINFO_CLOCK_WIZ_LEGACY,		\
		XOCL_DEVINFO_CLOCK_COUNTER_LEGACY

#define __RES_PRP_IORES_MGMT				\
		{					\
			.name	= RESNAME_MEMCALIB,	\
			.start	= 0x032000,		\
			.end	= 0x032003,		\
			.flags  = IORESOURCE_MEM,	\
		}					\

#define __RES_PRP_IORES_MGMT_SMARTN			\
		{					\
			.name	= RESNAME_MEMCALIB,	\
			.start	= 0x135000,		\
			.end	= 0x135003,		\
			.flags  = IORESOURCE_MEM,	\
		}					\

#define XOCL_RES_PRP_IORES_MGMT				\
	((struct resource []) {				\
	 __RES_PRP_IORES_MGMT,				\
	})

#define	XOCL_DEVINFO_PRP_IORES_MGMT			\
	{						\
		XOCL_SUBDEV_IORES,			\
		XOCL_IORES2,				\
		XOCL_RES_PRP_IORES_MGMT,		\
		ARRAY_SIZE(XOCL_RES_PRP_IORES_MGMT),	\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = XOCL_SUBDEV_LEVEL_PRP,	\
	}

#define XOCL_RES_AXIGATE_ULP				\
	((struct resource []) {				\
		{					\
			.name	= RESNAME_GATEULP,	\
			.start	= 0x030000,		\
			.end	= 0x03000b,		\
			.flags  = IORESOURCE_MEM,	\
		}					\
	 })

#define XOCL_DEVINFO_AXIGATE_ULP			\
	{						\
		XOCL_SUBDEV_AXIGATE,			\
		XOCL_AXIGATE,				\
		XOCL_RES_AXIGATE_ULP,			\
		ARRAY_SIZE(XOCL_RES_AXIGATE_ULP),	\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = XOCL_SUBDEV_LEVEL_PRP,	\
	}

#define XOCL_RES_AXIGATE_ULP_SMARTN			\
	((struct resource []) {				\
		{					\
			.name	= RESNAME_GATEULP,	\
			.start	= 0x134000,		\
			.end	= 0x13400b,		\
			.flags  = IORESOURCE_MEM,	\
		}					\
	 })

#define XOCL_DEVINFO_AXIGATE_ULP_SMARTN			\
	{						\
		XOCL_SUBDEV_AXIGATE,			\
		XOCL_AXIGATE,				\
		XOCL_RES_AXIGATE_ULP_SMARTN,		\
		ARRAY_SIZE(XOCL_RES_AXIGATE_ULP_SMARTN),\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = XOCL_SUBDEV_LEVEL_PRP,	\
	}

#define XOCL_RES_CLOCK_WIZ_HBM				\
    ((struct resource []) {	     			\
	__RES_CLOCK_K1,					\
	__RES_CLOCK_K2,					\
	__RES_CLOCK_HBM,				\
     })

#define XOCL_RES_CLOCK_COUNTER_HBM			\
    ((struct resource []) {	     			\
	__RES_CLKFREQ_K1_K2,				\
	__RES_CLKFREQ_HBM,				\
     })

#define XOCL_DEVINFO_CLOCK_WIZ_HBM			\
     {							\
	 XOCL_SUBDEV_CLOCK_WIZ,				\
	 XOCL_CLOCK_WIZ,				\
	 XOCL_RES_CLOCK_WIZ_HBM,			\
	 ARRAY_SIZE(XOCL_RES_CLOCK_WIZ_HBM),		\
	 .level = XOCL_SUBDEV_LEVEL_PRP,		\
	 .override_idx = XOCL_SUBDEV_LEVEL_PRP,	 	\
     }

#define XOCL_DEVINFO_CLOCK_COUNTER_HBM			\
     {							\
	 XOCL_SUBDEV_CLOCK_COUNTER,			\
	 XOCL_CLOCK_COUNTER,				\
	 XOCL_RES_CLOCK_COUNTER_HBM,			\
	 ARRAY_SIZE(XOCL_RES_CLOCK_COUNTER_HBM),	\
	 .level = XOCL_SUBDEV_LEVEL_PRP,		\
	 .override_idx = XOCL_SUBDEV_LEVEL_PRP,	 	\
     }

#define XOCL_DEVINFO_CLOCK_HBM				\
		XOCL_DEVINFO_CLOCK_WIZ_HBM,		\
		XOCL_DEVINFO_CLOCK_COUNTER_HBM		\
	

#define XOCL_RES_PRP_IORES_MGMT_SMARTN			\
	((struct resource []) {				\
	 __RES_PRP_IORES_MGMT_SMARTN,			\
	})

#define XOCL_DEVINFO_PRP_IORES_MGMT_SMARTN		\
	{						\
		XOCL_SUBDEV_IORES,			\
		XOCL_IORES2,				\
		XOCL_RES_PRP_IORES_MGMT_SMARTN,		\
		ARRAY_SIZE(XOCL_RES_PRP_IORES_MGMT_SMARTN),	\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = XOCL_SUBDEV_LEVEL_PRP,	\
	}

#define	XOCL_DEVINFO_ICAP_USER				\
	{						\
		XOCL_SUBDEV_ICAP,			\
		XOCL_ICAP,				\
		NULL,					\
		0,					\
		.override_idx = -1,			\
	}

#define XOCL_PRIV_XMC_U2			\
	((struct xocl_xmc_privdata){		\
		.flags	= XOCL_XMC_NOSC,	\
	 })

#define __RES_XMC			\
	{				\
	.start	= 0x120000,		\
	.end 	= 0x121FFF,		\
	.flags  = IORESOURCE_MEM,	\
	},				\
	{				\
	.start	= 0x131000,		\
	.end 	= 0x131FFF,		\
	.flags  = IORESOURCE_MEM,	\
	},				\
	{				\
	.start	= 0x140000,		\
	.end 	= 0x15FFFF,		\
	.flags  = IORESOURCE_MEM,	\
	},				\
	{				\
	.start	= 0x160000,		\
	.end 	= 0x17FFFF,		\
	.flags  = IORESOURCE_MEM,	\
	},				\
	{				\
	.start	= 0x190000,		\
	.end 	= 0x19FFFF,		\
	.flags  = IORESOURCE_MEM,	\
	}				\

#define __RES_XMC_SCALING		\
/* RUNTIME CLOCK SCALING FEATURE BASE */\
	{				\
	.start	= 0x053000,		\
	.end	= 0x053fff,		\
	.flags	= IORESOURCE_MEM,	\
	}				\

#define __RES_XMC_GPIO			\
	{				\
	.start	= 0x0132000,		\
	.end	= 0x0132fff,		\
	.flags	= IORESOURCE_MEM,	\
	}				\

#define	XOCL_RES_XMC					\
		((struct resource []) {			\
			__RES_XMC,			\
		})

#define	XOCL_DEVINFO_XMC				\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		XOCL_RES_XMC,				\
		ARRAY_SIZE(XOCL_RES_XMC),		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_XMC_SCALING				\
		((struct resource []) {			\
			__RES_XMC,			\
			__RES_XMC_SCALING,		\
		})

#define	XOCL_DEVINFO_XMC_SCALING			\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		XOCL_RES_XMC_SCALING,				\
		ARRAY_SIZE(XOCL_RES_XMC_SCALING),		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_XMC_SCALING_U2			\
		((struct resource []) {			\
			__RES_XMC,			\
			__RES_XMC_SCALING,		\
			__RES_XMC_GPIO,			\
		})

#define	XOCL_DEVINFO_XMC_SCALING_U2			\
	{							\
		XOCL_SUBDEV_MB,					\
		XOCL_XMC,					\
		XOCL_RES_XMC_SCALING_U2,			\
		ARRAY_SIZE(XOCL_RES_XMC_SCALING_U2),		\
		.override_idx = -1,				\
		.priv_data = &XOCL_PRIV_XMC_U2,			\
		.data_len = sizeof(struct xocl_xmc_privdata),	\
	}

#define	XOCL_DEVINFO_XMC_USER			\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		NULL,					\
		0,					\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_XMC_USER_U2			\
	{							\
		XOCL_SUBDEV_MB,					\
		XOCL_XMC,					\
		NULL,						\
		0,						\
		.override_idx = -1,				\
		.priv_data = &XOCL_PRIV_XMC_U2,			\
		.data_len = sizeof(struct xocl_xmc_privdata),	\
	}

#define	XOCL_RES_XMC_VERSAL				\
		((struct resource []) {			\
			{				\
			.start	= 0x3000000,		\
			.end 	= 0x3007FFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
		})

#define	XOCL_DEVINFO_XMC_VERSAL				\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		XOCL_RES_XMC_VERSAL,			\
		ARRAY_SIZE(XOCL_RES_XMC_VERSAL),	\
		.override_idx = -1,			\
	}

#define	XOCL_RES_MB					\
		((struct resource []) {			\
			{				\
			.start	= 0x120000,		\
			.end 	= 0x121FFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0x131000,		\
			.end 	= 0x131FFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0x140000,		\
			.end 	= 0x15FFFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0x160000,		\
			.end 	= 0x17FFFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
		})

#define	XOCL_DEVINFO_MB					\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_MB,				\
		XOCL_RES_MB,				\
		ARRAY_SIZE(XOCL_RES_MB),		\
		.override_idx = -1,			\
	}

#define	XOCL_RES_PS					\
		((struct resource []) {			\
			{				\
			.start	= 0x110000,		\
			.end	= 0x110FFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
		})

#define	XOCL_DEVINFO_PS					\
	{						\
		XOCL_SUBDEV_PS,				\
		XOCL_PS,				\
		XOCL_RES_PS,				\
		ARRAY_SIZE(XOCL_RES_PS),		\
		.override_idx = -1,			\
	}

#define XOCL_RES_QDMA					\
	((struct resource []) {				\
		{					\
			.start = 0x0,			\
			.end = 0x0,			\
	 		.name = NODE_QDMA,		\
			.flags = IORESOURCE_MEM,	\
		},					\
		{					\
			.start = 0x2000000,			\
			.end = 0x2001000,			\
	 		.name = NODE_STM,		\
			.flags = IORESOURCE_MEM,	\
		},					\
	 })

#define	XOCL_DEVINFO_QDMA				\
	{						\
		XOCL_SUBDEV_DMA,			\
		XOCL_QDMA,				\
		XOCL_RES_QDMA,				\
		ARRAY_SIZE(XOCL_RES_QDMA),		\
		.bar_idx = (char []){ 2, 0 },		\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_XDMA				\
	{						\
		XOCL_SUBDEV_DMA,			\
		XOCL_XDMA,				\
		NULL,					\
		0,					\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_MSIX_XDMA				\
	{						\
		XOCL_SUBDEV_MSIX,			\
		XOCL_MSIX_XDMA,				\
		NULL,					\
		0,					\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_DMA_MSIX				\
	{						\
		.id = XOCL_SUBDEV_DMA,			\
		.name = XOCL_DMA_MSIX,			\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_P2P				\
	{						\
		.id = XOCL_SUBDEV_P2P,			\
		.name = XOCL_P2P,			\
		.override_idx = -1,			\
	}

#define XOCL_RES_ICAP_CNTRL				\
	((struct resource []) {				\
		{					\
			.start = 0x380000,		\
			.end = 0x38000F,		\
			.flags = IORESOURCE_MEM,	\
		},					\
	 })

#define	XOCL_DEVINFO_ICAP_CNTRL			\
	{						\
		XOCL_SUBDEV_ICAP_CNTRL,			\
		XOCL_ICAP_CNTRL,			\
		XOCL_RES_ICAP_CNTRL,			\
		ARRAY_SIZE(XOCL_RES_ICAP_CNTRL),	\
		.override_idx = -1,			\
	}

#define	XOCL_RES_UARTLITE				\
	((struct resource []) {				\
		{					\
			.start	= 0x6060000, \
			.end	= 0x606FFFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 4,			\
			.end	= 4,			\
			.flags  = IORESOURCE_IRQ,	\
		},					\
	})

#define	XOCL_DEVINFO_UARTLITE				\
	{						\
		XOCL_SUBDEV_UARTLITE,			\
		XOCL_UARTLITE,				\
		XOCL_RES_UARTLITE,			\
		ARRAY_SIZE(XOCL_RES_UARTLITE),		\
	}

#define XOCL_RES_INTC					\
	((struct resource []) {				\
		{					\
		.start  = ERT_CSR_ADDR,			\
		.end    = ERT_CSR_ADDR + 0xfff,		\
		.flags  = IORESOURCE_MEM,		\
		},					\
		{					\
		.start  = 0,				\
		.end    = 3,				\
		.flags  = IORESOURCE_IRQ,		\
		}					\
	})

#define XOCL_DEVINFO_INTC				\
	{						\
		XOCL_SUBDEV_INTC,			\
		XOCL_INTC,				\
		XOCL_RES_INTC,				\
		ARRAY_SIZE(XOCL_RES_INTC),		\
		.override_idx = -1,			\
	}

#define XOCL_RES_INTC_QDMA				\
	((struct resource []) {				\
		{					\
		.start  = ERT_CSR_ADDR,			\
		.end    = ERT_CSR_ADDR + 0xfff,		\
		.flags  = IORESOURCE_MEM,		\
		},					\
		{					\
		.start  = 2,				\
		.end    = 5,				\
		.flags  = IORESOURCE_IRQ,		\
		}					\
	})

#define XOCL_DEVINFO_INTC_QDMA				\
	{						\
		XOCL_SUBDEV_INTC,			\
		XOCL_INTC,				\
		XOCL_RES_INTC_QDMA,			\
		ARRAY_SIZE(XOCL_RES_INTC_QDMA),		\
		.override_idx = -1,			\
	}

#define XOCL_RES_COMMAND_QUEUE				\
		((struct resource []) {			\
			{				\
			.start	= ERT_CQ_BASE_ADDR,	\
			.end	= ERT_CQ_BASE_ADDR +	\
			ERT_CQ_SIZE - 1,	\
			.flags	= IORESOURCE_MEM,	\
			},				\
		})

#define        XOCL_DEVINFO_ERT_CTRL			\
	{                                               \
		XOCL_SUBDEV_ERT_CTRL,                   \
		XOCL_ERT_CTRL,                          \
		XOCL_RES_COMMAND_QUEUE,                 \
		ARRAY_SIZE(XOCL_RES_COMMAND_QUEUE),     \
		NULL,                                   \
		0,                                      \
		.override_idx = -1,                     \
	}

#define	XOCL_DEVINFO_COMMAND_QUEUE			\
	{						\
		XOCL_SUBDEV_COMMAND_QUEUE,		\
		XOCL_COMMAND_QUEUE,			\
		NULL,					\
		0,					\
		NULL,					\
		0,					\
		.override_idx = -1,			\
	}	

#define XOCL_RES_SCHEDULER_PRIV				\
	((struct xocl_ert_sched_privdata) {		\
		1,					\
		1,					\
	 })

#define	XOCL_DEVINFO_ERT_USER				\
	{						\
		XOCL_SUBDEV_ERT_USER,			\
		XOCL_ERT_USER,				\
		NULL,					\
		0,					\
		&XOCL_RES_SCHEDULER_PRIV,		\
		sizeof(struct xocl_ert_sched_privdata),	\
		.level = XOCL_SUBDEV_LEVEL_BLD,		\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_HWMON_SDM				\
	{						\
		XOCL_SUBDEV_HWMON_SDM,			\
		XOCL_HWMON_SDM,				\
		NULL,					\
		0,					\
		.override_idx = -1,			\
	}

#define	ERT_CSR_ADDR_VERSAL		0x2040000
#define	ERT_CQ_BASE_ADDR_VERSAL		0x6000000

#define XOCL_RES_INTC_VERSAL				\
	((struct resource []) {				\
		{					\
		.start  = ERT_CSR_ADDR_VERSAL,		\
		.end    = ERT_CSR_ADDR_VERSAL + 0xfff,	\
		.flags  = IORESOURCE_MEM,		\
		},					\
		{					\
		.start  = 0,				\
		.end    = 0,				\
		.flags  = IORESOURCE_IRQ,		\
		}					\
	})

#define XOCL_DEVINFO_INTC_VERSAL			\
	{						\
		XOCL_SUBDEV_INTC,			\
		XOCL_INTC,				\
		XOCL_RES_INTC_VERSAL,			\
		ARRAY_SIZE(XOCL_RES_INTC_VERSAL),	\
		.override_idx = -1,			\
		.bar_idx = (char []){ 2 },		\
	}

#define	XOCL_DEVINFO_FMGR				\
	{						\
		XOCL_SUBDEV_FMGR,			\
		XOCL_FMGR,				\
		NULL,					\
		0,					\
		.override_idx = -1,			\
	}

#define	XOCL_RES_XFER_MGMT_VERSAL				\
		((struct resource []) {			\
			{				\
			.start	= 0x3008000,		\
			.end	= 0x300FFFF,		\
			.flags	= IORESOURCE_MEM,	\
			}				\
		})

#define	XOCL_DEVINFO_XFER_MGMT_VERSAL			\
	{						\
		XOCL_SUBDEV_XFER_VERSAL,		\
		XOCL_XFER_VERSAL,			\
		XOCL_RES_XFER_MGMT_VERSAL,			\
		ARRAY_SIZE(XOCL_RES_XFER_MGMT_VERSAL),	\
		.override_idx = -1,			\
	}

#define XOCL_RES_FLASH					\
	((struct resource []) {				\
		{					\
			.start = 0x40000,		\
			.end = 0x4007b,			\
			.flags = IORESOURCE_MEM,	\
		},					\
	 })

#define XOCL_DEVINFO_FLASH				\
	{						\
		XOCL_SUBDEV_FLASH,			\
		XOCL_FLASH,				\
		XOCL_RES_FLASH,				\
		ARRAY_SIZE(XOCL_RES_FLASH),		\
		.override_idx = -1,			\
	}

/* user pf defines */
#define	USER_RES_QDMA							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_QDMA,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_MAILBOX_USER_QDMA,			\
			XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
			XOCL_DEVINFO_INTC_QDMA,				\
			XOCL_DEVINFO_ERT_CTRL,                          \
		})

#define	XOCL_BOARD_USER_QDMA						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_QDMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_QDMA),		\
	}

#define	USER_RES_XDMA_DSA50						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_INTC,				\
			XOCL_DEVINFO_ERT_CTRL,                          \
		})

#define	USER_RES_XDMA							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_MAILBOX_USER,			\
			XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
			XOCL_DEVINFO_INTC,				\
			XOCL_DEVINFO_ERT_CTRL,                          \
		})

#define	USER_RES_XDMA_VERSAL						\
		((struct xocl_subdev_info []) {				\
		 	XOCL_DEVINFO_FEATURE_ROM_VERSAL,		\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_PF_MAILBOX_USER_VERSAL,		\
			XOCL_DEVINFO_MAILBOX_USER_VERSAL,		\
		 	XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_INTC_VERSAL,			\
			XOCL_DEVINFO_ERT_CTRL,                          \
		})

#define USER_RES_AWS_XDMA						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_MAILBOX_USER_SOFTWARE,		\
			XOCL_DEVINFO_ICAP_USER,				\
		})

#define USER_RES_AWS_NODMA						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_MSIX_XDMA,				\
			XOCL_DEVINFO_MAILBOX_USER_SOFTWARE,		\
			XOCL_DEVINFO_ICAP_USER,				\
		})

#define	USER_RES_DSA52							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_MAILBOX_USER,			\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
			XOCL_DEVINFO_INTC,				\
			XOCL_DEVINFO_ERT_CTRL,                          \
		})

#define	USER_RES_DSA52_U2					\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_VERSION_CTRL,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_MAILBOX_USER,			\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_XMC_USER_U2,			\
			XOCL_DEVINFO_AF_USER,				\
			XOCL_DEVINFO_INTC,				\
			XOCL_DEVINFO_ERT_CTRL,                          \
		})

#define USER_RES_SMARTN							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM_SMARTN,		\
			XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_MAILBOX_USER_QDMA,			\
		})


#define	XOCL_BOARD_USER_XDMA_DSA50					\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_MB_SCHE_OFF,		\
		.subdev_info	= USER_RES_XDMA_DSA50,			\
		.subdev_num = ARRAY_SIZE(USER_RES_XDMA_DSA50),		\
	}

#define	XOCL_BOARD_USER_XDMA						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_XDMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_XDMA),		\
	}

#define	XOCL_BOARD_USER_XDMA_VERSAL					\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_VERSAL,			\
		.subdev_info	= USER_RES_XDMA_VERSAL,			\
		.subdev_num = ARRAY_SIZE(USER_RES_XDMA_VERSAL),		\
	}

#define	XOCL_BOARD_USER_XDMA_ERT_OFF					\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_MB_SCHE_OFF,		\
		.subdev_info	= USER_RES_XDMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_XDMA),		\
	}

#define XOCL_BOARD_USER_AWS_XDMA					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_AWS_XDMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_AWS_XDMA),		\
	}

#define XOCL_BOARD_USER_AWS_NODMA					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_AWS_NODMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_AWS_NODMA),		\
	}

#define	XOCL_BOARD_USER_DSA52_U2				\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_DSA52_U2,			\
		.subdev_num = ARRAY_SIZE(USER_RES_DSA52_U2),		\
		.p2p_bar_sz = 4,					\
	}

#define	XOCL_BOARD_USER_DSA52						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_DSA52,			\
		.subdev_num = ARRAY_SIZE(USER_RES_DSA52),		\
	}

#define	XOCL_BOARD_USER_DSA52_U280					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_DSA52,			\
		.subdev_num = ARRAY_SIZE(USER_RES_DSA52),		\
		.p2p_bar_sz = 64,					\
	}

#define	XOCL_BOARD_USER_SMARTN						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_SMARTN,			\
		.subdev_info	= USER_RES_SMARTN,			\
		.subdev_num = ARRAY_SIZE(USER_RES_SMARTN),		\
	}

#define	XOCL_BOARD_USER_DSA_U250_NO_KDMA				\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_NO_KDMA,			\
		.subdev_info	= USER_RES_DSA52,			\
		.subdev_num = ARRAY_SIZE(USER_RES_DSA52),		\
	}

/* mgmt pf defines */
#define	MGMT_RES_DEFAULT						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
		 	XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})

#define	MGMT_RES_VERSAL							\
		((struct xocl_subdev_info []) {				\
		 	XOCL_DEVINFO_FEATURE_ROM_VERSAL,		\
			XOCL_DEVINFO_MAILBOX_MGMT_VERSAL,		\
		 	XOCL_DEVINFO_XMC_VERSAL,			\
		 	XOCL_DEVINFO_XFER_MGMT_VERSAL,			\
		 	XOCL_DEVINFO_UARTLITE,				\
		})

#define	MGMT_RES_DSA50							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
		 	XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
			XOCL_DEVINFO_FLASH,				\
		})

#define	MGMT_RES_U2						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_VERSION_CTRL,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON_U2,			\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_XMC_SCALING_U2,			\
			XOCL_DEVINFO_FLASH,				\
			XOCL_DEVINFO_ICAP_CNTRL,			\
		})

#define	XOCL_BOARD_MGMT_DEFAULT						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_DEFAULT,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_DEFAULT),		\
	}

#define	XOCL_BOARD_MGMT_DSA50						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_PCI_RESET_OFF |		\
			XOCL_DSAFLAG_AXILITE_FLUSH |			\
			XOCL_DSAFLAG_MB_SCHE_OFF,			\
		.subdev_info	= MGMT_RES_DSA50,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_DSA50),		\
	}

#define	XOCL_BOARD_MGMT_U2					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_U2,				\
		.subdev_num = ARRAY_SIZE(MGMT_RES_U2),			\
		.flash_type = FLASH_TYPE_SPI,				\
		.sched_bin = "xilinx/sched.bin",			\
	}

#define	XOCL_BOARD_MGMT_VERSAL						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_VERSAL |			\
			XOCL_DSAFLAG_FIXED_INTR,			\
		.subdev_info	= MGMT_RES_VERSAL,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_VERSAL),		\
		.flash_type = FLASH_TYPE_OSPI_VERSAL,			\
	}

#define	MGMT_RES_6A8F							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
		 	XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})

#define	MGMT_RES_6A8F_DSA50						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
		 	XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})

#define	MGMT_RES_XBB_DSA51						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
		 	XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_XMC,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})

#define	XOCL_BOARD_MGMT_6A8F						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6A8F,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6A8F),		\
	}

#define	XOCL_BOARD_MGMT_XBB_DSA51					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_XBB_DSA51,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_DSA51),		\
		.flash_type = FLASH_TYPE_SPI,				\
	}


#define	XOCL_BOARD_MGMT_888F	XOCL_BOARD_MGMT_6A8F
#define	XOCL_BOARD_MGMT_898F	XOCL_BOARD_MGMT_6A8F

#define	XOCL_BOARD_MGMT_6A8F_DSA50					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6A8F_DSA50,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6A8F_DSA50),		\
	}

#define	MGMT_RES_QDMA							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
		 	XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT_QDMA,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})


#define	XOCL_BOARD_MGMT_QDMA						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_QDMA,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_QDMA),		\
		.flash_type = FLASH_TYPE_SPI				\
	}

#define MGMT_RES_XBB_QEP						\
	((struct xocl_subdev_info []) {					\
		XOCL_DEVINFO_FEATURE_ROM,				\
		XOCL_DEVINFO_PRP_IORES_MGMT,				\
		XOCL_DEVINFO_AXIGATE_ULP,				\
		XOCL_DEVINFO_CLOCK_HBM,					\
		XOCL_DEVINFO_AF_DSA52,			 		\
		XOCL_DEVINFO_XMC,					\
		XOCL_DEVINFO_XVC_PRI,					\
		XOCL_DEVINFO_NIFD_PRI,					\
		XOCL_DEVINFO_MAILBOX_MGMT_QDMA,				\
		XOCL_DEVINFO_ICAP_MGMT,					\
		XOCL_DEVINFO_FMGR,					\
		XOCL_DEVINFO_FLASH,					\
	})


#define XOCL_BOARD_MGMT_U250_QEP					\
	(struct xocl_board_private){					\
		.flags	  = XOCL_DSAFLAG_FIXED_INTR,			\
		.subdev_info    = MGMT_RES_XBB_QEP,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_QEP),		\
		.flash_type = FLASH_TYPE_SPI				\
	}


#define	XOCL_BOARD_MGMT_6B0F		XOCL_BOARD_MGMT_6A8F

#define	MGMT_RES_6A8F_DSA52						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF_DSA52,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})

#define	XOCL_BOARD_MGMT_6A8F_DSA52					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6A8F_DSA52,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6A8F_DSA52),		\
	}

#define	MGMT_RES_XBB_DSA52						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_AF_DSA52,				\
			XOCL_DEVINFO_XMC,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_NIFD_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})

#define	XOCL_BOARD_MGMT_XBB_DSA52					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_XBB_DSA52,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_DSA52),		\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define	MGMT_RES_XBB_DSA52_U200						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_AF_DSA52,				\
			XOCL_DEVINFO_XMC_SCALING,			\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_NIFD_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})

#define	XOCL_BOARD_MGMT_XBB_DSA52_U200					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_XBB_DSA52_U200,		\
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_DSA52_U200),	\
		.flash_type = FLASH_TYPE_SPI,				\
	}


#define	MGMT_RES_XBB_DSA52_U280						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_HBM,				\
			XOCL_DEVINFO_AF_DSA52,				\
			XOCL_DEVINFO_XMC,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})

#define	XOCL_BOARD_MGMT_XBB_DSA52_U280					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_XBB_DSA52_U280,		\
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_DSA52_U280),	\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define MGMT_RES_XBB_SMARTN						\
	((struct xocl_subdev_info []) {					\
		XOCL_DEVINFO_FEATURE_ROM_SMARTN,			\
		XOCL_DEVINFO_PRP_IORES_MGMT_SMARTN,			\
		XOCL_DEVINFO_AXIGATE_ULP_SMARTN,			\
		XOCL_DEVINFO_XMC,					\
		XOCL_DEVINFO_MAILBOX_MGMT_QDMA,				\
		XOCL_DEVINFO_ICAP_MGMT_SMARTN,				\
		XOCL_DEVINFO_FMGR,					\
		XOCL_DEVINFO_FLASH,					\
	})

#define XOCL_BOARD_MGMT_XBB_SMARTN					\
	(struct xocl_board_private){					\
		.flags	  = XOCL_DSAFLAG_SMARTN,			\
		.subdev_info    = MGMT_RES_XBB_SMARTN,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_SMARTN),		\
		.flash_type = FLASH_TYPE_SPI				\
	}

#define	MGMT_RES_6E8F_DSA52						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
			XOCL_DEVINFO_FLASH,				\
		})

#define	XOCL_BOARD_MGMT_6E8F_DSA52					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6E8F_DSA52,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6E8F_DSA52),		\
	}

#define MGMT_RES_MPSOC							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
		})

#define MGMT_RES_MPSOC_U30						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AXIGATE_ULP,			\
			XOCL_DEVINFO_CLOCK_LEGACY,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_PS,				\
			XOCL_DEVINFO_XMC,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,				\
		})

#define	XOCL_BOARD_MGMT_MPSOC						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_MPSOC,			\
		.subdev_info	= MGMT_RES_MPSOC,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_MPSOC),		\
		.board_name = "samsung",				\
		.flash_type = FLASH_TYPE_QSPIPS,			\
	}

#define	XOCL_BOARD_MGMT_U30						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_MPSOC,			\
		.subdev_info	= MGMT_RES_MPSOC_U30,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_MPSOC_U30),		\
		.board_name = "u30",					\
		.flash_type = FLASH_TYPE_QSPIPS_X2_SINGLE,				\
	}

#define	XOCL_BOARD_USER_XDMA_MPSOC					\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_MPSOC,			\
		.subdev_info	= USER_RES_XDMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_XDMA),		\
	}

#define	XOCL_BOARD_XBB_MFG_U30						\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_MFG,				\
		.board_name = "u30",					\
		.flash_type = FLASH_TYPE_QSPIPS_X2_SINGLE,				\
	}

#define XOCL_RES_FLASH_MFG_U50				\
	((struct resource []) {				\
		{					\
			.start = 0x1f50000,		\
			.end = 0x1f5FFFF,		\
			.flags = IORESOURCE_MEM,	\
		},					\
	 })

#define XOCL_DEVINFO_FLASH_MFG_U50			\
	{						\
		XOCL_SUBDEV_FLASH,			\
		XOCL_FLASH,				\
		XOCL_RES_FLASH_MFG_U50,			\
		ARRAY_SIZE(XOCL_RES_FLASH_MFG_U50),	\
		.override_idx = -1,			\
	}

#define	XOCL_RES_XMC_MFG_U50				\
		((struct resource []) {			\
			{				\
			.start	= 0x140000,		\
			.end 	= 0x141FFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
		})

#define	XOCL_DEVINFO_XMC_MFG_U50			\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		XOCL_RES_XMC_MFG_U50,			\
		ARRAY_SIZE(XOCL_RES_XMC_MFG_U50),	\
		.override_idx = -1,			\
	}

#define MFG_RES_U50							\
	((struct xocl_subdev_info []) {					\
	 	XOCL_DEVINFO_FLASH_MFG_U50,				\
	 })

#define	XOCL_BOARD_XBB_MFG_U50						\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_MFG,				\
		.board_name = "u50",					\
		.subdev_info	= MFG_RES_U50,				\
		.subdev_num = ARRAY_SIZE(MFG_RES_U50),			\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define	XOCL_DEVINFO_FEATURE_ROM_MGMT_DYN		\
	{						\
		XOCL_SUBDEV_FEATURE_ROM,		\
		XOCL_FEATURE_ROM,			\
		NULL,					\
		0,					\
		.override_idx = -1,			\
	}

#define RES_MGMT_VSEC							\
	((struct xocl_subdev_info []) {					\
	 	XOCL_DEVINFO_FEATURE_ROM_MGMT_DYN,			\
		XOCL_DEVINFO_FMGR,					\
		XOCL_DEVINFO_CALIB_STORAGE,				\
	 })

/**********************VCK190 PCIE Golden Image START********************/

#define XOCL_RES_XFER_MGMT_VCK190                                       \
                ((struct resource []) {                                 \
                        {                                               \
                        .start  = 0x60000,                              \
                        .end    = 0x6FFFF ,                             \
                        .flags  = IORESOURCE_MEM,                       \
                        }                                               \
                })

#define XOCL_DEVINFO_FLASH_MFG_VCK190                                   \
        {                                                               \
                XOCL_SUBDEV_XFER_VERSAL,                                \
                XOCL_XFER_VERSAL,                                       \
                XOCL_RES_XFER_MGMT_VCK190,                              \
                ARRAY_SIZE(XOCL_RES_XFER_MGMT_VCK190),                  \
                .override_idx = -1,                                     \
                .bar_idx = (char []){ 0 },                              \
        }

#define RES_MFG_VCK190                                                  \
        ((struct xocl_subdev_info []) {                                 \
                 XOCL_DEVINFO_FLASH_MFG_VCK190,                         \
        })

#define XOCL_BOARD_XBB_MFG_VCK190                                       \
        (struct xocl_board_private){                                    \
                .flags = XOCL_DSAFLAG_MFG,                              \
                .subdev_info = RES_MFG_VCK190,                          \
                .subdev_num = ARRAY_SIZE(RES_MFG_VCK190),               \
                .flash_type = FLASH_TYPE_QSPI_VERSAL,                   \
                .board_name = "vck190"                                  \
        }

/**********************VCK190 PCIE Golden Image END**********************/

#define RES_USER_VSEC							\
	((struct xocl_subdev_info []) {					\
	 	XOCL_DEVINFO_FEATURE_ROM_USER_DYN,			\
		XOCL_DEVINFO_ICAP_USER,					\
		XOCL_DEVINFO_XMC_USER,					\
		XOCL_DEVINFO_AF_USER,					\
	 })

/*********************************VCK190 USERPF START********************/

/* HACK: Mailbox resource is hardcoded as VSEC is not available for this platform */
#define XOCL_RES_PF_MAILBOX_USER_VCK190                                 \
        ((struct resource []) {                                         \
                {                                                       \
                        .start  = 0x0,                                  \
                        .end    = 0xFFFF,                               \
                        .flags  = IORESOURCE_MEM,                       \
                },                                                      \
                {                                                       \
                        .start  = 4,                                    \
                        .end    = 4,                                    \
                        .flags  = IORESOURCE_IRQ,                       \
                },                                                      \
        })

#define XOCL_DEVINFO_PF_MAILBOX_USER_VCK190                             \
        {                                                               \
                XOCL_SUBDEV_MAILBOX,                                    \
                XOCL_MAILBOX,                                           \
                XOCL_RES_PF_MAILBOX_USER_VCK190,                        \
                ARRAY_SIZE(XOCL_RES_PF_MAILBOX_USER_VCK190),            \
                .override_idx = -1,                                     \
                .bar_idx = (char []){ 0 },                              \
        }

#define RES_USER_VCK190_VSEC                                            \
        ((struct xocl_subdev_info []) {                                 \
                XOCL_DEVINFO_FEATURE_ROM_USER_DYN,                      \
                XOCL_DEVINFO_ICAP_USER,                                 \
                XOCL_DEVINFO_XMC_USER,                                  \
                XOCL_DEVINFO_AF_USER,                                   \
                XOCL_DEVINFO_PF_MAILBOX_USER_VCK190,                    \
	 })

#define XOCL_BOARD_VCK190_USER_RAPTOR2                                  \
        (struct xocl_board_private){                                    \
                .flags = XOCL_DSAFLAG_DYNAMIC_IP |                      \
                        XOCL_DSAFLAG_CUSTOM_DTB |                       \
                        XOCL_DSAFLAG_VERSAL,                            \
                .subdev_info = RES_USER_VCK190_VSEC,                    \
                .subdev_num = ARRAY_SIZE(RES_USER_VCK190_VSEC),         \
                .board_name = "vck190"                                  \
        }

/*********************************VCK190 USERPF END**********************/

/* need static scheduler for a little while, and no AF user for now */
#define RES_USER_VERSAL_VSEC						\
	((struct xocl_subdev_info []) {					\
		XOCL_DEVINFO_FEATURE_ROM_USER_DYN,			\
		XOCL_DEVINFO_ICAP_USER,					\
		XOCL_DEVINFO_XMC_USER,					\
	 })

#define RES_MGMT_U2_VSEC						\
	((struct xocl_subdev_info []) {					\
		XOCL_DEVINFO_FEATURE_ROM_MGMT_DYN,			\
		XOCL_DEVINFO_FMGR,					\
	 })

#define XOCL_BOARD_U2_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,		\
		.subdev_info	= RES_MGMT_U2_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_U2_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define XOCL_BOARD_U2_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,		\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define XOCL_BOARD_U25_USER_RAPTOR2                                     \
	(struct xocl_board_private){                                    \
		.flags = XOCL_DSAFLAG_DYNAMIC_IP |                      \
		        XOCL_DSAFLAG_MB_SCHE_OFF,          		\
		.board_name = "u25",                                    \
		.subdev_info    = RES_USER_VSEC,                        \
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),                \
	}

#define XOCL_BOARD_U25_MGMT_RAPTOR2                                     \
	(struct xocl_board_private){                                    \
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,                       \
		.subdev_info    = RES_MGMT_VSEC,                        \
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),                \
		.flash_type = FLASH_TYPE_QSPIPS_X4_SINGLE,              \
		.board_name = "u25"                                     \
	}

#define XOCL_BOARD_U26Z_USER_RAPTOR2                          \
	(struct xocl_board_private){                               \
		.flags       = XOCL_DSAFLAG_DYNAMIC_IP,                 \
		.board_name  = "u26z",                                  \
		.subdev_info = RES_USER_VSEC,                           \
		.subdev_num  = ARRAY_SIZE(RES_USER_VSEC)                \
	}

#define XOCL_BOARD_U26Z_MGMT_RAPTOR2                          \
	(struct xocl_board_private){                               \
		.flags       = XOCL_DSAFLAG_DYNAMIC_IP,                 \
		.subdev_info = RES_MGMT_VSEC,                           \
		.subdev_num  = ARRAY_SIZE(RES_MGMT_VSEC),               \
		.flash_type  = FLASH_TYPE_SPI,                          \
		.board_name  = "u26z",                                  \
		.vbnv = "xilinx_u26z"	                          				\
	}

#define XOCL_BOARD_X3522PV_USER_RAPTOR2                     \
	(struct xocl_board_private){                              \
		.flags       = XOCL_DSAFLAG_DYNAMIC_IP,                 \
		.board_name  = "x3522pv",                               \
		.subdev_info = RES_USER_VSEC,                           \
		.subdev_num  = ARRAY_SIZE(RES_USER_VSEC)                \
	}

#define XOCL_BOARD_X3522PV_MGMT_RAPTOR2                     \
	(struct xocl_board_private){                              \
		.flags       = XOCL_DSAFLAG_DYNAMIC_IP,                 \
		.subdev_info = RES_MGMT_VSEC,                           \
		.subdev_num  = ARRAY_SIZE(RES_MGMT_VSEC),               \
		.flash_type  = FLASH_TYPE_SPI,                          \
		.board_name  = "x3522pv",                               \
		.vbnv        = "xilinx_x3522pv"                         \
	}

#define XOCL_BOARD_U30_USER_RAPTOR2                                     \
        (struct xocl_board_private){                                    \
                .flags = XOCL_DSAFLAG_DYNAMIC_IP |                      \
		        XOCL_DSAFLAG_MPSOC,				\
                .board_name = "u30",                                    \
                .subdev_info    = RES_USER_VSEC,                        \
                .subdev_num = ARRAY_SIZE(RES_USER_VSEC),                \
        }

#define XOCL_BOARD_U30_MGMT_RAPTOR2                                     \
        (struct xocl_board_private){                                    \
                .flags = XOCL_DSAFLAG_DYNAMIC_IP |                      \
		        XOCL_DSAFLAG_MPSOC,				\
                .subdev_info    = RES_MGMT_VSEC,                        \
                .subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),                \
                .flash_type = FLASH_TYPE_QSPIPS_X2_SINGLE,              \
                .board_name = "u30"                                     \
        }


#define	XOCL_BOARD_U50_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.board_name = "u50",					\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
	}

#define	XOCL_BOARD_U50_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,                       \
		.subdev_info	= RES_MGMT_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.board_name = "u50"					\
	}

#define	XOCL_BOARD_U55N_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.board_name = "u55n",					\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
	}

#define	XOCL_BOARD_U55N_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,                       \
		.subdev_info	= RES_MGMT_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.board_name = "u55n",					\
		.vbnv = "xilinx_u55n"					\
	}

#define	XOCL_BOARD_U55C_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.board_name = "u55c",					\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
	}

#define	XOCL_BOARD_U55C_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,                       \
		.subdev_info	= RES_MGMT_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.board_name = "u55c",					\
		.vbnv = "xilinx_u55c"					\
	}

#define	XOCL_BOARD_U50LV_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.board_name = "u50lv",					\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
	}

#define	XOCL_BOARD_U50LV_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,                       \
		.subdev_info	= RES_MGMT_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.board_name = "u50lv",					\
		.vbnv = "xilinx_u50lv"					\
	}

#define	XOCL_BOARD_U50C_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.board_name = "u50c",					\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
	}

#define	XOCL_BOARD_U50C_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,                       \
		.subdev_info	= RES_MGMT_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.board_name = "u50c",					\
		.vbnv = "xilinx_u50c"					\
	}

#define	XOCL_BOARD_U200_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
		.board_name = "u200"					\
	}

#define	XOCL_BOARD_U200_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.subdev_info	= RES_MGMT_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.board_name = "u200"					\
	}

#define	XOCL_BOARD_U250_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP, 			\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
		.board_name = "u250",					\
		.p2p_bar_sz = 64,					\
	}

#define	XOCL_BOARD_U280_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP, 			\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
		.board_name = "u280",					\
	}

#define	XOCL_BOARD_U250_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.subdev_info	= RES_MGMT_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.board_name = "u250"					\
	}

#define	XOCL_BOARD_U280_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.subdev_info	= RES_MGMT_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.board_name = "u280"					\
	}

#define	XOCL_BOARD_VERSAL_USER_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP |			\
			XOCL_DSAFLAG_VERSAL,				\
		.subdev_info = RES_USER_VERSAL_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VERSAL_VSEC),		\
		.board_name = "vck5000"					\
	}
#define	XOCL_BOARD_VERSAL_USER_RAPTOR2_ES3				\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP |			\
			XOCL_DSAFLAG_VERSAL_ES3 |			\
			XOCL_DSAFLAG_VERSAL,				\
		.subdev_info = RES_USER_VERSAL_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VERSAL_VSEC),		\
		.board_name = "vck5000",				\
		.vbnv       = "xilinx_vck5000"				\
	}
#define	XOCL_BOARD_VERSAL_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_VERSAL |				\
			XOCL_DSAFLAG_FIXED_INTR |			\
			XOCL_DSAFLAG_DYNAMIC_IP,	 		\
		.subdev_info = RES_MGMT_VSEC,				\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_OSPI_VERSAL,			\
		.board_name = "vck5000",				\
		.vbnv       = "xilinx_vck5000"				\
	}
#define	XOCL_BOARD_V70_MGMT_RAPTOR2					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_VERSAL |				\
			XOCL_DSAFLAG_FIXED_INTR |			\
			XOCL_DSAFLAG_DYNAMIC_IP,			\
		.subdev_info = RES_MGMT_VSEC,				\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_OSPI_VERSAL,			\
		.board_name = "v70",				\
		.vbnv       = "xilinx_v70"				\
	}
#define	XOCL_BOARD_V70_USER_RAPTOR2_ES3				\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP |			\
			XOCL_DSAFLAG_VERSAL_ES3 |			\
			XOCL_DSAFLAG_VERSAL,				\
		.subdev_info = RES_USER_VERSAL_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VERSAL_VSEC),		\
		.board_name = "v70",				\
		.vbnv       = "xilinx_v70"				\
	}

#define	XOCL_BOARD_AVALON_USER_RAPTOR2				\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.subdev_info	= RES_USER_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_USER_VSEC),		\
		.board_name = "avalon",					\
	}

#define	XOCL_BOARD_AVALON_MGMT_RAPTOR2				\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_DYNAMIC_IP,			\
		.subdev_info	= RES_MGMT_VSEC,			\
		.subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.board_name = "avalon"					\
	}

/*********************************VCK190 MGMTPF START*******************/

#define XOCL_BOARD_VCK190_MGMT_RAPTOR2                                  \
        (struct xocl_board_private){                                    \
                .flags = XOCL_DSAFLAG_VERSAL |                          \
                        XOCL_DSAFLAG_FIXED_INTR |                       \
                        XOCL_DSAFLAG_CUSTOM_DTB |                       \
                        XOCL_DSAFLAG_DYNAMIC_IP,                        \
                .subdev_info = RES_MGMT_VSEC,                           \
                .subdev_num = ARRAY_SIZE(RES_MGMT_VSEC),                \
                .flash_type = FLASH_TYPE_QSPI_VERSAL,                   \
                .board_name = "vck190"                                  \
        }

/*********************************VCK190 MGMTPF END**********************/

#define XOCL_RES_XMC_MFG				\
	((struct resource []) {				\
		{					\
			.start	= 0x120000,		\
			.end 	= 0x121FFF,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define XOCL_DEVINFO_XMC_MFG				\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		XOCL_RES_XMC_MFG,			\
		ARRAY_SIZE(XOCL_RES_XMC_MFG),		\
		.override_idx = -1,			\
	}

#define MFG_RES								\
	((struct xocl_subdev_info []) {					\
		XOCL_DEVINFO_FLASH,					\
	})

#define	XOCL_BOARD_XBB_MFG(board)					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_MFG,				\
		.board_name = board,					\
		.flash_type = FLASH_TYPE_SPI,				\
		.subdev_info	= MFG_RES,				\
		.subdev_num = ARRAY_SIZE(MFG_RES),			\
	}

#define XOCL_RES_FEATURE_ROM_DYN			\
	((struct resource []) {				\
	 	{					\
	 		.name = "uuid",			\
	 		.start = 0x1f10000,		\
	 		.end = 0x1f10fff,		\
	 		.flags = IORESOURCE_MEM,	\
	 	},					\
	 })


#define	XOCL_DEVINFO_FEATURE_ROM_DYN			\
	{						\
		XOCL_SUBDEV_FEATURE_ROM,		\
		XOCL_FEATURE_ROM,			\
		XOCL_RES_FEATURE_ROM_DYN,		\
		ARRAY_SIZE(XOCL_RES_FEATURE_ROM_DYN),	\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_FEATURE_ROM_USER_DYN		\
	{						\
		XOCL_SUBDEV_FEATURE_ROM,		\
		XOCL_FEATURE_ROM,			\
		NULL,					\
		0,					\
		.dyn_ip = 1,				\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = -1,			\
	}

#define XOCL_RES_MAILBOX_VSEC				\
	((struct resource []) {				\
		{					\
			.start	= 0x0,			\
			.end	= 0x2F,			\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define XOCL_DEVINFO_MAILBOX_VSEC			\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_MAILBOX_VSEC,			\
		ARRAY_SIZE(XOCL_RES_MAILBOX_VSEC),	\
		.bar_idx = (char []){ 0 },		\
		.override_idx = -1,			\
	}

#define XOCL_RES_XGQ_VMR_VSEC				\
	((struct resource []) {				\
		{					\
			.start	= 0x0,			\
			.end	= 0x0,			\
	 		.name   = NODE_XGQ_SQ_BASE,	\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 0x0,			\
			.end	= 0x0,			\
	 		.name   = NODE_XGQ_VMR_PAYLOAD_BASE,\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define XOCL_PRIV_FLASH_XGQ				\
	((struct xocl_flash_privdata) {			\
		0,					\
		FLASH_TYPE_OSPI_XGQ,			\
	 })

#define XOCL_DEVINFO_XGQ_VMR_VSEC			\
	{						\
		XOCL_SUBDEV_XGQ_VMR,			\
		XOCL_XGQ_VMR,				\
		XOCL_RES_XGQ_VMR_VSEC,			\
		ARRAY_SIZE(XOCL_RES_XGQ_VMR_VSEC),	\
		.bar_idx = (char []){ 0, 0 },		\
		.override_idx = -1,			\
		.priv_data = &XOCL_PRIV_FLASH_XGQ	\
	}


#define XOCL_RES_FLASH_BLP				\
	((struct resource []) {				\
		{					\
			.name	= "flash",		\
			.start	= 0x1f50000,		\
			.end	= 0x1f5ffff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define XOCL_PRIV_FLASH_BLP				\
	((struct xocl_flash_privdata) {			\
		0,					\
		FLASH_TYPE_SPI,				\
	 })

#define XOCL_DEVINFO_FLASH_BLP				\
	{						\
		XOCL_SUBDEV_FLASH,			\
		XOCL_FLASH,				\
		XOCL_RES_FLASH_BLP,			\
		ARRAY_SIZE(XOCL_RES_FLASH_BLP),		\
		.level = XOCL_SUBDEV_LEVEL_BLD,		\
		.bar_idx = (char []){ 0 },		\
		.priv_data = &XOCL_PRIV_FLASH_BLP,	\
		.data_len = sizeof(struct xocl_flash_privdata), \
		.override_idx = -1,			\
	}

#define XOCL_DEVINFO_FLASH_VSEC XOCL_DEVINFO_FLASH_BLP

#define XOCL_RES_XMC_BLP				\
	((struct resource []) {				\
		{					\
			.start	= 0x140000,		\
			.end	= 0x141fff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define XOCL_DEVINFO_XMC_BLP				\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		XOCL_RES_XMC_BLP,			\
		ARRAY_SIZE(XOCL_RES_XMC_BLP),		\
		.override_idx = -1,			\
	}


#define	XOCL_RES_MAILBOX_USER_DYN			\
	((struct resource []) {				\
		{					\
			.start	= 0x1f20000,		\
			.end	= 0x1f2002F,		\
			.flags  = IORESOURCE_MEM,	\
		}					\
	})
#define	XOCL_DEVINFO_MAILBOX_USER_DYN			\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_MAILBOX_USER_DYN,		\
		ARRAY_SIZE(XOCL_RES_MAILBOX_USER_DYN),	\
		.override_idx = -1,			\
	}


#define	XOCL_RES_MAILBOX_USER_U50			\
	((struct resource []) {				\
		{					\
			.start	= 0x1f20000,		\
			.end	= 0x1f2002F,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.start	= 4,			\
			.end	= 4,			\
			.flags  = IORESOURCE_IRQ,	\
		},					\
	})

#define	XOCL_DEVINFO_MAILBOX_USER_U50			\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_MAILBOX_USER_U50,		\
		ARRAY_SIZE(XOCL_RES_MAILBOX_USER_U50),	\
		.override_idx = -1,			\
	}

#define	XOCL_DEVINFO_MAILBOX_USER_SOFTWARE		\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		NULL,		\
		0,	\
		.override_idx = -1,			\
	}

#define XOCL_RES_IORES_MGMT_DYN					\
	((struct resource []) {					\
		__RES_CLKFREQ_K1_K2				\
	 })

#define XOCL_DEVINFO_IORES_MGMT_DYN				\
	{							\
		XOCL_SUBDEV_IORES,				\
		XOCL_IORES0,					\
		XOCL_RES_IORES_MGMT_DYN,			\
		ARRAY_SIZE(XOCL_RES_IORES_MGMT_DYN),		\
		.override_idx = -1,			\
	}

#define MGMT_RES_DYNAMIC_IP					\
	((struct xocl_subdev_info []) {				\
	 	XOCL_DEVINFO_FEATURE_ROM_DYN,			\
		XOCL_DEVINFO_IORES_MGMT_DYN,			\
	 	XOCL_DEVINFO_FLASH_BLP,				\
		XOCL_DEVINFO_FMGR,				\
		XOCL_DEVINFO_CALIB_STORAGE,			\
	})

#define	XOCL_BOARD_U200_MGMT_EA						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,		\
		.subdev_info	= MGMT_RES_DYNAMIC_IP,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_DYNAMIC_IP),		\
		.flash_type = FLASH_TYPE_SPI,				\
		.sched_bin = "xilinx/sched_u50.bin",			\
	}

#define USER_RES_DYNAMIC_IP						\
		((struct xocl_subdev_info []) {				\
		 	XOCL_DEVINFO_FEATURE_ROM_USER_DYN,		\
		 	XOCL_DEVINFO_MAILBOX_USER_DYN,			\
		 	XOCL_DEVINFO_ICAP_USER,				\
		 	XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
		})

#define	XOCL_BOARD_U200_USER_EA						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,		\
		.subdev_info	= USER_RES_DYNAMIC_IP,			\
		.subdev_num = ARRAY_SIZE(USER_RES_DYNAMIC_IP),		\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define XOCL_RES_FEATURE_ROM_U50			\
	((struct resource []) {				\
	 	{					\
	 		.start = 0x0,			\
	 		.end = 0xfff,			\
	 		.flags = IORESOURCE_MEM,	\
	 	},					\
	 })

#define	XOCL_DEVINFO_FEATURE_ROM_U50			\
	{						\
		XOCL_SUBDEV_FEATURE_ROM,		\
		XOCL_FEATURE_ROM,			\
		XOCL_RES_FEATURE_ROM_U50,		\
		ARRAY_SIZE(XOCL_RES_FEATURE_ROM_U50),	\
		.override_idx = -1,			\
	}

#define XOCL_RES_IORES_MGMT_U50					\
	((struct resource []) {					\
		__RES_CLKFREQ_K1_K2_STATIC,			\
		__RES_CLKFREQ_HBM_STATIC,			\
	 })

#define XOCL_DEVINFO_IORES_MGMT_U50				\
	{							\
		XOCL_SUBDEV_IORES,				\
		XOCL_IORES0,					\
		XOCL_RES_IORES_MGMT_U50,			\
		ARRAY_SIZE(XOCL_RES_IORES_MGMT_U50),		\
		.override_idx = -1,			\
	}

#define MGMT_RES_U50						\
	((struct xocl_subdev_info []) {				\
		XOCL_DEVINFO_FEATURE_ROM_U50,			\
		XOCL_DEVINFO_IORES_MGMT_U50,			\
		XOCL_DEVINFO_FLASH_BLP,				\
		XOCL_DEVINFO_XMC_BLP,				\
		XOCL_DEVINFO_FMGR,				\
	})

#define	XOCL_BOARD_MGMT_U50					\
	(struct xocl_board_private){				\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,	\
		.subdev_info	= MGMT_RES_U50,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_U50),		\
		.flash_type = FLASH_TYPE_SPI,			\
		.sched_bin = "xilinx/sched_u50.bin",		\
	}

#define USER_RES_U50						\
		((struct xocl_subdev_info []) {			\
			XOCL_DEVINFO_FEATURE_ROM_U50,		\
			XOCL_DEVINFO_MAILBOX_USER_U50,		\
			XOCL_DEVINFO_ICAP_USER,			\
			XOCL_DEVINFO_XMC_USER,			\
			XOCL_DEVINFO_AF_USER,			\
		})

#define	XOCL_BOARD_USER_U50					\
	(struct xocl_board_private){				\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,	\
		.subdev_info	= USER_RES_U50,			\
		.subdev_num = ARRAY_SIZE(USER_RES_U50),		\
		.p2p_bar_sz = 8, /* GB */			\
	}

#define XOCL_RES_FLASH_BLP_U25				\
	((struct resource []) {				\
		{					\
			.start	= 0x40000,		\
			.end	= 0x4ffff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

//access flash through qspi_ps in u25
#define XOCL_PRIV_FLASH_BLP_U25				\
	((struct xocl_flash_privdata) {			\
		0,					\
		FLASH_TYPE_QSPIPS_X4_SINGLE,		\
	 })

#define XOCL_DEVINFO_FLASH_BLP_U25			\
	{						\
		XOCL_SUBDEV_FLASH,			\
		XOCL_FLASH,				\
		XOCL_RES_FLASH_BLP_U25,			\
		ARRAY_SIZE(XOCL_RES_FLASH_BLP_U25),	\
		.level = XOCL_SUBDEV_LEVEL_BLD,		\
		.bar_idx = (char []){ 0 },		\
		.priv_data = &XOCL_PRIV_FLASH_BLP_U25,	\
		.data_len = sizeof(struct xocl_flash_privdata), \
		.override_idx = -1,			\
	}

#define XOCL_RES_FEATURE_ROM_U25			\
	((struct resource []) {				\
	 	{					\
	 		.name = "uuid",			\
	 		.start = 0x300000,		\
	 		.end = 0x300fff,		\
	 		.flags = IORESOURCE_MEM,	\
	 	},					\
	 })

#define	XOCL_DEVINFO_FEATURE_ROM_U25			\
	{						\
		XOCL_SUBDEV_FEATURE_ROM,		\
		XOCL_FEATURE_ROM,			\
		XOCL_RES_FEATURE_ROM_U25,		\
		ARRAY_SIZE(XOCL_RES_FEATURE_ROM_U25),	\
		.override_idx = -1,			\
	}

#define MGMT_RES_U25							\
	((struct xocl_subdev_info []) {					\
	 	XOCL_DEVINFO_FEATURE_ROM_U25,				\
	 	XOCL_DEVINFO_FLASH_BLP_U25,				\
		XOCL_DEVINFO_FMGR,      				\
	})

#define	XOCL_BOARD_MGMT_U25						\
	(struct xocl_board_private) {					\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,		\
		.subdev_info	= MGMT_RES_U25,				\
		.subdev_num = ARRAY_SIZE(MGMT_RES_U25),			\
		.flash_type = FLASH_TYPE_QSPIPS_X4_SINGLE,		\
	}

#define XOCL_DEVINFO_MAILBOX_USER_U25 	XOCL_DEVINFO_MAILBOX_USER_U50
#define USER_RES_U25							\
		((struct xocl_subdev_info []) {				\
	 		XOCL_DEVINFO_FEATURE_ROM_USER_DYN,		\
		 	XOCL_DEVINFO_MAILBOX_USER_U25,			\
		 	XOCL_DEVINFO_ICAP_USER,				\
		 	XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
		})

#define	XOCL_BOARD_USER_U25						\
	(struct xocl_board_private) {					\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP |		\
				XOCL_DSAFLAG_MB_SCHE_OFF,		\
		.subdev_info	= USER_RES_U25,				\
		.subdev_num = ARRAY_SIZE(USER_RES_U25),			\
		.flash_type = FLASH_TYPE_QSPIPS_X4_SINGLE		\
	}

#define MFG_RES_U25							\
	((struct xocl_subdev_info []) {					\
	 	XOCL_DEVINFO_FLASH_BLP_U25,				\
	 	/*XOCL_DEVINFO_XMC_MFG_U25,*/				\
	 })

#define	XOCL_BOARD_XBB_MFG_U25						\
	(struct xocl_board_private) {					\
		.flags = XOCL_DSAFLAG_MFG,				\
		.board_name = "u25",					\
		.subdev_info	= MFG_RES_U25,				\
		.subdev_num = ARRAY_SIZE(MFG_RES_U25),			\
		.flash_type = FLASH_TYPE_QSPIPS_X4_SINGLE,		\
	}

#define	XOCL_MGMT_PCI_IDS						\
	{ XOCL_PCI_DEVID(0x10EE, 0x4A47, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4A87, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B47, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B87, 0x4350, MGMT_DSA50) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B87, 0x4351, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x684F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0xA883, 0x1351, MGMT_MPSOC) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0xA983, 0x1351, MGMT_MPSOC) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x503C, PCI_ANY_ID, MGMT_U30) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x513C, PCI_ANY_ID, U30_MGMT_RAPTOR2) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x688F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x694F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6987, PCI_ANY_ID, MGMT_U2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5034, PCI_ANY_ID, MGMT_U2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x698F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A4F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4350, MGMT_6A8F_DSA50) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4351, MGMT_6A8F) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4352, MGMT_6A8F_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5030, PCI_ANY_ID, MGMT_XBB_SMARTN) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A9F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6E4F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6B0F, PCI_ANY_ID, MGMT_6B0F) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6E8F, 0x4352, MGMT_6E8F_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x888F, PCI_ANY_ID, MGMT_888F) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x898F, PCI_ANY_ID, MGMT_898F) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x788F, 0x4351, MGMT_XBB_DSA51) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x788F, 0x4352, MGMT_XBB_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x798F, 0x4352, MGMT_XBB_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4353, MGMT_6A8F_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5000, PCI_ANY_ID, MGMT_XBB_DSA52_U200) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5004, PCI_ANY_ID, MGMT_XBB_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5008, PCI_ANY_ID, MGMT_XBB_DSA52_U280) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x500C, PCI_ANY_ID, MGMT_XBB_DSA52_U280) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x5020, PCI_ANY_ID, MGMT_U50) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5028, PCI_ANY_ID, MGMT_VERSAL) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5044, PCI_ANY_ID, MGMT_VERSAL) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5048, PCI_ANY_ID, VERSAL_MGMT_RAPTOR2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5094, PCI_ANY_ID, V70_MGMT_RAPTOR2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6098, PCI_ANY_ID, VCK190_MGMT_RAPTOR2) },    \
	{ XOCL_PCI_DEVID(0x10EE, 0xE098, PCI_ANY_ID, XBB_MFG_VCK190) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x5078, PCI_ANY_ID, VERSAL_MGMT_RAPTOR2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5050, PCI_ANY_ID, MGMT_U25) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x504E, PCI_ANY_ID, U26Z_MGMT_RAPTOR2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5058, PCI_ANY_ID, U55N_MGMT_RAPTOR2) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x505C, PCI_ANY_ID, U55C_MGMT_RAPTOR2) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x5060, PCI_ANY_ID, U50LV_MGMT_RAPTOR2) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x506C, PCI_ANY_ID, U50C_MGMT_RAPTOR2) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x5074, PCI_ANY_ID, X3522PV_MGMT_RAPTOR2) },	\
	{ XOCL_PCI_DEVID(0x13FE, 0x006C, PCI_ANY_ID, MGMT_6A8F) },	\
	{ XOCL_PCI_DEVID(0x13FE, 0x0078, PCI_ANY_ID, MGMT_XBB_DSA52) },  \
	{ XOCL_PCI_DEVID(0x10EE, 0xE987, PCI_ANY_ID, XBB_MFG("u2")) },\
	{ XOCL_PCI_DEVID(0x10EE, 0xF987, PCI_ANY_ID, XBB_MFG("samsung_efuse")) },\
	{ XOCL_PCI_DEVID(0x10EE, 0xD000, PCI_ANY_ID, XBB_MFG("u200")) },\
	{ XOCL_PCI_DEVID(0x10EE, 0xD004, PCI_ANY_ID, XBB_MFG("u250")) },\
	{ XOCL_PCI_DEVID(0x10EE, 0xD008, PCI_ANY_ID, XBB_MFG("u280-es1")) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0xD00C, PCI_ANY_ID, XBB_MFG("u280")) },\
	{ XOCL_PCI_DEVID(0x10EE, 0xD030, PCI_ANY_ID, XBB_MFG("poc1465")) },\
	{ XOCL_PCI_DEVID(0x10EE, 0xD020, PCI_ANY_ID, XBB_MFG_U50) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0xD03C, PCI_ANY_ID, XBB_MFG_U30) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0xD04C, PCI_ANY_ID, XBB_MFG_U25) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0xEB10, PCI_ANY_ID, XBB_MFG("twitch")) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0x5098, PCI_ANY_ID, XBB_MFG("avalon")) },\
	{ XOCL_PCI_DEVID(0x13FE, 0x806C, PCI_ANY_ID, XBB_MFG("advantech")) }

#define	XOCL_USER_XDMA_PCI_IDS						\
	{ XOCL_PCI_DEVID(0x10EE, 0x4A48, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4A88, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B48, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B88, 0x4350, USER_XDMA_DSA50) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B88, 0x4351, USER_XDMA) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x6850, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6890, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6950, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6988, PCI_ANY_ID, USER_DSA52_U2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5035, PCI_ANY_ID, USER_DSA52_U2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0xA884, 0x1351, USER_XDMA_MPSOC) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0xA984, 0x1351, USER_XDMA_MPSOC) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x503D, PCI_ANY_ID, USER_XDMA_MPSOC) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x6990, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A50, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A90, 0x4350, USER_XDMA_DSA50) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A90, 0x4351, USER_XDMA) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A90, 0x4352, USER_DSA52) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A90, 0x4353, USER_DSA52) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x6E50, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6B10, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6E90, 0x4352, USER_DSA52) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x8890, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x8990, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x7890, 0x4351, USER_XDMA) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x7890, 0x4352, USER_DSA52) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x7990, 0x4352, USER_DSA52) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x5001, PCI_ANY_ID, USER_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5005, PCI_ANY_ID, USER_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5009, PCI_ANY_ID, USER_DSA52_U280) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x500D, PCI_ANY_ID, USER_DSA52_U280) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5021, PCI_ANY_ID, USER_U50) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5051, PCI_ANY_ID, USER_U25) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x504F, PCI_ANY_ID, U26Z_USER_RAPTOR2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x513D, PCI_ANY_ID, U30_USER_RAPTOR2) },       \
	{ XOCL_PCI_DEVID(0x10EE, 0x5059, PCI_ANY_ID, U55N_USER_RAPTOR2) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x505D, PCI_ANY_ID, U55C_USER_RAPTOR2) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x5061, PCI_ANY_ID, U50LV_USER_RAPTOR2) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x506D, PCI_ANY_ID, U50C_USER_RAPTOR2) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x5075, PCI_ANY_ID, X3522PV_USER_RAPTOR2) },	\
	{ XOCL_PCI_DEVID(0x13FE, 0x0065, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x13FE, 0x0077, PCI_ANY_ID, USER_DSA52) },	\
	{ XOCL_PCI_DEVID(0x1D0F, 0x1042, PCI_ANY_ID, USER_AWS_XDMA) },	\
	{ XOCL_PCI_DEVID(0x1D0F, 0xF010, PCI_ANY_ID, USER_AWS_XDMA) },	\
	{ XOCL_PCI_DEVID(0x1D0F, 0xF011, PCI_ANY_ID, USER_AWS_NODMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5031, PCI_ANY_ID, USER_SMARTN) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5086, PCI_ANY_ID, X3522PV_USER_RAPTOR2) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5029, PCI_ANY_ID, USER_XDMA_VERSAL) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x5045, PCI_ANY_ID, USER_XDMA_VERSAL) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x5049, PCI_ANY_ID, VERSAL_USER_RAPTOR2) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0x5095, PCI_ANY_ID, V70_USER_RAPTOR2_ES3) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0x6099, PCI_ANY_ID, VCK190_USER_RAPTOR2) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0x5079, PCI_ANY_ID, VERSAL_USER_RAPTOR2) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0x5099, PCI_ANY_ID, AVALON_USER_RAPTOR2) }

#define XOCL_DSA_VBNV_MAP						\
	{ 0x10EE, 0x5001, PCI_ANY_ID,					\
		.vbnv = "xilinx_u200_xdma_201820_1",		\
		.priv_data = &XOCL_BOARD_USER_XDMA },			\
	{ 0x10EE, 0x5000, PCI_ANY_ID,					\
		.vbnv = "xilinx_u200_xdma_201820_1",		\
		.priv_data = &XOCL_BOARD_MGMT_XBB_DSA51 },		\
	{ 0x10EE, 0x5005, PCI_ANY_ID,					\
		.vbnv = "xilinx_u250_xdma_201830_1",		\
		.priv_data = &XOCL_BOARD_USER_DSA_U250_NO_KDMA },	\
	{0x10EE, 0x5014, PCI_ANY_ID,					\
		.vbnv = "xilinx_u250_qep_201910_1",		  	\
		.priv_data = &XOCL_BOARD_MGMT_U250_QEP }

#define XOCL_DSA_DYNAMIC_MAP						\
	{ 0x10EE, 0x5001, PCI_ANY_ID,					\
		.vbnv = "xilinx_u200_xdma_201920_1",			\
		.priv_data = &XOCL_BOARD_U200_USER_EA,			\
		.type = XOCL_DSAMAP_DYNAMIC },				\
	{ 0x10EE, 0x5000, PCI_ANY_ID,					\
		.vbnv = "xilinx_u200_xdma_201920_1",			\
		.priv_data = &XOCL_BOARD_U200_MGMT_EA,			\
		.type = XOCL_DSAMAP_DYNAMIC },				\
	{ 0x10EE, 0x5001, PCI_ANY_ID,					\
		.vbnv = "xilinx_u200",			\
		.priv_data = &XOCL_BOARD_U200_USER_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5000, PCI_ANY_ID,					\
		.vbnv = "xilinx_u200",			\
		.priv_data = &XOCL_BOARD_U200_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5005, PCI_ANY_ID,					\
		.vbnv = "xilinx_u250",			\
		.priv_data = &XOCL_BOARD_U250_USER_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5004, PCI_ANY_ID,					\
		.vbnv = "xilinx_u250",			\
		.priv_data = &XOCL_BOARD_U250_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x500D, PCI_ANY_ID,					\
		.vbnv = "xilinx_u280",			\
		.priv_data = &XOCL_BOARD_U280_USER_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x500C, PCI_ANY_ID,					\
		.vbnv = "xilinx_u280",			\
		.priv_data = &XOCL_BOARD_U280_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5020, PCI_ANY_ID,					\
		.vbnv = "xilinx_u50",		\
		.priv_data = &XOCL_BOARD_U50_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5021, PCI_ANY_ID,					\
		.vbnv = "xilinx_u50",		\
		.priv_data = &XOCL_BOARD_U50_USER_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5044, PCI_ANY_ID,					\
		.vbnv = "xilinx_vck5000-es1",				\
		.priv_data = &XOCL_BOARD_VERSAL_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5045, PCI_ANY_ID,					\
		.vbnv = "xilinx_vck5000-es1",				\
		.priv_data = &XOCL_BOARD_VERSAL_USER_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5048, PCI_ANY_ID,					\
		.vbnv = "xilinx_vck5000",				\
		.priv_data = &XOCL_BOARD_VERSAL_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5049, PCI_ANY_ID,					\
		.vbnv = "xilinx_vck5000",				\
		.priv_data = &XOCL_BOARD_VERSAL_USER_RAPTOR2_ES3,	\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5094, PCI_ANY_ID,					\
		.vbnv = "xilinx_v70",				\
		.priv_data = &XOCL_BOARD_V70_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5095, PCI_ANY_ID,					\
		.vbnv = "xilinx_v70",				\
		.priv_data = &XOCL_BOARD_V70_USER_RAPTOR2_ES3,	\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x6098, PCI_ANY_ID,					\
                .vbnv = "xilinx_vck190",				\
                .priv_data = &XOCL_BOARD_VCK190_MGMT_RAPTOR2,		\
                .type = XOCL_DSAMAP_RAPTOR2 },                        \
        { 0x10EE, 0x6099, PCI_ANY_ID,					\
                .vbnv = "xilinx_vck190",				\
                .priv_data = &XOCL_BOARD_VCK190_USER_RAPTOR2,		\
                .type = XOCL_DSAMAP_RAPTOR2 },                        \
	{ 0x10EE, 0x5078, PCI_ANY_ID,					\
		.vbnv = "xilinx_v65",					\
		.priv_data = &XOCL_BOARD_VERSAL_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5079, PCI_ANY_ID,					\
		.vbnv = "xilinx_v65",					\
		.priv_data = &XOCL_BOARD_VERSAL_USER_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5050, PCI_ANY_ID,                                   \
		.vbnv = "xilinx_u25",					\
		.priv_data = &XOCL_BOARD_U25_MGMT_RAPTOR2,              \
		.type = XOCL_DSAMAP_RAPTOR2 },                          \
	{ 0x10EE, 0x5051, PCI_ANY_ID,                                   \
		.vbnv = "xilinx_u25",					\
		.priv_data = &XOCL_BOARD_U25_USER_RAPTOR2,              \
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x6987, PCI_ANY_ID,					\
		.vbnv = "xilinx_u2",					\
		.priv_data = &XOCL_BOARD_U2_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x6988, PCI_ANY_ID,					\
		.vbnv = "xilinx_u2",					\
		.priv_data = &XOCL_BOARD_U2_USER_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x513C, PCI_ANY_ID,					\
		.vbnv = "xilinx_u30",					\
		.priv_data = &XOCL_BOARD_U30_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x513D, PCI_ANY_ID,					\
		.vbnv = "xilinx_u30",					\
		.priv_data = &XOCL_BOARD_U30_USER_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5099, PCI_ANY_ID,					\
		.vbnv = "xilinx_avalon",				\
		.priv_data = &XOCL_BOARD_AVALON_USER_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 },				\
	{ 0x10EE, 0x5098, PCI_ANY_ID,					\
		.vbnv = "xilinx_avalon",				\
		.priv_data = &XOCL_BOARD_AVALON_MGMT_RAPTOR2,		\
		.type = XOCL_DSAMAP_RAPTOR2 }

#endif
