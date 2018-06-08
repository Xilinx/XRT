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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>

#include "xdev.h"
#include "qdma_mbox.h"


/*
 * qdma device management
 * maintains a list of the qdma devices
 */
static LIST_HEAD(xdev_list);
static DEFINE_MUTEX(xdev_mutex);

#ifndef list_last_entry
#define list_last_entry(ptr, type, member) \
		list_entry((ptr)->prev, type, member)
#endif

struct xlnx_dma_dev *xdev_list_first(void)
{
	struct xlnx_dma_dev *xdev;

	mutex_lock(&xdev_mutex);
	xdev = list_first_entry(&xdev_list, struct xlnx_dma_dev, list_head);
	mutex_unlock(&xdev_mutex);
	
	return xdev;
}

struct xlnx_dma_dev *xdev_list_next(struct xlnx_dma_dev *xdev)
{
	struct xlnx_dma_dev *next;

	mutex_lock(&xdev_mutex);
	next = list_next_entry(xdev, list_head);
	mutex_unlock(&xdev_mutex);

	return next;
}

int xdev_list_dump(char *buf, int buflen)
{
        struct xlnx_dma_dev *xdev, *tmp;
	int len = 0;

        mutex_lock(&xdev_mutex);
        list_for_each_entry_safe(xdev, tmp, &xdev_list, list_head) {
		len += sprintf(buf + len, "qdma%d\t%02x:%02x.%02x\n",
				xdev->conf.idx, xdev->conf.pdev->bus->number,
				PCI_SLOT(xdev->conf.pdev->devfn),
				PCI_FUNC(xdev->conf.pdev->devfn));
		if (len >= buflen)
			break;
        }
        mutex_unlock(&xdev_mutex);

	buf[len] = '\0';
	return len;	
}

static inline void xdev_list_add(struct xlnx_dma_dev *xdev)
{
	mutex_lock(&xdev_mutex);
	if (list_empty(&xdev_list))
		xdev->conf.idx = 0;
	else {
		struct xlnx_dma_dev *last;

		last = list_last_entry(&xdev_list, struct xlnx_dma_dev, list_head);
		xdev->conf.idx = last->conf.idx + 1;
	}
	list_add_tail(&xdev->list_head, &xdev_list);
	mutex_unlock(&xdev_mutex);
}

#undef list_last_entry

static inline void xdev_list_remove(struct xlnx_dma_dev *xdev)
{
	mutex_lock(&xdev_mutex);
	list_del(&xdev->list_head);
	mutex_unlock(&xdev_mutex);
}

struct xlnx_dma_dev *xdev_find_by_pdev(struct pci_dev *pdev)
{
        struct xlnx_dma_dev *xdev, *tmp;

        mutex_lock(&xdev_mutex);
        list_for_each_entry_safe(xdev, tmp, &xdev_list, list_head) {
                if (xdev->conf.pdev == pdev) {
                        mutex_unlock(&xdev_mutex);
                        return xdev;
                }
        }
        mutex_unlock(&xdev_mutex);
        return NULL;
}

struct xlnx_dma_dev *xdev_find_by_idx(int idx)
{
        struct xlnx_dma_dev *xdev, *tmp;

        mutex_lock(&xdev_mutex);
        list_for_each_entry_safe(xdev, tmp, &xdev_list, list_head) {
                if (xdev->conf.idx == idx) {
                        mutex_unlock(&xdev_mutex);
                        return xdev;
                }
        }
        mutex_unlock(&xdev_mutex);
        return NULL;
}

int xdev_check_hndl(const char *fname, struct pci_dev *pdev, unsigned long hndl)
{
	struct xlnx_dma_dev *xdev;

	if (!pdev)
		return -EINVAL;

	xdev = xdev_find_by_pdev(pdev);
	if (!xdev) {
		pr_info("%s pdev 0x%p, hndl 0x%lx, NO match found!\n",
			fname, pdev, hndl);
		return -EINVAL;
	}
	if (((unsigned long)xdev) != hndl) {
		pr_info("%s pdev 0x%p, hndl 0x%lx != 0x%p!\n",
			fname, pdev, hndl, xdev);
		return -EINVAL;
	}

	 if (xdev->conf.pdev != pdev) {
                pr_info("pci_dev(0x%lx) != pdev(0x%lx)\n",
                        (unsigned long)xdev->conf.pdev, (unsigned long)pdev);
		return -EINVAL;
        }

	return 0;
}

