/*
 * Copyright (C) 2016-2019 Xilinx, Inc. All rights reserved.
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

#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/aer.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/crc32c.h>
#include <linux/random.h>
#include "../xocl_drv.h"
#include "common.h"
#include "version.h"
#if defined(P2P_API_V1) || defined(P2P_API_V2)
#include <linux/memremap.h>
#endif

#ifndef PCI_EXT_CAP_ID_REBAR
#define PCI_EXT_CAP_ID_REBAR 0x15
#endif

#ifndef PCI_REBAR_CTRL
#define PCI_REBAR_CTRL          8       /* control register */
#endif

#ifndef PCI_REBAR_CTRL_BAR_SIZE
#define  PCI_REBAR_CTRL_BAR_SIZE        0x00001F00  /* BAR size */
#endif

#ifndef PCI_REBAR_CTRL_BAR_SHIFT
#define  PCI_REBAR_CTRL_BAR_SHIFT       8           /* shift for BAR size */
#endif

#define REBAR_FIRST_CAP		4

#define MAX_DYN_SUBDEV		1024
#define XDEV_DEFAULT_EXPIRE_SECS	1

static const struct pci_device_id pciidlist[] = {
	XOCL_USER_XDMA_PCI_IDS,
	{ 0, }
};

struct class *xrt_class;

MODULE_DEVICE_TABLE(pci, pciidlist);

static void xocl_mb_connect(struct xocl_dev *xdev);
static void xocl_mailbox_srv(void *arg, void *data, size_t len,
	u64 msgid, int err, bool sw_ch);

static void set_mig_cache_data(struct xocl_dev *xdev, struct xcl_mig_ecc *mig_ecc)
{
	int i, idx;
	uint32_t id;
	struct xcl_mig_ecc *cur;
	enum MEM_TYPE mem_type;
	uint64_t memidx;


	xocl_lock_xdev(xdev);
	for (i = 0; i < MAX_M_COUNT; ++i) {
		id = xocl_mig_get_id(xdev, i);
		if (!id)
			continue;

		mem_type = (id >> 16) & 0xFF;
		memidx = id & 0xFF;

		for (idx = 0; idx < MAX_M_COUNT; ++idx) {
			cur = &mig_ecc[idx];

			if (cur->mem_type != mem_type)
				continue;
			if (cur->mem_idx != memidx)
				continue;

			xocl_mig_set_data(xdev, i, &mig_ecc[idx]);
		}


	}
	xocl_unlock_xdev(xdev);

	xdev->mig_cache_expires = ktime_add(ktime_get_boottime(),
		ktime_set(xdev->mig_cache_expire_secs, 0));

}

static void xocl_mig_cache_read_from_peer(struct xocl_dev *xdev)
{
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	struct xcl_mig_ecc *mig_ecc = NULL;
	size_t resp_len = sizeof(struct xcl_mig_ecc)*MAX_M_COUNT;
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t reqlen = sizeof(struct xcl_mailbox_req) + data_len;
	int ret = 0;

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		goto done;

	mig_ecc = vzalloc(resp_len);
	if (!mig_ecc)
		goto done;

	mb_req->req = XCL_MAILBOX_REQ_PEER_DATA;
	subdev_peer.size = sizeof(struct xcl_mig_ecc);
	subdev_peer.kind = XCL_MIG_ECC;
	subdev_peer.entries = MAX_M_COUNT;

	memcpy(mb_req->data, &subdev_peer, data_len);

	ret = xocl_peer_request(xdev,
		mb_req, reqlen, mig_ecc, &resp_len, NULL, NULL, 0);

	if (!ret)
		set_mig_cache_data(xdev, mig_ecc);

done:
	vfree(mig_ecc);
	vfree(mb_req);
}

void xocl_update_mig_cache(struct xocl_dev *xdev)
{
	ktime_t now = ktime_get_boottime();

	mutex_lock(&xdev->dev_lock);

	if (ktime_compare(now, xdev->mig_cache_expires) > 0)
		xocl_mig_cache_read_from_peer(xdev);

	mutex_unlock(&xdev->dev_lock);
}

static void xocl_mb_read_p2p_addr(struct xocl_dev *xdev)
{
	struct pci_dev *pdev = xdev->core.pdev;
	struct xcl_mailbox_req *mb_req = NULL;
	struct xcl_mailbox_p2p_bar_addr *mb_p2p = NULL;
	size_t mb_p2p_len, reqlen;
	int ret = 0;
	size_t resplen = sizeof(ret);

	mb_p2p_len = sizeof(struct xcl_mailbox_p2p_bar_addr);
	reqlen = sizeof(struct xcl_mailbox_req) + mb_p2p_len;
	mb_req = vzalloc(reqlen);
	if (!mb_req) {
		userpf_err(xdev, "dropped request (%d), mem alloc issue\n",
			XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR);
		return;
	}

	mb_req->req = XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR;
	mb_p2p = (struct xcl_mailbox_p2p_bar_addr *)mb_req->data;
	mb_p2p->p2p_bar_len = pci_resource_len(pdev, xdev->p2p_bar_idx);
	mb_p2p->p2p_bar_addr = pci_resource_start(pdev, xdev->p2p_bar_idx);
	ret = xocl_peer_request(xdev, mb_req, reqlen, &ret,
		&resplen, NULL, NULL, 0);
	vfree(mb_req);
	if (ret) {
		userpf_info(xdev, "dropped request (%d), failed with err: %d",
			XCL_MAILBOX_REQ_READ_P2P_BAR_ADDR, ret);
	}
}

static int userpf_intr_config(xdev_handle_t xdev_hdl, u32 intr, bool en)
{
	return xocl_dma_intr_config(xdev_hdl, intr, en);
}

static int userpf_intr_register(xdev_handle_t xdev_hdl, u32 intr,
		irq_handler_t handler, void *arg)
{
	return handler ?
		xocl_dma_intr_register(xdev_hdl, intr, handler, arg, -1) :
		xocl_dma_intr_unreg(xdev_hdl, intr);
}

