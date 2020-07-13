/*
 * Copyright (C) 2016-2020 Xilinx, Inc. All rights reserved.
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
#include <linux/iommu.h>
#include <linux/pagemap.h>
#include "../xocl_drv.h"
#include "common.h"
#include "version.h"

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

extern int kds_mode;

static const struct pci_device_id pciidlist[] = {
	XOCL_USER_XDMA_PCI_IDS,
	{ 0, }
};

struct class *xrt_class;

MODULE_DEVICE_TABLE(pci, pciidlist);

#if defined(__PPC64__)
int xrt_reset_syncup = 1;
#else
int xrt_reset_syncup;
#endif
module_param(xrt_reset_syncup, int, (S_IRUGO|S_IWUSR));
MODULE_PARM_DESC(xrt_reset_syncup,
	"Enable config space syncup for pci hot reset");


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
		if (xdev->core.drm) {
			xocl_drm_fini(xdev->core.drm);
			xdev->core.drm = NULL;
		}
		xocl_fini_sysfs(xdev);
		xocl_subdev_destroy_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);
		xocl_subdev_offline_all(xdev);
		if (!xrt_reset_syncup)
			xocl_subdev_online_by_id(xdev, XOCL_SUBDEV_MAILBOX);
	} else {
		(void) xocl_config_pci(xdev);

		if (!xrt_reset_syncup)
			xocl_subdev_offline_by_id(xdev, XOCL_SUBDEV_MAILBOX);

		ret = xocl_subdev_online_all(xdev);
		if (ret)
			xocl_warn(&pdev->dev, "Online subdevs failed %d", ret);
		(void) xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);

		ret = XOCL_GET_XCLBIN_ID(xdev, xclbin_id);
		if (ret) {
			xocl_warn(&pdev->dev, "Unable to get on device uuid %d", ret);
			return;
		}

		ret = xocl_init_sysfs(xdev);
		if (ret) {
			xocl_warn(&pdev->dev, "Unable to create sysfs %d", ret);
			return;
		}

		if (kds_mode)
			xocl_kds_reset(xdev, xclbin_id);
		else
			xocl_exec_reset(xdev, xclbin_id);
		XOCL_PUT_XCLBIN_ID(xdev);
		if (!xdev->core.drm) {
			xdev->core.drm = xocl_drm_init(xdev);
			if (!xdev->core.drm) {
				xocl_warn(&pdev->dev, "Unable to init drm");
				return;
			}
		}
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

	/* free cma bank*/
	mutex_lock(&xdev->dev_lock);
	xocl_cma_bank_free(xdev);
	mutex_unlock(&xdev->dev_lock);

	/* cleanup drm */
	if (xdev->core.drm) {
		xocl_drm_fini(xdev->core.drm);
		xdev->core.drm = NULL;
	}

	xocl_fini_sysfs(xdev);

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

failed:
	return ret;
}

