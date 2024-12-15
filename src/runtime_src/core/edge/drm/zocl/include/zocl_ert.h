/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Min Ma <min.ma@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_ERT_H_
#define _ZOCL_ERT_H_

#include <linux/of.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include "ert.h"

/* DSA and/or Device tree related */
#define ERT_IRQ_NUMBER 2
#define ERT_CQ_IRQ     0
#define ERT_CU_IRQ     1

#define ZOCL_ERT_HW_RES     0
#define ZOCL_ERT_CQ_RES     1

/**
 * Address constants per spec
 */
#define WORD_SIZE                     4          /* 4 bytes */
#define CQ_SIZE                       0x10000    /* 64K */
#define CQ_SLOT_SIZE                  0x1000     /* 4K each cq slot */
#define CQ_BASE_ADDR                  0x190000
#define CSR_ADDR                      0x180000

#define CQ_SLOT_NUM	(CQ_SIZE / CQ_SLOT_SIZE)      /* defult cq slot num = 16 */

/**
 * The STATUS REGISTER is for communicating completed CQ slot indices
 * MicroBlaze write, host reads.  MB(W) / HOST(COR)
 */
#define ERT_STATUS_REG          0x0
#define ERT_STATUS_REG0         0x0
#define ERT_STATUS_REG1         0x4
#define ERT_STATUS_REG2         0x8
#define ERT_STATUS_REG3         0xC

/**
 * The CU DMA REGISTER is for communicating which CQ slot is to be started
 * on a specific CU.  MB selects a free CU on which the command can
 * run, then writes the 1<<CU back to the command slot CU mask and
 * writes the slot index to the CU DMA REGISTER.  HW is notified when
 * the register is written and now does the DMA transfer of CU regmap
 * map from command to CU, while MB continues its work. MB(W) / HW(R)
 */
#define ERT_CU_DMA_ENABLE       0x18
#define ERT_CU_DMA_REG          0x1C
#define ERT_CU_DMA_REG0         0x1C
#define ERT_CU_DMA_REG1         0x20
#define ERT_CU_DMA_REG2         0x24
#define ERT_CU_DMA_REG3         0x28

/**
 * The SLOT SIZE is the size of slots in command queue, it is
 * configurable per xclbin. MB(W) / HW(R)
 */
#define ERT_CQ_SLOT_SIZE_REG    0x2C

/**
 * The CU_OFFSET is the size of a CU's address map in power of 2.  For
 * example a 64K regmap is 2^16 so 16 is written to the CU_OFFSET_ADDR.
 * MB(W) / HW(R)
 */
#define ERT_CU_OFFSET_REG       0x30

/**
 * The number of slots is command_queue_size / slot_size.
 * MB(W) / HW(R)
 */
#define ERT_CQ_NUM_OF_SLOTS_REG 0x34

/**
 * All CUs placed in same address space separated by CU_OFFSET. The
 * CU_BASE_ADDRESS is the address of the first CU. MB(W) / HW(R)
 */
#define ERT_CU_BASE_ADDR_REG    0x38

/**
 * The CQ_BASE_ADDRESS is the base address of the command queue.
 * MB(W) / HW(R)
 */
#define ERT_CQ_BASE_ADDR_REG    0x3C

/**
 * The CU_ISR_HANDLER_ENABLE (MB(W)/HW(R)) enables the HW handling of
 * CU interrupts.  When a CU interrupts (when done), hardware handles
 * the interrupt and writes the index of the CU that completed into
 * the CU_STATUS_REGISTER (HW(W)/MB(COR)) as a bitmask
 */
#define ERT_CU_ISR_ENABLE       0x40
#define ERT_CU_STATUS_REG       0x44
#define ERT_CU_STATUS_REG0      0x44
#define ERT_CU_STATUS_REG1      0x48
#define ERT_CU_STATUS_REG2      0x4C
#define ERT_CU_STATUS_REG3      0x50

/**
 * The CQ_STATUS_ENABLE (MB(W)/HW(R)) enables interrupts from HOST to
 * MB to indicate the presense of a new command in some slot.  The
 * slot index is written to the CQ_STATUS_REGISTER (HOST(W)/MB(R))
 */
#define ERT_CQ_STATUS_ENABLE    0x54
#define ERT_CQ_STATUS_REG       0x58
#define ERT_CQ_STATUS_REG0      0x58
#define ERT_CQ_STATUS_REG1      0x5C
#define ERT_CQ_STATUS_REG2      0x60
#define ERT_CQ_STATUS_REG3      0x64

/**
 * The NUMBER_OF_CU (MB(W)/HW(R) is the number of CUs per current
 * xclbin.  This is an optimization that allows HW to only check CU
 * completion on actual CUs.
 */
#define ERT_NUM_OF_CU_REG       0x68

/**
 * Enable global interrupts from MB to HOST on command completion.
 * When enabled writing to STATUS_REGISTER causes an interrupt in HOST.
 * MB(W)
 */
#define ERT_HOST_INT_ENABLE     0x100

/* ERT drive name */
#define ZOCL_ERT_NAME "zocl_ert"

extern struct platform_driver zocl_ert_driver;
struct zocl_ert_ops;

struct zocl_ert_dev {
	struct platform_device       *pdev;
	void __iomem                 *hw_ioremap;
	void __iomem                 *cq_ioremap;
	unsigned int                  irq[ERT_IRQ_NUMBER];
	const struct zocl_ert_ops    *ops;
};

struct zocl_ert_ops {
	/**
	 * @init:
	 *
	 * Initial ERT dedicated FPGA module
	 */
	void (*init)(struct zocl_ert_dev *ert);
	/**
	 * @fini:
	 *
	 * Finish ERT dedicated FPGA module
	 */
	void (*fini)(struct zocl_ert_dev *ert);
	/**
	 * @config:
	 *
	 * Configure command could configure ERT dedicated module
	 */
	void (*config)(struct zocl_ert_dev *ert,
		       struct ert_configure_cmd *cfg);
	/**
	 * @get_next_cmd:
	 *
	 * Return next cmd. If no command found, return NULL.
	 */
	struct ert_packet *
	(*get_next_cmd)(struct zocl_ert_dev *ert,
			struct ert_packet *pkg, int *idx_ret);
	/**
	 * @notify_host:
	 *
	 * Notify host the status of a command is changed.
	 */
	void (*notify_host)(struct zocl_ert_dev *ert, int slot_idx);

	/**
	 * @updata_cmd:
	 *
	 * Update the data section of a CQ slot
	 */
	void (*update_cmd)(struct zocl_ert_dev *ert, int idx,
			    void *data, int sz);
};

struct zocl_ert_info {
	int			model;
	struct zocl_ert_ops    *ops;
};
struct ert_packet *get_packet(struct ert_packet *packet, u32 idx, u32 size);
#endif
