/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */

#ifndef __LIBQDMA_CONTEXT_H__
#define __LIBQDMA_CONTEXT_H__

#include "xdev.h"
#include "qdma_mbox.h"

int qdma_intr_context_setup(struct xlnx_dma_dev *xdev);

int qdma_descq_context_setup(struct qdma_descq *descq);
int qdma_descq_context_clear(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h);
int qdma_descq_context_read(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h,
				struct hw_descq_context *ctxt);
#ifndef __QDMA_VF__
int qdma_descq_context_program(struct xlnx_dma_dev *xdev, unsigned int qid_hw,
				bool st, bool c2h,
				struct hw_descq_context *ctxt);
#endif

#endif /* ifndef __LIBQDMA_CONTEXT_H__ */
