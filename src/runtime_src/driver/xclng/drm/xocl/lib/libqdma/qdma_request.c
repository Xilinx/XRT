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

#define pr_fmt(fmt)	KBUILD_MODNAME ":%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include "libqdma_export.h"
#include "qdma_device.h"
#include "qdma_descq.h"
#include "qdma_request.h"
#include "qdma_compat.h"

/*
 * qdma_req_copy_fl()
 * C2H: copy data received in freelist buffers to the qdma request buffers
 * return # of freelist entries that are used completedly
 */
int qdma_req_copy_fl(struct qdma_sw_sg *fsgl, unsigned int fsgcnt,
		struct qdma_request *req, unsigned int *copied_p)
{
	struct qdma_sw_sg *fsg = fsgl;
	bool use_sgt = req->use_sgt;
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);
	struct qdma_sw_sg *tsg = (struct qdma_sw_sg *)cb->sg;
	struct scatterlist *tsg_t = (struct scatterlist *)cb->sg;
	unsigned int foff = fsg->offset;
	unsigned int tsgcnt = use_sgt ? req->sgt->orig_nents : req->sgcnt;
        unsigned int tsgoff = cb->sg_offset;
	unsigned int flen = 0;
	unsigned int copied = 0;
	int i = 0;
	int j = cb->sg_idx;


	while ((i < fsgcnt) && (j < tsgcnt)) {
		unsigned char *faddr = page_address(fsg->pg) + fsg->offset;

		flen = fsg->len;
		foff = 0;

		pr_debug("fsg 0x%p, %d/%u,%u,%u, tsg 0x%p, %d/%u,%u,%u.\n",
			fsg, i, fsgcnt, fsg->offset, fsg->len,
			use_sgt ? (void *)tsg_t : (void *)tsg,
			j, tsgcnt, tsgoff,
			use_sgt ? tsg_t->length : tsg->len);

		while (flen && (j < tsgcnt)) {
			unsigned int copy;

			if (use_sgt) {
				copy = min_t(unsigned int, flen,
						 tsg_t->length - tsgoff);

				pr_debug("copy %u to sgt %d, 0x%p, len %u, off %u.\n",
					copy, j, tsg_t, tsg_t->length, tsgoff);

				memcpy(page_address(sg_page(tsg_t)) + tsgoff +
						tsg_t->offset, faddr, copy);
				tsgoff += copy;
				if (tsgoff == tsg_t->length) {
					tsg_t = sg_next(tsg_t);
					tsgoff = 0;
					j++;
				}
			} else {
				copy = min_t(unsigned int, flen,
						tsg->len - tsgoff);

				pr_debug("copy %u to sgl %d, 0x%p, len %u, off %u.\n",
					copy, j, tsg, tsg->len, tsgoff);

				memcpy(page_address(tsg->pg) + tsgoff +
						tsg->offset, faddr, copy);
				tsgoff += copy;
				if (tsgoff == tsg->len) {
					tsg = tsg->next;
					tsgoff = 0;
					j++;
				}
			}

			faddr += copy;
			flen -= copy;
			foff += copy;
			copied += copy;
		}

		if (foff == fsg->len) {
			i++;
			foff = 0;
			fsg = fsg->next;
		}
	}

	if (foff) {
		fsg->offset += foff;
		fsg->len -= foff;
	}

	cb->sg_idx = j;
	cb->sg_offset = tsgoff;
	cb->sg = use_sgt ? (void *)tsg_t : (void *)tsg;
	cb->left -= copied;
	cb->offset += copied;

	*copied_p = copied;

	return i;
}

static void sgl_dump(struct qdma_sw_sg *sgl, unsigned int sgcnt)
{
	struct qdma_sw_sg *sg = sgl;
	int i;

	pr_info("sgl 0x%p, sgcnt %u.\n", sgl, sgcnt);

	for (i = 0; i < sgcnt; i++, sg++)
		pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx.\n",
			i, sg, sg->pg, sg->offset, sg->len, sg->dma_addr);
}

static void sgt_dump(struct sg_table *sgt)
{
	int i;
	struct scatterlist *sg = sgt->sgl;

	pr_info("sgt 0x%p, sgl 0x%p, nents %u/%u.\n",
		sgt, sgt->sgl, sgt->nents, sgt->orig_nents);

	for_each_sg(sgt->sgl, sg, sgt->orig_nents, i) {
		if (i < sgt->nents)
			pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx,%u.\n",
				i, sg, sg_page(sg), sg->offset, sg->length,
				sg_dma_address(sg), sg_dma_len(sg));
		else
			pr_info("%d, 0x%p, pg 0x%p,%u+%u.\n",
				i, sg, sg_page(sg), sg->offset, sg->length);
	}
}

int qdma_req_find_offset(struct qdma_request *req, bool use_dma_addr)
{
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);
	struct sg_table *sgt = req->sgt;
	unsigned int off = req->offset;
	int i = 0;

	if (req->use_sgt) {
		int sgcnt = use_dma_addr ? sgt->nents : sgt->orig_nents;
		struct scatterlist *sg;

		for_each_sg(sgt->sgl, sg, sgcnt, i) {
			unsigned int len = use_dma_addr ?
						sg_dma_len(sg) : sg->length;

			if (off < len) {
				cb->sg_offset = off;
				cb->sg = (void *)sg;
				cb->sg_idx = i;

				return 0;
			}
			off -= len;
		}
		
		pr_info("bad offset %u.\n", req->offset);
		sgt_dump(sgt);
		return -EINVAL;
	} else {
		struct qdma_sw_sg *sg = req->sgl;
		unsigned int sgcnt = req->sgcnt;

		for (; i < sgcnt; i++, sg = sg->next) {
			if (off < sg->len) {
				cb->sg_offset = off;
				cb->sg = (void *)sg;
				cb->sg_idx = i;

				return 0;
			}
			off -= sg->len;
		}

		pr_info("bad offset %u.\n", req->offset);
		sgl_dump(req->sgl, req->sgcnt);
		return -EINVAL;
	}
}

