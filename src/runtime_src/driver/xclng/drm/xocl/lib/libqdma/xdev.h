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
/**
 * @file
 * @brief This file contains the declarations for QDMA PCIe device
 *
 */
#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#include "libqdma_export.h"
#include "qdma_mbox.h"

/**
 * QDMA config bar size - 64MB
 */
#define QDMA_MAX_BAR_LEN_MAPPED		0x4000000

/**
 * number of bits to describe the DMA transfer descriptor
 */
#define QDMA_DESC_BLEN_BITS	28
/**
 * maximum size of a single DMA transfer descriptor
 */
#define QDMA_DESC_BLEN_MAX	((1 << (QDMA_DESC_BLEN_BITS)) - 1)

/**
 * obtain the 32 most significant (high) bits of a 32-bit or 64-bit address
 */
#define PCI_DMA_H(addr) ((addr >> 16) >> 16)
/**
 * obtain the 32 least significant (low) bits of a 32-bit or 64-bit address
 */
#define PCI_DMA_L(addr) (addr & 0xffffffffUL)

/**
 * Xiling DMA device forward declaration
 */
struct xlnx_dma_dev;

/* XDMA PCIe device specific book-keeping */
/**
 * Flag for device offline
 */
#define XDEV_FLAG_OFFLINE	0x1
/**
 * Flag for IRQ
 */
#define XDEV_FLAG_IRQ		0x2
/**
 * Maximum number of interrupts supported per device
 */
#define XDEV_NUM_IRQ_MAX	8

/**
 * interrupt call back function handlers
 */
typedef irqreturn_t (*f_intr_handler)(int irq_index, int irq, void *dev_id);

/**
 * @struct - intr_coal_conf
 * @brief	interrut coalescing configuration
 */
struct intr_coal_conf {
	u16 vec_id;
	/**< interrupt vector index */
	u16 intr_rng_num_entries;
	/**< number of entries in interrupt ring per vector */
	dma_addr_t intr_ring_bus;
	/**< interrupt ring dma base address */
	struct qdma_intr_ring *intr_ring_base;
	/**< interrupt ring base address */
	u8 color;
	/**< color value indicates the valid entry in the interrupt ring */
	unsigned int cidx;
	/**< interrupt ring consumer index */
};

/**
 * intr_type_list - interrupt types
 */
enum intr_type_list {
	INTR_TYPE_ERROR,	/**< error interrupt */
	INTR_TYPE_USER,		/**< user interrupt */
	INTR_TYPE_DATA,		/**< data interrupt */
	INTR_TYPE_MAX		/**< max interrupt */
};

/**
 * @struct - intr_vec_map_type
 * @brief	interrupt vector map details
 */
struct intr_vec_map_type {
	enum intr_type_list intr_type;	/**< interrupt type */
	int intr_vec_index;		/**< interrupt vector index */
	f_intr_handler intr_handler;	/**< interrupt handler */
};

/**
 * @struct - xlnx_dma_dev
 * @brief	Xilinx DMA device details
 */
struct xlnx_dma_dev {
	char mod_name[QDMA_DEV_NAME_MAXLEN];
	/**< Xilinx DMA device name */
	struct qdma_dev_conf conf;
	/**< DMA device configuration */
	struct list_head list_head;
	/**< DMA device list */
	spinlock_t lock;
	/**< DMA device lock to protects concurrent access */
	spinlock_t hw_prg_lock;
	/**< DMA device hardware program lock */
	unsigned int flags;
	/**< device flags */
	u8 flr_prsnt:1;
	/**< flag to indicate the FLR present status */
	u8 st_mode_en:1;
	/**< flag to indicate the streaming mode enabled status */
	u8 mm_mode_en:1;
	/**< flag to indicate the memory mapped mode enabled status */
	u8 stm_en:1;
	/**< flag to indicate the presence of STM */
	void *vf_info;
	/**< sriov info */
	u8 vf_count;
	/**< number of virtual functions */
	u8 func_id;
	/**< function id */
#ifdef __QDMA_VF__
	u8 func_id_parent;
	/**< parent function id, valid only for virtual function */
#else
	u8 pf_count;
	/**< number of physical functions */
#endif
	u8 mm_channel_max;
	/**< max mm channels */
	u8 stm_rev;
	void __iomem *regs;
	/**< PCIe config. bar */
	void __iomem *stm_regs;
	/** PCIe Bar for STM config */