int xocl_hot_reset(struct xocl_dev *xdev, u32 flag)
{
	int ret = 0, mbret = 0;
	struct xcl_mailbox_req mbreq = { 0 };
	size_t resplen = sizeof(ret);
	u16 pci_cmd;
	struct pci_dev *pdev = XDEV(xdev)->pdev;

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

	/* On powerpc, it does not have secondary level bus reset.
	 * Instead, it uses fundemantal reset which does not allow mailbox polling
	 * xrt_reset_syncup might have to be true on power pc.
	 */

	if (!xrt_reset_syncup) {
		xocl_reset_notify(xdev->core.pdev, true);

		xocl_peer_request(xdev, &mbreq, sizeof(struct xcl_mailbox_req),
			&ret, &resplen, NULL, NULL, 0);
		/* userpf will back online till receiving mgmtpf notification */

		return 0;
	}

	mbret = xocl_peer_request(xdev, &mbreq, sizeof(struct xcl_mailbox_req),
		&ret, &resplen, NULL, NULL, 0);

	xocl_reset_notify(xdev->core.pdev, true);

	/*
	 * return value indicates how mgmtpf side handles hot reset request
	 * 0 indicates response from XRT mgmtpf driver, which supports
	 *   COMMAND_MASTER POLLing
	 *
	 * Usually, non-zero return values indicates MSD on the other side.
	 * EOPNOTSUPP: Polling COMMAND_MASTER is not supported, reset is done
	 * ESHUTDOWN: Polling COMMAND_MASTER is not supported,
	 * device is shutdown.
	 */
	if (!mbret && ret == -ESHUTDOWN)
		flag |= XOCL_RESET_SHUTDOWN;
	if (mbret) {
		userpf_err(xdev, "Requested peer failed %d", mbret);
		ret = mbret;
		goto failed_notify;
	}
	if (ret) {
		userpf_err(xdev, "Hotreset peer response %d", ret);
		goto failed_notify;
	}

	userpf_info(xdev, "Set master off then wait it on");
	pci_read_config_word(pdev, PCI_COMMAND, &pci_cmd);
	pci_cmd &= ~PCI_COMMAND_MASTER;
	pci_write_config_word(pdev, PCI_COMMAND, pci_cmd);
	/*
	 * Wait mgmtpf driver complete reset and set master.
	 * The reset will take 50 seconds on some platform.
	 * Set time out to 60 seconds.
	 */
	ret = xocl_wait_pci_status(XDEV(xdev)->pdev, PCI_COMMAND_MASTER,
			PCI_COMMAND_MASTER, 60);
	if (ret) {
		flag |= XOCL_RESET_SHUTDOWN;
		goto failed_notify;
	}

	pci_read_config_word(pdev, PCI_COMMAND, &pci_cmd);
	pci_cmd |= PCI_COMMAND_MASTER;
	pci_write_config_word(pdev, PCI_COMMAND, pci_cmd);

failed_notify:

	if (!(flag & XOCL_RESET_SHUTDOWN)) {
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
			struct xocl_dev, core.works[_work->op]);

	if (XDEV(xdev)->shutdown && _work->op != XOCL_WORK_ONLINE) {
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
	case XOCL_WORK_ONLINE:
		xocl_reset_notify(xdev->core.pdev, false);
		xocl_drvinst_set_offline(xdev->core.drm, false);
		XDEV(xdev)->shutdown = false;
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

	/*
	 * We should proactively check if the request is validate prior to send
	 * request via mailbox. When icap refactor work done, we should have
	 * dedicated module to parse xclbin and keep info. For example: the
	 * dedicated mouldes can be icap for ultrascale(+) board, or ospi for
	 * versal ACAP board.
	 */
	err = xocl_icap_xclbin_validate_clock_req(xdev, freqs);
	if (err)
		return err;

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
	if (err == 0) {
		if (kds_mode)
			(void) xocl_kds_reconfig(xdev);
		else
			(void) xocl_exec_reconfig(xdev);
	}

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
	bool offline = false;
	int ret = 0;

	ret = xocl_drvinst_get_offline(xdev->core.drm, &offline);
	if (ret == -ENODEV || offline) {
		userpf_info(xdev, "online current devices");
	        xocl_reset_notify(xdev->core.pdev, false);
		xocl_drvinst_set_offline(xdev->core.drm, false);
	}

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

	if (resp->rtncode == XOCL_MSG_SUBDEV_RTN_PENDINGPLP) {
		(void) xocl_program_shell(xdev, true);
		ret = -EAGAIN;
		goto failed;
	} else if (resp->rtncode == XOCL_MSG_SUBDEV_RTN_UNCHANGED &&
			xdev->core.fdt_blob)
		goto failed;

	if (!offset && !xdev->core.fdt_blob)
		goto failed;

	if (resp->rtncode != XOCL_MSG_SUBDEV_RTN_COMPLETE &&
		resp->rtncode != XOCL_MSG_SUBDEV_RTN_UNCHANGED) {
		userpf_err(xdev, "Unexpected return code %d", resp->rtncode);
		ret = -EINVAL;
		goto failed;
	}

	if (xdev->core.fdt_blob) {
		vfree(xdev->core.fdt_blob);
		xdev->core.fdt_blob = NULL;
	}
	xdev->core.fdt_blob = blob;

	xocl_drvinst_set_offline(xdev->core.drm, true);
	if (blob) {
		ret = xocl_fdt_blob_input(xdev, blob, blob_len, -1, NULL);
		if (ret) {
			userpf_err(xdev, "parse blob failed %d", ret);
			goto failed;
		}
		blob = NULL;
	}

	/* clean up mem topology */
	if (xdev->core.drm) {
		xocl_drm_fini(xdev->core.drm);
		xdev->core.drm = NULL;
	}
	xocl_fini_sysfs(xdev);

	xocl_subdev_offline_all(xdev);
	xocl_subdev_destroy_all(xdev);
	ret = xocl_subdev_create_all(xdev);
	if (ret) {
		userpf_err(xdev, "create subdev failed %d", ret);
		goto failed;
	}
	(void) xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);

	ret = xocl_init_sysfs(xdev);
	if (ret) {
		userpf_err(xdev, "Unable to create sysfs %d", ret);
		goto failed;
	}

	if (!xdev->core.drm) {
		xdev->core.drm = xocl_drm_init(xdev);
		if (!xdev->core.drm) {
			userpf_err(xdev, "Unable to init drm");
			goto failed;
		}
	}

	xocl_drvinst_set_offline(xdev->core.drm, false);

failed:
	if (!ret)
		(void) xocl_mb_connect(xdev);
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

void xocl_p2p_fini(struct xocl_dev *xdev)
{
	if (xocl_subdev_is_vsec(xdev))
		return;

	xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_P2P);
}

