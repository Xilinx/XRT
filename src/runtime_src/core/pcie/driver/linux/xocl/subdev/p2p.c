/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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
#include "../xocl_drm.h"
#include <linux/iommu.h>

int p2p_max_bar_size = 128; /* GB */
module_param(p2p_max_bar_size, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(p2p_max_bar_size,
	"Maximum P2P BAR size in GB, default is 128");


#if defined(RHEL_RELEASE_VERSION) /* CentOS/RedHat specific check */
	#if RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(7, 3) && \
		  RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(7, 6)
		#define P2P_API_V1
	#elif RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 6) && \
		  RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 6)
		#define P2P_API_V2
	#else
		#define P2P_API_V3
	#endif

#elif KERNEL_VERSION(4, 5, 0) > LINUX_VERSION_CODE && \
	  (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	#define P2P_API_V0
#elif KERNEL_VERSION(4, 16, 0) > LINUX_VERSION_CODE && \
	  (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0))
	#define P2P_API_V1
#elif KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE && \
	  (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0))
	#define P2P_API_V2
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	#define P2P_API_V3
#endif


#if defined(P2P_API_V1) || defined(P2P_API_V2) || defined(P2P_API_V3)
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
#define P2P_BYTES_TO_RBAR(bytes)	(fls64((bytes) - 1) - 20)
#define P2P_BAR_ROUNDUP(bytes)		(1UL << (fls64((bytes) - 1)))
#define P2P_EXP_BAR_SZ(p2p)						\
	P2P_BAR_ROUNDUP(min((ulong)p2p_max_bar_size * 1024 * 1024 * 1024,	\
			p2p->exp_mem_sz + p2p->user_buf_start))

struct remapper_regs {
	u32	ver;
	u32	cap;
	u32	slot_num;
	u32	rsvd1;
	u32	base_addr_lo;
	u32	base_addr_hi;
	u32	log_range;
	u32	bypass_mode;
	u32	wildcard_mode;
} __attribute__((packed));

#define SLOT_START_OFF		0x800
#define P2P_BANK_CONF_NUM	1024
#define MAX_BANK_TAG_LEN	64

#define P2P_DEFAULT_BAR_SIZE	(256UL << 20)

struct p2p_bank_conf {
	char bank_tag[MAX_BANK_TAG_LEN];
	ulong size;
};

struct p2p {
	struct platform_device	*pdev;
	void		__iomem	*remapper;
	struct mutex		p2p_lock;
	struct xocl_p2p_privdata *priv_data;

	int			p2p_bar_idx;
	ulong			p2p_bar_len;
	ulong			p2p_bar_start;
	u64			p2p_max_mem_sz;
	ulong			exp_mem_sz;

	ulong			p2p_mem_chunk_num;
	void			*p2p_mem_chunks;
	ulong			p2p_mem_chunk_ref;

	ulong			remap_slot_num;
	ulong			remap_slot_sz;
	ulong			remap_range;
	void			*remap_slots;

	ulong			rbar_len;

	bool			sysfs_created;

	struct p2p_bank_conf	*bank_conf;
	ulong			user_buf_start;

	bool			p2p_conf_changed;
};

struct p2p_remap_slot {
	uint			ref;
	ulong			ep_addr;
	ulong			map_head_chunk;
	ulong			map_chunk_num;
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
#if defined(P2P_API_V2) || defined(P2P_API_V3)
	struct dev_pagemap	xpmc_pgmap;
#endif
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

#define remap_get_max_slot_logsz(p2p)			\
	((remap_reg_rd(p2p, cap) & 0xff))
#define remap_get_max_slot_num(p2p)			\
	((remap_reg_rd(p2p, cap) >> 16) & 0x1ff)

static void p2p_ulpmap_release(struct p2p *p2p);

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
	if (p2p->p2p_mem_chunks && P2P_EXP_BAR_SZ(p2p) == p2p->p2p_bar_len)
		return true;

	return false;
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
	#if ((RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7, 6)) && (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 1)))
	unsigned long __percpu *percpu_count = (unsigned long __percpu *)
		(ref->percpu_count_ptr & ~__PERCPU_REF_ATOMIC_DEAD);
	unsigned long count = 0;
	int cpu;

