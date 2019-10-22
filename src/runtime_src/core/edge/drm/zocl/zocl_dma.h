/*
 * A GEM style (optionally CMA backed) device manager for ZynQ based
 * OpenCL accelerators.
 *
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
 *
 * Authors:
 *    Sonal Santan <sonal.santan@xilinx.com>
 *    Umang Parekh <umang.parekh@xilinx.com>
 *    Jan Stephan  <j.stephan@hzdr.de>
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

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>

/**
 * DOC: Embedded driver handler interface for DMA
 *
 * Internal interfaces designed for interacting with Embedded Linux DMA Engine.
 */
#ifndef _ZOCL_DMA_H_
#define _ZOCL_DMA_H_

#define ZOCL_DMA_DONE	(1 << 0)
#define ZOCL_DMA_ERROR	(1 << 1)

typedef void (*zocl_dma_complete_cb)(void *arg, int ret);

/**
 * struct zocl_dma_handle - DMA handler for zocl driver
 *
 * @dma_flags: indicate if DMA returns error
 * @dma_chan: DMA Channel, acquired before use DMA
 * @dma_cookie: DMA Engine cookie
 * @dma_done: DMA completion opaque
 * @dma_func: call back function when dma complete
 * @dma_arg: private data for dma_func call back function
 *
 * Prior to pass this handler to ZOCL DMA Engine, user can set dma_func and
 * dma_arg for handling subsequent asynchronous operations. When DMA is
 * completed the dma_func will be called with dma_arg. DMA error is returned
 * via zocl_dma_complete_cb argument ret.
 */
typedef struct zocl_dma_handle {
	int		 	dma_flags;
	struct dma_chan 	*dma_chan;
	dma_cookie_t		dma_cookie;
	struct completion 	dma_done;
	zocl_dma_complete_cb 	dma_func;
	void 			*dma_arg;
} zocl_dma_handle_t;

/**
 * zocl_dma_memcpy_pre() - DMA memory copy preparation.
 *
 * @dma_handle: zocl dma handler
 * @dst_paddr: destination dma address
 * @src_paddr: source dma address
 * @size: number of bytes to read from source
 */
int zocl_dma_memcpy_pre(zocl_dma_handle_t *dma_handle,
    dma_addr_t dst_paddr, dma_addr_t src_paddr, size_t size);

/**
 * zocl_dma_start() - Start DMA Engine.
 *
 * @dma_handle: zocl dma handler
 *
 * Common API for starting async DMA Engine. Current ZOCL DMA Engine only
 * support memcpy, it can be enhanced for other type of DMAs when necessary.
 */
void zocl_dma_start(zocl_dma_handle_t *dma_handle);

#endif
