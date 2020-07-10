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

#ifndef PCI_EXT_CAP_ID_REBAR
#define PCI_EXT_CAP_ID_REBAR 0x15
#endif

#ifndef PCI_REBAR_CTRL
#define PCI_REBAR_CTRL		8
#endif

#ifndef PCI_REBAR_CTRL_BAR_SIZE
#define  PCI_REBAR_CTRL_BAR_SIZE	0x00001F00
#endif

#ifndef PCI_REBAR_CTRL_BAR_SHIFT
#define  PCI_REBAR_CTRL_BAR_SHIFT		8
#endif

#define REBAR_FIRST_CAP		4


#define P2P_ADDR_HI(addr)		((u32)(((addr) >> 32) & 0xffffffff))
#define P2P_ADDR_LO(addr)		((u32)((addr) & 0xffffffff))

#define P2P_RBAR_TO_BYTES(rbar_sz)	(1UL << ((rbar_sz) + 20))
#define P2P_BYTES_TO_RBAR(bytes)	(fls64((bytes) + 1) - 21)

struct remapper_regs {
	u32	ver;
	u32	cap;
	u32	slot_num;
	u32	rsvd1;
	u32	base_addr_lo;
	u32	base_addr_hi;
	u32	log_range;
} __attribute__((packed));

#define SLOT_START_OFF		0x800

struct p2p {
	struct platform_device	*pdev;
	void		__iomem	*remapper;
	struct mutex		p2p_lock;

	int			p2p_bar_idx;
	ulong			p2p_bar_len;
	u64			p2p_exp_bar_sz;

	ulong			p2p_mem_chunk_num;
	void			*p2p_mem_chunks;
	ulong			p2p_mem_chunk_ref;

	ulong			remap_slot_num;
	ulong			remap_slot_sz;
	ulong			remap_range;

	bool			sysfs_created;
};

struct p2p_mem_chunk {
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

	/* Used by remap */
	uint			remap_ref;
	ulong			ep_addr;
	ulong			map_head_chunk;
	ulong			map_chunk_num;
};

#define remap_reg_rd(g, r)				\
	((g->remapper) ?				\
	XOCL_READ_REG32(&((struct remapper_regs *)g->remapper)->r) : -ENODEV)
#define remap_reg_wr(g, v, r)				\
	((g->remapper) ?				\
	XOCL_WRITE_REG32(v, &((struct remapper_regs *)g->remapper)->r): -ENODEV)

#define SLOT(g, s)	(g->remapper + SLOT_START_OFF + ((ulong)(s) << 3))

#define remap_write_slot(g, s, epa)				\
 	do {							\
		if (!g->remapper)					\
			break;						\
		XOCL_WRITE_REG32(P2P_ADDR_LO(epa), SLOT(g, s));		\
		XOCL_WRITE_REG32(P2P_ADDR_HI(epa), SLOT(g, s) + 4);	\
	} while (0)

#define remap_get_max_slot_sz(p2p)			\
	(1UL << (remap_reg_rd(p2p, cap) & 0xff))
#define remap_get_max_slot_num(p2p)			\
	((remap_reg_rd(p2p, cap) >> 16) & 0x1ff)

/* for legacy platforms only */
static int legacy_identify_p2p_bar(struct p2p *p2p)
{
	struct pci_dev *pdev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	resource_size_t bar_len;
	int i;

	for (i = PCI_STD_RESOURCES; i <= PCI_STD_RESOURCE_END; i++) {
		bar_len = pci_resource_len(pdev, i);
		if (bar_len >= XOCL_P2P_CHUNK_SIZE) {
			p2p->p2p_bar_idx = i;
			return 0;
		}
	}

	p2p->p2p_bar_idx = -1;
	return -ENOTSUPP;
}

