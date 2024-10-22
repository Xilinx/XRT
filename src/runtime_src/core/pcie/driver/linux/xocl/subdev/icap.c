/**
 *  Copyright (C) 2017-2022 Xilinx, Inc. All rights reserved.
 *  Copyright (C) 2022 Advanced Micro Devices, Inc.
 *  Author: Sonal Santan
 *  Code copied verbatim from SDAccel xcldma kernel mode driver
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/uuid.h>
#include <linux/pid.h>
#include <linux/key.h>
#include <linux/efi.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
#include <linux/verification.h>
#endif
#include "xclbin.h"
#include "xrt_xclbin.h"
#include "../xocl_drv.h"
#include "../xocl_drm.h"
#include "mgmt-ioctl.h"
#include "ps_kernel.h"

#if defined(XOCL_UUID)
static xuid_t uuid_null = NULL_UUID_LE;
#endif

static struct key *icap_keys = NULL;

#define	ICAP_ERR(icap, fmt, arg...)	\
	xocl_err(&(icap)->icap_pdev->dev, fmt "\n", ##arg)
#define	ICAP_WARN(icap, fmt, arg...)	\
	xocl_warn(&(icap)->icap_pdev->dev, fmt "\n", ##arg)
#define	ICAP_INFO(icap, fmt, arg...)	\
	xocl_info(&(icap)->icap_pdev->dev, fmt "\n", ##arg)
#define	ICAP_DBG(icap, fmt, arg...)	\
	xocl_dbg(&(icap)->icap_pdev->dev, fmt "\n", ##arg)

#define	ICAP_PRIVILEGED(icap)	((icap)->icap_regs != NULL)

/*
 * Only DDR, PLRAM, and HBM memory banks require MIG calibration
 */
#define MEM_NEEDS_CALIBRATION(m_tag) \
	((convert_mem_tag(m_tag) == MEM_TAG_DDR) || (convert_mem_tag(m_tag) == MEM_TAG_PLRAM) || (convert_mem_tag(m_tag) == MEM_TAG_HBM))

/*
 * Block comment for spliting old icap into subdevs (icap, clock, xclbin, etc.)
 *
 * Current design: all-in-one icap
 * Future design: multiple subdevs with their own territory
 *
 * Phase1 design:
 *    - The clock subdev would only handle clock specific logic.
 *    - Before we are able to take xclbin subdev out of icap, we can keep
 *    icap+xclbin together and only isolate clock as subdev. Therefore, the
 *    clock subdev will be a mgmt subdev only. xclbin related feature, like
 *    topology will remain in icap; icap caching data, like user pf cached data
 *    will remain in icap, all sysfs info are unchanged.
 *    Callers still call APIs through icap in phase1, eventually those APIs
 *    will be moved to xclbin subdev, and icap will redirect requests to clock
 *    subdev.
 *
 * Phase2 design:
 *    - The clock is already a stand alone subdev on mgmt pf only.
 *    - The xclbin is a library and cache data in pdev. Legacy icap interfaces
 *      are relocated from icap to xclbin library. Since xclbin is splitted
 *      from icap, icap subdev can be offline/online without lossing loaded
 *      xclbin info.
 */

/*
 * Note: there are 2 max num clocks, ICAP_MAX_... and CLOCK_MAX_...,
 * those should be concept from XCLBIN_MAX_... in the future.
 */
#define	ICAP_MAX_NUM_CLOCKS		4
#define ICAP_DEFAULT_EXPIRE_SECS	1
#define MAX_SLOT_SUPPORT		128

#define INVALID_MEM_IDX			0xFFFF

#define ICAP_SET_RESET			0x1
#define ICAP_CLEAR_RESET		0x0

static struct attribute_group icap_attr_group;

enum icap_sec_level {
	ICAP_SEC_NONE = 0,
	ICAP_SEC_DEDICATE,
	ICAP_SEC_SYSTEM,
	ICAP_SEC_MAX = ICAP_SEC_SYSTEM,
};

/*
 * AXI-HWICAP IP register layout
 */
struct icap_reg {
	u32			ir_rsvd1[7];
	u32			ir_gier;
	u32			ir_isr;
	u32			ir_rsvd2;
	u32			ir_ier;
	u32			ir_rsvd3[53];
	u32			ir_wf;
	u32			ir_rf;
	u32			ir_sz;
	u32			ir_cr;
	u32			ir_sr;
	u32			ir_wfv;
	u32			ir_rfo;
	u32			ir_asr;
} __attribute__((packed));

struct icap_generic_state {
	u32			igs_state;
} __attribute__((packed));

struct icap_config_engine {
	u32			ice_reg;
} __attribute__((packed));

struct icap_bitstream_user {
	struct list_head	ibu_list;
	pid_t			ibu_pid;
};

struct islot_info {
	uint32_t		slot_idx;
	bool 			pl_slot;

	struct clock_freq_topology *xclbin_clock_freq_topology;
	unsigned long		xclbin_clock_freq_topology_length;
	struct mem_topology	*mem_topo;
	struct mem_topology	*group_topo;
	struct ip_layout	*ip_layout;
	struct debug_ip_layout	*debug_layout;
	struct ps_kernel_node	*ps_kernel;
	struct connectivity	*connectivity;
	struct connectivity	*group_connectivity;
	uint64_t		max_host_mem_aperture;
	void			*partition_metadata;

	xuid_t			icap_bitstream_uuid;
	int			icap_bitstream_ref;

	/* Use reader_ref as xclbin metadata reader counter
	 * Ther reference count increases by 1
	 * if icap_xclbin_rd_lock get called.
	 */
	u64			busy;
	int			reader_ref;
	wait_queue_head_t	reader_wq;
};

struct icap {
	struct platform_device	*icap_pdev;
	struct mutex		icap_lock;
	struct icap_reg		*icap_regs;
	struct icap_generic_state *icap_state;
	struct icap_config_engine *icap_config_engine;
	unsigned int		idcode;
	bool			icap_axi_gate_frozen;

	void			*rp_bit;
	size_t			rp_bit_len;
	void			*rp_fdt;
	size_t			rp_fdt_len;
	void			*rp_mgmt_bin;
	size_t			rp_mgmt_bin_len;
	void			*rp_sche_bin;
	size_t			rp_sche_bin_len;
	void			*rp_sc_bin;
	size_t			*rp_sc_bin_len;
	char			rp_vbnv[128];

	struct bmc		bmc_header;

	uint64_t		cache_expire_secs;
	struct xcl_pr_region	cache;
	ktime_t			cache_expires;

	enum icap_sec_level	sec_level;
	uint32_t		data_retention;

	/* xclbin specific informations */
	struct islot_info	*slot_info[MAX_SLOT_SUPPORT];
};

static inline u32 reg_rd(void __iomem *reg)
{
	if (!reg)
		return -1;

	return XOCL_READ_REG32(reg);
}

static inline void reg_wr(void __iomem *reg, u32 val)
{
	if (!reg)
		return;

	iowrite32(val, reg);
}

static int icap_cache_bitstream_axlf_section(struct platform_device *pdev,
	const struct axlf *xclbin, enum axlf_section_kind kind, uint32_t slot_id);
static void icap_set_data(struct icap *icap, struct xcl_pr_region *hwicap);
static uint64_t icap_get_data_nolock(struct platform_device *pdev, enum data_kind kind);
static uint64_t icap_get_data(struct platform_device *pdev, enum data_kind kind);
static void icap_refresh_addrs(struct platform_device *pdev);
static int icap_calib_and_check(struct platform_device *pdev, uint32_t slot_id);
static void icap_probe_urpdev(struct platform_device *pdev, struct axlf *xclbin,
	int *num_urpdev, struct xocl_subdev **urpdevs, uint32_t slot_id);

static int icap_slot_init(struct icap *icap, uint32_t slot_id)
{
	struct islot_info *islot = icap->slot_info[slot_id];
	
	mutex_lock(&icap->icap_lock);
	if (islot) {
		vfree(islot);
		icap->slot_info[slot_id] = NULL;
	}

	islot = vzalloc(sizeof(struct islot_info));
	if (!islot) {
		mutex_unlock(&icap->icap_lock);
		ICAP_ERR(icap, "Memory allocation failure");
		return -ENOMEM;
	}
	
	init_waitqueue_head(&islot->reader_wq);
	islot->slot_idx = slot_id;
	islot->pl_slot = false;
	icap->slot_info[slot_id] = islot;
	mutex_unlock(&icap->icap_lock);

	return 0;
}

static int icap_xclbin_wr_lock(struct icap *icap, uint32_t slot_id)
{
	struct islot_info *islot = NULL;
	pid_t pid = pid_nr(task_tgid(current));
	int ret = 0;

	mutex_lock(&icap->icap_lock);
	islot = icap->slot_info[slot_id];
	if (islot->busy) {
		ret = -EBUSY;
	} else {
		islot->busy = (u64)pid;
	}
	mutex_unlock(&icap->icap_lock);

	if (ret)
		goto done;

	ret = wait_event_interruptible(islot->reader_wq,
				       islot->reader_ref == 0);

	if (ret)
		goto done;

	BUG_ON(islot->reader_ref != 0);

done:
	ICAP_DBG(icap, "%d ret: %d", pid, ret);
	return ret;
}

static void icap_xclbin_wr_unlock(struct icap *icap, uint32_t slot_id)
{
	struct islot_info *islot = NULL;
	pid_t pid = pid_nr(task_tgid(current));

	mutex_lock(&icap->icap_lock);
	islot = icap->slot_info[slot_id];
	BUG_ON(islot->busy != (u64)pid);

	islot->busy = 0;
	mutex_unlock(&icap->icap_lock);
	ICAP_DBG(icap, "%d", pid);
}

static int icap_xclbin_rd_lock(struct icap *icap, uint32_t slot_id)
{
	struct islot_info *islot = NULL;
	pid_t pid = pid_nr(task_tgid(current));
	int ret = 0;

	mutex_lock(&icap->icap_lock);

	islot = icap->slot_info[slot_id];
	if (islot->busy) {
		ret = -EBUSY;
		goto done;
	}

	islot->reader_ref++;
done:
	mutex_unlock(&icap->icap_lock);
	ICAP_DBG(icap, "%d ret: %d", pid, ret);
	return ret;
}

static  void icap_xclbin_rd_unlock(struct icap *icap, uint32_t slot_id)
{
	struct islot_info *islot = NULL;
	pid_t pid = pid_nr(task_tgid(current));
	bool wake = false;

	mutex_lock(&icap->icap_lock);

	islot = icap->slot_info[slot_id];
	BUG_ON(islot->reader_ref == 0);

	ICAP_DBG(icap, "%d", pid);

	wake = (--islot->reader_ref == 0);

	mutex_unlock(&icap->icap_lock);
	if (wake)
		wake_up_interruptible(&islot->reader_wq);
}


static void icap_free_bins(struct icap *icap)
{
	if (icap->rp_bit) {
		vfree(icap->rp_bit);
		icap->rp_bit = NULL;
		icap->rp_bit_len = 0;
	}
	if (icap->rp_fdt) {
		vfree(icap->rp_fdt);
		icap->rp_fdt = NULL;
		icap->rp_fdt_len = 0;
	}
	if (icap->rp_mgmt_bin) {
		vfree(icap->rp_mgmt_bin);
		icap->rp_mgmt_bin = NULL;
		icap->rp_mgmt_bin_len = 0;
	}
	if (icap->rp_sche_bin) {
		vfree(icap->rp_sche_bin);
		icap->rp_sche_bin = NULL;
		icap->rp_sche_bin_len = 0;
	}
}

static uint32_t icap_multislot_version_from_peer(struct platform_device *pdev)
{
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	struct icap *icap = platform_get_drvdata(pdev);
	struct xcl_multislot_info xcl_multislot = {0};
	size_t resp_len = sizeof(struct xcl_multislot_info);
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t reqlen = struct_size(mb_req, data, 1) + data_len;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	ICAP_INFO(icap, "reading from peer");
	BUG_ON(ICAP_PRIVILEGED(icap));

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		return -ENOMEM;

	mb_req->req = XCL_MAILBOX_REQ_PEER_DATA;
	subdev_peer.size = resp_len;
	subdev_peer.kind = XCL_MULTISLOT_VERSION;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	(void) xocl_peer_request(xdev,
		mb_req, reqlen, &xcl_multislot, &resp_len, NULL, NULL, 0, 0);

	vfree(mb_req);

	return xcl_multislot.multislot_version;
}

static void icap_read_from_peer(struct platform_device *pdev)
{
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	struct icap *icap = platform_get_drvdata(pdev);
	struct xcl_pr_region xcl_hwicap = {0};
	size_t resp_len = sizeof(struct xcl_pr_region);
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t reqlen = struct_size(mb_req, data, 1) + data_len;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	ICAP_INFO(icap, "reading from peer");
	BUG_ON(ICAP_PRIVILEGED(icap));

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		return;

	mb_req->req = XCL_MAILBOX_REQ_PEER_DATA;
	subdev_peer.size = resp_len;
	subdev_peer.kind = XCL_ICAP;
	subdev_peer.entries = 1;

	memcpy(mb_req->data, &subdev_peer, data_len);

	(void) xocl_peer_request(xdev,
		mb_req, reqlen, &xcl_hwicap, &resp_len, NULL, NULL, 0, 0);

	icap_set_data(icap, &xcl_hwicap);

	vfree(mb_req);
}

static void icap_set_data(struct icap *icap, struct xcl_pr_region *hwicap)
{
	memcpy(&icap->cache, hwicap, sizeof(struct xcl_pr_region));
	icap->cache_expires = ktime_add(ktime_get_boottime(), ktime_set(icap->cache_expire_secs, 0));
}

static unsigned short icap_cached_ocl_frequency(const struct icap *icap, int idx)
{
	u64 freq = 0;

	switch (idx) {
	case 0:
		freq = icap_get_data_nolock(icap->icap_pdev, CLOCK_FREQ_0);
		break;
	case 1:
		freq = icap_get_data_nolock(icap->icap_pdev, CLOCK_FREQ_1);
		break;
	case 2:
		freq = icap_get_data_nolock(icap->icap_pdev, CLOCK_FREQ_2);
		break;
	default:
		ICAP_INFO(icap, "no cached data for %d", idx);
		break;
	}

	return freq;
}

static bool icap_any_bitstream_in_use(struct icap *icap)
{
	struct islot_info *islot = NULL;
	int i = 0;

	/* Check whether any of the bitstream is busy */
	for (i = 0; i < MAX_SLOT_SUPPORT; i++) {
		islot = icap->slot_info[i];
		if (islot == NULL)
			continue;

		BUG_ON(islot->icap_bitstream_ref < 0);
		if (islot->icap_bitstream_ref != 0)
			return true;
	}

	return false;
}

static bool icap_bitstream_in_use(struct icap *icap, uint32_t slot_id)
{
	struct islot_info *islot = icap->slot_info[slot_id];

	BUG_ON(islot->icap_bitstream_ref < 0);
	return islot->icap_bitstream_ref != 0;
}

static int icap_freeze_axi_gate(struct icap *icap)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int ret;

	ICAP_INFO(icap, "freezing CL AXI gate");
	BUG_ON(icap->icap_axi_gate_frozen);
	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	ret = xocl_axigate_freeze(xdev, XOCL_SUBDEV_LEVEL_PRP);
	if (ret)
		ICAP_ERR(icap, "freeze ULP gate failed %d", ret);
	else
		icap->icap_axi_gate_frozen = true;

	return ret;
}

static int icap_free_axi_gate(struct icap *icap)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int ret;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));
	ICAP_INFO(icap, "freeing CL AXI gate");
	/*
	 * First pulse the OCL RESET. This is important for PR with multiple
	 * clocks as it resets the edge triggered clock converter FIFO
	 */

	if (!icap->icap_axi_gate_frozen)
		return 0;

	ret = xocl_axigate_free(xdev, XOCL_SUBDEV_LEVEL_PRP);
	if (ret)
		ICAP_ERR(icap, "free ULP gate failed %d", ret);
	else
		icap->icap_axi_gate_frozen = false;
	return 0;
}

static void platform_reset_axi_gate(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);

	/* Can only be done from mgmt pf. */
	if (!ICAP_PRIVILEGED(icap))
		return;

	mutex_lock(&icap->icap_lock);
	if (!icap_any_bitstream_in_use(icap)) {
		(void) icap_freeze_axi_gate(platform_get_drvdata(pdev));
		(void) icap_free_axi_gate(platform_get_drvdata(pdev));
	}
	mutex_unlock(&icap->icap_lock);
}

static unsigned short icap_get_ocl_frequency(const struct icap *icap, int idx)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	u64 freq = 0;
	int err;

	if (ICAP_PRIVILEGED(icap)) {
		unsigned short value;

		err = xocl_clock_get_freq_by_id(xdev, 0, &value, idx);
		if (err)
			ICAP_WARN(icap, "clock subdev returns %d.", err);
		else
			freq = value;
	} else
		freq = icap_cached_ocl_frequency(icap, idx);

	return freq;
}

static unsigned int icap_get_clock_frequency_counter_khz(const struct icap *icap, int idx)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	u32 freq = 0;
	int err;
	uint32_t slot_id = 0;
	struct islot_info *islot = NULL;

	for (slot_id = 0; slot_id < MAX_SLOT_SUPPORT; slot_id++) {
		islot = icap->slot_info[slot_id];
		if (!islot)
			continue;

		/* Clock frequence is only related to PL Slots */
		if (islot->pl_slot)
			break;
	}

	/* No PL Slot found. Returning from here */
	if (slot_id == MAX_SLOT_SUPPORT) 
		return 0;

	if (ICAP_PRIVILEGED(icap)) {
		if (uuid_is_null(&islot->icap_bitstream_uuid))
			return freq;
		err = xocl_clock_get_freq_counter(xdev, &freq, idx);
		if (err)
			ICAP_WARN(icap, "clock subdev returns %d.", err);
	} else {
		switch (idx) {
		case 0:
			freq = icap_get_data_nolock(icap->icap_pdev, FREQ_COUNTER_0);
			break;
		case 1:
			freq = icap_get_data_nolock(icap->icap_pdev, FREQ_COUNTER_1);
			break;
		case 2:
			freq = icap_get_data_nolock(icap->icap_pdev, FREQ_COUNTER_2);
			break;
		default:
			break;
		}
	}
	return freq;
}