int xocl_p2p_init(struct xocl_dev *xdev)
{
	struct xocl_subdev_info subdev_info = XOCL_DEVINFO_P2P;
	int ret;

	if (xocl_subdev_is_vsec(xdev))
		return 0;

	/* create p2p subdev for legacy platform */
	ret = xocl_subdev_create(xdev, &subdev_info);
	if (ret) {
		xocl_xdev_info(xdev, "create p2p subdev failed. ret %d", ret);
		return ret;
	}

	return 0;
}

static int identify_bar(struct xocl_dev *xdev)
{
	struct pci_dev *pdev = xdev->core.pdev;
	resource_size_t bar_len;
	int		i;

	for (i = PCI_STD_RESOURCES; i <= PCI_STD_RESOURCE_END; i++) {
		bar_len = pci_resource_len(pdev, i);
		if (bar_len >= 32 * 1024 * 1024 &&
			bar_len < XOCL_P2P_CHUNK_SIZE) {
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
}

void xocl_userpf_remove(struct pci_dev *pdev)
{
	struct xocl_dev		*xdev;
	void *hdl;

	xdev = pci_get_drvdata(pdev);
	if (!xdev) {
		xocl_warn(&pdev->dev, "driver data is NULL");
		return;
	}

	xocl_drvinst_release(xdev, &hdl);

	xocl_queue_destroy(xdev);
	
	/* Free pinned pages before call xocl_drm_fini */
	xocl_cma_bank_free(xdev);

	/*
	 * need to shutdown drm and sysfs before destroy subdevices
	 * drm and sysfs could access subdevices
	 */
	if (xdev->core.drm) {
		xocl_drm_fini(xdev->core.drm);
		xdev->core.drm = NULL;
	}

	xocl_p2p_fini(xdev);
	xocl_fini_persist_sysfs(xdev);
	xocl_fini_sysfs(xdev);

	xocl_subdev_destroy_all(xdev);

	xocl_free_dev_minor(xdev);

	pci_disable_device(pdev);

	unmap_bar(xdev);

	xocl_fini_sched(xdev);

	xocl_subdev_fini(xdev);
	if (xdev->ulp_blob)
		vfree(xdev->ulp_blob);
	mutex_destroy(&xdev->dev_lock);

	pci_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);
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
static void xocl_cma_mem_free(struct xocl_dev *xdev, uint32_t idx)
{
	struct xocl_cma_memory *cma_mem = &xdev->cma_bank->cma_mem[idx];
	struct sg_table *sgt = NULL;

	if (!cma_mem)
		return;

	sgt = cma_mem->sgt;
	if (sgt) {
		dma_unmap_sg(&xdev->core.pdev->dev, sgt->sgl, sgt->orig_nents, DMA_BIDIRECTIONAL);
		sg_free_table(sgt);
		vfree(sgt);
		cma_mem->sgt = NULL;
	}

	if (cma_mem->vaddr) {
		dma_free_coherent(&xdev->core.pdev->dev, cma_mem->size, cma_mem->vaddr, cma_mem->paddr);
		cma_mem->vaddr = NULL;
	} else if (cma_mem->pages) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 15, 0)
		release_pages(cma_mem->pages, cma_mem->size >> PAGE_SHIFT);
#else
		release_pages(cma_mem->pages, cma_mem->size >> PAGE_SHIFT, 0);
#endif
	}

	if (cma_mem->pages) {
		vfree(cma_mem->pages);
		cma_mem->pages = NULL;
	}
}

