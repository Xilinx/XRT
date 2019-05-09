/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
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

/*
 * TODO: Currently, locking / unlocking bitstream is implemented w/ pid as
 * identification of bitstream users. We assume that, on bare metal, an app
 * has only one process and will open both user and mgmt pfs. In this model,
 * xclmgmt has enough information to handle locking/unlocking alone, but we
 * still involve user pf and mailbox here so that it'll be easier to support
 * cloud env later. We'll replace pid with a token that is more appropriate
 * to identify a user later as well.
 */

#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/uuid.h>
#include <linux/pid.h>
#include "xclbin.h"
#include "../xocl_drv.h"
#include "../xocl_drm.h"
#include "mgmt-ioctl.h"

#if defined(XOCL_UUID)
static xuid_t uuid_null = NULL_UUID_LE;
#endif

#define	ICAP_ERR(icap, fmt, arg...)	\
	xocl_err(&(icap)->icap_pdev->dev, fmt "\n", ##arg)
#define	ICAP_INFO(icap, fmt, arg...)	\
	xocl_info(&(icap)->icap_pdev->dev, fmt "\n", ##arg)
#define	ICAP_DBG(icap, fmt, arg...)	\
	xocl_dbg(&(icap)->icap_pdev->dev, fmt "\n", ##arg)

#define	ICAP_PRIVILEGED(icap)	((icap)->icap_regs != NULL)
#define DMA_HWICAP_BITFILE_BUFFER_SIZE 1024
#define	ICAP_MAX_REG_GROUPS		ARRAY_SIZE(XOCL_RES_ICAP_MGMT_U280)

#define	ICAP_MAX_NUM_CLOCKS		4
#define OCL_CLKWIZ_STATUS_OFFSET	0x4
#define OCL_CLKWIZ_CONFIG_OFFSET(n)	(0x200 + 4 * (n))
#define OCL_CLK_FREQ_COUNTER_OFFSET	0x8
#define ICAP_DEFAULT_EXPIRE_SECS	1

#define DATA_CLK			0
#define KERNEL_CLK			1
#define SYSTEM_CLK			2

#define INVALID_MEM_IDX			0xFFFF
/*
 * Bitstream header information.
 */
typedef struct {
	unsigned int HeaderLength;     /* Length of header in 32 bit words */
	unsigned int BitstreamLength;  /* Length of bitstream to read in bytes*/
	unsigned char *DesignName;     /* Design name read from bitstream header */
	unsigned char *PartName;       /* Part name read from bitstream header */
	unsigned char *Date;           /* Date read from bitstream header */
	unsigned char *Time;           /* Bitstream creation time read from header */
	unsigned int MagicLength;      /* Length of the magic numbers in header */
} XHwIcap_Bit_Header;
#define XHI_BIT_HEADER_FAILURE	-1
/* Used for parsing bitstream header */
#define XHI_EVEN_MAGIC_BYTE	0x0f
#define XHI_ODD_MAGIC_BYTE	0xf0
/* Extra mode for IDLE */
#define XHI_OP_IDLE		-1
/* The imaginary module length register */
#define XHI_MLR			15

#define	GATE_FREEZE_USER	0x0c
#define GATE_FREEZE_SHELL	0x00

static u32 gate_free_user[] = {0xe, 0xc, 0xe, 0xf};
static u32 gate_free_shell[] = {0x8, 0xc, 0xe, 0xf};

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

struct icap_axi_gate {
	u32			iag_wr;
	u32			iag_rvsd;
	u32			iag_rd;
} __attribute__((packed));

struct icap_bitstream_user {
	struct list_head	ibu_list;
	pid_t			ibu_pid;
};

struct icap {
	struct platform_device	*icap_pdev;
	struct mutex		icap_lock;
	struct icap_reg		*icap_regs;
	struct icap_generic_state *icap_state;
	unsigned int		idcode;
	bool			icap_axi_gate_frozen;
	bool			icap_axi_gate_shell_frozen;
	struct icap_axi_gate	*icap_axi_gate;

	u64			icap_bitstream_id;
	xuid_t			icap_bitstream_uuid;
	int			icap_bitstream_ref;
	struct list_head	icap_bitstream_users;

	char			*icap_clear_bitstream;
	unsigned long		icap_clear_bitstream_length;

	char			*icap_clock_bases[ICAP_MAX_NUM_CLOCKS];
	unsigned short		icap_ocl_frequency[ICAP_MAX_NUM_CLOCKS];

	struct clock_freq_topology *icap_clock_freq_topology;
	unsigned long		icap_clock_freq_topology_length;
	char			*icap_clock_freq_counter;
	struct mem_topology	*mem_topo;
	struct ip_layout	*ip_layout;
	struct debug_ip_layout	*debug_layout;
	struct connectivity	*connectivity;

	char			*bit_buffer;
	unsigned long		bit_length;
	char			*icap_clock_freq_counter_hbm;

	uint64_t		cache_expire_secs;
	struct xcl_hwicap	cache;
	ktime_t			cache_expires;

};

static inline u32 reg_rd(void __iomem *reg)
{
	return XOCL_READ_REG32(reg);
}

static inline void reg_wr(void __iomem *reg, u32 val)
{
	iowrite32(val, reg);
}

/*
 * Precomputed table with config0 and config2 register values together with
 * target frequency. The steps are approximately 5 MHz apart. Table is
 * generated by wiz.pl.
 */
const static struct xclmgmt_ocl_clockwiz {
	/* target frequency */
	unsigned short ocl;
	/* config0 register */
	unsigned long config0;
	/* config2 register */
	unsigned short config2;
} frequency_table[] = {
	{/* 600*/   60, 0x0601, 0x000a},
	{/* 600*/   66, 0x0601, 0x0009},
	{/* 600*/   75, 0x0601, 0x0008},
	{/* 800*/   80, 0x0801, 0x000a},
	{/* 600*/   85, 0x0601, 0x0007},
	{/* 900*/   90, 0x0901, 0x000a},
	{/*1000*/  100, 0x0a01, 0x000a},
	{/*1100*/  110, 0x0b01, 0x000a},
	{/* 700*/  116, 0x0701, 0x0006},
	{/*1100*/  122, 0x0b01, 0x0009},
	{/* 900*/  128, 0x0901, 0x0007},
	{/*1200*/  133, 0x0c01, 0x0009},
	{/*1400*/  140, 0x0e01, 0x000a},
	{/*1200*/  150, 0x0c01, 0x0008},
	{/*1400*/  155, 0x0e01, 0x0009},
	{/* 800*/  160, 0x0801, 0x0005},
	{/*1000*/  166, 0x0a01, 0x0006},
	{/*1200*/  171, 0x0c01, 0x0007},
	{/* 900*/  180, 0x0901, 0x0005},
	{/*1300*/  185, 0x0d01, 0x0007},
	{/*1400*/  200, 0x0e01, 0x0007},
	{/*1300*/  216, 0x0d01, 0x0006},
	{/* 900*/  225, 0x0901, 0x0004},
	{/*1400*/  233, 0x0e01, 0x0006},
	{/*1200*/  240, 0x0c01, 0x0005},
	{/*1000*/  250, 0x0a01, 0x0004},
	{/*1300*/  260, 0x0d01, 0x0005},
	{/* 800*/  266, 0x0801, 0x0003},
	{/*1100*/  275, 0x0b01, 0x0004},
	{/*1400*/  280, 0x0e01, 0x0005},
	{/*1200*/  300, 0x0c01, 0x0004},
	{/*1300*/  325, 0x0d01, 0x0004},
	{/*1000*/  333, 0x0a01, 0x0003},
	{/*1400*/  350, 0x0e01, 0x0004},
	{/*1100*/  366, 0x0b01, 0x0003},
	{/*1200*/  400, 0x0c01, 0x0003},
	{/*1300*/  433, 0x0d01, 0x0003},
	{/* 900*/  450, 0x0901, 0x0002},
	{/*1400*/  466, 0x0e01, 0x0003},
	{/*1000*/  500, 0x0a01, 0x0002}
};

static int icap_verify_bitstream_axlf(struct platform_device *pdev,
	struct axlf *xclbin);
static int icap_parse_bitstream_axlf_section(struct platform_device *pdev,
	const struct axlf *xclbin, enum axlf_section_kind kind);
static void icap_set_data(struct icap *icap, struct xcl_hwicap *hwicap);
static uint64_t icap_get_data_nolock(struct platform_device *pdev, enum data_kind kind);
static uint64_t icap_get_data(struct platform_device *pdev, enum data_kind kind);

static struct icap_bitstream_user *alloc_user(pid_t pid)
{
	struct icap_bitstream_user *u =
		kzalloc(sizeof(struct icap_bitstream_user), GFP_KERNEL);

	if (u) {
		INIT_LIST_HEAD(&u->ibu_list);
		u->ibu_pid = pid;
	}
	return u;
}

static void free_user(struct icap_bitstream_user *u)
{
	kfree(u);
}

static struct icap_bitstream_user *obtain_user(struct icap *icap, pid_t pid)
{
	struct list_head *pos, *n;

	list_for_each_safe(pos, n, &icap->icap_bitstream_users) {
		struct icap_bitstream_user *u = list_entry(pos, struct icap_bitstream_user, ibu_list);

		if (u->ibu_pid == pid)
			return u;
	}

	return NULL;
}

static void icap_read_from_peer(struct platform_device *pdev)
{
	struct mailbox_subdev_peer subdev_peer = {0};
	struct icap *icap = platform_get_drvdata(pdev);
	struct xcl_hwicap xcl_hwicap = {0};
	size_t resp_len = sizeof(struct xcl_hwicap);
	size_t data_len = sizeof(struct mailbox_subdev_peer);
	struct mailbox_req *mb_req = NULL;
	size_t reqlen = sizeof(struct mailbox_req) + data_len;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	ICAP_INFO(icap, "reading from peer");
	BUG_ON(ICAP_PRIVILEGED(icap));

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		return;

	mb_req->req = MAILBOX_REQ_PEER_DATA;
	subdev_peer.size = resp_len;
	subdev_peer.kind = ICAP;

	memcpy(mb_req->data, &subdev_peer, data_len);

	(void) xocl_peer_request(xdev,
		mb_req, reqlen, &xcl_hwicap, &resp_len, NULL, NULL);

	icap_set_data(icap, &xcl_hwicap);

	vfree(mb_req);
}

static void icap_set_data(struct icap *icap, struct xcl_hwicap *hwicap)
{
	memcpy(&icap->cache, hwicap, sizeof(struct xcl_hwicap));
	icap->cache_expires = ktime_add(ktime_get_boottime(), ktime_set(icap->cache_expire_secs, 0));
}

static int add_user(struct icap *icap, pid_t pid)
{
	struct icap_bitstream_user *u;

	u = obtain_user(icap, pid);
	if (u)
		return 0;

	u = alloc_user(pid);
	if (!u)
		return -ENOMEM;

	list_add_tail(&u->ibu_list, &icap->icap_bitstream_users);
	icap->icap_bitstream_ref++;
	return 0;
}

static int del_user(struct icap *icap, pid_t pid)
{
	struct icap_bitstream_user *u = NULL;

	u = obtain_user(icap, pid);
	if (!u)
		return -EINVAL;

	list_del(&u->ibu_list);
	free_user(u);
	icap->icap_bitstream_ref--;
	return 0;
}

static void del_all_users(struct icap *icap)
{
	struct icap_bitstream_user *u = NULL;
	struct list_head *pos, *n;

	if (icap->icap_bitstream_ref == 0)
		return;

	list_for_each_safe(pos, n, &icap->icap_bitstream_users) {
		u = list_entry(pos, struct icap_bitstream_user, ibu_list);
		list_del(&u->ibu_list);
		free_user(u);
	}

	ICAP_INFO(icap, "removed %d users", icap->icap_bitstream_ref);
	icap->icap_bitstream_ref = 0;
}

static unsigned find_matching_freq_config(unsigned freq)
{
	unsigned start = 0;
	unsigned end = ARRAY_SIZE(frequency_table) - 1;
	unsigned idx = ARRAY_SIZE(frequency_table) - 1;

	if (freq < frequency_table[0].ocl)
		return 0;

	if (freq > frequency_table[ARRAY_SIZE(frequency_table) - 1].ocl)
		return ARRAY_SIZE(frequency_table) - 1;

	while (start < end) {
		if (freq == frequency_table[idx].ocl)
			break;
		if (freq < frequency_table[idx].ocl)
			end = idx;
		else
			start = idx + 1;
		idx = start + (end - start) / 2;
	}
	if (freq < frequency_table[idx].ocl)
		idx--;

	return idx;
}

static unsigned find_matching_freq(unsigned freq)
{
	int idx = find_matching_freq_config(freq);

	return frequency_table[idx].ocl;
}


static unsigned short icap_get_ocl_frequency(const struct icap *icap, int idx)
{
#define XCL_INPUT_FREQ 100
	const u64 input = XCL_INPUT_FREQ;
	u32 val;
	u32 mul0, div0;
	u32 mul_frac0 = 0;
	u32 div1;
	u32 div_frac1 = 0;
	u64 freq = 0;
	char *base = NULL;

	if (ICAP_PRIVILEGED(icap)) {
		base = icap->icap_clock_bases[idx];
		if (!base)
			return 0;
		val = reg_rd(base + OCL_CLKWIZ_STATUS_OFFSET);
		if ((val & 1) == 0)
			return 0;

		val = reg_rd(base + OCL_CLKWIZ_CONFIG_OFFSET(0));

		div0 = val & 0xff;
		mul0 = (val & 0xff00) >> 8;
		if (val & BIT(26)) {
			mul_frac0 = val >> 16;
			mul_frac0 &= 0x3ff;
		}

		/*
		 * Multiply both numerator (mul0) and the denominator (div0) with 1000
		 * to account for fractional portion of multiplier
		 */
		mul0 *= 1000;
		mul0 += mul_frac0;
		div0 *= 1000;

		val = reg_rd(base + OCL_CLKWIZ_CONFIG_OFFSET(2));

		div1 = val & 0xff;
		if (val & BIT(18)) {
			div_frac1 = val >> 8;
			div_frac1 &= 0x3ff;
		}

		/*
		 * Multiply both numerator (mul0) and the denominator (div1) with 1000 to
		 * account for fractional portion of divider
		 */

		div1 *= 1000;
		div1 += div_frac1;
		div0 *= div1;
		mul0 *= 1000;
		if (div0 == 0) {
			ICAP_ERR(icap, "clockwiz 0 divider");
			return 0;
		}
		freq = (input * mul0) / div0;
	} else {
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
			break;
		}
	}
	return freq;
}

static unsigned int icap_get_clock_frequency_counter_khz(const struct icap *icap, int idx)
{
	u32 freq = 0, status;
	int times = 10;
	/*
	 * reset and wait until done
	 */
	if (ICAP_PRIVILEGED(icap)) {
		if (uuid_is_null(&icap->icap_bitstream_uuid))
			return freq;

		if (idx < 2) {
			reg_wr(icap->icap_clock_freq_counter, 0x1);
			while (times != 0) {
				status = reg_rd(icap->icap_clock_freq_counter);
				if (status == 0x2)
					break;
				mdelay(1);
				times--;
			};
			freq = reg_rd(icap->icap_clock_freq_counter + OCL_CLK_FREQ_COUNTER_OFFSET + idx*sizeof(u32));
		} else if (idx == 2) {
			if (!icap->icap_clock_freq_counter_hbm)
				return 0;

			reg_wr(icap->icap_clock_freq_counter_hbm, 0x1);
			while (times != 0) {
				status = reg_rd(icap->icap_clock_freq_counter_hbm);
				if (status == 0x2)
					break;
				mdelay(1);
				times--;
			};
			freq = reg_rd(icap->icap_clock_freq_counter_hbm + OCL_CLK_FREQ_COUNTER_OFFSET);
		}

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
/*
 * Based on Clocking Wizard v5.1, section Dynamic Reconfiguration
 * through AXI4-Lite
 */
static int icap_ocl_freqscaling(struct icap *icap, bool force)
{
	unsigned curr_freq;
	u32 config;
	int i;
	int j = 0;
	u32 val = 0;
	unsigned idx = 0;
	long err = 0;

	for (i = 0; i < ICAP_MAX_NUM_CLOCKS; ++i) {
		/* A value of zero means skip scaling for this clock index */
		if (!icap->icap_ocl_frequency[i])
			continue;

		idx = find_matching_freq_config(icap->icap_ocl_frequency[i]);
		curr_freq = icap_get_ocl_frequency(icap, i);
		ICAP_INFO(icap, "Clock %d, Current %d Mhz, New %d Mhz ",
				i, curr_freq, icap->icap_ocl_frequency[i]);

		/*
		 * If current frequency is in the same step as the
		 * requested frequency then nothing to do.
		 */
		if (!force && (find_matching_freq_config(curr_freq) == idx))
			continue;

		val = reg_rd(icap->icap_clock_bases[i] +
			OCL_CLKWIZ_STATUS_OFFSET);
		if (val != 1) {
			ICAP_ERR(icap, "clockwiz %d is busy", i);
			err = -EBUSY;
			break;
		}

		config = frequency_table[idx].config0;
		reg_wr(icap->icap_clock_bases[i] + OCL_CLKWIZ_CONFIG_OFFSET(0),
			config);
		config = frequency_table[idx].config2;
		reg_wr(icap->icap_clock_bases[i] + OCL_CLKWIZ_CONFIG_OFFSET(2),
			config);
		msleep(10);
		reg_wr(icap->icap_clock_bases[i] + OCL_CLKWIZ_CONFIG_OFFSET(23),
			0x00000007);
		msleep(1);
		reg_wr(icap->icap_clock_bases[i] + OCL_CLKWIZ_CONFIG_OFFSET(23),
			0x00000002);

		ICAP_INFO(icap, "clockwiz waiting for locked signal");
		msleep(100);
		for (j = 0; j < 100; j++) {
			val = reg_rd(icap->icap_clock_bases[i] +
				OCL_CLKWIZ_STATUS_OFFSET);
			if (val != 1) {
				msleep(100);
				continue;
			}
		}
		if (val != 1) {
			ICAP_ERR(icap, "clockwiz MMCM/PLL did not lock after %d"
				"ms, restoring the original configuration",
				100 * 100);
			/* restore the original clock configuration */
			reg_wr(icap->icap_clock_bases[i] +
				OCL_CLKWIZ_CONFIG_OFFSET(23), 0x00000004);
			msleep(10);
			reg_wr(icap->icap_clock_bases[i] +
				OCL_CLKWIZ_CONFIG_OFFSET(23), 0x00000000);
			err = -ETIMEDOUT;
			break;
		}
		val = reg_rd(icap->icap_clock_bases[i] +
			OCL_CLKWIZ_CONFIG_OFFSET(0));
		ICAP_INFO(icap, "clockwiz CONFIG(0) 0x%x", val);
		val = reg_rd(icap->icap_clock_bases[i] +
			OCL_CLKWIZ_CONFIG_OFFSET(2));
		ICAP_INFO(icap, "clockwiz CONFIG(2) 0x%x", val);
	}

	return err;
}

static bool icap_bitstream_in_use(struct icap *icap, pid_t pid)
{
	BUG_ON(icap->icap_bitstream_ref < 0);

	/* Any user counts if pid isn't specified. */
	if (pid == 0)
		return icap->icap_bitstream_ref != 0;

	if (icap->icap_bitstream_ref == 0)
		return false;
	if ((icap->icap_bitstream_ref == 1) && obtain_user(icap, pid))
		return false;
	return true;
}

static int icap_freeze_axi_gate_shell(struct icap *icap)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);

	ICAP_INFO(icap, "freezing Shell AXI gate");
	BUG_ON(icap->icap_axi_gate_shell_frozen);

	(void) reg_rd(&icap->icap_axi_gate->iag_rd);
	reg_wr(&icap->icap_axi_gate->iag_wr, GATE_FREEZE_SHELL);
	(void) reg_rd(&icap->icap_axi_gate->iag_rd);

	if (!xocl_is_unified(xdev)) {
		reg_wr(&icap->icap_regs->ir_cr, 0xc);
		ndelay(20);
	} else {
		/* New ICAP reset sequence applicable only to unified dsa. */
		reg_wr(&icap->icap_regs->ir_cr, 0x8);
		ndelay(2000);
		reg_wr(&icap->icap_regs->ir_cr, 0x0);
		ndelay(2000);
		reg_wr(&icap->icap_regs->ir_cr, 0x4);
		ndelay(2000);
		reg_wr(&icap->icap_regs->ir_cr, 0x0);
		ndelay(2000);
	}

	icap->icap_axi_gate_shell_frozen = true;

	return 0;
}

static int icap_free_axi_gate_shell(struct icap *icap)
{
	int i;

	ICAP_INFO(icap, "freeing Shell AXI gate");
	/*
	 * First pulse the OCL RESET. This is important for PR with multiple
	 * clocks as it resets the edge triggered clock converter FIFO
	 */

	if (!icap->icap_axi_gate_shell_frozen)
		return 0;

	for (i = 0; i < ARRAY_SIZE(gate_free_shell); i++) {
		(void) reg_rd(&icap->icap_axi_gate->iag_rd);
		reg_wr(&icap->icap_axi_gate->iag_wr, gate_free_shell[i]);
		mdelay(50);
	}

	(void) reg_rd(&icap->icap_axi_gate->iag_rd);

	icap->icap_axi_gate_shell_frozen = false;

	return 0;
}

static int icap_freeze_axi_gate(struct icap *icap)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);

	ICAP_INFO(icap, "freezing CL AXI gate");
	BUG_ON(icap->icap_axi_gate_frozen);

	write_lock(&XDEV(xdev)->rwlock);
	(void) reg_rd(&icap->icap_axi_gate->iag_rd);
	reg_wr(&icap->icap_axi_gate->iag_wr, GATE_FREEZE_USER);
	(void) reg_rd(&icap->icap_axi_gate->iag_rd);

	if (!xocl_is_unified(xdev)) {
		reg_wr(&icap->icap_regs->ir_cr, 0xc);
		ndelay(20);
	} else {
		/* New ICAP reset sequence applicable only to unified dsa. */
		reg_wr(&icap->icap_regs->ir_cr, 0x8);
		ndelay(2000);
		reg_wr(&icap->icap_regs->ir_cr, 0x0);
		ndelay(2000);
		reg_wr(&icap->icap_regs->ir_cr, 0x4);
		ndelay(2000);
		reg_wr(&icap->icap_regs->ir_cr, 0x0);
		ndelay(2000);
	}

	icap->icap_axi_gate_frozen = true;

	return 0;
}