static void xclbin_get_ocl_frequency_max_min(struct icap *icap,
	int idx, unsigned short *freq_max, unsigned short *freq_min, uint32_t slot_id)
{
	struct clock_freq_topology *topology = 0;
	struct islot_info *islot = icap->slot_info[slot_id];
	int num_clocks = 0;

	if (!islot)
		return;
		
	if (!uuid_is_null(&islot->icap_bitstream_uuid)) {
		topology = islot->xclbin_clock_freq_topology;
		if (!topology)
			return;

		num_clocks = topology->m_count;

		if (idx >= num_clocks)
			return;

		if (freq_max)
			*freq_max = topology->m_clock_freq[idx].m_freq_Mhz;

		if (freq_min)
			*freq_min = 10;
	}
}

static int ulp_clock_update(struct icap *icap, unsigned short *freqs,
	int num_freqs, int verify)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	err = xocl_clock_freq_scaling_by_request(xdev, freqs, num_freqs, verify);

	ICAP_INFO(icap, "returns: %d", err);
	return err;
}

static int icap_xclbin_validate_clock_req_impl(struct platform_device *pdev,
	struct drm_xocl_reclock_info *freq_obj, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	unsigned short freq_max, freq_min;
	int i;

	if (!islot)
		return -EINVAL;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	if (uuid_is_null(&islot->icap_bitstream_uuid)) {
		ICAP_ERR(icap, "ERROR: There isn't a hardware accelerator loaded in the dynamic region."
			" Validation of accelerator frequencies cannot be determine");
		return -EDOM;
	}

	for (i = 0; i < ARRAY_SIZE(freq_obj->ocl_target_freq); i++) {
		if (!freq_obj->ocl_target_freq[i])
			continue;
		freq_max = freq_min = 0;
		xclbin_get_ocl_frequency_max_min(icap, i, &freq_max, &freq_min,
						 slot_id);
		ICAP_INFO(icap, "requested frequency is : %d, "
			"xclbin freq is: %d, "
			"xclbin minimum freq allowed is: %d",
			freq_obj->ocl_target_freq[i],
			freq_max, freq_min);
		if (freq_obj->ocl_target_freq[i] > freq_max ||
			freq_obj->ocl_target_freq[i] < freq_min) {
			ICAP_ERR(icap, "Unable to set frequency! "
				"Frequency max: %d, Frequency min: %d, "
				"Requested frequency: %d",
				freq_max, freq_min,
				freq_obj->ocl_target_freq[i]);
			return -EDOM;
		}
	}

	return 0;
}

static int icap_xclbin_validate_clock_req(struct platform_device *pdev,
	struct drm_xocl_reclock_info *freq_obj)
{
	struct icap *icap = platform_get_drvdata(pdev);
	uint32_t slot_id = 0;
	struct islot_info *islot = NULL;
	int err = 0;

	mutex_lock(&icap->icap_lock);
	for (slot_id = 0; slot_id < MAX_SLOT_SUPPORT; slot_id++) {
		islot = icap->slot_info[slot_id];
		if (!islot)
			continue;

		/* Clock frequence is only related to PL Slots */
		if (islot->pl_slot) {
			err = icap_xclbin_validate_clock_req_impl(pdev, freq_obj, slot_id);
			if (err) {
				mutex_unlock(&icap->icap_lock);
				return err;
			}
		}
	}
	mutex_unlock(&icap->icap_lock);

	return err;
}

static int icap_ocl_update_clock_freq_topology(struct platform_device *pdev,
	struct xclmgmt_ioc_freqscaling *freq_obj)
{
	struct icap *icap = platform_get_drvdata(pdev);
	uint32_t slot_id = 0;
	struct islot_info *islot = NULL;
	int err = 0;

	err = icap_xclbin_rd_lock(icap, slot_id);
	if (err)
		return err;

	mutex_lock(&icap->icap_lock);
	for (slot_id = 0; slot_id < MAX_SLOT_SUPPORT; slot_id++) {
		islot = icap->slot_info[slot_id];
		if (!islot)
			continue;

		/* Clock frequence is only related to PL Slots */
		if (islot->pl_slot) {
			err = icap_xclbin_validate_clock_req_impl(pdev,
				(struct drm_xocl_reclock_info *)freq_obj, slot_id);
			if (err)
				goto done;
		}
	}

	err = ulp_clock_update(icap, freq_obj->ocl_target_freq,
	    ARRAY_SIZE(freq_obj->ocl_target_freq), 1);
	if (err)
		goto done;

	for (slot_id = 0; slot_id < MAX_SLOT_SUPPORT; slot_id++) {
		islot = icap->slot_info[slot_id];
		if (!islot)
			continue;

		if (islot->pl_slot) {
			err = icap_calib_and_check(pdev, slot_id);
			if (err)
				goto done;
		}
	}
done:
	mutex_unlock(&icap->icap_lock);
	icap_xclbin_rd_unlock(icap, slot_id);
	return err;
}

static int icap_cached_get_freq(struct platform_device *pdev,
	unsigned int region, unsigned short *freqs, int num_freqs)
{
	int i;
	struct icap *icap = platform_get_drvdata(pdev);

	if (ICAP_PRIVILEGED(icap)) {
		ICAP_ERR(icap, "no cached data in mgmt pf");
		return -EINVAL;
	}

	mutex_lock(&icap->icap_lock);
	for (i = 0; i < min(ICAP_MAX_NUM_CLOCKS, num_freqs); i++)
		freqs[i] = icap_cached_ocl_frequency(icap, i);
	mutex_unlock(&icap->icap_lock);

	return 0;
}

static int icap_ocl_get_freqscaling(struct platform_device *pdev,
	unsigned int region, unsigned short *freqs, int num_freqs)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0;

	if (ICAP_PRIVILEGED(icap)) {
		err = xocl_clock_get_freq(xdev, region, freqs, num_freqs);
		if (err == -ENODEV)
			ICAP_ERR(icap, "no clock subdev");
		return err;
	} else {
		return icap_cached_get_freq(pdev, region, freqs, num_freqs);
	}
}

static inline bool mig_calibration_done(struct icap *icap)
{
	BUG_ON(!mutex_is_locked(&icap->icap_lock));
	return icap->icap_state ? (reg_rd(&icap->icap_state->igs_state) & BIT(0)) != 0 : 0;
}

/* Check for MIG calibration. */
static int calibrate_mig(struct icap *icap, uint32_t slot_id)
{
	int i;
	struct mem_topology *mem_topo = NULL; 
	struct islot_info *islot = icap->slot_info[slot_id];
	/* Check for any DDR or PLRAM banks that are in use */
	bool is_memory_bank_connected = false;
	
	if (!islot)
		return -EINVAL;

	mem_topo = islot->mem_topo;

	/* If a DDR or PLRAM bank is found no need to keep searching */
	for (i = 0; (i < mem_topo->m_count) && (!is_memory_bank_connected); i++) {
		struct mem_data* mem_bank = &mem_topo->m_mem_data[i];
		if (MEM_NEEDS_CALIBRATION(mem_bank->m_tag) && (mem_bank->m_used != 0))
			is_memory_bank_connected = true;
	}

	// If no DDR or PLRAM banks are in use there is nothing to calibrate
	if (!is_memory_bank_connected) {
		ICAP_INFO(icap, "No DDR, HBM, or PLRAM memory used. Skipping MIG Calibration\n");
		return 0;
	}

	for (i = 0; i < 20 && !mig_calibration_done(icap); ++i)
		msleep(500);

	if (!mig_calibration_done(icap)) {
		ICAP_ERR(icap,
			"MIG calibration timeout after bitstream download");
		return -ETIMEDOUT;
	}

	ICAP_DBG(icap, "took %ds", i/2);
	return 0;
}

static inline void xclbin_free_clock_freq_topology(struct icap *icap,
						   uint32_t slot_id)
{
	struct islot_info *islot = icap->slot_info[slot_id];

	vfree(islot->xclbin_clock_freq_topology);
	islot->xclbin_clock_freq_topology = NULL;
	islot->xclbin_clock_freq_topology_length = 0;
}

static void xclbin_write_clock_freq(struct clock_freq *dst, struct clock_freq *src)
{
	dst->m_freq_Mhz = src->m_freq_Mhz;
	dst->m_type = src->m_type;
	memcpy(&dst->m_name, &src->m_name, sizeof(src->m_name));
}


static int icap_cache_clock_freq_topology(struct icap *icap,
	const struct axlf *xclbin, uint32_t slot_id)
{
	int i = 0;
	struct clock_freq_topology *topology = NULL;
	struct clock_freq_topology *islot_topology = NULL;
	struct clock_freq *clk_freq = NULL;
	const struct axlf_section_header *hdr =
		xrt_xclbin_get_section_hdr(xclbin, CLOCK_FREQ_TOPOLOGY);
	struct islot_info *islot = icap->slot_info[slot_id];

	/* Can't find CLOCK_FREQ_TOPOLOGY, just return*/
	if (!hdr)
		return 0;
		
	if (!islot)
		return -EINVAL;

	xclbin_free_clock_freq_topology(icap, slot_id);

	islot_topology = vzalloc(hdr->m_sectionSize);
	if (!islot_topology)
		return -ENOMEM;

	topology = (struct clock_freq_topology *)(((char *)xclbin) + hdr->m_sectionOffset);

	/*
	 *  islot_topology->m_clock_freq
	 *  must follow the order
	 *
	 *	0: DATA_CLK
	 *	1: KERNEL_CLK
	 *	2: SYSTEM_CLK
	 *
	 */
	islot_topology->m_count = topology->m_count;
	for (i = 0; i < topology->m_count; ++i) {
		if (topology->m_clock_freq[i].m_type == CT_SYSTEM)
			clk_freq = &islot_topology->m_clock_freq[SYSTEM_CLK];
		else if (topology->m_clock_freq[i].m_type == CT_DATA)
			clk_freq = &islot_topology->m_clock_freq[DATA_CLK];
		else if (topology->m_clock_freq[i].m_type == CT_KERNEL)
			clk_freq = &islot_topology->m_clock_freq[KERNEL_CLK];
		else
			break;

		xclbin_write_clock_freq(clk_freq, &topology->m_clock_freq[i]);
	}

	islot->xclbin_clock_freq_topology = islot_topology;
	islot->xclbin_clock_freq_topology_length = hdr->m_sectionSize;

	return 0;
}

static int wait_for_done(struct icap *icap)
{
	u32 w;
	int i = 0;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));
	for (i = 0; i < 10; i++) {
		udelay(5);
		w = reg_rd(&icap->icap_regs->ir_sr);
		ICAP_INFO(icap, "XHWICAP_SR: %x", w);
		if (w & 0x5)
			return 0;
	}

	ICAP_ERR(icap, "bitstream download timeout");
	return -ETIMEDOUT;
}

static int icap_write(struct icap *icap, const u32 *word_buf, int size)
{
	int i;
	u32 value = 0;

	for (i = 0; i < size; i++) {
		value = be32_to_cpu(word_buf[i]);
		reg_wr(&icap->icap_regs->ir_wf, value);
	}

	reg_wr(&icap->icap_regs->ir_cr, 0x1);

	for (i = 0; i < 20; i++) {
		value = reg_rd(&icap->icap_regs->ir_cr);
		if ((value & 0x1) == 0)
			return 0;
		ndelay(50);
	}

	ICAP_ERR(icap, "writing %d dwords timeout", size);
	return -EIO;
}

static uint64_t icap_get_section_size(struct icap *icap,
			 enum axlf_section_kind kind, uint32_t slot_id)
{
	uint64_t size = 0;
	struct islot_info *islot = icap->slot_info[slot_id];

	switch (kind) {
	case IP_LAYOUT:
		size = sizeof_sect(islot->ip_layout, m_ip_data);
		break;
	case MEM_TOPOLOGY:
		size = sizeof_sect(islot->mem_topo, m_mem_data);
		break;
	case ASK_GROUP_TOPOLOGY:
		size = sizeof_sect(islot->group_topo, m_mem_data);
		break;
	case DEBUG_IP_LAYOUT:
		size = sizeof_sect(islot->debug_layout, m_debug_ip_data);
		break;
	case CONNECTIVITY:
		size = sizeof_sect(islot->connectivity, m_connection);
		break;
	case ASK_GROUP_CONNECTIVITY:
		size = sizeof_sect(islot->group_connectivity, m_connection);
		break;
	case CLOCK_FREQ_TOPOLOGY:
		size = sizeof_sect(islot->xclbin_clock_freq_topology,
				   m_clock_freq);
		break;
	case PARTITION_METADATA:
		size = fdt_totalsize(islot->partition_metadata);
		break;
	default:
		break;
	}

	return size;
}

static int bitstream_helper(struct icap *icap, const u32 *word_buffer,
	unsigned word_count)
{
	unsigned remain_word;
	unsigned word_written = 0;
	int wr_fifo_vacancy = 0;
	int err = 0;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));
	for (remain_word = word_count; remain_word > 0;
		remain_word -= word_written, word_buffer += word_written) {
		wr_fifo_vacancy = reg_rd(&icap->icap_regs->ir_wfv);
		if (wr_fifo_vacancy <= 0) {
			ICAP_ERR(icap, "no vacancy: %d", wr_fifo_vacancy);
			err = -EIO;
			break;
		}
		word_written = (wr_fifo_vacancy < remain_word) ?
			wr_fifo_vacancy : remain_word;
		if (icap_write(icap, word_buffer, word_written) != 0) {
			ICAP_ERR(icap, "write failed remain %d, written %d",
					remain_word, word_written);
			err = -EIO;
			break;
		}
	}

	return err;
}

static void icap_config_engine_reset(struct icap *icap)
{
	BUG_ON(!mutex_is_locked(&icap->icap_lock));
	if (!icap->icap_config_engine)
		return;

	reg_wr(&icap->icap_config_engine->ice_reg, ICAP_SET_RESET);
	msleep(200);
	reg_wr(&icap->icap_config_engine->ice_reg, ICAP_CLEAR_RESET);
	msleep(200);
}

static long icap_download(struct icap *icap, const char *buffer,
	unsigned long length)
{
	long err = 0;
	struct XHwIcap_Bit_Header bit_header = { 0 };
	unsigned numCharsRead = DMA_HWICAP_BITFILE_BUFFER_SIZE;
	unsigned byte_read;

	BUG_ON(!buffer);
	BUG_ON(!length);

	if (xrt_xclbin_parse_header(buffer,
		DMA_HWICAP_BITFILE_BUFFER_SIZE, &bit_header)) {
		err = -EINVAL;
		goto free_buffers;
	}

	if ((bit_header.HeaderLength + bit_header.BitstreamLength) > length) {
		err = -EINVAL;
		goto free_buffers;
	}

	buffer += bit_header.HeaderLength;

	icap_config_engine_reset(icap);

	for (byte_read = 0; byte_read < bit_header.BitstreamLength;
		byte_read += numCharsRead) {
		numCharsRead = bit_header.BitstreamLength - byte_read;
		if (numCharsRead > DMA_HWICAP_BITFILE_BUFFER_SIZE)
			numCharsRead = DMA_HWICAP_BITFILE_BUFFER_SIZE;

		err = bitstream_helper(icap, (u32 *)buffer,
			numCharsRead / sizeof(u32));
		if (err)
			goto free_buffers;
		buffer += numCharsRead;
	}

	err = wait_for_done(icap);

free_buffers:
	xrt_xclbin_free_header(&bit_header);
	return err;
}

static int icap_download_hw(struct icap *icap, const struct axlf *axlf)
{
	uint64_t primaryFirmwareOffset = 0;
	uint64_t primaryFirmwareLength = 0;
	const struct axlf_section_header *primaryHeader = 0;
	uint64_t length;
	int err = -EINVAL;
	char *buffer = (char *)axlf;

	if (!axlf) {
		err = -EINVAL;
		goto done;
	}

	length = axlf->m_header.m_length;

	primaryHeader = xrt_xclbin_get_section_hdr(axlf, BITSTREAM);

	if (primaryHeader) {
		primaryFirmwareOffset = primaryHeader->m_sectionOffset;
		primaryFirmwareLength = primaryHeader->m_sectionSize;
	}
        else {
                ICAP_ERR(icap,"Invalid xclbin. Bitstream is not present in xclbin");
                err = -ENODATA;
                goto done;
	}

	if ((primaryFirmwareOffset + primaryFirmwareLength) > length) {
		ICAP_ERR(icap, "Invalid BITSTREAM size");
		err = -EINVAL;
		goto done;
	}

	if (primaryFirmwareLength) {
		ICAP_INFO(icap,
			"found second stage bitstream of size 0x%llx",
			primaryFirmwareLength);
		err = icap_download(icap, buffer + primaryFirmwareOffset,
			primaryFirmwareLength);
		if (err) {
			ICAP_ERR(icap, "Dowload bitstream failed");
			goto done;
		}
	}

done:
	ICAP_INFO(icap, "%s, err = %d", __func__, err);
	return err;
}