static bool p2p_is_enabled(struct p2p *p2p)
{
	if (!p2p->p2p_mem_chunks) {
		p2p_info(p2p, "no mem chunks");
		return false;
	} else if (p2p->p2p_exp_bar_sz != 0 && p2p->p2p_exp_bar_sz != p2p->p2p_bar_len)
		return false;

	return true;
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
	#if (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 6))
	unsigned long __percpu *percpu_count = (unsigned long __percpu *)
		(ref->percpu_count_ptr & ~__PERCPU_REF_ATOMIC_DEAD);
	unsigned long count = 0;
	int cpu;

	/* Nasty hack for CentOS 7.6 and above versions (7.7, 7.8 etc.)
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
static void p2p_mem_chunk_release(struct p2p *p2p, struct p2p_mem_chunk *chk)
{
//	struct pci_dev *pdev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	struct platform_device  *pdev = p2p->pdev;

	/*
	 * When reseration fails, error handling could bring us here with
	 * ref == 0. Since we've already cleaned up during reservation error
	 * handling, nothing needs to be done now.
	 */
	if (chk->xpmc_ref == 0)
		return;

	chk->xpmc_ref--;
	if (chk->xpmc_ref == 0) {
		if (chk->xpmc_res_grp)
			devres_release_group(&pdev->dev, chk->xpmc_res_grp);
		else
			BUG_ON(1);

		chk->xpmc_va = NULL;
		chk->xpmc_res_grp = NULL;
	}

	p2p_info(p2p, "released P2P mem chunk [0x%llx, 0x%llx), cur ref: %d",
		chk->xpmc_pa, chk->xpmc_pa + chk->xpmc_size, chk->xpmc_ref);
}

static int p2p_mem_chunk_reserve(struct p2p *p2p, struct p2p_mem_chunk *chk)
{
//	struct pci_dev *pdev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	struct platform_device  *pdev = p2p->pdev;
	struct device *dev = &pdev->dev;
	struct resource res;
	struct percpu_ref *pref = &chk->xpmc_percpu_ref;
	int ret;

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
			p2p_err(p2p, "add kill action failed");
			percpu_ref_kill(pref);
		}
	}
#elif	defined(P2P_API_V2)
	ret = devm_add_action_or_reset(dev, p2p_percpu_ref_exit, pref);
	if (ret) {
		p2p_err(p2p, "add exit action failed");
		percpu_ref_exit(pref);
	} else {
		chk->xpmc_pgmap.res = res;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
		chk->xpmc_pgmap.ref = pref;
		chk->xpmc_pgmap.altmap_valid = false;
#else
		chk->xpmc_pgmap.type = MEMORY_DEVICE_PCI_P2PDMA;
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
		p2p_mem_chunk_release(p2p, chk);
		ret = ret ? ret : -ENOMEM;
	}

done:
	p2p_info(p2p,
		"reserved P2P mem chunk [0x%llx, 0x%llx), ret: %d, cur ref: %d",
		chk->xpmc_pa, chk->xpmc_pa+chk->xpmc_size, ret, chk->xpmc_ref);

	return ret;
}