static int icap_free_axi_gate(struct icap *icap)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int i;

	ICAP_INFO(icap, "freeing CL AXI gate");
	/*
	 * First pulse the OCL RESET. This is important for PR with multiple
	 * clocks as it resets the edge triggered clock converter FIFO
	 */

	if (!icap->icap_axi_gate_frozen)
		return 0;

	for (i = 0; i < ARRAY_SIZE(gate_free_user); i++) {
		(void) reg_rd(&icap->icap_axi_gate->iag_rd);
		reg_wr(&icap->icap_axi_gate->iag_wr, gate_free_user[i]);
		ndelay(500);
	}

	(void) reg_rd(&icap->icap_axi_gate->iag_rd);

	icap->icap_axi_gate_frozen = false;
	write_unlock(&XDEV(xdev)->rwlock);

	return 0;
}

static void platform_reset_axi_gate(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);

	/* Can only be done from mgmt pf. */
	if (!ICAP_PRIVILEGED(icap))
		return;

	mutex_lock(&icap->icap_lock);
	if (!icap_bitstream_in_use(icap, 0)) {
		(void) icap_freeze_axi_gate(platform_get_drvdata(pdev));
		msleep(500);
		(void) icap_free_axi_gate(platform_get_drvdata(pdev));
		msleep(500);
	}
	mutex_unlock(&icap->icap_lock);
}

