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

#ifndef LIBQDMA_QDMA_INTR_H_
#define LIBQDMA_QDMA_INTR_H_

#include <linux/types.h>
#include <linux/workqueue.h>

struct xlnx_dma_dev;

/*
 * Interrupt ring data
 */
struct qdma_intr_ring {
	__be64 status_desc:37;
	__be64 rsvd:13;
	__be64 err_int:1;
	__be64 intr_type:1;
	__be64 qid:11;
	__be64 coal_color:1;
};

void intr_teardown(struct xlnx_dma_dev *xdev);
int intr_setup(struct xlnx_dma_dev *xdev);
void intr_ring_teardown(struct xlnx_dma_dev *xdev);
int intr_context_setup(struct xlnx_dma_dev *xdev);
int intr_ring_setup(struct xlnx_dma_dev *xdev, int ring_size);
void intr_work(struct work_struct *work);

#endif /* LIBQDMA_QDMA_DEVICE_H_ */