static void xocl_cma_mem_free_all(struct xocl_dev *xdev)
{
	int i = 0;
	uint64_t num = 0;

	if (!xdev->cma_bank)
		return;

	num = xdev->cma_bank->entry_num;

	for (i = 0; i < num; ++i)
		xocl_cma_mem_free(xdev, i);

	xocl_info(&xdev->core.pdev->dev, "%s done", __func__);
}

static int xocl_cma_mem_alloc_huge_page_by_idx(struct xocl_dev *xdev, uint32_t idx, uint64_t user_addr, uint64_t page_sz)
{
	uint64_t page_count = 0, nr = 0;
	struct device *dev = &xdev->core.pdev->dev;
	int ret = 0;
	struct xocl_cma_memory *cma_mem = &xdev->cma_bank->cma_mem[idx];
	struct sg_table *sgt = NULL;

	if (!(XOCL_ACCESS_OK(VERIFY_WRITE, user_addr, page_sz))) {
		xocl_err(dev, "Invalid huge page user pointer\n");
		ret = -ENOMEM;
		goto done;
	}

	page_count = (page_sz) >> PAGE_SHIFT;
	cma_mem->pages = vzalloc(page_count*sizeof(struct page *));
	if (!cma_mem->pages) {
		ret = -ENOMEM;
		goto done;
	}

	nr = get_user_pages_fast(user_addr, page_count, 1, cma_mem->pages);
	if (nr != page_count) {
		xocl_err(dev, "Can't pin down enough page_nr %llx\n", nr);
		ret = -EINVAL;
		goto done;
	}

	sgt = vzalloc(sizeof(struct sg_table));
	if (!sgt) {
		ret = -ENOMEM;
		goto done;
	}

	ret = sg_alloc_table_from_pages(sgt, cma_mem->pages, page_count, 0, page_sz, GFP_KERNEL);
	if (ret) {
		ret = -ENOMEM;
		goto done;
	}

	if (sgt->orig_nents != 1) {
		xocl_err(dev, "Host mem is not physically contiguous\n");
		ret = -EINVAL;
		goto done;
	}

	if (!dma_map_sg(dev, sgt->sgl, sgt->orig_nents, DMA_BIDIRECTIONAL)) {
		ret =-ENOMEM;
		goto done;
	}

	if (sgt->orig_nents != sgt->nents) {
		ret =-ENOMEM;
		goto done;		
	}

	cma_mem->size = page_sz;
	cma_mem->paddr = sg_dma_address(sgt->sgl);
	cma_mem->sgt = sgt;

done:
	if (ret) {
		vfree(cma_mem->pages);
		cma_mem->pages = NULL;
		if (sgt) {
			dma_unmap_sg(dev, sgt->sgl, sgt->orig_nents, DMA_BIDIRECTIONAL);
			sg_free_table(sgt);
			vfree(sgt);
		}
	}

	return ret;
}