	/* Nasty hack for CentOS 7.6, 7.7 and 7.8)
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
	struct page *page;
	void *addr;

	/*
	 * When reseration fails, error handling could bring us here with
	 * ref == 0. Since we've already cleaned up during reservation error
	 * handling, nothing needs to be done now.
	 */
	if (chk->xpmc_ref == 0 && !chk->xpmc_va)
		return;

	if (chk->xpmc_ref > 0)
		chk->xpmc_ref--;
	if (chk->xpmc_ref == 0) {
		for (addr = chk->xpmc_va; addr < chk->xpmc_va + chk->xpmc_size;
		    addr += PAGE_SIZE) {
			page = virt_to_page(addr);
#if KERNEL_VERSION(4, 5, 0) <= LINUX_VERSION_CODE
			if (page_ref_count(page) > 1)
				break;
#else
			if (atomic_read(&page->_count) > 1)
				break;
#endif
		}
		if (addr < chk->xpmc_va + chk->xpmc_size) {
			p2p_info(p2p, "P2P mem chunk [0x%llx, 0x%llx) is busy",
				chk->xpmc_pa, chk->xpmc_pa + chk->xpmc_size);
			return;
		}

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
	struct resource res = { 0 };
	struct percpu_ref *pref = &chk->xpmc_percpu_ref;
	int ret = 0;

	BUG_ON(chk->xpmc_ref < 0);

	if (chk->xpmc_ref > 0) {
		chk->xpmc_ref++;
		goto done;
	}

	if (chk->xpmc_va) {
		p2p_info(p2p, "reuse P2P mem chunk [0x%llx, 0x%llx)",
			chk->xpmc_pa, chk->xpmc_pa + chk->xpmc_size);
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
	#if defined(RHEL_RELEASE_CODE)
		#if RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(8, 2)
			chk->xpmc_pgmap.ref = pref;
			chk->xpmc_pgmap.altmap_valid = false;
			#if RHEL_RELEASE_CODE == RHEL_RELEASE_VERSION(8, 1)
				chk->xpmc_pgmap.kill = p2p_percpu_ref_kill_noop;
			#endif
		#else
			chk->xpmc_pgmap.type = MEMORY_DEVICE_PCI_P2PDMA;
		#endif
	#else
		chk->xpmc_pgmap.ref = pref;
		chk->xpmc_pgmap.altmap_valid = false;
	#endif
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

#elif   defined(P2P_API_V3)
                ret = devm_add_action_or_reset(dev, p2p_percpu_ref_exit, pref);
                if (ret) {
                        p2p_err(p2p, "add exit action failed");
                        percpu_ref_exit(pref);
                } else {
                        chk->xpmc_pgmap.type = MEMORY_DEVICE_PCI_P2PDMA;
                        chk->xpmc_pgmap.range.start = res.start;
                        chk->xpmc_pgmap.range.end = res.end;
                        chk->xpmc_pgmap.nr_range = 1;


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
	reqlen = struct_size(mb_req, data, 1) + mb_p2p_len;
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
		&resplen, NULL, NULL, 0, 0);
	vfree(mb_req);
	if (ret) {
		p2p_err(p2p, "dropped request (%d), failed with err: %d",
			XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR, ret);
	}
}


static int p2p_mem_fini(struct p2p *p2p, bool free_trunk)
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
	p2p->p2p_mem_chunk_ref = 0;

	p2p_ulpmap_release(p2p);

	if (!free_trunk)
		return 0;

	vfree(p2p->p2p_mem_chunks);
	vfree(p2p->remap_slots);

	p2p->p2p_mem_chunk_num = 0;
	p2p->p2p_mem_chunks = NULL;

	remap_reg_wr(p2p, 0, slot_num);
	p2p->remap_slot_num = 0;
	p2p->remap_range = 0;
	p2p->remap_slots = NULL;

	/* Reset Virtualization registers */
	(void) p2p_read_addr_mgmtpf(p2p);

	return 0;
}

static int p2p_mem_init(struct p2p *p2p)
{
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	resource_size_t pa;
	struct p2p_mem_chunk *chunks;
	struct p2p_remap_slot *slots;
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
		if (fls64(p2p->p2p_bar_len /
		    remap_get_max_slot_num(p2p)) <
		    remap_get_max_slot_logsz(p2p))
			p2p->remap_range = p2p->p2p_bar_len;
		else {
			p2p->remap_range = remap_get_max_slot_num(p2p) <<
				remap_get_max_slot_logsz(p2p);
		}

		p2p->remap_slot_sz = p2p->remap_range /
			remap_get_max_slot_num(p2p);
	}
	/* range is 2 ** n */
	p2p->remap_slot_num = p2p->remap_range / p2p->remap_slot_sz;
	p2p->remap_slots = vzalloc(sizeof(struct p2p_remap_slot) *
		p2p->remap_slot_num);
	if (!p2p->remap_slots)
		return -ENOMEM;

