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

#ifndef LIBQDMA_QDMA_THREAD_H_
#define LIBQDMA_QDMA_THREAD_H_
/**
 * @file
 * @brief This file contains the declarations for qdma thread handlers
 *
 */

/** qdma_descq forward declaration */
struct qdma_descq;

/*****************************************************************************/
/**
 * qdma_threads_create() - create qdma threads
 * This functions creates two threads for each cpu in the system
 * and assigns the thread handlers
 * 1: queue processing thread
 * 2: queue completion handler thread
 *
 * @return	0: success
 * @return	<0: failure
 *****************************************************************************/
int qdma_threads_create(unsigned int num_threads);

/*****************************************************************************/
/**
 * qdma_threads_destroy() - destroy all the qdma threads created
 *                          during system initialization
 *
 * @return	none
 *****************************************************************************/
void qdma_threads_destroy(void);

/*****************************************************************************/
/**
 * qdma_thread_remove_work() - handler to remove the attached work thread
 *
 * @param[in]	descq:	pointer to qdma_descq
 *
 * @return	none
 *****************************************************************************/
void qdma_thread_remove_work(struct qdma_descq *descq);

/*****************************************************************************/
/**
 * qdma_thread_add_work() - handler to add a work thread
 *
 * @param[in]	descq:	pointer to qdma_descq
 *
 * @return	none
 *****************************************************************************/
void qdma_thread_add_work(struct qdma_descq *descq);

#endif /* LIBQDMA_QDMA_THREAD_H_ */