static int set_freqs(struct icap *icap, unsigned short *freqs, int num_freqs)
{
	int i;
	int err;
	u32 val;

	for (i = 0; i < min(ICAP_MAX_NUM_CLOCKS, num_freqs); ++i) {
		if (freqs[i] == 0)
			continue;

		if (!icap->icap_clock_bases[i])
			continue;

		val = reg_rd(icap->icap_clock_bases[i] +
			OCL_CLKWIZ_STATUS_OFFSET);
		if ((val & 0x1) == 0) {
			ICAP_ERR(icap, "clockwiz %d is busy", i);
			err = -EBUSY;
			goto done;
		}
	}

	memcpy(icap->icap_ocl_frequency, freqs,
		sizeof(*freqs) * min(ICAP_MAX_NUM_CLOCKS, num_freqs));

	icap_freeze_axi_gate(icap);
	err = icap_ocl_freqscaling(icap, false);
	icap_free_axi_gate(icap);

done:
	return err;

}

static int set_and_verify_freqs(struct icap *icap, unsigned short *freqs, int num_freqs)
{
	int i;
	int err;
	u32 clock_freq_counter, request_in_khz, tolerance, lookup_freq;

	err = set_freqs(icap, freqs, num_freqs);
	if (err)
		return err;

	for (i = 0; i < min(ICAP_MAX_NUM_CLOCKS, num_freqs); ++i) {
		if (!freqs[i])
			continue;

		lookup_freq = find_matching_freq(freqs[i]);
		clock_freq_counter = icap_get_clock_frequency_counter_khz(icap, i);
		request_in_khz = lookup_freq*1000;
		tolerance = lookup_freq*50;
		if (tolerance < abs(clock_freq_counter-request_in_khz)) {
			ICAP_ERR(icap, "Frequency is higher than tolerance value, request %u"
					"khz, actual %u khz", request_in_khz, clock_freq_counter);
			err = -EDOM;
			break;
		}
	}

	return err;
}

static int icap_ocl_set_freqscaling(struct platform_device *pdev,
	unsigned int region, unsigned short *freqs, int num_freqs)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0;

	/* Can only be done from mgmt pf. */
	if (!ICAP_PRIVILEGED(icap))
		return -EPERM;

	/* For now, only PR region 0 is supported. */
	if (region != 0)
		return -EINVAL;

	mutex_lock(&icap->icap_lock);

	err = set_freqs(icap, freqs, num_freqs);

	mutex_unlock(&icap->icap_lock);

	return err;
}

static int icap_ocl_update_clock_freq_topology(struct platform_device *pdev, struct xclmgmt_ioc_freqscaling *freq_obj)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct clock_freq_topology *topology = 0;
	int num_clocks = 0;
	int i = 0;
	int err = 0;

	mutex_lock(&icap->icap_lock);
	if (!uuid_is_null(&icap->icap_bitstream_uuid)) {
		topology = icap->icap_clock_freq_topology;
		num_clocks = topology->m_count;
		ICAP_INFO(icap, "Num clocks is %d", num_clocks);
		for (i = 0; i < ARRAY_SIZE(freq_obj->ocl_target_freq); i++) {
			ICAP_INFO(icap, "requested frequency is : %d xclbin freq is: %d",
				freq_obj->ocl_target_freq[i],
				topology->m_clock_freq[i].m_freq_Mhz);
			if (freq_obj->ocl_target_freq[i] >
				topology->m_clock_freq[i].m_freq_Mhz) {
				ICAP_ERR(icap, "Unable to set frequency as requested frequency %d is greater than set by xclbin %d",
					freq_obj->ocl_target_freq[i],
					topology->m_clock_freq[i].m_freq_Mhz);
				err = -EDOM;
				goto done;
			}
		}
	} else {
		ICAP_ERR(icap, "ERROR: There isn't a hardware accelerator loaded in the dynamic region."
			" Validation of accelerator frequencies cannot be determine");
		err = -EDOM;
		goto done;
	}

	err = set_and_verify_freqs(icap, freq_obj->ocl_target_freq, ARRAY_SIZE(freq_obj->ocl_target_freq));

done:
	mutex_unlock(&icap->icap_lock);
	return err;
}

static int icap_ocl_get_freqscaling(struct platform_device *pdev,
	unsigned int region, unsigned short *freqs, int num_freqs)
{
	int i;
	struct icap *icap = platform_get_drvdata(pdev);

	/* For now, only PR region 0 is supported. */
	if (region != 0)
		return -EINVAL;

	mutex_lock(&icap->icap_lock);
	for (i = 0; i < min(ICAP_MAX_NUM_CLOCKS, num_freqs); i++)
		freqs[i] = icap_get_ocl_frequency(icap, i);
	mutex_unlock(&icap->icap_lock);

	return 0;
}

static inline bool mig_calibration_done(struct icap *icap)
{
	return (reg_rd(&icap->icap_state->igs_state) & BIT(0)) != 0;
}

/* Check for MIG calibration. */
static int calibrate_mig(struct icap *icap)
{
	int i;

	for (i = 0; i < 10 && !mig_calibration_done(icap); ++i)
		msleep(500);

	if (!mig_calibration_done(icap)) {
		ICAP_ERR(icap,
			"MIG calibration timeout after bitstream download");
		return -ETIMEDOUT;
	}

	return 0;
}

static inline void free_clock_freq_topology(struct icap *icap)
{
	vfree(icap->icap_clock_freq_topology);
	icap->icap_clock_freq_topology = NULL;
	icap->icap_clock_freq_topology_length = 0;
}

static void icap_write_clock_freq(struct clock_freq *dst, struct clock_freq *src)
{
	dst->m_freq_Mhz = src->m_freq_Mhz;
	dst->m_type = src->m_type;
	memcpy(&dst->m_name, &src->m_name, sizeof(src->m_name));
}


static int icap_setup_clock_freq_topology(struct icap *icap,
	const char *buffer, unsigned long length)
{
	int i;
	struct clock_freq_topology *topology = (struct clock_freq_topology *)buffer;
	struct clock_freq *clk_freq = NULL;

	if (length == 0)
		return 0;

	free_clock_freq_topology(icap);

	icap->icap_clock_freq_topology = vmalloc(length);
	if (!icap->icap_clock_freq_topology)
		return -ENOMEM;
	/*
	 *  icap->icap_clock_freq_topology->m_clock_freq
	 *  must follow the order
	 *
	 *	0: DATA_CLK
	 *	1: KERNEL_CLK
	 *	2: SYSTEM_CLK
	 *
	 */
	icap->icap_clock_freq_topology->m_count = topology->m_count;
	for (i = 0; i < topology->m_count; ++i) {
		if (topology->m_clock_freq[i].m_type == CT_SYSTEM)
			clk_freq = &icap->icap_clock_freq_topology->m_clock_freq[SYSTEM_CLK];
		else if (topology->m_clock_freq[i].m_type == CT_DATA)
			clk_freq = &icap->icap_clock_freq_topology->m_clock_freq[DATA_CLK];
		else if (topology->m_clock_freq[i].m_type == CT_KERNEL)
			clk_freq = &icap->icap_clock_freq_topology->m_clock_freq[KERNEL_CLK];
		else
			break;

		icap_write_clock_freq(clk_freq, &topology->m_clock_freq[i]);
	}
	icap->icap_clock_freq_topology_length = length;

	return 0;
}

static inline void free_clear_bitstream(struct icap *icap)
{
	vfree(icap->icap_clear_bitstream);
	icap->icap_clear_bitstream = NULL;
	icap->icap_clear_bitstream_length = 0;
}

static int icap_setup_clear_bitstream(struct icap *icap,
	const char *buffer, unsigned long length)
{
	if (length == 0)
		return 0;

	free_clear_bitstream(icap);

	icap->icap_clear_bitstream = vmalloc(length);
	if (!icap->icap_clear_bitstream)
		return -ENOMEM;

	memcpy(icap->icap_clear_bitstream, buffer, length);
	icap->icap_clear_bitstream_length = length;

	return 0;
}

