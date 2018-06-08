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

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s: " fmt, __func__

#include "qdma_intr.h"

#include <linux/kernel.h>
#include "qdma_descq.h"
#include "qdma_device.h"
#include "qdma_regs.h"
#include "thread.h"
#include "version.h"


static inline void intr_ring_free(struct xlnx_dma_dev *xdev, int ring_sz,
			int intr_desc_sz, u8 *intr_desc, dma_addr_t desc_bus)
{
	unsigned int len = ring_sz * intr_desc_sz;

	pr_debug("free %u(0x%x)=%d*%u, 0x%p, bus 0x%llx.\n",
		len, len, intr_desc_sz, ring_sz, intr_desc, desc_bus);

	dma_free_coherent(&xdev->conf.pdev->dev, ring_sz * intr_desc_sz,
			intr_desc, desc_bus);
}

static void *intr_ring_alloc(struct xlnx_dma_dev *xdev, int ring_sz,
		    int intr_desc_sz, dma_addr_t *bus)
{
	unsigned int len = ring_sz * intr_desc_sz ;
	u8 *p = dma_alloc_coherent(&xdev->conf.pdev->dev, len, bus, GFP_KERNEL);

	if (!p) {
		pr_info("%s, OOM, sz ring %d, intr_desc %d.\n",
			xdev->conf.name, ring_sz, intr_desc_sz);
		return NULL;
	}

	memset(p, 0, len);

	pr_debug("alloc %u(0x%x)=%d*%u, bus 0x%llx .\n",
		len, len, intr_desc_sz, ring_sz, *bus);

	return p;
}

void intr_ring_teardown(struct xlnx_dma_dev *xdev)
{
	int i = xdev->num_vecs;
	struct intr_coal_conf  *ring_entry;

	while (--i >= 0) {
		ring_entry = (xdev->intr_coal_list + i);
		intr_ring_free(xdev, ring_entry->intr_ring_size,
						sizeof(struct qdma_intr_ring),
						(u8 *)ring_entry->intr_ring_base,
						ring_entry->intr_ring_bus);
	}

	kfree(xdev->intr_coal_list);
	pr_info("dev %s interrupt coalescing ring teardown successful\n",
				dev_name(&xdev->conf.pdev->dev));
}

static irqreturn_t irq_top(int irq, void *dev_id)
{
	struct xlnx_dma_dev *xdev = dev_id;
	struct qdma_descq *descq = NULL;
	unsigned long flags;
	int i;

	for (i = 0; i < xdev->num_vecs; i++) {
		if (xdev->msix[i].vector == irq) {
			pr_info("IRQ fired: vector=%d, entry=%d\n", irq, i);
			break;
		}
	}

	if (i == xdev->num_vecs) {
		pr_err("Unrecognized IRQ fired: vector=%d\n", irq);
		return IRQ_NONE;
	}

	spin_lock_irqsave(&xdev->lock, flags);

	if (xdev->intr_coal_en) {
		struct intr_coal_conf *coal_entry = (xdev->intr_coal_list + i);
		struct qdma_intr_ring *ring_entry;

		pr_info("IRQ fired: msix[%d].vector=%d, vec_id=%d\n",
			i, xdev->msix[i].vector, coal_entry->vec_id);
		if(xdev->msix[i].vector ==  coal_entry->vec_id) {
			int counter = 0;

			pr_info("IRQ[%d] fired: intr vec_entry[%d] pidx = %d\n",
				irq, i, coal_entry->pidx);

			for(counter = coal_entry->pidx; counter <
				(coal_entry->intr_ring_size - coal_entry->pidx);
				counter++) {

				ring_entry = (coal_entry->intr_ring_base + counter);
				pr_info("IRQ[%d] fired: expected_color = %d, current_color = %d\n",
					irq, coal_entry->color,
					ring_entry->coal_color);
				pr_info("IRQ[%d] fired: Interrupt Vector_entry[%d] Qid = %d, coal_color = %d\n",
					irq, i, ring_entry->qid,
					ring_entry->coal_color);
				if (ring_entry->coal_color != coal_entry->color)
					break;
				else {
					pr_info("IRQ[%d] fired: Interrupt Vector_entry[%d] Qid = %d, coal_color = %d\n",
						irq, i, ring_entry->qid,
						ring_entry->coal_color);

					descq = qdma_device_get_descq_by_id(
								xdev,
								ring_entry->qid,
								NULL, 0, 0);
					if (!descq)
						return -EINVAL;

					schedule_work(&descq->work);
				}
			}
			coal_entry->pidx = counter;
		}

	} else {
		list_for_each_entry(descq, &xdev->intr_list[i], intr_list)
			schedule_work(&descq->work);
	}
	spin_unlock_irqrestore(&xdev->lock, flags);

	return IRQ_HANDLED;
}

void intr_teardown(struct xlnx_dma_dev *xdev)
{
	int i = xdev->num_vecs;

	while (--i >= 0)
		free_irq(xdev->msix[i].vector, xdev);

	if (xdev->num_vecs)
		pci_disable_msix(xdev->conf.pdev);
}