	int num_vecs;
	/**< number of MSI-X interrupt vectors per device */
	struct msix_entry msix[XDEV_NUM_IRQ_MAX];
	/**< msix_entry list for all vectors */
	struct list_head intr_list[XDEV_NUM_IRQ_MAX];
	/**< queue list for each interrupt */
	int intr_list_cnt[XDEV_NUM_IRQ_MAX];
	/**< number of queues assigned for each interrupt */
	int dvec_start_idx;
	/**< data vector start index */
	struct intr_vec_map_type intr_vec_map[XDEV_NUM_IRQ_MAX];
	/**< interrupt vector map */
	void *dev_priv;
	/**< DMA private device to hold the qdma que details */
	u8 intr_coal_en;
	/**< flag to indicate the interrupt aggregation enable status */
	struct intr_coal_conf  *intr_coal_list;
	/**< list of interrupt coalescing configuration for each vector */
	unsigned int dev_ulf_extra[0];
	/**< for upper layer calling function */
#ifdef ERR_DEBUG
	spinlock_t err_lock;
	/**< error lock */
	u8 err_mon_cancel;
	/**< flag to indicate the error minitor status */
	struct delayed_work err_mon;
	/**< error minitor work handler */
#endif

	struct qdma_mbox mbox;
};

/*****************************************************************************/
/**
 * xlnx_dma_device_flag_check() - helper function to check the flag status
 *
 * @param[in]	xdev:	pointer to xilinx dma device
 * @param[in]	f:	flag value
 *
 *
 * @return	1 if the flag is on
 * @return	0 if the flag is off
 *****************************************************************************/
static inline int xlnx_dma_device_flag_check(struct xlnx_dma_dev *xdev,
					unsigned int f)
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

/*****************************************************************************/
/**
 * xlnx_dma_device_flag_test_n_set() - helper function to test n set the flag
 *
 * @param[in]	xdev:	pointer to xilinx dma device
 * @param[in]	f:	flag value
 *
 *
 * @return	1 if the flag is already enabled
 * @return	0 if the flag is off
 *****************************************************************************/
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

/*****************************************************************************/
/**
 * xdev_flag_set() - helper function to set the device flag
 *
 * @param[in]	xdev:	pointer to xilinx dma device
 * @param[in]	f:	flag value
 *
 *
 * @return	none
 *****************************************************************************/
static inline void xdev_flag_set(struct xlnx_dma_dev *xdev, unsigned int f)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev->lock, flags);
	xdev->flags |= f;
	spin_unlock_irqrestore(&xdev->lock, flags);
}

/*****************************************************************************/
/**
 * xlnx_dma_device_flag_test_n_set() - helper function to clear the device flag
 *
 * @param[in]	xdev:	pointer to xilinx dma device
 * @param[in]	f:	flag value
 *
 * @return	none
 *****************************************************************************/
static inline void xdev_flag_clear(struct xlnx_dma_dev *xdev, unsigned int f)
{
	unsigned long flags;

	spin_lock_irqsave(&xdev->lock, flags);
	xdev->flags &= ~f;
	spin_unlock_irqrestore(&xdev->lock, flags);
}

/*****************************************************************************/
/**
 * xdev_find_by_pdev() - find the xdev using struct pci_dev
 *
 * @param[in]	pdev:	pointer to struct pci_dev
 *
 * @return	pointer to xlnx_dma_dev on success
 * @return	NULL on failure
 *****************************************************************************/
struct xlnx_dma_dev *xdev_find_by_pdev(struct pci_dev *pdev);

/*****************************************************************************/
/**
 * xdev_find_by_idx() - find the xdev using the index value
 *
 * @param[in]	idx:	index value in the xdev list
 *
 * @return	pointer to xlnx_dma_dev on success
 * @return	NULL on failure
 *****************************************************************************/
struct xlnx_dma_dev *xdev_find_by_idx(int idx);

/*****************************************************************************/
/**
 * xdev_list_first() - handler to return the first xdev entry from the list
 *
 * @return	pointer to first xlnx_dma_dev on success
 * @return	NULL on failure
 *****************************************************************************/
struct xlnx_dma_dev *xdev_list_first(void);