static int wait_for_done(struct icap *icap)
{
	u32 w;
	int i = 0;

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

static uint64_t icap_get_section_size(struct icap *icap, enum axlf_section_kind kind)
{
	uint64_t size = 0;

	switch (kind) {
	case IP_LAYOUT:
		size = sizeof_sect(icap->ip_layout, m_ip_data);
		break;
	case MEM_TOPOLOGY:
		size = sizeof_sect(icap->mem_topo, m_mem_data);
		break;
	case DEBUG_IP_LAYOUT:
		size = sizeof_sect(icap->debug_layout, m_debug_ip_data);
		break;
	case CONNECTIVITY:
		size = sizeof_sect(icap->connectivity, m_connection);
		break;
	default:
		break;
	}

	return size;
}

static int bitstream_parse_header(struct icap *icap, const unsigned char *data,
	unsigned int size, XHwIcap_Bit_Header *header)
{
	unsigned int i;
	unsigned int len;
	unsigned int tmp;
	unsigned int index;

	/* Start Index at start of bitstream */
	index = 0;

	/* Initialize HeaderLength.  If header returned early inidicates
	 * failure.
	 */
	header->HeaderLength = XHI_BIT_HEADER_FAILURE;

	/* Get "Magic" length */
	header->MagicLength = data[index++];
	header->MagicLength = (header->MagicLength << 8) | data[index++];

	/* Read in "magic" */
	for (i = 0; i < header->MagicLength - 1; i++) {
		tmp = data[index++];
		if (i%2 == 0 && tmp != XHI_EVEN_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

		if (i%2 == 1 && tmp != XHI_ODD_MAGIC_BYTE)
			return -1;   /* INVALID_FILE_HEADER_ERROR */

	}

	/* Read null end of magic data. */
	tmp = data[index++];

	/* Read 0x01 (short) */
	tmp = data[index++];
	tmp = (tmp << 8) | data[index++];

	/* Check the "0x01" half word */
	if (tmp != 0x01)
		return -1;	 /* INVALID_FILE_HEADER_ERROR */

	/* Read 'a' */
	tmp = data[index++];
	if (tmp != 'a')
		return -1;	  /* INVALID_FILE_HEADER_ERROR	*/

	/* Get Design Name length */
	len = data[index++];
	len = (len << 8) | data[index++];

	/* allocate space for design name and final null character. */
	header->DesignName = kmalloc(len, GFP_KERNEL);

	/* Read in Design Name */
	for (i = 0; i < len; i++)
		header->DesignName[i] = data[index++];


	if (header->DesignName[len-1] != '\0')
		return -1;

	/* Read 'b' */
	tmp = data[index++];
	if (tmp != 'b')
		return -1;	/* INVALID_FILE_HEADER_ERROR */

	/* Get Part Name length */
	len = data[index++];
	len = (len << 8) | data[index++];

	/* allocate space for part name and final null character. */
	header->PartName = kmalloc(len, GFP_KERNEL);

	/* Read in part name */
	for (i = 0; i < len; i++)
		header->PartName[i] = data[index++];

	if (header->PartName[len-1] != '\0')
		return -1;

	/* Read 'c' */
	tmp = data[index++];
	if (tmp != 'c')
		return -1;	/* INVALID_FILE_HEADER_ERROR */

	/* Get date length */
	len = data[index++];
	len = (len << 8) | data[index++];

	/* allocate space for date and final null character. */
	header->Date = kmalloc(len, GFP_KERNEL);

	/* Read in date name */
	for (i = 0; i < len; i++)
		header->Date[i] = data[index++];

	if (header->Date[len - 1] != '\0')
		return -1;

	/* Read 'd' */
	tmp = data[index++];
	if (tmp != 'd')
		return -1;	/* INVALID_FILE_HEADER_ERROR  */

	/* Get time length */
	len = data[index++];
	len = (len << 8) | data[index++];

	/* allocate space for time and final null character. */
	header->Time = kmalloc(len, GFP_KERNEL);

	/* Read in time name */
	for (i = 0; i < len; i++)
		header->Time[i] = data[index++];

	if (header->Time[len - 1] != '\0')
		return -1;

	/* Read 'e' */
	tmp = data[index++];
	if (tmp != 'e')
		return -1;	/* INVALID_FILE_HEADER_ERROR */

	/* Get byte length of bitstream */
	header->BitstreamLength = data[index++];
	header->BitstreamLength = (header->BitstreamLength << 8) | data[index++];
	header->BitstreamLength = (header->BitstreamLength << 8) | data[index++];
	header->BitstreamLength = (header->BitstreamLength << 8) | data[index++];
	header->HeaderLength = index;

	ICAP_INFO(icap, "Design \"%s\"", header->DesignName);
	ICAP_INFO(icap, "Part \"%s\"", header->PartName);
	ICAP_INFO(icap, "Timestamp \"%s %s\"", header->Time, header->Date);
	ICAP_INFO(icap, "Raw data size 0x%x", header->BitstreamLength);
	return 0;
}

static int bitstream_helper(struct icap *icap, const u32 *word_buffer,
	unsigned word_count)
{
	unsigned remain_word;
	unsigned word_written = 0;
	int wr_fifo_vacancy = 0;
	int err = 0;

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
			err = -EIO;
			break;
		}
	}

	return err;
}

static long icap_download(struct icap *icap, const char *buffer,
	unsigned long length)
{
	long err = 0;
	XHwIcap_Bit_Header bit_header = { 0 };
	unsigned numCharsRead = DMA_HWICAP_BITFILE_BUFFER_SIZE;
	unsigned byte_read;

	BUG_ON(!buffer);
	BUG_ON(!length);

	if (bitstream_parse_header(icap, buffer,
		DMA_HWICAP_BITFILE_BUFFER_SIZE, &bit_header)) {
		err = -EINVAL;
		goto free_buffers;
	}

	if ((bit_header.HeaderLength + bit_header.BitstreamLength) > length) {
		err = -EINVAL;
		goto free_buffers;
	}

	buffer += bit_header.HeaderLength;

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
	kfree(bit_header.DesignName);
	kfree(bit_header.PartName);
	kfree(bit_header.Date);
	kfree(bit_header.Time);
	return err;
}

static const struct axlf_section_header *get_axlf_section_hdr(
	struct icap *icap, const struct axlf *top, enum axlf_section_kind kind)
{
	int i;
	const struct axlf_section_header *hdr = NULL;

	ICAP_INFO(icap,
		"trying to find section header for axlf section %d", kind);

	for (i = 0; i < top->m_header.m_numSections; i++) {
		ICAP_INFO(icap, "saw section header: %d",
			top->m_sections[i].m_sectionKind);
		if (top->m_sections[i].m_sectionKind == kind) {
			hdr = &top->m_sections[i];
			break;
		}
	}

	if (hdr) {
		if ((hdr->m_sectionOffset + hdr->m_sectionSize) >
			top->m_header.m_length) {
			ICAP_INFO(icap, "found section is invalid");
			hdr = NULL;
		} else {
			ICAP_INFO(icap, "header offset: %llu, size: %llu",
				hdr->m_sectionOffset, hdr->m_sectionSize);
		}
	} else {
		ICAP_INFO(icap, "could not find section header %d", kind);
	}

	return hdr;
}

static int alloc_and_get_axlf_section(struct icap *icap,
	const struct axlf *top, enum axlf_section_kind kind,
	void **addr, uint64_t *size)
{
	void *section = NULL;
	const struct axlf_section_header *hdr =
		get_axlf_section_hdr(icap, top, kind);

	if (hdr == NULL)
		return -EINVAL;

	section = vmalloc(hdr->m_sectionSize);
	if (section == NULL)
		return -ENOMEM;

	memcpy(section, ((const char *)top) + hdr->m_sectionOffset,
		hdr->m_sectionSize);

	*addr = section;
	*size = hdr->m_sectionSize;
	return 0;
}

static int icap_download_boot_firmware(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct pci_dev *pcidev = XOCL_PL_TO_PCI_DEV(pdev);
	struct pci_dev *pcidev_user = NULL;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int funcid = PCI_FUNC(pcidev->devfn);
	int slotid = PCI_SLOT(pcidev->devfn);
	unsigned short deviceid = pcidev->device;
	struct axlf *bin_obj_axlf;
	const struct firmware *fw;
	char fw_name[128];
	XHwIcap_Bit_Header bit_header = { 0 };
	long err = 0;
	uint64_t length = 0;
	uint64_t primaryFirmwareOffset = 0;
	uint64_t primaryFirmwareLength = 0;
	uint64_t secondaryFirmwareOffset = 0;
	uint64_t secondaryFirmwareLength = 0;
	uint64_t mbBinaryOffset = 0;
	uint64_t mbBinaryLength = 0;
	const struct axlf_section_header *primaryHeader = 0;
	const struct axlf_section_header *secondaryHeader = 0;
	const struct axlf_section_header *mbHeader = 0;
	bool load_mbs = false;

	/* Can only be done from mgmt pf. */
	if (!ICAP_PRIVILEGED(icap))
		return -EPERM;

	/* Read dsabin from file system. */

	if (funcid != 0) {
		pcidev_user = pci_get_slot(pcidev->bus,
			PCI_DEVFN(slotid, funcid - 1));
		if (!pcidev_user) {
			pcidev_user = pci_get_device(pcidev->vendor,
				pcidev->device + 1, NULL);
		}
		if (pcidev_user)
			deviceid = pcidev_user->device;
	}

	snprintf(fw_name, sizeof(fw_name),
		"xilinx/%04x-%04x-%04x-%016llx.dsabin",
		le16_to_cpu(pcidev->vendor),
		le16_to_cpu(deviceid),
		le16_to_cpu(pcidev->subsystem_device),
		le64_to_cpu(xocl_get_timestamp(xdev)));
	ICAP_INFO(icap, "try load dsabin %s", fw_name);
	err = request_firmware(&fw, fw_name, &pcidev->dev);
	if (err) {
		snprintf(fw_name, sizeof(fw_name),
			"xilinx/%04x-%04x-%04x-%016llx.dsabin",
			le16_to_cpu(pcidev->vendor),
			le16_to_cpu(deviceid + 1),
			le16_to_cpu(pcidev->subsystem_device),
			le64_to_cpu(xocl_get_timestamp(xdev)));
		ICAP_INFO(icap, "try load dsabin %s", fw_name);
		err = request_firmware(&fw, fw_name, &pcidev->dev);
	}
	/* Retry with the legacy dsabin. */
	if (err) {
		snprintf(fw_name, sizeof(fw_name),
			"xilinx/%04x-%04x-%04x-%016llx.dsabin",
			le16_to_cpu(pcidev->vendor),
			le16_to_cpu(pcidev->device + 1),
			le16_to_cpu(pcidev->subsystem_device),
			le64_to_cpu(0x0000000000000000));
		ICAP_INFO(icap, "try load dsabin %s", fw_name);
		err = request_firmware(&fw, fw_name, &pcidev->dev);
	}
	if (err) {
		/* Give up on finding .dsabin. */
		ICAP_ERR(icap, "unable to find firmware, giving up");
		return err;
	}

	/* Grab lock and touch hardware. */
	mutex_lock(&icap->icap_lock);

	if (xocl_mb_sched_on(xdev)) {
		/* Try locating the microblaze binary. */
		bin_obj_axlf = (struct axlf *)fw->data;
		mbHeader = get_axlf_section_hdr(icap, bin_obj_axlf, SCHED_FIRMWARE);
		if (mbHeader) {
			mbBinaryOffset = mbHeader->m_sectionOffset;
			mbBinaryLength = mbHeader->m_sectionSize;
			length = bin_obj_axlf->m_header.m_length;
			xocl_mb_load_sche_image(xdev, fw->data + mbBinaryOffset,
				mbBinaryLength);
			ICAP_INFO(icap, "stashed mb sche binary");
			load_mbs = true;
		}
	}

	if (xocl_mb_mgmt_on(xdev)) {
		/* Try locating the board mgmt binary. */
		bin_obj_axlf = (struct axlf *)fw->data;
		mbHeader = get_axlf_section_hdr(icap, bin_obj_axlf, FIRMWARE);
		if (mbHeader) {
			mbBinaryOffset = mbHeader->m_sectionOffset;
			mbBinaryLength = mbHeader->m_sectionSize;
			length = bin_obj_axlf->m_header.m_length;
			xocl_mb_load_mgmt_image(xdev, fw->data + mbBinaryOffset,
				mbBinaryLength);
			ICAP_INFO(icap, "stashed mb mgmt binary");
			load_mbs = true;
		}
	}

	if (load_mbs)
		xocl_mb_reset(xdev);


	if (memcmp(fw->data, ICAP_XCLBIN_V2, sizeof(ICAP_XCLBIN_V2)) != 0) {
		ICAP_ERR(icap, "invalid firmware %s", fw_name);
		err = -EINVAL;
		goto done;
	}

	ICAP_INFO(icap, "boot_firmware in axlf format");
	bin_obj_axlf = (struct axlf *)fw->data;
	length = bin_obj_axlf->m_header.m_length;
	/* Match the xclbin with the hardware. */
	if (!xocl_verify_timestamp(xdev,
		bin_obj_axlf->m_header.m_featureRomTimeStamp)) {
		ICAP_ERR(icap, "timestamp of ROM did not match xclbin");
		err = -EINVAL;
		goto done;
	}
	ICAP_INFO(icap, "VBNV and timestamps matched");

	if (xocl_xrt_version_check(xdev, bin_obj_axlf, true)) {
		ICAP_ERR(icap, "Major version does not match xrt");
		err = -EINVAL;
		goto done;
	}
	ICAP_INFO(icap, "runtime version matched");

	primaryHeader = get_axlf_section_hdr(icap, bin_obj_axlf, BITSTREAM);
	secondaryHeader = get_axlf_section_hdr(icap, bin_obj_axlf,
		CLEARING_BITSTREAM);
	if (primaryHeader) {
		primaryFirmwareOffset = primaryHeader->m_sectionOffset;
		primaryFirmwareLength = primaryHeader->m_sectionSize;
	}
	if (secondaryHeader) {
		secondaryFirmwareOffset = secondaryHeader->m_sectionOffset;
		secondaryFirmwareLength = secondaryHeader->m_sectionSize;
	}

	if (length > fw->size) {
		err = -EINVAL;
		goto done;
	}

	if ((primaryFirmwareOffset + primaryFirmwareLength) > length) {
		err = -EINVAL;
		goto done;
	}

	if ((secondaryFirmwareOffset + secondaryFirmwareLength) > length) {
		err = -EINVAL;
		goto done;
	}

	if (primaryFirmwareLength) {
		ICAP_INFO(icap,
			"found second stage bitstream of size 0x%llx in %s",
			primaryFirmwareLength, fw_name);
		err = icap_download(icap, fw->data + primaryFirmwareOffset,
			primaryFirmwareLength);
		/*
		 * If we loaded a new second stage, we do not need the
		 * previously stashed clearing bitstream if any.
		 */
		free_clear_bitstream(icap);
		if (err) {
			ICAP_ERR(icap,
				"failed to download second stage bitstream");
			goto done;
		}
		ICAP_INFO(icap, "downloaded second stage bitstream");
	}

	/*
	 * If both primary and secondary bitstreams have been provided then
	 * ignore the previously stashed bitstream if any. If only secondary
	 * bitstream was provided, but we found a previously stashed bitstream
	 * we should use the latter since it is more appropriate for the
	 * current state of the device
	 */
	if (secondaryFirmwareLength && (primaryFirmwareLength ||
		!icap->icap_clear_bitstream)) {
		free_clear_bitstream(icap);
		icap->icap_clear_bitstream = vmalloc(secondaryFirmwareLength);
		if (!icap->icap_clear_bitstream) {
			err = -ENOMEM;
			goto done;
		}
		icap->icap_clear_bitstream_length = secondaryFirmwareLength;
		memcpy(icap->icap_clear_bitstream,
			fw->data + secondaryFirmwareOffset,
			icap->icap_clear_bitstream_length);
		ICAP_INFO(icap, "found clearing bitstream of size 0x%lx in %s",
			icap->icap_clear_bitstream_length, fw_name);
	} else if (icap->icap_clear_bitstream) {
		ICAP_INFO(icap,
			"using existing clearing bitstream of size 0x%lx",
		       icap->icap_clear_bitstream_length);
	}

	if (icap->icap_clear_bitstream &&
		bitstream_parse_header(icap, icap->icap_clear_bitstream,
		DMA_HWICAP_BITFILE_BUFFER_SIZE, &bit_header)) {
		err = -EINVAL;
		free_clear_bitstream(icap);
	}

done:
	mutex_unlock(&icap->icap_lock);
	release_firmware(fw);
	kfree(bit_header.DesignName);
	kfree(bit_header.PartName);
	kfree(bit_header.Date);
	kfree(bit_header.Time);
	ICAP_INFO(icap, "%s err: %ld", __func__, err);
	return err;
}


