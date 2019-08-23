/*
 *  Copyright (C) 2018, Xilinx Inc
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
	XOCL_DSAFLAG_PCI_RESET_OFF		= 0x01,
	XOCL_DSAFLAG_MB_SCHE_OFF		= 0x02,
	XOCL_DSAFLAG_AXILITE_FLUSH		= 0x04,
	XOCL_DSAFLAG_SET_DSA_VER		= 0x08,
	XOCL_DSAFLAG_SET_XPR			= 0x10,
	XOCL_DSAFLAG_MFG			= 0x20,
	XOCL_DSAFLAG_FIXED_INTR			= 0x40,
	XOCL_DSAFLAG_NO_KDMA			= 0x80,
	XOCL_DSAFLAG_CUDMA_OFF			= 0x100,
	XOCL_DSAFLAG_DYNAMIC_IP			= 0x200,
	XOCL_DSAFLAG_SMARTN			= 0x400,
};

#define	FLASH_TYPE_SPI	"spi"
#define	FLASH_TYPE_QSPIPS	"qspi_ps"

#define XOCL_SUBDEV_MAX_RES		32
#define XOCL_SUBDEV_RES_NAME_LEN	64
#define XOCL_SUBDEV_MAX_INST		64

enum {
	XOCL_SUBDEV_LEVEL_STATIC,
	XOCL_SUBDEV_LEVEL_BLD,
	XOCL_SUBDEV_LEVEL_PRP,
	XOCL_SUBDEV_LEVEL_URP,
	XOCL_SUBDEV_LEVEL_MAX,
};
struct xocl_subdev_info {
	uint32_t		id;
	const char		*name;
	struct resource	*res;
	int				num_res;
	void			*priv_data;
	int				data_len;
	bool			multi_inst;
	int				level;
	char			*bar_idx;
	int				dyn_ip;
	const char		*override_name;
	int				override_idx;
};

struct xocl_board_private {
	uint64_t		flags;
	struct xocl_subdev_info	*subdev_info;
	uint32_t		subdev_num;
	uint32_t		dsa_ver;
	bool			xpr;
	char			*flash_type; /* used by xbflash */
	char			*board_name; /* used by xbflash */
	bool			mpsoc;
	uint64_t		p2p_bar_sz;
};

struct xocl_flash_privdata {
	u32			flash_type;
	u32			properties;
	uint64_t		data[1];
};

struct xocl_msix_privdata {
	u32			start;
	u32			total;
};

#ifdef __KERNEL__
#define XOCL_PCI_DEVID(ven, dev, subsysid, priv)        \
         .vendor = ven, .device=dev, .subvendor = PCI_ANY_ID, \
         .subdevice = subsysid, .driver_data =          \
         (kernel_ulong_t) &XOCL_BOARD_##priv

struct xocl_dsa_vbnv_map {
	uint16_t		vendor;
	uint16_t		device;
	uint16_t		subdevice;
	char			*vbnv;
	struct xocl_board_private	*priv_data;
};

#else
struct xocl_board_info {
	uint16_t		vendor;
	uint16_t		device;
	uint16_t		subdevice;
	struct xocl_board_private	*priv_data;
};

#define XOCL_PCI_DEVID(ven, dev, subsysid, priv)        \
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

#define XOCL_FEATURE_ROM	"rom"
#define XOCL_IORES0		"iores0"
#define XOCL_IORES1		"iores1"
#define XOCL_IORES2		"iores2"
#define XOCL_XDMA		"dma.xdma"
#define XOCL_QDMA		"dma.qdma"
#define XOCL_MB_SCHEDULER	"mb_scheduler"
#define XOCL_XVC_PUB		"xvc_pub"
#define XOCL_XVC_PRI		"xvc_pri"
#define XOCL_NIFD_PRI		"nifd_pri"
#define XOCL_SYSMON		"sysmon"
#define XOCL_FIREWALL		"firewall"
#define	XOCL_MB			"microblaze"
#define	XOCL_XIIC		"xiic"
#define	XOCL_MAILBOX		"mailbox"
#define	XOCL_ICAP		"icap"
#define	XOCL_AXIGATE		"axigate"
#define	XOCL_MIG		"mig"
#define	XOCL_XMC		"xmc"
#define	XOCL_DNA		"dna"
#define	XOCL_FMGR		"fmgr"
#define	XOCL_FLASH		"flash"
#define XOCL_DMA_MSIX		"dma_msix"