	p2p->p2p_mem_chunk_num = p2p->remap_range / XOCL_P2P_CHUNK_SIZE;
	p2p->p2p_mem_chunks = vzalloc(sizeof(struct p2p_mem_chunk) *
			p2p->p2p_mem_chunk_num);
	if (!p2p->p2p_mem_chunks)
		return -ENOMEM;
	slots = p2p->remap_slots;
	for (i = 0; i < p2p->remap_slot_num; i++)
		slots[i].ep_addr = ~0UL;

	pa = pci_resource_start(pcidev, p2p->p2p_bar_idx);
	chunks = p2p->p2p_mem_chunks;
	for (i = 0; i < p2p->p2p_mem_chunk_num; i++) {
		chunks[i].xpmc_pa = pa + XOCL_P2P_CHUNK_SIZE * i;
		chunks[i].xpmc_size = XOCL_P2P_CHUNK_SIZE;
		init_completion(&chunks[i].xpmc_comp);
	}

	remap_reg_wr(p2p, 0, slot_num);
	if (p2p->priv_data &&
	    p2p->priv_data->flags == XOCL_P2P_FLAG_SIBASE_NEEDED) {
		remap_reg_wr(p2p, P2P_ADDR_LO(pa), base_addr_lo);
		remap_reg_wr(p2p, P2P_ADDR_HI(pa), base_addr_hi);
	}
	remap_reg_wr(p2p, fls64(p2p->remap_range) - 1, log_range);
	remap_reg_wr(p2p, 1, wildcard_mode);

	p2p_info(p2p, "Init remapper. range %ld, slot size %ld, num %ld",
		p2p->remap_range, p2p->remap_slot_sz, p2p->remap_slot_num);

	p2p_rbar_len(p2p, &p2p->rbar_len);
	/* Pass P2P bar address and len to mgmtpf */
	(void) p2p_read_addr_mgmtpf(p2p);

	p2p->p2p_conf_changed = true;

	return 0;
}

static int p2p_configure(struct p2p *p2p, ulong range)
{
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	struct resource *res = pcidev->resource + p2p->p2p_bar_idx;
	int pos, ret = 0;
	u16 cmd;
	ulong rbar_sz, flags, bar_len;
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

		p2p_mem_fini(p2p, true);
		ret = p2p_mem_init(p2p);
		return ret;
	}

	p2p_mem_fini(p2p, true);

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

	flags = res->flags;
	if (res->parent)
		release_resource(res);

	res->start = 0;
	res->end = range - 1;

	pci_assign_unassigned_bus_resources(pcidev->bus);

	res->flags = flags;
	bar_len = (ulong) pci_resource_len(pcidev, p2p->p2p_bar_idx);
	if (!bar_len) {
		p2p_err(p2p, "Not enough IO space, please warm reboot");
		res->start = 0;
		res->end = p2p->p2p_bar_len - 1;
		pci_assign_unassigned_bus_resources(pcidev->bus);
		res->flags = flags;
	}

	p2p->p2p_bar_start = (ulong) pci_resource_start(pcidev,
			p2p->p2p_bar_idx);
	p2p->p2p_bar_len = (ulong) pci_resource_len(pcidev, p2p->p2p_bar_idx);
	pci_write_config_word(pcidev, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);
	if (p2p->p2p_bar_len) {
		ret = p2p_mem_init(p2p);
		pci_request_selected_regions(pcidev, (1 << p2p->p2p_bar_idx),
			NODE_P2P);
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
	struct p2p_remap_slot *slot;
	ulong idx;
	ulong num;
	int i;

	idx = bar_off / p2p->remap_slot_sz;
	slot = p2p->remap_slots;
	if (!slot)
		return;

	num = slot[idx].map_chunk_num;
	for (i = slot[idx].map_head_chunk; i < num; i++) {
		slot[i].ref--;
		if (slot[i].ref == 0)
			slot[i].ep_addr = ~0UL;
		slot[i].map_head_chunk = 0;
		slot[i].map_chunk_num = 0;
	}
}