static long icap_download_clear_bitstream(struct icap *icap)
{
	long err = 0;
	const char *buffer = icap->icap_clear_bitstream;
	unsigned long length = icap->icap_clear_bitstream_length;

	ICAP_INFO(icap, "downloading clear bitstream of length 0x%lx", length);

	if (!buffer)
		return 0;

	err = icap_download(icap, buffer, length);

	free_clear_bitstream(icap);
	return err;
}

/*
 * This function should be called with icap_mutex lock held
 */
static long axlf_set_freqscaling(struct icap *icap, struct platform_device *pdev,
	const char *clk_buf, unsigned long length)
{
	struct clock_freq_topology *freqs = NULL;
	int clock_type_count = 0;
	int i = 0;
	struct clock_freq *freq = NULL;
	int data_clk_count = 0;
	int kernel_clk_count = 0;
	int system_clk_count = 0;
	unsigned short target_freqs[4] = {0};

	freqs = (struct clock_freq_topology *)clk_buf;
	if (freqs->m_count > 4) {
		ICAP_ERR(icap, "More than 4 clocks found in clock topology");
		return -EDOM;
	}

	/* Error checks - we support 1 data clk (reqd), one kernel clock(reqd) and
	 * at most 2 system clocks (optional/reqd for aws).
	 * Data clk needs to be the first entry, followed by kernel clock
	 * and then system clocks
	 */

	for (i = 0; i < freqs->m_count; i++) {
		freq = &(freqs->m_clock_freq[i]);
		if (freq->m_type == CT_DATA)
			data_clk_count++;
		if (freq->m_type == CT_KERNEL)
			kernel_clk_count++;
		if (freq->m_type == CT_SYSTEM)
			system_clk_count++;
	}

	if (data_clk_count != 1) {
		ICAP_ERR(icap, "Data clock not found in clock topology");
		return -EDOM;
	}
	if (kernel_clk_count != 1) {
		ICAP_ERR(icap, "Kernel clock not found in clock topology");
		return -EDOM;
	}
	if (system_clk_count > 2) {
		ICAP_ERR(icap,
			"More than 2 system clocks found in clock topology");
		return -EDOM;
	}

	for (i = 0; i < freqs->m_count; i++) {
		freq = &(freqs->m_clock_freq[i]);
		if (freq->m_type == CT_DATA)
			target_freqs[0] = freq->m_freq_Mhz;
	}

	for (i = 0; i < freqs->m_count; i++) {
		freq = &(freqs->m_clock_freq[i]);
		if (freq->m_type == CT_KERNEL)
			target_freqs[1] = freq->m_freq_Mhz;
	}

	clock_type_count = 2;
	for (i = 0; i < freqs->m_count; i++) {
		freq = &(freqs->m_clock_freq[i]);
		if (freq->m_type == CT_SYSTEM)
			target_freqs[clock_type_count++] = freq->m_freq_Mhz;
	}


	ICAP_INFO(icap, "setting clock freq, num: %lu, data_freq: %d , clk_freq: %d, sys_freq[0]: %d, sys_freq[1]: %d",
		ARRAY_SIZE(target_freqs), target_freqs[0], target_freqs[1],
		target_freqs[2], target_freqs[3]);
	return set_freqs(icap, target_freqs, 4);
}


static int icap_download_user(struct icap *icap, const char *bit_buf,
	unsigned long length)
{
	long err = 0;
	XHwIcap_Bit_Header bit_header = { 0 };
	unsigned numCharsRead = DMA_HWICAP_BITFILE_BUFFER_SIZE;
	unsigned byte_read;

	ICAP_INFO(icap, "downloading bitstream, length: %lu", length);

	icap_freeze_axi_gate(icap);

	err = icap_download_clear_bitstream(icap);
	if (err)
		goto free_buffers;

	if (bitstream_parse_header(icap, bit_buf,
		DMA_HWICAP_BITFILE_BUFFER_SIZE, &bit_header)) {
		err = -EINVAL;
		goto free_buffers;
	}
	if ((bit_header.HeaderLength + bit_header.BitstreamLength) > length) {
		err = -EINVAL;
		goto free_buffers;
	}

	bit_buf += bit_header.HeaderLength;
	for (byte_read = 0; byte_read < bit_header.BitstreamLength;
		byte_read += numCharsRead) {
		numCharsRead = bit_header.BitstreamLength - byte_read;
		if (numCharsRead > DMA_HWICAP_BITFILE_BUFFER_SIZE)
			numCharsRead = DMA_HWICAP_BITFILE_BUFFER_SIZE;

		err = bitstream_helper(icap, (u32 *)bit_buf,
			numCharsRead / sizeof(u32));
		if (err)
			goto free_buffers;

		bit_buf += numCharsRead;
	}

	err = wait_for_done(icap);
	if (err)
		goto free_buffers;

	/*
	 * Perform frequency scaling since PR download can silenty overwrite
	 * MMCM settings in static region changing the clock frequencies
	 * although ClockWiz CONFIG registers will misleading report the older
	 * configuration from before bitstream download as if nothing has
	 * changed.
	 */
	if (!err)
		err = icap_ocl_freqscaling(icap, true);

free_buffers:
	icap_free_axi_gate(icap);
	kfree(bit_header.DesignName);
	kfree(bit_header.PartName);
	kfree(bit_header.Date);
	kfree(bit_header.Time);
	return err;
}


static int __icap_lock_peer(struct platform_device *pdev, const xuid_t *id)
{
	int err = 0;
	struct icap *icap = platform_get_drvdata(pdev);
	int resp = 0;
	size_t resplen = sizeof(resp);
	struct mailbox_req_bitstream_lock bitstream_lock = {0};
	size_t data_len = sizeof(struct mailbox_req_bitstream_lock);
	struct mailbox_req *mb_req = NULL;
	size_t reqlen = sizeof(struct mailbox_req) + data_len;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	/* if there is no user there
	 * ask mgmt to lock the bitstream
	 */
	if (icap->icap_bitstream_ref == 0) {
		mb_req = vmalloc(reqlen);
		if (!mb_req) {
			err = -ENOMEM;
			goto done;
		}

		mb_req->req = MAILBOX_REQ_LOCK_BITSTREAM;
		uuid_copy((xuid_t *)bitstream_lock.uuid, id);

		memcpy(mb_req->data, &bitstream_lock, data_len);

		err = xocl_peer_request(xdev,
			mb_req, reqlen, &resp, &resplen, NULL, NULL);

		if (err) {
			err = -ENODEV;
			/*
			 * ignore error if aws
			 */
			if (xocl_is_aws(xdev))
				err = 0;
			goto done;
		}

		if (resp < 0) {
			err = resp;
			goto done;
		}
	}

done:
	vfree(mb_req);
	return err;
}

static int __icap_unlock_peer(struct platform_device *pdev, const xuid_t *id)
{
	int err = 0;
	struct icap *icap = platform_get_drvdata(pdev);
	struct mailbox_req_bitstream_lock bitstream_lock = {0};
	size_t data_len = sizeof(struct mailbox_req_bitstream_lock);
	struct mailbox_req *mb_req = NULL;
	size_t reqlen = sizeof(struct mailbox_req) + data_len;
	int resp = 0;
	size_t resplen = sizeof(resp);
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	/* if there is no user there
	 * ask mgmt to unlock the bitstream
	 */
	if (icap->icap_bitstream_ref == 0) {
		mb_req = vmalloc(reqlen);
		if (!mb_req) {
			err = -ENOMEM;
			goto done;
		}

		mb_req->req = MAILBOX_REQ_UNLOCK_BITSTREAM;
		memcpy(mb_req->data, &bitstream_lock, data_len);
		err = xocl_peer_request(XOCL_PL_DEV_TO_XDEV(pdev),
			mb_req, reqlen, &resp, &resplen, NULL, NULL);
		if (err) {
			err = -ENODEV;
			/*
			 * ignore error if aws
			 */
			if (xocl_is_aws(xdev))
				err = 0;
			goto done;
		}
	}
done:
	vfree(mb_req);
	return err;
}

