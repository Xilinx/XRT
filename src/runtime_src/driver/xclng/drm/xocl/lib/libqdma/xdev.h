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

#ifndef __XDEV_H__
#define __XDEV_H__

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#include "libqdma_export.h"
#include "qdma_mbox.h"

#define XDMA_MAX_BARS			6
#define XDMA_MAX_BAR_LEN_MAPPED		0x4000000 /* 64MB */

/* maximum size of a single DMA transfer descriptor */
#define XDMA_DESC_BLEN_BITS 	28
#define XDMA_DESC_BLEN_MAX	((1 << (XDMA_DESC_BLEN_BITS)) - 1)

/* obtain the 32 most significant (high) bits of a 32-bit or 64-bit address */
#define PCI_DMA_H(addr) ((addr >> 16) >> 16)
/* obtain the 32 least significant (low) bits of a 32-bit or 64-bit address */
#define PCI_DMA_L(addr) (addr & 0xffffffffUL)

struct xlnx_dma_dev;

/* XDMA PCIe device specific book-keeping */
#define XDEV_FLAG_OFFLINE	0x1
#define XDEV_FLAG_IRQ		0x2
#define XDEV_NUM_IRQ_MAX	8
#define XDEV_INTR_COAL_ENABLE 1
#define XDEV_INTR_COAL_RING_SIZE INTR_RING_SZ_4KB /* ring size is 4KB, i.e 512 entries */

struct intr_coal_conf {
	u16 		vec_id;
	u16 		intr_ring_size;
	dma_addr_t      intr_ring_bus;
	struct qdma_intr_ring *intr_ring_base;
	u8 color; /* color value indicates the valid entry in the interrupt ring */
	unsigned int pidx;
	unsigned int cidx;
};

struct xlnx_dma_dev {
	struct qdma_dev_conf conf;

	struct list_head list_head;

	spinlock_t lock;		/* protects concurrent access */
	unsigned int flags;

	u8 func_id;
	u8 func_id_parent;

	u8 reserved[1];
	/* sriov */
	u8 vf_count;
	void *vf_info;	

	/* PCIe BAR management */
	void *__iomem bar[XDMA_MAX_BARS];	/* addresses for mapped BARs */
	void __iomem *regs;

	/* mailbox */
	spinlock_t mbox_lock;
	struct timer_list mbox_timer;
	wait_queue_head_t mbox_wq;
	struct mbox_msg m_req;
	struct mbox_msg m_resp;

	/* MSI-X interrupt allocation */
	int num_vecs;
	struct msix_entry msix[XDEV_NUM_IRQ_MAX];
	struct list_head intr_list[XDEV_NUM_IRQ_MAX];
	int intr_list_cnt[XDEV_NUM_IRQ_MAX];

	void *dev_priv;
	u8 intr_coal_en;
	struct intr_coal_conf  *intr_coal_list;

	unsigned int dev_ulf_extra[0];	/* for upper layer calling function */
};

static inline int xlnx_dma_device_flag_check(struct xlnx_dma_dev *xdev, unsigned int f)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev->lock, flags);
	if (xdev->flags & f) {
		spin_unlock_irqrestore(&xdev->lock, flags);
		return 1;
	}
	spin_unlock_irqrestore(&xdev->lock, flags);
	return 0;
}

static inline int xlnx_dma_device_flag_test_n_set(struct xlnx_dma_dev *xdev,
					 unsigned int f)
{
	unsigned long flags;
	int rv = 0;

	spin_lock_irqsave(&xdev->lock, flags);
	if (xdev->flags & f) {
		spin_unlock_irqrestore(&xdev->lock, flags);
		rv = 1;
	} else
		xdev->flags |= f;
	spin_unlock_irqrestore(&xdev->lock, flags);
	return rv;
}

static inline void xdev_flag_set(struct xlnx_dma_dev *xdev, unsigned int f)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev->lock, flags);
	xdev->flags |= f;
	spin_unlock_irqrestore(&xdev->lock, flags);
}

static inline void xdev_flag_clear(struct xlnx_dma_dev *xdev, unsigned int f)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev->lock, flags);
	xdev->flags &= ~f;
	spin_unlock_irqrestore(&xdev->lock, flags);
}

struct xlnx_dma_dev *xdev_find_by_pdev(struct pci_dev *pdev);
struct xlnx_dma_dev *xdev_find_by_idx(int);
struct xlnx_dma_dev *xdev_list_first(void);
struct xlnx_dma_dev *xdev_list_next(struct xlnx_dma_dev *);
int xdev_list_dump(char *, int);
int xdev_check_hndl(const char *f, struct pci_dev *pdev, unsigned long hndl);


#ifdef __QDMA_VF__
int xdev_sriov_vf_offline(struct xlnx_dma_dev *xdev, u8 func_id);
int xdev_sriov_vf_online(struct xlnx_dma_dev *xdev, u8 func_id);
#elif defined(CONFIG_PCI_IOV)
/* SR-IOV */
void xdev_sriov_disable(struct xlnx_dma_dev *xdev);
int xdev_sriov_enable(struct xlnx_dma_dev *xdev, int num_vfs);
void xdev_sriov_vf_offline(struct xlnx_dma_dev *xdev, u8 func_id);
int xdev_sriov_vf_online(struct xlnx_dma_dev *xdev, u8 func_id);
int xdev_sriov_vf_fmap(struct xlnx_dma_dev *xdev, u8 func_id,
			unsigned short qbase, unsigned short qmax);
#else
#define xdev_sriov_disable(xdev)
#define xdev_sriov_enable(xdev, num_vfs)
#define xdev_sriov_vf_offline(xdev, func_id)
#define xdev_sriov_vf_online(xdev, func_id)
#endif

int sgt_find_offset(struct sg_table *, unsigned int, struct scatterlist **,
		unsigned int *);
void sgt_dump(struct sg_table *);

#endif /* XDMA_LIB_H */