static int xocl_cma_mem_alloc_huge_page(struct xocl_dev *xdev, struct drm_xocl_alloc_cma_info *cma_info)
{
	int ret = 0;
	size_t page_sz = cma_info->total_size/cma_info->entry_num;
	uint32_t i, j, num = xocl_addr_translator_get_entries_num(xdev);
	uint64_t *user_addr = NULL, *phys_addrs = NULL, cma_mem_size = 0;
	uint64_t rounddown_num = rounddown_pow_of_two(cma_info->entry_num);

	BUG_ON(!mutex_is_locked(&xdev->dev_lock));

	if (!num)
		return -ENODEV;
	/* Limited by hardware, the entry number can only be power of 2
	 * rounddown_pow_of_two 255=>>128 63=>>32
	 */
	if (rounddown_num != cma_info->entry_num) {
		DRM_ERROR("Request %lld, round down to power of 2 %lld\n", 
				cma_info->entry_num, rounddown_num);
		return -EINVAL;
	}

	if (rounddown_num > num)
		return -EINVAL;

	user_addr = vzalloc(sizeof(uint64_t)*rounddown_num);
	if (!user_addr)
		return -ENOMEM;

	ret = copy_from_user(user_addr, cma_info->user_addr, sizeof(uint64_t)*rounddown_num);
	if (ret) {
		ret = -EFAULT;
		goto done;
	}

	for (i = 0; i < rounddown_num-1; ++i) {
		for (j = i+1; j < rounddown_num; ++j) {
			if (user_addr[i] == user_addr[j]) {
				ret = -EINVAL;
				DRM_ERROR("duplicated Huge Page");
				goto done;
			}
		}
	}

	for (i = 0; i < rounddown_num; ++i) {
		if (user_addr[i] & (page_sz - 1)) {
			DRM_ERROR("Invalid Huge Page");
			ret = -EINVAL;
			goto done;
		}

		ret = xocl_cma_mem_alloc_huge_page_by_idx(xdev, i, user_addr[i], page_sz);
		if (ret)
			goto done;
	}

	phys_addrs = vzalloc(rounddown_num*sizeof(uint64_t));
	if (!phys_addrs) {
		ret = -ENOMEM;
		goto done;		
	}

	for (i = 0; i < rounddown_num; ++i) {
		struct xocl_cma_memory *cma_mem = &xdev->cma_bank->cma_mem[i];

		if (!cma_mem) {
			ret = -ENOMEM;
			break;
		}

		/* All the cma mem should have the same size,
		 * find the black sheep
		 */
		if (cma_mem_size && cma_mem_size != cma_mem->size) {
			DRM_ERROR("CMA memory mixmatch");
			ret = -EINVAL;
			break;
		}

		phys_addrs[i] = cma_mem->paddr;
		cma_mem_size = cma_mem->size;
	}

	if (ret)
		goto done;

	/* Remember how many cma mem we allocate*/
	xdev->cma_bank->entry_num = rounddown_num;
	xdev->cma_bank->entry_sz = page_sz;

	ret = xocl_addr_translator_set_page_table(xdev, phys_addrs, page_sz, rounddown_num);
done:
	vfree(user_addr);
	vfree(phys_addrs);
	return ret;
}

static struct page **xocl_virt_addr_get_pages(void *vaddr, int npages)

{
	struct page *p, **pages;
	int i;
	uint64_t offset = 0;

	pages = vzalloc(npages*sizeof(struct page *));
	if (pages == NULL)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < npages; i++) {
		p = virt_to_page(vaddr + offset);
		pages[i] = p;
		if (IS_ERR(p))
			goto fail;
		offset += PAGE_SIZE;
	}

	return pages;
fail:
	vfree(pages);
	return ERR_CAST(p);
}

