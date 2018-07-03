/*
 * Copyright (C) 2016-2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Lizhi.Hou@xilinx.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include "../xocl_drv.h"
#include "common.h"

static const struct pci_device_id pciidlist[] = {
	XOCL_USER_XDMA_PCI_IDS,
	{ 0, }
};

struct class *xrt_class = NULL;

MODULE_DEVICE_TABLE(pci, pciidlist);

#define	EBUF_LEN	256
void xocl_reset_notify(struct pci_dev *pdev, bool prepare)
{
        struct xocl_dev *xdev = pci_get_drvdata(pdev);

	if (!pdev->driver ||
		strcmp(pdev->driver->driver.mod_name, XOCL_MODULE_NAME)) {
		xocl_err(&pdev->dev, "XOCL driver is not bound");
		return;
	}
        xocl_info(&pdev->dev, "PCI reset NOTIFY, prepare %d", prepare);

        if (prepare) {
		xocl_mailbox_reset(xdev, false);
		xocl_user_dev_offline(xdev);
        } else {
		reset_notify_client_ctx(xdev);
		xocl_user_dev_online(xdev);
		xocl_mailbox_reset(xdev, true);
        }
}
EXPORT_SYMBOL_GPL(xocl_reset_notify);

/* hack for mgmt driver to request scheduler reset */
int xocl_reset_scheduler(struct pci_dev *pdev)
{
	return xocl_exec_reset(pci_get_drvdata(pdev));
}
EXPORT_SYMBOL_GPL(xocl_reset_scheduler);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
void user_pci_reset_prepare(struct pci_dev *pdev)
{
        xocl_reset_notify(pdev, true);
}

void user_pci_reset_done(struct pci_dev *pdev)
{
        xocl_reset_notify(pdev, false);
}
#endif

int xocl_qdma_queue_destroy(struct platform_device *pdev,
	struct xocl_qdma_queue *queue)
{
	char		ebuf[EBUF_LEN + 1];
	int		ret = 0;

	if (queue->flag & XOCL_QDMA_QUEUE_STARTED) {
		ret = qdma_queue_stop(queue->dma_handle, queue->handle,
			ebuf, EBUF_LEN);
		if (ret < 0) {
			xocl_err(&pdev->dev, "Stop queue failed, ret = %d",
				ret);
			goto failed;
		}
		queue->flag &= ~XOCL_QDMA_QUEUE_STARTED;
	}
	if (queue->flag & XOCL_QDMA_QUEUE_ADDED) {
		ret = qdma_queue_remove(queue->dma_handle, queue->handle,
			ebuf, EBUF_LEN);
		if (ret < 0) {
			xocl_err(&pdev->dev, "Remove queue failed, ret = %d",
				ret);
			goto failed;
		}
		queue->flag &= ~XOCL_QDMA_QUEUE_ADDED;
	}

	if (queue->sgl_cache)
		vfree(queue->sgl_cache);

	if (queue->flag & XOCL_QDMA_QUEUE_DONE) {
		mutex_destroy(&queue->lock);
		queue->flag &= ~XOCL_QDMA_QUEUE_DONE;
	}
		
	return 0;
failed:
	ebuf[EBUF_LEN] = 0;
	xocl_err(&pdev->dev, "libqdma: %s", ebuf); 
	return ret;
}

int xocl_qdma_queue_create(struct platform_device *pdev,
	struct qdma_queue_conf *qconf, struct xocl_qdma_queue *queue)
{
	struct xocl_dev	*xdev;
	char		ebuf[EBUF_LEN + 1];
	int		ret;

	xdev = xocl_get_xdev(pdev);

	queue->dma_handle = (unsigned long)xdev->dma_handle;
	ret = qdma_queue_add(queue->dma_handle, qconf, &queue->handle,
		ebuf, EBUF_LEN);
	if (ret < 0) {
		xocl_err(&pdev->dev, "Creating queue failed, ret = %d", ret);
		goto failed;
	}
	queue->flag |= XOCL_QDMA_QUEUE_ADDED;

	ret = qdma_queue_start(queue->dma_handle, queue->handle,
		ebuf, EBUF_LEN);
	if (ret < 0) {
		xocl_err(&pdev->dev, "Starting queue failed, ret = %d", ret);
		goto failed;
	}
	queue->flag |= XOCL_QDMA_QUEUE_STARTED;

	queue->qconf = qdma_queue_get_config(queue->dma_handle, queue->handle,
		ebuf, EBUF_LEN);
	if (!queue->qconf) {
		xocl_err(&pdev->dev, "Query queue config failed");
		goto failed;
	}
	queue->q_len = queue->qconf->rngsz;

	queue->sgl_cache = vzalloc(queue->q_len * sizeof (*queue->sgl_cache));

	mutex_init(&queue->lock);
	queue->flag |= XOCL_QDMA_QUEUE_DONE;
	return 0;

failed:
	xocl_qdma_queue_destroy(pdev, queue);
	ebuf[EBUF_LEN] = 0;
	xocl_err(&pdev->dev, "libqdma: %s", ebuf); 
	return ret;
}

ssize_t xocl_qdma_post_wr(struct platform_device *pdev,
	struct xocl_qdma_queue *queue,
	struct qdma_request *wr, struct sg_table *sgt, off_t off)
{
	struct xocl_dev *xdev;
	struct scatterlist *sg, *sgl;
	struct qdma_sw_sg *qdma_sg = queue->sgl_cache;
	int max_sg, req_sg;
	off_t tmp_off;
	uint64_t ep_addr;
	int i;
	size_t left, req_len;
	ssize_t ret = 0, total = 0;

