/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
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
/*
 * P2P Linux kernel API has gone through changes over the time. We are trying
 * to maintain our driver compabile w/ all kernels we support here.
 */
#include "../xocl_drv.h"
#include <linux/iommu.h>

#if KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#define P2P_API_V0
#elif KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE && \
	(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
#define P2P_API_V1
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
#define P2P_API_V2
#elif defined(RHEL_RELEASE_VERSION) /* CentOS/RedHat specific check */

#if RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(7, 3) && \
	RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 6)
#define P2P_API_V1
#elif RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 6)
#define P2P_API_V2
#endif

#endif

#if defined(P2P_API_V1) || defined(P2P_API_V2)
#include <linux/memremap.h>
#endif

#define p2p_err(p2p, fmt, arg...)		\
	xocl_err(&(p2p)->pdev->dev, fmt "\n", ##arg)
#define p2p_info(p2p, fmt, arg...)		\
	xocl_info(&(p2p)->pdev->dev, fmt "\n", ##arg)

struct remapper_regs {
	u32	ver;
	u32	cap;
	u32	entry_num;
	u64	base_addr;
	u32	log_range;
} __attribute__((packed));

#define ENTRY_OFFSET		0x800

struct p2p {
	struct platform_device	*pdev;
	void		__iomem	*remapper;
	struct mutex		p2p_lock;
	ulong			bank_sz;
	uint			bank_num;
	int			p2p_bar_idx;
	ulong			p2p_bar_len;

	ulong			p2p_mem_chunk_num;
	void			*p2p_mem_chunks;
};

struct p2p_mem_chunk {
	struct p2p		*p2p;
	void			*xpmc_res_grp;
	void __iomem		*xpmc_va;
	resource_size_t		xpmc_pa;
	resource_size_t		xpmc_size;
	int			xpmc_ref;

	/* Used by kernel API */
	struct percpu_ref	xpmc_percpu_ref;
	struct completion	xpmc_comp;
#ifdef  P2P_API_V2
	struct dev_pagemap	xpmc_pgmap;
#endif
};

#define remap_reg_rd(g, r)				\
	XOCL_READ_REG32(&((struct remapper_regs *)g->remapper)->r)
#define remap_reg_wr(g, v, r)				\
	XOCL_WRITE_REG32(v, &((struct remapper_regs *)g->remapper)->r)
#define remap_entry(g, e)				\
	(g->remapper + ENTRY_OFFSET + ((ulong)e << 3))

static int p2p_enable(struct platform_device *pdev, ulong bank_sz, uint bank_num)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	p2p->bank_sz = bank_sz;
	p2p->bank_num = bank_num;

	return 0;
}

static void p2p_percpu_ref_release(struct percpu_ref *ref)
{
	struct p2p_mem_chunk *chk =
		container_of(ref, struct p2p_mem_chunk, xpmc_percpu_ref);
	complete(&chk->xpmc_comp);
}

static void p2p_percpu_ref_kill(void *data)
{
	struct percpu_ref *ref = data;
#if defined(RHEL_RELEASE_CODE)
	#if (RHEL_RELEASE_CODE == RHEL_RELEASE_VERSION(7, 7))
	unsigned long __percpu *percpu_count = (unsigned long __percpu *)
		(ref->percpu_count_ptr & ~__PERCPU_REF_ATOMIC_DEAD);
	unsigned long count = 0;
	int cpu;

	/* Nasty hack for CentOS7.7 only
	 * percpu_ref->count have to substract the percpu counters
	 * to guarantee the percpu_ref->count will drop to 0
	 */
	for_each_possible_cpu(cpu)
		count += *per_cpu_ptr(percpu_count, cpu);

	rcu_read_lock_sched();
	atomic_long_sub(count, &ref->count);
	rcu_read_unlock_sched();
	#endif
#endif

	percpu_ref_kill(ref);
}

static void p2p_percpu_ref_kill_noop(struct percpu_ref *ref)
{
	/* Used for pgmap, no op here */
}

static void p2p_percpu_ref_exit(void *data)
{
	struct percpu_ref *ref = data;
	struct p2p_mem_chunk *chk =
		container_of(ref, struct p2p_mem_chunk, xpmc_percpu_ref);

	wait_for_completion(&chk->xpmc_comp);
	percpu_ref_exit(ref);
}
static void p2p_mem_chunk_release(struct p2p_mem_chunk *chk)
{
	struct p2p *p2p = chk->p2p;
	struct pci_dev *pdev = XOCL_PL_TO_PCI_DEV(p2p->pdev);

	BUG_ON(!mutex_is_locked(&p2p->p2p_lock));

	/*
	 * When reseration fails, error handling could bring us here with
	 * ref == 0. Since we've already cleaned up during reservation error
	 * handling, nothing needs to be done now.
	 */
	if (chk->xpmc_ref == 0)
		return;

	chk->xpmc_ref--;
	if (chk->xpmc_ref == 0) {
		if (chk->xpmc_va)
			devres_release_group(&pdev->dev, chk->xpmc_res_grp);
		else if (chk->xpmc_res_grp)
			devres_remove_group(&pdev->dev, chk->xpmc_res_grp);
		else
			BUG_ON(1);

		chk->xpmc_va = NULL;
		chk->xpmc_res_grp = NULL;
	}

	p2p_info(p2p, "released P2P mem chunk [0x%llx, 0x%llx), cur ref: %d",
		chk->xpmc_pa, chk->xpmc_pa + chk->xpmc_size, chk->xpmc_ref);
}

static int p2p_mem_chunk_reserve(struct p2p_mem_chunk *chk)
{
	struct p2p *p2p = chk->p2p;
	struct pci_dev *pdev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	struct device *dev = &pdev->dev;
	struct resource res;
	struct percpu_ref *pref = &chk->xpmc_percpu_ref;
	int ret;

	BUG_ON(!mutex_is_locked(&p2p->p2p_lock));
	BUG_ON(chk->xpmc_ref < 0);

	if (chk->xpmc_ref > 0) {
		chk->xpmc_ref++;
		ret = 0;
		goto done;
	}

	if (percpu_ref_init(pref, p2p_percpu_ref_release, 0, GFP_KERNEL)) {
		p2p_err(p2p, "init percpu ref failed");
		ret = -EFAULT;
		goto done;
	}

	BUG_ON(chk->xpmc_res_grp);
	chk->xpmc_res_grp = devres_open_group(dev, NULL, GFP_KERNEL);
	if (!chk->xpmc_res_grp) {
		percpu_ref_exit(pref);
		p2p_err(p2p, "open p2p resource group failed");
		ret = -EFAULT;
		goto done;
	}

	res.start = chk->xpmc_pa;
	res.end = res.start + chk->xpmc_size - 1;
	res.name = NULL;
	res.flags = IORESOURCE_MEM;

	/* Suppressing the defined-but-not-used warning */
	{
		void *fn= NULL;
		ret = 0;
		fn = (void *)p2p_percpu_ref_exit;
		fn = (void *)p2p_percpu_ref_kill_noop;
		fn = (void *)p2p_percpu_ref_kill;
	}

#if	defined(P2P_API_V0)
	chk->xpmc_va = devm_memremap_pages(dev, &res);
#elif	defined(P2P_API_V1)
	ret = devm_add_action_or_reset(dev, p2p_percpu_ref_exit, pref);
	if (ret) {
		p2p_err(p2p, "add exit action failed");
		percpu_ref_exit(pref);
	} else {
		chk->xpmc_va = devm_memremap_pages(dev, &res,
			&chk->xpmc_percpu_ref, NULL);	
		ret = devm_add_action_or_reset(dev,
			p2p_percpu_ref_kill, pref);
		if (ret != 0) {
			p2p_err(dev, "add kill action failed");
			percpu_ref_kill(pref);
		}
	}
#elif	defined(P2P_API_V2)
	ret = devm_add_action_or_reset(dev, p2p_percpu_ref_exit, pref);
	if (ret) {
		p2p_err(p2p, "add exit action failed");
		percpu_ref_exit(pref);
	} else {
		chk->xpmc_pgmap.ref = pref;
		chk->xpmc_pgmap.res = res;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
		chk->xpmc_pgmap.altmap_valid = false;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 2) && \
	LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
		chk->xpmc_pgmap.kill = p2p_percpu_ref_kill_noop;
#endif
		chk->xpmc_va = devm_memremap_pages(dev, &chk->xpmc_pgmap);
		ret = devm_add_action_or_reset(dev, p2p_percpu_ref_kill, pref);
		if (ret) {
			p2p_err(p2p, "add kill action failed");
			percpu_ref_kill(pref);
		}
	}
#endif

	devres_close_group(dev, chk->xpmc_res_grp);
	chk->xpmc_ref = 1;

	if (ret || IS_ERR_OR_NULL(chk->xpmc_va)) {
		if (IS_ERR(chk->xpmc_va)) {
			ret = PTR_ERR(chk->xpmc_va);
			chk->xpmc_va = NULL;
		}
		p2p_err(p2p, "reserve p2p chunk failed, releasing");
		p2p_mem_chunk_release(chk);
		ret = ret ? ret : -ENOMEM;
	}

done:
	p2p_info(p2p,
		"reserved P2P mem chunk [0x%llx, 0x%llx), ret: %d, cur ref: %d",
		chk->xpmc_pa, chk->xpmc_pa+chk->xpmc_size, ret, chk->xpmc_ref);

	return ret;
}

