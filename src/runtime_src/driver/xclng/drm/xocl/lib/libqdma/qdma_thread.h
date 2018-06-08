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

struct qdma_descq;

int qdma_threads_create(void);
void qdma_threads_destroy(void);
void qdma_thread_remove_work(struct qdma_descq *descq);
void qdma_thread_add_work(struct qdma_descq *descq);

#endif /* LIBQDMA_QDMA_THREAD_H_ */