static void icap_clean_axlf_section(struct icap *icap,
	enum axlf_section_kind kind)
{
	void **target = NULL;

	switch (kind) {
	case IP_LAYOUT:
		target = (void **)&icap->ip_layout;
		break;
	case MEM_TOPOLOGY:
		target = (void **)&icap->mem_topo;
		break;
	case DEBUG_IP_LAYOUT:
		target = (void **)&icap->debug_layout;
		break;
	case CONNECTIVITY:
		target = (void **)&icap->connectivity;
		break;
	default:
		break;
	}
	if (target) {
		vfree(*target);
		*target = NULL;
	}
}

static void icap_clean_bitstream_axlf(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);

	icap->icap_bitstream_id = 0;
	uuid_copy(&icap->icap_bitstream_uuid, &uuid_null);
	icap_clean_axlf_section(icap, IP_LAYOUT);
	icap_clean_axlf_section(icap, MEM_TOPOLOGY);
	icap_clean_axlf_section(icap, DEBUG_IP_LAYOUT);
	icap_clean_axlf_section(icap, CONNECTIVITY);
}

static int icap_download_bitstream_axlf(struct platform_device *pdev,
	const void *u_xclbin)
{
	/*
	 * decouple as 1. download xclbin, 2. parse xclbin 3. verify xclbin
	 */
	struct icap *icap = platform_get_drvdata(pdev);
	long err = 0;
	uint64_t primaryFirmwareOffset = 0;
	uint64_t primaryFirmwareLength = 0;
	uint64_t secondaryFirmwareOffset = 0;
	uint64_t secondaryFirmwareLength = 0;
	const struct axlf_section_header *primaryHeader = NULL;
	const struct axlf_section_header *clockHeader = NULL;
	const struct axlf_section_header *secondaryHeader = NULL;
	struct axlf *xclbin = (struct axlf *)u_xclbin;
	char *buffer;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	bool need_download;
	int msg = -ETIMEDOUT;
	size_t resplen = sizeof(msg);
	int pid = pid_nr(task_tgid(current));
	uint32_t data_len = 0;
	struct mailbox_req *mb_req = NULL;
	struct mailbox_bitstream_kaddr mb_addr = {0};
	xuid_t *peer_uuid;
	uint64_t ch_state = 0;

	xocl_mailbox_get(xdev, CHAN_STATE, &ch_state);

	if (memcmp(xclbin->m_magic, ICAP_XCLBIN_V2, sizeof(ICAP_XCLBIN_V2)))
		return -EINVAL;

	if (ICAP_PRIVILEGED(icap)) {
		if (xocl_xrt_version_check(xdev, xclbin, true)) {
			ICAP_ERR(icap, "XRT version does not match");
			return -EINVAL;
		}

		/* Match the xclbin with the hardware. */
		if (!xocl_verify_timestamp(xdev,
			xclbin->m_header.m_featureRomTimeStamp)) {
			ICAP_ERR(icap, "timestamp of ROM not match Xclbin");
			xocl_sysfs_error(xdev, "timestamp of ROM not match Xclbin");
			return -EINVAL;
		}

		mutex_lock(&icap->icap_lock);

		ICAP_INFO(icap,
			"incoming xclbin: %016llx, on device xclbin: %016llx",
			xclbin->m_uniqueId, icap->icap_bitstream_id);

		need_download = (icap->icap_bitstream_id != xclbin->m_uniqueId);

		mutex_unlock(&icap->icap_lock);

		if (!need_download) {
			ICAP_INFO(icap, "bitstream exists, skip downloading");
			return 0;
		}

		/*
		 * Find sections in xclbin.
		 */
		ICAP_INFO(icap, "finding CLOCK_FREQ_TOPOLOGY section");
		/* Read the CLOCK section but defer changing clocks to later */
		clockHeader = get_axlf_section_hdr(icap, xclbin,
			CLOCK_FREQ_TOPOLOGY);

		ICAP_INFO(icap, "finding bitstream sections");
		primaryHeader = get_axlf_section_hdr(icap, xclbin, BITSTREAM);
		if (primaryHeader == NULL) {
			err = -EINVAL;
			goto done;
		}
		primaryFirmwareOffset = primaryHeader->m_sectionOffset;
		primaryFirmwareLength = primaryHeader->m_sectionSize;

		secondaryHeader = get_axlf_section_hdr(icap, xclbin,
			CLEARING_BITSTREAM);
		if (secondaryHeader) {
			if (XOCL_PL_TO_PCI_DEV(pdev)->device == 0x7138) {
				err = -EINVAL;
				goto done;
			} else {
				secondaryFirmwareOffset =
					secondaryHeader->m_sectionOffset;
				secondaryFirmwareLength =
					secondaryHeader->m_sectionSize;
			}
		}

		mutex_lock(&icap->icap_lock);

		if (icap_bitstream_in_use(icap, 0)) {
			ICAP_ERR(icap, "bitstream is locked, can't download new one");
			err = -EBUSY;
			goto done;
		}

		/* All clear, go ahead and start fiddling with hardware */

		if (clockHeader != NULL) {
			uint64_t clockFirmwareOffset = clockHeader->m_sectionOffset;
			uint64_t clockFirmwareLength = clockHeader->m_sectionSize;

			buffer = (char *)xclbin;
			buffer += clockFirmwareOffset;
			err = axlf_set_freqscaling(icap, pdev, buffer, clockFirmwareLength);
			if (err)
				goto done;
			err = icap_setup_clock_freq_topology(icap, buffer, clockFirmwareLength);
			if (err)
				goto done;
		}

		buffer = (char *)xclbin;
		buffer += primaryFirmwareOffset;
		err = icap_download_user(icap, buffer, primaryFirmwareLength);
		if (err)
			goto done;

		buffer = (char *)u_xclbin;
		buffer += secondaryFirmwareOffset;
		err = icap_setup_clear_bitstream(icap, buffer, secondaryFirmwareLength);
		if (err)
			goto done;

		if ((xocl_is_unified(xdev) || XOCL_DSA_XPR_ON(xdev)))
			err = calibrate_mig(icap);
		if (err)
			goto done;
		/* Remember "this" bitstream, so avoid redownload the next time. */
		icap->icap_bitstream_id = xclbin->m_uniqueId;
		if (!uuid_is_null(&xclbin->m_header.uuid)) {
			uuid_copy(&icap->icap_bitstream_uuid, &xclbin->m_header.uuid);
		} else {
			/* Legacy xclbin, convert legacy id to new id */
			memcpy(&icap->icap_bitstream_uuid,
				&xclbin->m_header.m_timeStamp, 8);
		}
	} else {

		mutex_lock(&icap->icap_lock);

		if (icap_bitstream_in_use(icap, pid)) {
			if (!uuid_equal(&xclbin->m_header.uuid, &icap->icap_bitstream_uuid)) {
				err = -EBUSY;
				goto done;
			}
		}

		peer_uuid = (xuid_t *)icap_get_data_nolock(pdev, PEER_UUID);

		if (!uuid_equal(peer_uuid, &xclbin->m_header.uuid)) {

			/*
			 * Clean up and expire cache if we need to download xclbin
			 */
			memset(&icap->cache, 0, sizeof(struct xcl_hwicap));
			icap->cache_expires = ktime_sub(ktime_get_boottime(), ktime_set(1, 0));

			if ((ch_state & MB_PEER_SAME_DOMAIN) != 0) {
				data_len = sizeof(struct mailbox_req) + sizeof(struct mailbox_bitstream_kaddr);
				mb_req = vmalloc(data_len);
				if (!mb_req) {
					ICAP_ERR(icap, "Unable to create mb_req\n");
					err = -ENOMEM;
					goto done;
				}
				mb_req->req = MAILBOX_REQ_LOAD_XCLBIN_KADDR;
				mb_addr.addr = (uint64_t)xclbin;
				memcpy(mb_req->data, &mb_addr, sizeof(struct mailbox_bitstream_kaddr));

			} else {
				data_len = sizeof(struct mailbox_req) +
					xclbin->m_header.m_length;
				mb_req = vmalloc(data_len);
				if (!mb_req) {
					ICAP_ERR(icap, "Unable to create mb_req\n");
					err = -ENOMEM;
					goto done;
				}
				memcpy(mb_req->data, u_xclbin, xclbin->m_header.m_length);
				mb_req->req = MAILBOX_REQ_LOAD_XCLBIN;
			}
			(void) xocl_peer_request(xdev,
				mb_req, data_len, &msg, &resplen, NULL, NULL);

			/*
			 *  Ignore fail if it's an AWS device
			 */
			if (msg != 0 && !xocl_is_aws(xdev)) {
				ICAP_ERR(icap,
					"%s peer failed to download xclbin",
					__func__);
				err = -EFAULT;
				goto done;
			}
		} else {
			ICAP_INFO(icap, "Already downloaded xclbin ID: %016llx",
				xclbin->m_uniqueId);
		}

		icap->icap_bitstream_id = xclbin->m_uniqueId;
		if (!uuid_is_null(&xclbin->m_header.uuid)) {
			uuid_copy(&icap->icap_bitstream_uuid, &xclbin->m_header.uuid);
		} else {
			/* Legacy xclbin, convert legacy id to new id */
			memcpy(&icap->icap_bitstream_uuid,
				&xclbin->m_header.m_timeStamp, 8);
		}
	}

	if (ICAP_PRIVILEGED(icap)) {
		icap_parse_bitstream_axlf_section(pdev, xclbin, MEM_TOPOLOGY);
		icap_parse_bitstream_axlf_section(pdev, xclbin, IP_LAYOUT);
	} else {
		icap_parse_bitstream_axlf_section(pdev, xclbin, IP_LAYOUT);
		icap_parse_bitstream_axlf_section(pdev, xclbin, MEM_TOPOLOGY);
		icap_parse_bitstream_axlf_section(pdev, xclbin, CONNECTIVITY);
		icap_parse_bitstream_axlf_section(pdev, xclbin, DEBUG_IP_LAYOUT);
	}

	if (ICAP_PRIVILEGED(icap))
		err = icap_verify_bitstream_axlf(pdev, xclbin);

	/* if verify failed */
done:
	if (err)
		icap_clean_bitstream_axlf(pdev);
	mutex_unlock(&icap->icap_lock);
	vfree(mb_req);
	ICAP_INFO(icap, "%s err: %ld", __func__, err);
	return err;
}



static uint32_t convert_mem_type(const char *name)
{
	/* Use MEM_DDR3 as a invalid memory type. */
	enum MEM_TYPE mem_type = MEM_DDR3;

	if (!strncasecmp(name, "DDR", 3))
		mem_type = MEM_DRAM;
	else if (!strncasecmp(name, "HBM", 3))
		mem_type = MEM_HBM;

	return mem_type;
}