struct xocl_pci_funcs userpf_pci_ops = {
	.intr_config = userpf_intr_config,
	.intr_register = userpf_intr_register,
};

void xocl_reset_notify(struct pci_dev *pdev, bool prepare)
{
	struct xocl_dev *xdev = pci_get_drvdata(pdev);
	int ret;
	xuid_t *xclbin_id = NULL;

	xocl_info(&pdev->dev, "PCI reset NOTIFY, prepare %d", prepare);

	if (prepare) {
		/* clean up mem topology */
		xocl_cleanup_mem(XOCL_DRM(xdev));
		xocl_subdev_destroy_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);
		xocl_subdev_offline_all(xdev);
		ret = xocl_subdev_online_by_id(xdev, XOCL_SUBDEV_MAILBOX);
		if (ret)
			xocl_warn(&pdev->dev, "Online mailbox failed %d", ret);
		(void) xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);
		(void) xocl_mb_connect(xdev);
	} else {
		ret = xocl_subdev_offline_by_id(xdev, XOCL_SUBDEV_MAILBOX);
		if (ret)
			xocl_warn(&pdev->dev, "Offline mailbox failed %d", ret);
		ret = xocl_subdev_online_all(xdev);
		if (ret)
			xocl_warn(&pdev->dev, "Online subdevs failed %d", ret);
		(void) xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);

		ret = XOCL_GET_XCLBIN_ID(xdev, xclbin_id);
		if (ret) {
			xocl_warn(&pdev->dev, "Unable to get on device uuid %d", ret);
			return;
		}
		xocl_exec_reset(xdev, xclbin_id);
		XOCL_PUT_XCLBIN_ID(xdev);
	}
}

int xocl_program_shell(struct xocl_dev *xdev, bool force)
{
	int ret = 0, mbret = 0;
	struct xcl_mailbox_req mbreq = { 0 };
	size_t resplen = sizeof(ret);
	int i;

	mbreq.req = XCL_MAILBOX_REQ_PROGRAM_SHELL;
	mutex_lock(&xdev->dev_lock);
	if (!force && !list_is_singular(&xdev->ctx_list)) {
		/* We should have one context for ourselves. */
		BUG_ON(list_empty(&xdev->ctx_list));
		userpf_err(xdev, "device is in use, can't program");
		ret = -EBUSY;
	}
	mutex_unlock(&xdev->dev_lock);
	if (ret < 0)
		return ret;

	userpf_info(xdev, "program shell...");


	xocl_drvinst_set_offline(xdev->core.drm, true);
	if (force)
		xocl_drvinst_kill_proc(xdev->core.drm);

	if (XOCL_DRM(xdev))
		xocl_cleanup_mem(XOCL_DRM(xdev));

	ret = xocl_subdev_offline_all(xdev);
	if (ret) {
		userpf_err(xdev, "failed to offline subdevs %d", ret);
		goto failed;
	}

	for (i = XOCL_SUBDEV_LEVEL_MAX - 1; i > XOCL_SUBDEV_LEVEL_BLD; i--)
		xocl_subdev_destroy_by_level(xdev, i);

	ret = xocl_subdev_online_by_id(xdev, XOCL_SUBDEV_MAILBOX);
	if (ret) {
		userpf_err(xdev, "online mailbox failed %d", ret);
		goto failed;
	}
	ret = xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);
	if (ret)
		goto failed;

	userpf_info(xdev, "request mgmtpf to program prp");
	mbret = xocl_peer_request(xdev, &mbreq, sizeof(struct xcl_mailbox_req),
		&ret, &resplen, NULL, NULL, 0);
	if (mbret)
		ret = mbret;
	if (ret) {
		userpf_info(xdev, "request program prp failed %d, mret %d",
				ret, mbret);
		goto failed;
	}

	if (xdev->core.fdt_blob) {
		vfree(xdev->core.fdt_blob);
		xdev->core.fdt_blob = NULL;
	}

	xocl_mb_connect(xdev);
	(void) xocl_refresh_subdevs(xdev);

failed:
	return ret;
}

int xocl_hot_reset(struct xocl_dev *xdev, u32 flag)
{
	int ret = 0, mbret = 0;
	struct xcl_mailbox_req mbreq = { 0 };
	size_t resplen = sizeof(ret);

	mbreq.req = XCL_MAILBOX_REQ_HOT_RESET;
	mutex_lock(&xdev->dev_lock);
	if (!(flag & XOCL_RESET_FORCE) && !list_is_singular(&xdev->ctx_list)) {
		/* We should have one context for ourselves. */
		BUG_ON(list_empty(&xdev->ctx_list));
		userpf_err(xdev, "device is in use, can't reset");
		ret = -EBUSY;
	}
	mutex_unlock(&xdev->dev_lock);
	if (ret < 0)
		return ret;

	userpf_info(xdev, "resetting device...");

	if (flag & XOCL_RESET_FORCE)
		xocl_drvinst_kill_proc(xdev->core.drm);

	xocl_reset_notify(xdev->core.pdev, true);

	/*
	 * Reset mgmt. The reset will take 50 seconds on some platform.
	 * Set time out to 60 seconds.
	 */
	mbret = xocl_peer_request(xdev, &mbreq, sizeof(struct xcl_mailbox_req),
		&ret, &resplen, NULL, NULL, 60);
	if (mbret)
		ret = mbret;

#if defined(__PPC64__)
	/* During reset we can't poll mailbox registers to get notified when
	 * peer finishes reset. Just do a timer based wait for 20 seconds,
	 * which is long enough for reset to be done.
	 */
	msleep(20 * 1000);
#endif

	if (!(flag & XOCL_RESET_SHUTDOWN)) {
		(void) xocl_config_pci(xdev);
		(void) xocl_pci_resize_resource(xdev->core.pdev,
			xdev->p2p_bar_idx, xdev->p2p_bar_sz_cached);

		xocl_reset_notify(xdev->core.pdev, false);

		xocl_drvinst_set_offline(xdev->core.drm, false);
	}

	return ret;
}