#define XOCL_DEVNAME(str)	str SUBDEV_SUFFIX

enum subdev_id {
	XOCL_SUBDEV_FEATURE_ROM,
	XOCL_SUBDEV_IORES,
	XOCL_SUBDEV_FLASH,
	XOCL_SUBDEV_DMA,
	XOCL_SUBDEV_MB_SCHEDULER,
	XOCL_SUBDEV_XVC_PUB,
	XOCL_SUBDEV_XVC_PRI,
	XOCL_SUBDEV_NIFD_PRI,
	XOCL_SUBDEV_SYSMON,
	XOCL_SUBDEV_AF,
	XOCL_SUBDEV_MIG,
	XOCL_SUBDEV_MB,
	XOCL_SUBDEV_XIIC,
	XOCL_SUBDEV_MAILBOX,
	XOCL_SUBDEV_AXIGATE,
	XOCL_SUBDEV_ICAP,
	XOCL_SUBDEV_DNA,
	XOCL_SUBDEV_FMGR,
	XOCL_SUBDEV_MIG_HBM,
	XOCL_SUBDEV_NUM
};

#define	XOCL_SUBDEV_MAP_USERPF_ONLY		0x1
struct xocl_subdev_map {
	int	id;
	const char *dev_name;
	char	*res_names[XOCL_SUBDEV_MAX_RES];
	u32	required_ip;
	u32	flags;
	void	*(*build_priv_data)(void *dev_hdl, void *subdev, size_t *len);
	void	(*devinfo_cb)(void *dev_hdl, void *subdevs, int num);
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
	}

#define	XOCL_RES_MIG_HBM				\
		((struct resource []) {			\
			{				\
			.start	= 0x5800,		\
			.end 	= 0x58FF,		\
			.flags  = IORESOURCE_MEM,	\
			}				\
		})

#define	XOCL_DEVINFO_MIG_HBM				\
	{						\
		XOCL_SUBDEV_MIG,			\
		XOCL_MIG,				\
		XOCL_RES_MIG_HBM,			\
		ARRAY_SIZE(XOCL_RES_MIG_HBM),		\
		.level = XOCL_SUBDEV_LEVEL_URP,		\
		.multi_inst = true,			\
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
	}

#define	XOCL_DEVINFO_AF_USER				\
	{						\
		XOCL_SUBDEV_AF,				\
		XOCL_FIREWALL,				\
		NULL,					\
		0,					\
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
	}

#define __RES_PRP_IORES_MGMT				\
		{					\
			.name	= RESNAME_MEMCALIB,	\
			.start	= 0x032000,		\
			.end	= 0x032003,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.name	= RESNAME_GATEPRPRP,	\
			.start	= 0x030000,		\
			.end	= 0x03000b,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.name	= RESNAME_CLKWIZKERNEL1,\
			.start	= 0x050000,		\
			.end	= 0x050fff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.name	= RESNAME_CLKWIZKERNEL2,\
			.start	= 0x051000,		\
			.end	= 0x051fff,		\
			.flags  = IORESOURCE_MEM,	\
		}