static void p2p_ulpmap_release(struct p2p *p2p)
{
	struct p2p_remap_slot *slot;
	int i;

	i = p2p->user_buf_start / p2p->remap_slot_sz;
	slot = p2p->remap_slots;
	if (!slot)
		return;

	for (; i < p2p->remap_slot_num; i++) {
		slot[i].ref = 0;
		slot[i].ep_addr = ~0UL;
		slot[i].map_head_chunk = 0;
		slot[i].map_chunk_num = 0;
	}
}

static long p2p_bar_map(struct p2p *p2p, ulong bank_addr, ulong bank_size,
	ulong bar_off_align)
{
	struct p2p_remap_slot *slot;
	long i, j, bar_off;
	ulong ep_addr, ep_size, addr;
	ulong num;

	p2p_info(p2p, "bank addr %lx, sz %ld, slots %ld",
		bank_addr, bank_size, p2p->remap_slot_num);
	slot = p2p->remap_slots;
	if (!slot)
		return -EINVAL;

	ep_addr = rounddown(bank_addr, p2p->remap_slot_sz);
	ep_size = roundup(bank_size, p2p->remap_slot_sz);
	num = ep_size / p2p->remap_slot_sz;
	if (num > p2p->remap_slot_num)
		return -ENOENT;

	for (i = 0; i <= p2p->remap_slot_num - num; i++) {
		if (slot[i].ref && (slot[i].ep_addr != ep_addr ||
		    bank_addr == ~0UL))
			continue;

		if ((i * p2p->remap_slot_sz) % bar_off_align)
			continue;

		for (j = i; j < i + num; j++) {
			if (bank_addr == ~0UL && !slot[i].ref)
				continue;
			addr = ep_addr + (j - i) * p2p->remap_slot_sz;
			if (slot[j].ep_addr != ~0UL && slot[j].ep_addr != addr)
				break;
		}
		if (j == i + num)
			break;
	}

	if (i > p2p->remap_slot_num - num)
		return -ENOENT;

	if (bank_addr == ~0UL) {
		for (j = i; j < i + num; j++) {
			slot[j].ref = 1;
			slot[j].map_head_chunk = i;
			slot[j].map_chunk_num = num;
		}
		bar_off = i * p2p->remap_slot_sz;

		p2p_info(p2p, "mark %ld - %ld chunks", i, i +  num - 1);
		return bar_off;
	}

	/* mark all slots */
	bar_off = i * p2p->remap_slot_sz + ep_addr % p2p->remap_slot_sz;
	j = i - (ep_addr - rounddown(bank_addr, p2p->remap_slot_sz)) /
		p2p->remap_slot_sz;
	ep_addr = rounddown(bank_addr, p2p->remap_slot_sz);
	ep_size = roundup(bank_size, p2p->remap_slot_sz);
	num = ep_size / p2p->remap_slot_sz;

	p2p_info(p2p, "mark %ld - %ld chunks", j, j +  num - 1);

	remap_reg_wr(p2p, 0, slot_num);
	for (i = j; i < j + num; i++) {
		slot[i].ref++;
		slot[i].ep_addr = ep_addr;
		slot[i].map_head_chunk = j;
		slot[i].map_chunk_num = num;

		if (ep_addr % p2p->remap_slot_sz == 0)
			remap_write_slot(p2p, i, ep_addr);
		ep_addr += p2p->remap_slot_sz;
	}
	remap_reg_wr(p2p, p2p->remap_slot_num, slot_num);

	return bar_off;
}