/* pci driver callbacks */
static void xocl_work_cb(struct work_struct *work)
{
	struct xocl_work *_work = (struct xocl_work *)to_delayed_work(work);
	struct xocl_dev *xdev = container_of(_work,
			struct xocl_dev, works[_work->op]);

	if (XDEV(xdev)->shutdown) {
		xocl_xdev_info(xdev, "device is shutdown please hotplug");
		return;
	}

	switch (_work->op) {
	case XOCL_WORK_RESET:
		(void) xocl_hot_reset(xdev, XOCL_RESET_FORCE);
		break;
	case XOCL_WORK_SHUTDOWN:
		(void) xocl_hot_reset(xdev, XOCL_RESET_FORCE |
				XOCL_RESET_SHUTDOWN);
		/* mark device offline. Only hotplug is allowed. */
		XDEV(xdev)->shutdown = true;
		break;
	case XOCL_WORK_PROGRAM_SHELL:
		/* program shell */
		(void) xocl_program_shell(xdev, true);
		break;
	case XOCL_WORK_REFRESH_SUBDEV:
		(void) xocl_refresh_subdevs(xdev);
		break;
	default:
		xocl_xdev_err(xdev, "Invalid op code %d", _work->op);
		break;
	}
}

static void xocl_mb_connect(struct xocl_dev *xdev)
{
	struct xcl_mailbox_req *mb_req = NULL;
	struct xcl_mailbox_conn *mb_conn = NULL;
	struct xcl_mailbox_conn_resp *resp = (struct xcl_mailbox_conn_resp *)
		vzalloc(sizeof(struct xcl_mailbox_conn_resp));
	size_t data_len = 0;
	size_t reqlen = 0;
	size_t resplen = sizeof(struct xcl_mailbox_conn_resp);
	void *kaddr = NULL;
	int ret;

	if (!resp)
		goto done;

	data_len = sizeof(struct xcl_mailbox_conn);
	reqlen = sizeof(struct xcl_mailbox_req) + data_len;
	mb_req = vzalloc(reqlen);
	if (!mb_req)
		goto done;

	kaddr = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!kaddr)
		goto done;

	mb_req->req = XCL_MAILBOX_REQ_USER_PROBE;
	mb_conn = (struct xcl_mailbox_conn *)mb_req->data;
	mb_conn->kaddr = (uint64_t)kaddr;
	mb_conn->paddr = (uint64_t)virt_to_phys(kaddr);
	get_random_bytes(kaddr, PAGE_SIZE);
	mb_conn->crc32 = crc32c_le(~0, kaddr, PAGE_SIZE);
	mb_conn->version = XCL_MB_PROTOCOL_VER;

	ret = xocl_peer_request(xdev, mb_req, reqlen, resp, &resplen,
		NULL, NULL, 0);
	(void) xocl_mailbox_set(xdev, CHAN_STATE, resp->conn_flags);
	(void) xocl_mailbox_set(xdev, CHAN_SWITCH, resp->chan_switch);
	(void) xocl_mailbox_set(xdev, COMM_ID, (u64)(uintptr_t)resp->comm_id);

	userpf_info(xdev, "ch_state 0x%llx, ret %d\n", resp->conn_flags, ret);

done:
	if (!kaddr)
		kfree(kaddr);
	if (!mb_req)
		vfree(mb_req);
	if (!resp)
		vfree(resp);
}

int xocl_reclock(struct xocl_dev *xdev, void *data)
{
	int err = 0, i = 0;
	int msg = -ENODEV;
	struct xcl_mailbox_req *req = NULL;
	size_t resplen = sizeof(msg);
	size_t data_len = sizeof(struct xcl_mailbox_clock_freqscaling);
	size_t reqlen = sizeof(struct xcl_mailbox_req)+data_len;
	struct drm_xocl_reclock_info *freqs = (struct drm_xocl_reclock_info *)data;
	struct xcl_mailbox_clock_freqscaling mb_freqs = {0};

	mb_freqs.region = freqs->region;
	for (i = 0; i < 4; ++i)
		mb_freqs.target_freqs[i] = freqs->ocl_target_freq[i];

	req = kzalloc(reqlen, GFP_KERNEL);
	req->req = XCL_MAILBOX_REQ_RECLOCK;
	memcpy(req->data, data, data_len);

	if (get_live_clients(xdev, NULL)) {
		userpf_err(xdev, "device is in use, can't reset");
		err = -EBUSY;
	}

	mutex_lock(&xdev->dev_lock);

	if (err == 0) {
		err = xocl_peer_request(xdev, req, reqlen,
			&msg, &resplen, NULL, NULL, 0);
		if (err == 0)
			err = msg;
	}

	mutex_unlock(&xdev->dev_lock);

	/* Re-clock changes PR region, make sure next ERT configure cmd will
	 * go through
	 */
	if (err == 0)
		(void) xocl_exec_reconfig(xdev);

	kfree(req);
	return err;
}