static int p2p_rbar_len(struct p2p *p2p, ulong *rbar_sz)
{
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	int pos;
	u32 ctrl, cap;

	pos = pci_find_ext_capability(pcidev, PCI_EXT_CAP_ID_REBAR);
	if (!pos) {
		p2p_info(p2p, "rebar cap does not exist");
		return -ENOTSUPP;
	}

	if (!rbar_sz)
		return 0;

	pos += REBAR_FIRST_CAP;
	pos += PCI_REBAR_CTRL * p2p->p2p_bar_idx;

	pci_read_config_dword(pcidev, pos, &cap);
	pci_read_config_dword(pcidev, pos + 4, &ctrl);

	*rbar_sz = P2P_RBAR_TO_BYTES((ctrl & PCI_REBAR_CTRL_BAR_SIZE) >>
		PCI_REBAR_CTRL_BAR_SHIFT);

	return 0;
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

	if (!p2p_is_enabled(p2p))
		return;

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
		mb_p2p->p2p_bar_addr = pci_resource_start(pcidev,
				p2p->p2p_bar_idx);
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


static int p2p_mem_fini(struct p2p *p2p)
{
	struct p2p_mem_chunk *chunks;
	int i;

	if (!p2p->p2p_mem_chunks)
		return 0;

	chunks = p2p->p2p_mem_chunks;
	for (i = 0; i < p2p->p2p_mem_chunk_num; i++) {
		if (chunks[i].xpmc_ref > 0) {
			p2p_err(p2p, "still %d ref for P2P chunk[%d]",
				chunks[i].xpmc_ref, i);
			chunks[i].xpmc_ref = 1;
			p2p_mem_chunk_release(p2p, &chunks[i]);
		}
	}

	vfree(p2p->p2p_mem_chunks);

	p2p->p2p_mem_chunk_num = 0;
	p2p->p2p_mem_chunks = NULL;
	p2p->p2p_mem_chunk_ref = 0;

	remap_reg_wr(p2p, 0, slot_num);
	p2p->remap_slot_num = 0;
	p2p->remap_range = 0;

	/* Reset Virtualization registers */
	(void) p2p_read_addr_mgmtpf(p2p);

	return 0;
}

static int p2p_mem_init(struct p2p *p2p)
{
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	resource_size_t pa;
	struct p2p_mem_chunk *chunks;
	int i;

	if (p2p->p2p_mem_chunks) {
		p2p_info(p2p, "already initialized");
		return 0;
	}

	/* init chunk table */
	p2p_info(p2p, "Init chunks. BAR len %ld, chunk sz %ld",
			p2p->p2p_bar_len, XOCL_P2P_CHUNK_SIZE);
	if (!p2p->p2p_bar_len)
		return 0;

	if (!p2p->remapper) {
		/* assume remap the entired bar */
		p2p->remap_range = p2p->p2p_bar_len;
		p2p->remap_slot_sz = XOCL_P2P_CHUNK_SIZE;
	} else {
		p2p->remap_range = remap_get_max_slot_sz(p2p) *
			remap_get_max_slot_num(p2p);
		if (!p2p->remap_range) {
			p2p_err(p2p, "Invalid range max slot size %ld, max slot num %d", remap_get_max_slot_sz(p2p), remap_get_max_slot_num(p2p));
			return -EINVAL;
		}

		if (p2p->remap_range > p2p->p2p_bar_len)
			p2p->remap_range = p2p->p2p_bar_len;

		p2p->remap_slot_sz = p2p->remap_range / remap_get_max_slot_num(p2p);
		if (p2p->remap_slot_sz < XOCL_P2P_CHUNK_SIZE)
			p2p->remap_slot_sz = XOCL_P2P_CHUNK_SIZE;
	}
	/* range is 2 ** n */
	p2p->remap_slot_num = p2p->remap_range / p2p->remap_slot_sz;

	p2p->p2p_mem_chunk_num = p2p->p2p_bar_len / XOCL_P2P_CHUNK_SIZE;
	p2p->p2p_mem_chunks = vzalloc(sizeof(struct p2p_mem_chunk) *
			p2p->p2p_mem_chunk_num);
	if (!p2p->p2p_mem_chunks)
		return -ENOMEM;
	chunks = p2p->p2p_mem_chunks;

	pa = pci_resource_start(pcidev, p2p->p2p_bar_idx);
	for (i = 0; i < p2p->p2p_mem_chunk_num; i++) {
		chunks[i].xpmc_pa = pa + XOCL_P2P_CHUNK_SIZE * i; 
		chunks[i].xpmc_size = XOCL_P2P_CHUNK_SIZE;
		chunks[i].ep_addr = ~0UL;
		init_completion(&chunks[i].xpmc_comp);
	}


	remap_reg_wr(p2p, 0, slot_num);
	remap_reg_wr(p2p, P2P_ADDR_LO(pa), base_addr_lo);
	remap_reg_wr(p2p, P2P_ADDR_HI(pa), base_addr_hi);
	remap_reg_wr(p2p, fls64(p2p->remap_range) - 1, log_range);

	p2p_info(p2p, "Init remapper. range %ld, slot size %ld, num %ld",
		p2p->remap_range, p2p->remap_slot_sz, p2p->remap_slot_num);

	/* Pass P2P bar address and len to mgmtpf */
	(void) p2p_read_addr_mgmtpf(p2p);

	return 0;
}

static int p2p_configure(struct p2p *p2p, ulong range)
{
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	struct resource *res = pcidev->resource + p2p->p2p_bar_idx;
	int pos, ret = 0;
	u16 cmd;
	ulong rbar_sz, flags;
	u32 ctrl;

	p2p_info(p2p, "Configuring p2p, range %ld", range);
	if (range < XOCL_P2P_CHUNK_SIZE) {
		p2p_info(p2p, "p2p bar is too small");
		return -ENOTSUPP;
	}

	pos = pci_find_ext_capability(pcidev, PCI_EXT_CAP_ID_REBAR);
	if (!pos) {
		p2p_info(p2p, "rebar cap does not exist");
		if (p2p->p2p_bar_len < range) {
			p2p_info(p2p, "bar size less than requested range");
			return -ENOTSUPP;
		}

		p2p_mem_fini(p2p);
		ret = p2p_mem_init(p2p);
		return ret;
	}

	if (range != p2p->p2p_bar_len)
		p2p_mem_fini(p2p);

	pos += p2p->p2p_bar_idx * PCI_REBAR_CTRL;
	pci_read_config_dword(pcidev, pos + PCI_REBAR_CTRL, &ctrl);

	rbar_sz = P2P_RBAR_TO_BYTES((ctrl & PCI_REBAR_CTRL_BAR_SIZE) >>
		PCI_REBAR_CTRL_BAR_SHIFT);

	if (p2p->p2p_bar_len)
		pci_release_selected_regions(pcidev, (1 << p2p->p2p_bar_idx));

	pci_read_config_word(pcidev, PCI_COMMAND, &cmd);
	pci_write_config_word(pcidev, PCI_COMMAND, cmd & ~PCI_COMMAND_MEMORY);
	ctrl &= ~PCI_REBAR_CTRL_BAR_SIZE;
	ctrl |= P2P_BYTES_TO_RBAR(range) << PCI_REBAR_CTRL_BAR_SHIFT;
	pci_write_config_dword(pcidev, pos + PCI_REBAR_CTRL, ctrl);

	if (range == p2p->p2p_bar_len) {
		pci_write_config_word(pcidev, PCI_COMMAND,
				cmd | PCI_COMMAND_MEMORY);
		goto done;
	}

	flags = res->flags;
	if (res->parent)
		release_resource(res);

	res->start = 0;
	res->end = range - 1;

	pci_assign_unassigned_bus_resources(pcidev->bus);

	res->flags = flags;
	p2p->p2p_bar_len = (ulong) pci_resource_len(pcidev, p2p->p2p_bar_idx);
	pci_write_config_word(pcidev, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);
	if (p2p->p2p_bar_len)
		ret = p2p_mem_init(p2p);

done:
	if (p2p->p2p_bar_len) {
		pci_request_selected_regions(pcidev, (1 << p2p->p2p_bar_idx),
			NODE_P2P);
	} else {
		p2p_err(p2p, "Not enough IO space, please warm reboot");
		ret = -ENXIO;
	}

	return ret;
}

static int p2p_reserve_release(struct p2p *p2p, ulong off, ulong sz,
	       bool reserve)
{
	ulong start_index = off / XOCL_P2P_CHUNK_SIZE;
	ulong num_chunks = ALIGN((off % XOCL_P2P_CHUNK_SIZE) + sz,
			XOCL_P2P_CHUNK_SIZE) / XOCL_P2P_CHUNK_SIZE;
	struct p2p_mem_chunk *chk = p2p->p2p_mem_chunks;
	long i, ret = 0;

	/* Make sure P2P is init'ed before we do anything. */
	if (p2p->p2p_mem_chunk_num == 0)
		return -EINVAL;

	for (i = start_index; i < start_index + num_chunks; i++) {
		if (reserve)
			ret = p2p_mem_chunk_reserve(p2p, &chk[i]);
		else
			p2p_mem_chunk_release(p2p, &chk[i]);

		if (ret)
			break;
	}

	/* Undo reserve, if failed. */
	if (ret) {
		for (chk--; chk + 1 != p2p->p2p_mem_chunks; chk--)
			p2p_mem_chunk_release(p2p, chk);
	} else {
		if (reserve)
			p2p->p2p_mem_chunk_ref += num_chunks;
		else
			p2p->p2p_mem_chunk_ref -= num_chunks;
	}

	return ret;
}

static void p2p_bar_unmap(struct p2p *p2p, ulong bar_off)
{
	struct p2p_mem_chunk *chunk;
	ulong idx;
	ulong num;
	int i;

	idx = bar_off / XOCL_P2P_CHUNK_SIZE;
	chunk = p2p->p2p_mem_chunks;
	num = chunk[idx].map_chunk_num;
	for (i = chunk[idx].map_head_chunk; i < num; i++) {
		chunk[i].remap_ref--;
		if (chunk[i].remap_ref == 0)
			chunk[i].ep_addr = ~0UL;
		chunk[i].map_head_chunk = 0;
		chunk[i].map_chunk_num = 0;
	}
}

static long p2p_bar_map(struct p2p *p2p, ulong bank_addr, ulong bank_size)
{
	struct p2p_mem_chunk *chunk;
	long i, j, bar_off;
	ulong ep_addr, ep_size, addr;
	ulong num;

	p2p_info(p2p, "bank addr %lx, sz %ld, slots %ld",
		bank_addr, bank_size, p2p->remap_slot_num);
	chunk = p2p->p2p_mem_chunks;
	if (!chunk)
		return -EINVAL;

	ep_addr = rounddown(bank_addr, XOCL_P2P_CHUNK_SIZE);
	ep_size = roundup(bank_size, XOCL_P2P_CHUNK_SIZE);
	num = ep_size / XOCL_P2P_CHUNK_SIZE;
	if (num > p2p->p2p_mem_chunk_num)
		return -ENOENT;

	for (i = 0; i <= p2p->p2p_mem_chunk_num - num; i++) {
		if (chunk[i].ep_addr != ~0UL && chunk[i].ep_addr != ep_addr)
			continue;

		for (j = i; j < i + num; j++) {
			addr = ep_addr + (j - i) * XOCL_P2P_CHUNK_SIZE;
			if (chunk[j].ep_addr != ~0UL && chunk[j].ep_addr != addr)
				break;
		}
		if (j == i + num)
			break;
	}

	if (i > p2p->p2p_mem_chunk_num - num)
		return -ENOENT;

	/* mark all slots */
	bar_off = i * XOCL_P2P_CHUNK_SIZE + ep_addr % XOCL_P2P_CHUNK_SIZE;
	j = i - (ep_addr - rounddown(bank_addr, p2p->remap_slot_sz)) /
		XOCL_P2P_CHUNK_SIZE;
	ep_addr = rounddown(bank_addr, p2p->remap_slot_sz);
	ep_size = roundup(bank_size, p2p->remap_slot_sz);
	num = ep_size / XOCL_P2P_CHUNK_SIZE;

	p2p_info(p2p, "mark %ld - %ld chunks", j, j +  num - 1);

	remap_reg_wr(p2p, 0, slot_num);
	for (i = j; i < j + num; i++) {
		chunk[i].remap_ref++;
		chunk[i].ep_addr = ep_addr;
		chunk[j].map_head_chunk = j;
		chunk[j].map_chunk_num = num;

		if (ep_addr % p2p->remap_slot_sz == 0)
			remap_write_slot(p2p,
			    i * XOCL_P2P_CHUNK_SIZE / p2p->remap_slot_sz, ep_addr);
		ep_addr += XOCL_P2P_CHUNK_SIZE;
	}
	remap_reg_wr(p2p, p2p->remap_slot_num, slot_num);

	return bar_off;
}

static int p2p_mem_unmap(struct platform_device *pdev, ulong bar_off,
		ulong len)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	int ret = 0;

	if (p2p->p2p_bar_idx < 0) {
		p2p_err(p2p, "can not find p2p bar");
		return -EINVAL;
	}

	mutex_lock(&p2p->p2p_lock);

	p2p_reserve_release(p2p, bar_off, len, false);
	p2p_bar_unmap(p2p, bar_off);

	mutex_unlock(&p2p->p2p_lock);

	return ret;
}

