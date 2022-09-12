/*
 * Copyright (C) 2016-2022 Xilinx, Inc. All rights reserved.
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
#include "xocl_errors.h"
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

#define MAX_SB_APERTURES		256

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
static int identify_bar(struct xocl_dev *xdev);

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
		mb_req, reqlen, mig_ecc, &resp_len, NULL, NULL, 0, 0);

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

int xocl_register_cus(xdev_handle_t xdev_hdl, int slot_hdl, xuid_t *uuid,
		      struct ip_layout *ip_layout,
		      struct ps_kernel_node *ps_kernel)
{
	struct xocl_dev *xdev = container_of(XDEV(xdev_hdl), struct xocl_dev, core);

	return xocl_kds_register_cus(xdev, slot_hdl, uuid, ip_layout, ps_kernel);
}

int xocl_unregister_cus(xdev_handle_t xdev_hdl, int slot_hdl)
{
	struct xocl_dev *xdev = container_of(XDEV(xdev_hdl), struct xocl_dev, core);

	return xocl_kds_unregister_cus(xdev, slot_hdl);
}

static int userpf_intr_config(xdev_handle_t xdev_hdl, u32 intr, bool en)
{
	int ret;

	ret = xocl_dma_intr_config(xdev_hdl, intr, en);
	if (ret != -ENODEV)
		return ret;

	return xocl_msix_intr_config(xdev_hdl, intr, en);
}

static int userpf_intr_register(xdev_handle_t xdev_hdl, u32 intr,
		irq_handler_t handler, void *arg)
{
	int ret;

	ret = handler ?
		xocl_dma_intr_register(xdev_hdl, intr, handler, arg, -1) :
		xocl_dma_intr_unreg(xdev_hdl, intr);
	if (ret != -ENODEV)
		return ret;

	return handler ?
		xocl_msix_intr_register(xdev_hdl, intr, handler, arg, -1) :
		xocl_msix_intr_unreg(xdev_hdl, intr);
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
	mutex_lock(&xdev->core.errors_lock);
	xocl_clear_all_error_record(&xdev->core);
	mutex_unlock(&xdev->core.errors_lock);

	if (prepare) {
		xocl_kds_reset(xdev, xclbin_id);

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
		xocl_clear_pci_errors(xdev);

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

		if (XOCL_DSA_IS_VERSAL_ES3(xdev)) {
			ret = xocl_hwmon_sdm_init(xdev);
			if (ret) {
				userpf_err(xdev, "failed to init hwmon_sdm driver, err: %d", ret);
				return;
			}
		}

		xocl_kds_reset(xdev, xclbin_id);
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
		&ret, &resplen, NULL, NULL, 0, 0);
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

/*
 * Reset command should support following cases
 * case 1) When device is not in ready state
 *  - xbutil should not send any request to the xocl.
 *  - It should just return fail status from userspace itself
 * case 2) When device is ready & device offline status is true
 *  - Need to check when we hit this case
 * case 3) When device is ready & online
 *  a) If xocl unable to communicate to mgmt/mpd
 *     - xocl should reenable all the sub-devices and mark the device online/ready.
 *  b) If reset Channel is disabled
 *     - xocl should reenable all the sub-devices and mark the device online/ready.
 *  c) Reset is issued to mpd, but mpd doesn’t have serial number of requested device
 *     - MPD returns E_EMPTY serial number error code to xocl
 *     - xocl should reenable all the sub-devices and mark the device online/ready.
 *  d) Reset is issued to mgmt/mpd, but mgmt/mpd unable to reset properly
 *     - xocl gets a ESHUTDOWN response from mgmt/mpd,
 *     - xocl assumes that reset is successful,
 *     - xbutil waits on the device ready state in a loop.
 *     - xbutil reset would be in waiting state forever.
 *     - Need to handle this case to exit xbutil reset gracefully.
 *  e) Reset is issued to mgmt/mpd, but mgmt/mpd reset properly
 *     - xocl gets a ESHUTDOWN response from mgmt/mpd,
 *     - Device becomes ready and xbutil reset successful.
 */
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
		if (flag & XOCL_RESET_SHUTDOWN)
			xocl_reset_notify(xdev->core.pdev, true);

		if (flag & XOCL_RESET_NO)
			return 0;

		mbret = xocl_peer_request(xdev, &mbreq, sizeof(struct xcl_mailbox_req),
			&ret, &resplen, NULL, NULL, 0, 6);
		/*
		 * Check the return values mbret & ret (mpd (peer) side response) and confirm
		 * reset request success.
		 * MPD acknowledge the reset request with below responses, and it can be
		 * read from ret variable.
		 *  -E_EMPTY_SN (2040): indicates that MPD doesn't have serial number associated
		 *  with this device. So, aborting the reset request. This case hits
		 *  when vm boots & it is ready before the mgmt side is ready.
		 *  -ESHUTDOWN (108): indicates that MPD forwards reset requests to mgmt
		 *  successfully.
		 */
		if (mbret || (ret && ret != -ESHUTDOWN)) {
			userpf_err(xdev, "reset request failed, mbret: %d, peer resp: %d",
					   mbret, ret);
			xocl_reset_notify(xdev->core.pdev, false);
			xocl_drvinst_set_offline(xdev->core.drm, false);
		}
		/* userpf will back online till receiving mgmtpf notification */
		return 0;
	}

	mbret = xocl_peer_request(xdev, &mbreq, sizeof(struct xcl_mailbox_req),
		&ret, &resplen, NULL, NULL, 0, 0);

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