static void xocl_mailbox_srv(void *arg, void *data, size_t len,
	u64 msgid, int err, bool sw_ch)
{
	struct xocl_dev *xdev = (struct xocl_dev *)arg;
	struct xcl_mailbox_req *req = (struct xcl_mailbox_req *)data;
	struct xcl_mailbox_peer_state *st = NULL;

	if (err != 0)
		return;

	userpf_info(xdev, "received request (%d) from peer\n", req->req);

	switch (req->req) {
	case XCL_MAILBOX_REQ_FIREWALL:
		userpf_info(xdev,
			"Card is in a BAD state, please issue xbutil reset");
		xocl_drvinst_set_offline(xdev->core.drm, true);
		/* Once firewall tripped, need to reset in secs */
		xocl_queue_work(xdev, XOCL_WORK_RESET, XOCL_RESET_DELAY);
		break;
	case XCL_MAILBOX_REQ_MGMT_STATE:
		st = (struct xcl_mailbox_peer_state *)req->data;
		if (st->state_flags & XCL_MB_STATE_ONLINE) {
			/* Mgmt is online, try to probe peer */
			userpf_info(xdev, "mgmt driver online\n");
			(void) xocl_mb_connect(xdev);
			xocl_queue_work(xdev, XOCL_WORK_REFRESH_SUBDEV, 1);

		} else if (st->state_flags & XCL_MB_STATE_OFFLINE) {
			/* Mgmt is offline, mark peer as not ready */
			userpf_info(xdev, "mgmt driver offline\n");
			(void) xocl_mailbox_set(xdev, CHAN_STATE, 0);
		} else {
			userpf_err(xdev, "unknown peer state flag (0x%llx)\n",
				st->state_flags);
		}
		break;
	case XCL_MAILBOX_REQ_CHG_SHELL:
		xocl_queue_work(xdev, XOCL_WORK_PROGRAM_SHELL,
				XOCL_PROGRAM_SHELL_DELAY);
		break;
	default:
		userpf_err(xdev, "dropped bad request (%d)\n", req->req);
		break;
	}
}

void get_pcie_link_info(struct xocl_dev *xdev,
	unsigned short *link_width, unsigned short *link_speed, bool is_cap)
{
	u16 stat;
	long result;
	int pos = is_cap ? PCI_EXP_LNKCAP : PCI_EXP_LNKSTA;

	result = pcie_capability_read_word(xdev->core.pdev, pos, &stat);
	if (result) {
		*link_width = *link_speed = 0;
		xocl_info(&xdev->core.pdev->dev, "Read pcie capability failed");
		return;
	}
	*link_width = (stat & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;
	*link_speed = stat & PCI_EXP_LNKSTA_CLS;
}

uint64_t xocl_get_data(struct xocl_dev *xdev, enum data_kind kind)
{
	uint64_t ret = 0;

	switch (kind) {
	case MIG_CALIB:
		ret = xocl_icap_get_data(xdev, MIG_CALIB);
		break;
	default:
		userpf_err(xdev, "dropped bad request (%d)\n", kind);
		break;
	}

	return ret;
}

int xocl_refresh_subdevs(struct xocl_dev *xdev)
{
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req	*mb_req = NULL;
	size_t reqlen = sizeof(struct xcl_mailbox_req) + data_len;
	struct xcl_subdev	*resp = NULL;
	size_t resp_len = sizeof(*resp) + XOCL_MSG_SUBDEV_DATA_LEN;
	char *blob = NULL, *tmp;
	u32 blob_len;
	uint64_t checksum;
	size_t offset = 0;
	int ret = 0;

	userpf_info(xdev, "get fdt from peer");
	mb_req = vzalloc(reqlen);
	if (!mb_req) {
		ret = -ENOMEM;
		goto failed;
	}

	resp = vzalloc(resp_len);
	if (!resp) {
		ret = -ENOMEM;
		goto failed;
	}

	mb_req->req = XCL_MAILBOX_REQ_PEER_DATA;

	subdev_peer.size = resp_len;
	subdev_peer.kind = XCL_SUBDEV;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	do {
		tmp = vzalloc(offset + resp_len);
		if (!tmp) {
			ret = -ENOMEM;
			goto failed;
		}

		if (blob) {
			memcpy(tmp, blob, offset);
			vfree(blob);
		}
		blob = tmp;
		blob_len = offset + resp_len;

		subdev_peer.offset = offset;
		ret = xocl_peer_request(xdev, mb_req, reqlen,
			resp, &resp_len, NULL, NULL, 0);
		if (ret)
			goto failed;

		if (!offset)
			checksum = resp->checksum;

		if (offset != resp->offset) {
			ret = -EINVAL;
			goto failed;
		}

		memcpy(blob + offset, resp->data, resp->size);
		offset += resp->size;
	} while (resp->rtncode == XOCL_MSG_SUBDEV_RTN_PARTIAL);

	if (resp->rtncode == XOCL_MSG_SUBDEV_RTN_UNCHANGED &&
			xdev->core.fdt_blob)
		goto failed;

	if (!offset && !xdev->core.fdt_blob)
		goto failed;

	if (xdev->core.fdt_blob) {
		vfree(xdev->core.fdt_blob);
		xdev->core.fdt_blob = NULL;
	}
	xdev->core.fdt_blob = blob;

	xocl_drvinst_set_offline(xdev->core.drm, true);
	if (blob) {
		ret = xocl_fdt_blob_input(xdev, blob, blob_len);
		if (ret) {
			userpf_err(xdev, "parse blob failed %d", ret);
			goto failed;
		}
		blob = NULL;
	}

	if (XOCL_DRM(xdev))
		xocl_cleanup_mem(XOCL_DRM(xdev));

	xocl_subdev_offline_all(xdev);
	xocl_subdev_destroy_all(xdev);
	ret = xocl_subdev_create_all(xdev);
	if (ret) {
		userpf_err(xdev, "create subdev failed %d", ret);
		goto failed;
	}
	(void) xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);
	(void) xocl_mb_connect(xdev);
	xocl_drvinst_set_offline(xdev->core.drm, false);

failed:
	if (blob)
		vfree(blob);
	if (mb_req)
		vfree(mb_req);
	if (resp)
		vfree(resp);

	return ret;
}

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

static void xocl_p2p_percpu_ref_release(struct percpu_ref *ref)
{
	struct xocl_p2p_mem_chunk *chk =
		container_of(ref, struct xocl_p2p_mem_chunk, xpmc_percpu_ref);
	complete(&chk->xpmc_comp);
}

static void xocl_p2p_percpu_ref_kill(void *data)
{
	struct percpu_ref *ref = data;
	percpu_ref_kill(ref);
}

static void xocl_p2p_percpu_ref_kill_noop(struct percpu_ref *ref)
{
	/* Used for pgmap, no op here */
}