/**********************************************************************
 * PCI-level Functions
 **********************************************************************/

/*
 * Unmap the BAR regions that had been mapped earlier using map_bars()
 */
static void xdev_unmap_bars(struct xlnx_dma_dev *xdev, struct pci_dev *pdev)
{
	int i;

	for (i = 0; i < XDMA_MAX_BARS; i++) {
		/* is this BAR mapped? */
		if (xdev->bar[i]) {
			/* unmap BAR */
			pci_iounmap(pdev, xdev->bar[i]);
			/* mark as unmapped */
			xdev->bar[i] = NULL;
		}
	}
}

/* map_bars() -- map device regions into kernel virtual address space
 *
 * Map the device memory regions into kernel virtual address space after
 * verifying their sizes respect the minimum sizes needed
 */
static int xdev_map_bars(struct xlnx_dma_dev *xdev, struct pci_dev *pdev)
{
	struct qdma_dev_conf *conf = &xdev->conf;
	int rv;
	int i;

#ifdef __QDMA_VF__
	/* QDMA: hard code the VF config bar to be 0 */
	conf->bar_num_config = 0;
#else
	conf->bar_num_config = -1;
#endif
	/* iterate through all the BARs */
	for (i = 0; i < XDMA_MAX_BARS; i++) {
		int map_len;

		map_len = pci_resource_len(pdev, i);
		if (map_len > XDMA_MAX_BAR_LEN_MAPPED)
			map_len = XDMA_MAX_BAR_LEN_MAPPED;

		xdev->bar[i] = pci_iomap(pdev, i, map_len);
		if (!xdev->bar[i])
			break;

		/* Try to identify BAR as XDMA control BAR */
		if (conf->bar_num_config < 0) {
			u32 id = readl(xdev->bar[i]);

			if ((id & 0xFFFF0000) == 0x1FD30000) {
				conf->bar_num_config = i;
				pr_info("%s DMA BAR: %d.\n", xdev->conf.name, i);
			}
		}
	}

	/* The XDMA config BAR must always be present */
	if (conf->bar_num_config < 0) {
		pr_info("Failed to detect XDMA config BAR\n");
		rv = -EINVAL;
		goto fail;
	}

	xdev->regs = xdev->bar[(int)conf->bar_num_config];

	/* successfully mapped all required BAR regions */
	return 0;

fail:
	/* unwind; unmap any BARs that we did map */
	xdev_unmap_bars(xdev, pdev);
	return rv;
}

static struct xlnx_dma_dev *xdev_alloc(struct qdma_dev_conf *conf)
{
	struct xlnx_dma_dev *xdev;

	/* allocate zeroed device book keeping structure */
	xdev = kzalloc(sizeof(struct xlnx_dma_dev), GFP_KERNEL);
	if (!xdev) {
		pr_info("OOM, xlnx_dma_dev.\n");
		return NULL;
	}
	spin_lock_init(&xdev->lock);
	spin_lock_init(&xdev->mbox_lock);
	init_waitqueue_head(&xdev->mbox_wq);

	/* create a driver to device reference */
	memcpy(&xdev->conf, conf, sizeof(*conf));

	xdev->conf.bar_num_config = -1;
	xdev->conf.bar_num_user = -1;
	xdev->conf.bar_num_bypass = -1;

	return xdev;
}