static int icap_download_boot_firmware(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct axlf *bin_obj_axlf;
	int err = 0;
	uint64_t mbBinaryOffset = 0;
	uint64_t mbBinaryLength = 0;
	const struct axlf_section_header *mbHeader = 0;
	bool load_sched = false, load_mgmt = false;
	char *fw_buf = NULL, *sched_buf = NULL;
	size_t fw_size = 0, sched_len = 0;

	/* Can only be done from mgmt pf. */
	if (!ICAP_PRIVILEGED(icap))
		return -EPERM;

	err = xocl_rom_load_firmware(xdev, &fw_buf, &fw_size);
	if (err)
		return err;

	bin_obj_axlf = (struct axlf *)fw_buf;

	if (xocl_mb_sched_on(xdev)) {
		const char *sched_bin = XDEV(xdev)->priv.sched_bin;
		char bin[32] = {0}; 

		/* Try locating the microblaze binary. 
		 * For dynamic platforms like 1RP or 2RP, we load the ert fw
		 * under /lib/firmware/xilinx regardless there is an ert fw 
		 * in partition.xsabin or not
		 */

		if (!sched_bin) {
			mbHeader = xrt_xclbin_get_section_hdr(bin_obj_axlf,
					PARTITION_METADATA);
			if (mbHeader) {
				const char *ert_ver = xocl_fdt_get_ert_fw_ver(xdev,
					fw_buf + mbHeader->m_sectionOffset);

				if (ert_ver && !strncmp(ert_ver, "legacy", 6)) {
					sched_bin = "xilinx/sched.bin";
				} else if (ert_ver) {
					snprintf(bin, sizeof(bin),
						"xilinx/sched_%s.bin", ert_ver);
					sched_bin = bin;
				}
			}
		}

		if (sched_bin) {
			err = xocl_request_firmware(&pcidev->dev, sched_bin,
						    &sched_buf, &sched_len);
			if (!err)  {
				xocl_mb_load_sche_image(xdev, sched_buf, sched_len);
				ICAP_INFO(icap, "stashed shared mb sche bin, len %ld", sched_len);
				load_sched = true;
				vfree(sched_buf);
			}
		}

		if (!load_sched) {
			ICAP_WARN(icap, "Couldn't find /lib/firmware/%s", sched_bin);
			err = 0;
		}
	}

	if (xocl_mb_mgmt_on(xdev)) {
		/* Try locating the board mgmt binary. */
		mbHeader = xrt_xclbin_get_section_hdr(bin_obj_axlf, FIRMWARE);
		if (mbHeader) {
			mbBinaryOffset = mbHeader->m_sectionOffset;
			mbBinaryLength = mbHeader->m_sectionSize;
			xocl_mb_load_mgmt_image(xdev, fw_buf + mbBinaryOffset,
				mbBinaryLength);
			ICAP_INFO(icap, "stashed mb mgmt binary, len %lld",
					mbBinaryLength);
			load_mgmt = true;
		}
	}

	if (load_mgmt || load_sched)
		xocl_mb_reset(xdev);

	/* save BMC version */
	(void)sprintf(icap->bmc_header.m_version, "%s", NONE_BMC_VERSION);
	mbHeader = xrt_xclbin_get_section_hdr(bin_obj_axlf, BMC);
	if (mbHeader) {
		if (mbHeader->m_sectionSize < sizeof(struct bmc)) {
			err = -EINVAL;
			ICAP_ERR(icap, "Invalid bmc section size %lld",
					mbHeader->m_sectionSize);
			goto done;
		}
		memcpy(&icap->bmc_header, fw_buf + mbHeader->m_sectionOffset,
				sizeof(struct bmc));
		if (icap->bmc_header.m_size > mbHeader->m_sectionSize) {
			err = -EINVAL;
			ICAP_ERR(icap, "Invalid bmc size %lld",
					icap->bmc_header.m_size);
			goto done;
		}
	}

done:
	vfree(fw_buf);
	ICAP_INFO(icap, "%s err: %d", __func__, err);
	return err;
}

static int icap_post_download_rp(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	bool load_mbs = false;

	if (xocl_mb_mgmt_on(xdev) && icap->rp_mgmt_bin) {
		xocl_mb_load_mgmt_image(xdev, icap->rp_mgmt_bin,
			icap->rp_mgmt_bin_len);
		ICAP_INFO(icap, "stashed mb mgmt binary, len %ld",
			icap->rp_mgmt_bin_len);
		vfree(icap->rp_mgmt_bin);
		icap->rp_mgmt_bin = NULL;
		icap->rp_mgmt_bin_len = 0;
		load_mbs = true;
	}

	if (xocl_mb_sched_on(xdev) && icap->rp_sche_bin) {
		xocl_mb_load_sche_image(xdev, icap->rp_sche_bin,
			icap->rp_sche_bin_len);
		ICAP_INFO(icap, "stashed mb sche binary, len %ld",
			icap->rp_sche_bin_len);
		vfree(icap->rp_sche_bin);
		icap->rp_sche_bin = NULL;
		icap->rp_sche_bin_len = 0;
		/* u200 2RP EA does not have ert subdev */
		if (xocl_ert_reset(xdev) == -ENODEV)
			load_mbs = true;
	}

	if (load_mbs)
		xocl_mb_reset(xdev);

	return 0;
}

static int icap_download_rp(struct platform_device *pdev, int level, int flag)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xcl_mailbox_req mbreq = { 0 };
	int ret = 0;

	mbreq.req = XCL_MAILBOX_REQ_CHG_SHELL;
	mutex_lock(&icap->icap_lock);
	if (flag == RP_DOWNLOAD_CLEAR) {
		xocl_xdev_info(xdev, "Clear firmware bins");
		icap_free_bins(icap);
		goto end;
	}
	if (!icap->rp_bit || !icap->rp_fdt) {
		xocl_xdev_err(xdev, "Invalid reprogram request %p.%p",
			icap->rp_bit, icap->rp_fdt);
		ret = -EINVAL;
		goto failed;
	}

	if (!XDEV(xdev)->blp_blob) {
		xocl_xdev_err(xdev, "Empty BLP blob");
		ret = -EINVAL;
		goto failed;
	}

	ret = xocl_fdt_check_uuids(xdev, icap->rp_fdt,
		XDEV(xdev)->blp_blob);
	if (ret) {
		xocl_xdev_err(xdev, "Incompatible uuids");
		goto failed;
	}

	if (flag == RP_DOWNLOAD_DRY)
		goto end;

	else if (flag == RP_DOWNLOAD_NORMAL) {
		(void) xocl_peer_notify(xocl_get_xdev(icap->icap_pdev), &mbreq,
				struct_size(&mbreq, data, 1));
		ICAP_INFO(icap, "Notified userpf to program rp");
		goto end;
	}

	ret = xocl_fdt_blob_input(xdev, icap->rp_fdt, icap->rp_fdt_len,
			XOCL_SUBDEV_LEVEL_PRP, icap->rp_vbnv);
	if (ret) {
		xocl_xdev_err(xdev, "failed to parse fdt %d", ret);
		goto failed;
	}

	ret = xocl_axigate_freeze(xdev, XOCL_SUBDEV_LEVEL_BLD);
	if (ret) {
		xocl_xdev_err(xdev, "freeze blp gate failed %d", ret);
		goto failed;
	}


	//wait_event_interruptible(mytestwait, false);

	reg_wr(&icap->icap_regs->ir_cr, 0x8);
	ndelay(2000);
	reg_wr(&icap->icap_regs->ir_cr, 0x0);
	ndelay(2000);
	reg_wr(&icap->icap_regs->ir_cr, 0x4);
	ndelay(2000);
	reg_wr(&icap->icap_regs->ir_cr, 0x0);
	ndelay(2000);

	ret = icap_download(icap, icap->rp_bit, icap->rp_bit_len);
	if (ret)
		goto failed;

	ret = xocl_axigate_free(xdev, XOCL_SUBDEV_LEVEL_BLD);
	if (ret) {
		xocl_xdev_err(xdev, "freeze blp gate failed %d", ret);
		goto failed;
	}

failed:
	if (icap->rp_bit) {
		vfree(icap->rp_bit);
		icap->rp_bit = NULL;
		icap->rp_bit_len = 0;
	}
	if (icap->rp_fdt) {
		vfree(icap->rp_fdt);
		icap->rp_fdt = NULL;
		icap->rp_fdt_len = 0;
	}

end:
	mutex_unlock(&icap->icap_lock);
	return ret;
}

static long axlf_set_freqscaling(struct icap *icap, uint32_t slot_id)
{
	struct islot_info *islot = icap->slot_info[slot_id];
	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	return xocl_clock_freq_scaling_by_topo(xocl_get_xdev(icap->icap_pdev),
	    islot->xclbin_clock_freq_topology, 0);
}

static int icap_download_bitstream(struct icap *icap, const struct axlf *axlf)
{
	long err = 0;

	icap_freeze_axi_gate(icap);

	err = icap_download_hw(icap, axlf);
	/*
	 * Perform frequency scaling since PR download can silenty overwrite
	 * MMCM settings in static region changing the clock frequencies
	 * although ClockWiz CONFIG registers will misleading report the older
	 * configuration from before bitstream download as if nothing has
	 * changed.
	 */
	if (!err) {
		err = xocl_clock_freq_rescaling(xocl_get_xdev(icap->icap_pdev), true);
		err = (err == -ENODEV) ? 0 : err;
	}

	icap_free_axi_gate(icap);
	return err;
}

static void icap_clean_axlf_section(struct icap *icap,
	enum axlf_section_kind kind, uint32_t slot_id)
{
	void **target = NULL;
	struct islot_info *islot = icap->slot_info[slot_id];

	switch (kind) {
	case IP_LAYOUT:
		target = (void **)&islot->ip_layout;
		break;
	case SOFT_KERNEL:
		target = (void **)&islot->ps_kernel;
		break;
	case MEM_TOPOLOGY:
		target = (void **)&islot->mem_topo;
		break;
	case ASK_GROUP_TOPOLOGY:
		target = (void **)&islot->group_topo;
		break;
	case DEBUG_IP_LAYOUT:
		target = (void **)&islot->debug_layout;
		break;
	case CONNECTIVITY:
		target = (void **)&islot->connectivity;
		break;
	case ASK_GROUP_CONNECTIVITY:
		target = (void **)&islot->group_connectivity;
		break;
	case CLOCK_FREQ_TOPOLOGY:
		target = (void **)&islot->xclbin_clock_freq_topology;
		break;
	case PARTITION_METADATA:
		target = (void **)&islot->partition_metadata;
		break;
	default:
		break;
	}
	if (target && *target) {
		vfree(*target);
		*target = NULL;
	}
}

static void icap_clean_bitstream_axlf(struct platform_device *pdev,
		uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];

	if (!islot)
	       return;

	uuid_copy(&islot->icap_bitstream_uuid, &uuid_null);
	icap_clean_axlf_section(icap, IP_LAYOUT, slot_id);
	icap_clean_axlf_section(icap, SOFT_KERNEL, slot_id);
	icap_clean_axlf_section(icap, MEM_TOPOLOGY, slot_id);
	icap_clean_axlf_section(icap, ASK_GROUP_TOPOLOGY, slot_id);
	icap_clean_axlf_section(icap, DEBUG_IP_LAYOUT, slot_id);
	icap_clean_axlf_section(icap, CONNECTIVITY, slot_id);
	icap_clean_axlf_section(icap, ASK_GROUP_CONNECTIVITY, slot_id);
	icap_clean_axlf_section(icap, CLOCK_FREQ_TOPOLOGY, slot_id);
	icap_clean_axlf_section(icap, PARTITION_METADATA, slot_id);
}

static uint16_t icap_get_memidx(struct mem_topology *mem_topo, enum IP_TYPE ecc_type,
	int idx)
{
	uint16_t memidx = INVALID_MEM_IDX, mem_idx = 0;
	uint32_t i;
	enum MEM_TAG m_tag, target_m_tag;

	if (!mem_topo)
		return INVALID_MEM_IDX;

	/*
	 * Get global memory index by feeding desired memory type and index
	 */
	if (ecc_type == IP_MEM_DDR4)
		target_m_tag = MEM_TAG_DDR;
	else if (ecc_type == IP_DDR4_CONTROLLER)
		target_m_tag = MEM_TAG_DDR;
	else if (ecc_type == IP_MEM_HBM_ECC)
		target_m_tag = MEM_TAG_HBM;
	else
		return INVALID_MEM_IDX;

	for (i = 0; i < mem_topo->m_count; ++i) {
		m_tag = convert_mem_tag(mem_topo->m_mem_data[i].m_tag);
		if (m_tag == target_m_tag) {
			if (idx == mem_idx)
				return i;
			mem_idx++;
		}
	}

	return memidx;
}

static int icap_create_subdev_debugip(struct platform_device *pdev,
				      uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0, i = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct debug_ip_layout *debug_ip_layout = NULL;
	struct islot_info *islot = icap->slot_info[slot_id];

	if (!islot)
	       return -EINVAL;

	debug_ip_layout = islot->debug_layout;
	if (!debug_ip_layout)
		return err;

	for (i = 0; i < debug_ip_layout->m_count; ++i) {
		struct debug_ip_data *ip = &debug_ip_layout->m_debug_ip_data[i];

		if (ip->m_type == AXI_MM_MONITOR) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_AIM;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create AXI_MM_MONITOR subdev");
				break;
			}
		} else if (ip->m_type == ACCEL_MONITOR) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_AM;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create ACCEL_MONITOR subdev");
				break;
			}
		} else if (ip->m_type == AXI_STREAM_MONITOR) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_ASM;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create AXI_STREAM_MONITOR subdev");
				break;
			}
		} else if (ip->m_type == AXI_MONITOR_FIFO_LITE) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_TRACE_FIFO_LITE;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create AXI_MONITOR_FIFO_LITE subdev");
				break;
			}
		} else if (ip->m_type == AXI_MONITOR_FIFO_FULL) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_TRACE_FIFO_FULL;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create AXI_MONITOR_FIFO_FULL subdev");
				break;
			}
		} else if (ip->m_type == AXI_TRACE_FUNNEL) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_TRACE_FUNNEL;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create AXI_MONITOR_TRACE_FUNNEL subdev");
				break;
			}
		} else if (ip->m_type == TRACE_S2MM) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_TRACE_S2MM;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create AXI_MONITOR_TRACE_S2MM subdev");
				break;
			}
		} else if (ip->m_type == LAPC) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_LAPC;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create LAPC subdev");
				break;
			}
		} else if (ip->m_type == AXI_STREAM_PROTOCOL_CHECKER) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_SPC;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create SPC subdev");
				break;
			}
		} else if (ip->m_type == ACCEL_DEADLOCK_DETECTOR) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_ACCEL_DEADLOCK_DETECTOR;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
			subdev_info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create ACCEL_DEADLOCK_DETECTOR subdev");
				break;
			}
		}
	}
	return err;
}

/*
 * TODO: clear the comments, it seems that different subdev has different
 *    flow during creation. Using specific function to create specific subdev
 *    gives us flexibility to adjust the download procedure.
 *
 * Add sub device dynamically.
 * restrict any dynamically added sub-device and 1 base address,
 * Has pre-defined length
 *  Ex:    "ip_data": {
 *         "m_type": "IP_DNASC",
 *         "properties": "0x0",
 *         "m_base_address": "0x1100000", <--  base address
 *         "m_name": "slr0\/dna_self_check_0"
 */
static int icap_create_subdev_ip_layout(struct platform_device *pdev,
					uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0, i = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct ip_layout *ip_layout = NULL;
	struct mem_topology *mem_topo = NULL;
	struct islot_info *islot = icap->slot_info[slot_id];

	if (!islot)
	       return -EINVAL;

	ip_layout = islot->ip_layout;
	mem_topo = islot->mem_topo;
	if (!ip_layout || !mem_topo) {
		err = -ENODEV;
		goto done;
	}

	for (i = 0; i < ip_layout->m_count; ++i) {
		struct ip_data *ip = &ip_layout->m_ip_data[i];
		struct xocl_mig_label mig_label = { {0} };
		uint32_t memidx = 0;

		if (ip->m_type == IP_KERNEL)
			continue;

		if (ip->m_type == IP_DDR4_CONTROLLER || ip->m_type == IP_MEM_DDR4) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_MIG;

			if (!strncasecmp(ip->m_name, "SRSR", 4))
				continue;

			memidx = icap_get_memidx(mem_topo, ip->m_type, ip->properties);

			if (memidx == INVALID_MEM_IDX) {
				ICAP_ERR(icap, "INVALID_MEM_IDX: %u",
					ip->properties);
				continue;
			}

			if (!mem_topo || memidx >= mem_topo->m_count) {
				ICAP_ERR(icap, "bad ECC controller index: %u",
					ip->properties);
				continue;
			}
			if (!mem_topo->m_mem_data[memidx].m_used) {
				ICAP_INFO(icap,
					"ignore ECC controller for: %s",
					mem_topo->m_mem_data[memidx].m_tag);
				continue;
			}

			memcpy(&mig_label.tag, mem_topo->m_mem_data[memidx].m_tag, 16);
			mig_label.mem_idx = memidx;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = &mig_label;
			subdev_info.data_len =
				sizeof(struct xocl_mig_label);
			subdev_info.slot_idx = slot_id;

			if (!ICAP_PRIVILEGED(icap))
				subdev_info.num_res = 0;

			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create MIG subdev");
				goto done;
			}

		} else if (ip->m_type == IP_MEM_HBM_ECC) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_MIG_HBM;
			uint16_t memidx = icap_get_memidx(mem_topo, IP_MEM_HBM_ECC, ip->indices.m_index);

			if (memidx == INVALID_MEM_IDX)
				continue;

			if (!mem_topo || memidx >= mem_topo->m_count) {
				ICAP_ERR(icap, "bad ECC controller index: %u",
					ip->properties);
				continue;
			}

			if (!mem_topo->m_mem_data[memidx].m_used) {
				ICAP_INFO(icap,
					"ignore ECC controller for: %s",
					mem_topo->m_mem_data[memidx].m_tag);
				continue;
			}

			memcpy(&mig_label.tag, mem_topo->m_mem_data[memidx].m_tag, 16);
			mig_label.mem_idx = memidx;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data = &mig_label;
			subdev_info.data_len =
				sizeof(struct xocl_mig_label);
			subdev_info.slot_idx = slot_id;

			if (!ICAP_PRIVILEGED(icap))
				subdev_info.num_res = 0;

			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create MIG_HBM subdev");
				goto done;
			}

		} else if (ip->m_type == IP_DNASC) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_DNA;

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.slot_idx = slot_id;

			if (!ICAP_PRIVILEGED(icap))
				subdev_info.num_res = 0;

			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create DNA subdev");
				goto done;
			}
		}
	}

