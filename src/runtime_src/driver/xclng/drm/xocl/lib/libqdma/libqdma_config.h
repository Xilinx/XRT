/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#ifndef __LIBQDMA_CONFIG_H__
#define __LIBQDMA_CONFIG_H__
/**
 * @file
 * @brief This file contains the declarations for qdma configuration apis
 *
 */
/*****************************************************************************
 * GLOBAL CONSTANTS
 *****************************************************************************/
#include <linux/types.h>

/**
 * Maximum number of physical functions
 */
#define QDMA_PF_MAX		4	/* 4 PFs */
/**
 * Maximum number of virtual functions
 */
#define QDMA_VF_MAX		252

/**
 * Maximum number of queues per physical function
 */
#define QDMA_Q_PER_PF_MAX	512

/**
 * Maximum number of QDMA devices in the system
 */
#define MAX_DMA_DEV 32

/**
 * Total number of qdma qs
 */
#define TOTAL_QDMA_QS	(QDMA_PF_MAX * QDMA_Q_PER_PF_MAX)

/**
 * Maximum number of queues per virtual function
 */
#define QDMA_Q_PER_VF_MAX	1

/**
 * Total number of qs for all VF
 */
#define TOTAL_VF_QS	0

/**
 * Total number of qs for all PFs
 */
#define TOTAL_PF_QS (TOTAL_QDMA_QS - TOTAL_VF_QS)

/**
 * Maximum number of qs for PF
 */
#define MAX_QS_PER_PF   (TOTAL_PF_QS/QDMA_PF_MAX)

/**
 * Shift for bus 'B' in B:D:F
 */
#define PCI_SHIFT_BUS 12

/**
 * Shift for device 'D' in B:D:F
 */
#define PCI_SHIFT_DEV 4

/**
 * To shift the Bus number for getting BDF
 */
#define SHIFT_DEC_PCI_BUS	1000

/**
 * To shift the device number for getting BDF
 */
#define SHIFT_DEC_PCI_DEV	10

/**
 * Maximum number of MSI-X vector per function
 */
#define QDMA_DEV_MSIX_VEC_MAX	8

/**
 * ring size is 4KB, i.e 512 entries
 */
#define QDMA_INTR_COAL_RING_SIZE INTR_RING_SZ_4KB

/** Maximum data vectors to be used for each function
 * TODO: Please note that for 2018.2 only one vector would be used
 * per pf and only one ring would be created for this vector
 * It is also assumed that all functions have the same number of data vectors
 * and currently different number of vectors per PF is not supported
 */
#define QDMA_NUM_DATA_VEC_FOR_INTR_CXT  1

/*****************************************************************************
 * Function Declaration
 *****************************************************************************/

/*****************************************************************************/
/**
 * qdma_set_qmax() -  Handler function to set the qmax configuration value
 *
 * @param[in]	dev_hndl:	qdma device handle
 * @param[in]	qsets_max:	qmax configuration value
 *
 *
 * @return	QDMA_OPERATION_SUCCESSFUL on success
 * @return	-1 on failure
 *****************************************************************************/
int qdma_set_qmax(unsigned long dev_hndl, u32 qsets_max, bool forced);

/*****************************************************************************/
/**
 * qdma_get_qmax() -  Handler function to get the qmax configuration value
 *
 * @param[in]	dev_hndl:	qdma device handle
 *
 * @return	qmax value on success
 * @return	< 0 on failure
 *****************************************************************************/
unsigned int qdma_get_qmax(unsigned long dev_hndl);

/*****************************************************************************/
/**
 * qdma_set_intr_rngsz() - Handler function to set the intr_ring_size value
 *
 * @param[in]	dev_hndl:	qdma device handle
 * @param[in]	rngsz:		interrupt aggregation ring size
 *
 * @return	QDMA_OPERATION_SUCCESSFUL on success
 * @return	-1 on failure
 *****************************************************************************/
int qdma_set_intr_rngsz(unsigned long dev_hndl, u32 rngsz);

/*****************************************************************************/
/**
 * qdma_get_intr_rngsz() - Handler function to get the intr_ring_size value
 *
 * @param[in]	dev_hndl:	qdma device handle
 *
 *
 * @return	interrupt ring size on success
 * @return	-1 on failure
 *****************************************************************************/
unsigned int qdma_get_intr_rngsz(unsigned long dev_hndl);