static int p2p_mem_map(struct platform_device *pdev,
		ulong bank_addr, ulong bank_size,
		ulong offset, ulong len, ulong *bar_off)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	long bank_off = 0;
	int ret;

	if (p2p->p2p_bar_idx < 0) {
		p2p_err(p2p, "can not find p2p bar");
		return -EINVAL;
	}

	mutex_lock(&p2p->p2p_lock);

	if (bank_addr == ~0UL && p2p->remapper) {
		/* do not need to reserve bar space if remapper present */
		ret = 0;
		goto  failed;
	}

	p2p_info(p2p, "map bank addr 0x%lx, size %ld, offset %ld, len %ld",
			bank_addr, bank_size, offset, len);

	bank_off = p2p_bar_map(p2p, bank_addr, bank_size);
	if (bank_off < 0) {
		ret = -ENOENT;
		goto failed;
	}

	if (!len) {
		ret = 0;
		goto failed;
	}

	ret = p2p_reserve_release(p2p, bank_off + offset, len, true);
	if (ret) {
		p2p_err(p2p, "reserve p2p chunks failed ret = %d", ret);
		p2p_bar_unmap(p2p, bank_off);
		goto failed;
	}

	p2p_info(p2p, "map bar offset %ld", bank_off + offset);

	if (bar_off)
		*bar_off = bank_off + offset;