done:
	return err;
}

static int icap_create_post_download_subdevs(struct platform_device *pdev,
					 struct axlf *xclbin, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0, i = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct ip_layout *ip_layout = NULL;
	struct mem_topology *mem_topo = NULL;
	struct islot_info *islot = icap->slot_info[slot_id];
	uint32_t memidx = 0;

	BUG_ON(!ICAP_PRIVILEGED(icap));
	if (!islot)
	       return -EINVAL;

	ip_layout = islot->ip_layout;
	mem_topo = islot->mem_topo;
	if (!ip_layout || !mem_topo) {
		err = -ENODEV;
		goto done;
	}

	for (i = 0; i < ip_layout->m_count; ++i) {
		struct ip_data *ip = &ip_layout->m_ip_data[i];

		if (ip->m_type == IP_KERNEL)
			continue;

		if (ip->m_type == IP_DDR4_CONTROLLER && !strncasecmp(ip->m_name, "SRSR", 4)) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_SRSR;
			uint32_t idx = 0;

			if (sscanf(ip->m_name, "SRSR-BANK%x", &idx) != 1) {
				err = -EINVAL;
				goto done;
			}

			/* hardcoded, to find a global*/
			memidx = icap_get_memidx(mem_topo, ip->m_type, idx);
			if (memidx == INVALID_MEM_IDX) {
				ICAP_ERR(icap, "INVALID_MEM_IDX: %u",
					ip->properties);
				continue;
			}

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.override_idx = memidx;
			subdev_info.slot_idx = slot_id;

			if (!ICAP_PRIVILEGED(icap))
				subdev_info.num_res = 0;

			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create SRSR subdev");
				goto done;
			}
		}
	}
done:
	if (err)
		xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_SRSR);
	return err;
}

static int icap_create_subdev_dna(struct platform_device *pdev,
	struct axlf *xclbin)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int err = 0;
	uint64_t section_size = 0;
	u32 capability;

	/* capability BIT8 as DRM IP enable, BIT0 as AXI mode
	 * We only check if anyone of them is set.
	 */
	capability = ((xocl_dna_capability(xdev) & 0x101) != 0);

	if (capability) {
		uint32_t *cert = NULL;

		if (0x1 & xocl_dna_status(xdev))
			goto done;
		/*
		 * Any error occurs here should return -EACCES for app to
		 * know that DNA has failed.
		 */
		err = -EACCES;

		ICAP_INFO(icap, "DNA version: %s", (capability & 0x1) ? "AXI" : "BRAM");

		if (xrt_xclbin_get_section(xclbin, DNA_CERTIFICATE,
			(void **)&cert, &section_size) != 0) {

			/* We keep dna sub device if IP_DNASC presents */
			ICAP_ERR(icap, "Can't get certificate section");
			goto done;
		}

		ICAP_INFO(icap, "DNA Certificate Size 0x%llx", section_size);
		if (section_size % 64 || section_size < 576)
			ICAP_ERR(icap, "Invalid certificate size");
		else
			xocl_dna_write_cert(xdev, cert, section_size);

		vfree(cert);


		/* Check DNA validation result. */
		if (0x1 & xocl_dna_status(xdev))
			err = 0; /* xclbin is valid */
		else {
			ICAP_ERR(icap, "DNA inside xclbin is invalid");
			goto done;
		}
	}

done:
	return err;
}

static int icap_peer_xclbin_prepare(struct icap *icap, struct axlf *xclbin,
	uint32_t icap_ver, uint64_t ch_state, uint32_t slot_id, struct xcl_mailbox_req **mb_req)
{
	uint32_t datalen = 0;
	struct xcl_mailbox_req *mb_ptr = NULL;

	if ((ch_state & XCL_MB_PEER_SAME_DOMAIN) != 0) {
		if (icap_ver == MULTISLOT_VERSION) {
			struct xcl_mailbox_bitstream_slot_kaddr slot_mb_addr = {0};
			datalen = struct_size(mb_ptr, data, 1) +
				sizeof(struct xcl_mailbox_bitstream_slot_kaddr);
			mb_ptr = vmalloc(datalen);
			if (!mb_ptr) {
				ICAP_ERR(icap, "can't create mb_req for slot %d\n", slot_id);
				return -ENOMEM;
			}
			mb_ptr->req = XCL_MAILBOX_REQ_LOAD_XCLBIN_SLOT_KADDR;
			slot_mb_addr.addr = (uint64_t)xclbin;
			slot_mb_addr.slot_idx = slot_id;
			memcpy(mb_ptr->data, &slot_mb_addr,
					sizeof(struct xcl_mailbox_bitstream_slot_kaddr));
		}
		else {
			struct xcl_mailbox_bitstream_kaddr mb_addr = {0};
			datalen = struct_size(mb_ptr, data, 1) +
				sizeof(struct xcl_mailbox_bitstream_kaddr);
			mb_ptr = vmalloc(datalen);
			if (!mb_ptr) {
				ICAP_ERR(icap, "can't create mb_req\n");
				return -ENOMEM;
			}
			mb_ptr->req = XCL_MAILBOX_REQ_LOAD_XCLBIN_KADDR;
			mb_addr.addr = (uint64_t)xclbin;
			memcpy(mb_ptr->data, &mb_addr,
					sizeof(struct xcl_mailbox_bitstream_kaddr));
		}
	} else {
		if (icap_ver == MULTISLOT_VERSION) {
			struct xcl_mailbox_bitstream_slot_xclbin slot_xclbin = {0};
			void *data_ptr = NULL;
			datalen =  struct_size(mb_ptr, data, 1) +
				sizeof(struct xcl_mailbox_bitstream_slot_xclbin) +
				xclbin->m_header.m_length;
			mb_ptr = vmalloc(datalen);
			if (!mb_ptr) {
				ICAP_ERR(icap, "can't create mb_req for slot %d\n", slot_id);
				return -ENOMEM;
			}
			mb_ptr->req = XCL_MAILBOX_REQ_LOAD_SLOT_XCLBIN;
			slot_xclbin.slot_idx = slot_id;

			data_ptr = mb_ptr->data;

			/* First copy the slot information */
			memcpy(data_ptr, &slot_xclbin,
					sizeof(struct xcl_mailbox_bitstream_slot_xclbin));

			data_ptr = (void *)((uint64_t)data_ptr +
			       sizeof(struct xcl_mailbox_bitstream_slot_xclbin));
			/* Now copy the actual xclbin data */
			memcpy(data_ptr, xclbin, xclbin->m_header.m_length);
		}
		else {
			datalen = struct_size(mb_ptr, data, 1) +
				xclbin->m_header.m_length;
			mb_ptr = vmalloc(datalen);
			if (!mb_ptr) {
				ICAP_ERR(icap, "can't create mb_req\n");
				return -ENOMEM;
			}
			mb_ptr->req = XCL_MAILBOX_REQ_LOAD_XCLBIN;
			memcpy(mb_ptr->data, xclbin, xclbin->m_header.m_length);
		}
	}

	*mb_req = mb_ptr;
	return datalen;
}

static int __icap_peer_xclbin_download(struct icap *icap, struct axlf *xclbin, uint32_t slot_id)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	uint64_t ch_state = 0;
	uint32_t data_len = 0;
	struct xcl_mailbox_req *mb_req = NULL;
	int msgerr = -ETIMEDOUT;
	size_t resplen = sizeof(msgerr);
	struct islot_info *islot = icap->slot_info[slot_id];
	struct mem_topology *mem_topo = islot->mem_topo;
	int i, mig_count = 0;
	uint32_t timeout;
	uint32_t icap_ver = 0;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	/* We need to always download the xclbin at this point */
	/* Check icap version before transfer xclbin thru mailbox. */
	icap_ver = icap_multislot_version_from_peer(icap->icap_pdev);

	xocl_mailbox_get(xdev, CHAN_STATE, &ch_state);
	data_len = icap_peer_xclbin_prepare(icap, xclbin, icap_ver, ch_state,
		       slot_id, &mb_req);
	if (data_len < 0)
		return data_len;

	if (mem_topo) {
		for (i = 0; i < mem_topo->m_count; i++) {
			if (XOCL_IS_STREAM(mem_topo, i))
				continue;

			if (XOCL_IS_DDR_USED(mem_topo, i))
				mig_count++;
		}
	}

	if (!XOCL_DSA_IS_VERSAL(xdev)) {
		/* Set timeout to be 1s per 2MB for downloading xclbin.
		 * plus toggling axigate time 5s
		 * plus #MIG * 0.5s
		 */
		timeout = xclbin->m_header.m_length / (2048 * 1024) +
			5 + mig_count / 2;
	} else {
		/* Temporarily setting timeout to be 2s per 1MB for downloading
		 * xclbin. TODO Revisit this value after understanding the
		 * expected time consumption on Versal.
		 */
		timeout = (xclbin->m_header.m_length) / (1024 * 1024) * 2 + 5;
	}

	/* In Azure cloud, there is special requirement for xclbin download
	 * that the minumum timeout should be 50s.
	 */
	timeout = max((size_t)timeout, 50UL);

	(void) xocl_peer_request(xdev, mb_req, data_len,
		&msgerr, &resplen, NULL, NULL, timeout, 0);

	vfree(mb_req);

	if (msgerr != 0) {
		ICAP_ERR(icap, "peer xclbin download err: %d", msgerr);
		return msgerr;
	}

	/* Clean up and expire cache after download xclbin */
	memset(&icap->cache, 0, sizeof(struct xcl_pr_region));
	icap->cache_expires = ktime_sub(ktime_get_boottime(), ktime_set(1, 0));
	return 0;
}

static int icap_verify_signature(struct icap *icap,
	const void *data, size_t data_len, const void *sig, size_t sig_len)
{
	int ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0) && defined(CONFIG_SYSTEM_DATA_VERIFICATION)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
/* Starting with Ubuntu 20.04 we need to use VERIFY_USE_PLATFORM_KEYRING in order to use MOK keys*/
#define	SYS_KEYS	(VERIFY_USE_PLATFORM_KEYRING)
#else
#define	SYS_KEYS	((void *)1UL)
#endif
	ret = verify_pkcs7_signature(data, data_len, sig, sig_len,
		(icap->sec_level == ICAP_SEC_SYSTEM) ? SYS_KEYS : icap_keys,
		VERIFYING_UNSPECIFIED_SIGNATURE, NULL, NULL);
	if (ret) {
		ICAP_ERR(icap, "signature verification failed: %d", ret);
		if (icap->sec_level == ICAP_SEC_NONE) {
			/* Ignore error to allow bitstream downloading. */
			ret = 0;
		} else {
			ret = -EKEYREJECTED;
		}
	} else {
		ICAP_INFO(icap, "signature verification is done successfully");
	}
#else
	ret = -EOPNOTSUPP;
	ICAP_ERR(icap,
		"signature verification isn't supported with kernel < 4.7.0");
#endif
	return ret;
}

static int icap_refresh_clock_freq(struct icap *icap, struct axlf *xclbin,
				   uint32_t slot_id)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	if (ICAP_PRIVILEGED(icap) && !XOCL_DSA_IS_SMARTN(xdev)) {
		err = icap_cache_clock_freq_topology(icap, xclbin, slot_id);
		if (!err) {
			err = axlf_set_freqscaling(icap, slot_id);
			/* No clock subdev is ok? */
			err = err == -ENODEV ? 0 : err;
		}
	}

	ICAP_INFO(icap, "ret: %d", err);
	return err;
}

static void icap_save_calib(struct icap *icap, uint32_t slot_id)
{
	struct mem_topology *mem_topo = NULL;
	int err = 0, i = 0, ddr_idx = -1;
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);

	if (!ICAP_PRIVILEGED(icap))
		return;

	if (icap->slot_info[slot_id] == NULL)
		return;

	mem_topo = icap->slot_info[slot_id]->mem_topo;
	if (!mem_topo)
		return;
		
	for (i = 0; i < mem_topo->m_count; ++i) {
		if (convert_mem_tag(mem_topo->m_mem_data[i].m_tag) != MEM_TAG_DDR)
			continue;
		else
			ddr_idx++;

		if (!mem_topo->m_mem_data[i].m_used)
			continue;

		err = xocl_srsr_save_calib(xdev, ddr_idx);
		if (err)
			ICAP_DBG(icap, "Not able to save mem %d calibration data.", i);

	}
	err = xocl_calib_storage_save(xdev);
}

static void icap_calib(struct icap *icap, uint32_t slot_id, bool retain)
{
	int err = 0, i = 0, ddr_idx = -1;
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	struct mem_topology *mem_topo = NULL;
	s64 time_total = 0, delta = 0;
	ktime_t time_start, time_end;

	BUG_ON(!islot);

	mem_topo = islot->mem_topo;
	BUG_ON(!mem_topo);

	(void) xocl_calib_storage_restore(xdev);

	for (; i < mem_topo->m_count; ++i) {
		if (convert_mem_tag(mem_topo->m_mem_data[i].m_tag) != MEM_TAG_DDR)
			continue;
		else
			ddr_idx++;

		if (!mem_topo->m_mem_data[i].m_used)
			continue;

		time_start = ktime_get();
		err = xocl_srsr_calib(xdev, ddr_idx, retain);
		time_end = ktime_get();

		if (err)
			ICAP_DBG(icap, "Not able to calibrate mem %d.", i);
		else {
			/* We only sum up the SRSR calibration time which are valid */
			delta = ktime_ms_delta(time_end, time_start);
			time_total += delta;
		}
	}

	if (time_total)
		ICAP_INFO(icap, "SRSR Calibration: %lld ms.", time_total);
}

static int icap_iores_write32(struct icap *icap, uint32_t id, uint32_t offset, uint32_t val)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	// try PRP first then BLD
	err = xocl_iores_write32(xdev, XOCL_SUBDEV_LEVEL_PRP, id, offset, val);

	if (err)
		err = xocl_iores_write32(xdev, XOCL_SUBDEV_LEVEL_BLD, id, offset, val);


	return err;
}

static int icap_iores_read32(struct icap *icap, uint32_t id, uint32_t offset, uint32_t *val)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	// try PRP first then BLD
	err = xocl_iores_read32(xdev, XOCL_SUBDEV_LEVEL_PRP, id, offset, val);
	if (err)
		err = xocl_iores_read32(xdev, XOCL_SUBDEV_LEVEL_BLD, id, offset, val);

	return err;
}

static int icap_reset_ddr_gate_pin(struct icap *icap)
{
	int err = 0;

	err = icap_iores_write32(icap, IORES_DDR4_RESET_GATE, 0, 1);

	ICAP_INFO(icap, "%s ret %d", __func__, err);
	return err;
}

static int icap_release_ddr_gate_pin(struct icap *icap)
{
	int err = 0;

	err = icap_iores_write32(icap, IORES_DDR4_RESET_GATE, 0, 0);

	ICAP_INFO(icap, "%s ret %d", __func__, err);
	return err;
}

static int icap_calibrate_mig(struct platform_device *pdev, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	/* Wait for mig recalibration */
	if ((xocl_is_unified(xdev) || XOCL_DSA_XPR_ON(xdev)))
		err = calibrate_mig(icap, slot_id);

	return err;
}

static int icap_calib_and_check(struct platform_device *pdev, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	
	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	if (icap->data_retention)
		ICAP_WARN(icap, "xbutil reclock may not retain data");

	icap_calib(icap, slot_id, false);

	return icap_calibrate_mig(pdev, slot_id);
}

static int icap_verify_signed_signature(struct icap *icap, struct axlf *xclbin)
{
	int err = 0;

	if (xclbin->m_signature_length != -1) {
		int siglen = xclbin->m_signature_length;
		u64 origlen = xclbin->m_header.m_length - siglen;

		ICAP_INFO(icap, "signed xclbin detected");
		ICAP_INFO(icap, "original size: %llu, signature size: %d",
			origlen, siglen);

		/* restore original xclbin for verification */
		xclbin->m_signature_length = -1;
		xclbin->m_header.m_length = origlen;

		err = icap_verify_signature(icap, xclbin, origlen,
			((char *)xclbin) + origlen, siglen);
		if (err)
			goto out;
	} else if (icap->sec_level > ICAP_SEC_NONE) {
		ICAP_ERR(icap, "xclbin is not signed, rejected");
		err = -EKEYREJECTED;
		goto out;
	}

out:
	return err;
}

/* Create all urp subdevs */
static void icap_probe_urpdev_all(struct platform_device *pdev,
	struct axlf *xclbin, uint32_t slot_id)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int i, num_dev = 0;
	struct xocl_subdev *subdevs = NULL;

	/* create the rest of subdevs for both mgmt and user pf */
	icap_probe_urpdev(pdev, xclbin, &num_dev, &subdevs, slot_id);
	if (num_dev > 0) {
		for (i = 0; i < num_dev; i++) {
			subdevs[i].info.slot_idx = slot_id;
			(void) xocl_subdev_create(xdev, &subdevs[i].info);
			xocl_subdev_dyn_free(subdevs + i);
		}
	}

	if (subdevs)
		kfree(subdevs);
}