static void p2p_read_addr_mgmtpf(struct p2p *p2p)
{
	xdev_handle_t xdev = xocl_get_xdev(p2p->pdev);
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	struct xcl_mailbox_req *mb_req = NULL;
	struct xcl_mailbox_p2p_bar_addr *mb_p2p = NULL;
	size_t mb_p2p_len, reqlen;
	int ret = 0;
	size_t resplen = sizeof(ret);

	mb_p2p_len = sizeof(struct xcl_mailbox_p2p_bar_addr);
	reqlen = sizeof(struct xcl_mailbox_req) + mb_p2p_len;
	mb_req = vzalloc(reqlen);
	if (!mb_req) {
		p2p_err(p2p, "dropped request (%d), mem alloc issue",
			XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR);
		return;
	}

	mb_req->req = XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR;
	mb_p2p = (struct xcl_mailbox_p2p_bar_addr *)mb_req->data;

	if (!iommu_present(&pci_bus_type)){
		mb_p2p->p2p_bar_len = pci_resource_len(pcidev, p2p->p2p_bar_idx);
		mb_p2p->p2p_bar_addr = pci_resource_start(pcidev, p2p->p2p_bar_idx);
	} else {
		mb_p2p->p2p_bar_len = 0;
		mb_p2p->p2p_bar_addr = 0;
	}
	p2p_info(p2p, "sending req %d to peer: bar_len=%lld, bar_addr=%lld",
		XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR,
		mb_p2p->p2p_bar_len, mb_p2p->p2p_bar_addr); 

	ret = xocl_peer_request(xdev, mb_req, reqlen, &ret,
		&resplen, NULL, NULL, 0);
	vfree(mb_req);
	if (ret) {
		p2p_err(p2p, "dropped request (%d), failed with err: %d",
			XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR, ret);
	}
}