int intr_setup(struct xlnx_dma_dev *xdev)
{
	int rv = 0;
	int i;

	if (xdev->conf.poll_mode) {
		pr_info("Polled mode configured, skipping interrupt setup\n");
		return 0;
	}

	xdev->num_vecs = pci_msix_vec_count(xdev->conf.pdev);

	if (!xdev->num_vecs) {
		pr_info("MSI-X not supported, running in polled mode\n");
		return 0;
	}

	if (xdev->num_vecs > XDEV_NUM_IRQ_MAX)
		xdev->num_vecs = XDEV_NUM_IRQ_MAX;

	for (i = 0; i < xdev->num_vecs; i++) {
		xdev->msix[i].entry = i;
		INIT_LIST_HEAD(&xdev->intr_list[i]);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,12,0)
	rv = pci_enable_msix_exact(xdev->conf.pdev, xdev->msix, xdev->num_vecs);
#else
	rv = pci_enable_msix(xdev->conf.pdev, xdev->msix, xdev->num_vecs);
#endif
	if (rv < 0) {
		pr_err("Error enabling MSI-X (%d)\n", rv);
		goto exit;
	}

	for (i = 0; i < xdev->num_vecs; i++) {
		pr_info("Requesting IRQ vector %d\n", xdev->msix[i].vector);
		rv = request_irq(xdev->msix[i].vector, irq_top, 0,
					LIBQDMA_MODULE_NAME, xdev);
		if (rv) {
			pr_err("request_irq for vector %d fail\n", i);
			goto cleanup_irq;
		}
	}

	xdev->flags |= XDEV_FLAG_IRQ;
	return rv;

cleanup_irq:
	while (--i >= 0)
		free_irq(xdev->msix[i].vector, xdev);

	pci_disable_msix(xdev->conf.pdev);

exit:
	return rv;
}

int intr_ring_setup(struct xlnx_dma_dev *xdev, int ring_size)
{

	int num_entries = 0;
	int counter = 0;
	struct intr_coal_conf  *intr_coal_list;
	struct intr_coal_conf  *intr_coal_list_entry;

	if (xdev->conf.poll_mode || !xdev->conf.indirect_intr_mode) {
		pr_info("skipping interrupt aggregation setup, as poll_mode = %d or indirect_intr_mode= %d\n",
				xdev->conf.poll_mode, xdev->conf.indirect_intr_mode);
		xdev->intr_coal_en = 0;
		xdev->intr_coal_list = NULL;
		return 0;
	}

	if((xdev->num_vecs != 0) && (xdev->num_vecs < xdev->conf.qsets_max)) {
		pr_info("dev %s num_vectors[%d] < num_queues [%d], Enabling Interrupt aggregation\n",
					dev_name(&xdev->conf.pdev->dev), xdev->num_vecs, xdev->conf.qsets_max);
		xdev->intr_coal_en = 1;
		/* obtain the number of queue entries in each inr_ring based on ring size */
		switch(ring_size) {
		case INTR_RING_SZ_4KB:
			num_entries = 512;
			break;
		case INTR_RING_SZ_8KB:
			num_entries = 1024;
			break;
		case INTR_RING_SZ_12KB:
			num_entries = 1536;
			break;
		case INTR_RING_SZ_16KB:
			num_entries = 2048;
			break;
		case INTR_RING_SZ_20KB:
			num_entries = 2560;
			break;
		case INTR_RING_SZ_24KB:
			num_entries = 3072;
			break;
		case INTR_RING_SZ_28KB:
			num_entries = 3584;
			break;
		case INTR_RING_SZ_32KB:
			num_entries = 4096;
			break;
		default:
			num_entries = 512;
		}

		pr_info("%s interrupt coalescing ring with %d entries \n",
			dev_name(&xdev->conf.pdev->dev), num_entries);
		/*
		 * Initially assuming that each vector has the same size of the
		 * ring, In practical it is possible to have different ring
		 * size of different vectors (?)
	 	 */
		intr_coal_list = kzalloc(
				sizeof(struct intr_coal_conf) * xdev->num_vecs,
				GFP_KERNEL);
		if (!intr_coal_list) {
			pr_info("dev %s num_vecs %d OOM.\n",
				dev_name(&xdev->conf.pdev->dev), xdev->num_vecs);
			return -ENOMEM;
		}

		for(counter = 0; counter < xdev->num_vecs; counter++) {
			intr_coal_list_entry = (intr_coal_list + counter);
			intr_coal_list_entry->intr_ring_size = num_entries;
			intr_coal_list_entry->intr_ring_base = intr_ring_alloc(
					xdev, num_entries,
					sizeof(struct qdma_intr_ring),
					&intr_coal_list_entry->intr_ring_bus);
			if (!intr_coal_list_entry->intr_ring_base) {
				pr_info("dev %s, sz %u, intr_desc ring OOM.\n",
					xdev->conf.name,
					intr_coal_list_entry->intr_ring_size);
				goto err_out;
			}

			intr_coal_list_entry->vec_id = xdev->msix[counter].vector;
			intr_coal_list_entry->pidx = 0;
			intr_coal_list_entry->cidx = 0;
			intr_coal_list_entry->color = 1;
		}

		pr_info("dev %s interrupt coalescing ring setup successful\n",
					dev_name(&xdev->conf.pdev->dev));

		xdev->intr_coal_list = intr_coal_list;
	} else 	{
		pr_info("dev %s intr vec[%d] >= queues[%d], No aggregation\n",
			dev_name(&xdev->conf.pdev->dev), xdev->num_vecs,
			xdev->conf.qsets_max);
		xdev->intr_coal_en = 0;
		xdev->intr_coal_list = NULL;
	}
	return 0;

err_out:
	while(--counter >= 0) {
		intr_coal_list_entry = (intr_coal_list + counter);
		intr_ring_free(xdev, intr_coal_list_entry->intr_ring_size,
				sizeof(struct qdma_intr_ring),
				(u8 *)intr_coal_list_entry->intr_ring_base,
				intr_coal_list_entry->intr_ring_bus);
	}
	kfree(intr_coal_list);
	return -ENOMEM;
}

void intr_work(struct work_struct *work)
{
	struct qdma_descq *descq;

	descq = container_of(work, struct qdma_descq, work);
	qdma_descq_service_wb(descq);
}