#define __RES_PRP_IORES_MGMT_SMARTN			\
		{					\
			.name	= RESNAME_MEMCALIB,	\
			.start	= 0x135000,		\
			.end	= 0x135003,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		{					\
			.name	= RESNAME_GATEPRPRP,	\
			.start	= 0x134000,		\
			.end	= 0x13400b,		\
			.flags  = IORESOURCE_MEM,	\
		}


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

#define	XOCL_RES_PRP_IORES_MGMT_U280			\
	((struct resource []) {				\
		__RES_PRP_IORES_MGMT,			\
		/* OCL_CLKWIZ2_BASE */			\
		{					\
			.name	= RESNAME_CLKWIZKERNEL3,\
			.start	= 0x053000,		\
			.end	= 0x053fff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define	XOCL_DEVINFO_PRP_IORES_MGMT_U280		\
	{						\
		XOCL_SUBDEV_IORES,			\
		XOCL_IORES2,				\
		XOCL_RES_PRP_IORES_MGMT_U280,		\
		ARRAY_SIZE(XOCL_RES_PRP_IORES_MGMT_U280),	\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
		.override_idx = XOCL_SUBDEV_LEVEL_PRP,	\
	}

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

#define XOCL_RES_IORES_MGMT				\
	((struct resource []) {				\
		/* OCL_CLKFREQ_BASE */			\
		{					\
			.name	= RESNAME_CLKFREQ1,	\
			.start	= 0x052000,		\
			.end	= 0x052fff,		\
			.flags  = IORESOURCE_MEM,	\
		}					\
	 })

#define XOCL_RES_IORES_MGMT_U280			\
	((struct resource []) {				\
		/* OCL_CLKFREQ_BASE */			\
		{					\
			.name	= RESNAME_CLKFREQ1,	\
			.start	= 0x052000,		\
			.end	= 0x052fff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		/* OCL_CLKFREQ_BASE */			\
		{					\
			.name	= RESNAME_CLKFREQ2,	\
			.start	= 0x055000,		\
			.end	= 0x055fff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	 })

#define	XOCL_DEVINFO_IORES_MGMT				\
	{						\
		XOCL_SUBDEV_IORES,			\
		XOCL_IORES0,				\
		XOCL_RES_IORES_MGMT,			\
		ARRAY_SIZE(XOCL_RES_IORES_MGMT),	\
	}

#define	XOCL_DEVINFO_IORES_MGMT_U280		\
	{						\
		XOCL_SUBDEV_IORES,			\
		XOCL_IORES0,				\
		XOCL_RES_IORES_MGMT_U280,		\
		ARRAY_SIZE(XOCL_RES_IORES_MGMT_U280),	\
	}

#define	XOCL_DEVINFO_ICAP_USER				\
	{						\
		XOCL_SUBDEV_ICAP,			\
		XOCL_ICAP,				\
		NULL,					\
		0,					\
	}

#define	XOCL_RES_XMC					\
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
			{				\
			.start	= 0x190000,		\
			.end 	= 0x19FFFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			/* RUNTIME CLOCK SCALING FEATURE BASE */	\
			{				\
			.start	= 0x053000,		\
			.end	= 0x053fff,		\
			.flags	= IORESOURCE_MEM,	\
			},				\
		})

#define	XOCL_DEVINFO_XMC					\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		XOCL_RES_XMC,				\
		ARRAY_SIZE(XOCL_RES_XMC),		\
	}

#define	XOCL_DEVINFO_XMC_USER			\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		NULL,					\
		0,					\
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
	}

#define XOCL_RES_QDMA					\
	((struct resource []) {				\
		{					\
			.start = 0x0,			\
			.end = 0x0,			\
			.flags = IORESOURCE_MEM,	\
		},					\
	 })

#define	XOCL_DEVINFO_QDMA				\
	{						\
		XOCL_SUBDEV_DMA,			\
		XOCL_QDMA,				\
		XOCL_RES_QDMA,				\
		ARRAY_SIZE(XOCL_RES_QDMA),		\
		.bar_idx = (char []){ 2 },		\
	}

#define	XOCL_DEVINFO_XDMA				\
	{						\
		XOCL_SUBDEV_DMA,			\
		XOCL_XDMA,				\
		NULL,					\
		0,					\
	}

#define	XOCL_DEVINFO_DMA_MSIX				\
	{						\
		.id = XOCL_SUBDEV_DMA,			\
		.name = XOCL_DMA_MSIX,			\
	}

#define XOCL_RES_SCHEDULER				\
		((struct resource []) {			\
		/*
 		 * map entire bar for now because scheduler directly
		 * programs CUs
		 */					\
			{				\
			.start	= ERT_CSR_ADDR,		\
			.end	= ERT_CSR_ADDR + 0xfff,	\
			.flags	= IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= ERT_CQ_BASE_ADDR,	\
			.end	= ERT_CQ_BASE_ADDR +	\
		       		ERT_CQ_SIZE - 1,	\
			.flags	= IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0,			\
			.end	= 3,			\
			.flags	= IORESOURCE_IRQ,	\
			}				\
		})


#define	XOCL_DEVINFO_SCHEDULER				\
	{						\
		XOCL_SUBDEV_MB_SCHEDULER,		\
		XOCL_MB_SCHEDULER,			\
		XOCL_RES_SCHEDULER,			\
		ARRAY_SIZE(XOCL_RES_SCHEDULER),		\
		&(char []){1},				\
		1					\
	}

#define XOCL_RES_SCHEDULER_QDMA				\
		((struct resource []) {			\
			{				\
			.start	= ERT_CSR_ADDR,		\
			.end	= ERT_CSR_ADDR + 0xfff,	\
			.flags	= IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= ERT_CQ_BASE_ADDR,	\
			.end	= ERT_CQ_BASE_ADDR +	\
		       		ERT_CQ_SIZE - 1,	\
			.flags	= IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 2,			\
			.end	= 5,			\
			.flags	= IORESOURCE_IRQ,	\
			}				\
		})


#define	XOCL_DEVINFO_SCHEDULER_QDMA				\
	{						\
		XOCL_SUBDEV_MB_SCHEDULER,		\
		XOCL_MB_SCHEDULER,			\
		XOCL_RES_SCHEDULER_QDMA,			\
		ARRAY_SIZE(XOCL_RES_SCHEDULER_QDMA),		\
		&(char []){1},				\
		1					\
	}

#define	XOCL_DEVINFO_SCHEDULER_51				\
	{						\
		XOCL_SUBDEV_MB_SCHEDULER,		\
		XOCL_MB_SCHEDULER,			\
		XOCL_RES_SCHEDULER,			\
		ARRAY_SIZE(XOCL_RES_SCHEDULER),		\
		&(char []){0},				\
		1					\
	}

#define	XOCL_DEVINFO_FMGR				\
	{						\
		XOCL_SUBDEV_FMGR,			\
		XOCL_FMGR,				\
		NULL,					\
		0,					\
	}


/* user pf defines */
#define	USER_RES_QDMA							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_QDMA,				\
			XOCL_DEVINFO_SCHEDULER_QDMA, 			\
			XOCL_DEVINFO_XVC_PUB,				\
		 	XOCL_DEVINFO_MAILBOX_USER_QDMA,			\
			XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
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
			XOCL_DEVINFO_SCHEDULER_51,			\
			XOCL_DEVINFO_ICAP_USER,				\
		})

#define	USER_RES_XDMA							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_SCHEDULER_51,			\
			XOCL_DEVINFO_MAILBOX_USER,			\
			XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
		})