/*
 * On u30, there are 2 FPGAs, due to the issue of
 * https://jira.xilinx.com/browse/ALVEO-266
 * reset either FPGA will cause the other one being reset too.
 * A workaround is required to handle this case
 */
static int xocl_get_buddy_cb(struct device *dev, void *data)
{
	struct xocl_dev *src_xdev = *(struct xocl_dev **)(data);
	struct xocl_dev *tgt_xdev;

	/*
	 * skip
	 * 1.non xilinx device
	 * 2.itself
	 * 3.other devcies not being droven by same driver. using func id
	 * may not handle u25 where there is another device on same card 
	 */
	if (!src_xdev || !dev || to_pci_dev(dev)->vendor != 0x10ee ||
	   	XOCL_DEV_ID(to_pci_dev(dev)) ==
		XOCL_DEV_ID(src_xdev->core.pdev) || !dev->driver ||
		strcmp(dev->driver->name, "xocl")) 
		return 0;

	tgt_xdev = dev_get_drvdata(dev);
	if (tgt_xdev && strcmp(src_xdev->core.serial_num, "") &&
		strcmp(tgt_xdev->core.serial_num, "") &&
		!strcmp(src_xdev->core.serial_num, tgt_xdev->core.serial_num)) {
	       *(struct xocl_dev **)data = tgt_xdev;
		xocl_xdev_info(src_xdev, "2nd FPGA found on same card: %x:%x:%x",
			to_pci_dev(dev)->bus->number,
			PCI_SLOT(to_pci_dev(dev)->devfn),
			PCI_FUNC(to_pci_dev(dev)->devfn));
       		return 1;
	}
	return 0;
}

/*
 * mutex lock to prevent multile reset from happening simutaniously
 * this is necessary for case where there are multiple FPGAs on same
 * card, and reset one also triggers reset on others.
 * to simplify, just don't allow reset to any multiple FPGAs happen 
 */
static DEFINE_MUTEX(xocl_reset_mutex);