static void xocl_p2p_percpu_ref_exit(void *data)
{
	struct percpu_ref *ref = data;
	struct xocl_p2p_mem_chunk *chk =
		container_of(ref, struct xocl_p2p_mem_chunk, xpmc_percpu_ref);

	wait_for_completion(&chk->xpmc_comp);
	percpu_ref_exit(ref);
}

static void xocl_p2p_mem_chunk_release(struct xocl_p2p_mem_chunk *chk)
{
	struct pci_dev *pdev = chk->xpmc_xdev->core.pdev;

	BUG_ON(!mutex_is_locked(&chk->xpmc_xdev->p2p_mem_chunk_lock));

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

	xocl_info(&pdev->dev,
		"released P2P mem chunk [0x%llx, 0x%llx), cur ref: %d",
		chk->xpmc_pa, chk->xpmc_pa + chk->xpmc_size, chk->xpmc_ref);
}

static int xocl_p2p_mem_chunk_reserve(struct xocl_p2p_mem_chunk *chk)
{
	struct pci_dev *pdev = chk->xpmc_xdev->core.pdev;
	struct device *dev = &pdev->dev;
	struct resource res;
	struct percpu_ref *pref = &chk->xpmc_percpu_ref;
	int ret = -ENOMEM;

	BUG_ON(!mutex_is_locked(&chk->xpmc_xdev->p2p_mem_chunk_lock));
	BUG_ON(chk->xpmc_ref < 0);

	if (chk->xpmc_ref > 0) {
		chk->xpmc_ref++;
		ret = 0;
		goto done;
	}

	if (percpu_ref_init(pref, xocl_p2p_percpu_ref_release, 0, GFP_KERNEL)) {
		xocl_err(dev, "init percpu ref failed");
		goto done;
	}

	BUG_ON(chk->xpmc_res_grp);
	chk->xpmc_res_grp = devres_open_group(dev, NULL, GFP_KERNEL);
	if (!chk->xpmc_res_grp) {
		percpu_ref_exit(pref);
		xocl_err(dev, "open p2p resource group failed");
		goto done;
	}

	res.start = chk->xpmc_pa;
	res.end   = res.start + chk->xpmc_size - 1;
	res.name  = NULL;
	res.flags = IORESOURCE_MEM;

	/* Suppressing the defined-but-not-used warning */
	{
		void *fn= NULL;
		ret = 0;
		fn = (void *)xocl_p2p_percpu_ref_exit;
		fn = (void *)xocl_p2p_percpu_ref_kill_noop;
		fn = (void *)xocl_p2p_percpu_ref_kill;
	}

#if	defined(P2P_API_V0)
	chk->xpmc_va = devm_memremap_pages(dev, &res);
#elif	defined(P2P_API_V1)
	ret = devm_add_action_or_reset(dev, xocl_p2p_percpu_ref_exit, pref);
	if (ret != 0) {
		xocl_err(dev, "add exit action failed");
		percpu_ref_exit(pref);
	} else {
		chk->xpmc_va = devm_memremap_pages(dev, &res,
			&chk->xpmc_percpu_ref, NULL);
		ret = devm_add_action_or_reset(dev,
			xocl_p2p_percpu_ref_kill, pref);
		if (ret != 0) {
			xocl_err(dev, "add kill action failed");
			percpu_ref_kill(pref);
		}
	}
#elif	defined(P2P_API_V2)
	ret = devm_add_action_or_reset(dev, xocl_p2p_percpu_ref_exit, pref);
	if (ret != 0) {
		xocl_err(dev, "add exit action failed");
		percpu_ref_exit(pref);
	} else {
		chk->xpmc_pgmap.ref = pref;
		chk->xpmc_pgmap.res = res;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
		chk->xpmc_pgmap.altmap_valid = false;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 2) && \
	LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
		chk->xpmc_pgmap.kill = xocl_p2p_percpu_ref_kill_noop;
#endif
		chk->xpmc_va = devm_memremap_pages(dev, &chk->xpmc_pgmap);
		ret = devm_add_action_or_reset(dev,
			xocl_p2p_percpu_ref_kill, pref);
		if (ret != 0) {
			xocl_err(dev, "add kill action failed");
			percpu_ref_kill(pref);
		}
	}
#endif

	devres_close_group(dev, chk->xpmc_res_grp);

	chk->xpmc_ref = 1;

	if (ret != 0 || IS_ERR_OR_NULL(chk->xpmc_va)) {
		if (IS_ERR(chk->xpmc_va)) {
			ret = PTR_ERR(chk->xpmc_va);
			chk->xpmc_va = NULL;
		}
		xocl_err(dev, "reserve p2p chunk failed, releasing");
		xocl_p2p_mem_chunk_release(chk);
		ret = ret ? ret : -ENOMEM;
	}

done:
	xocl_info(dev,
		"reserved P2P mem chunk [0x%llx, 0x%llx), ret: %d, cur ref: %d",
		chk->xpmc_pa, chk->xpmc_pa+chk->xpmc_size, ret, chk->xpmc_ref);
	return ret;
}

int xocl_p2p_reserve_release_range(struct xocl_dev *xdev,
	resource_size_t off, resource_size_t sz, bool reserve)
{
	int i, j;
	int ret = 0;
	int start_index = off / XOCL_P2P_CHUNK_SIZE;
	int num_chunks =
		ALIGN((off % XOCL_P2P_CHUNK_SIZE) + sz, XOCL_P2P_CHUNK_SIZE) /
		XOCL_P2P_CHUNK_SIZE;

	/* Make sure P2P is init'ed before we do anything. */
	if (xdev->p2p_mem_chunk_num == 0)
		return -EINVAL;

	mutex_lock(&xdev->p2p_mem_chunk_lock);
	for (i = start_index; i < start_index + num_chunks; i++) {
		struct xocl_p2p_mem_chunk *chk = &xdev->p2p_mem_chunks[i];

		ret = 0;
		if (reserve)
			ret = xocl_p2p_mem_chunk_reserve(chk);
		else
			xocl_p2p_mem_chunk_release(chk);

		if (ret)
			break;
	}

	/* Undo reserve, if failed. */
	if (ret) {
		for (j = start_index; j < i; j++)
			xocl_p2p_mem_chunk_release(&xdev->p2p_mem_chunks[j]);
	}
	mutex_unlock(&xdev->p2p_mem_chunk_lock);
	return ret;
}

