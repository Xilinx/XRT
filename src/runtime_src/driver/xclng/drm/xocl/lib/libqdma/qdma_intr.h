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
 * Sujatha Banoth <sbanoth@xilinx.com>
 *
 ******************************************************************************/

#ifndef LIBQDMA_QDMA_INTR_H_
#define LIBQDMA_QDMA_INTR_H_

#include <linux/types.h>
#include <linux/workqueue.h>

struct xlnx_dma_dev;

/*
 * Interrupt ring data
 */
struct qdma_intr_ring {
	__be64 pidx:16;
	__be64 cidx:16;
	__be64 s_color:1;
	__be64 intr_satus:2;
	__be64 error:4;
	__be64 rsvd:11;
	__be64 error_int:1;
	__be64 intr_type:1;
	__be64 qid:11;
	__be64 coal_color:1;
};

void intr_teardown(struct xlnx_dma_dev *xdev);
int intr_setup(struct xlnx_dma_dev *xdev);
void intr_ring_teardown(struct xlnx_dma_dev *xdev);
int intr_context_setup(struct xlnx_dma_dev *xdev);
int intr_ring_setup(struct xlnx_dma_dev *xdev);
void intr_work(struct work_struct *work);

void qdma_err_intr_setup(struct xlnx_dma_dev *xdev);
void qdma_enable_hw_err(struct xlnx_dma_dev *xdev, u8 hw_err_type);
int get_intr_ring_index(struct xlnx_dma_dev *xdev, u32 vector_index);

#ifdef ERR_DEBUG
void err_stat_handler(struct xlnx_dma_dev *xdev);
#endif

#endif /* LIBQDMA_QDMA_DEVICE_H_ */