/* pci driver callbacks */
static void xocl_work_cb(struct work_struct *work)
{
	struct xocl_work *_work = (struct xocl_work *)to_delayed_work(work);
	struct xocl_dev *xdev = container_of(_work,
			struct xocl_dev, core.works[_work->op]);
	struct xocl_dev *buddy_xdev = xdev;

	if (XDEV(xdev)->shutdown && _work->op != XOCL_WORK_ONLINE) {
		xocl_xdev_info(xdev, "device is shutdown please hotplug");
		return;
	}

	switch (_work->op) {
	case XOCL_WORK_RESET:
		/*
		 * if 2nd FPGA is found, buddy_xdev is set as xdev of the other
		 * one, otherwise, it is set as null
		 */
		mutex_lock(&xocl_reset_mutex);
		if (!xocl_get_buddy_fpga(&buddy_xdev, xocl_get_buddy_cb))
			buddy_xdev = NULL;
		if (buddy_xdev) {
			xocl_drvinst_set_offline(buddy_xdev->core.drm, true);
			(void) xocl_hot_reset(buddy_xdev, XOCL_RESET_FORCE |
				XOCL_RESET_SHUTDOWN | XOCL_RESET_NO);
		}
		(void) xocl_hot_reset(xdev, XOCL_RESET_FORCE |
			       	XOCL_RESET_SHUTDOWN);
		mutex_unlock(&xocl_reset_mutex);
		break;
	case XOCL_WORK_SHUTDOWN_WITH_RESET:
		(void) xocl_hot_reset(xdev, XOCL_RESET_FORCE |
			XOCL_RESET_SHUTDOWN);
		/* mark device offline. Only hotplug is allowed. */
		XDEV(xdev)->shutdown = true;
		break;
	case XOCL_WORK_SHUTDOWN_WITHOUT_RESET:
		(void) xocl_hot_reset(xdev, XOCL_RESET_FORCE |
			XOCL_RESET_NO);
		/* Only kill applitions running on FPGA. */
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
		NULL, NULL, 0, 0);
	(void) xocl_mailbox_set(xdev, CHAN_STATE, resp->conn_flags);
	(void) xocl_mailbox_set(xdev, CHAN_SWITCH, resp->chan_switch);
	(void) xocl_mailbox_set(xdev, CHAN_DISABLE, resp->chan_disable);
	(void) xocl_mailbox_set(xdev, COMM_ID, (u64)(uintptr_t)resp->comm_id);

	/*
	 * we assume the FPGA is in good state and we can get & save S/N
	 * do it here in case we can't do it when we want to reset for u30
	 */
	xocl_xmc_get_serial_num(xdev);

	userpf_info(xdev, "ch_state 0x%llx, ret %d\n", resp->conn_flags, ret);

done:
	kfree(kaddr);
	vfree(mb_req);
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
			&msg, &resplen, NULL, NULL, 0, 0);
		if (err == 0)
			err = msg;
	}

	mutex_unlock(&xdev->dev_lock);

	/* Re-clock changes PR region, make sure next ERT configure cmd will
	 * go through
	 */
	if (err == 0)
		(void) xocl_kds_reconfig(xdev);

	kfree(req);
	return err;
}