failed:
	mutex_unlock(&p2p->p2p_lock);
	return ret;
}

static int p2p_mem_init_locked(struct platform_device *pdev)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	int ret = 0;


	if (p2p->p2p_bar_idx < 0)
		return 0;

	mutex_lock(&p2p->p2p_lock);
	ret = p2p_mem_init(p2p);

	mutex_unlock(&p2p->p2p_lock);
	return ret;
}

static int p2p_mem_cleanup_locked(struct platform_device *pdev)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	int ret = 0;

	if (p2p->p2p_bar_idx < 0)
		return 0;

	mutex_lock(&p2p->p2p_lock);

	p2p_mem_fini(p2p);

	mutex_unlock(&p2p->p2p_lock);
	return ret;
}

static int p2p_mem_get_pages(struct platform_device *pdev,
	ulong bar_off, ulong size, struct page **pages, ulong npages)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	struct p2p_mem_chunk *chunk;
	ulong i;
	ulong offset;
	int ret = 0;

	if (p2p->p2p_bar_idx < 0) {
		p2p_err(p2p, "can not find p2p bar");
		return -EINVAL;
	}

	p2p_info(p2p, "bar_off: %ld, size %ld, npages %ld",
		bar_off, size, npages);

	mutex_lock(&p2p->p2p_lock);
	if (!p2p_is_enabled(p2p)) {
		p2p_err(p2p, "p2p is not enabled");
		ret = -EINVAL;
		goto failed;
	}

	chunk = p2p->p2p_mem_chunks;
	for (i = 0, offset = bar_off; i < npages; i++, offset += PAGE_SIZE) {
		int idx = offset >> XOCL_P2P_CHUNK_SHIFT;
		void *addr;

		if (idx >= p2p->p2p_mem_chunk_num) {
			p2p_err(p2p, "not enough space");
			ret = -EINVAL;
			break;
		} else if (chunk[idx].xpmc_ref == 0) {
			p2p_err(p2p, "map is not created");
			ret = -EINVAL;
			break;
		}
		addr = chunk[idx].xpmc_va;
		addr += offset & (XOCL_P2P_CHUNK_SIZE - 1);
		pages[i] = virt_to_page(addr);
		if (IS_ERR(pages[i])) {
			p2p_err(p2p, "get p2p pages failed");
			ret = -EINVAL;
			break;
		}
	}