#define USER_RES_AWS							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_SCHEDULER_51,			\
			XOCL_DEVINFO_ICAP_USER,				\
		})

#define	USER_RES_DSA52							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_SCHEDULER,				\
			XOCL_DEVINFO_MAILBOX_USER,			\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_ICAP_USER,				\
			XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
		})

#define USER_RES_SMARTN							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM_SMARTN,		\
			XOCL_DEVINFO_SCHEDULER_DYN,			\
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

#define	XOCL_BOARD_USER_XDMA_ERT_OFF					\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_MB_SCHE_OFF,		\
		.subdev_info	= USER_RES_XDMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_XDMA),		\
	}

#define XOCL_BOARD_USER_AWS                         \
   (struct xocl_board_private){                    \
       .flags      = 0,                    \
       .subdev_info    = USER_RES_AWS,         \
       .subdev_num = ARRAY_SIZE(USER_RES_AWS),     \
   }

#define	XOCL_BOARD_USER_DSA52						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_DSA52,			\
		.subdev_num = ARRAY_SIZE(USER_RES_DSA52),		\
	}

#define	XOCL_BOARD_USER_DSA52_U280						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_DSA52,			\
		.subdev_num = ARRAY_SIZE(USER_RES_DSA52),		\
		.p2p_bar_sz = 8,					\
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
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,		        	\
		})