/* Create specific subdev */
static int icap_probe_urpdev_by_id(struct platform_device *pdev,
	struct axlf *xclbin, enum subdev_id devid, uint32_t slot_id)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int i, err = 0, num_dev = 0;
	struct xocl_subdev *subdevs = NULL;
	bool found = false;

	/* create specific subdev for both mgmt and user pf */
	icap_probe_urpdev(pdev, xclbin, &num_dev, &subdevs, slot_id);
	if (num_dev > 0) {
		for (i = 0; i < num_dev; i++) {
			if (subdevs[i].info.id != devid)
				continue;

			subdevs[i].info.slot_idx = slot_id;
			err = xocl_subdev_create(xdev, &subdevs[i].info);
			found = true;
			break;
		}
	}

	for (i = 0; i < num_dev; i++)
		xocl_subdev_dyn_free(subdevs + i);
	if (subdevs)
		kfree(subdevs);

	return found ? err : -ENODATA;
}

static int __icap_xclbin_download(struct icap *icap, struct axlf *xclbin, bool sref,
		uint32_t slot_id)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	int err = 0;
	bool retention = ((icap->data_retention & 0x1) == 0x1) && sref;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	err = icap_verify_signed_signature(icap, xclbin);
	if (err)
		goto out;

	err = icap_refresh_clock_freq(icap, xclbin, slot_id);
	if (err)
		goto out;

	if (retention) {
		err = icap_reset_ddr_gate_pin(icap);
		if (err == -ENODEV)
			ICAP_INFO(icap, "No ddr gate pin, err: %d", err);
		else if (err) {
			ICAP_ERR(icap, "not able to reset ddr gate pin, err: %d", err);
			goto out;
		}
	}

	/* xclbin generated for the flat shell contains MCS files which
	 * includes the accelerator these MCS files should have been already
	 * flashed into the device using xbmgmt tool we dont need to reprogram
	 * the xclbin for the FLAT shells.
	 * TODO: Currently , There is no way to check whether the programmed
	 * xclbin matches with this xclbin or not
	 */
	if (xclbin->m_header.m_mode != XCLBIN_FLAT) {
		err = icap_download_bitstream(icap, xclbin);
		if (err)
			goto out;
	} else {
		uuid_copy(&islot->icap_bitstream_uuid, &xclbin->m_header.uuid);
		ICAP_INFO(icap, "xclbin is generated for flat shell, dont need to program the bitstream ");
		err = xocl_clock_freq_rescaling(xdev, true);
		if (err)
			ICAP_ERR(icap, "not able to configure clocks, err: %d", err);
	}

	/* calibrate hbm and ddr should be performed when resources are ready */
	err = icap_create_post_download_subdevs(icap->icap_pdev, xclbin,
						slot_id);
	if (err)
		goto out;

	/*
	 * Perform the following exact sequence to avoid firewall trip.
	 *    1) ucs_control set to 0x1
	 *    2) DDR SRSR IP and MIG
	 *    3) MIG calibration
	 */
	/* If xclbin has clock metadata, refresh all clock subdevs */
	err = icap_probe_urpdev_by_id(icap->icap_pdev, xclbin,
				      XOCL_SUBDEV_CLOCK_WIZ, slot_id);
	if (!err)
		err = icap_probe_urpdev_by_id(icap->icap_pdev, xclbin,
			XOCL_SUBDEV_CLOCK_COUNTER, slot_id);

	if (!err) {
		err = icap_refresh_clock_freq(icap, xclbin, slot_id);
		if (err)
			ICAP_ERR(icap, "not able to refresh clock freq");
	}

	icap_calib(icap, slot_id, retention);

	if (retention) {
		err = icap_release_ddr_gate_pin(icap);
		if (err == -ENODEV)
			ICAP_INFO(icap, "No ddr gate pin");
		else if (err)
			ICAP_ERR(icap, "not able to release ddr gate pin");
	}

	err = icap_calibrate_mig(icap->icap_pdev, slot_id);

out:
	if (err && retention)
		icap_release_ddr_gate_pin(icap);
	ICAP_INFO(icap, "ret: %d", (int)err);
	return err;
}

static void icap_probe_urpdev(struct platform_device *pdev, struct axlf *xclbin,
	int *num_urpdev, struct xocl_subdev **urpdevs, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);

	icap_cache_bitstream_axlf_section(pdev, xclbin, PARTITION_METADATA,
					  slot_id);
	if (islot->partition_metadata) {
		*num_urpdev = xocl_fdt_parse_blob(xdev, islot->partition_metadata,
			icap_get_section_size(icap, PARTITION_METADATA, slot_id),
			urpdevs);
		ICAP_INFO(icap, "found %d sub devices", *num_urpdev);
	}
}

/*
 * freeze/free cmc via xmc subdev driver, the cmc is in mgmt pf.
 *
 * Before performing hardware configuratin changes, like downloading xclbin
 * then reset clock, mig etc., we should stop cmc first, in case cmc still
 * reach out the hardware that could cause potential firewall trip.
 *
 * After hardware configuration is done, we can restart the cmc by xmc free.
 */
static inline int icap_xmc_freeze(struct icap *icap)
{
	int err = 0;

	if (ICAP_PRIVILEGED(icap))
		err = xocl_xmc_freeze(xocl_get_xdev(icap->icap_pdev));

	return err == -ENODEV ? 0 : err;
}

static inline int icap_xmc_free(struct icap *icap)
{
	int err = 0;

	if (ICAP_PRIVILEGED(icap))
		err = xocl_xmc_free(xocl_get_xdev(icap->icap_pdev));

	return err == -ENODEV ? 0 : err;
}

static bool check_mem_topo_and_data_retention(struct icap *icap,
	struct axlf *xclbin, uint32_t slot_id)
{
	struct islot_info *islot = icap->slot_info[slot_id];
	struct mem_topology *mem_topo = NULL;
	const struct axlf_section_header *hdr =
		xrt_xclbin_get_section_hdr(xclbin, MEM_TOPOLOGY);
	uint64_t size = 0, offset = 0;

	if (!islot)
		return false;

	mem_topo = islot->mem_topo;
	if (!hdr || !mem_topo || !icap->data_retention)
		return false;

	size = hdr->m_sectionSize;
	offset = hdr->m_sectionOffset;

	/* Data retention feature ONLY works if the xclbins have identical mem_topology
	 * or it will lead to hardware failure.
	 * If the incoming xclbin has different mem_topology, disable data retention feature
	 */

	if ((size != sizeof_sect(mem_topo, m_mem_data)) ||
		    memcmp(((char *)xclbin)+offset, mem_topo, size)) {
		ICAP_WARN(icap, "Data retention is enabled. "
			"However, the incoming mem_topology doesn't match, "
			"data in device memory can not be retained");
		return false;
	}

	return true;
}

static void icap_cache_max_host_mem_aperture(struct icap *icap,
					     uint32_t slot_id)
{
	int i = 0;
	struct islot_info *islot = icap->slot_info[slot_id];
	struct mem_topology *mem_topo = islot->mem_topo;

	if (!mem_topo)
		return;

	islot->max_host_mem_aperture = 0;
	for ( i=0; i< mem_topo->m_count; ++i) {
		if (!mem_topo->m_mem_data[i].m_used)
			continue;

		if (convert_mem_tag(mem_topo->m_mem_data[i].m_tag) == MEM_TAG_HOST)
			islot->max_host_mem_aperture = mem_topo->m_mem_data[i].m_size << 10;
	}

	return;
}

/*
 * Axlf xclbin download flow on user pf:
 *   1) after xclbin validation, remove all URP subdevs;
 *   2) cache mem_topology first (see comments);
 *   3) request peer(aka. mgmt pf to do real download);
 *   4) cache and create subdevs, including URP subdevs;
 *   5) if fail, set uuid to NULL to allow next download;
 * TODO: ignoring errors for 4) now, need more justification.
 */
static int __icap_download_bitstream_user(struct platform_device *pdev,
	struct axlf *xclbin, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int err = 0;

	/* Using slot handle to unregister CUs. CU subdev will be destroyed */
	err = xocl_unregister_cus(xdev, slot_id);
	if (err && (err != -ENODEV))
		goto done;

	xocl_subdev_destroy_by_slot(xdev, slot_id);

	err = __icap_peer_xclbin_download(icap, xclbin, slot_id);

	if (err)
		goto done;

	/* TODO: ignoring any return value or just -ENODEV? */
	icap_cache_bitstream_axlf_section(pdev, xclbin, IP_LAYOUT, slot_id);
	icap_cache_bitstream_axlf_section(pdev, xclbin, SOFT_KERNEL, slot_id);
	icap_cache_bitstream_axlf_section(pdev, xclbin, CONNECTIVITY, slot_id);
	icap_cache_bitstream_axlf_section(pdev, xclbin,
		DEBUG_IP_LAYOUT, slot_id);

	icap_cache_clock_freq_topology(icap, xclbin, slot_id);

	/* Create cu/scu subdev by slot */
	err = xocl_register_cus(xdev, slot_id, &xclbin->m_header.uuid,
				islot->ip_layout, islot->ps_kernel);
	if (err)
		goto done;

	icap_create_subdev_debugip(pdev, slot_id);

	/* Initialize Group Topology and Group Connectivity */
	icap_cache_bitstream_axlf_section(pdev, xclbin, ASK_GROUP_CONNECTIVITY,
					  slot_id);

	icap_probe_urpdev_all(pdev, xclbin, slot_id);
	xocl_subdev_create_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);
done:
	/* TODO: link this comment to specific function in xocl_ioctl.c */
	/* has to create mem topology even with failure case
	 * please refer the comment in xocl_ioctl.c
	 * without creating mem topo, memory corruption could happen
	 */
	icap_cache_bitstream_axlf_section(pdev, xclbin, MEM_TOPOLOGY, slot_id);
	icap_cache_bitstream_axlf_section(pdev, xclbin, ASK_GROUP_TOPOLOGY,
					  slot_id);

	icap_cache_max_host_mem_aperture(icap, slot_id);

	if (err) {
		uuid_copy(&islot->icap_bitstream_uuid, &uuid_null);
	} else {
		icap_create_subdev_ip_layout(pdev, slot_id);
		/* Remember "this" bitstream, so avoid re-download next time. */
		uuid_copy(&islot->icap_bitstream_uuid, &xclbin->m_header.uuid);
	}
	return err;
}

/*
 * Axlf xclbin download flow on mgmt pf:
 *    1) after xclbin validation, freeze(isolate) xmc;
 *    2) save calib;
 *    3) remove all URP subdevs;
 *    4) save retention flag before caching mem_topology and ip_layout;
 *    5) verify signed signature;
 *    6) re-config clock;
 *    7) reset ddr pin for retention only;
 *    8) perform icap download for non-flat design;
 *    9) create SRSR subdev;
 *    10) create CLOCK subdev, re-config clock;
 *    11) perform mig calibration;
 *    12) create subdev ip_layout;
 *    13) create subdev dna;
 *    14) create subdev from xclbin;
 *    15) create URP subdevs;
 *    16) free xmc;
 * NOTE: any steps above can fail, return err and set uuid to NULL.
 */
static int __icap_download_bitstream_mgmt(struct platform_device *pdev,
	struct axlf *xclbin, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	bool sref = false;
	int err = 0;

	err = icap_xmc_freeze(icap);
	if (err)
		return err;

	/* TODO: why void, ignoring any errors */
	icap_save_calib(icap, slot_id);

	/* remove any URP subdev before downloading xclbin */
	xocl_subdev_destroy_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);

	/* Check the incoming mem topology with the current one before overwrite */
	sref = check_mem_topo_and_data_retention(icap, xclbin, slot_id);
	icap_cache_bitstream_axlf_section(pdev, xclbin, MEM_TOPOLOGY, slot_id);
	icap_cache_bitstream_axlf_section(pdev, xclbin, IP_LAYOUT, slot_id);

	err = __icap_xclbin_download(icap, xclbin, sref, slot_id);
	if (err)
		goto done;

	err = icap_create_subdev_ip_layout(pdev, slot_id);
	if (err)
		goto done;

	err = icap_create_subdev_dna(pdev, xclbin);
	if (err)
		goto done;

	/* Initialize Group Topology and Group Connectivity */
	icap_cache_bitstream_axlf_section(pdev, xclbin, ASK_GROUP_TOPOLOGY,
					  slot_id);
	icap_cache_bitstream_axlf_section(pdev, xclbin, ASK_GROUP_CONNECTIVITY,
					  slot_id);

	icap_probe_urpdev_all(pdev, xclbin, slot_id);
	xocl_subdev_create_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);

	/* Only when everything has been successfully setup, then enable xmc */
	if (!err)
		err = icap_xmc_free(icap);

done:
	if (err) {
		uuid_copy(&islot->icap_bitstream_uuid, &uuid_null);
	} else {
		/* Remember "this" bitstream, so avoid re-download next time. */
		uuid_copy(&islot->icap_bitstream_uuid, &xclbin->m_header.uuid);
	}
	return err;

}

static int __icap_download_bitstream_axlf(struct platform_device *pdev,
	struct axlf *xclbin, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	ICAP_INFO(icap, "incoming xclbin: %pUb\non device xclbin: %pUb",
		&xclbin->m_header.uuid, &islot->icap_bitstream_uuid);

	return ICAP_PRIVILEGED(icap) ?
		__icap_download_bitstream_mgmt(pdev, xclbin, slot_id) :
		__icap_download_bitstream_user(pdev, xclbin, slot_id);
}

/*
 * Both icap user and mgmt subdev call into this function, it should
 * only perform common validation, then call into different function
 * for user icap or mgmt icap.
 */
static int icap_download_bitstream_axlf(struct platform_device *pdev,
	const void *u_xclbin, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct axlf *xclbin = (struct axlf *)u_xclbin;
	int err = 0;
	struct islot_info *islot = NULL;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	const struct axlf_section_header *header = NULL;
	const void *bitstream = NULL;
	const void *bitstream_part_pdi = NULL;

	/* This is the first entry for slot in icap. 
	 * Hence allocate required memory here
	 */
	err = icap_slot_init(icap, slot_id);
	if (err)
		return err;

	err = icap_xclbin_wr_lock(icap, slot_id);
	if (err) {
		return err;
	}

	mutex_lock(&icap->icap_lock);

	/* Sanity check xclbin. */
	if (memcmp(xclbin->m_magic, ICAP_XCLBIN_V2, sizeof(ICAP_XCLBIN_V2))) {
		ICAP_ERR(icap, "invalid xclbin magic string");
		err = -EINVAL;
		goto done;
	}

	header = xrt_xclbin_get_section_hdr(xclbin, PARTITION_METADATA);
	bitstream = xrt_xclbin_get_section_hdr(xclbin, BITSTREAM);
	bitstream_part_pdi = xrt_xclbin_get_section_hdr(xclbin, BITSTREAM_PARTIAL_PDI);
	/*
	 * don't check uuid if the xclbin is a lite one
	 * the lite xclbin will not have BITSTREAM
	 * we need the SOFT_KERNEL section since the OBJ and METADATA are
	 * coupled together.
	 * The OBJ (soft kernel) is not needed, we can use xclbinutil to
	 * add a temp small OBJ to reduce the lite xclbin size
	 */
	islot = icap->slot_info[slot_id];
	if (header && (bitstream || bitstream_part_pdi)) {
		ICAP_INFO(icap, "check interface uuid");
		err = xocl_fdt_check_uuids(xdev,
				(const void *)XDEV(xdev)->fdt_blob,
				(const void *)((char *)xclbin +
				header->m_sectionOffset));
		if (err) {
			ICAP_ERR(icap, "interface uuids do not match");
			err = -EINVAL;
			goto done;
		}

		/* Set this slot is as a PL Slot */
		islot->pl_slot = true;
	}

	/*
	 * If the previous frequency was very high and we load an incompatible
	 * bitstream it may damage the hardware!
	 *
	 * But if Platform contain all fixed clocks, xclbin doesnt contain
	 * CLOCK_FREQ_TOPOLOGY section as there are no clocks to configure,
	 * downloading xclbin should be succesful in this case.
	 */
	header = xrt_xclbin_get_section_hdr(xclbin, CLOCK_FREQ_TOPOLOGY);
	if (!header) {
		ICAP_WARN(icap, "CLOCK_FREQ_TOPOLOGY doesn't exist. XRT is not configuring any clocks");
	}

	if (xocl_xrt_version_check(xdev, xclbin, true)) {
		ICAP_ERR(icap, "xclbin isn't supported by current XRT");
		err = -EINVAL;
		goto done;
	}
	if (!xocl_verify_timestamp(xdev,
		xclbin->m_header.m_featureRomTimeStamp)) {
		ICAP_ERR(icap, "TimeStamp of ROM did not match Xclbin");
		err = -EOPNOTSUPP;
		goto done;
	}
	if (icap_bitstream_in_use(icap, slot_id)) {
		ICAP_ERR(icap, "bitstream is in-use, can't change");
		err = -EBUSY;
		goto done;
	}

	err = __icap_download_bitstream_axlf(pdev, xclbin, slot_id);

done:
	mutex_unlock(&icap->icap_lock);
	icap_xclbin_wr_unlock(icap, slot_id);
	ICAP_INFO(icap, "err: %d", err);

	return err;
}

/*
 * On x86_64, reset hwicap by loading special bitstream sequence which
 * forces the FPGA to reload from PROM.
 */
