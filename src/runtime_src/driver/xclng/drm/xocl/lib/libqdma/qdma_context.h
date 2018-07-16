/*******************************************************************************
 *
 * Xilinx DMA IP Core Linux Driver
 * Copyright(c) 2017 Xilinx, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "LICENSE".
 *
 * Karen Xie <karen.xie@xilinx.com>
 *
 ******************************************************************************/
#ifndef __LIBQDMA_CONTEXT_H__
#define __LIBQDMA_CONTEXT_H__

#include "xdev.h"
#include "qdma_mbox.h"

int qdma_intr_context_setup(struct xlnx_dma_dev *xdev);

int qdma_descq_context_setup(struct qdma_descq *descq);
int qdma_descq_context_clear(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h, bool clr);
int qdma_descq_context_read(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h,
				struct hw_descq_context *ctxt);
int qdma_intr_context_read(struct xlnx_dma_dev *xdev, int ring_index, u32 *context);

#ifndef __QDMA_VF__
int qdma_descq_context_program(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h,
				struct hw_descq_context *ctxt);
#endif

#endif /* ifndef __LIBQDMA_CONTEXT_H__ */
