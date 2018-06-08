/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@Xilinx.com
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

#ifndef _XOCL_SUBDEV_H
#define	_XOCL_SUBDEV_H

#define	MGMT_SUFFIX		".m"
#define	USER_SUFFIX		".u"

#define XOCL_FEATURE_ROM        "rom" SUBDEV_SUFFIX
#define XOCL_MM_XDMA            "mm_dma.v5" SUBDEV_SUFFIX
#define XOCL_MM_QDMA            "mm_dma.v6" SUBDEV_SUFFIX
#define	XOCL_STR_QDMA		"str_dma" SUBDEV_SUFFIX
#define XOCL_MB_SCHEDULER       "mb_scheduler" SUBDEV_SUFFIX
#define XOCL_XVC_PUB            "xvc_pub" SUBDEV_SUFFIX
#define XOCL_XVC_PRI            "xvc_pri" SUBDEV_SUFFIX
#define XOCL_SYSMON             "sysmon" SUBDEV_SUFFIX
#define XOCL_FIREWALL           "firewall" SUBDEV_SUFFIX
#define	XOCL_MB			"microblaze" SUBDEV_SUFFIX
#define	XOCL_XIIC		"xiic" SUBDEV_SUFFIX
#define	XOCL_MAILBOX		"mailbox" SUBDEV_SUFFIX
#define	XOCL_ICAP		"icap" SUBDEV_SUFFIX

enum {
        XOCL_SUBDEV_FEATURE_ROM,
        XOCL_SUBDEV_MM_DMA,
        XOCL_SUBDEV_MB_SCHEDULER,
        XOCL_SUBDEV_XVC_PUB,
        XOCL_SUBDEV_XVC_PRI,
        XOCL_SUBDEV_SYSMON,
        XOCL_SUBDEV_AF,
	XOCL_SUBDEV_MB,
	XOCL_SUBDEV_XIIC,
	XOCL_SUBDEV_MAILBOX,
	XOCL_SUBDEV_ICAP,
	XOCL_SUBDEV_STR_DMA,
        XOCL_SUBDEV_NUM
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
			{				\
			.start	= 0x330000,		\
			.end 	= 0x330FFF,		\
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
			{				\
			.start	= 0x330000,		\
			.end 	= 0x330FFF,		\
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

#define	XOCL_RES_ICAP_MGMT				\
	((struct resource []) {				\
		/* HWICAP registers */			\
		{					\
			.start	= 0x020000,		\
			.end	= 0x020119,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		/* GENERAL_STATUS_BASE */		\
		{					\
			.start	= 0x032000,		\
			.end	= 0x032003,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		/* AXI Gate registers */		\
		{					\
			.start	= 0x030000,		\
			.end	= 0x03000b,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		/* OCL_CLKWIZ0_BASE */			\
		{					\
			.start	= 0x050000,		\
			.end	= 0x050fff,		\
			.flags  = IORESOURCE_MEM,	\
		},					\
		/* OCL_CLKWIZ1_BASE */			\
		{					\
			.start	= 0x051000,		\
			.end	= 0x051fff,		\
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

#define	XOCL_DEVINFO_ICAP_USER				\
	{						\
		XOCL_SUBDEV_ICAP,			\
		XOCL_ICAP,				\
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

#define	XOCL_DEVINFO_QDMA				\
	{						\
		XOCL_SUBDEV_MM_DMA,			\
		XOCL_MM_QDMA,				\
		NULL,					\
		0,					\
	}

#define	XOCL_DEVINFO_QDMA_STREAM			\
	{						\
		XOCL_SUBDEV_STR_DMA,			\
		XOCL_STR_QDMA,				\
		NULL,					\
		0,					\
	}

#define	XOCL_DEVINFO_XDMA				\
	{						\
		XOCL_SUBDEV_MM_DMA,			\
		XOCL_MM_XDMA,				\
		NULL,					\
		0,					\
	}

#define XOCL_RES_SCHEDULER				\
		((struct resource []) {			\
		/*
 		 *map entire bar for now because scheduler directly
		 * programs CUs
		 */					\
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
	}

/* user pf defines */
#define	USER_RES_QDMA							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_QDMA,				\
			XOCL_DEVINFO_QDMA_STREAM,			\
			XOCL_DEVINFO_SCHEDULER,				\
			XOCL_DEVINFO_MAILBOX_USER,			\
			XOCL_DEVINFO_ICAP_USER,				\
		})

#define	XOCL_BOARD_USER_QDMA						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_QDMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_QDMA),		\
		.user_bar = 2,						\
		.intr_bar = 1,						\
	}

#define	USER_RES_XDMA_DSA50						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_SCHEDULER,				\
			XOCL_DEVINFO_ICAP_USER,				\
		})

#define	USER_RES_XDMA							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_XDMA,				\
			XOCL_DEVINFO_SCHEDULER,				\
			XOCL_DEVINFO_MAILBOX_USER,			\
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
		})

#define	XOCL_BOARD_USER_XDMA_DSA50					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_XDMA_DSA50,			\
		.subdev_num = ARRAY_SIZE(USER_RES_XDMA_DSA50),		\
		.user_bar = 0,						\
		.intr_bar = 1,						\
	}

#define	XOCL_BOARD_USER_XDMA						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_XDMA,			\
		.subdev_num = ARRAY_SIZE(USER_RES_XDMA),		\
		.user_bar = 0,						\
		.intr_bar = 1,						\
	}

#define	XOCL_BOARD_USER_DSA52						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= USER_RES_DSA52,			\
		.subdev_num = ARRAY_SIZE(USER_RES_DSA52),		\
		.user_bar = 0,						\
		.intr_bar = 1,						\
	}

/* mgmt pf defines */
#define	MGMT_RES_DEFAULT						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
		})