#define	MGMT_RES_DSA50							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
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

#define	MGMT_RES_6A8F							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
		})

#define	MGMT_RES_6A8F_DSA50						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
		})

#define	MGMT_RES_XBB_DSA51						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_XMC,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
		})

#define	XOCL_BOARD_MGMT_6A8F						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6A8F,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6A8F),		\
	}

#define	XOCL_BOARD_MGMT_XBB_DSA51						\
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
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT_QDMA,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
		})


#define	XOCL_BOARD_MGMT_QDMA					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_QDMA,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_QDMA),		\
		.flash_type = FLASH_TYPE_SPI				\
	}

#define MGMT_RES_XBB_QDMA                                               \
	((struct xocl_subdev_info []) {                         \
		XOCL_DEVINFO_FEATURE_ROM,                       \
		XOCL_DEVINFO_IORES_MGMT,			\
		XOCL_DEVINFO_PRP_IORES_MGMT,			\
		XOCL_DEVINFO_AF_DSA52,                          \
		XOCL_DEVINFO_XMC,                               \
		XOCL_DEVINFO_XVC_PRI,                           \
		XOCL_DEVINFO_NIFD_PRI,				\
		XOCL_DEVINFO_MAILBOX_MGMT_QDMA,			\
		XOCL_DEVINFO_ICAP_MGMT,                         \
		XOCL_DEVINFO_FMGR,      			\
	})

#define XOCL_BOARD_MGMT_XBB_QDMA                                        \
	(struct xocl_board_private){                                    \
		.flags          = XOCL_DSAFLAG_FIXED_INTR,		\
		.subdev_info    = MGMT_RES_XBB_QDMA,                    \
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_QDMA),            \
		.flash_type = FLASH_TYPE_SPI				\
	}

#define	XOCL_BOARD_MGMT_6B0F		XOCL_BOARD_MGMT_6A8F

#define	MGMT_RES_6A8F_DSA52						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF_DSA52,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
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
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_AF_DSA52,				\
			XOCL_DEVINFO_XMC,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_NIFD_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
		})

#define	XOCL_BOARD_MGMT_XBB_DSA52					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_XBB_DSA52,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_DSA52),		\
		.flash_type = FLASH_TYPE_SPI,				\
	}


#define	MGMT_RES_XBB_DSA52_U280						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_IORES_MGMT_U280,			\
			XOCL_DEVINFO_PRP_IORES_MGMT_U280,		\
			XOCL_DEVINFO_AF_DSA52,				\
			XOCL_DEVINFO_XMC,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,			\
			XOCL_DEVINFO_FMGR,      			\
		})

#define	XOCL_BOARD_MGMT_XBB_DSA52_U280					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_XBB_DSA52_U280,		\
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_DSA52_U280),	\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define MGMT_RES_XBB_QDMA_U280                                               \
	((struct xocl_subdev_info []) {                         \
		XOCL_DEVINFO_FEATURE_ROM,                       \
		XOCL_DEVINFO_IORES_MGMT_U280,			\
		XOCL_DEVINFO_PRP_IORES_MGMT_U280,		\
		XOCL_DEVINFO_AF_DSA52,                          \
		XOCL_DEVINFO_XMC,                               \
		XOCL_DEVINFO_XVC_PRI,                           \
		XOCL_DEVINFO_MAILBOX_MGMT_QDMA,			\
		XOCL_DEVINFO_ICAP_MGMT,                    \
		XOCL_DEVINFO_FMGR,      			\
	})

#define XOCL_BOARD_MGMT_XBB_QDMA_U280                                   \
	(struct xocl_board_private){                                    \
		.flags          = XOCL_DSAFLAG_FIXED_INTR,		\
		.subdev_info    = MGMT_RES_XBB_QDMA_U280,               \
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_QDMA_U280),       \
		.flash_type = FLASH_TYPE_SPI				\
	}