failed:
	mutex_unlock(&p2p->p2p_lock);

	return ret;
}

struct xocl_p2p_funcs p2p_ops = {
	.mem_map = p2p_mem_map,
	.mem_unmap = p2p_mem_unmap,
	.mem_init = p2p_mem_init_locked,
	.mem_cleanup = p2p_mem_cleanup_locked,
	.mem_get_pages = p2p_mem_get_pages,
};

static ssize_t config_store(struct device *dev, struct device_attribute *da,
		const char *buf, size_t count)
{
	struct p2p *p2p = platform_get_drvdata(to_platform_device(dev));
	long range;
	int ret = 0;

	if (kstrtol(buf, 10, &range)) {
		p2p_err(p2p, "invalid input");
		return -EINVAL;
	}

	if (p2p->p2p_bar_idx < 0) {
		p2p_err(p2p, "can not find p2p bar");
		return -EINVAL;
	}

	if (range == 0 && p2p->p2p_exp_bar_sz > 0) {
		/*  used hardcoded range */
		range = p2p->p2p_exp_bar_sz;
	} else if (range == -1) {
		/* disable p2p */
		mutex_lock(&p2p->p2p_lock);
		if (p2p->p2p_exp_bar_sz > XOCL_P2P_CHUNK_SIZE) {
			ret = p2p_configure(p2p, XOCL_P2P_CHUNK_SIZE);
		}
		if (ret)
			p2p_mem_fini(p2p);
		mutex_unlock(&p2p->p2p_lock);
		return count;
	} 

	mutex_lock(&p2p->p2p_lock);
	p2p_configure(p2p, range);
	mutex_unlock(&p2p->p2p_lock);

	return count;
}

