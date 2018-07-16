/*******************************************************************************
 *
 * Xilinx XDMA IP Core Linux Driver
 * Copyright(c) 2015 - 2017 Xilinx, Inc.
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
#ifndef LIBQDMA_QDMA_THREAD_H_
#define LIBQDMA_QDMA_THREAD_H_

struct qdma_descq;

int qdma_threads_create(void);
void qdma_threads_destroy(void);
void qdma_thread_remove_work(struct qdma_descq *descq);
void qdma_thread_add_work(struct qdma_descq *descq);

#endif /* LIBQDMA_QDMA_THREAD_H_ */