#define MGMT_RES_XBB_SMARTN                                     \
	((struct xocl_subdev_info []) {                         \
		XOCL_DEVINFO_FEATURE_ROM_SMARTN,		\
		XOCL_DEVINFO_PRP_IORES_MGMT_SMARTN,		\
		XOCL_DEVINFO_XMC,                               \
		XOCL_DEVINFO_MAILBOX_MGMT_QDMA,			\
		XOCL_DEVINFO_ICAP_MGMT_SMARTN,                  \
		XOCL_DEVINFO_FMGR,      			\
	})

#define XOCL_BOARD_MGMT_XBB_SMARTN                                  	\
	(struct xocl_board_private){                                    \
		.flags          = XOCL_DSAFLAG_SMARTN,		\
		.subdev_info    = MGMT_RES_XBB_SMARTN,               \
		.subdev_num = ARRAY_SIZE(MGMT_RES_XBB_SMARTN),       \
		.flash_type = FLASH_TYPE_SPI				\
	}

#define	MGMT_RES_6E8F_DSA52						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
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
			XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_PRP_IORES_MGMT,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
			XOCL_DEVINFO_FMGR,      			\
		})

#define	XOCL_BOARD_MGMT_MPSOC						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_MPSOC,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_MPSOC),		\
		.mpsoc = true,						\
		.board_name = "samsung",				\
		.flash_type = FLASH_TYPE_QSPIPS,			\
	}

#define	XOCL_BOARD_USER_XDMA_MPSOC					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_XDMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_XDMA),		\
		.mpsoc = true,						\
	}

#define XOCL_RES_FLASH_MFG_U50				\
	((struct resource []) {				\
		{					\
			.start = 0x1f50000,		\
			.end = 0x1f5FFFF,		\
			.flags = IORESOURCE_MEM,	\
		},					\
	 })

#define XOCL_DEVINFO_FLASH_MFG_U50				\
	{						\
		XOCL_SUBDEV_FLASH,			\
		XOCL_FLASH,				\
		XOCL_RES_FLASH_MFG_U50,			\
		ARRAY_SIZE(XOCL_RES_FLASH_MFG_U50),	\
	}