void qdma_request_dump(const char *str, struct qdma_request *req, bool dump_cb)
{
	pr_info("%s, req 0x%p %u,%u, ep 0x%llx, tm %u ms, %s,%s,%s,%s,%s,async %d.\n",
                str, req, req->offset, req->count, req->ep_addr,
                req->timeout_ms,
                req->write ? "W":"R", req->dma_mapped ? "M":"",
                req->eot ? "EOT":"", req->use_sgt ? "SGT":"SGL",
                req->eot_rcved ? "EOT RCV":"", req->fp_done ? 1 : 0);

	if (req->use_sgt)
		sgt_dump(req->sgt);
	else
		sgl_dump(req->sgl, req->sgcnt);

	if (dump_cb) {
		struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);

		pr_info("req 0x%p, desc %u, %u,%u, sg %d,%u,0x%p.\n",
			req, cb->desc_nr, cb->offset, cb->left, cb->sg_idx,
			cb->sg_offset, cb->sg);
	}
}

/*****************************************************************************/
/**
 * qdma_request_unmap() - unmap the request's data buffers
 *
 * @param[in]   pdev:   pointer to struct pci_dev
 * @param[in]   req:	request
 *
 * @return      none
 *****************************************************************************/
void qdma_request_unmap(struct pci_dev *pdev, struct qdma_request *req)
{
	enum dma_data_direction dir = req->write ? DMA_TO_DEVICE :
						DMA_FROM_DEVICE;

	if (req->use_sgt) {
		pci_unmap_sg(pdev, req->sgt->sgl, req->sgt->orig_nents, dir);
	} else {
		struct qdma_sw_sg *sg = req->sgl;
		unsigned int sgcnt = req->sgcnt;
		int i = 0;

		for (i = 0; i < sgcnt; i++, sg++) {
			if (!sg->pg)
				break;
			if (sg->dma_addr) {
				pci_unmap_page(pdev, sg->dma_addr - sg->offset,
						PAGE_SIZE, dir);
				sg->dma_addr = 0UL;
			}
		}
	}
}

/*****************************************************************************/
/**
 * qdma_request_map() - map the request's data buffers to bus address
 *
 * @param[in]   pdev:   pointer to struct pci_dev
 * @param[in]   req:	request
 *
 * @return      none
 *****************************************************************************/

int qdma_request_map(struct pci_dev *pdev, struct qdma_request *req)
{
	enum dma_data_direction dir = req->write ? DMA_TO_DEVICE :
						DMA_FROM_DEVICE;

	if (req->use_sgt) {
		int nents = pci_map_sg(pdev, req->sgt->sgl,
					req->sgt->orig_nents, dir);

		if (!nents) {
			pr_info("map sgt failed, sgt %u,%u.\n",
				req->sgt->orig_nents, req->count);
			return -EIO;
		}
		req->sgt->nents = nents;
	} else {
		struct qdma_sw_sg *sg = req->sgl;
		unsigned int sgcnt = req->sgcnt;
		int i = 0;

		for (i = 0; i < sgcnt; i++, sg++) {
			/* !! TODO  page size !! */
			sg->dma_addr = pci_map_page(pdev, sg->pg, 0,
						PAGE_SIZE, dir);
			if (unlikely(pci_dma_mapping_error(pdev,
							sg->dma_addr))) {
				pr_info("map sgl failed, sg %d, %u.\n",
					i, sg->len);
				if (i)
					qdma_request_unmap(pdev, req);
				return -EIO;
			}
			sg->dma_addr += sg->offset;
		}
	}

	return 0;
}

void qdma_request_cancel_done(struct qdma_descq *descq,
				struct qdma_request *req)
{
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);

	/* caller should hold the descq lock */
	list_del(&cb->list);

	pr_info("%s, %s, req 0x%p cancelled.\n",
		descq->xdev->conf.name, descq->conf.name, req);

	cb->canceled = 1;
	cb->status = -ECANCELED;
	cb->done = 1;

	if (cb->unmap_needed) {
	       qdma_request_unmap(descq->xdev->conf.pdev, req);
	       cb->unmap_needed = 0;
	}

	if (req->fp_done)
		req->fp_done(req->uld_data, cb->offset, -ECANCELED);
	else
		qdma_waitq_wakeup(&cb->wq);
}

int qdma_request_cancel(unsigned long dev_hndl, unsigned long qhndl,
			struct qdma_request *req)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;
	struct qdma_descq *descq =
		qdma_device_get_descq_by_id(xdev, qhndl, NULL, 0, 1);
	struct qdma_sgt_req_cb *cb = qdma_req_cb_get(req);
	
	pr_info("%s, %s, cancel req 0x%p.\n",
		xdev->conf.name, descq->conf.name, req);

        qdma_request_dump(descq->conf.name, req, 1);

	lock_descq(descq);
	cb->cancel = 1;
	if (!cb->offset || (descq->conf.st && descq->conf.c2h)) {
		qdma_request_cancel_done(descq, req);
	}
	unlock_descq(descq);

	schedule_work(&descq->work);

	return 0;
}