void xocl_p2p_mem_chunk_init(struct xocl_dev *xdev,
	struct xocl_p2p_mem_chunk *chk, resource_size_t off, resource_size_t sz)
{
	chk->xpmc_xdev = xdev;
	chk->xpmc_pa = off;
	chk->xpmc_size = sz;
	init_completion(&chk->xpmc_comp);
}

void xocl_p2p_fini(struct xocl_dev *xdev, bool recov_bar_sz)
{
	struct pci_dev *pdev = xdev->core.pdev;
	int p2p_bar = -1;
	int i;

	mutex_destroy(&xdev->p2p_mem_chunk_lock);

	for (i = 0; i < xdev->p2p_mem_chunk_num; i++) {
		if (xdev->p2p_mem_chunks[i].xpmc_ref > 0) {
			xocl_err(&pdev->dev, "still %d ref for P2P chunk[%d]",
				xdev->p2p_mem_chunks[i].xpmc_ref, i);
			xdev->p2p_mem_chunks[i].xpmc_ref = 1;
			xocl_p2p_mem_chunk_release(&xdev->p2p_mem_chunks[i]);
		}
	}

	vfree(xdev->p2p_mem_chunks);
	xdev->p2p_mem_chunk_num = 0;
	xdev->p2p_mem_chunks = NULL;

	if (recov_bar_sz) {
		p2p_bar = xocl_get_p2p_bar(xdev, NULL);
		if (p2p_bar < 0)
			return;

		xocl_pci_resize_resource(pdev, p2p_bar,
			(XOCL_P2P_CHUNK_SHIFT - 20));

		xocl_info(&pdev->dev, "Resize p2p bar %d to %d M ", p2p_bar,
			(1 << XOCL_P2P_CHUNK_SHIFT));
	}

	//Reset Virtualization registers
	(void) xocl_mb_read_p2p_addr(xdev);
}

int xocl_p2p_init(struct xocl_dev *xdev)
{
	struct pci_dev *pdev = xdev->core.pdev;
	int i;
	resource_size_t pa;

	xocl_info(&pdev->dev, "Initializing P2P, bar %d, len %lld",
			xdev->p2p_bar_idx, xdev->p2p_bar_len);

	if (xdev->p2p_bar_idx < 0 ||
		xdev->p2p_bar_len <= XOCL_P2P_CHUNK_SIZE ||
		(xdev->p2p_bar_len % XOCL_P2P_CHUNK_SIZE)) {
		/* valid p2p_bar_len should be > P2P chunk size */
		xocl_info(&pdev->dev, "Did not find p2p BAR");
		return 0;
	}

	xdev->p2p_mem_chunk_num = xdev->p2p_bar_len / XOCL_P2P_CHUNK_SIZE;
	xdev->p2p_mem_chunks = vzalloc(sizeof(struct xocl_p2p_mem_chunk) *
		xdev->p2p_mem_chunk_num);
	if (xdev->p2p_mem_chunks == NULL)
		return -ENOMEM;

	pa = pci_resource_start(pdev, xdev->p2p_bar_idx);
	for (i = 0; i < xdev->p2p_mem_chunk_num; i++) {
		xocl_p2p_mem_chunk_init(xdev, &xdev->p2p_mem_chunks[i],
			pa + ((resource_size_t)i) * XOCL_P2P_CHUNK_SIZE,
			XOCL_P2P_CHUNK_SIZE);
	}

	mutex_init(&xdev->p2p_mem_chunk_lock);

	//Pass P2P bar address and len to mgmtpf
	(void) xocl_mb_read_p2p_addr(xdev);

	return 0;
}

int xocl_get_p2p_bar(struct xocl_dev *xdev, u64 *bar_size)
{
	struct pci_dev *dev = xdev->core.pdev;
	int i, pos;
	u32 cap, ctrl, size;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_REBAR);
	if (!pos) {
		xocl_info(&dev->dev, "rebar cap does not exist");
		return -ENOTSUPP;
	}

	pos += REBAR_FIRST_CAP;
	for (i = PCI_STD_RESOURCES; i <= PCI_STD_RESOURCE_END; i++) {
		pci_read_config_dword(dev, pos, &cap);
		pci_read_config_dword(dev, pos + 4, &ctrl);
		size = (ctrl & PCI_REBAR_CTRL_BAR_SIZE) >>
			PCI_REBAR_CTRL_BAR_SHIFT;
		if (xocl_pci_rebar_size_to_bytes(size) >= XOCL_P2P_CHUNK_SIZE &&
			cap >= 0x1000) {
			if (bar_size)
				*bar_size = xocl_pci_rebar_size_to_bytes(size);
			return i;
		}
		pos += 8;
	}

	if (bar_size)
		*bar_size = 0;

	return -1;
}

static int xocl_reassign_resources(struct pci_dev *dev, int resno)
{
	pci_assign_unassigned_bus_resources(dev->bus);

	return 0;
}