static ssize_t config_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct p2p *p2p = platform_get_drvdata(to_platform_device(dev));
	ulong rbar_len = 0;
	int ret;
	ssize_t count = 0;

	mutex_lock(&p2p->p2p_lock);
	if (p2p->p2p_bar_idx >= 0) {
		count += sprintf(buf, "bar:%ld\n", p2p->p2p_bar_len);
	}

	count += sprintf(buf + count, "exp_bar:%lld\n", p2p->p2p_exp_bar_sz);

	ret = p2p_rbar_len(p2p, &rbar_len);
	if (!ret)
		count += sprintf(buf + count, "rbar:%ld\n", rbar_len);

	if (p2p->remapper) {
		count += sprintf(buf + count, "remap:%ld\n",
				p2p->remap_range);
	}

	mutex_unlock(&p2p->p2p_lock);

	return count;
}

static DEVICE_ATTR(config, 0644, config_show, config_store);

static ssize_t p2p_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct p2p *p2p = platform_get_drvdata(to_platform_device(dev));
	ssize_t count = 0;

	if (p2p_is_enabled(p2p))
		count = sprintf(buf, "1\n");
	else
		count = sprintf(buf, "0\n");

	return count;
}

static DEVICE_ATTR_RO(p2p_enable);

static struct attribute *p2p_attrs[] = {
	&dev_attr_config.attr,
	&dev_attr_p2p_enable.attr,
	NULL,
};

static struct attribute_group p2p_attr_group = {
	.attrs = p2p_attrs,
};

static void p2p_sysfs_destroy(struct p2p *p2p)
{
	if (!p2p->sysfs_created)
		return;

	sysfs_remove_group(&p2p->pdev->dev.kobj, &p2p_attr_group);
	p2p->sysfs_created = false;
}

static int p2p_sysfs_create(struct p2p *p2p)
{
	int ret;

	if (p2p->sysfs_created)
		return 0;

	ret = sysfs_create_group(&p2p->pdev->dev.kobj, &p2p_attr_group);
	if (ret) {
		p2p_err(p2p, "create ert attrs failed: 0x%x", ret);
		return ret;
	}
	p2p->sysfs_created = true;

	return 0;
}