void p2p_mem_chunk_init(struct p2p *p2p, struct p2p_mem_chunk *chk,
		resource_size_t off, resource_size_t sz)
{
	chk->p2p = p2p;
	chk->xpmc_pa = off;
	chk->xpmc_size = sz;
	init_completion(&chk->xpmc_comp);
}

static void p2p_remove(struct platform_device *pdev)
{
	struct p2p *p2p;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	struct p2p_mem_chunk *chunks;
	void *hdl;
	int i;

	p2p = platform_get_drvdata(pdev);
	if (!p2p) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return;
	}
	xocl_drvinst_release(p2p, &hdl);

	mutex_lock(&p2p->p2p_lock);
	chunks = p2p->p2p_mem_chunks;
	for (i = 0; i < p2p->p2p_mem_chunk_num; i++) {
		if (chunks[i].xpmc_ref > 0) {
			xocl_err(&pdev->dev, "still %d ref for P2P chunk[%d]",
				chunks[i].xpmc_ref, i);
			chunks[i].xpmc_ref = 1;
			p2p_mem_chunk_release(&chunks[i]);
		}
	}
	mutex_unlock(&p2p->p2p_lock);

	if (p2p->p2p_mem_chunks)
		vfree(p2p->p2p_mem_chunks);

	if (p2p->p2p_bar_len >= 0)
		pci_release_selected_regions(pcidev, 1 << p2p->p2p_bar_idx);

	if (p2p->remapper)
		iounmap(p2p->remapper);

	mutex_destroy(&p2p->p2p_lock);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);
}