#define	XOCL_RES_XMC_MFG_U50					\
		((struct resource []) {			\
			{				\
			.start	= 0x140000,		\
			.end 	= 0x141FFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
			{				\
			.start	= 0x180000,		\
			.end 	= 0x181FFF,		\
			.flags  = IORESOURCE_MEM,	\
			},				\
		})

#define	XOCL_DEVINFO_XMC_MFG_U50			\
	{						\
		XOCL_SUBDEV_MB,				\
		XOCL_XMC,				\
		XOCL_RES_XMC_MFG_U50,			\
		ARRAY_SIZE(XOCL_RES_XMC_MFG_U50),	\
	}

#define MFG_RES_U50							\
	((struct xocl_subdev_info []) {					\
	 	XOCL_DEVINFO_FLASH_MFG_U50,				\
	 	XOCL_DEVINFO_XMC_MFG_U50,				\
	 })

#define	XOCL_BOARD_XBB_MFG_U50						\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_MFG,				\
		.board_name = "u50",					\
		.subdev_info	= MFG_RES_U50,				\
		.subdev_num = ARRAY_SIZE(MFG_RES_U50),			\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define	XOCL_BOARD_XBB_MFG(board)					\
	(struct xocl_board_private){					\
		.flags = XOCL_DSAFLAG_MFG,				\
		.board_name = board,					\
		.flash_type = FLASH_TYPE_SPI,				\
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
	}

#define	XOCL_DEVINFO_FEATURE_ROM_USER_DYN		\
	{						\
		XOCL_SUBDEV_FEATURE_ROM,		\
		XOCL_FEATURE_ROM,			\
		NULL,					\
		0,					\
		.dyn_ip = 1,				\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
	}

#define XOCL_RES_MAILBOX_PRP				\
	((struct resource []) {				\
		{					\
			.start	= 0x0,		\
			.end	= 0x2F,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	})

#define XOCL_DEVINFO_MAILBOX_PRP			\
	{						\
		XOCL_SUBDEV_MAILBOX,			\
		XOCL_MAILBOX,				\
		XOCL_RES_MAILBOX_PRP,			\
		ARRAY_SIZE(XOCL_RES_MAILBOX_PRP),	\
		.level = XOCL_SUBDEV_LEVEL_PRP,		\
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
	}

#define MGMT_RES_DYNAMIC_IP						\
		((struct xocl_subdev_info []) {				\
		 	XOCL_DEVINFO_FEATURE_ROM_DYN,			\
		 	XOCL_DEVINFO_IORES_MGMT,			\
			XOCL_DEVINFO_FMGR,      			\
		})

#define	XOCL_BOARD_MGMT_DYNAMIC_IP					\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,		\
		.subdev_info	= MGMT_RES_DYNAMIC_IP,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_DYNAMIC_IP),		\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define	XOCL_DEVINFO_SCHEDULER_DYN				\
	{						\
		XOCL_SUBDEV_MB_SCHEDULER,		\
		XOCL_MB_SCHEDULER,			\
		NULL,					\
		0,					\
		&(char []){1},				\
		1,					\
		.level = XOCL_SUBDEV_LEVEL_PRP,         \
	}

#define USER_RES_DYNAMIC_IP						\
		((struct xocl_subdev_info []) {				\
		 	XOCL_DEVINFO_FEATURE_ROM_USER_DYN,		\
		 	XOCL_DEVINFO_SCHEDULER_DYN,			\
		 	XOCL_DEVINFO_ICAP_USER,				\
		 	XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
		})

#define	XOCL_BOARD_USER_DYNAMIC_IP					\
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
	}

