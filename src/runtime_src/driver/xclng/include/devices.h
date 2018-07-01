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

/* board flags */
enum {
        XOCL_DSAFLAG_PCI_RESET_OFF =            0x01,
        XOCL_DSAFLAG_MB_SCHE_OFF =              0x02,
        XOCL_DSAFLAG_AXILITE_FLUSH =            0x04,
        XOCL_DSAFLAG_SET_DSA_VER =              0x08,
        XOCL_DSAFLAG_SET_XPR =                  0x10,
};

struct xocl_subdev_info {
        uint32_t		id;
        char			*name;
        struct resource		*res;
        int			num_res;
};

struct xocl_board_private {
        uint64_t		flags;
        struct xocl_subdev_info	*subdev_info;
        uint32_t		subdev_num;
        uint32_t		user_bar;
        uint32_t		intr_bar;
        uint32_t		dsa_ver;
        bool                    xpr;
};

#ifdef __KERNEL__
#define XOCL_PCI_DEVID(ven, dev, subsysid, priv)        \
         .vendor = ven, .device=dev, .subvendor = PCI_ANY_ID, \
         .subdevice = subsysid, .driver_data =          \
         (kernel_ulong_t) &XOCL_BOARD_##priv
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

#define USER_RES_AWS                            \
       ((struct xocl_subdev_info []) {             \
           XOCL_DEVINFO_FEATURE_ROM,           \
           XOCL_DEVINFO_XDMA,              \
           XOCL_DEVINFO_SCHEDULER,             \
           XOCL_DEVINFO_ICAP_USER,             \
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

#define XOCL_BOARD_USER_AWS                         \
   (struct xocl_board_private){                    \
       .flags      = 0,                    \
       .subdev_info    = USER_RES_AWS,         \
       .subdev_num = ARRAY_SIZE(USER_RES_AWS),     \
       .user_bar = 0,                      \
       .intr_bar = 1,                      \
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

#define	MGMT_RES_QDMA							\
		((struct xocl_subdev_info []) {				\
			XOCL_DEVINFO_FEATURE_ROM,			\
			XOCL_DEVINFO_SYSMON,				\
			XOCL_DEVINFO_AF,				\
			XOCL_DEVINFO_MB,				\
			XOCL_DEVINFO_XVC_PUB,				\
			XOCL_DEVINFO_ICAP_MGMT,				\
		})


#define	XOCL_BOARD_MGMT_QDMA					\
	(struct xocl_board_private){					\
		.flags		= 0,					\
		.subdev_info	= MGMT_RES_QDMA,			\
		.subdev_num = ARRAY_SIZE(MGMT_RES_QDMA),		\
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

#define	XOCL_MGMT_PCI_IDS			\
	{ XOCL_PCI_DEVID(0x10EE, 0x4A47, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4A87, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B47, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B87, 0x4350, MGMT_DSA50) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B87, 0x4351, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x684F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0xA883, 0x1351, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x688F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x694F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x698F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A4F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4350, MGMT_6A8F_DSA50) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4351, MGMT_6A8F) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A8F, 0x4352, MGMT_6A8F_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A9F, 0x4360, MGMT_QDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A9F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6E4F, PCI_ANY_ID, MGMT_DEFAULT) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6B0F, PCI_ANY_ID, MGMT_6B0F) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6E8F, 0x4352, MGMT_6E8F_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x888F, PCI_ANY_ID, MGMT_888F) },   \
	{ XOCL_PCI_DEVID(0x13FE, 0x006C, PCI_ANY_ID, MGMT_DEFAULT) }

#define	XOCL_USER_XDMA_PCI_IDS			\
	{ XOCL_PCI_DEVID(0x10EE, 0x4A48, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4A88, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B48, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B88, 0x4350, USER_XDMA_DSA50) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x4B88, 0x4351, USER_XDMA) },		\
	{ XOCL_PCI_DEVID(0x10EE, 0x6850, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6890, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6950, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0xA884, 0x1351, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6990, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A50, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A90, 0x4350, USER_XDMA_DSA50) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A90, 0x4351, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6A90, 0x4352, USER_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6AA0, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6E50, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6B10, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x6E90, 0x4352, USER_DSA52) },	\
	{ XOCL_PCI_DEVID(0x10EE, 0x8890, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x13FE, 0x0065, PCI_ANY_ID, USER_XDMA) },	\
	{ XOCL_PCI_DEVID(0x1D0F, 0x1042, PCI_ANY_ID, USER_AWS) },	\
	{ XOCL_PCI_DEVID(0x1D0F, 0xF000, PCI_ANY_ID, USER_AWS) },   \
	{ XOCL_PCI_DEVID(0x1D0F, 0xF040, PCI_ANY_ID, USER_AWS) }
 
#define	XOCL_USER_QDMA_PCI_IDS			\
	{ XOCL_PCI_DEVID(0x10EE, 0x6AA0, 0x4360, USER_QDMA) }


#endif
