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
/**
 * @file
 * @brief This file contains the declarations for qdma dev interrupt handlers
 *
 */
#include <linux/types.h>
#include <linux/workqueue.h>
/**
 * forward declaration for xlnx_dma_dev
 */
struct xlnx_dma_dev;

/**
 * @struct - qdma_intr_ring
 * @brief	Interrupt ring entry definition
 */
struct qdma_intr_ring {
	/** producer index. This is from Interrupt source.
	 *  Cumulative pointer of total interrupt Aggregation
	 *  Ring entry written
	 */
	__be64 pidx:16;
	/** consumer index. This is from Interrupt source.
	 *  Cumulative consumed pointer
	 */
	__be64 cidx:16;
	/** source color. This is from Interrupt source.
	 *  This bit inverts every time pidx wraps around
	 *  and this field gets copied to color field of descriptor.
	 */
	__be64 s_color:1;
	/** This is from Interrupt source.
	 *  Interrupt state, 0: WRB_INT_ISR; 1: WRB_INT_TRIG; 2: WRB_INT_ARMED
	 */
	__be64 intr_satus:2;
	/** error. This is from interrupt source
	 *  {C2h_err[1:0], h2c_err[1:0]}
	 */
	__be64 error:4;
	/**  11 reserved bits*/
	__be64 rsvd:11;
	/**  Is the interrupt raised due to error ?
	 *   1: error interrupt; 0: non-error interrupt
	 */
	__be64 error_int:1;
	/**  interrupt type, 0: H2C; 1: C2H*/
	__be64 intr_type:1;
	/**  This is from Interrupt source. Queue ID*/
	__be64 qid:11;
	/**  The color bit of the Interrupt Aggregation Ring.
	 *   This bit inverts every time pidx wraps around on the
	 *   Interrupt Aggregation Ring.
	 */
	__be64 coal_color:1;
};

/*****************************************************************************/
/**
 * intr_teardown() - un register the interrupts for the device
 *
 * @param[in]	xdev:		pointer to xdev
 *
 * @return	none
 *****************************************************************************/
void intr_teardown(struct xlnx_dma_dev *xdev);

/*****************************************************************************/
/**
 * intr_setup() - register the interrupts for the device
 *
 * @param[in]	xdev:		pointer to xdev
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int intr_setup(struct xlnx_dma_dev *xdev);

/*****************************************************************************/
/**
 * intr_ring_teardown() - delete the interrupt ring
 *
 * @param[in]	xdev:		pointer to xdev
 *
 * @return	none
 *****************************************************************************/
void intr_ring_teardown(struct xlnx_dma_dev *xdev);

/*****************************************************************************/
/**
 * intr_context_setup() - set up the interrupt context
 *
 * @param[in]	xdev:		pointer to xdev
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int intr_context_setup(struct xlnx_dma_dev *xdev);

/*****************************************************************************/
/**
 * intr_ring_setup() - create the interrupt ring
 *
 * @param[in]	xdev:		pointer to xdev
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int intr_ring_setup(struct xlnx_dma_dev *xdev);

/*****************************************************************************/
/**
 * intr_work() - attach the top half for the interrupt
 *
 * @param[in]	work:		pointer to struct work_struct
 *
 * @return	none
 *****************************************************************************/
void intr_work(struct work_struct *work);
void delayed_intr_work(struct work_struct *work);

/*****************************************************************************/
/**
 * qdma_err_intr_setup() - set up the error interrupt
 *
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	rearm:		flag to control the error interrupt arming
 *
 * @return	none
 *****************************************************************************/
void qdma_err_intr_setup(struct xlnx_dma_dev *xdev, u8 rearm);

/*****************************************************************************/
/**
 * qdma_enable_hw_err() - enable the hw errors
 *
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	hw_err_type:	hw error type
 *
 * @return	none
 *****************************************************************************/
void qdma_enable_hw_err(struct xlnx_dma_dev *xdev, u8 hw_err_type);

/*****************************************************************************/
/**
 * qdma_enable_hw_err() - get the interrupt ring index based on vector index
 *
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	vector_index:	vector index
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int get_intr_ring_index(struct xlnx_dma_dev *xdev, u32 vector_index);

#ifdef ERR_DEBUG
/*****************************************************************************/
/**
 * err_stat_handler() - error interrupt handler
 *
 * @param[in]	xdev:		pointer to xdev
 *
 * @return	none
 *****************************************************************************/
void err_stat_handler(struct xlnx_dma_dev *xdev);
#endif

#endif /* LIBQDMA_QDMA_DEVICE_H_ */