#define XOCL_RES_IORES_MGMT_U50				\
	((struct resource []) {				\
		/* OCL_CLKFREQ_BASE */			\
		{					\
			.name	= RESNAME_CLKFREQ1,	\
			.start	= 0x1000000,		\
			.end	= 0x1000fff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		/* OCL_CLKFREQ_BASE */			\
		{					\
			.name	= RESNAME_CLKFREQ2,	\
			.start	= 0x1001000,		\
			.end	= 0x1001fff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
	 })

#define	XOCL_DEVINFO_IORES_MGMT_U50			\
	{						\
		XOCL_SUBDEV_IORES,			\
		XOCL_IORES0,				\
		XOCL_RES_IORES_MGMT_U50,		\
		ARRAY_SIZE(XOCL_RES_IORES_MGMT_U50),	\
	}


#define MGMT_RES_U50							\
	((struct xocl_subdev_info []) {					\
	 	XOCL_DEVINFO_FEATURE_ROM_U50,				\
	 	XOCL_DEVINFO_IORES_MGMT_U50,				\
		XOCL_DEVINFO_FMGR,      				\
	})

#define	XOCL_BOARD_MGMT_U50						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,		\
		.subdev_info	= MGMT_RES_U50,				\
		.subdev_num = ARRAY_SIZE(MGMT_RES_U50),			\
		.flash_type = FLASH_TYPE_SPI,				\
	}

#define USER_RES_U50							\
		((struct xocl_subdev_info []) {				\
		 	XOCL_DEVINFO_FEATURE_ROM_U50,			\
		 	XOCL_DEVINFO_MAILBOX_USER_U50,			\
		 	XOCL_DEVINFO_ICAP_USER,				\
		 	XOCL_DEVINFO_XMC_USER,				\
			XOCL_DEVINFO_AF_USER,				\
		})

#define	XOCL_BOARD_USER_U50						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_DYNAMIC_IP,		\
		.subdev_info	= USER_RES_U50,				\
		.subdev_num = ARRAY_SIZE(USER_RES_U50),			\
		.p2p_bar_sz = 8, /* GB */				\
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
	{ XOCL_PCI_DEVID(0x10EE, 0x688F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x694F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6987, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x698F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A4F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4350, MGMT_6A8F_DSA50) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4351, MGMT_6A8F) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4352, MGMT_6A8F_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A9F, 0x4360, MGMT_QDMA) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x5010, PCI_ANY_ID, MGMT_XBB_QDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5014, PCI_ANY_ID, MGMT_XBB_QDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5018, PCI_ANY_ID, MGMT_XBB_QDMA_U280) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x501C, PCI_ANY_ID, MGMT_XBB_QDMA_U280) },\
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
	{ XOCL_PCI_DEVID(0x10EE, 0x5000, PCI_ANY_ID, MGMT_XBB_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5004, PCI_ANY_ID, MGMT_XBB_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5008, PCI_ANY_ID, MGMT_XBB_DSA52_U280) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x500C, PCI_ANY_ID, MGMT_XBB_DSA52_U280) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x7020, PCI_ANY_ID, MGMT_DYNAMIC_IP) },\
	{ XOCL_PCI_DEVID(0x10EE, 0x5020, PCI_ANY_ID, MGMT_U50) },	\
	{ XOCL_PCI_DEVID(0x13FE, 0x006C, PCI_ANY_ID, MGMT_6A8F) },	\
	{ XOCL_PCI_DEVID(0x13FE, 0x0078, PCI_ANY_ID, MGMT_XBB_DSA52) },  \
	{ XOCL_PCI_DEVID(0x10EE, 0xD000, PCI_ANY_ID, XBB_MFG("u200")) },\
	{ XOCL_PCI_DEVID(0x10EE, 0xD004, PCI_ANY_ID, XBB_MFG("u250")) },\
	{ XOCL_PCI_DEVID(0x10EE, 0xD008, PCI_ANY_ID, XBB_MFG("u280-es1")) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0xD00C, PCI_ANY_ID, XBB_MFG("u280")) },\
	{ XOCL_PCI_DEVID(0x10EE, 0xD020, PCI_ANY_ID, XBB_MFG_U50) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0xEB10, PCI_ANY_ID, XBB_MFG("twitch")) }, \
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
	{ XOCL_PCI_DEVID(0x10EE, 0x6988, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0xA884, 0x1351, USER_XDMA_MPSOC) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0xA984, 0x1351, USER_XDMA_MPSOC) },	\
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
	{ XOCL_PCI_DEVID(0x10EE, 0x7021, PCI_ANY_ID, USER_DYNAMIC_IP) }, \
	{ XOCL_PCI_DEVID(0x10EE, 0x5021, PCI_ANY_ID, USER_U50) },	\
	{ XOCL_PCI_DEVID(0x13FE, 0x0065, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x13FE, 0x0077, PCI_ANY_ID, USER_DSA52) },	\
	{ XOCL_PCI_DEVID(0x1D0F, 0x1042, PCI_ANY_ID, USER_AWS) },	\
	{ XOCL_PCI_DEVID(0x1D0F, 0xF000, PCI_ANY_ID, USER_AWS) },	\
	{ XOCL_PCI_DEVID(0x1D0F, 0xF010, PCI_ANY_ID, USER_AWS) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6AA0, 0x4360, USER_QDMA) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x5011, PCI_ANY_ID, USER_QDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5015, PCI_ANY_ID, USER_QDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5019, PCI_ANY_ID, USER_QDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x501D, PCI_ANY_ID, USER_QDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x5031, PCI_ANY_ID, USER_SMARTN) }
#define XOCL_DSA_VBNV_MAP						\
	{ 0x10EE, 0x5001, PCI_ANY_ID, "xilinx_u200_xdma_201820_1",	\
		&XOCL_BOARD_USER_XDMA },				\
	{ 0x10EE, 0x5000, PCI_ANY_ID, "xilinx_u200_xdma_201820_1",	\
		&XOCL_BOARD_MGMT_XBB_DSA51 },				\
	{ 0x10EE, 0x5005, PCI_ANY_ID, "xilinx_u250_xdma_201830_1",	\
		&XOCL_BOARD_USER_DSA_U250_NO_KDMA }

#endif