static int icap_reset_bitstream(struct platform_device *pdev)
{
/*
 * Booting FPGA from PROM
 * http://www.xilinx.com/support/documentation/user_guides/ug470_7Series_Config.pdf
 * Table 7.1
 */
#define DUMMY_WORD         0xFFFFFFFF
#define SYNC_WORD          0xAA995566
#define TYPE1_NOOP         0x20000000
#define TYPE1_WRITE_WBSTAR 0x30020001
#define WBSTAR_ADD10       0x00000000
#define WBSTAR_ADD11       0x01000000
#define TYPE1_WRITE_CMD    0x30008001
#define IPROG_CMD          0x0000000F
#define SWAP_ENDIAN_32(x)						\
	(unsigned)((((x) & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | \
		   (((x) & 0x0000FF00) << 8)  | (((x) & 0x000000FF) << 24))
	/*
	 * The bitstream is expected in big endian format
	 */
	const unsigned fpga_boot_seq[] = {				\
		SWAP_ENDIAN_32(DUMMY_WORD),				\
		SWAP_ENDIAN_32(SYNC_WORD),				\
		SWAP_ENDIAN_32(TYPE1_NOOP),				\
		SWAP_ENDIAN_32(TYPE1_WRITE_CMD),			\
		SWAP_ENDIAN_32(IPROG_CMD),				\
		SWAP_ENDIAN_32(TYPE1_NOOP),				\
		SWAP_ENDIAN_32(TYPE1_NOOP)				\
	};
	struct icap *icap = platform_get_drvdata(pdev);
	int i;

	/* Can only be done from mgmt pf. */
	if (!ICAP_PRIVILEGED(icap))
		return -EPERM;

	mutex_lock(&icap->icap_lock);

	if (icap_any_bitstream_in_use(icap)) {
		mutex_unlock(&icap->icap_lock);
		ICAP_ERR(icap, "bitstream is locked, can't reset");
		return -EBUSY;
	}

	for (i = 0; i < ARRAY_SIZE(fpga_boot_seq); i++) {
		unsigned value = be32_to_cpu(fpga_boot_seq[i]);

		reg_wr(&icap->icap_regs->ir_wfv, value);
	}
	reg_wr(&icap->icap_regs->ir_cr, 0x1);

	msleep(4000);

	mutex_unlock(&icap->icap_lock);

	ICAP_INFO(icap, "reset bitstream is done");
	return 0;
}

static int icap_lock_bitstream(struct platform_device *pdev, const xuid_t *id,
		uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	int ref = 0, err = 0;

	if (!islot)
		return -EINVAL;

	/* ioctl arg might be passed in with NULL uuid */
	if (uuid_is_null(id)) {
		ICAP_WARN(icap, "NULL uuid.");
		return -EINVAL;
	}

	err = icap_xclbin_rd_lock(icap, slot_id);
	if (err) {
		ICAP_ERR(icap, "Failed to get on device uuid, device busy");
		return err;
	}

	mutex_lock(&icap->icap_lock);

	if (!uuid_equal(id, &islot->icap_bitstream_uuid)) {
		ICAP_ERR(icap, "lock bitstream %pUb failed, on device: %pUb",
			id, &islot->icap_bitstream_uuid);
		err = -EBUSY;
		goto done;
	}

	ref = islot->icap_bitstream_ref;
	islot->icap_bitstream_ref++;
	ICAP_INFO(icap, "bitstream %pUb locked, ref=%d", id,
		islot->icap_bitstream_ref);

done:
	mutex_unlock(&icap->icap_lock);
	icap_xclbin_rd_unlock(icap, slot_id);
	return err;
}

static int icap_unlock_bitstream(struct platform_device *pdev, const xuid_t *id,
		uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	int err = 0;
	xuid_t on_slot_uuid;

	if (id == NULL)
		id = &uuid_null;

	err = icap_xclbin_rd_lock(icap, slot_id);
	if (err) {
		ICAP_ERR(icap, "Failed to get on device uuid, device busy");
		return err;
	}

	mutex_lock(&icap->icap_lock);

	uuid_copy(&on_slot_uuid, &islot->icap_bitstream_uuid);

	if (uuid_is_null(id)) /* force unlock all */
		islot->icap_bitstream_ref = 0;
	else if (uuid_equal(id, &on_slot_uuid))
		islot->icap_bitstream_ref--;
	else
		err = -EINVAL;

	if (err == 0) {
		ICAP_INFO(icap, "bitstream %pUb unlocked, ref=%d",
			&on_slot_uuid, islot->icap_bitstream_ref);
	} else {
		ICAP_ERR(icap, "unlock bitstream %pUb failed, on device: %pUb",
			id, &on_slot_uuid);
		goto done;
	}

done:
	mutex_unlock(&icap->icap_lock);
	icap_xclbin_rd_unlock(icap, slot_id);
	return err;
}

static bool icap_bitstream_is_locked(struct platform_device *pdev,
		uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];

	if (!islot)
		return false;

	/* This operation let caller glance at if bitstream is locked */
	return icap_bitstream_in_use(icap, slot_id);
}

static int icap_cache_ps_kernel_axlf_section(const struct axlf *xclbin,
	void **data)
{
	struct axlf_section_header *header = NULL;
	struct ps_kernel_node *pnode;
	char *blob = (char *)xclbin;
	int count;
	int idx = 0;

	count = xrt_xclbin_get_section_num(xclbin, SOFT_KERNEL);
	if (count == 0)
		return -EINVAL;

	*data = vzalloc(sizeof(struct ps_kernel_node) +
	    sizeof(struct ps_kernel_data) * (count - 1));
	if (*data == NULL)
		return -ENOMEM;

	pnode = (struct ps_kernel_node *)(*data);
	pnode->pkn_count = count;

	header = xrt_xclbin_get_section_hdr_next(xclbin, SOFT_KERNEL,
	    header);
	while (header) {
		struct soft_kernel *sp =
		    (struct soft_kernel *)&blob[header->m_sectionOffset];
		char *begin = (char *)sp;

		strncpy(pnode->pkn_data[idx].pkd_sym_name,
		    begin + sp->mpo_symbol_name,
		    PS_KERNEL_NAME_LENGTH - 1);
		pnode->pkn_data[idx].pkd_num_instances =
		    sp->m_num_instances;

		idx++;
		header = xrt_xclbin_get_section_hdr_next(xclbin,
		    SOFT_KERNEL, header);
	}

	return 0;
}

static int icap_cache_bitstream_axlf_section(struct platform_device *pdev,
	const struct axlf *xclbin, enum axlf_section_kind kind,
	uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	long err = 0;
	uint64_t section_size = 0, sect_sz = 0;
	void **target = NULL;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	if (memcmp(xclbin->m_magic, ICAP_XCLBIN_V2, sizeof(ICAP_XCLBIN_V2)))
		return -EINVAL;

	switch (kind) {
	case IP_LAYOUT:
		target = (void **)&islot->ip_layout;
		break;
	case SOFT_KERNEL:
		target = (void **)&islot->ps_kernel;
		break;
	case MEM_TOPOLOGY:
		target = (void **)&islot->mem_topo;
		break;
	case ASK_GROUP_TOPOLOGY:
		target = (void **)&islot->group_topo;
		break;
	case DEBUG_IP_LAYOUT:
		target = (void **)&islot->debug_layout;
		break;
	case CONNECTIVITY:
		target = (void **)&islot->connectivity;
		break;
	case ASK_GROUP_CONNECTIVITY:
		target = (void **)&islot->group_connectivity;
		break;
	case CLOCK_FREQ_TOPOLOGY:
		target = (void **)&islot->xclbin_clock_freq_topology;
		break;
	case PARTITION_METADATA:
		target = (void **)&islot->partition_metadata;
		break;
	default:
		return -EINVAL;
	}
	if (target && *target) {
		vfree(*target);
		*target = NULL;
	}

	if (kind == SOFT_KERNEL) {
		err = icap_cache_ps_kernel_axlf_section(xclbin, target);
		goto done;
	}

	err = xrt_xclbin_get_section(xclbin, kind, target, &section_size);
	if (err != 0) {
		/* If group topology or group connectivity doesn't exists then use the
		 * mem topology or connectivity respectively section for the same.
		 */
		if (kind == ASK_GROUP_TOPOLOGY)
			err = xrt_xclbin_get_section(xclbin, MEM_TOPOLOGY,
							target, &section_size);
		else if (kind == ASK_GROUP_CONNECTIVITY)
			err = xrt_xclbin_get_section(xclbin, CONNECTIVITY,
							target, &section_size);
		if (err != 0) {
			ICAP_ERR(icap, "get section err: %ld", err);
			goto done;
		}
	}
	sect_sz = icap_get_section_size(icap, kind, slot_id);
	if (sect_sz > section_size) {
		err = -EINVAL;
		goto done;
	}

	if (kind == MEM_TOPOLOGY || kind == ASK_GROUP_TOPOLOGY) {
		struct mem_topology *mem_topo = *target;
		int i;

		for (i = 0; i< mem_topo->m_count; ++i) {
			if (!(convert_mem_tag(mem_topo->m_mem_data[i].m_tag) == MEM_TAG_HOST) ||
			    mem_topo->m_mem_data[i].m_used)
				continue;

			xocl_m2m_host_bank(xdev,
				&(mem_topo->m_mem_data[i].m_base_address),
				&(mem_topo->m_mem_data[i].m_size),
				&(mem_topo->m_mem_data[i].m_used));
		}
		/* Xclbin binary has been adjusted as a workaround of Bios Limitation of some machine 
		 * We won't be able to retain the device memory because of the limitation
		 */
		xocl_p2p_adjust_mem_topo(xdev, mem_topo);
	}
done:
	if (err) {
		if (target && *target) {
			vfree(*target);
			*target = NULL;
		}
		ICAP_INFO(icap, "skip kind %d(%s), return code %ld", kind,
			xrt_xclbin_kind_to_string(kind), err);
	} else {
		ICAP_INFO(icap, "found kind %d(%s)", kind,
			xrt_xclbin_kind_to_string(kind));
	}

	return err;
}

static uint64_t icap_get_data_nolock(struct platform_device *pdev,
	enum data_kind kind)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct icap *icap = platform_get_drvdata(pdev);
	ktime_t now = ktime_get_boottime();
	uint64_t target = 0;

	if (!ICAP_PRIVILEGED(icap)) {

		if (ktime_compare(now, icap->cache_expires) > 0)
			icap_read_from_peer(pdev);

		switch (kind) {
		case CLOCK_FREQ_0:
			target = icap->cache.freq_0;
			break;
		case CLOCK_FREQ_1:
			target = icap->cache.freq_1;
			break;
		case CLOCK_FREQ_2:
			target = icap->cache.freq_2;
			break;
		case FREQ_COUNTER_0:
			target = icap->cache.freq_cntr_0;
			break;
		case FREQ_COUNTER_1:
			target = icap->cache.freq_cntr_1;
			break;
		case FREQ_COUNTER_2:
			target = icap->cache.freq_cntr_2;
			break;
		case IDCODE:
			target = icap->cache.idcode;
			break;
		case PEER_UUID:
			target = (uint64_t)&icap->cache.uuid;
			break;
		case MIG_CALIB:
			target = (uint64_t)icap->cache.mig_calib;
			break;
		case DATA_RETAIN:
			target = (uint64_t)icap->cache.data_retention;
			break;
		default:
			break;
		}
	} else {
		unsigned short freq = 0;

		switch (kind) {
		case IDCODE:
			target = icap->idcode;
			break;
		case CLOCK_FREQ_0:
			if (!xocl_clock_get_freq_by_id(xdev, 0, &freq, 0))
				target = freq;
			break;
		case CLOCK_FREQ_1:
			if (!xocl_clock_get_freq_by_id(xdev, 0, &freq, 1))
				target = freq;
			break;
		case CLOCK_FREQ_2:
			if (!xocl_clock_get_freq_by_id(xdev, 0, &freq, 2))
				target = freq;
			break;
		case FREQ_COUNTER_0:
			target = icap_get_clock_frequency_counter_khz(icap, 0);
			break;
		case FREQ_COUNTER_1:
			target = icap_get_clock_frequency_counter_khz(icap, 1);
			break;
		case FREQ_COUNTER_2:
			target = icap_get_clock_frequency_counter_khz(icap, 2);
			break;
		case MIG_CALIB:
			target = mig_calibration_done(icap);
			break;
		case EXP_BMC_VER:
			target = (uint64_t)icap->bmc_header.m_version;
			break;
		case DATA_RETAIN:
			target = (uint64_t)icap->data_retention;
			break;
		default:
			break;
		}
	}
	return target;
}
static uint64_t icap_get_data(struct platform_device *pdev,
	enum data_kind kind)
{
	struct icap *icap = platform_get_drvdata(pdev);
	uint64_t target = 0;

	mutex_lock(&icap->icap_lock);
	target = icap_get_data_nolock(pdev, kind);
	mutex_unlock(&icap->icap_lock);
	return target;
}

static void icap_put_xclbin_metadata(struct platform_device *pdev,
		uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];

	if (!islot)
		return;

	icap_xclbin_rd_unlock(icap, slot_id);
}

static int icap_get_xclbin_metadata(struct platform_device *pdev,
	enum data_kind kind, void **buf, uint32_t slot_id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct islot_info *islot = icap->slot_info[slot_id];
	int err = 0;

	if (!islot)
		return err;

	err = icap_xclbin_rd_lock(icap, slot_id);
	if (err)
		return err;

	mutex_lock(&icap->icap_lock);

	switch (kind) {
	case IPLAYOUT_AXLF:
		*buf = islot->ip_layout;
		break;
	case GROUPTOPO_AXLF:
		*buf = islot->group_topo;
		break;
	case MEMTOPO_AXLF:
		*buf = islot->mem_topo;
		break;
	case DEBUG_IPLAYOUT_AXLF:
		*buf = islot->debug_layout;
		break;
	case GROUPCONNECTIVITY_AXLF:
		*buf = islot->group_connectivity;
		break;
	case CONNECTIVITY_AXLF:
		*buf = islot->connectivity;
		break;
	case XCLBIN_UUID:
		*buf = &islot->icap_bitstream_uuid;
		break;
	case SOFT_KERNEL:
		*buf = islot->ps_kernel;
		break;
	default:
		break;
	}
	mutex_unlock(&icap->icap_lock);
	return 0;
}

static void icap_refresh_addrs(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	icap->icap_state = xocl_iores_get_base(xdev, IORES_MEMCALIB);
	ICAP_INFO(icap, "memcalib @ %lx", (unsigned long)icap->icap_state);

	icap->icap_config_engine = xocl_iores_get_base(xdev, IORES_ICAP_RESET);
	ICAP_INFO(icap, "icap_reset @ %lx", (unsigned long)icap->icap_config_engine);
}

static int icap_offline(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	uint32_t slot_id = 0;
	struct islot_info *islot = NULL;

	xocl_drvinst_kill_proc(platform_get_drvdata(pdev));

	sysfs_remove_group(&pdev->dev.kobj, &icap_attr_group);
	for (slot_id = 0; slot_id < MAX_SLOT_SUPPORT; slot_id++) {
                islot = icap->slot_info[slot_id];
                if (!islot)
                        continue;

                /* Clock frequence is only related to PL Slots */
                if (islot->pl_slot)
                        xclbin_free_clock_freq_topology(icap, slot_id);

                icap_clean_bitstream_axlf(pdev, slot_id);
        }

	return 0;
}

static int icap_online(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int ret;

	icap_refresh_addrs(pdev);
	ret = sysfs_create_group(&pdev->dev.kobj, &icap_attr_group);
	if (ret)
		ICAP_ERR(icap, "create icap attrs failed: %d", ret);

	return ret;
}

/* Kernel APIs exported from this sub-device driver. */
static struct xocl_icap_funcs icap_ops = {
	.offline_cb = icap_offline,
	.online_cb = icap_online,
	.reset_axi_gate = platform_reset_axi_gate,
	.reset_bitstream = icap_reset_bitstream,
	.download_boot_firmware = icap_download_boot_firmware,
	.download_bitstream_axlf = icap_download_bitstream_axlf,
	.download_rp = icap_download_rp,
	.post_download_rp = icap_post_download_rp,
	.ocl_get_freq = icap_ocl_get_freqscaling,
	.ocl_update_clock_freq_topology = icap_ocl_update_clock_freq_topology,
	.xclbin_validate_clock_req = icap_xclbin_validate_clock_req,
	.ocl_lock_bitstream = icap_lock_bitstream,
	.ocl_unlock_bitstream = icap_unlock_bitstream,
	.ocl_bitstream_is_locked = icap_bitstream_is_locked,
	.get_data = icap_get_data,
	.get_xclbin_metadata = icap_get_xclbin_metadata,
	.put_xclbin_metadata = icap_put_xclbin_metadata,
	.mig_calibration = icap_calibrate_mig,
	.clean_bitstream = icap_clean_bitstream_axlf,
};

static ssize_t clock_freqs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;
	int i, err;
	int st = 0;
	struct islot_info *islot = NULL;
	u32 freq_counter, freq, request_in_khz, tolerance;

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (islot == NULL)
			continue;

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return cnt;

		mutex_lock(&icap->icap_lock);
		for (i = 0; i < ICAP_MAX_NUM_CLOCKS; i++) {
			freq = icap_get_ocl_frequency(icap, i);

			if (!uuid_is_null(&islot->icap_bitstream_uuid)) {
				freq_counter = icap_get_clock_frequency_counter_khz(icap, i);

				request_in_khz = freq*1000;
				tolerance = freq*50;

			if (abs(freq_counter-request_in_khz) > tolerance)
				ICAP_INFO(icap, "Frequency mismatch, Should be %u khz, Now is %ukhz", request_in_khz, freq_counter);
			cnt += sprintf(buf + cnt, "%d\n", DIV_ROUND_CLOSEST(freq_counter, 1000));
		} else
				cnt += sprintf(buf + cnt, "%d\n", freq);
		}

		mutex_unlock(&icap->icap_lock);
		icap_xclbin_rd_unlock(icap, st);
	}

	return cnt;
}
static DEVICE_ATTR_RO(clock_freqs);