#define	MGMT_RES_DSA50							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_ICAP_MGMT,				\
		})

#define	XOCL_BOARD_MGMT_DEFAULT						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_DEFAULT,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_DEFAULT),		\
		.user_bar = 0,						\
		.intr_bar = 1,						\
	}

#define	XOCL_BOARD_MGMT_DSA50						\
	(struct xocl_board_private){					\
		.flags		= XOCL_DSAFLAG_PCI_RESET_OFF |		\
			XOCL_DSAFLAG_AXILITE_FLUSH |			\
			XOCL_DSAFLAG_MB_SCHE_OFF,			\
		.subdev_info	= MGMT_RES_DSA50,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_DSA50),		\
		.user_bar = 0,						\
		.intr_bar = 1,						\
	}

#define	MGMT_RES_6A8F							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
		})

#define	MGMT_RES_6A8F_DSA50						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_ICAP_MGMT,				\
		})

#define	XOCL_BOARD_MGMT_6A8F						\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6A8F,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6A8F),		\
		.user_bar = 0,						\
		.intr_bar = 1,						\
	}

#define	XOCL_BOARD_MGMT_888F	XOCL_BOARD_MGMT_6A8F

#define	XOCL_BOARD_MGMT_6A8F_DSA50					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6A8F_DSA50,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6A8F_DSA50),		\
		.user_bar = 0,						\
		.intr_bar = 1,						\
	}

#define	XOCL_BOARD_MGMT_QDMA					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6A8F,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6A8F),		\
		.user_bar = 2,						\
		.intr_bar = 1,						\
	}

#define	XOCL_BOARD_MGMT_6B0F		XOCL_BOARD_MGMT_6A8F

#define	MGMT_RES_6A8F_DSA52						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF_DSA52,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
		})

#define	XOCL_BOARD_MGMT_6A8F_DSA52					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6A8F_DSA52,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6A8F_DSA52),		\
		.user_bar = 0,						\
		.intr_bar = 1,						\
	}

#define	MGMT_RES_6E8F_DSA52						\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PRI,				\
			XOCL_DEVINFO_XIIC,				\
			XOCL_DEVINFO_MAILBOX_MGMT,			\
			XOCL_DEVINFO_ICAP_MGMT,				\
		})

#define	XOCL_BOARD_MGMT_6E8F_DSA52					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_6E8F_DSA52,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_6E8F_DSA52),		\
		.user_bar = 0,						\
		.intr_bar = 1,						\
	}

#endif