#ifndef __QDMA_VF__
/*****************************************************************************/
/**
 * qdma_set_cmpl_status_acc() -  Handler function to set the cmpl_status_acc
 * configuration value
 *
 * @param[in]	dev_hndl:	qdma device handle
 * @param[in]	cmpl_status_acc:	Writeback Accumulation value
 *
 * @return	QDMA_OPERATION_SUCCESSFUL on success
 * @return	-1 on failure
 *****************************************************************************/
int qdma_set_cmpl_status_acc(unsigned long dev_hndl, u32 cmpl_status_acc);

/*****************************************************************************/
/**
 * qdma_get_cmpl_status_acc() -  Handler function to get the cmpl_status_acc
 * configuration value
 *
 * @param[in] dev_hndl:		qdma device handle
 *
 * Handler function to get the writeback accumulation value
 *
 * @return	cmpl_status_acc on success
 * @return	-1 on failure
 *****************************************************************************/
unsigned int qdma_get_cmpl_status_acc(unsigned long dev_hndl);

/*****************************************************************************/
/**
 * qdma_set_buf_sz() - Handler function to set the buf_sz value
 *
 * @param[in]	dev_hndl:	qdma device handle
 * @param[in]	buf_sz:		buf sizes
 *
 * @return	QDMA_OPERATION_SUCCESSFUL on success
 * @return	-1 on failure
 *****************************************************************************/
int qdma_set_buf_sz(unsigned long dev_hndl, u32 *buf_sz);

/*****************************************************************************/
/**
 * qdma_get_buf_sz() - Handler function to get the buf_sz value
 *
 * @param[in]	dev_hndl:	qdma device handle
 * @param[in]	buf_sz:		buf sizes
 *
 * @return	buf sizes on success
 * @return	-1 on failure
 *****************************************************************************/
unsigned int qdma_get_buf_sz(unsigned long dev_hndl, u32 *buf_sz);

/*****************************************************************************/
/**
 * qdma_set_glbl_rng_sz() - Handler function to set the glbl_rng_sz value
 *
 * @param[in]	dev_hndl:			qdma device handle
 * @param[in]	glbl_rng_sz:		glbl_rng_sizes
 *
 * @return	QDMA_OPERATION_SUCCESSFUL on success
 * @return	-1 on failure
 *****************************************************************************/
int qdma_set_glbl_rng_sz(unsigned long dev_hndl, u32 *glbl_rng_sz);

/*****************************************************************************/
/**
 * qdma_get_glbl_rng_sz() - Handler function to get the glbl_rng_sz value
 *
 * @param[in]	dev_hndl:			qdma device handle
 * @param[in]	glbl_rng_sz:		glbl_rng sizes
 *
 * @return	glbl_rng_sz on success
 * @return	-1 on failure
 *****************************************************************************/
unsigned int qdma_get_glbl_rng_sz(unsigned long dev_hndl, u32 *glbl_rng_sz);

/*****************************************************************************/
/**
 * qdma_set_timer_cnt() - Handler function to set the buf_sz value
 *
 * @param[in]	dev_hndl:		qdma device handle
 * @param[in]	tmr_cnt:		Array of 16 timer count values
 *
 * @return	QDMA_OPERATION_SUCCESSFUL on success
 * @return	-1 on failure
 *****************************************************************************/
int qdma_set_timer_cnt(unsigned long dev_hndl, u32 *tmr_cnt);

/*****************************************************************************/
/**
 * qdma_get_timer_cnt() - Handler function to get the timer_cnt value
 *
 * @param[in]	dev_hndl:	qdma device handle
 *
 *
 * @return	timer_cnt  on success
 * @return	-1 on failure
 *****************************************************************************/
unsigned int qdma_get_timer_cnt(unsigned long dev_hndl, u32 *tmr_cnt);

/*****************************************************************************/
/**
 * qdma_set_cnt_thresh() - Handler function to set the counter threshold value
 *
 * @param[in]	dev_hndl:		qdma device handle
 * @param[in]	cnt_th:		Array of 16 timer count values
 *
 * @return	QDMA_OPERATION_SUCCESSFUL on success
 * @return	-1 on failure
 *****************************************************************************/
int qdma_set_cnt_thresh(unsigned long dev_hndl, unsigned int *cnt_th);

/*****************************************************************************/
/**
 * qdma_get_cnt_thresh() - Handler function to get the counter thresh value
 *
 * @param[in]	dev_hndl:	qdma device handle
 *
 *
 * @return	counter threshold values  on success
 * @return	-1 on failure
 *****************************************************************************/
unsigned int qdma_get_cnt_thresh(unsigned long dev_hndl, u32 *cnt_th);
#endif

#endif