static void xocl_mailbox_srv(void *arg, void *data, size_t len,
	u64 msgid, int err, bool sw_ch)
{
	struct xocl_dev *xdev = (struct xocl_dev *)arg;
	struct xcl_mailbox_req *req = (struct xcl_mailbox_req *)data;
	struct xcl_mailbox_peer_state *st = NULL;
	struct xclErrorLast err_last;
	/* Variables for firewall request processing */
	struct xcl_firewall fw_status = { 0 };

	if (err != 0)
		return;

	userpf_info(xdev, "received request (%d) from peer\n", req->req);
	switch (req->req) {
	case XCL_MAILBOX_REQ_FIREWALL:
		/* Update the xocl firewall status */
		xocl_af_check(xdev, NULL);
		/* Get the updated xocl firewall status */
		xocl_af_get_data(xdev, &fw_status);
		userpf_info(xdev, 
			"AXI Firewall %llu tripped", fw_status.err_detected_level);
		userpf_info(xdev,
			"Card is in a BAD state, please issue xbutil reset");
		err_last.pid = 0;
		err_last.ts = fw_status.err_detected_time;
		err_last.err_code = XRT_ERROR_CODE_BUILD(XRT_ERROR_NUM_FIRWWALL_TRIP, 
			XRT_ERROR_DRIVER_XOCL, XRT_ERROR_SEVERITY_CRITICAL, 
			XRT_ERROR_MODULE_FIREWALL, XRT_ERROR_CLASS_HARDWARE);
		xocl_insert_error_record(&xdev->core, &err_last);
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

void store_pcie_link_info(struct xocl_dev *xdev)
{
	u16 stat = 0;
	long result;
	int pos = PCI_EXP_LNKCAP;

	result = pcie_capability_read_word(xdev->core.pdev, pos, &stat);
	if (result) {
		xdev->pci_stat.link_width_max = xdev->pci_stat.link_speed_max = 0;
		userpf_err(xdev, "Read pcie capability failed for offset: 0x%x", pos);
	} else {
		xdev->pci_stat.link_width_max = (stat & PCI_EXP_LNKSTA_NLW) >>
			PCI_EXP_LNKSTA_NLW_SHIFT;
		xdev->pci_stat.link_speed_max = stat & PCI_EXP_LNKSTA_CLS;
	}

	stat = 0;
	pos = PCI_EXP_LNKSTA;
	result = pcie_capability_read_word(xdev->core.pdev, pos, &stat);
	if (result) {
		xdev->pci_stat.link_width = xdev->pci_stat.link_speed = 0;
		userpf_err(xdev, "Read pcie capability failed for offset: 0x%x", pos);
	} else {
		xdev->pci_stat.link_width = (stat & PCI_EXP_LNKSTA_NLW) >>
			PCI_EXP_LNKSTA_NLW_SHIFT;
		xdev->pci_stat.link_speed = stat & PCI_EXP_LNKSTA_CLS;
	}

	return;
}

void get_pcie_link_info(struct xocl_dev *xdev,
	unsigned short *link_width, unsigned short *link_speed, bool is_cap)
{
	int pos = is_cap ? PCI_EXP_LNKCAP : PCI_EXP_LNKSTA;

	if (pos == PCI_EXP_LNKCAP) {
		*link_width = xdev->pci_stat.link_width_max;
		*link_speed = xdev->pci_stat.link_speed_max;
	} else {
		*link_width = xdev->pci_stat.link_width;
		*link_speed = xdev->pci_stat.link_speed;
	}
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

	store_pcie_link_info(xdev);

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
			resp, &resp_len, NULL, NULL, 0, 0);
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

	ret = identify_bar(xdev);
	if (ret) {
		userpf_err(xdev, "failed to identify bar");
		goto failed;
	}

	ret = xocl_subdev_create_all(xdev);
	if (ret) {
		userpf_err(xdev, "create subdev failed %d", ret);
		goto failed;
	}

	ret = xocl_p2p_init(xdev);
	if (ret) {
		userpf_err(xdev, "failed to init p2p memory");
		goto failed;
	}

	if (XOCL_DSA_IS_VERSAL_ES3(xdev)) {
		//probe & initialize hwmon_sdm driver only on versal
		ret = xocl_hwmon_sdm_init(xdev);
		if (ret) {
			userpf_err(xdev, "failed to init hwmon_sdm driver, err: %d", ret);
			goto failed;
		}
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

int xocl_p2p_init(struct xocl_dev *xdev)
{
	struct xocl_subdev_info subdev_info = XOCL_DEVINFO_P2P;
	int ret;

	/* create p2p subdev for legacy platform */
	ret = xocl_subdev_create(xdev, &subdev_info);
	if (ret && ret != -EEXIST) {
		xocl_xdev_err(xdev, "create p2p subdev failed. ret %d", ret);
		return ret;
	}

	return 0;
}

static int xocl_hwmon_sdm_init_sysfs(struct xocl_dev *xdev, enum xcl_group_kind kind)
{
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	size_t resp_len = 4 * 1024;
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req *mb_req = NULL;
	char *in_buf = NULL;
	size_t reqlen = sizeof(struct xcl_mailbox_req) + data_len;
	int ret = 0;

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		goto done;

	in_buf = vzalloc(resp_len);
	if (!in_buf)
		goto done;

	mb_req->req = XCL_MAILBOX_REQ_SDR_DATA;
	mb_req->flags = 0x0;
	subdev_peer.size = resp_len;
	subdev_peer.kind = kind;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	ret = xocl_peer_request(xdev, mb_req, reqlen, in_buf, &resp_len, NULL, NULL, 0, 0);
	if (ret) {
		userpf_err(xdev, "sdr peer request failed, err: %d", ret);
		goto done;
	}

	// if the response has any error, mgmt sets the resp_len to size of int (error code).
	if (resp_len <= sizeof(int))
		goto done;

	ret = xocl_hwmon_sdm_create_sensors_sysfs(xdev, in_buf, resp_len, kind);
	if (ret)
		userpf_err(xdev, "hwmon_sdm sysfs creation failed for xcl_sdr 0x%x, err: %d", kind, ret);
	else
		userpf_dbg(xdev, "successfully created hwmon_sdm sensor sysfs node for xcl_sdr 0x%x", kind);

done:
	vfree(in_buf);
	vfree(mb_req);

	return ret;
}

int xocl_hwmon_sdm_init(struct xocl_dev *xdev)
{
	struct xocl_subdev_info subdev_info = XOCL_DEVINFO_HWMON_SDM;
	int ret;

	ret = xocl_subdev_create(xdev, &subdev_info);
	if (ret && ret != -EEXIST)
		return ret;

	(void) xocl_hwmon_sdm_init_sysfs(xdev, XCL_SDR_BDINFO);
	(void) xocl_hwmon_sdm_init_sysfs(xdev, XCL_SDR_TEMP);
	(void) xocl_hwmon_sdm_init_sysfs(xdev, XCL_SDR_CURRENT);
	(void) xocl_hwmon_sdm_init_sysfs(xdev, XCL_SDR_POWER);
	(void) xocl_hwmon_sdm_init_sysfs(xdev, XCL_SDR_VOLTAGE);

	return 0;
}

/*
 * Legacy platform uses bar_len to identify user bar.
 */
static int identify_bar_legacy(struct xocl_dev *xdev)
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

/*
 * For data driven platform, ep_mailbox_user_00 indicates user bar.
 * Remap the user bar based on bar id from device tree medadata (dts).
 */
static int identify_bar_by_dts(struct xocl_dev *xdev)
{
	struct pci_dev *pdev = xdev->core.pdev;
	int ret;
	int bar_id;
	resource_size_t bar_len;

	BUG_ON(!XOCL_DEV_HAS_DEVICE_TREE(xdev));

	ret = xocl_subdev_get_baridx(xdev, NODE_MAILBOX_USER, IORESOURCE_MEM,
		&bar_id);
	if (ret)
		return ret;

	bar_len = pci_resource_len(pdev, bar_id);

	xdev->core.bar_addr = ioremap_nocache(
		pci_resource_start(pdev, bar_id), bar_len);
	if (!xdev->core.bar_addr)
		return -EIO;

	xdev->core.bar_idx = bar_id;
	xdev->core.bar_size = bar_len;

	xocl_xdev_info(xdev, "user bar:%d size: %lld", bar_id, bar_len);
	return 0;
}

static void unmap_bar(struct xocl_dev *xdev)
{
	if (xdev->core.bar_addr) {
		iounmap(xdev->core.bar_addr);
		xdev->core.bar_addr = NULL;
	}
}

static int identify_bar(struct xocl_dev *xdev)
{
	unmap_bar(xdev);
	return XOCL_DEV_HAS_DEVICE_TREE(xdev) ?
		identify_bar_by_dts(xdev) :
		identify_bar_legacy(xdev);
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

	/* If fast adapter is present in the xclbin, new kds would
	 * hold a bo for reserve plram bank.
	 */
	xocl_fini_sched(xdev);

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

	xocl_fini_persist_sysfs(xdev);
	xocl_fini_sysfs(xdev);
	xocl_fini_errors(&xdev->core);

	xocl_subdev_destroy_all(xdev);

	xocl_free_dev_minor(xdev);

	pci_disable_device(pdev);

	unmap_bar(xdev);

	xocl_subdev_fini(xdev);
	if (xdev->ulp_blob)
		vfree(xdev->ulp_blob);
	mutex_destroy(&xdev->dev_lock);

	if (xdev->core.bars)
		kfree(xdev->core.bars);

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

	if (cma_mem->regular_page) {
		dma_unmap_page(&xdev->core.pdev->dev, cma_mem->paddr,
			cma_mem->size, DMA_BIDIRECTIONAL);
		__free_pages(cma_mem->regular_page, get_order(cma_mem->size));
		cma_mem->regular_page = NULL;
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
	uint32_t i, j, num = MAX_SB_APERTURES;
	uint64_t *user_addr = NULL, *phys_addrs = NULL, cma_mem_size = 0;
	uint64_t rounddown_num = rounddown_pow_of_two(cma_info->entry_num);

	BUG_ON(!mutex_is_locked(&xdev->dev_lock));

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
		goto fail;
	}

	for (i = 0; i < rounddown_num-1; ++i) {
		for (j = i+1; j < rounddown_num; ++j) {
			if (user_addr[i] == user_addr[j]) {
				ret = -EINVAL;
				DRM_ERROR("duplicated Huge Page");
				goto fail;
			}
		}
	}

	for (i = 0; i < rounddown_num; ++i) {
		if (user_addr[i] & (page_sz - 1)) {
			DRM_ERROR("Invalid Huge Page");
			ret = -EINVAL;
			goto fail;
		}

		ret = xocl_cma_mem_alloc_huge_page_by_idx(xdev, i, user_addr[i], page_sz);
		if (ret)
			goto fail;
	}

	phys_addrs = vzalloc(rounddown_num*sizeof(uint64_t));
	if (!phys_addrs) {
		ret = -ENOMEM;
		goto fail;
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
		goto fail;

	/* Remember how many cma mem we allocate*/
	xdev->cma_bank->entry_num = rounddown_num;
	xdev->cma_bank->entry_sz = page_sz;
	xdev->cma_bank->phys_addrs = phys_addrs;

	goto done;

fail:
	vfree(phys_addrs);
done:
	vfree(user_addr);
	return ret;
}

static struct page **xocl_phy_addr_get_pages(uint64_t paddr, int npages)
{
	struct page *p, **pages;
	int i;
	uint64_t offset = 0;

	pages = vzalloc(npages*sizeof(struct page *));
	if (pages == NULL)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < npages; i++) {
		p = pfn_to_page(PHYS_PFN(paddr + offset));
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
	struct device *dev = &xdev->core.pdev->dev;
	struct xocl_cma_memory *cma_mem = &xdev->cma_bank->cma_mem[idx];
	int order = get_order(size);
	struct page *page;
	dma_addr_t dma_addr;
	int node = dev_to_node(dev);

	page = alloc_pages_node(node, GFP_HIGHUSER, order);
	if (unlikely(!page))
		DRM_ERROR("Unable to alloc numa pages, %d", order);
	if (!page)
		page = alloc_pages(GFP_HIGHUSER, order);

	if (unlikely(!page)) {
		DRM_ERROR("Unable to alloc pages, %d", order);
		return -ENOMEM;
	}

	dma_addr = dma_map_page(dev, page, 0, size,
		DMA_BIDIRECTIONAL);
	if (unlikely(dma_mapping_error(dev, dma_addr))) {
		DRM_ERROR("Unable to dma map pages");
		__free_pages(page, order);
		return -EFAULT;
	}

	cma_mem->pages = xocl_phy_addr_get_pages(PFN_PHYS(page_to_pfn(page)),
		roundup(PAGE_SIZE, size) >> PAGE_SHIFT);

	if (!cma_mem->pages) {
		dma_unmap_page(dev, dma_addr, size, DMA_BIDIRECTIONAL);
		__free_pages(page, order);
		return -ENOMEM;
	}

	cma_mem->regular_page = page;
	cma_mem->paddr = dma_addr;
	cma_mem->size = size;

	return 0;
}

static void __xocl_cma_bank_free(struct xocl_dev *xdev)
{
	if (!xdev->cma_bank)
		return;

	xocl_cma_mem_free_all(xdev);
	xocl_addr_translator_clean(xdev);
	vfree(xdev->cma_bank->phys_addrs);
	vfree(xdev->cma_bank);
	xdev->cma_bank = NULL;
}

static int xocl_cma_mem_alloc(struct xocl_dev *xdev, uint64_t size)
{
	int ret = 0;
	uint64_t page_sz;
	int64_t i = 0;
	uint64_t page_num = MAX_SB_APERTURES;
	uint64_t *phys_addrs = NULL, cma_mem_size = 0;

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
			xdev->cma_bank->entry_num = i;
			goto fail;
		}
	}
	xdev->cma_bank->entry_num = page_num;

	phys_addrs = vzalloc(page_num*sizeof(uint64_t));
	if (!phys_addrs) {
		ret = -ENOMEM;
		goto fail;
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
		goto fail;

	xdev->cma_bank->entry_sz = page_sz;
	xdev->cma_bank->phys_addrs = phys_addrs;

	return 0;

fail:
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
	int num = MAX_SB_APERTURES;

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
			err = -ENOMEM;
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

	/* initialize xocl_errors */
	xocl_init_errors(&xdev->core);

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

	if (xocl_subdev_is_vsec_recovery(xdev)) {
		xocl_err(&pdev->dev, "recovery image, return");
		return 0;
	}

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

	/* Launch the mailbox server. */
	ret = xocl_peer_listen(xdev, xocl_mailbox_srv, (void *)xdev);
	if (ret) {
		xocl_err(&pdev->dev, "mailbox subdev is not created");
		goto failed;
	}

	xocl_queue_work(xdev, XOCL_WORK_REFRESH_SUBDEV, 1);
	/* Waiting for all subdev to be initialized before returning. */
	flush_delayed_work(&xdev->core.works[XOCL_WORK_REFRESH_SUBDEV].work);

	xdev->mig_cache_expire_secs = XDEV_DEFAULT_EXPIRE_SECS;

	/* store link width & speed stats */
	store_pcie_link_info(xdev);

	/*
	 * sysfs has to be the last thing to init because xbutil
	 * relies it to report if the card is ready. Driver should
	 * only announce ready after syncing metadata and creating
	 * all subdevices
	 */
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

	xocl_drvinst_set_offline(xdev, false);

	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(64))) {
		/* query for DMA transfer */
		/* @see Documentation/DMA-mapping.txt */
		xocl_info(&pdev->dev, "pci_set_dma_mask()\n");
		/* use 64-bit DMA */
		xocl_info(&pdev->dev, "Using a 64-bit DMA mask.\n");
		/* use 32-bit DMA for descriptors */
		dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
		/* use 64-bit DMA, 32-bit for consistent */
	} else if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(32))) {
		xocl_info(&pdev->dev, "Could not set 64-bit DMA mask.\n");
		dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
		/* use 32-bit DMA */
		xocl_info(&pdev->dev, "Using a 32-bit DMA mask.\n");
	} else {
		xocl_err(&pdev->dev, "No suitable DMA possible.\n");
		return -EINVAL;
	}

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
	xocl_init_version_control,
	xocl_init_iores,
	xocl_init_xdma,
	xocl_init_qdma,
	xocl_init_mailbox,
	xocl_init_xmc,
	xocl_init_xmc_u2,
	xocl_init_icap,
	xocl_init_clock_wiz,
	xocl_init_clock_counter,
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
	xocl_init_accel_deadlock_detector,
	xocl_init_mem_hbm,
	/* Initial intc sub-device before CU/ERT sub-devices */
	xocl_init_intc,
	xocl_init_cu,
	xocl_init_scu,
	xocl_init_addr_translator,
	xocl_init_p2p,
	xocl_init_spc,
	xocl_init_lapc,
	xocl_init_msix_xdma,
	xocl_init_ert_user,
	xocl_init_m2m,
	xocl_init_config_gpio,
	xocl_init_command_queue,
	xocl_init_hwmon_sdm,
	xocl_init_ert_ctrl,
};