static int pci_dma_mask_set(struct pci_dev *pdev)
{
	/* 64-bit addressing capability for XDMA? */
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(64))) {
		/* use 32-bit DMA for descriptors */
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		/* use 64-bit DMA, 32-bit for consistent */
	} else if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) {
		pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
		/* use 32-bit DMA */
		dev_info(&pdev->dev, "Using a 32-bit DMA mask.\n");
	} else {
		dev_info(&pdev->dev, "No suitable DMA possible.\n");
		return -EINVAL;
	}

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
static void pci_enable_relaxed_ordering(struct pci_dev *pdev)
{
	pcie_capability_set_word(pdev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_RELAX_EN);
}
#else
static void pci_enable_relaxed_ordering(struct pci_dev *pdev)
{
	u16 v;
	int pos;

	pos = pci_pcie_cap(pdev);
	if (pos > 0) {
		pci_read_config_word(pdev, pos + PCI_EXP_DEVCTL, &v);
		v |= PCI_EXP_DEVCTL_RELAX_EN;
		pci_write_config_word(pdev, pos + PCI_EXP_DEVCTL, v);
	}
}
#endif

int qdma_device_init(struct xlnx_dma_dev *);
void qdma_device_cleanup(struct xlnx_dma_dev *);

int qdma_device_open(const char *mod_name, struct qdma_dev_conf *conf,
			unsigned long *dev_hndl)
{
	struct pci_dev *pdev = conf->pdev;
	struct xlnx_dma_dev *xdev = NULL;
	int rv = 0;

	*dev_hndl = 0UL;
	conf->bar_num_config = -1;
	conf->bar_num_user = -1;
	conf->bar_num_bypass = -1;

	if (!pdev) {
		pr_info("%s: pci device NULL.\n", mod_name);
		return -EINVAL;
	}

	pr_debug("%s, %02x:%02x.%02x, pdev 0x%p, 0x%x:0x%x.\n",
		mod_name, pdev->bus->number, PCI_SLOT(pdev->devfn),
		PCI_FUNC(pdev->devfn), pdev, pdev->vendor, pdev->device);

	xdev = xdev_find_by_pdev(pdev);
	if (xdev) {
		pr_warn("%s, device %s already attached!\n",
			mod_name, dev_name(&pdev->dev));
                return -EINVAL;
        }

	rv = pci_request_regions(pdev, mod_name);
	if (rv) {
                /* Just info, some other driver may have claimed the device. */
                dev_info(&pdev->dev, "cannot obtain PCI resources\n");
		return rv;
        }

	rv = pci_enable_device(pdev);
	if (rv) {
		dev_err(&pdev->dev, "cannot enable PCI device\n");
		goto release_regions;
	}

	/* enable relaxed ordering */
	pci_enable_relaxed_ordering(pdev);

	/* enable bus master capability */
	pci_set_master(pdev);

	rv = pci_dma_mask_set(pdev);
	if (rv)
		goto disable_device; 

	/* allocate zeroed device book keeping structure */
	xdev = xdev_alloc(conf);
	if (!xdev)
		goto disable_device; 

	xdev_flag_set(xdev, XDEV_FLAG_OFFLINE);
	xdev_list_add(xdev);

	rv = sprintf(xdev->conf.name, "qdma%d-p%s",
		xdev->conf.idx, dev_name(&xdev->conf.pdev->dev));
	xdev->conf.name[rv] = '\0';

	rv = xdev_map_bars(xdev, pdev);
	if (rv)
		goto unmap_bars;

	rv = qdma_device_init(xdev);
	if (rv < 0) {
		pr_warn("qdma_init failed %d.\n", rv);
		goto cleanup_qdma;
	}
	xdev_flag_clear(xdev, XDEV_FLAG_OFFLINE);
	qdma_mbox_timer_init(xdev);
        //qdma_mbox_timer_start(xdev);

	pr_info("%s, %d, pdev 0x%p, xdev 0x%p, usr %u, ch %u,%u, q %u, vf %u.\n",
                dev_name(&pdev->dev), xdev->conf.idx, pdev, xdev,
		conf->user_max, conf->h2c_channel_max, conf->c2h_channel_max,
		conf->qsets_max, conf->vf_max);

	*dev_hndl = (unsigned long)xdev;

	memcpy(conf, &xdev->conf, sizeof(*conf));

#ifdef __QDMA_VF__
	rv = xdev_sriov_vf_online(xdev, 0);
#elif defined(CONFIG_PCI_IOV)
	if (conf->vf_max)
		rv = xdev_sriov_enable(xdev, conf->vf_max);
#endif

	return 0;

cleanup_qdma:
	qdma_device_cleanup(xdev);

unmap_bars:
	xdev_unmap_bars(xdev, pdev);

	xdev_list_remove(xdev);
	kfree(xdev);

disable_device:
	pci_disable_device(pdev);

release_regions:
	pci_release_regions(pdev);

	return rv;
}