static int p2p_mem_unmap(struct platform_device *pdev, ulong bar_off,
		ulong len)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	int ret = 0;

	if (p2p->p2p_bar_idx < 0)
		return -ENODEV;

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

	if (p2p->p2p_bar_idx < 0)
		return -ENODEV;

	mutex_lock(&p2p->p2p_lock);

	if (bank_addr == ~0UL && p2p->remapper) {
		/* do not need to reserve bar space if remapper present */
		ret = 0;
		goto  failed;
	}

	p2p_info(p2p, "map bank addr 0x%lx, size %ld, offset %ld, len %ld",
			bank_addr, bank_size, offset, len);
	bank_off = p2p_bar_map(p2p, bank_addr, bank_size,
		XOCL_P2P_CHUNK_SIZE);
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

	p2p_mem_fini(p2p, false);

	mutex_unlock(&p2p->p2p_lock);
	return ret;
}

static int p2p_mem_reclaim_locked(struct platform_device *pdev)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	struct p2p_mem_chunk *chunks;
	int i;

	mutex_lock(&p2p->p2p_lock);
	if (!p2p_is_enabled(p2p)) {
		mutex_unlock(&p2p->p2p_lock);
		return 0;
	}

	chunks = p2p->p2p_mem_chunks;
	for (i = 0; i < p2p->p2p_mem_chunk_num; i++) {
		if (chunks[i].xpmc_ref == 0 && chunks[i].xpmc_va) {
			p2p_err(p2p, "reclaim P2P chunk[%d]", i);
			p2p_mem_chunk_release(p2p, &chunks[i]);
		}
	}

	mutex_unlock(&p2p->p2p_lock);

	return 0;
}

static int p2p_mem_get_pages(struct platform_device *pdev,
	ulong bar_off, ulong size, struct page **pages, ulong npages)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	struct p2p_mem_chunk *chunk;
	ulong i;
	ulong offset;
	int ret = 0;

	if (p2p->p2p_bar_idx < 0)
		return -ENODEV;

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

static int p2p_remap_resource(struct platform_device *pdev, int bar_idx,
	struct resource *res, int level)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	long bar_off;
	ulong res_len = res->end - res->start + 1;
	int ret = 0;

	if (bar_idx != p2p->p2p_bar_idx)
		return 0;

	if (!p2p->remapper) {
		p2p_err(p2p, "remap does not exist");
		/* remap bar doesn't exist on u2 platforms, return -ENODEV error
		 */
		return -ENODEV;
	}

	mutex_lock(&p2p->p2p_lock);
	p2p_info(p2p, "Remap reserve resource %pR", res);
	bar_off = p2p_bar_map(p2p, res->start, res_len, 1);
	if (bar_off < 0) {
		p2p_err(p2p, "not enough remap space");
		ret = -ENOENT;
		goto failed;
	}
	res->start = bar_off;
	res->end = bar_off + res_len - 1;

	if (level < XOCL_SUBDEV_LEVEL_URP &&
	    p2p->user_buf_start < bar_off + res_len) {
		p2p->user_buf_start = roundup(bar_off + res_len,
			p2p->remap_slot_sz);
	}

failed:
	mutex_unlock(&p2p->p2p_lock);

	return ret;
}

static int p2p_release_resource(struct platform_device *pdev,
	struct resource *res)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	ulong bar_off;

	if (res->start < p2p->p2p_bar_start ||
	    res->start >= p2p->p2p_bar_start + p2p->p2p_bar_len)
		return 0;

	bar_off = res->start - p2p->p2p_bar_start;
	if (!p2p->remapper) {
		p2p_err(p2p, "remap does not exist");
		return -EINVAL;
	}

	mutex_lock(&p2p->p2p_lock);
	p2p_info(p2p, "Remap release resource %lx", bar_off);
	p2p_bar_unmap(p2p, bar_off);

	mutex_unlock(&p2p->p2p_lock);

	return 0;
}

static int p2p_conf_status(struct platform_device *pdev, bool *changed)
{
	struct p2p *p2p = platform_get_drvdata(pdev);

	mutex_lock(&p2p->p2p_lock);
	*changed = p2p->p2p_conf_changed;
	p2p->p2p_conf_changed = false;
	mutex_unlock(&p2p->p2p_lock);

	return 0;
}