static int p2p_probe(struct platform_device *pdev)
{
	struct p2p *p2p;
	struct resource *res;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	struct p2p_mem_chunk *chunks;
	resource_size_t pa;
	int ret, i = 0;

	p2p = xocl_drvinst_alloc(&pdev->dev, sizeof(*p2p));
	if (!p2p)
		return -ENOMEM;

	platform_set_drvdata(pdev, p2p);
	p2p->pdev = pdev;
	mutex_init(&p2p->p2p_lock);

	for (res = platform_get_resource(pdev, IORESOURCE_MEM, i); res;
	    res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {
		if (strncmp(res->name, NODE_REMAP_P2P, strlen(NODE_REMAP_P2P))) {
			p2p->remapper = ioremap_nocache(res->start,
					res->end - res->start + 1);
		}
	}
	if (!p2p->remapper) {
		xocl_err(&pdev->dev, "did not find remapper");
		ret = -EFAULT;
		goto failed;
	}
		
	p2p->p2p_bar_idx = xocl_fdt_get_p2pbar(xdev, XDEV(xdev)->fdt_blob);
	if (p2p->p2p_bar_idx < 0) {
		xocl_err(&pdev->dev, "can not find p2p bar in metadata");
		ret = -ENOTSUPP;
		goto failed;
	}
	pci_request_selected_regions(pcidev, 1 << p2p->p2p_bar_idx,
			NODE_P2P);

	p2p->p2p_bar_len = pci_resource_len(pcidev, ret);
	if (!p2p->p2p_bar_len || p2p->p2p_bar_len % XOCL_P2P_CHUNK_SIZE) {
		xocl_err(&pdev->dev, "invalid bar len %ld", p2p->p2p_bar_len);
		ret = -EINVAL;
		goto failed;
	}

	p2p->p2p_mem_chunk_num = p2p->p2p_bar_len / XOCL_P2P_CHUNK_SIZE;
	p2p->p2p_mem_chunks = vzalloc(sizeof(struct p2p_mem_chunk) *
			p2p->p2p_mem_chunk_num);
	if (p2p->p2p_mem_chunks == NULL) {
		ret = -ENOMEM;
		goto failed;
	}

	pa = pci_resource_start(pcidev, p2p->p2p_bar_idx);
	chunks = p2p->p2p_mem_chunks;
	for (i = 0; i < p2p->p2p_mem_chunk_num; i++) {
		p2p_mem_chunk_init(p2p, &chunks[i],
			pa + ((resource_size_t)i) * XOCL_P2P_CHUNK_SIZE,
			XOCL_P2P_CHUNK_SIZE);
	}

	/* Pass P2P bar address and len to mgmtpf */
	(void) p2p_read_addr_mgmtpf(p2p);

	return 0;

failed:
	p2p_remove(pdev);
	return ret;
}
