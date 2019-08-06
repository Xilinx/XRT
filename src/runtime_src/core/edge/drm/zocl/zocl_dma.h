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

#define ZOCL_DMA_DONE (1 << 0)

typedef void (*zocl_dma_complete_cb)(void *arg);

typedef struct zocl_dma_handle {
	int		 	dma_flags;
	struct dma_chan 	*dma_chan;
	dma_cookie_t		dma_cookie;
	struct completion 	dma_done;
	zocl_dma_complete_cb 	dma_func;
	void 			*dma_arg;
} zocl_dma_handle_t;

int zocl_dma_memcpy_pre(zocl_dma_handle_t *dma_handle,
    dma_addr_t dst_paddr, dma_addr_t src_paddr, size_t size);

void zocl_dma_start(zocl_dma_handle_t *dma_handle);