static int p2p_refresh_rbar(struct platform_device *pdev)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	int pos;
	u32 ctrl;

	pos = pci_find_ext_capability(pcidev, PCI_EXT_CAP_ID_REBAR);
	if (!pos) {
		p2p_info(p2p, "rebar cap does not exist");
		return 0;
	}

	mutex_lock(&p2p->p2p_lock);
	pos += p2p->p2p_bar_idx * PCI_REBAR_CTRL;
	pci_read_config_dword(pcidev, pos + PCI_REBAR_CTRL, &ctrl);
	pci_write_config_dword(pcidev, pos + PCI_REBAR_CTRL, ctrl);
	mutex_unlock(&p2p->p2p_lock);

	return 0;
}

static int p2p_get_bar_paddr(struct platform_device *pdev, ulong bank_addr,
			     ulong bank_size, ulong *bar_paddr)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	long bank_off = 0;
	int ret = 0;

	if (p2p->p2p_bar_idx < 0)
		return -ENODEV;

	mutex_lock(&p2p->p2p_lock);

	if (bank_addr == ~0UL && p2p->remapper) {
		/* do not need to reserve bar space if remapper present */
		ret = 0;
		goto  failed;
	}

	bank_off = p2p_bar_map(p2p, bank_addr, bank_size, 1);
	if (bank_off < 0) {
		ret = -ENOENT;
		goto failed;
	}

	if (bar_paddr)
		*bar_paddr = bank_off + p2p->p2p_bar_start;

failed:
	mutex_unlock(&p2p->p2p_lock);
	return ret;
}

static int p2p_adjust_mem_topo(struct platform_device *pdev, void *mem_topo)
{
	struct p2p *p2p = platform_get_drvdata(pdev);
	struct mem_topology *topo = mem_topo;
	int i;
	u64 adjust_sz = 0, sz, fixed_sz;
	u32 align = max(XOCL_P2P_CHUNK_SIZE, p2p->remap_slot_sz);

	if (!p2p_is_enabled(p2p))
		return 0;

	if (p2p->p2p_bar_len >= p2p->exp_mem_sz + p2p->user_buf_start)
		return 0;

	if (!p2p->remapper) {
		p2p_err(p2p, "does not have remapper, can not adjust");
		return -EINVAL;
	}

	fixed_sz = p2p->user_buf_start;
	for (i = 0; i< topo->m_count; ++i) {
		if (!XOCL_IS_P2P_MEM(topo, i) || !topo->m_mem_data[i].m_used)
			continue;
		if (convert_mem_tag(topo->m_mem_data[i].m_tag) == MEM_TAG_HOST)
			continue;

		sz = roundup((topo->m_mem_data[i].m_size << 10), align);
		if (sz <= align)
			fixed_sz += sz;
		else
			adjust_sz += sz;
	}

	if (adjust_sz + fixed_sz <= p2p->p2p_bar_len)
		return 0;

	p2p_info(p2p, "can not cover all memory, adjust bank sizes");

	for (i = 0; i< topo->m_count; ++i) {
		if (!XOCL_IS_P2P_MEM(topo, i) || !topo->m_mem_data[i].m_used)
			continue;
		if (convert_mem_tag(topo->m_mem_data[i].m_tag) == MEM_TAG_HOST)
			continue;

		sz = roundup((topo->m_mem_data[i].m_size << 10), align);
		if (sz <= align)
			continue;

		topo->m_mem_data[i].m_size = (p2p->p2p_bar_len - fixed_sz) /
			align * (sz / align) / (adjust_sz / align);
		topo->m_mem_data[i].m_size *= align;
		topo->m_mem_data[i].m_size >>= 10;

		p2p_info(p2p, "adjusted bank %d to %lld k", i,
				topo->m_mem_data[i].m_size);
	}

	return 0;

}
struct xocl_p2p_funcs p2p_ops = {
	.mem_map = p2p_mem_map,
	.mem_unmap = p2p_mem_unmap,
	.mem_init = p2p_mem_init_locked,
	.mem_cleanup = p2p_mem_cleanup_locked,
	.mem_reclaim = p2p_mem_reclaim_locked,
	.mem_get_pages = p2p_mem_get_pages,
	.remap_resource = p2p_remap_resource,
	.release_resource = p2p_release_resource,
	.conf_status = p2p_conf_status,
	.refresh_rbar = p2p_refresh_rbar,
	.get_bar_paddr = p2p_get_bar_paddr,
	.adjust_mem_topo = p2p_adjust_mem_topo,
};

