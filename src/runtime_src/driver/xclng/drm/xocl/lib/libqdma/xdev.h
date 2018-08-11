/*******************************************************************************
 *
 * Xilinx DMA IP Core Linux Driver
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
#ifndef __XDEV_H__
#define __XDEV_H__

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#include "libqdma_export.h"
#include "qdma_mbox.h"

#define QDMA_CONFIG_BAR			0
#define QDMA_MAX_BAR_LEN_MAPPED		0x4000000 /* 64MB */

/* maximum size of a single DMA transfer descriptor */
#define QDMA_DESC_BLEN_BITS	28
#define QDMA_DESC_BLEN_MAX	((1 << (QDMA_DESC_BLEN_BITS)) - 1)

/* obtain the 32 most significant (high) bits of a 32-bit or 64-bit address */
#define PCI_DMA_H(addr) ((addr >> 16) >> 16)
/* obtain the 32 least significant (low) bits of a 32-bit or 64-bit address */
#define PCI_DMA_L(addr) (addr & 0xffffffffUL)

struct xlnx_dma_dev;

/* XDMA PCIe device specific book-keeping */
#define XDEV_FLAG_OFFLINE	0x1
#define XDEV_FLAG_IRQ		0x2
#define XDEV_NUM_IRQ_MAX	8

typedef irqreturn_t (*f_intr_handler)(int irq_index, int irq, void *dev_id);

struct intr_coal_conf {
	u16 vec_id;
	u16 intr_rng_num_entries;
	dma_addr_t intr_ring_bus;
	struct qdma_intr_ring *intr_ring_base;
	u8 color; /* color value indicates the valid entry in the interrupt ring */
	unsigned int pidx;
	unsigned int cidx;
};

typedef enum intr_type_list {
	INTR_TYPE_ERROR,
	INTR_TYPE_USER,
	INTR_TYPE_DATA,
	INTR_TYPE_MAX
}intr_type_list;

struct intr_vec_map_type {
	intr_type_list intr_type;
	int intr_vec_index;
	f_intr_handler intr_handler;
};

struct xlnx_dma_dev {
	char mod_name[QDMA_DEV_NAME_MAXLEN];
	struct qdma_dev_conf conf;

	struct list_head list_head;

	spinlock_t lock;		/* protects concurrent access */
	spinlock_t hw_prg_lock;
	unsigned int flags;

	/* attributes */
	u8 flr_prsnt:1; 		/* FLR present? */
	u8 st_mode_en:1;		/* Streaming mode enabled? */
	u8 mm_mode_en:1;		/* Memory mapped mode enabled? */

	/* sriov */
	void *vf_info;	
	u8 vf_count;
	u8 func_id;
	u8 func_id_parent;
	u8 mm_channel_max;

	/* PCIe config. bar */
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
	int dvec_start_idx;
	struct intr_vec_map_type intr_vec_map[XDEV_NUM_IRQ_MAX];

	void *dev_priv;
	u8 intr_coal_en;
	struct intr_coal_conf  *intr_coal_list;

	unsigned int dev_ulf_extra[0];	/* for upper layer calling function */
#ifdef ERR_DEBUG
	spinlock_t err_lock;
	u8 err_mon_cancel;
	struct delayed_work err_mon;
#endif
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

#endif /* XDMA_LIB_H */
