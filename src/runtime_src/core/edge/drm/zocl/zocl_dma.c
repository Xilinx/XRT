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

#include "zocl_dma.h"
#include <drm/drmP.h>

static void zocl_dma_irq_done(void *data)
{
	zocl_dma_handle_t *dma_handle = data;
	enum dma_status status;

	status = dmaengine_tx_status(dma_handle->dma_chan,
	    dma_handle->dma_cookie, NULL);
	if (status == DMA_ERROR || status == DMA_COMPLETE) {
		complete(&dma_handle->dma_done);
		//registered callback function from user
		if (dma_handle->dma_func)
			dma_handle->dma_func(dma_handle->dma_arg);
	}
}

int zocl_dma_memcpy_pre(zocl_dma_handle_t *dma_handle,
    dma_addr_t dst_paddr, dma_addr_t src_paddr, size_t size)
{
	struct dma_async_tx_descriptor 	*dma_tx;
	int 				rc = 0;

	/* prep dma memcpy engine */
	dma_tx = dmaengine_prep_dma_memcpy(dma_handle->dma_chan, dst_paddr,
	    src_paddr, size, DMA_CTRL_ACK);
	if (!dma_tx) {
		DRM_ERROR("Failed to prepare DMA memcpy\n");
		rc = -EINVAL;
		goto out;
	}

	dma_tx->callback = zocl_dma_irq_done;
	dma_tx->callback_param = dma_handle;

	/* submit dma engine */
	dma_handle->dma_cookie = dmaengine_submit(dma_tx);
	if (dma_submit_error(dma_handle->dma_cookie)) {
		DRM_ERROR("Failed to submit dma\n");
		rc = -EINVAL;
		goto out;
	}
out:
	return rc;
}

void zocl_dma_start(zocl_dma_handle_t *dma_handle)
{
	init_completion(&dma_handle->dma_done);
	dma_async_issue_pending(dma_handle->dma_chan);
}