static void (*xocl_drv_unreg_funcs[])(void) = {
	xocl_fini_feature_rom,
	xocl_fini_version_control,
	xocl_fini_iores,
	xocl_fini_xdma,
	xocl_fini_qdma,
	xocl_fini_mailbox,
	xocl_fini_xmc,
	xocl_fini_xmc_u2,
	xocl_fini_icap,
	xocl_fini_clock_wiz,
	xocl_fini_clock_counter,
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
	xocl_fini_accel_deadlock_detector,
	xocl_fini_mem_hbm,
	xocl_fini_cu,
	xocl_fini_scu,
	xocl_fini_addr_translator,
	xocl_fini_p2p,
	xocl_fini_spc,
	xocl_fini_lapc,
	xocl_fini_msix_xdma,
	xocl_fini_ert_user,
	xocl_fini_m2m,
	/* Remove intc sub-device after CU/ERT sub-devices */
	xocl_fini_intc,
	xocl_fini_config_gpio,
	xocl_fini_command_queue,
	xocl_fini_hwmon_sdm,
	xocl_fini_ert_ctrl,
};

static int __init xocl_init(void)
{
	int		ret, i = 0;

	xrt_class = class_create(THIS_MODULE, "xrt_user");
	if (IS_ERR(xrt_class)) {
		ret = PTR_ERR(xrt_class);
		goto err_class_create;
	}

	ret = xocl_debug_init();
	if (ret) {
		pr_err("failed to init debug");
		goto failed;
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

	xocl_debug_fini();

	class_destroy(xrt_class);
}

module_init(xocl_init);
module_exit(xocl_exit);

MODULE_VERSION(XRT_DRIVER_VERSION);

MODULE_DESCRIPTION(XOCL_DRIVER_DESC);
MODULE_AUTHOR("Lizhi Hou <lizhi.hou@xilinx.com>");
MODULE_LICENSE("GPL v2");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,16,0)
MODULE_IMPORT_NS(DMA_BUF);
#endif