/*****************************************************************************/
/**
 * xdev_list_next() - handler to return the next xdev entry from the list
 *
 * @param[in]	xdev:	pointer to current xdev
 *
 * @return	pointer to next xlnx_dma_dev on success
 * @return	NULL on failure
 *****************************************************************************/
struct xlnx_dma_dev *xdev_list_next(struct xlnx_dma_dev *xdev);

/*****************************************************************************/
/**
 * xdev_list_dump() - list the dma device details
 *
 * @param[in]	buflen:		length of the input buffer
 * @param[out]	buf:		message buffer
 *
 * @return	pointer to next xlnx_dma_dev on success
 * @return	NULL on failure
 *****************************************************************************/
int xdev_list_dump(char *buf, int buflen);

/*****************************************************************************/
/**
 * xdev_check_hndl() - helper function to validate the device handle
 *
 * @param[in]	f:		device name
 * @param[in]	pdev:	pointer to struct pci_dev
 * @param[in]	hndl:	device handle
 *
 * @return	0: success
 * @return	EINVAL: on failure
 *****************************************************************************/
int xdev_check_hndl(const char *f, struct pci_dev *pdev, unsigned long hndl);


#ifdef __QDMA_VF__
/*****************************************************************************/
/**
 * xdev_sriov_vf_offline() - API to set the virtual function to offline mode
 *
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	func_id:	function identifier
 *
 * @return	0: success
 * @return	-1: on failure
 *****************************************************************************/
int xdev_sriov_vf_offline(struct xlnx_dma_dev *xdev, u8 func_id);

/*****************************************************************************/
/**
 * xdev_sriov_vf_online() - API to set the virtual function to online mode
 *
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	func_id:	function identifier
 *
 * @return	0: success
 * @return	-1: on failure
 *****************************************************************************/
int xdev_sriov_vf_online(struct xlnx_dma_dev *xdev, u8 func_id);
#elif defined(CONFIG_PCI_IOV)
/* SR-IOV */
/*****************************************************************************/
/**
 * xdev_sriov_vf_online() - API to disable the virtual function
 *
 * @param[in]	xdev:		pointer to xdev
 *
 * @return	none
 *****************************************************************************/
void xdev_sriov_disable(struct xlnx_dma_dev *xdev);

/*****************************************************************************/
/**
 * xdev_sriov_vf_online() - API to enable the virtual function
 *
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	func_id:	function identifier
 *
 * @return	number of vfs enabled on success
 * @return	-1: on failure
 *****************************************************************************/
int xdev_sriov_enable(struct xlnx_dma_dev *xdev, int num_vfs);

/*****************************************************************************/
/**
 * xdev_sriov_vf_offline() - API to set the virtual function to offline mode
 *
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	func_id:	function identifier
 *
 * @return	none
 *****************************************************************************/
void xdev_sriov_vf_offline(struct xlnx_dma_dev *xdev, u8 func_id);

/*****************************************************************************/
/**
 * xdev_sriov_vf_offline() - API to set the virtual function to offline mode
 *
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	func_id:	function identifier
 *
 * @return	0: success
 * @return	-1: on failure
 *****************************************************************************/
int xdev_sriov_vf_online(struct xlnx_dma_dev *xdev, u8 func_id);

/*****************************************************************************/
/**
 * xdev_sriov_vf_offline() - API to configure the fmap for virtual function
 *
 * @param[in]	xdev:		pointer to xdev
 * @param[in]	func_id:	function identifier
 * @param[in]	qbase:		queue start
 * @param[in]	qmax:		queue max
 *
 * @return	0: success
 * @return	-1: on failure
 *****************************************************************************/
int xdev_sriov_vf_fmap(struct xlnx_dma_dev *xdev, u8 func_id,
			unsigned short qbase, unsigned short qmax);
#else
/** dummy declaration for xdev_sriov_disable()
 *  When virtual function is not enabled
 */
#define xdev_sriov_disable(xdev)
/** dummy declaration for xdev_sriov_enable()
 *  When virtual function is not enabled
 */
#define xdev_sriov_enable(xdev, num_vfs)
/** dummy declaration for xdev_sriov_vf_offline()
 *  When virtual function is not enabled
 */
#define xdev_sriov_vf_offline(xdev, func_id)
/** dummy declaration for xdev_sriov_vf_online()
 *  When virtual function is not enabled
 */
#define xdev_sriov_vf_online(xdev, func_id)
#endif

#endif /* XDMA_LIB_H */