static int xocl_cma_mem_alloc_by_idx(struct xocl_dev *xdev, uint64_t size, uint32_t idx)
{
	int ret = 0;
	uint64_t page_count;
	struct xocl_cma_memory *cma_mem = &xdev->cma_bank->cma_mem[idx];
	struct page **pages = NULL;
	dma_addr_t dma_addr;

	page_count = (size) >> PAGE_SHIFT;

	cma_mem->vaddr = dma_alloc_coherent(&xdev->core.pdev->dev, size, &dma_addr, GFP_KERNEL);

	if (!cma_mem->vaddr) {
		DRM_ERROR("Unable to alloc %llx bytes CMA buffer", size);
		ret = -ENOMEM;
		goto done;
	}

	pages = xocl_virt_addr_get_pages(cma_mem->vaddr, size >> PAGE_SHIFT);
	if (IS_ERR(pages)) {
		ret = PTR_ERR(pages);
		goto done;
	}

	cma_mem->pages = pages;
	cma_mem->paddr = dma_addr;
	cma_mem->size = size;

done:
	if (ret)
		xocl_cma_mem_free(xdev, idx);

	return ret;
}

static void __xocl_cma_bank_free(struct xocl_dev *xdev)
{
	if (!xdev->cma_bank)
		return;

	xocl_cma_mem_free_all(xdev);
	xocl_addr_translator_clean(xdev);
	vfree(xdev->cma_bank);
	xdev->cma_bank = NULL;
}

static int xocl_cma_mem_alloc(struct xocl_dev *xdev, uint64_t size)
{
	int ret = 0;
	uint64_t i = 0, page_sz;
	uint64_t page_num = xocl_addr_translator_get_entries_num(xdev);
	uint64_t *phys_addrs = NULL, cma_mem_size = 0;

	if (!page_num) {
		DRM_ERROR("Doesn't support CMA BANK feature");
		return -ENODEV;		
	}

	page_sz = size/page_num;

	if (page_sz < PAGE_SIZE || !is_power_of_2(page_sz)) {
		DRM_ERROR("Invalid CMA bank size");
		return -EINVAL;
	}

	if (page_sz > (PAGE_SIZE << (MAX_ORDER-1))) {
		DRM_WARN("Unable to allocate with page size 0x%llx", page_sz);
		return -EINVAL;
	}

	for (; i < page_num; ++i) {
		ret = xocl_cma_mem_alloc_by_idx(xdev, page_sz, i);
		if (ret) {
			ret = -ENOMEM;
			goto done;
		}
	}

	phys_addrs = vzalloc(page_num*sizeof(uint64_t));
	if (!phys_addrs) {
		ret = -ENOMEM;
		goto done;		
	}

	for (i = 0; i < page_num; ++i) {
		struct xocl_cma_memory *cma_mem = &xdev->cma_bank->cma_mem[i];

		if (!cma_mem) {
			ret = -ENOMEM;
			break;
		}

		/* All the cma mem should have the same size,
		 * find the black sheep
		 */
		if (cma_mem_size && cma_mem_size != cma_mem->size) {
			DRM_ERROR("CMA memory mixmatch");
			ret = -EINVAL;
			break;
		}

		phys_addrs[i] = cma_mem->paddr;
		cma_mem_size = cma_mem->size;
	}

	if (ret)
		goto done;

	xdev->cma_bank->entry_num = page_num;
	xdev->cma_bank->entry_sz = page_sz;

	ret = xocl_addr_translator_set_page_table(xdev, phys_addrs, page_sz, page_num);
done:	
	vfree(phys_addrs);
	return ret;
}

void xocl_cma_bank_free(struct xocl_dev	*xdev)
{
	__xocl_cma_bank_free(xdev);
	if (xdev->core.drm)
		xocl_cleanup_mem(xdev->core.drm);
	xocl_icap_clean_bitstream(xdev);
}