int xocl_pci_resize_resource(struct pci_dev *dev, int resno, int size)
{
	struct resource *res = dev->resource + resno;
	u64 bar_size, req_size;
	unsigned long flags;
	u16 cmd;
	int pos, ret = 0;
	u32 ctrl;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_REBAR);
	if (!pos) {
		xocl_info(&dev->dev, "rebar cap does not exist");
		return -ENOTSUPP;
	}

	pos += resno * PCI_REBAR_CTRL;
	pci_read_config_dword(dev, pos + PCI_REBAR_CTRL, &ctrl);

	bar_size = xocl_pci_rebar_size_to_bytes(
			(ctrl & PCI_REBAR_CTRL_BAR_SIZE) >>
			PCI_REBAR_CTRL_BAR_SHIFT);
	req_size = xocl_pci_rebar_size_to_bytes(size);

	xocl_info(&dev->dev, "req_size %lld, bar size %lld\n",
			req_size, bar_size);
	if (req_size == bar_size) {
		xocl_info(&dev->dev, "same size, return success");
		return -EALREADY;
	}

	pci_release_selected_regions(dev, (1 << resno));
	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	pci_write_config_word(dev, PCI_COMMAND,
		cmd & ~PCI_COMMAND_MEMORY);

	flags = res->flags;
	if (res->parent)
		release_resource(res);

	ctrl &= ~PCI_REBAR_CTRL_BAR_SIZE;
	ctrl |= size << PCI_REBAR_CTRL_BAR_SHIFT;
	pci_write_config_dword(dev, pos + PCI_REBAR_CTRL, ctrl);


	res->start = 0;
	res->end = req_size - 1;

	xocl_info(&dev->dev, "new size %lld", resource_size(res));
	xocl_reassign_resources(dev, resno);
	res->flags = flags;

	pci_write_config_word(dev, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);
	pci_request_selected_regions(dev, (1 << resno),
		XOCL_MODULE_NAME);

	return ret;
}

int xocl_pci_rbar_refresh(struct pci_dev *dev, int resno)
{
	u32 ctrl;
	u16 cmd;
	int pos;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_REBAR);
	if (!pos) {
		xocl_err(&dev->dev, "rebar cap does not exist");
		return -ENOTSUPP;
	}

	pos += resno * PCI_REBAR_CTRL;
	pci_read_config_dword(dev, pos + PCI_REBAR_CTRL, &ctrl);

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	pci_write_config_word(dev, PCI_COMMAND, cmd & ~PCI_COMMAND_MEMORY);

	pci_write_config_dword(dev, pos + PCI_REBAR_CTRL, ctrl);

	pci_write_config_word(dev, PCI_COMMAND, cmd | PCI_COMMAND_MEMORY);

	return 0;
}

static int identify_bar(struct xocl_dev *xdev)
{
	struct pci_dev *pdev = xdev->core.pdev;
	resource_size_t bar_len;
	int		i;

	xdev->p2p_bar_idx = -1;

	for (i = PCI_STD_RESOURCES; i <= PCI_STD_RESOURCE_END; i++) {
		bar_len = pci_resource_len(pdev, i);
		if (bar_len >= XOCL_P2P_CHUNK_SIZE) {
			xdev->p2p_bar_idx = i;
			xdev->p2p_bar_len = bar_len;
			pci_request_selected_regions(pdev, 1 << i,
				XOCL_MODULE_NAME);
		} else if (bar_len >= 32 * 1024 * 1024) {
			xdev->core.bar_addr = ioremap_nocache(
				pci_resource_start(pdev, i), bar_len);
			if (!xdev->core.bar_addr)
				return -EIO;
			xdev->core.bar_idx = i;
			xdev->core.bar_size = bar_len;
		}
	}

	return 0;
}

static void unmap_bar(struct xocl_dev *xdev)
{
	if (xdev->core.bar_addr) {
		iounmap(xdev->core.bar_addr);
		xdev->core.bar_addr = NULL;
	}

	if (xdev->p2p_bar_len)
		pci_release_selected_regions(xdev->core.pdev,
				1 << xdev->p2p_bar_idx);
}

void xocl_userpf_remove(struct pci_dev *pdev)
{
	struct xocl_dev		*xdev;

	xdev = pci_get_drvdata(pdev);
	if (!xdev) {
		xocl_warn(&pdev->dev, "driver data is NULL");
		return;
	}

	xocl_queue_destroy(xdev);

	xocl_p2p_fini(xdev, false);
	xocl_subdev_destroy_all(xdev);

	xocl_fini_sysfs(&pdev->dev);
	if (xdev->core.drm)
		xocl_drm_fini(xdev->core.drm);
	xocl_free_dev_minor(xdev);

	pci_disable_device(pdev);

	unmap_bar(xdev);

	xocl_subdev_fini(xdev);
	if (xdev->core.dyn_subdev_store)
		vfree(xdev->core.dyn_subdev_store);
	if (xdev->ulp_blob)
		vfree(xdev->ulp_blob);
	mutex_destroy(&xdev->core.lock);
	mutex_destroy(&xdev->dev_lock);
	mutex_destroy(&xdev->wq_lock);

	pci_set_drvdata(pdev, NULL);
	xocl_drvinst_free(xdev);
}

int xocl_config_pci(struct xocl_dev *xdev)
{
	struct pci_dev *pdev = xdev->core.pdev;
	int ret = 0;

	ret = pci_enable_device(pdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to enable device.");
		goto failed;
	}

failed:
	return ret;

}