static ssize_t config_store(struct device *dev, struct device_attribute *da,
		const char *buf, size_t count)
{
	struct p2p *p2p = platform_get_drvdata(to_platform_device(dev));
	char *p_tag, *p_sz;
	long idx, sz;

	p_tag = strnstr(buf, ":", count);
	if (!p_tag)
		return -EINVAL;
	*p_tag = 0;
	p_tag++;

	p_sz = strnstr(p_tag, ":", count - (ulong)(p_tag - buf));
	if (!p_sz)
		return -EINVAL;
	*p_sz = 0;
	p_sz++;

	if (kstrtol(buf, 10, &idx) || idx > P2P_BANK_CONF_NUM - 2)
		return -EINVAL;

	if (kstrtol(p_sz, 10, &sz))
		return -EINVAL;

	if (p2p->p2p_bar_idx < 0)
		return -ENODEV;

	mutex_lock(&p2p->p2p_lock);
	strncpy(p2p->bank_conf[idx].bank_tag, p_tag, MAX_BANK_TAG_LEN - 1);
	p2p->bank_conf[idx].size = sz;

	p2p->exp_mem_sz = 0;
	for (idx = 0;
	    idx < P2P_BANK_CONF_NUM && p2p->bank_conf[idx].size != 0;
	    idx++) {
		p2p->exp_mem_sz += roundup(p2p->bank_conf[idx].size,
				p2p->remap_slot_sz);
	}
	if (p2p->exp_mem_sz > p2p->p2p_max_mem_sz) {
		p2p_err(p2p, "invalid range %ld", p2p->exp_mem_sz);
		mutex_unlock(&p2p->p2p_lock);
		return -EINVAL;
	}
	mutex_unlock(&p2p->p2p_lock);
	return count;
}

static ssize_t config_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct p2p *p2p = platform_get_drvdata(to_platform_device(dev));
	int i;
	ssize_t count = 0;

	mutex_lock(&p2p->p2p_lock);
	if (p2p->p2p_bar_idx >= 0) {
		count += sprintf(buf, "bar:%ld\n", p2p->p2p_bar_len);
	}

	count += sprintf(buf + count, "max_bar:%lld\n", p2p->p2p_max_mem_sz);
	count += sprintf(buf + count, "exp_bar:%ld\n", P2P_EXP_BAR_SZ(p2p));
	if (p2p->rbar_len > 0)
		count += sprintf(buf + count, "rbar:%ld\n", p2p->rbar_len);

	if (p2p->remapper) {
		count += sprintf(buf + count, "remap:%ld\n",
				p2p->remap_range);
	}

	for (i = 0; i < P2P_BANK_CONF_NUM && p2p->bank_conf[i].size != 0; i++) {
		char *tag = p2p->bank_conf[i].bank_tag;
		int n = snprintf(buf + count, PAGE_SIZE - count, "%d:%s:%ld\n", i,
			tag ? tag : "", p2p->bank_conf[i].size);

		if (n < 0 || n >= PAGE_SIZE - count)
			break; // Can't fit in
		count += n;
	}

	mutex_unlock(&p2p->p2p_lock);

	return count;
}

static DEVICE_ATTR(config, 0644, config_show, config_store);

static ssize_t bar_map_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct p2p *p2p = platform_get_drvdata(to_platform_device(dev));
	ssize_t count = 0;

	mutex_lock(&p2p->p2p_lock);
	count += sprintf(buf, "slot_size: %ld\n", p2p->remap_slot_sz);
	count += sprintf(buf + count, "slot_num: %ld\n", p2p->remap_slot_num);
	count += sprintf(buf + count, "ulp_start: %ld\n", p2p->user_buf_start);
	mutex_unlock(&p2p->p2p_lock);

	return count;
}

static DEVICE_ATTR_RO(bar_map);