void qdma_device_close(struct pci_dev *pdev, unsigned long dev_hndl)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (!dev_hndl)
		return;

	if (xdev_check_hndl(__func__, pdev, dev_hndl) < 0)
		return;

	if (xdev->conf.pdev != pdev) {
		pr_info("pci_dev(0x%lx) != pdev(0x%lx)\n",
			(unsigned long)xdev->conf.pdev, (unsigned long)pdev);
	}
#ifdef __QDMA_VF__
	xdev_sriov_vf_offline(xdev, 0);
#elif defined(CONFIG_PCI_IOV)
	xdev_sriov_disable(xdev);
#endif

	qdma_device_cleanup(xdev);

        qdma_mbox_timer_stop(xdev);

	xdev_unmap_bars(xdev, pdev);

	pci_release_regions(pdev);
	pci_disable_device(pdev);

	xdev_list_remove(xdev);

	kfree(xdev);
}

struct qdma_dev_conf *qdma_device_get_config(unsigned long dev_hndl,
					char *ebuf, int ebuflen)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (xdev_check_hndl(__func__, xdev->conf.pdev, dev_hndl) < 0)
		return NULL;

	return &xdev->conf;
}

unsigned int qdma_device_read_config_register(unsigned long dev_hndl,
					unsigned int reg_addr)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (xdev_check_hndl(__func__, xdev->conf.pdev, dev_hndl) < 0)
		return -EINVAL;

        return readl(xdev->regs + reg_addr);
}

void qdma_device_write_config_register(unsigned long dev_hndl,
				unsigned int reg_addr, unsigned int val)
{
	struct xlnx_dma_dev *xdev = (struct xlnx_dma_dev *)dev_hndl;

	if (xdev_check_hndl(__func__, xdev->conf.pdev, dev_hndl) < 0)
		return;

        pr_debug("%s reg 0x%x, w 0x%08x.\n", xdev->conf.name, reg_addr, val);
        writel(val, xdev->regs + reg_addr);
}

void sgt_dump(struct sg_table *sgt)
{
	int i;
	struct scatterlist *sg = sgt->sgl;

	pr_info("sgt 0x%p, sgl 0x%p, nents %u/%u.\n",
		sgt, sgt->sgl, sgt->nents, sgt->orig_nents);

	for (i = 0; i < sgt->orig_nents; i++, sg = sg_next(sg))
		pr_info("%d, 0x%p, pg 0x%p,%u+%u, dma 0x%llx,%u.\n",
			i, sg, sg_page(sg), sg->offset, sg->length,
			sg_dma_address(sg), sg_dma_len(sg)); 
}

int sgt_find_offset(struct sg_table *sgt, unsigned int offset,
		struct scatterlist **sg_p, unsigned int *sg_offset)
{
	struct scatterlist *sg = sgt->sgl;
	int max = sgt->nents;
	unsigned int len = 0;
	int i;

	for (i = 0;  i < max; i++, sg = sg_next(sg)) {
		len += sg_dma_len(sg);

		if (len == offset) {
			*sg_p = sg_next(sg);	
			*sg_offset = 0;
			return ++i;
		} else if (len > offset) {
			*sg_p = sg;	
			*sg_offset = len - offset;
			return i;
		}
	}

	return -EINVAL;
}