	xdev = xocl_get_xdev(pdev);

	wr->sgl = qdma_sg;

	mutex_lock(&queue->lock);
	req_sg = sgt->nents;
	sgl = sgt->sgl;
	tmp_off = off;
	req_len = wr->count;
	left = wr->count;
	ep_addr = wr->ep_addr;
	while (total < req_len) {
		/* offset sgl */
		for_each_sg(sgl, sg, req_sg, i) {
			if (tmp_off < sg->length) {
				break;
			}
			tmp_off -= sg->length;
		}
		if (i == req_sg && tmp_off + left > sg->length) {
			xocl_err(&pdev->dev,
				"Invalid length %ld, offset %ld, left %ld, "
				"total %ld, sg %d, req_sg %d",
				req_len,  tmp_off, left, total, i, req_sg);
			ret = -EINVAL;
			break;
		}
		req_sg -= i;

		sgl = sg;
		wr->count = 0;
		max_sg = qdma_queue_avail_desc((unsigned long)xdev->dma_handle,
			queue->handle);

		for_each_sg(sgl, sg, min(req_sg, max_sg), i) {
			qdma_sg[i].pg = sg_page(sg);
			qdma_sg[i].offset = sg->offset;
			qdma_sg[i].len = min(left, (size_t)sg->length);
			qdma_sg[i].dma_addr = 0;

			if (tmp_off > 0) {
				qdma_sg[i].offset += tmp_off;
				qdma_sg[i].len =
					min(left, (size_t)sg->length - tmp_off);
				tmp_off = 0;
			}
			left -= qdma_sg[i].len;
			wr->count += qdma_sg[i].len;
			if (!left) {
				i++;
				break;
			} else {
				qdma_sg[i].next = &qdma_sg[i + 1];
			}
		}

		qdma_sg[i - 1].next = NULL;
		wr->sgcnt = i;

		ret = qdma_request_submit((unsigned long)xdev->dma_handle,
			queue->handle, wr);
		if (ret < 0) {
			xocl_err(&pdev->dev, "request submit failed. "
				"offset %d, len %d, sgcnt %d\n",
				 qdma_sg[0].offset, wr->count, wr->sgcnt);
			break;
		}
		total += ret;
		wr->ep_addr += ret;
		left += wr->count - ret;
		tmp_off = ret;
	}

	mutex_unlock(&queue->lock);

	return total > 0 ? total : ret;
}

void xocl_dump_sgtable(struct device *dev, struct sg_table *sgt)
{
	int i;
	struct page *pg;
	struct scatterlist *sg = sgt->sgl;
	unsigned long long pgaddr;
	int nents = sgt->orig_nents;

        for (i = 0; i < nents; i++, sg = sg_next(sg)) {
	        if (!sg)
       		     break;
                pg = sg_page(sg);
                if (!pg)
                        continue;
                pgaddr = page_to_phys(pg);
                xocl_err(dev, "%i, 0x%llx, offset %d, len %d\n",
			i, pgaddr, sg->offset, sg->length);
        }
}

/* INIT */
static int (*xocl_drv_reg_funcs[])(void) __initdata = {
	xocl_init_feature_rom,
	xocl_init_mm_xdma,
	xocl_init_mm_qdma,
	xocl_init_str_qdma,
	xocl_init_mb_scheduler,
	xocl_init_mailbox,
	xocl_init_icap,
	xocl_init_xvc,
	xocl_init_drv_user_xdma,
	xocl_init_drv_user_qdma,
};

static void (*xocl_drv_unreg_funcs[])(void) = {
	xocl_fini_feature_rom,
	xocl_fini_mm_xdma,
	xocl_fini_mm_qdma,
	xocl_fini_str_qdma,
	xocl_fini_mb_scheduler,
	xocl_fini_mailbox,
	xocl_fini_icap,
	xocl_fini_xvc,
	xocl_fini_drv_user_xdma,
	xocl_fini_drv_user_qdma,
};

static int __init xocl_init(void)
{
	int		ret, i;

	xrt_class = class_create(THIS_MODULE, "xrt_user");
	if (IS_ERR(xrt_class)) {
		ret = PTR_ERR(xrt_class);
		goto err_class_create;
	}

	for (i = 0; i < ARRAY_SIZE(xocl_drv_reg_funcs); ++i) {
		ret = xocl_drv_reg_funcs[i]();
		if (ret) {
			goto failed;
		}
	}

	return 0;

failed:
	for (i--; i >= 0; i--) {
		xocl_drv_unreg_funcs[i]();
	}
	class_destroy(xrt_class);

err_class_create:
	return ret;
}

static void __exit xocl_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(xocl_drv_unreg_funcs) - 1; i >= 0; i--) {
		xocl_drv_unreg_funcs[i]();
	}

	class_destroy(xrt_class);
}

module_init(xocl_init);
module_exit(xocl_exit);

MODULE_VERSION(XOCL_DRIVER_VERSION);

MODULE_DESCRIPTION(XOCL_DRIVER_DESC);
MODULE_AUTHOR("Lizhi Hou <lizhi.hou@xilinx.com>");
MODULE_LICENSE("GPL v2");