static uint16_t icap_get_memidx(struct icap *icap, enum MEM_TYPE mem_type, int idx)
{
	uint16_t memidx = INVALID_MEM_IDX, i, mem_idx = 0;
	enum MEM_TYPE m_type;

	if (!icap->mem_topo)
		goto done;

	for (i = 0; i < icap->mem_topo->m_count; ++i) {
		/* Don't trust m_type in xclbin, convert name to m_type instead.
		 * m_tag[i] = "HBM[0]" -> m_type = MEM_HBM
		 * m_tag[i] = "DDR[1]" -> m_type = MEM_DRAM
		 */
		m_type = convert_mem_type(icap->mem_topo->m_mem_data[i].m_tag);
		if (m_type == mem_type) {
			if (idx == mem_idx)
				return i;
			mem_idx++;
		}
	}

done:
	return memidx;
}


static int icap_verify_bitstream_axlf(struct platform_device *pdev,
	struct axlf *xclbin)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0, i;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	bool dna_check = false;
	uint64_t section_size = 0;

	/* Destroy all dynamically add sub-devices*/
	xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_DNA);
	xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_MIG);
	xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_MIG_HBM);
	/*
	 * Add sub device dynamically.
	 * restrict any dynamically added sub-device and 1 base address,
	 * Has pre-defined length
	 *  Ex:    "ip_data": {
	 *         "m_type": "IP_DNASC",
	 *         "properties": "0x0",
	 *         "m_base_address": "0x1100000", <--  base address
	 *         "m_name": "slr0\/dna_self_check_0"
	 */

	if (!icap->ip_layout) {
		err = -EFAULT;
		goto done;
	}
	for (i = 0; i < icap->ip_layout->m_count; ++i) {
		struct ip_data *ip = &icap->ip_layout->m_ip_data[i];

		if (ip->m_type == IP_KERNEL)
			continue;

		if (ip->m_type == IP_DDR4_CONTROLLER) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_MIG;
			uint32_t memidx = ip->properties;

			if (!icap->mem_topo || ip->properties >= icap->mem_topo->m_count ||
				icap->mem_topo->m_mem_data[memidx].m_type !=
				MEM_DDR4) {
				ICAP_ERR(icap, "bad ECC controller index: %u",
					ip->properties);
				continue;
			}
			if (!icap->mem_topo->m_mem_data[memidx].m_used) {
				ICAP_INFO(icap,
					"ignore ECC controller for: %s",
					icap->mem_topo->m_mem_data[memidx].m_tag);
				continue;
			}

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data =
				icap->mem_topo->m_mem_data[memidx].m_tag;
			subdev_info.data_len =
				sizeof(icap->mem_topo->m_mem_data[memidx].m_tag);
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create MIG subdev");
				goto done;
			}
		} else if (ip->m_type == IP_MEM_DDR4) {
			/*
			 * Get global memory index by feeding desired memory type and index
			 */
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_MIG;
			uint16_t memidx = icap_get_memidx(icap, MEM_DRAM, ip->properties);

			if (memidx == INVALID_MEM_IDX)
				continue;

			if (!icap->mem_topo || memidx >= icap->mem_topo->m_count ||
				icap->mem_topo->m_mem_data[memidx].m_type !=
				MEM_DRAM) {
				ICAP_ERR(icap, "bad ECC controller index: %u",
					ip->properties);
				continue;
			}
			if (!icap->mem_topo->m_mem_data[memidx].m_used) {
				ICAP_INFO(icap,
					"ignore ECC controller for: %s",
					icap->mem_topo->m_mem_data[memidx].m_tag);
				continue;
			}

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data =
				icap->mem_topo->m_mem_data[memidx].m_tag;
			subdev_info.data_len =
				sizeof(icap->mem_topo->m_mem_data[memidx].m_tag);
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create MIG subdev");
				goto done;
			}
		} else if (ip->m_type == IP_MEM_HBM) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_MIG_HBM;
			uint16_t memidx = icap_get_memidx(icap, MEM_HBM, ip->indices.m_index);

			if (memidx == INVALID_MEM_IDX)
				continue;

			if (!icap->mem_topo || memidx >= icap->mem_topo->m_count) {
				ICAP_ERR(icap, "bad ECC controller index: %u",
					ip->properties);
				continue;
			}

			if (!icap->mem_topo->m_mem_data[memidx].m_used) {
				ICAP_INFO(icap,
					"ignore ECC controller for: %s",
					icap->mem_topo->m_mem_data[memidx].m_tag);
				continue;
			}

			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			subdev_info.priv_data =
				icap->mem_topo->m_mem_data[memidx].m_tag;
			subdev_info.data_len =
				sizeof(icap->mem_topo->m_mem_data[memidx].m_tag);
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create MIG_HBM subdev");
				goto done;
			}
		} else if (ip->m_type == IP_DNASC) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_DNA;

			dna_check = true;
			subdev_info.res[0].start += ip->m_base_address;
			subdev_info.res[0].end += ip->m_base_address;
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create DNA subdev");
				goto done;
			}
		}
	}

	if (dna_check) {
		bool is_axi = ((xocl_dna_capability(xdev) & 0x1) != 0);

		/*
		 * Any error occurs here should return -EACCES for app to
		 * know that DNA has failed.
		 */
		err = -EACCES;

		ICAP_INFO(icap, "DNA version: %s", is_axi ? "AXI" : "BRAM");

		if (is_axi) {
			uint32_t *cert = NULL;

			if (alloc_and_get_axlf_section(icap, xclbin,
				DNA_CERTIFICATE,
				(void **)&cert, &section_size) != 0) {

				/* We keep dna sub device if IP_DNASC presents */
				ICAP_ERR(icap, "Can't get certificate section");
				goto dna_cert_fail;
			}

			ICAP_INFO(icap, "DNA Certificate Size 0x%llx", section_size);
			if (section_size % 64 || section_size < 576)
				ICAP_ERR(icap, "Invalid certificate size");
			else
				xocl_dna_write_cert(xdev, cert, section_size);

			vfree(cert);
		}

		/* Check DNA validation result. */
		if (0x1 & xocl_dna_status(xdev))
			err = 0; /* xclbin is valid */
		else {
			ICAP_ERR(icap, "DNA inside xclbin is invalid");
			goto dna_cert_fail;
		}
	}

done:
	if (err) {
		xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_DNA);
		xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_MIG);
		xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_MIG_HBM);
	}
dna_cert_fail:
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

	if (icap_bitstream_in_use(icap, 0)) {
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
	pid_t pid)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0;

	if (uuid_is_null(id)) {
		ICAP_ERR(icap, "proc %d invalid UUID", pid);
		return -EINVAL;
	}

	mutex_lock(&icap->icap_lock);

	if (!ICAP_PRIVILEGED(icap)) {
		err = __icap_lock_peer(pdev, id);
		if (err < 0)
			goto done;
	}

	if (uuid_equal(id, &icap->icap_bitstream_uuid))
		err = add_user(icap, pid);
	else
		err = -EBUSY;

	if (err >= 0)
		err = icap->icap_bitstream_ref;

	ICAP_INFO(icap, "proc %d try to lock bitstream %pUb, ref=%d, err=%d",
		  pid, id, icap->icap_bitstream_ref, err);
done:
	mutex_unlock(&icap->icap_lock);

	if (!ICAP_PRIVILEGED(icap) && (err == 1)) /* reset on first reference */
		xocl_exec_reset(xocl_get_xdev(pdev));

	if (err >= 0)
		err = 0;

	return err;
}

static int icap_unlock_bitstream(struct platform_device *pdev, const xuid_t *id,
	pid_t pid)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0;

	if (id == NULL)
		id = &uuid_null;

	mutex_lock(&icap->icap_lock);

	/* Force unlock. */
	if (uuid_is_null(id))
		del_all_users(icap);
	else if (uuid_equal(id, &icap->icap_bitstream_uuid))
		err = del_user(icap, pid);
	else
		err = -EINVAL;

	if (!ICAP_PRIVILEGED(icap))
		__icap_unlock_peer(pdev, id);

	if (err >= 0)
		err = icap->icap_bitstream_ref;

	if (!ICAP_PRIVILEGED(icap)) {
		if (err == 0)
			xocl_exec_stop(xocl_get_xdev(pdev));
	}

	ICAP_INFO(icap, "proc %d try to unlock bitstream %pUb, ref=%d, err=%d",
		  pid, id, icap->icap_bitstream_ref, err);

	mutex_unlock(&icap->icap_lock);
	if (err >= 0)
		err = 0;
	return err;
}

static int icap_parse_bitstream_axlf_section(struct platform_device *pdev,
	const struct axlf *xclbin, enum axlf_section_kind kind)
{
	struct icap *icap = platform_get_drvdata(pdev);
	long err = 0;
	uint64_t section_size = 0, sect_sz = 0;
	void **target = NULL;

	if (memcmp(xclbin->m_magic, ICAP_XCLBIN_V2, sizeof(ICAP_XCLBIN_V2)))
		return -EINVAL;

	switch (kind) {
	case IP_LAYOUT:
		target = (void **)&icap->ip_layout;
		break;
	case MEM_TOPOLOGY:
		target = (void **)&icap->mem_topo;
		break;
	case DEBUG_IP_LAYOUT:
		target = (void **)&icap->debug_layout;
		break;
	case CONNECTIVITY:
		target = (void **)&icap->connectivity;
		break;
	default:
		break;
	}
	if (target) {
		vfree(*target);
		*target = NULL;
	}
	err = alloc_and_get_axlf_section(icap, xclbin, kind,
		target, &section_size);
	if (err != 0)
		goto done;
	sect_sz = icap_get_section_size(icap, kind);
	if (sect_sz > section_size) {
		err = -EINVAL;
		goto done;
	}
done:
	if (err) {
		vfree(*target);
		*target = NULL;
	}
	ICAP_INFO(icap, "%s kind %d, err: %ld", __func__, kind, err);
	return err;
}


/*
 * should always get the latest value of IDCODE and PEER_UUID
 */
static bool get_latest_force(enum data_kind kind)
{
	bool ret = false;

	switch (kind) {
	case IDCODE:
		ret = true;
		break;
	case PEER_UUID:
		ret = true;
		break;
	default:
		break;
	}
	return ret;
}

static uint64_t icap_get_data_nolock(struct platform_device *pdev,
	enum data_kind kind)
{
	struct icap *icap = platform_get_drvdata(pdev);
	ktime_t now = ktime_get_boottime();
	uint64_t target = 0;