static ssize_t p2p_enable_store(struct device *dev, struct device_attribute *da,
		const char *buf, size_t count)
{
	struct p2p *p2p = platform_get_drvdata(to_platform_device(dev));
	xdev_handle_t xdev = xocl_get_xdev(p2p->pdev);
	ulong range = 0;
	u32 val = 0;

	if (!capable(CAP_SYS_ADMIN))
		return -EACCES;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&p2p->p2p_lock);
	if (val > 0) {
		if (p2p->exp_mem_sz > p2p->p2p_max_mem_sz) {
			p2p_err(p2p, "Invalid configure, exp_bar %ld",
				p2p->exp_mem_sz);
			mutex_unlock(&p2p->p2p_lock);
			return -EINVAL;
		}
		range = P2P_EXP_BAR_SZ(p2p);
	} else
		range = P2P_DEFAULT_BAR_SIZE;

	mutex_unlock(&p2p->p2p_lock);

	xocl_subdev_destroy_by_baridx(xdev, p2p->p2p_bar_idx);

	mutex_lock(&p2p->p2p_lock);
	p2p_configure(p2p, range);
	mutex_unlock(&p2p->p2p_lock);

	xocl_subdev_create_by_baridx(xdev, p2p->p2p_bar_idx);

	return count;
}

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

static DEVICE_ATTR_RW(p2p_enable);

static struct attribute *p2p_attrs[] = {
	&dev_attr_config.attr,
	&dev_attr_p2p_enable.attr,
	&dev_attr_bar_map.attr,
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

static int __p2p_remove(struct platform_device *pdev)
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
	p2p_mem_fini(p2p, true);

	if (p2p->remapper)
		iounmap(p2p->remapper);

	pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	if (p2p->p2p_bar_len)
		pci_release_selected_regions(pcidev, (1 << p2p->p2p_bar_idx));

	if (p2p->bank_conf)
		vfree(p2p->bank_conf);

	mutex_destroy(&p2p->p2p_lock);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void p2p_remove(struct platform_device *pdev)
{
	__p2p_remove(pdev);
}
#else
#define p2p_remove __p2p_remove
#endif

static int p2p_probe(struct platform_device *pdev)
{
	struct p2p *p2p;
	struct resource *res;
	struct pci_dev *pcidev;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	long bar_sz;
	int ret = 0, i = 0;

	p2p = xocl_drvinst_alloc(&pdev->dev, sizeof(*p2p));
	if (!p2p) {
		xocl_err(&pdev->dev, "failed to alloc data");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, p2p);
	p2p->pdev = pdev;
	mutex_init(&p2p->p2p_lock);

	p2p->priv_data = XOCL_GET_SUBDEV_PRIV(&pdev->dev);
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

	p2p->bank_conf = vzalloc(P2P_BANK_CONF_NUM * sizeof(p2p->bank_conf[0]));
	if (!p2p->bank_conf) {
		ret = -ENOMEM;
		goto failed;
	}

	pcidev = XOCL_PL_TO_PCI_DEV(p2p->pdev);
	p2p->p2p_bar_start = (ulong) pci_resource_start(pcidev,
			p2p->p2p_bar_idx);
	p2p->p2p_bar_len = (ulong) pci_resource_len(pcidev, p2p->p2p_bar_idx);
	if (p2p->p2p_bar_len < XOCL_P2P_CHUNK_SIZE) {
		xocl_err(&pdev->dev, "Invalid p2p bar len %ld",
			p2p->p2p_bar_len);
		p2p->p2p_bar_idx = -1;
		ret = -EINVAL;
		goto failed;
	}

	bar_sz = xocl_fdt_get_p2pbar_len(xdev, XDEV(xdev)->fdt_blob);

	if (XDEV(xdev)->priv.p2p_bar_sz > 0) {
		p2p->p2p_max_mem_sz = XDEV(xdev)->priv.p2p_bar_sz;
		p2p->p2p_max_mem_sz <<= 30;
	} else if (bar_sz > 0)
		p2p->p2p_max_mem_sz = bar_sz;
	else {
		p2p->p2p_max_mem_sz = xocl_get_ddr_channel_size(xdev) *
		       	xocl_get_ddr_channel_count(xdev); /* GB */
		p2p->p2p_max_mem_sz <<= 30;
	}

	/* default: set conf to max_bar_sz */
	p2p->bank_conf[0].size = p2p->p2p_max_mem_sz;
	p2p->exp_mem_sz = p2p->p2p_max_mem_sz;

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