int xocl_cma_bank_alloc(struct xocl_dev	*xdev, struct drm_xocl_alloc_cma_info *cma_info)
{
	int err = 0;
	int num = xocl_addr_translator_get_entries_num(xdev);

	if (!num) {
		DRM_ERROR("Doesn't support HOST MEM feature");
		return -ENODEV;
	}

	xocl_cleanup_mem(xdev->core.drm);
	xocl_icap_clean_bitstream(xdev);

	if (xdev->cma_bank) {
		uint64_t allocated_size = xdev->cma_bank->entry_num * xdev->cma_bank->entry_sz;
		if (allocated_size == cma_info->total_size) {
			DRM_INFO("HOST MEM already allocated, skip");
			goto unlock;
		} else {
			DRM_ERROR("HOST MEM already allocated, size 0x%llx", allocated_size);
			DRM_ERROR("Please run xbutil host disable first");
			err = -EBUSY;
			goto unlock;
		}
	}

	xdev->cma_bank = vzalloc(sizeof(struct xocl_cma_bank)+num*sizeof(struct xocl_cma_memory));
	if (!xdev->cma_bank) {
		err = -ENOMEM;
		goto done;
	}

	if (cma_info->entry_num)
		err = xocl_cma_mem_alloc_huge_page(xdev, cma_info);
	else {
		/* Cast all err as E2BIG */
		err = xocl_cma_mem_alloc(xdev, cma_info->total_size);
		if (err) {
			err = -E2BIG;
			goto done;
		}
	}
done:
	if (err)
		__xocl_cma_bank_free(xdev);
unlock:
	DRM_INFO("%s, %d", __func__, err);
	return err;
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

	mutex_init(&xdev->dev_lock);
	atomic64_set(&xdev->total_execs, 0);
	atomic_set(&xdev->outstanding_execs, 0);
	INIT_LIST_HEAD(&xdev->ctx_list);


	ret = xocl_subdev_init(xdev, pdev, &userpf_pci_ops);
	if (ret) {
		xocl_err(&pdev->dev, "failed to failed to init subdev");
		goto failed;
	}

	(void) xocl_init_sched(xdev);

	ret = xocl_config_pci(xdev);
	if (ret)
		goto failed;

	xocl_fill_dsa_priv(xdev, (struct xocl_board_private *)ent->driver_data);

	for (i = XOCL_WORK_RESET; i < XOCL_WORK_NUM; i++) {
		INIT_DELAYED_WORK(&xdev->core.works[i].work, xocl_work_cb);
		xdev->core.works[i].op = i;
	}

	ret = xocl_alloc_dev_minor(xdev);
	if (ret)
		goto failed;

	ret = identify_bar(xdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to identify bar");
		goto failed;
	}

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
	xdev->core.wq = create_singlethread_workqueue(wq_name);
	if (!xdev->core.wq) {
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
	ret = xocl_init_sysfs(xdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to init sysfs");
		goto failed;
	}
	ret = xocl_init_persist_sysfs(xdev);
	if (ret) {
		xocl_err(&pdev->dev, "failed to init persist sysfs");
		goto failed;
	}

	/* Launch the mailbox server. */
	ret = xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);
	if (ret) {
		xocl_err(&pdev->dev, "mailbox subdev is not created");
		goto failed;
	}

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
	xocl_init_qdma4,
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
	xocl_init_aim,
	xocl_init_am,
	xocl_init_asm,
	xocl_init_trace_fifo_lite,
	xocl_init_trace_fifo_full,
	xocl_init_trace_funnel,
	xocl_init_trace_s2mm,
	xocl_init_mem_hbm,
	xocl_init_cu,
	xocl_init_addr_translator,
	xocl_init_p2p,
	xocl_init_spc,
	xocl_init_lapc,
};

static void (*xocl_drv_unreg_funcs[])(void) = {
	xocl_fini_feature_rom,
	xocl_fini_iores,
	xocl_fini_xdma,
	xocl_fini_qdma,
	xocl_fini_qdma4,
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
	xocl_fini_aim,
	xocl_fini_am,
	xocl_fini_asm,
	xocl_fini_trace_fifo_lite,
	xocl_fini_trace_fifo_full,
	xocl_fini_trace_funnel,
	xocl_fini_trace_s2mm,
	xocl_fini_mem_hbm,
	xocl_fini_cu,
	xocl_fini_addr_translator,
	xocl_fini_p2p,
	xocl_fini_spc,
	xocl_fini_lapc,
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