	if (!ICAP_PRIVILEGED(icap)) {

		if (ktime_compare(now, icap->cache_expires) > 0 || get_latest_force(kind))
			icap_read_from_peer(pdev);

		switch (kind) {
		case IPLAYOUT_AXLF:
			target = (uint64_t)icap->ip_layout;
			break;
		case MEMTOPO_AXLF:
			target = (uint64_t)icap->mem_topo;
			break;
		case DEBUG_IPLAYOUT_AXLF:
			target = (uint64_t)icap->debug_layout;
			break;
		case CONNECTIVITY_AXLF:
			target = (uint64_t)icap->connectivity;
			break;
		case XCLBIN_UUID:
			target = (uint64_t)&icap->icap_bitstream_uuid;
			break;
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
		default:
			break;
		}


	} else {
		switch (kind) {
		case IPLAYOUT_AXLF:
			target = (uint64_t)icap->ip_layout;
			break;
		case MEMTOPO_AXLF:
			target = (uint64_t)icap->mem_topo;
			break;
		case DEBUG_IPLAYOUT_AXLF:
			target = (uint64_t)icap->debug_layout;
			break;
		case CONNECTIVITY_AXLF:
			target = (uint64_t)icap->connectivity;
			break;
		case IDCODE:
			target = icap->idcode;
			break;
		case XCLBIN_UUID:
			target = (uint64_t)&icap->icap_bitstream_uuid;
			break;
		case CLOCK_FREQ_0:
			target = icap_get_ocl_frequency(icap, 0);
			break;
		case CLOCK_FREQ_1:
			target = icap_get_ocl_frequency(icap, 1);
			break;
		case CLOCK_FREQ_2:
			target = icap_get_ocl_frequency(icap, 2);
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

/* Kernel APIs exported from this sub-device driver. */
static struct xocl_icap_funcs icap_ops = {
	.reset_axi_gate = platform_reset_axi_gate,
	.reset_bitstream = icap_reset_bitstream,
	.download_boot_firmware = icap_download_boot_firmware,
	.download_bitstream_axlf = icap_download_bitstream_axlf,
	.ocl_set_freq = icap_ocl_set_freqscaling,
	.ocl_get_freq = icap_ocl_get_freqscaling,
	.ocl_update_clock_freq_topology = icap_ocl_update_clock_freq_topology,
	.ocl_lock_bitstream = icap_lock_bitstream,
	.ocl_unlock_bitstream = icap_unlock_bitstream,
	.get_data = icap_get_data,
};

static ssize_t clock_freq_topology_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	mutex_lock(&icap->icap_lock);
	if (ICAP_PRIVILEGED(icap)) {
		memcpy(buf, icap->icap_clock_freq_topology, icap->icap_clock_freq_topology_length);
		cnt = icap->icap_clock_freq_topology_length;
	}
	mutex_unlock(&icap->icap_lock);

	return cnt;

}

static DEVICE_ATTR_RO(clock_freq_topology);

static ssize_t clock_freqs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;
	int i;
	u32 freq_counter, freq, request_in_khz, tolerance;

	mutex_lock(&icap->icap_lock);

	for (i = 0; i < ICAP_MAX_NUM_CLOCKS; i++) {
		freq = icap_get_ocl_frequency(icap, i);
		if (!uuid_is_null(&icap->icap_bitstream_uuid)) {
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

	return cnt;
}
static DEVICE_ATTR_RO(clock_freqs);

static ssize_t icap_rl_program(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t off, size_t count)
{
	XHwIcap_Bit_Header bit_header = { 0 };
	struct device *dev = container_of(kobj, struct device, kobj);
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	ssize_t ret = count;

	if (off == 0) {
		if (count < DMA_HWICAP_BITFILE_BUFFER_SIZE) {
			ICAP_ERR(icap, "count is too small %ld", count);
			return -EINVAL;
		}

		if (bitstream_parse_header(icap, buffer,
			DMA_HWICAP_BITFILE_BUFFER_SIZE, &bit_header)) {
			ICAP_ERR(icap, "parse header failed");
			return -EINVAL;
		}

		icap->bit_length = bit_header.HeaderLength +
			bit_header.BitstreamLength;
		icap->bit_buffer = vmalloc(icap->bit_length);
	}

	if (off + count >= icap->bit_length) {
		/*
		 * assumes all subdevices are removed at this time
		 */
		memcpy(icap->bit_buffer + off, buffer, icap->bit_length - off);
		icap_freeze_axi_gate_shell(icap);
		ret = icap_download(icap, icap->bit_buffer, icap->bit_length);
		if (ret) {
			ICAP_ERR(icap, "bitstream download failed");
			ret = -EIO;
		} else {
			ret = count;
		}
		icap_free_axi_gate_shell(icap);
		/* has to reset pci, otherwise firewall trips */
		xocl_reset(xocl_get_xdev(icap->icap_pdev));
		icap->icap_bitstream_id = 0;
		memset(&icap->icap_bitstream_uuid, 0, sizeof(xuid_t));
		vfree(icap->bit_buffer);
		icap->bit_buffer = NULL;
	} else {
		memcpy(icap->bit_buffer + off, buffer, count);
	}

	return ret;
}

static struct bin_attribute shell_program_attr = {
	.attr = {
		.name = "shell_program",
		.mode = 0200
	},
	.read = NULL,
	.write = icap_rl_program,
	.size = 0
};

static struct bin_attribute *icap_mgmt_bin_attrs[] = {
	&shell_program_attr,
	NULL,
};

static struct attribute_group icap_mgmt_bin_attr_group = {
	.bin_attrs = icap_mgmt_bin_attrs,
};

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

static struct attribute *icap_attrs[] = {
	&dev_attr_clock_freq_topology.attr,
	&dev_attr_clock_freqs.attr,
	&dev_attr_idcode.attr,
	&dev_attr_cache_expire_secs.attr,
	NULL,
};

/*- Debug IP_layout-- */
static ssize_t icap_read_debug_ip_layout(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap;
	u32 nread = 0;
	size_t size = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (!icap || !icap->debug_layout)
		return 0;

	mutex_lock(&icap->icap_lock);

	size = sizeof_sect(icap->debug_layout, m_debug_ip_data);
	if (offset >= size)
		goto unlock;

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, ((char *)icap->debug_layout) + offset, nread);

unlock:
	mutex_unlock(&icap->icap_lock);
	return nread;
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
	struct icap *icap;
	u32 nread = 0;
	size_t size = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (!icap || !icap->ip_layout)
		return 0;

	mutex_lock(&icap->icap_lock);

	size = sizeof_sect(icap->ip_layout, m_ip_data);
	if (offset >= size)
		goto unlock;

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, ((char *)icap->ip_layout) + offset, nread);

unlock:
	mutex_unlock(&icap->icap_lock);
	return nread;
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

/* -Connectivity-- */
static ssize_t icap_read_connectivity(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap;
	u32 nread = 0;
	size_t size = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (!icap || !icap->connectivity)
		return 0;

	mutex_lock(&icap->icap_lock);

	size = sizeof_sect(icap->connectivity, m_connection);
	if (offset >= size)
		goto unlock;

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, ((char *)icap->connectivity) + offset, nread);

unlock:
	mutex_unlock(&icap->icap_lock);
	return nread;
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


/* -Mem_topology-- */
static ssize_t icap_read_mem_topology(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap;
	u32 nread = 0;
	size_t size = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (!icap || !icap->mem_topo)
		return 0;

	mutex_lock(&icap->icap_lock);

	size = sizeof_sect(icap->mem_topo, m_mem_data);
	if (offset >= size)
		goto unlock;

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, ((char *)icap->mem_topo) + offset, nread);
unlock:
	mutex_unlock(&icap->icap_lock);
	return nread;
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

static struct bin_attribute *icap_bin_attrs[] = {
	&debug_ip_layout_attr,
	&ip_layout_attr,
	&connectivity_attr,
	&mem_topology_attr,
	NULL,
};

static struct attribute_group icap_attr_group = {
	.attrs = icap_attrs,
	.bin_attrs = icap_bin_attrs,
};

static int icap_remove(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int i;

	BUG_ON(icap == NULL);

	del_all_users(icap);

	xocl_subdev_register(pdev, XOCL_SUBDEV_ICAP, NULL);

	if (ICAP_PRIVILEGED(icap))
		sysfs_remove_group(&pdev->dev.kobj, &icap_mgmt_bin_attr_group);

	if (icap->bit_buffer)
		vfree(icap->bit_buffer);

	iounmap(icap->icap_regs);
	iounmap(icap->icap_state);
	iounmap(icap->icap_axi_gate);
	for (i = 0; i < ICAP_MAX_NUM_CLOCKS; i++)
		iounmap(icap->icap_clock_bases[i]);
	free_clear_bitstream(icap);
	free_clock_freq_topology(icap);

	sysfs_remove_group(&pdev->dev.kobj, &icap_attr_group);

	ICAP_INFO(icap, "cleaned up successfully");
	platform_set_drvdata(pdev, NULL);
	vfree(icap->mem_topo);
	vfree(icap->ip_layout);
	vfree(icap->debug_layout);
	vfree(icap->connectivity);
	kfree(icap);
	return 0;
}

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
	int reg_grp;
	void **regs;

	icap = kzalloc(sizeof(struct icap), GFP_KERNEL);
	if (!icap)
		return -ENOMEM;
	platform_set_drvdata(pdev, icap);
	icap->icap_pdev = pdev;
	mutex_init(&icap->icap_lock);
	INIT_LIST_HEAD(&icap->icap_bitstream_users);

	for (reg_grp = 0; reg_grp < ICAP_MAX_REG_GROUPS; reg_grp++) {
		switch (reg_grp) {
		case 0:
			regs = (void **)&icap->icap_regs;
			break;
		case 1:
			regs = (void **)&icap->icap_state;
			break;
		case 2:
			regs = (void **)&icap->icap_axi_gate;
			break;
		case 3:
			regs = (void **)&icap->icap_clock_bases[0];
			break;
		case 4:
			regs = (void **)&icap->icap_clock_bases[1];
			break;
		case 5:
			regs = (void **)&icap->icap_clock_freq_counter;
			break;
		case 6:
			regs = (void **)&icap->icap_clock_bases[2];
			break;
		case 7:
			regs = (void **)&icap->icap_clock_freq_counter_hbm;
			break;
		default:
			BUG();
			break;
		}
		res = platform_get_resource(pdev, IORESOURCE_MEM, reg_grp);
		if (res != NULL) {
			*regs = ioremap_nocache(res->start,
				res->end - res->start + 1);
			if (*regs == NULL) {
				ICAP_ERR(icap,
					"failed to map in register group: %d",
					reg_grp);
				ret = -EIO;
				goto failed;
			} else {
				ICAP_INFO(icap,
					"mapped in register group %d @ 0x%p",
					reg_grp, *regs);
			}
		} else
			break;
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &icap_attr_group);
	if (ret) {
		ICAP_ERR(icap, "create icap attrs failed: %d", ret);
		goto failed;
	}

	if (ICAP_PRIVILEGED(icap)) {
		ret = sysfs_create_group(&pdev->dev.kobj,
			&icap_mgmt_bin_attr_group);
		if (ret) {
			ICAP_ERR(icap, "create icap attrs failed: %d", ret);
			goto failed;
		}
	}

	icap->cache_expire_secs = ICAP_DEFAULT_EXPIRE_SECS;

	icap_probe_chip(icap);
	ICAP_INFO(icap, "successfully initialized FPGA IDCODE 0x%x",
			icap->idcode);
	xocl_subdev_register(pdev, XOCL_SUBDEV_ICAP, &icap_ops);
	return 0;

failed:
	(void) icap_remove(pdev);
	return ret;
}


struct platform_device_id icap_id_table[] = {
	{ XOCL_ICAP, 0 },
	{ },
};

static struct platform_driver icap_driver = {
	.probe		= icap_probe,
	.remove		= icap_remove,
	.driver		= {
		.name	= XOCL_ICAP,
	},
	.id_table = icap_id_table,
};

int __init xocl_init_icap(void)
{
	return platform_driver_register(&icap_driver);
}

void xocl_fini_icap(void)
{
	platform_driver_unregister(&icap_driver);
}
