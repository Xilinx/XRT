/*******************************************************************************
 *
 * Xilinx QDMA IP Core Linux Driver
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
#ifndef LIBQDMA_QDMA_DEVICE_H_
#define LIBQDMA_QDMA_DEVICE_H_

#include <linux/spinlock_types.h>

struct qdma_descq;
struct xlnx_dma_dev;

struct qdma_dev {
	u8 init_qrange:1;
	u8 filler[3];

	unsigned short qmax;
	unsigned short qbase;

	spinlock_t lock;
	unsigned short h2c_qcnt;
	unsigned short c2h_qcnt;

	struct qdma_descq *h2c_descq;
	struct qdma_descq *c2h_descq;
};

#define xdev_2_qdev(xdev)	(struct qdma_dev *)((xdev)->dev_priv)

int qdma_device_init(struct xlnx_dma_dev *);
void qdma_device_cleanup(struct xlnx_dma_dev *);
struct qdma_descq* qdma_device_get_descq_by_id(struct xlnx_dma_dev *xdev,
			unsigned long idx, char *buf, int buflen, int init);
struct qdma_descq* qdma_device_get_descq_by_hw_qid(struct xlnx_dma_dev *xdev,
			unsigned long qidx_hw, u8 c2h);

int qdma_device_prep_q_resource(struct xlnx_dma_dev *xdev);

#ifndef __QDMA_VF__
void qdma_csr_read_wbacc(struct xlnx_dma_dev *xdev, unsigned int *wb_acc);
void qdma_csr_read_rngsz(struct xlnx_dma_dev *xdev, unsigned int *rngsz);
void qdma_csr_read_bufsz(struct xlnx_dma_dev *xdev, unsigned int *bufsz);
void qdma_csr_read_timer_cnt(struct xlnx_dma_dev *xdev, unsigned int *cnt);
void qdma_csr_read_cnt_thresh(struct xlnx_dma_dev *xdev, unsigned int *th);
#else
#ifdef ERR_DEBUG
void qdma_csr_read_wbacc(struct xlnx_dma_dev *xdev, unsigned int *wb_acc);
#endif
#endif

#endif /* LIBQDMA_QDMA_DEVICE_H_ */