static int p2p_remove(struct platform_device *pdev)
{
	struct p2p *p2p;
	struct pci_dev *pcidev;
	void *hdl;

	p2p = platform_get_drvdata(pdev);
	if (!p2p) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	xocl_drvinst_release(p2p, &hdl);

	p2p_sysfs_destroy(p2p);
	p2p_mem_fini(p2p);

	if (p2p->remapper)
		iounmap(p2p->remapper);

	pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	if (p2p->p2p_bar_len)
		pci_release_selected_regions(pcidev, (1 << p2p->p2p_bar_idx));

	mutex_destroy(&p2p->p2p_lock);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

static int p2p_probe(struct platform_device *pdev)
{
	struct p2p *p2p;
	struct resource *res;
	struct pci_dev *pcidev;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int ret = 0, i = 0;

	p2p = xocl_drvinst_alloc(&pdev->dev, sizeof(*p2p));
	if (!p2p) {
		xocl_err(&pdev->dev, "failed to alloc data");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, p2p);
	p2p->pdev = pdev;
	mutex_init(&p2p->p2p_lock);

	for (res = platform_get_resource(pdev, IORESOURCE_MEM, i); res;
	    res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {
		if (!strncmp(res->name, NODE_REMAP_P2P,
		    strlen(NODE_REMAP_P2P))) {
			p2p->remapper = ioremap_nocache(res->start,
					res->end - res->start + 1);
		}
	}

	p2p->p2p_bar_idx = xocl_fdt_get_p2pbar(xdev, XDEV(xdev)->fdt_blob);
	if (p2p->p2p_bar_idx < 0) {
		xocl_info(&pdev->dev, "can not find p2p bar in metadata");
		if (!xocl_subdev_is_vsec(xdev))
			legacy_identify_p2p_bar(p2p);
	}

	if (p2p->p2p_bar_idx < 0)
		return 0;

	pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	p2p->p2p_bar_len = (ulong) pci_resource_len(pcidev, p2p->p2p_bar_idx);
	if (p2p->p2p_bar_len < XOCL_P2P_CHUNK_SIZE) {
		xocl_err(&pdev->dev, "p2p bar len is 0");
		p2p->p2p_bar_idx = -1;
		goto failed;
	}

	if (XDEV(xdev)->priv.p2p_bar_sz > 0) {
		p2p->p2p_exp_bar_sz = XDEV(xdev)->priv.p2p_bar_sz;
		p2p->p2p_exp_bar_sz <<= 30;
	} else if (p2p_rbar_len(p2p, NULL))
		p2p->p2p_exp_bar_sz = p2p->p2p_bar_len;
	else {
		p2p->p2p_exp_bar_sz = xocl_get_ddr_channel_size(xdev) *
		       	xocl_get_ddr_channel_count(xdev); /* GB */
		p2p->p2p_exp_bar_sz <<= 30;
	}

	pci_request_selected_regions(pcidev, (1 << p2p->p2p_bar_idx),
		NODE_P2P);

	ret = p2p_mem_init(p2p);;
	if (ret)
		goto failed;

	ret = p2p_sysfs_create(p2p);
	if (ret)
		goto failed;

	return 0;

failed:
	p2p_remove(pdev);
	return ret;
}

struct xocl_drv_private p2p_priv = {
	.ops = &p2p_ops,
};

struct platform_device_id p2p_id_table[] = {
	{ XOCL_DEVNAME(XOCL_P2P), (kernel_ulong_t)&p2p_priv },
	{ },
};

static struct platform_driver	p2p_driver = {
	.probe		= p2p_probe,
	.remove		= p2p_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_P2P),
	},
	.id_table = p2p_id_table,
};

int __init xocl_init_p2p(void)
{
	return platform_driver_register(&p2p_driver);
}

void xocl_fini_p2p(void)
{
	platform_driver_unregister(&p2p_driver);
}