static ssize_t clock_freqs_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	struct islot_info *islot = NULL;
	int st = 0;
	ssize_t cnt = 0;
	int i, err;
	unsigned short freq;

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
			if (islot == NULL)
				continue;

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return cnt;

		mutex_lock(&icap->icap_lock);
		for (i = 0; i < ICAP_MAX_NUM_CLOCKS; i++) {
			freq = 0;
			xclbin_get_ocl_frequency_max_min(icap, i, &freq, NULL,
							 st);
			cnt += sprintf(buf + cnt, "%d\n", freq);
		}
		mutex_unlock(&icap->icap_lock);

		icap_xclbin_rd_unlock(icap, st);
	}
	return cnt;
}
static DEVICE_ATTR_RO(clock_freqs_max);

static ssize_t clock_freqs_min_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	struct islot_info *islot = NULL;
	int st = 0;
	ssize_t cnt = 0;
	int i, err;
	unsigned short freq;

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (islot == NULL)
			continue;

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return cnt;

		mutex_lock(&icap->icap_lock);
		for (i = 0; i < ICAP_MAX_NUM_CLOCKS; i++) {
			freq = 0;
			xclbin_get_ocl_frequency_max_min(icap, i, NULL, &freq,
							 st);
			cnt += sprintf(buf + cnt, "%d\n", freq);
		}
		mutex_unlock(&icap->icap_lock);

		icap_xclbin_rd_unlock(icap, st);
	}
	return cnt;
}
static DEVICE_ATTR_RO(clock_freqs_min);

static ssize_t idcode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;
	uint32_t val;

	mutex_lock(&icap->icap_lock);
	if (ICAP_PRIVILEGED(icap)) {
		cnt = sprintf(buf, "0x%x\n", icap->idcode);
	} else {
		val = icap_get_data_nolock(to_platform_device(dev), IDCODE);
		cnt = sprintf(buf, "0x%x\n", val);
	}
	mutex_unlock(&icap->icap_lock);

	return cnt;
}
static DEVICE_ATTR_RO(idcode);

static ssize_t cache_expire_secs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	u64 val = 0;

	mutex_lock(&icap->icap_lock);
	if (!ICAP_PRIVILEGED(icap))
		val = icap->cache_expire_secs;

	mutex_unlock(&icap->icap_lock);
	return sprintf(buf, "%llu\n", val);
}

static ssize_t cache_expire_secs_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	u64 val;

	mutex_lock(&icap->icap_lock);
	if (kstrtou64(buf, 10, &val) == -EINVAL || val > 10) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0 ~ 10] > cache_expire_secs");
		return -EINVAL;
	}

	if (!ICAP_PRIVILEGED(icap))
		icap->cache_expire_secs = val;

	mutex_unlock(&icap->icap_lock);
	return count;
}
static DEVICE_ATTR_RW(cache_expire_secs);

#ifdef	KEY_DEBUG
/* Test code for now, will remove later. */
void icap_key_test(struct icap *icap)
{
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(icap->icap_pdev);
	char *sig_buf = NULL, text_buf = NULL;
	size_t sig_len = 0, text_len = 0;
	int err = 0;

	err = xocl_request_firmware(&pcidev->dev, "xilinx/signature", &sig_buf, &sig_len);
	if (err) {
		ICAP_ERR(icap, "can't load signature: %d", err);
		goto done;
	}
	err = xocl_request_firmware(&pcidev->dev, "xilinx/text", &pcidev->dev,
				    &text_buf, &text_len);
	if (err) {
		ICAP_ERR(icap, "can't load text: %d", err);
		goto done;
	}

	err = icap_verify_signature(icap, text_buf, text_len,
		sig_buf, sig_len);
	if (err) {
		ICAP_ERR(icap, "Failed to verify data file");
		goto done;
	}

	ICAP_INFO(icap, "Successfully verified data file!!!");

done:
	vfree(sig_buf);
	vfree(text_buf);
}
#endif

static ssize_t sec_level_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	u64 val = 0;

	mutex_lock(&icap->icap_lock);
	if (!ICAP_PRIVILEGED(icap))
		val = ICAP_SEC_NONE;
	else
		val = icap->sec_level;
	mutex_unlock(&icap->icap_lock);
	return sprintf(buf, "%llu\n", val);
}

static ssize_t sec_level_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	u64 val;
	int ret = count;

	if (kstrtou64(buf, 10, &val) == -EINVAL || val > ICAP_SEC_MAX) {
		xocl_err(&to_platform_device(dev)->dev,
			"max sec level is %d", ICAP_SEC_MAX);
		return -EINVAL;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0)
	if (val == 0)
		return ret;
	/* Can't enable xclbin signature verification. */
	ICAP_ERR(icap,
		"verifying signed xclbin is not supported with < 4.7.0 kernel");
	return -EOPNOTSUPP;
#else
	mutex_lock(&icap->icap_lock);

	if (ICAP_PRIVILEGED(icap)) {
#if defined(EFI_SECURE_BOOT)
		if (!efi_enabled(EFI_SECURE_BOOT)) {
			icap->sec_level = val;
		} else {
			ICAP_ERR(icap,
				"security level is fixed in secure boot");
			ret = -EROFS;
		}
#else
		icap->sec_level = val;
#endif

#ifdef	KEY_DEBUG
		icap_key_test(icap);
#endif
	}

	mutex_unlock(&icap->icap_lock);

	return ret;
#endif
}
static DEVICE_ATTR_RW(sec_level);

static ssize_t reader_cnt_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	struct islot_info *islot = NULL;
	u64 val = 0;
	ssize_t cnt = 0;
	int st = 0;

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (islot == NULL)
			continue;

		mutex_lock(&icap->icap_lock);
		val = islot->reader_ref;
		cnt += sprintf(buf + cnt, "%d %llu\n", st, val);
		mutex_unlock(&icap->icap_lock);
	}

	return cnt;
}
static DEVICE_ATTR_RO(reader_cnt);


static ssize_t data_retention_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0, ack;
	int err;

	if (!ICAP_PRIVILEGED(icap)){
		val = icap_get_data(to_platform_device(dev), DATA_RETAIN);
		goto done;
	}

	err = icap_iores_read32(icap, IORES_DDR4_RESET_GATE, 0, &ack);
	if (err)
		return err;

	mutex_lock(&icap->icap_lock);
	val = icap->data_retention;
	mutex_unlock(&icap->icap_lock);
done:
	return sprintf(buf, "%u\n", val);
}

static ssize_t data_retention_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	u32 val, ack;
	int err = 0;

	if (!ICAP_PRIVILEGED(icap))
		goto done;

	/* Must have ddr gate pin */
	err = icap_iores_read32(icap, IORES_DDR4_RESET_GATE, 0, &ack);
	if (err) {
		xocl_err(&to_platform_device(dev)->dev,
			"%d", err);
		return err;
	}

	if (kstrtou32(buf, 10, &val) == -EINVAL || val > 2) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo [0 ~ 1] > data_retention");
		return -EINVAL;
	}

	mutex_lock(&icap->icap_lock);
	icap->data_retention = val;
	mutex_unlock(&icap->icap_lock);
done:
	return count;
}
static DEVICE_ATTR_RW(data_retention);

static ssize_t max_host_mem_aperture_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	struct islot_info *islot = NULL;
	u64 val = 0;
        ssize_t cnt = 0;
        int st = 0;

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (islot == NULL)
			continue;

		mutex_lock(&icap->icap_lock);
		val = islot->max_host_mem_aperture;
		cnt += sprintf(buf + cnt, "%d %llu\n", st, val);
		mutex_unlock(&icap->icap_lock);
	}

	return sprintf(buf, "%llu\n", val);
}
static DEVICE_ATTR_RO(max_host_mem_aperture);

static struct attribute *icap_attrs[] = {
	&dev_attr_clock_freqs.attr,
	&dev_attr_idcode.attr,
	&dev_attr_cache_expire_secs.attr,
	&dev_attr_sec_level.attr,
	&dev_attr_clock_freqs_max.attr,
	&dev_attr_clock_freqs_min.attr,
	&dev_attr_reader_cnt.attr,
	&dev_attr_data_retention.attr,
	&dev_attr_max_host_mem_aperture.attr,
	NULL,
};

/*- Debug IP_layout-- */
static ssize_t icap_read_debug_ip_layout(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap;
	struct islot_info *islot = NULL;
	u32 nread = 0;
	u32 f_nread = 0;
	size_t size = 0;
	int err = 0;
        int st = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj,
				struct device, kobj));

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (!islot || !islot->debug_layout)
			continue;

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return f_nread;

		size = sizeof_sect(islot->debug_layout, m_debug_ip_data);
		if (offset >= size) {
			icap_xclbin_rd_unlock(icap, st);
			return f_nread;
		}

		if (count < size - offset)
			nread = count;
		else
			nread = size - offset;

		memcpy(buffer, ((char *)islot->debug_layout) + offset, nread);
		buffer += nread;
                f_nread += nread;
		icap_xclbin_rd_unlock(icap, st);
	}
	
	return f_nread;
}
static struct bin_attribute debug_ip_layout_attr = {
	.attr = {
		.name = "debug_ip_layout",
		.mode = 0444
	},
	.read = icap_read_debug_ip_layout,
	.write = NULL,
	.size = 0
};

/* IP layout */
static ssize_t icap_read_ip_layout(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap = NULL;
	struct islot_info *islot = NULL;
	u32 nread = 0;
	u32 f_nread = 0;
	size_t size = 0;
	int err = 0;
        int st = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (!islot || !islot->ip_layout)
			continue;

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return f_nread;

		size = sizeof_sect(islot->ip_layout, m_ip_data);
		if (offset >= size) {
			icap_xclbin_rd_unlock(icap, st);
			return f_nread;
		}

		if (count < size - offset)
			nread = count;
		else
			nread = size - offset;

		memcpy(buffer, ((char *)islot->ip_layout) + offset, nread);
		buffer += nread;
                f_nread += nread;
		icap_xclbin_rd_unlock(icap, st);
	}
	
	return f_nread;
}

static struct bin_attribute ip_layout_attr = {
	.attr = {
		.name = "ip_layout",
		.mode = 0444
	},
	.read = icap_read_ip_layout,
	.write = NULL,
	.size = 0
};

/* PS kernel */
static ssize_t icap_read_ps_kernel(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap = NULL;
	struct islot_info *islot = NULL;
	u32 nread = 0;
	u32 f_nread = 0;
	size_t size = 0;
	int err = 0;
	int st = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (!islot || !islot->ps_kernel)
			continue;

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return f_nread;

		size = sizeof(struct ps_kernel_node) + sizeof(struct ps_kernel_data) *
			(islot->ps_kernel->pkn_count - 1);
		if (offset >= size) {
			icap_xclbin_rd_unlock(icap, st);
			return f_nread;
		}

		if (count < size - offset)
			nread = count;
		else
			nread = size - offset;

		memcpy(buffer, ((char *)islot->ps_kernel) + offset, nread);

		buffer += nread;
		f_nread += nread;
		icap_xclbin_rd_unlock(icap, st);
	}
	
	return f_nread;
}

static struct bin_attribute ps_kernel_attr = {
	.attr = {
		.name = "ps_kernel",
		.mode = 0444
	},
	.read = icap_read_ps_kernel,
	.write = NULL,
	.size = 0
};

/* -Connectivity-- */
static ssize_t icap_read_connectivity(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap = NULL;
	struct islot_info *islot = NULL;
	u32 nread = 0;
	u32 f_nread = 0;
	size_t size = 0;
	int err = 0;
	int st = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (!islot || !islot->connectivity)
			continue;

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return f_nread;

		size = sizeof_sect(islot->connectivity, m_connection);
		if (offset >= size) {
			icap_xclbin_rd_unlock(icap, st);
			return f_nread;
		}

		if (count < size - offset)
			nread = count;
		else
			nread = size - offset;

		memcpy(buffer, ((char *)islot->connectivity) + offset, nread);

		buffer += nread;
		f_nread += nread;
		icap_xclbin_rd_unlock(icap, st);
	}

	return f_nread;
}

static struct bin_attribute connectivity_attr = {
	.attr = {
		.name = "connectivity",
		.mode = 0444
	},
	.read = icap_read_connectivity,
	.write = NULL,
	.size = 0
};

/* -Group Connectivity-- */
static ssize_t icap_read_group_connectivity(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap = NULL;
	struct islot_info *islot = NULL;
	u32 nread = 0;
	u32 f_nread = 0;
	size_t size = 0;
	int err = 0;
	int st = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (!islot || !islot->group_connectivity)
			continue;

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return f_nread;

		size = sizeof_sect(islot->group_connectivity, m_connection);
		if (offset >= size) {
			icap_xclbin_rd_unlock(icap, st);
			return f_nread;
		}

		if (count < size - offset)
			nread = count;
		else
			nread = size - offset;

		memcpy(buffer, ((char *)islot->group_connectivity) + offset, nread);

		buffer += nread;
		f_nread += nread;
		icap_xclbin_rd_unlock(icap, st);
	}
	
	return f_nread;
}

static struct bin_attribute group_connectivity_attr = {
	.attr = {
		.name = "group_connectivity",
		.mode = 0444
	},
	.read = icap_read_group_connectivity,
	.write = NULL,
	.size = 0
};

/* -Mem_topology-- */
static ssize_t icap_read_mem_topology(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));
	struct islot_info *islot = NULL;
	u32 nread = 0;
	u32 f_nread = 0;
	size_t size = 0;
	uint64_t range = 0;
	int err = 0, i;
	int st = 0;
	struct mem_topology *mem_topo = NULL;
	xdev_handle_t xdev;

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (!islot || !islot->mem_topo)
			continue;

		xdev = xocl_get_xdev(icap->icap_pdev);

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return f_nread;

		size = sizeof_sect(islot->mem_topo, m_mem_data);
		if (offset >= size) {
			icap_xclbin_rd_unlock(icap, st);
			vfree(mem_topo);
			return f_nread;
		}

		mem_topo = vzalloc(size);
		if (!mem_topo) { 
			icap_xclbin_rd_unlock(icap, st);
			vfree(mem_topo);
			return f_nread;
		}

		memcpy(mem_topo, islot->mem_topo, size);
		range = xocl_addr_translator_get_range(xdev);
		for ( i=0; i< mem_topo->m_count; ++i) {
			if (convert_mem_tag(mem_topo->m_mem_data[i].m_tag) == MEM_TAG_HOST){
				/* m_size in KB, convert Byte to KB */
				mem_topo->m_mem_data[i].m_size = (range>>10);
			}
		}

		if (count < size - offset)
			nread = count;
		else
			nread = size - offset;

		memcpy(buffer, ((char *)mem_topo) + offset, nread);

		buffer += nread;
		f_nread += nread;
		icap_xclbin_rd_unlock(icap, st);
		vfree(mem_topo);
	}
			
	return f_nread;
}


static struct bin_attribute mem_topology_attr = {
	.attr = {
		.name = "mem_topology",
		.mode = 0444
	},
	.read = icap_read_mem_topology,
	.write = NULL,
	.size = 0
};

/* -Group_topology-- */
static ssize_t icap_read_group_topology(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));
	struct islot_info *islot = NULL;
	u32 nread = 0;
	u32 f_nread = 0;
	size_t size = 0;
	uint64_t range = 0;
	int err = 0, i;
	int st = 0;
	struct mem_topology *group_topo = NULL;
	xdev_handle_t xdev;

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (!islot || !islot->group_topo)
			continue;

		xdev = xocl_get_xdev(icap->icap_pdev);

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return f_nread;

		size = sizeof_sect(islot->group_topo, m_mem_data);
		if (offset >= size) {
			icap_xclbin_rd_unlock(icap, st);
			vfree(group_topo);
			return f_nread;
		}

		group_topo = vzalloc(size);
		if (!group_topo) {
			icap_xclbin_rd_unlock(icap, st);
			vfree(group_topo);
			return f_nread;
		}

		memcpy(group_topo, islot->group_topo, size);
		range = xocl_addr_translator_get_range(xdev);
		for ( i=0; i< group_topo->m_count; ++i) {
			if (convert_mem_tag(group_topo->m_mem_data[i].m_tag) == MEM_TAG_HOST){
				/* m_size in KB, convert Byte to KB */
				group_topo->m_mem_data[i].m_size = (range>>10);
			} else
				continue;
		}

		if (count < size - offset)
			nread = count;
		else
			nread = size - offset;

		memcpy(buffer, ((char *)group_topo) + offset, nread);

		buffer += nread;
		f_nread += nread;
		icap_xclbin_rd_unlock(icap, st);
		vfree(group_topo);
	}
			
	return f_nread;
}

static struct bin_attribute group_topology_attr = {
	.attr = {
		.name = "group_topology",
		.mode = 0444
	},
	.read = icap_read_group_topology,
	.write = NULL,
	.size = 0
};

/* -Mem_topology-- */
static ssize_t icap_read_clock_freqs(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap = NULL;
	struct islot_info *islot = NULL;
	u32 nread = 0;
	u32 f_nread = 0;
	size_t size = 0;
	int err = 0;
	int st = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	for (st = 0; st < MAX_SLOT_SUPPORT; st++) {
		islot = icap->slot_info[st];
		if (!islot || !islot->xclbin_clock_freq_topology)
			continue;

		err = icap_xclbin_rd_lock(icap, st);
		if (err)
			return f_nread;

		size = sizeof_sect(islot->xclbin_clock_freq_topology, m_clock_freq);
		if (offset >= size) {
			icap_xclbin_rd_unlock(icap, st);
			return f_nread;
		}

		if (count < size - offset)
			nread = count;
		else
			nread = size - offset;

		memcpy(buffer, ((char *)islot->xclbin_clock_freq_topology) + offset, nread);
		buffer += nread;
		f_nread += nread;
		icap_xclbin_rd_unlock(icap, st);
	}

	return f_nread;
}