int xocl_userpf_probe(struct pci_dev *pdev,
		const struct pci_device_id *ent)
{
	struct xocl_dev			*xdev;
	char				wq_name[15];
	int				ret, i;

	xdev = xocl_drvinst_alloc(&pdev->dev, sizeof(*xdev));
	if (!xdev) {
		xocl_err(&pdev->dev, "failed to alloc xocl_dev");
		return -ENOMEM;
	}

	/* this is used for all subdevs, bind it to device earlier */
	pci_set_drvdata(pdev, xdev);

	mutex_init(&xdev->core.lock);
	xdev->core.pci_ops = &userpf_pci_ops;
	xdev->core.pdev = pdev;
	xdev->core.dev_minor = XOCL_INVALID_MINOR;
	rwlock_init(&xdev->core.rwlock);
	xocl_fill_dsa_priv(xdev, (struct xocl_board_private *)ent->driver_data);
	mutex_init(&xdev->dev_lock);
	mutex_init(&xdev->wq_lock);
	atomic64_set(&xdev->total_execs, 0);
	atomic_set(&xdev->outstanding_execs, 0);
	INIT_LIST_HEAD(&xdev->ctx_list);

	for (i = XOCL_WORK_RESET; i < XOCL_WORK_NUM; i++) {
		INIT_DELAYED_WORK(&xdev->works[i].work, xocl_work_cb);
		xdev->works[i].op = i;
	}

	ret = xocl_subdev_init(xdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to failed to init subdev");
		goto failed;
	}

	ret = xocl_alloc_dev_minor(xdev);
	if (ret)
		goto failed;

	ret = identify_bar(xdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to identify bar");
		goto failed;
	}

	ret = xocl_config_pci(xdev);
	if (ret)
		goto failed;

	ret = xocl_subdev_create_all(xdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to register subdevs");
		goto failed;
	}

	ret = xocl_p2p_init(xdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to init p2p memory");
		goto failed;
	}

	snprintf(wq_name, sizeof(wq_name), "xocl_wq%d", xdev->core.dev_minor);
	xdev->wq = create_singlethread_workqueue(wq_name);
	if (!xdev->wq) {
		xocl_err(&pdev->dev, "failed to create work queue");
		ret = -EFAULT;
		goto failed;
	}

	/*
	 * NOTE: We'll expose ourselves through device node and sysfs from now
	 * on. Make sure we can handle incoming requests through them by now.
	 */
	xdev->core.drm = xocl_drm_init(xdev);
	if (!xdev->core.drm) {
		ret = -EFAULT;
		xocl_err(&pdev->dev, "failed to init drm mm");
		goto failed;
	}
	ret = xocl_init_sysfs(&pdev->dev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to init sysfs");
		goto failed;
	}

	/* Don't check mailbox on versal for now. */
	if (XOCL_DSA_IS_VERSAL(xdev))
		return 0;

	/* Launch the mailbox server. */
	ret = xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);
	if (ret) {
		xocl_err(&pdev->dev, "mailbox subdev is not created");
		goto failed;
	}
	/* Say hi to peer via mailbox. */
	(void) xocl_mb_connect(xdev);
	xocl_queue_work(xdev, XOCL_WORK_REFRESH_SUBDEV, 1);


	xdev->mig_cache_expire_secs = XDEV_DEFAULT_EXPIRE_SECS;

	return 0;

failed:
	xocl_userpf_remove(pdev);
	return ret;
}

static pci_ers_result_t user_pci_error_detected(struct pci_dev *pdev,
		pci_channel_state_t state)
{
	switch (state) {
	case pci_channel_io_normal:
		xocl_info(&pdev->dev, "PCI normal state error\n");
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		xocl_info(&pdev->dev, "PCI frozen state error\n");
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		xocl_info(&pdev->dev, "PCI failure state error\n");
		return PCI_ERS_RESULT_DISCONNECT;
	default:
		xocl_info(&pdev->dev, "PCI unknown state (%d) error\n", state);
		break;
	}

	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t user_pci_slot_reset(struct pci_dev *pdev)
{
	xocl_info(&pdev->dev, "PCI reset slot");
	pci_restore_state(pdev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void user_pci_error_resume(struct pci_dev *pdev)
{
	xocl_info(&pdev->dev, "PCI error resume");
	pci_cleanup_aer_uncorrect_error_status(pdev);
}

static const struct pci_error_handlers xocl_err_handler = {
	.error_detected	= user_pci_error_detected,
	.slot_reset	= user_pci_slot_reset,
	.resume		= user_pci_error_resume,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
	.reset_prepare	= user_pci_reset_prepare,
	.reset_done	= user_pci_reset_done,
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	.reset_notify   = xocl_reset_notify,
#endif
};

static struct pci_driver userpf_driver = {
	.name = XOCL_MODULE_NAME,
	.id_table = pciidlist,
	.probe = xocl_userpf_probe,
	.remove = xocl_userpf_remove,
	.err_handler = &xocl_err_handler,
};

/* INIT */
static int (*xocl_drv_reg_funcs[])(void) __initdata = {
	xocl_init_feature_rom,
	xocl_init_iores,
	xocl_init_xdma,
	xocl_init_qdma,
	xocl_init_mb_scheduler,
	xocl_init_mailbox,
	xocl_init_xmc,
	xocl_init_icap,
	xocl_init_clock,
	xocl_init_xvc,
	xocl_init_firewall,
	xocl_init_mig,
	xocl_init_dna,
	xocl_init_mailbox_versal,
};

static void (*xocl_drv_unreg_funcs[])(void) = {
	xocl_fini_feature_rom,
	xocl_fini_iores,
	xocl_fini_xdma,
	xocl_fini_qdma,
	xocl_fini_mb_scheduler,
	xocl_fini_mailbox,
	xocl_fini_xmc,
	xocl_fini_icap,
	xocl_fini_clock,
	xocl_fini_xvc,
	xocl_fini_firewall,
	xocl_fini_mig,
	xocl_fini_dna,
	xocl_fini_mailbox_versal,
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
		if (ret)
			goto failed;
	}

	ret = pci_register_driver(&userpf_driver);
	if (ret)
		goto failed;

	return 0;

failed:
	for (i--; i >= 0; i--)
		xocl_drv_unreg_funcs[i]();
	class_destroy(xrt_class);

err_class_create:
	return ret;
}

static void __exit xocl_exit(void)
{
	int i;

	pci_unregister_driver(&userpf_driver);

	for (i = ARRAY_SIZE(xocl_drv_unreg_funcs) - 1; i >= 0; i--)
		xocl_drv_unreg_funcs[i]();

	class_destroy(xrt_class);
}

module_init(xocl_init);
module_exit(xocl_exit);

MODULE_VERSION(XRT_DRIVER_VERSION);

MODULE_DESCRIPTION(XOCL_DRIVER_DESC);
MODULE_AUTHOR("Lizhi Hou <lizhi.hou@xilinx.com>");
MODULE_LICENSE("GPL v2");