static struct bin_attribute clock_freq_topology_attr = {
	.attr = {
		.name = "clock_freq_topology",
		.mode = 0444
	},
	.read = icap_read_clock_freqs,
	.write = NULL,
	.size = 0
};

static ssize_t rp_bit_output(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off, size_t count)
{
	struct icap *icap;
	ssize_t ret = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj,
				struct device, kobj));
	if (!icap || !icap->rp_bit)
		return 0;

	if (off >= icap->rp_bit_len)
		goto bail;

	if (off + count > icap->rp_bit_len)
		count = icap->rp_bit_len - off;

	memcpy(buf, icap->rp_bit + off, count);

	ret = count;

bail:
	return ret;
}

static struct bin_attribute rp_bit_attr = {
	.attr = {
		.name = "rp_bit",
		.mode = 0400
	},
	.read = rp_bit_output,
	.size = 0
};

static struct bin_attribute *icap_bin_attrs[] = {
	&debug_ip_layout_attr,
	&ip_layout_attr,
	&ps_kernel_attr,
	&connectivity_attr,
	&group_connectivity_attr,
	&mem_topology_attr,
	&group_topology_attr,
	&rp_bit_attr,
	&clock_freq_topology_attr,
	NULL,
};

static struct attribute_group icap_attr_group = {
	.attrs = icap_attrs,
	.bin_attrs = icap_bin_attrs,
};

static int __icap_remove(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	uint32_t slot_id = 0;
	struct islot_info *islot = NULL;
	void *hdl;

	BUG_ON(icap == NULL);
	xocl_drvinst_release(icap, &hdl);

	xocl_xmc_freeze(xdev);
	icap_free_bins(icap);

	iounmap(icap->icap_regs);

	sysfs_remove_group(&pdev->dev.kobj, &icap_attr_group);

	for (slot_id = 0; slot_id < MAX_SLOT_SUPPORT; slot_id++) {
		islot = icap->slot_info[slot_id];
		if (!islot)
			continue;

		/* Clock frequence is only related to PL Slots */
		if (islot->pl_slot)
			xclbin_free_clock_freq_topology(icap, slot_id);

		icap_clean_bitstream_axlf(pdev, slot_id);
	}

	ICAP_INFO(icap, "cleaned up successfully");
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void icap_remove(struct platform_device *pdev)
{
	__icap_remove(pdev);
}
#else
#define icap_remove __icap_remove
#endif

/*
 * Run the following sequence of canned commands to obtain IDCODE of the FPGA
 */
static void icap_probe_chip(struct icap *icap)
{
	u32 w;

	if (!ICAP_PRIVILEGED(icap))
		return;

	w = reg_rd(&icap->icap_regs->ir_sr);
	w = reg_rd(&icap->icap_regs->ir_sr);
	reg_wr(&icap->icap_regs->ir_gier, 0x0);
	w = reg_rd(&icap->icap_regs->ir_wfv);
	reg_wr(&icap->icap_regs->ir_wf, 0xffffffff);
	reg_wr(&icap->icap_regs->ir_wf, 0xaa995566);
	reg_wr(&icap->icap_regs->ir_wf, 0x20000000);
	reg_wr(&icap->icap_regs->ir_wf, 0x20000000);
	reg_wr(&icap->icap_regs->ir_wf, 0x28018001);
	reg_wr(&icap->icap_regs->ir_wf, 0x20000000);
	reg_wr(&icap->icap_regs->ir_wf, 0x20000000);
	w = reg_rd(&icap->icap_regs->ir_cr);
	reg_wr(&icap->icap_regs->ir_cr, 0x1);
	w = reg_rd(&icap->icap_regs->ir_cr);
	w = reg_rd(&icap->icap_regs->ir_cr);
	w = reg_rd(&icap->icap_regs->ir_sr);
	w = reg_rd(&icap->icap_regs->ir_cr);
	w = reg_rd(&icap->icap_regs->ir_sr);
	reg_wr(&icap->icap_regs->ir_sz, 0x1);
	w = reg_rd(&icap->icap_regs->ir_cr);
	reg_wr(&icap->icap_regs->ir_cr, 0x2);
	w = reg_rd(&icap->icap_regs->ir_rfo);
	icap->idcode = reg_rd(&icap->icap_regs->ir_rf);
	w = reg_rd(&icap->icap_regs->ir_cr);
}

static int icap_probe(struct platform_device *pdev)
{
	struct icap *icap = NULL;
	struct resource *res;
	int ret;
	void **regs;

	icap = xocl_drvinst_alloc(&pdev->dev, sizeof(*icap));
	if (!icap)
		return -ENOMEM;

	platform_set_drvdata(pdev, icap);
	icap->icap_pdev = pdev;
	mutex_init(&icap->icap_lock);

	regs = (void **)&icap->icap_regs;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res != NULL) {
		*regs = ioremap_nocache(res->start,
			res->end - res->start + 1);
		if (*regs == NULL) {
			ICAP_ERR(icap, "failed to map in register");
			ret = -EIO;
			goto failed;
		} else {
			ICAP_INFO(icap,
				"%s mapped in register @ 0x%p",
				res->name, *regs);
		}

		icap_refresh_addrs(pdev);
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &icap_attr_group);
	if (ret) {
		ICAP_ERR(icap, "create icap attrs failed: %d", ret);
		goto failed;
	}

	if (ICAP_PRIVILEGED(icap)) {
#ifdef	EFI_SECURE_BOOT
		if (efi_enabled(EFI_SECURE_BOOT)) {
			ICAP_INFO(icap, "secure boot mode detected");
			icap->sec_level = ICAP_SEC_SYSTEM;
		} else {
			icap->sec_level = ICAP_SEC_NONE;
		}
#else
		ICAP_INFO(icap, "no support for detection of secure boot mode");
		icap->sec_level = ICAP_SEC_NONE;
#endif
	}

	icap->cache_expire_secs = ICAP_DEFAULT_EXPIRE_SECS;

	icap_probe_chip(icap);
	ICAP_INFO(icap, "successfully initialized FPGA IDCODE 0x%x",
			icap->idcode);
	return 0;

failed:
	(void) icap_remove(pdev);
	return ret;
}

#if PF == MGMTPF
static int icap_open(struct inode *inode, struct file *file)
{
	struct icap *icap = NULL;

	icap = xocl_drvinst_open_single(inode->i_cdev);
	if (!icap)
		return -ENXIO;

	file->private_data = icap;
	return 0;
}

static int icap_close(struct inode *inode, struct file *file)
{
	struct icap *icap = file->private_data;

	xocl_drvinst_close(icap);
	return 0;
}

static ssize_t icap_write_rp(struct file *filp, const char __user *data,
		size_t data_len, loff_t *off)
{
	struct icap *icap = filp->private_data;
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(icap->icap_pdev);
	struct axlf axlf_header = { {0} };
	struct axlf *axlf = NULL;
	const struct axlf_section_header *section;
	void *header;
	struct XHwIcap_Bit_Header bit_header = { 0 };
	ssize_t ret, len;
	int err;
	const char *sched_bin = XDEV(xdev)->priv.sched_bin;
	char bin[32] = {0}; 

	mutex_lock(&icap->icap_lock);
	if (icap->rp_fdt) {
		ICAP_ERR(icap, "Previous Dowload is not completed");
		mutex_unlock(&icap->icap_lock);
		return -EBUSY;
	}
	if (*off == 0) {
		ICAP_INFO(icap, "Download rp dsabin");
		if (data_len < sizeof(struct axlf)) {
			ICAP_ERR(icap, "axlf file is too small %ld", data_len);
			ret = -ENOMEM;
			goto failed;
		}

		ret = copy_from_user(&axlf_header, data, sizeof(struct axlf));
		if (ret) {
			ICAP_ERR(icap, "copy header buffer failed %ld", ret);
			goto failed;
		}

		if (memcmp(axlf_header.m_magic, ICAP_XCLBIN_V2,
			sizeof(ICAP_XCLBIN_V2))) {
			ICAP_ERR(icap, "Incorrect magic string");
			ret = -EINVAL;
			goto failed;
		}

		if (!axlf_header.m_header.m_length ||
			axlf_header.m_header.m_length >= GB(1)) {
			ICAP_ERR(icap, "Invalid xclbin size");
			ret = -EINVAL;
			goto failed;
		}

		icap->rp_bit_len = axlf_header.m_header.m_length;

		icap->rp_bit = vmalloc(icap->rp_bit_len);
		if (!icap->rp_bit) {
			ret = -ENOMEM;
			goto failed;
		}

		ret = copy_from_user(icap->rp_bit, data, data_len);
		if (ret) {
			ICAP_ERR(icap, "copy bit file failed %ld", ret);
			goto failed;
		}
		len = data_len;
	} else {
		len = (ssize_t)(min((loff_t)(icap->rp_bit_len),
				(*off + (loff_t)data_len)) - *off);
		if (len < 0) {
			ICAP_ERR(icap, "Invalid len %ld", len);
			ret = -EINVAL;
			goto failed;
		}
		ret = copy_from_user(icap->rp_bit + *off, data, len);
		if (ret) {
			ICAP_ERR(icap, "copy failed off %lld, len %ld",
					*off, len);
			goto failed;
		}
	}

	*off += len;
	if (*off < icap->rp_bit_len) {
		mutex_unlock(&icap->icap_lock);
		return len;
	}

	ICAP_INFO(icap, "parse incoming axlf");

	axlf = vmalloc(icap->rp_bit_len);
	if (!axlf) {
		ICAP_ERR(icap, "it stream buffer allocation failed");
		ret = -ENOMEM;
		goto failed;
	}

	memcpy(axlf, icap->rp_bit, icap->rp_bit_len);
	vfree(icap->rp_bit);
	icap->rp_bit = NULL;
	icap->rp_bit_len = 0;

	strncpy(icap->rp_vbnv, axlf->m_header.m_platformVBNV,
			sizeof(icap->rp_vbnv) - 1);
	section = xrt_xclbin_get_section_hdr(axlf, PARTITION_METADATA);
	if (!section) {
		ICAP_ERR(icap, "did not find PARTITION_METADATA section");
		ret = -EINVAL;
		goto failed;
	}

	header = (char *)axlf + section->m_sectionOffset;
	if (fdt_check_header(header) || fdt_totalsize(header) >
			section->m_sectionSize) {
		ICAP_ERR(icap, "Invalid PARTITION_METADATA");
		ret = -EINVAL;
		goto failed;
	}

	icap->rp_fdt = vmalloc(fdt_totalsize(header));
	if (!icap->rp_fdt) {
		ICAP_ERR(icap, "Not enough memory for PARTITION_METADATA");
		ret = -ENOMEM;
		goto failed;
	}
	icap->rp_fdt_len = fdt_totalsize(header);
	memcpy(icap->rp_fdt, header, fdt_totalsize(header));

	section = xrt_xclbin_get_section_hdr(axlf, BITSTREAM);
	if (!section) {
		ICAP_ERR(icap, "did not find BITSTREAM section");
		ret = -EINVAL;
		goto failed;
	}

	if (section->m_sectionSize < DMA_HWICAP_BITFILE_BUFFER_SIZE) {
		ICAP_ERR(icap, "bitstream is too small");
		ret = -EINVAL;
		goto failed;
	}

	header = (char *)axlf + section->m_sectionOffset;
	if (xrt_xclbin_parse_header(header,
			DMA_HWICAP_BITFILE_BUFFER_SIZE, &bit_header)) {
		ICAP_ERR(icap, "parse header failed");
		ret = -EINVAL;
		goto failed;
	}

	icap->rp_bit_len = bit_header.HeaderLength + bit_header.BitstreamLength;
	if (icap->rp_bit_len > section->m_sectionSize) {
		ICAP_ERR(icap, "bitstream is too big");
		ret = -EINVAL;
		goto failed;
	}

	icap->rp_bit = vmalloc(icap->rp_bit_len);
	if (!icap->rp_bit) {
		ICAP_ERR(icap, "Not enough memory for BITSTREAM");
		ret = -ENOMEM;
		goto failed;
	}

	memcpy(icap->rp_bit, header, icap->rp_bit_len);

	/* Try locating the board mgmt binary. */
	section = xrt_xclbin_get_section_hdr(axlf, FIRMWARE);
	if (section) {
		header = (char *)axlf + section->m_sectionOffset;
		icap->rp_mgmt_bin = vmalloc(section->m_sectionSize);
		if (!icap->rp_mgmt_bin) {
			ICAP_ERR(icap, "Not enough memory for cmc bin");
			ret = -ENOMEM;
			goto failed;
		}
		memcpy(icap->rp_mgmt_bin, header, section->m_sectionSize);
		icap->rp_mgmt_bin_len = section->m_sectionSize;
	}
	/* Try locating the microblaze binary. 
	 * For dynamic platforms like 1RP or 2RP, we load the ert fw
	 * under /lib/firmware/xilinx regardless there is an ert fw 
	 * in partition.xsabin or not
	 */
	if (!sched_bin) {
		section = xrt_xclbin_get_section_hdr(axlf, PARTITION_METADATA);
		if (section) {
			const char *ert_ver = xocl_fdt_get_ert_fw_ver(xdev,
				(char *)axlf + section->m_sectionOffset);
			if (ert_ver && !strncmp(ert_ver, "legacy", 6)) {
				sched_bin = "xilinx/sched.bin";
			} else if (ert_ver) {
				snprintf(bin, sizeof(bin),
					"xilinx/sched_%s.bin", ert_ver);
				sched_bin = bin;
			}
		}
	}

	if (sched_bin) {
		err = xocl_request_firmware(&pcidev->dev, sched_bin, (char **)&icap->rp_sche_bin,
					    &icap->rp_sche_bin_len);
		if (err)
			goto failed;

		ICAP_INFO(icap, "stashed shared mb sche bin, len %ld", icap->rp_sche_bin_len);
	}


	section = xrt_xclbin_get_section_hdr(axlf, SCHED_FIRMWARE);
	if (section && !icap->rp_sche_bin) {
		header = (char *)axlf + section->m_sectionOffset;
		icap->rp_sche_bin = vmalloc(section->m_sectionSize);
		if (!icap->rp_sche_bin) {
			ICAP_ERR(icap, "Not enough memory for sched bin");
			ret = -ENOMEM;
			goto failed;
		}
		memcpy(icap->rp_sche_bin, header, section->m_sectionSize);
		icap->rp_sche_bin_len = section->m_sectionSize;
		ICAP_INFO(icap, "sche bin from xsabin , len %ld", icap->rp_sche_bin_len);
	}

	vfree(axlf);

	ICAP_INFO(icap, "write axlf to device successfully. len %ld", len);

	mutex_unlock(&icap->icap_lock);
	xrt_xclbin_free_header(&bit_header);

	return len;

failed:
	xrt_xclbin_free_header(&bit_header);
	icap_free_bins(icap);

	vfree(axlf);
	mutex_unlock(&icap->icap_lock);

	return ret;
}

static const struct file_operations icap_fops = {
	.open = icap_open,
	.release = icap_close,
	.write = icap_write_rp,
};

struct xocl_drv_private icap_drv_priv = {
	.ops = &icap_ops,
	.fops = &icap_fops,
	.dev = -1,
	.cdev_name = NULL,
};
#else
struct xocl_drv_private icap_drv_priv = {
	.ops = &icap_ops,
};
#endif

struct platform_device_id icap_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ICAP), (kernel_ulong_t)&icap_drv_priv },
	{ },
};

static struct platform_driver icap_driver = {
	.probe		= icap_probe,
	.remove		= icap_remove,
	.driver		= {
		.name	= XOCL_DEVNAME(XOCL_ICAP),
	},
	.id_table = icap_id_table,
};

int __init xocl_init_icap(void)
{
	int err = 0;

	if (icap_drv_priv.fops) {
		err = alloc_chrdev_region(&icap_drv_priv.dev, 0,
				XOCL_MAX_DEVICES, icap_driver.driver.name);
		if (err < 0)
			goto err_reg_cdev;
	}

	err = platform_driver_register(&icap_driver);
	if (err)
		goto err_reg_driver;

	icap_keys = NULL;
#if PF == MGMTPF

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
	icap_keys = keyring_alloc(".xilinx_fpga_xclbin_keys", KUIDT_INIT(0),
		KGIDT_INIT(0), current_cred(),
		((KEY_POS_ALL & ~KEY_POS_SETATTR) |
		KEY_USR_VIEW | KEY_USR_WRITE | KEY_USR_SEARCH),
		KEY_ALLOC_NOT_IN_QUOTA, NULL, NULL);
#endif

#endif
	if (IS_ERR(icap_keys)) {
		err = PTR_ERR(icap_keys);
		icap_keys = NULL;
		pr_err("create icap keyring failed: %d", err);
		goto err_key;
	}

	return 0;

err_key:
	platform_driver_unregister(&icap_driver);
err_reg_driver:
	if (icap_drv_priv.fops && icap_drv_priv.dev != -1)
		unregister_chrdev_region(icap_drv_priv.dev, XOCL_MAX_DEVICES);
err_reg_cdev:
	return err;
}

void xocl_fini_icap(void)
{
	if (icap_keys)
		key_put(icap_keys);
	if (icap_drv_priv.fops && icap_drv_priv.dev != -1)
		unregister_chrdev_region(icap_drv_priv.dev, XOCL_MAX_DEVICES);
	platform_driver_unregister(&icap_driver);
}
