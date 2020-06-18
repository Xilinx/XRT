/**
 *  Copyright (C) 2017-2020 Xilinx, Inc. All rights reserved.
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
#include "../xocl_drv.h"
#include "../xocl_drm.h"
#include "mgmt-ioctl.h"

#if PF == MGMTPF
int kds_mode = 0;
#else
extern int kds_mode;
#endif

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
#define DMA_HWICAP_BITFILE_BUFFER_SIZE 1024

/*
 * Block comment for spliting old icap into subdevs (icap, clock, xclbin, etc.)
 * Current design: all-in-one icap
 * Future design: multiple subdevs with their own territory
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
 */

/*
 * Note: there are 2 max num clocks, ICAP_MAX_... and CLOCK_MAX_...,
 * those should be concept from XCLBIN_MAX_... in the future.
 */
#define	ICAP_MAX_NUM_CLOCKS		4
#define ICAP_DEFAULT_EXPIRE_SECS	1

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

	xuid_t			icap_bitstream_uuid;
	int			icap_bitstream_ref;

	struct clock_freq_topology *xclbin_clock_freq_topology;
	unsigned long		xclbin_clock_freq_topology_length;
	struct mem_topology	*mem_topo;
	struct ip_layout	*ip_layout;
	struct debug_ip_layout	*debug_layout;
	struct connectivity	*connectivity;
	uint64_t		max_host_mem_aperture;
	void			*partition_metadata;

	void			*rp_bit;
	unsigned long		rp_bit_len;
	void			*rp_fdt;
	unsigned long		rp_fdt_len;
	void			*rp_mgmt_bin;
	unsigned long		rp_mgmt_bin_len;
	void			*rp_sche_bin;
	unsigned long		rp_sche_bin_len;
	void			*rp_sc_bin;
	unsigned long		*rp_sc_bin_len;
	char			rp_vbnv[128];

	struct bmc		bmc_header;

	uint64_t		cache_expire_secs;
	struct xcl_pr_region	cache;
	ktime_t			cache_expires;

	enum icap_sec_level	sec_level;


	/* Use reader_ref as xclbin metadata reader counter
	 * Ther reference count increases by 1
	 * if icap_xclbin_rd_lock get called.
	 */
	u64			busy;
	int			reader_ref;
	wait_queue_head_t	reader_wq;

	uint32_t		data_retention;
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
	unsigned config2;
} frequency_table[] = {
	{/*1275.000*/   10.000, 	0x02EE0C01,     0x0001F47F},
	{/*1575.000*/   15.000, 	0x02EE0F01,     0x00000069},
	{/*1600.000*/   20.000, 	0x00001001,     0x00000050},
	{/*1600.000*/   25.000, 	0x00001001,     0x00000040},
	{/*1575.000*/   30.000, 	0x02EE0F01,     0x0001F434},
	{/*1575.000*/   35.000, 	0x02EE0F01,     0x0000002D},
	{/*1600.000*/   40.000, 	0x00001001,     0x00000028},
	{/*1575.000*/   45.000, 	0x02EE0F01,     0x00000023},
	{/*1600.000*/   50.000, 	0x00001001,     0x00000020},
	{/*1512.500*/   55.000, 	0x007D0F01,     0x0001F41B},
	{/*1575.000*/   60.000, 	0x02EE0F01,     0x0000FA1A},
	{/*1462.500*/   65.000, 	0x02710E01,     0x0001F416},
	{/*1575.000*/   70.000, 	0x02EE0F01,     0x0001F416},
	{/*1575.000*/   75.000, 	0x02EE0F01,     0x00000015},
	{/*1600.000*/   80.000, 	0x00001001,     0x00000014},
	{/*1487.500*/   85.000, 	0x036B0E01,     0x0001F411},
	{/*1575.000*/   90.000, 	0x02EE0F01,     0x0001F411},
	{/*1425.000*/   95.000, 	0x00FA0E01,     0x0000000F},
	{/*1600.000*/   100.000,        0x00001001,     0x00000010},
	{/*1575.000*/   105.000,        0x02EE0F01,     0x0000000F},
	{/*1512.500*/   110.000,        0x007D0F01,     0x0002EE0D},
	{/*1437.500*/   115.000,        0x01770E01,     0x0001F40C},
	{/*1575.000*/   120.000,        0x02EE0F01,     0x00007D0D},
	{/*1562.500*/   125.000,        0x02710F01,     0x0001F40C},
	{/*1462.500*/   130.000,        0x02710E01,     0x0000FA0B},
	{/*1350.000*/   135.000,        0x01F40D01,     0x0000000A},
	{/*1575.000*/   140.000,        0x02EE0F01,     0x0000FA0B},
	{/*1450.000*/   145.000,        0x01F40E01,     0x0000000A},
	{/*1575.000*/   150.000,        0x02EE0F01,     0x0001F40A},
	{/*1550.000*/   155.000,        0x01F40F01,     0x0000000A},
	{/*1600.000*/   160.000,        0x00001001,     0x0000000A},
	{/*1237.500*/   165.000,        0x01770C01,     0x0001F407},
	{/*1487.500*/   170.000,        0x036B0E01,     0x0002EE08},
	{/*1575.000*/   175.000,        0x02EE0F01,     0x00000009},
	{/*1575.000*/   180.000,        0x02EE0F01,     0x0002EE08},
	{/*1387.500*/   185.000,        0x036B0D01,     0x0001F407},
	{/*1425.000*/   190.000,        0x00FA0E01,     0x0001F407},
	{/*1462.500*/   195.000,        0x02710E01,     0x0001F407},
	{/*1600.000*/   200.000,        0x00001001,     0x00000008},
	{/*1537.500*/   205.000,        0x01770F01,     0x0001F407},
	{/*1575.000*/   210.000,        0x02EE0F01,     0x0001F407},
	{/*1075.000*/   215.000,        0x02EE0A01,     0x00000005},
	{/*1512.500*/   220.000,        0x007D0F01,     0x00036B06},
	{/*1575.000*/   225.000,        0x02EE0F01,     0x00000007},
	{/*1437.500*/   230.000,        0x01770E01,     0x0000FA06},
	{/*1175.000*/   235.000,        0x02EE0B01,     0x00000005},
	{/*1500.000*/   240.000,        0x00000F01,     0x0000FA06},
	{/*1225.000*/   245.000,        0x00FA0C01,     0x00000005},
	{/*1562.500*/   250.000,        0x02710F01,     0x0000FA06},
	{/*1275.000*/   255.000,        0x02EE0C01,     0x00000005},
	{/*1462.500*/   260.000,        0x02710E01,     0x00027105},
	{/*1325.000*/   265.000,        0x00FA0D01,     0x00000005},
	{/*1350.000*/   270.000,        0x01F40D01,     0x00000005},
	{/*1512.500*/   275.000,        0x007D0F01,     0x0001F405},
	{/*1575.000*/   280.000,        0x02EE0F01,     0x00027105},
	{/*1425.000*/   285.000,        0x00FA0E01,     0x00000005},
	{/*1450.000*/   290.000,        0x01F40E01,     0x00000005},
	{/*1475.000*/   295.000,        0x02EE0E01,     0x00000005},
	{/*1575.000*/   300.000,        0x02EE0F01,     0x0000FA05},
	{/*1525.000*/   305.000,        0x00FA0F01,     0x00000005},
	{/*1550.000*/   310.000,        0x01F40F01,     0x00000005},
	{/*1575.000*/   315.000,        0x02EE0F01,     0x00000005},
	{/*1600.000*/   320.000,        0x00001001,     0x00000005},
	{/*1462.500*/   325.000,        0x02710E01,     0x0001F404},
	{/*1237.500*/   330.000,        0x01770C01,     0x0002EE03},
	{/*837.500*/    335.000,        0x01770801,     0x0001F402},
	{/*1487.500*/   340.000,        0x036B0E01,     0x00017704},
	{/*862.500*/    345.000,        0x02710801,     0x0001F402},
	{/*1575.000*/   350.000,        0x02EE0F01,     0x0001F404},
	{/*887.500*/    355.000,        0x036B0801,     0x0001F402},
	{/*1575.000*/   360.000,        0x02EE0F01,     0x00017704},
	{/*912.500*/    365.000,        0x007D0901,     0x0001F402},
	{/*1387.500*/   370.000,        0x036B0D01,     0x0002EE03},
	{/*1500.000*/   375.000,        0x00000F01,     0x00000004},
	{/*1425.000*/   380.000,        0x00FA0E01,     0x0002EE03},
	{/*962.500*/    385.000,        0x02710901,     0x0001F402},
	{/*1462.500*/   390.000,        0x02710E01,     0x0002EE03},
	{/*987.500*/    395.000,        0x036B0901,     0x0001F402},
	{/*1600.000*/   400.000,        0x00001001,     0x00000004},
	{/*1012.500*/   405.000,        0x007D0A01,     0x0001F402},
	{/*1537.500*/   410.000,        0x01770F01,     0x0002EE03},
	{/*1037.500*/   415.000,        0x01770A01,     0x0001F402},
	{/*1575.000*/   420.000,        0x02EE0F01,     0x0002EE03},
	{/*1487.500*/   425.000,        0x036B0E01,     0x0001F403},
	{/*1075.000*/   430.000,        0x02EE0A01,     0x0001F402},
	{/*1087.500*/   435.000,        0x036B0A01,     0x0001F402},
	{/*1375.000*/   440.000,        0x02EE0D01,     0x00007D03},
	{/*1112.500*/   445.000,        0x007D0B01,     0x0001F402},
	{/*1575.000*/   450.000,        0x02EE0F01,     0x0001F403},
	{/*1137.500*/   455.000,        0x01770B01,     0x0001F402},
	{/*1437.500*/   460.000,        0x01770E01,     0x00007D03},
	{/*1162.500*/   465.000,        0x02710B01,     0x0001F402},
	{/*1175.000*/   470.000,        0x02EE0B01,     0x0001F402},
	{/*1425.000*/   475.000,        0x00FA0E01,     0x00000003},
	{/*1500.000*/   480.000,        0x00000F01,     0x00007D03},
	{/*1212.500*/   485.000,        0x007D0C01,     0x0001F402},
	{/*1225.000*/   490.000,        0x00FA0C01,     0x0001F402},
	{/*1237.500*/   495.000,        0x01770C01,     0x0001F402},
	{/*1562.500*/   500.000,        0x02710F01,     0x00007D03},
	{/*1262.500*/   505.000,        0x02710C01,     0x0001F402},
	{/*1275.000*/   510.000,        0x02EE0C01,     0x0001F402},
	{/*1287.500*/   515.000,        0x036B0C01,     0x0001F402},
	{/*1300.000*/   520.000,        0x00000D01,     0x0001F402},
	{/*1575.000*/   525.000,        0x02EE0F01,     0x00000003},
	{/*1325.000*/   530.000,        0x00FA0D01,     0x0001F402},
	{/*1337.500*/   535.000,        0x01770D01,     0x0001F402},
	{/*1350.000*/   540.000,        0x01F40D01,     0x0001F402},
	{/*1362.500*/   545.000,        0x02710D01,     0x0001F402},
	{/*1512.500*/   550.000,        0x007D0F01,     0x0002EE02},
	{/*1387.500*/   555.000,        0x036B0D01,     0x0001F402},
	{/*1400.000*/   560.000,        0x00000E01,     0x0001F402},
	{/*1412.500*/   565.000,        0x007D0E01,     0x0001F402},
	{/*1425.000*/   570.000,        0x00FA0E01,     0x0001F402},
	{/*1437.500*/   575.000,        0x01770E01,     0x0001F402},
	{/*1450.000*/   580.000,        0x01F40E01,     0x0001F402},
	{/*1462.500*/   585.000,        0x02710E01,     0x0001F402},
	{/*1475.000*/   590.000,        0x02EE0E01,     0x0001F402},
	{/*1487.500*/   595.000,        0x036B0E01,     0x0001F402},
	{/*1575.000*/   600.000,        0x02EE0F01,     0x00027102},
	{/*1512.500*/   605.000,        0x007D0F01,     0x0001F402},
	{/*1525.000*/   610.000,        0x00FA0F01,     0x0001F402},
	{/*1537.500*/   615.000,        0x01770F01,     0x0001F402},
	{/*1550.000*/   620.000,        0x01F40F01,     0x0001F402},
	{/*1562.500*/   625.000,        0x02710F01,     0x0001F402},
	{/*1575.000*/   630.000,        0x02EE0F01,     0x0001F402},
	{/*1587.500*/   635.000,        0x036B0F01,     0x0001F402},
	{/*1600.000*/   640.000,        0x00001001,     0x0001F402},
	{/*1290.000*/   645.000,        0x01F44005,     0x00000002},
	{/*1462.500*/   650.000,        0x02710E01,     0x0000FA02}
};

static int icap_parse_bitstream_axlf_section(struct platform_device *pdev,
	const struct axlf *xclbin, enum axlf_section_kind kind);
static void icap_set_data(struct icap *icap, struct xcl_pr_region *hwicap);
static uint64_t icap_get_data_nolock(struct platform_device *pdev, enum data_kind kind);
static uint64_t icap_get_data(struct platform_device *pdev, enum data_kind kind);
static const struct axlf_section_header *get_axlf_section_hdr(
	struct icap *icap, const struct axlf *top, enum axlf_section_kind kind);
static void icap_refresh_addrs(struct platform_device *pdev);
static inline int icap_calibrate_mig(struct platform_device *pdev);
static void icap_probe_urpdev(struct platform_device *pdev, struct axlf *xclbin,
	int *num_urpdev, struct xocl_subdev **urpdevs);

static int icap_xclbin_wr_lock(struct icap *icap)
{
	pid_t pid = pid_nr(task_tgid(current));
	int ret = 0;

	mutex_lock(&icap->icap_lock);
	if (icap->busy) {
		ret = -EBUSY;
	} else {
		icap->busy = (u64)pid;
	}
	mutex_unlock(&icap->icap_lock);

	if (ret)
		goto done;

	ret = wait_event_interruptible(icap->reader_wq, icap->reader_ref == 0);

	if (ret)
		goto done;

	BUG_ON(icap->reader_ref != 0);

done:
	ICAP_DBG(icap, "%d ret: %d", pid, ret);
	return ret;
}
static void icap_xclbin_wr_unlock(struct icap *icap)
{
	pid_t pid = pid_nr(task_tgid(current));

	BUG_ON(icap->busy != (u64)pid);

	mutex_lock(&icap->icap_lock);
	icap->busy = 0;
	mutex_unlock(&icap->icap_lock);
	ICAP_DBG(icap, "%d", pid);
}
static int icap_xclbin_rd_lock(struct icap *icap)
{
	pid_t pid = pid_nr(task_tgid(current));
	int ret = 0;

	mutex_lock(&icap->icap_lock);

	if (icap->busy) {
		ret = -EBUSY;
		goto done;
	}

	icap->reader_ref++;

done:
	mutex_unlock(&icap->icap_lock);
	ICAP_DBG(icap, "%d ret: %d", pid, ret);
	return ret;
}
static  void icap_xclbin_rd_unlock(struct icap *icap)
{
	pid_t pid = pid_nr(task_tgid(current));
	bool wake = false;

	mutex_lock(&icap->icap_lock);

	BUG_ON(icap->reader_ref == 0);

	ICAP_DBG(icap, "%d", pid);

	wake = (--icap->reader_ref == 0);

	mutex_unlock(&icap->icap_lock);
	if (wake)
		wake_up_interruptible(&icap->reader_wq);
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

static void icap_read_from_peer(struct platform_device *pdev)
{
	struct xcl_mailbox_subdev_peer subdev_peer = {0};
	struct icap *icap = platform_get_drvdata(pdev);
	struct xcl_pr_region xcl_hwicap = {0};
	size_t resp_len = sizeof(struct xcl_pr_region);
	size_t data_len = sizeof(struct xcl_mailbox_subdev_peer);
	struct xcl_mailbox_req *mb_req = NULL;
	size_t reqlen = sizeof(struct xcl_mailbox_req) + data_len;
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
		mb_req, reqlen, &xcl_hwicap, &resp_len, NULL, NULL, 0);

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

static bool icap_bitstream_in_use(struct icap *icap)
{
	BUG_ON(icap->icap_bitstream_ref < 0);
	return icap->icap_bitstream_ref != 0;
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
	if (!icap_bitstream_in_use(icap)) {
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

	if (ICAP_PRIVILEGED(icap)) {
		if (uuid_is_null(&icap->icap_bitstream_uuid))
			return freq;
		err = xocl_clock_get_freq_counter_khz(xdev, &freq, idx);
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
	int idx, unsigned short *freq_max, unsigned short *freq_min)
{
	struct clock_freq_topology *topology = 0;
	int num_clocks = 0;

	if (!uuid_is_null(&icap->icap_bitstream_uuid)) {
		topology = icap->xclbin_clock_freq_topology;
		if (!topology)
			return;

		num_clocks = topology->m_count;

		if (idx >= num_clocks)
			return;

		if (freq_max)
			*freq_max = topology->m_clock_freq[idx].m_freq_Mhz;

		if (freq_min)
			*freq_min = frequency_table[0].ocl;
	}
}

static int ulp_clock_update(struct icap *icap, unsigned short *freqs,
	int num_freqs, int verify)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	err = xocl_clock_update_freq(xdev, freqs, num_freqs, verify);

	ICAP_INFO(icap, "returns: %d", err);
	return err;
}

static int icap_xclbin_validate_clock_req_impl(struct platform_device *pdev,
	struct drm_xocl_reclock_info *freq_obj)
{
	struct icap *icap = platform_get_drvdata(pdev);
	unsigned short freq_max, freq_min;
	int i;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	if (uuid_is_null(&icap->icap_bitstream_uuid)) {
		ICAP_ERR(icap, "ERROR: There isn't a hardware accelerator loaded in the dynamic region."
			" Validation of accelerator frequencies cannot be determine");
		return -EDOM;
	}

	for (i = 0; i < ARRAY_SIZE(freq_obj->ocl_target_freq); i++) {
		if (!freq_obj->ocl_target_freq[i])
			continue;
		freq_max = freq_min = 0;
		xclbin_get_ocl_frequency_max_min(icap, i, &freq_max, &freq_min);
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
	int err;

	mutex_lock(&icap->icap_lock);
	err = icap_xclbin_validate_clock_req_impl(pdev, freq_obj);
	mutex_unlock(&icap->icap_lock);

	return err;
}

static int icap_ocl_update_clock_freq_topology(struct platform_device *pdev,
	struct xclmgmt_ioc_freqscaling *freq_obj)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0;

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return err;

	mutex_lock(&icap->icap_lock);

	err = icap_xclbin_validate_clock_req_impl(pdev,
	    (struct drm_xocl_reclock_info *)freq_obj);
	if (err)
		goto done;

	err = ulp_clock_update(icap, freq_obj->ocl_target_freq,
	    ARRAY_SIZE(freq_obj->ocl_target_freq), 1);
	if (err)
		goto done;

	err = icap_calibrate_mig(pdev);
done:
	mutex_unlock(&icap->icap_lock);
	icap_xclbin_rd_unlock(icap);
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
static int calibrate_mig(struct icap *icap)
{
	int i;

	for (i = 0; i < 20 && !mig_calibration_done(icap); ++i)
		msleep(500);

	if (!mig_calibration_done(icap)) {
		ICAP_ERR(icap,
			"MIG calibration timeout after bitstream download");
		return -ETIMEDOUT;
	}

	ICAP_INFO(icap, "took %ds", i/2);
	return 0;
}

static inline void xclbin_free_clock_freq_topology(struct icap *icap)
{
	vfree(icap->xclbin_clock_freq_topology);
	icap->xclbin_clock_freq_topology = NULL;
	icap->xclbin_clock_freq_topology_length = 0;
}

static void xclbin_write_clock_freq(struct clock_freq *dst, struct clock_freq *src)
{
	dst->m_freq_Mhz = src->m_freq_Mhz;
	dst->m_type = src->m_type;
	memcpy(&dst->m_name, &src->m_name, sizeof(src->m_name));
}


static int xclbin_setup_clock_freq_topology(struct icap *icap,
	const struct axlf *xclbin)
{
	int i;
	struct clock_freq_topology *topology;
	struct clock_freq *clk_freq = NULL;
	const struct axlf_section_header *hdr =
		get_axlf_section_hdr(icap, xclbin, CLOCK_FREQ_TOPOLOGY);

	/* Can't find CLOCK_FREQ_TOPOLOGY, just return*/
	if (!hdr)
		return 0;

	xclbin_free_clock_freq_topology(icap);

	icap->xclbin_clock_freq_topology = vzalloc(hdr->m_sectionSize);
	if (!icap->xclbin_clock_freq_topology)
		return -ENOMEM;

	topology = (struct clock_freq_topology *)(((char *)xclbin) + hdr->m_sectionOffset);

	/*
	 *  icap->xclbin_clock_freq_topology->m_clock_freq
	 *  must follow the order
	 *
	 *	0: DATA_CLK
	 *	1: KERNEL_CLK
	 *	2: SYSTEM_CLK
	 *
	 */
	icap->xclbin_clock_freq_topology->m_count = topology->m_count;
	for (i = 0; i < topology->m_count; ++i) {
		if (topology->m_clock_freq[i].m_type == CT_SYSTEM)
			clk_freq = &icap->xclbin_clock_freq_topology->m_clock_freq[SYSTEM_CLK];
		else if (topology->m_clock_freq[i].m_type == CT_DATA)
			clk_freq = &icap->xclbin_clock_freq_topology->m_clock_freq[DATA_CLK];
		else if (topology->m_clock_freq[i].m_type == CT_KERNEL)
			clk_freq = &icap->xclbin_clock_freq_topology->m_clock_freq[KERNEL_CLK];
		else
			break;

		xclbin_write_clock_freq(clk_freq, &topology->m_clock_freq[i]);
	}

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
	case CLOCK_FREQ_TOPOLOGY:
		size = sizeof_sect(icap->xclbin_clock_freq_topology, m_clock_freq);
		break;
	case PARTITION_METADATA:
		size = fdt_totalsize(icap->partition_metadata);
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

	for (i = 0; i < top->m_header.m_numSections; i++) {
		if (top->m_sections[i].m_sectionKind == kind) {
			hdr = &top->m_sections[i];
			break;
		}
	}

	if (hdr) {
		if ((hdr->m_sectionOffset + hdr->m_sectionSize) >
			top->m_header.m_length) {
			ICAP_ERR(icap, "found section %d is invalid", kind);
			hdr = NULL;
		} else {
			ICAP_INFO(icap, "section %d offset: %llu, size: %llu",
				kind, hdr->m_sectionOffset, hdr->m_sectionSize);
		}
	} else {
		ICAP_WARN(icap, "could not find section header %d", kind);
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

	primaryHeader = get_axlf_section_hdr(icap, axlf, BITSTREAM);

	if (primaryHeader) {
		primaryFirmwareOffset = primaryHeader->m_sectionOffset;
		primaryFirmwareLength = primaryHeader->m_sectionSize;
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
	const struct firmware *sche_fw;
	int err = 0;
	uint64_t mbBinaryOffset = 0;
	uint64_t mbBinaryLength = 0;
	const struct axlf_section_header *mbHeader = 0;
	bool load_sched = false, load_mgmt = false;
	char *fw_buf = NULL;
	size_t fw_size = 0;

	/* Can only be done from mgmt pf. */
	if (!ICAP_PRIVILEGED(icap))
		return -EPERM;

	err = xocl_rom_load_firmware(xdev, &fw_buf, &fw_size);
	if (err)
		return err;

	bin_obj_axlf = (struct axlf *)fw_buf;

	if (xocl_mb_sched_on(xdev)) {
		/* Try locating the microblaze binary. */
		if (XDEV(xdev)->priv.sched_bin) {
			err = request_firmware(&sche_fw,
				XDEV(xdev)->priv.sched_bin, &pcidev->dev);
			if (!err)  {
				xocl_mb_load_sche_image(xdev, sche_fw->data,
					sche_fw->size);
				ICAP_INFO(icap, "stashed shared mb sche bin, len %ld", sche_fw->size);
				load_sched = true;
				release_firmware(sche_fw);
			}
		}
		if (!load_sched) {
			mbHeader = get_axlf_section_hdr(icap, bin_obj_axlf,
					SCHED_FIRMWARE);
			if (mbHeader) {
				mbBinaryOffset = mbHeader->m_sectionOffset;
				mbBinaryLength = mbHeader->m_sectionSize;
				xocl_mb_load_sche_image(xdev,
					fw_buf + mbBinaryOffset,
					mbBinaryLength);
				ICAP_INFO(icap,
					"stashed mb sche binary, len %lld",
					mbBinaryLength);
				load_sched = true;
				err = 0;
			}
		}
	}

	if (xocl_mb_mgmt_on(xdev)) {
		/* Try locating the board mgmt binary. */
		mbHeader = get_axlf_section_hdr(icap, bin_obj_axlf, FIRMWARE);
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
	mbHeader = get_axlf_section_hdr(icap, bin_obj_axlf, BMC);
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
				sizeof(struct xcl_mailbox_req));
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

static long axlf_set_freqscaling(struct icap *icap)
{
	struct clock_freq_topology *freqs = NULL;
	int clock_type_count = 0;
	int i = 0;
	struct clock_freq *freq = NULL;
	int data_clk_count = 0;
	int kernel_clk_count = 0;
	int system_clk_count = 0;
	unsigned short target_freqs[4] = {0};

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	if (!icap->xclbin_clock_freq_topology)
		return 0;

	freqs = icap->xclbin_clock_freq_topology;
	if (freqs->m_count > 4) {
		ICAP_ERR(icap, "More than 4 clocks found in clock topology");
		return -EDOM;
	}

	/* Error checks - we support 1 data clk (reqd), 1 kernel clock(reqd) and
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

	ICAP_INFO(icap, "set %lu freq, data: %d, kernel: %d, sys: %d, sys1: %d",
		ARRAY_SIZE(target_freqs), target_freqs[0], target_freqs[1],
		target_freqs[2], target_freqs[3]);
	//return set_freqs(icap, target_freqs, ARRAY_SIZE(target_freqs));
	return ulp_clock_update(icap, target_freqs, ARRAY_SIZE(target_freqs), 0);
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
		err = xocl_clock_freqscaling(xocl_get_xdev(icap->icap_pdev), true);
		err = (err == -ENODEV) ? 0 : err;
	}

	icap_free_axi_gate(icap);
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
	case CLOCK_FREQ_TOPOLOGY:
		target = (void **)&icap->xclbin_clock_freq_topology;
		break;
	case PARTITION_METADATA:
		target = (void **)&icap->partition_metadata;
		break;
	default:
		break;
	}
	if (target && *target) {
		vfree(*target);
		*target = NULL;
	}
}

static void icap_clean_bitstream_axlf(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);

	uuid_copy(&icap->icap_bitstream_uuid, &uuid_null);
	icap_clean_axlf_section(icap, IP_LAYOUT);
	icap_clean_axlf_section(icap, MEM_TOPOLOGY);
	icap_clean_axlf_section(icap, DEBUG_IP_LAYOUT);
	icap_clean_axlf_section(icap, CONNECTIVITY);
	icap_clean_axlf_section(icap, CLOCK_FREQ_TOPOLOGY);
	icap_clean_axlf_section(icap, PARTITION_METADATA);
}

static uint32_t convert_mem_type(const char *name)
{
	/* Don't trust m_type in xclbin, convert name to m_type instead.
	 * m_tag[i] = "HBM[0]" -> m_type = MEM_HBM
	 * m_tag[i] = "DDR[1]" -> m_type = MEM_DRAM
	 *
	 * Use MEM_DDR3 as a invalid memory type. */
	enum MEM_TYPE mem_type = MEM_DDR3;

	if (!strncasecmp(name, "DDR", 3))
		mem_type = MEM_DRAM;
	else if (!strncasecmp(name, "HBM", 3))
		mem_type = MEM_HBM;
	else if (!strncasecmp(name, "bank", 4))
		mem_type = MEM_DRAM;

	return mem_type;
}

static uint16_t icap_get_memidx(struct mem_topology *mem_topo, enum IP_TYPE ecc_type,
	int idx)
{
	uint16_t memidx = INVALID_MEM_IDX, i, mem_idx = 0;
	enum MEM_TYPE m_type, target_m_type;

	/*
	 * Get global memory index by feeding desired memory type and index
	 */
	if (ecc_type == IP_MEM_DDR4)
		target_m_type = MEM_DRAM;
	else if (ecc_type == IP_DDR4_CONTROLLER)
		target_m_type = MEM_DRAM;
	else if (ecc_type == IP_MEM_HBM)
		target_m_type = MEM_HBM;
	else
		goto done;

	if (!mem_topo)
		goto done;

	for (i = 0; i < mem_topo->m_count; ++i) {
		m_type = convert_mem_type(mem_topo->m_mem_data[i].m_tag);
		if (m_type == target_m_type) {
			if (idx == mem_idx)
				return i;
			mem_idx++;
		}
	}

done:
	return memidx;
}

static int icap_create_subdev_debugip(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0, i = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct debug_ip_layout *debug_ip_layout = icap->debug_layout;

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
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create AXI_MONITOR_FIFO_LITE subdev");
				break;
			}
		} else if (ip->m_type == AXI_MONITOR_FIFO_FULL) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_TRACE_FIFO_FULL;
			subdev_info.priv_data = ip;
			subdev_info.data_len = sizeof(struct debug_ip_data);
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
			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create SPC subdev");
				break;
			}
		}
	}
	return err;
}

static int icap_create_cu(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct ip_layout *ip_layout = icap->ip_layout;
	struct xrt_cu_info info;
	int err = 0, i;

	/* Let CU controller know the dynamic resources */
	for (i = 0; i < ip_layout->m_count; ++i) {
		struct xocl_subdev_info subdev_info = XOCL_DEVINFO_CU;
		struct ip_data *ip = &ip_layout->m_ip_data[i];

		if (ip->m_type != IP_KERNEL)
			continue;

		if (ip->m_base_address == 0xFFFFFFFF)
			continue;

		/* NOTE: Only support 64 instences in subdev framework */

		/* TODO: use HLS CU as default.
		 * don't know how to distinguish plram CU and normal CU
		 */
		info.model = XCU_HLS;
		info.num_res = subdev_info.num_res;

		/* TODO: Consider where should we determine CU index in
		 * the driver.. Right now, user space determine it and let
		 * driver known by configure command.
		 */
		info.cu_idx = -1;
		info.inst_idx = i;
		info.addr = ip->m_base_address;
		info.intr_enable = ip->properties & IP_INT_ENABLE_MASK;
		info.protocol = (ip->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT;
		info.intr_id = (ip->properties & IP_INTERRUPT_ID_MASK) >> IP_INTERRUPT_ID_SHIFT;

		subdev_info.res[0].start += ip->m_base_address;
		subdev_info.res[0].end += ip->m_base_address;
		subdev_info.priv_data = &info;
		subdev_info.data_len = sizeof(info);
		subdev_info.override_idx = info.inst_idx;
		err = xocl_subdev_create(xdev, &subdev_info);
		if (err) {
			//ICAP_ERR(icap, "can't create CU subdev");
			break;
		}
	}

	return err;
}

static int icap_create_subdev(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0, i = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct ip_layout *ip_layout = icap->ip_layout;
	struct mem_topology *mem_topo = icap->mem_topo;

	if (!ip_layout) {
		err = -ENODEV;
		goto done;
	}

	if (!mem_topo) {
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

			if (!ICAP_PRIVILEGED(icap))
				subdev_info.num_res = 0;

			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create MIG subdev");
				goto done;
			}

		} else if (ip->m_type == IP_MEM_HBM) {
			struct xocl_subdev_info subdev_info = XOCL_DEVINFO_MIG_HBM;
			uint16_t memidx = icap_get_memidx(mem_topo, IP_MEM_HBM, ip->indices.m_index);

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

			if (!ICAP_PRIVILEGED(icap))
				subdev_info.num_res = 0;

			err = xocl_subdev_create(xdev, &subdev_info);
			if (err) {
				ICAP_ERR(icap, "can't create DNA subdev");
				goto done;
			}
		}
	}

	if (!ICAP_PRIVILEGED(icap))
		err = icap_create_cu(pdev);

	if (!ICAP_PRIVILEGED(icap))
		err = icap_create_subdev_debugip(pdev);
done:
	return err;
}

static int icap_create_post_download_subdevs(struct platform_device *pdev, struct axlf *xclbin)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0, i = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct ip_layout *ip_layout = icap->ip_layout;
	struct mem_topology *mem_topo = icap->mem_topo;
	uint32_t memidx = 0;

	BUG_ON(!ICAP_PRIVILEGED(icap));

	if (!ip_layout) {
		err = -ENODEV;
		goto done;
	}

	if (!mem_topo) {
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

static int icap_verify_bitstream_axlf(struct platform_device *pdev,
	struct axlf *xclbin)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	int err = 0;
	uint64_t section_size = 0;
	u32 capability;

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

	err = icap_create_subdev(pdev);
	if (err)
		goto done;


	/* Skip dna validation in userpf*/
	if (!ICAP_PRIVILEGED(icap))
		goto done;

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

		if (alloc_and_get_axlf_section(icap, xclbin,
			DNA_CERTIFICATE,
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

static int __icap_peer_xclbin_download(struct icap *icap, struct axlf *xclbin)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	uint64_t ch_state = 0;
	uint32_t data_len = 0;
	struct xcl_mailbox_req *mb_req = NULL;
	int msgerr = -ETIMEDOUT;
	size_t resplen = sizeof(msgerr);
	xuid_t *peer_uuid = NULL;
	struct xcl_mailbox_bitstream_kaddr mb_addr = {0};
	struct mem_topology *mem_topo = icap->mem_topo;
	int i, mig_count = 0;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	/* Optimization for transferring entire xclbin thru mailbox. */
	peer_uuid = (xuid_t *)icap_get_data_nolock(icap->icap_pdev, PEER_UUID);
	if (uuid_equal(peer_uuid, &xclbin->m_header.uuid)) {
		ICAP_INFO(icap, "xclbin already on peer, skip downloading");
		return 0;
	}

	xocl_mailbox_get(xdev, CHAN_STATE, &ch_state);
	if ((ch_state & XCL_MB_PEER_SAME_DOMAIN) != 0) {
		data_len = sizeof(struct xcl_mailbox_req) +
			sizeof(struct xcl_mailbox_bitstream_kaddr);
		mb_req = vmalloc(data_len);
		if (!mb_req) {
			ICAP_ERR(icap, "can't create mb_req\n");
			return -ENOMEM;
		}
		mb_req->req = XCL_MAILBOX_REQ_LOAD_XCLBIN_KADDR;
		mb_addr.addr = (uint64_t)xclbin;
		memcpy(mb_req->data, &mb_addr,
			sizeof(struct xcl_mailbox_bitstream_kaddr));
	} else {
		data_len = sizeof(struct xcl_mailbox_req) +
			xclbin->m_header.m_length;
		mb_req = vmalloc(data_len);
		if (!mb_req) {
			ICAP_ERR(icap, "can't create mb_req\n");
			return -ENOMEM;
		}
		mb_req->req = XCL_MAILBOX_REQ_LOAD_XCLBIN;
		memcpy(mb_req->data, xclbin, xclbin->m_header.m_length);
	}

	if (mem_topo) {
		for (i = 0; i < mem_topo->m_count; i++) {
			if (XOCL_IS_STREAM(mem_topo, i))
				continue;

			if (XOCL_IS_DDR_USED(mem_topo, i))
				mig_count++;
		}
	}

	/* Set timeout to be 1s per 2MB for downloading xclbin.
	 * plus toggling axigate time 5s
	 * plus #MIG * 0.5s
	 * In Azure cloud, there is special requirement for xclbin download
	 * that the minumum timeout should be 50s.
	 */
	(void) xocl_peer_request(xdev, mb_req, data_len,
		&msgerr, &resplen, NULL, NULL,
		max(((size_t)xclbin->m_header.m_length) / (2048 * 1024) +
			5 + mig_count / 2, 50UL));
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
#define	SYS_KEYS	((void *)1UL)
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

static int icap_refresh_clock_freq(struct icap *icap, struct axlf *xclbin)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	if (ICAP_PRIVILEGED(icap) && !XOCL_DSA_IS_SMARTN(xdev)) {
		err = xclbin_setup_clock_freq_topology(icap, xclbin);
		if (!err) {
			err = axlf_set_freqscaling(icap);
			err = err == -ENODEV ? 0 : err;
		}
	}

	ICAP_INFO(icap, "ret: %d", err);
	return err;
}

static void icap_save_calib(struct icap *icap)
{
	struct mem_topology *mem_topo = icap->mem_topo;
	int err = 0, i = 0, ddr_idx = 0;
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);

	if (!mem_topo)
		return;

	if (!ICAP_PRIVILEGED(icap))
		return;

	for (; i < mem_topo->m_count; ++i) {
		if (!mem_topo->m_mem_data[i].m_used)
			continue;
		if (convert_mem_type(mem_topo->m_mem_data[i].m_tag) != MEM_DRAM)
			continue;

		err = xocl_srsr_save_calib(xdev, ddr_idx);
		if (err)
			ICAP_DBG(icap, "Not able to save mem %d calibration data.", i);

		ddr_idx++;
	}
	err = xocl_calib_storage_save(xdev);
}

static void icap_calib(struct icap *icap, bool retain)
{
	int err = 0, i = 0, ddr_idx = 0;
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	struct mem_topology *mem_topo = icap->mem_topo;

	BUG_ON(!mem_topo);

	err = xocl_calib_storage_restore(xdev);

	for (; i < mem_topo->m_count; ++i) {
		if (!mem_topo->m_mem_data[i].m_used)
			continue;
		if (convert_mem_type(mem_topo->m_mem_data[i].m_tag) != MEM_DRAM)
			continue;

		err = xocl_srsr_calib(xdev, ddr_idx, retain);
		if (err)
			ICAP_DBG(icap, "Not able to calibrate mem %d.", i);

		ddr_idx++;
	}

}

static int icap_reset_ddr_gate_pin(struct icap *icap)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	err = xocl_iores_write32(xdev, XOCL_SUBDEV_LEVEL_PRP,
		IORES_DDR4_RESET_GATE, 0, 1);

	ICAP_INFO(icap, "%s ret %d", __func__, err);
	return err;
}

static int icap_release_ddr_gate_pin(struct icap *icap)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	err = xocl_iores_write32(xdev, XOCL_SUBDEV_LEVEL_PRP,
		IORES_DDR4_RESET_GATE, 0, 0);

	ICAP_INFO(icap, "%s ret %d", __func__, err);
	return err;
}

static int icap_calibrate_mig(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int err = 0;

	/* Wait for mig recalibration */
	if ((xocl_is_unified(xdev) || XOCL_DSA_XPR_ON(xdev)))
		err = calibrate_mig(icap);

	return err;
}

static int __icap_xclbin_download(struct icap *icap, struct axlf *xclbin)
{
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);
	int i = 0, err = 0, num_dev = 0;
	bool retention = (icap->data_retention & 0x1) == 0x1;
	struct xocl_subdev *subdevs = NULL;
	bool has_ulp_clock = false;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));
	icap_probe_urpdev(icap->icap_pdev, xclbin, &num_dev, &subdevs);

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

	err = icap_refresh_clock_freq(icap, xclbin);
	if (err)
		goto out;

	if (retention) {
		err = icap_reset_ddr_gate_pin(icap);
		if (err == -ENODEV)
			ICAP_INFO(icap, "No ddr gate pin");
		else if (err) {
			ICAP_ERR(icap, "not able to reset ddr gate pin");
			goto out;
		}
	}
	/* xclbin generated for the flat shell contains MCS files which includes the accelerator
	 * these MCS files should have been already flashed into the device using xbmgmt tool
	 * we dont need to reprogram the xclbin for the FLAT shells.
	 * TODO Currently , There is no way to check whether the programmed xclbin matches with this xclbin or not
	 */
	if (xclbin->m_header.m_mode != XCLBIN_FLAT) {
		err = icap_download_bitstream(icap, xclbin);
		if (err)
			goto out;
	} else {
		uuid_copy(&icap->icap_bitstream_uuid, &xclbin->m_header.uuid);
		ICAP_INFO(icap, "xclbin is generated for flat shell, dont need to program the bitstream ");
	}

	/* calibrate hbm and ddr should be performed when resources are ready */
	err = icap_create_post_download_subdevs(icap->icap_pdev, xclbin);
	if (err)
		goto out;

	/* For 2RP, the majority of ULP IP can only be touched after ucs control bit set to 0x1
	 * which is done in icap_refresh_clock_freq. Move so logics(create clock devices and set ucs control bit)
	 * to xclbin download function as workaround to solve interleaving issue.
	 * DDR SRSR IP and MIG need to wait until ucs control bit set to 0x1, 
	 * and icap mig calibration needs to wait until DDR SRSR calibration finish
	 */
	if (num_dev > 0) {
		/* if has clock, create clock subdev first */
		for (i = 0; i < num_dev; i++) {
			if (subdevs[i].info.id != XOCL_SUBDEV_CLOCK)
				continue;
			err = xocl_subdev_create(xdev, &subdevs[i].info);
			if (err)
				goto out;

			has_ulp_clock = true;
			break;
		}

		icap_refresh_addrs(icap->icap_pdev);
		err = icap_refresh_clock_freq(icap, xclbin);
		if (err)
			goto out;
	}

	icap_calib(icap, retention);

	if (retention) {
		err = icap_release_ddr_gate_pin(icap);
		if (err == -ENODEV)
			ICAP_INFO(icap, "No ddr gate pin");
		else if (err)
			ICAP_ERR(icap, "not able to release ddr gate pin");
	}

	err = icap_calibrate_mig(icap->icap_pdev);
	if (err)
		goto out;
out:
	if (err && retention)
		icap_release_ddr_gate_pin(icap);
	if (subdevs)
		vfree(subdevs);
	ICAP_INFO(icap, "ret: %d", (int)err);
	return err;
}

static void icap_probe_urpdev(struct platform_device *pdev, struct axlf *xclbin,
	int *num_urpdev, struct xocl_subdev **urpdevs)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(icap->icap_pdev);

	icap_parse_bitstream_axlf_section(pdev, xclbin, PARTITION_METADATA);
	if (icap->partition_metadata) {
		*num_urpdev = xocl_fdt_parse_blob(xdev, icap->partition_metadata,
			icap_get_section_size(icap, PARTITION_METADATA),
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

static void check_mem_topo_and_data_retention(struct icap *icap,
	struct axlf *xclbin)
{
	struct mem_topology *mem_topo = icap->mem_topo;
	const struct axlf_section_header *hdr = get_axlf_section_hdr(icap, xclbin, MEM_TOPOLOGY);
	uint64_t size = 0, offset = 0;

	if (!hdr || !mem_topo || !icap->data_retention)
		return;

	size = hdr->m_sectionSize;
	offset = hdr->m_sectionOffset;

	/* Data retention feature ONLY works if the xclbins have identical mem_topology 
	 * or it will lead to hardware failure.
	 * If the incoming xclbin has different mem_topology, disable data retention feature
	 */

	if ((size != sizeof_sect(mem_topo, m_mem_data)) ||
		    memcmp(((char *)xclbin)+offset, mem_topo, size)) {
		ICAP_WARN(icap, "Incoming mem_topology doesn't match, disable data retention");
	}

	return;
}

static void icap_get_max_host_mem_aperture(struct icap *icap)
{
	int i = 0;
	struct mem_topology *mem_topo = icap->mem_topo;

	icap->max_host_mem_aperture = 0;

	if (!mem_topo)
		return;

	for ( i=0; i< mem_topo->m_count; ++i) {
		if (!mem_topo->m_mem_data[i].m_used)
			continue;
		if (IS_HOST_MEM(mem_topo->m_mem_data[i].m_tag))
			icap->max_host_mem_aperture = mem_topo->m_mem_data[i].m_size << 10;
	}

	return;
}

static int __icap_download_bitstream_axlf(struct platform_device *pdev,
	struct axlf *xclbin)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0, i = 0, num_dev = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_subdev *subdevs = NULL;

	BUG_ON(!mutex_is_locked(&icap->icap_lock));

	ICAP_INFO(icap, "incoming xclbin: %pUb\non device xclbin: %pUb",
		&xclbin->m_header.uuid, &icap->icap_bitstream_uuid);

	/* NOTE: xmc freeze -> xclbin download -> xmc free */
	err = icap_xmc_freeze(icap);
	if (err)
		return err;

	icap_save_calib(icap);

	xocl_subdev_destroy_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);
	icap_refresh_addrs(pdev);

	icap_probe_urpdev(pdev, xclbin, &num_dev, &subdevs);

	if (ICAP_PRIVILEGED(icap)) {
		if (XOCL_DSA_IS_VERSAL(xdev))
			return 0;

		/* Check the incoming mem topoloy with the current one before overwrite */
		check_mem_topo_and_data_retention(icap, xclbin);

		icap_parse_bitstream_axlf_section(pdev, xclbin, MEM_TOPOLOGY);
		icap_parse_bitstream_axlf_section(pdev, xclbin, IP_LAYOUT);

		err = __icap_xclbin_download(icap, xclbin);
		if (err)
			goto done;

		/* reconfig mig and dna after calibrate_mig */
		err = icap_verify_bitstream_axlf(pdev, xclbin);
		if (err)
			goto done;
	} else {
		/* has to create mem topology even with failure case
		 * please refer the comment in xocl_ioctl.c
		 * without creating mem topo, memory corruption could happen
		 */
		icap_parse_bitstream_axlf_section(pdev, xclbin, MEM_TOPOLOGY);

		err = __icap_peer_xclbin_download(icap, xclbin);

		/* TODO: Remove this after new KDS replace the legacy one */
		/*
		 * xclbin download changes PR region, make sure next
		 * ERT configure cmd will go through
		 */
		if (!kds_mode)
			(void) xocl_exec_reconfig(xdev);
		if (err)
			goto done;

		icap_parse_bitstream_axlf_section(pdev, xclbin, IP_LAYOUT);
		icap_parse_bitstream_axlf_section(pdev, xclbin, CONNECTIVITY);
		icap_parse_bitstream_axlf_section(pdev, xclbin,
			DEBUG_IP_LAYOUT);
		xclbin_setup_clock_freq_topology(icap, xclbin);

		if (!XOCL_DSA_IS_VERSAL(xdev)) {
			/*
			 * not really doing verification, but
			 * just create subdevs
			 */
			(void) icap_verify_bitstream_axlf(pdev, xclbin);
		}

		icap_get_max_host_mem_aperture(icap);

	}
	/* create the rest of subdevs for both mgmt and user pf */
	if (num_dev > 0) {
		for (i = 0; i < num_dev; i++)
			(void) xocl_subdev_create(xdev, &subdevs[i].info);

		xocl_subdev_create_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);
	}

	/* Only when everything has been successfully setup, then enable xmc */
	if (!err)
		err = icap_xmc_free(icap);

done:
	if (err) {
		uuid_copy(&icap->icap_bitstream_uuid, &uuid_null);
	} else {
		/* Remember "this" bitstream, so avoid redownload next time. */
		uuid_copy(&icap->icap_bitstream_uuid, &xclbin->m_header.uuid);
	}
	return err;
}

static int icap_download_bitstream_axlf(struct platform_device *pdev,
	const void *u_xclbin)
{
	struct icap *icap = platform_get_drvdata(pdev);
	struct axlf *xclbin = (struct axlf *)u_xclbin;
	int err = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	const struct axlf_section_header *header = NULL;

	err = icap_xclbin_wr_lock(icap);
	if (err)
		return err;

	mutex_lock(&icap->icap_lock);

	/* Sanity check xclbin. */
	if (memcmp(xclbin->m_magic, ICAP_XCLBIN_V2, sizeof(ICAP_XCLBIN_V2))) {
		ICAP_ERR(icap, "invalid xclbin magic string");
		err = -EINVAL;
		goto done;
	}

	header = get_axlf_section_hdr(icap, xclbin, PARTITION_METADATA);
	if (header) {
		ICAP_INFO(icap, "check interface uuid");
		if (!XDEV(xdev)->fdt_blob) {
			ICAP_ERR(icap, "did not find platform dtb");
			err = -EINVAL;
			goto done;
		}
		err = xocl_fdt_check_uuids(xdev,
				(const void *)XDEV(xdev)->fdt_blob,
				(const void *)((char *)xclbin +
				header->m_sectionOffset));
		if (err) {
			ICAP_ERR(icap, "interface uuids do not match");
			err = -EINVAL;
			goto done;
		}
	}

	/*
	 * If the previous frequency was very high and we load an incompatible
	 * bitstream it may damage the hardware!
	 * If no clock freq, must return without touching the hardware.
	 */
	header = get_axlf_section_hdr(icap, xclbin, CLOCK_FREQ_TOPOLOGY);
	if (!header) {
		err = -EINVAL;
		goto done;
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
	if (icap_bitstream_in_use(icap)) {
		ICAP_ERR(icap, "bitstream is in-use, can't change");
		err = -EBUSY;
		goto done;
	}

	err = __icap_download_bitstream_axlf(pdev, xclbin);

done:
	mutex_unlock(&icap->icap_lock);
	icap_xclbin_wr_unlock(icap);
	ICAP_INFO(icap, "%s err: %d", __func__, err);
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

	if (icap_bitstream_in_use(icap)) {
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

static int icap_lock_bitstream(struct platform_device *pdev, const xuid_t *id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int ref = 0, err = 0;

	BUG_ON(uuid_is_null(id));

	err = icap_xclbin_rd_lock(icap);
	if (err) {
		ICAP_ERR(icap, "Failed to get on device uuid, device busy");
		return err;
	}

	mutex_lock(&icap->icap_lock);

	if (!uuid_equal(id, &icap->icap_bitstream_uuid)) {
		ICAP_ERR(icap, "lock bitstream %pUb failed, on device: %pUb",
			id, &icap->icap_bitstream_uuid);
		err = -EBUSY;
		goto done;
	}

	ref = icap->icap_bitstream_ref;
	icap->icap_bitstream_ref++;
	ICAP_INFO(icap, "bitstream %pUb locked, ref=%d", id,
		icap->icap_bitstream_ref);

	/* TODO: Remove this after new KDS replace the legacy one */
	if (!kds_mode && ref == 0) {
		/* reset on first reference */
		xocl_exec_reset(xocl_get_xdev(pdev), id);
	}

done:
	mutex_unlock(&icap->icap_lock);
	icap_xclbin_rd_unlock(icap);
	return 0;
}

static int icap_unlock_bitstream(struct platform_device *pdev, const xuid_t *id)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0;
	xuid_t on_device_uuid;

	if (id == NULL)
		id = &uuid_null;

	err = icap_xclbin_rd_lock(icap);
	if (err) {
		ICAP_ERR(icap, "Failed to get on device uuid, device busy");
		return err;
	}

	mutex_lock(&icap->icap_lock);

	uuid_copy(&on_device_uuid, &icap->icap_bitstream_uuid);

	if (uuid_is_null(id)) /* force unlock all */
		icap->icap_bitstream_ref = 0;
	else if (uuid_equal(id, &on_device_uuid))
		icap->icap_bitstream_ref--;
	else
		err = -EINVAL;

	if (err == 0) {
		ICAP_INFO(icap, "bitstream %pUb unlocked, ref=%d",
			&on_device_uuid, icap->icap_bitstream_ref);
	} else {
		ICAP_ERR(icap, "unlock bitstream %pUb failed, on device: %pUb",
			id, &on_device_uuid);
		goto done;
	}

	/* TODO: Remove this after new KDS replace the legacy one */
	if (!kds_mode && icap->icap_bitstream_ref == 0 && !ICAP_PRIVILEGED(icap))
		(void) xocl_exec_stop(xocl_get_xdev(pdev));

done:
	mutex_unlock(&icap->icap_lock);
	icap_xclbin_rd_unlock(icap);
	return 0;
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
	case CLOCK_FREQ_TOPOLOGY:
		target = (void **)&icap->xclbin_clock_freq_topology;
		break;
	case PARTITION_METADATA:
		target = (void **)&icap->partition_metadata;
		break;
	default:
		return -EINVAL;
	}
	if (target && *target) {
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
		if (target && *target) {
			vfree(*target);
			*target = NULL;
		}
	}
	ICAP_INFO(icap, "%s kind %d, err: %ld", __func__, kind, err);
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

static void icap_put_xclbin_metadata(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);

	icap_xclbin_rd_unlock(icap);
}

static int icap_get_xclbin_metadata(struct platform_device *pdev,
	enum data_kind kind, void **buf)
{
	struct icap *icap = platform_get_drvdata(pdev);
	int err = 0;

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return err;

	mutex_lock(&icap->icap_lock);

	switch (kind) {
	case IPLAYOUT_AXLF:
		*buf = icap->ip_layout;
		break;
	case MEMTOPO_AXLF:
		*buf = icap->mem_topo;
		break;
	case DEBUG_IPLAYOUT_AXLF:
		*buf = icap->debug_layout;
		break;
	case CONNECTIVITY_AXLF:
		*buf = icap->connectivity;
		break;
	case XCLBIN_UUID:
		*buf = &icap->icap_bitstream_uuid;
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
}

static int icap_offline(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);

	xocl_drvinst_kill_proc(platform_get_drvdata(pdev));

	sysfs_remove_group(&pdev->dev.kobj, &icap_attr_group);
	xclbin_free_clock_freq_topology(icap);

	icap_clean_bitstream_axlf(pdev);

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
	u32 freq_counter, freq, request_in_khz, tolerance;

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return cnt;

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
	icap_xclbin_rd_unlock(icap);
	return cnt;
}
static DEVICE_ATTR_RO(clock_freqs);

static ssize_t clock_freqs_max_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;
	int i, err;
	unsigned short freq;

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return cnt;

	for (i = 0; i < ICAP_MAX_NUM_CLOCKS; i++) {
		freq = 0;
		xclbin_get_ocl_frequency_max_min(icap, i, &freq, NULL);
		cnt += sprintf(buf + cnt, "%d\n", freq);
	}

	icap_xclbin_rd_unlock(icap);
	return cnt;
}
static DEVICE_ATTR_RO(clock_freqs_max);

static ssize_t clock_freqs_min_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;
	int i, err;
	unsigned short freq;

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return cnt;

	for (i = 0; i < ICAP_MAX_NUM_CLOCKS; i++) {
		freq = 0;
		xclbin_get_ocl_frequency_max_min(icap, i, NULL, &freq);
		cnt += sprintf(buf + cnt, "%d\n", freq);
	}

	icap_xclbin_rd_unlock(icap);
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
	const struct firmware *sig = NULL;
	const struct firmware *text = NULL;
	int err = 0;

	err = request_firmware(&sig, "xilinx/signature", &pcidev->dev);
	if (err) {
		ICAP_ERR(icap, "can't load signature: %d", err);
		goto done;
	}
	err = request_firmware(&text, "xilinx/text", &pcidev->dev);
	if (err) {
		ICAP_ERR(icap, "can't load text: %d", err);
		goto done;
	}

	err = icap_verify_signature(icap, text->data, text->size,
		sig->data, sig->size);
	if (err) {
		ICAP_ERR(icap, "Failed to verify data file");
		goto done;
	}

	ICAP_INFO(icap, "Successfully verified data file!!!");

done:
	if (sig)
		release_firmware(sig);
	if (text)
		release_firmware(text);
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
	u64 val = 0;

	mutex_lock(&icap->icap_lock);

	val = icap->reader_ref;

	mutex_unlock(&icap->icap_lock);

	return sprintf(buf, "%llu\n", val);
}
static DEVICE_ATTR_RO(reader_cnt);


static ssize_t data_retention_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct icap *icap = platform_get_drvdata(to_platform_device(dev));
	xdev_handle_t xdev = xocl_get_xdev(to_platform_device(dev));
	u32 val = 0, ack;
	int err;

	if (!ICAP_PRIVILEGED(icap))
		goto done;

	err = xocl_iores_read32(xdev, XOCL_SUBDEV_LEVEL_PRP,
			IORES_DDR4_RESET_GATE, 0, &ack);
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
	xdev_handle_t xdev = xocl_get_xdev(to_platform_device(dev));
	u32 val, ack;
	int err = 0;

	if (!ICAP_PRIVILEGED(icap))
		goto done;

	/* Must have ddr gate pin */
	err = xocl_iores_read32(xdev, XOCL_SUBDEV_LEVEL_PRP,
			IORES_DDR4_RESET_GATE, 0, &ack);
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
	u64 val = 0;

	mutex_lock(&icap->icap_lock);

	val = icap->max_host_mem_aperture;

	mutex_unlock(&icap->icap_lock);

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
	u32 nread = 0;
	size_t size = 0;
	int err = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (!icap || !icap->debug_layout)
		return nread;

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return nread;

	size = sizeof_sect(icap->debug_layout, m_debug_ip_data);
	if (offset >= size)
		goto unlock;

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, ((char *)icap->debug_layout) + offset, nread);

unlock:
	icap_xclbin_rd_unlock(icap);
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
	int err = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (!icap || !icap->ip_layout)
		return nread;

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return nread;


	size = sizeof_sect(icap->ip_layout, m_ip_data);
	if (offset >= size)
		goto unlock;

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, ((char *)icap->ip_layout) + offset, nread);

unlock:
	icap_xclbin_rd_unlock(icap);
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
	int err = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (!icap || !icap->connectivity)
		return nread;

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return nread;

	size = sizeof_sect(icap->connectivity, m_connection);
	if (offset >= size)
		goto unlock;

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, ((char *)icap->connectivity) + offset, nread);

unlock:
	icap_xclbin_rd_unlock(icap);
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
	struct icap *icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));
	u32 nread = 0;
	size_t size = 0;
	uint64_t range = 0;
	int err = 0, i;
	struct mem_topology *mem_topo = NULL;
	xdev_handle_t xdev;

	if (!icap || !icap->mem_topo)
		return nread;

	xdev = xocl_get_xdev(icap->icap_pdev);

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return nread;

	size = sizeof_sect(icap->mem_topo, m_mem_data);
	if (offset >= size)
		goto unlock;

	mem_topo = vzalloc(size);
	if (!mem_topo)
		goto unlock;

	memcpy(mem_topo, icap->mem_topo, size);
	range = xocl_addr_translator_get_range(xdev);	
	for ( i=0; i< mem_topo->m_count; ++i) {
		if (IS_HOST_MEM(mem_topo->m_mem_data[i].m_tag)){
			/* m_size in KB, convert Byte to KB */
			mem_topo->m_mem_data[i].m_size = (range>>10);
		} else
			continue;
	}

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, ((char *)mem_topo) + offset, nread);
unlock:
	icap_xclbin_rd_unlock(icap);
	vfree(mem_topo);
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

/* -Mem_topology-- */
static ssize_t icap_read_clock_freqs(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buffer, loff_t offset, size_t count)
{
	struct icap *icap;
	u32 nread = 0;
	size_t size = 0;
	int err = 0;

	icap = (struct icap *)dev_get_drvdata(container_of(kobj, struct device, kobj));

	if (!icap || !icap->xclbin_clock_freq_topology)
		return nread;

	err = icap_xclbin_rd_lock(icap);
	if (err)
		return nread;

	size = sizeof_sect(icap->xclbin_clock_freq_topology, m_clock_freq);
	if (offset >= size)
		goto unlock;

	if (count < size - offset)
		nread = count;
	else
		nread = size - offset;

	memcpy(buffer, ((char *)icap->xclbin_clock_freq_topology) + offset, nread);
unlock:
	icap_xclbin_rd_unlock(icap);
	return nread;
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
	&connectivity_attr,
	&mem_topology_attr,
	&rp_bit_attr,
	&clock_freq_topology_attr,
	NULL,
};

static struct attribute_group icap_attr_group = {
	.attrs = icap_attrs,
	.bin_attrs = icap_bin_attrs,
};

static int icap_remove(struct platform_device *pdev)
{
	struct icap *icap = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	void *hdl;

	BUG_ON(icap == NULL);
	xocl_drvinst_release(icap, &hdl);

	xocl_xmc_freeze(xdev);
	icap_free_bins(icap);

	iounmap(icap->icap_regs);
	xclbin_free_clock_freq_topology(icap);

	sysfs_remove_group(&pdev->dev.kobj, &icap_attr_group);
	icap_clean_bitstream_axlf(pdev);
	ICAP_INFO(icap, "cleaned up successfully");
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);
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
	void **regs;

	icap = xocl_drvinst_alloc(&pdev->dev, sizeof(*icap));
	if (!icap)
		return -ENOMEM;
	platform_set_drvdata(pdev, icap);
	icap->icap_pdev = pdev;
	mutex_init(&icap->icap_lock);
	init_waitqueue_head(&icap->reader_wq);

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
	XHwIcap_Bit_Header bit_header = { 0 };
	const struct firmware *sche_fw = NULL;
	ssize_t ret, len;
	int err;

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
	section = get_axlf_section_hdr(icap, axlf, PARTITION_METADATA);
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

	section = get_axlf_section_hdr(icap, axlf, BITSTREAM);
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
	if (bitstream_parse_header(icap, header,
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
	section = get_axlf_section_hdr(icap, axlf, FIRMWARE);
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

	if (XDEV(xdev)->priv.sched_bin) {
		err = request_firmware(&sche_fw,
			XDEV(xdev)->priv.sched_bin, &pcidev->dev);
		if (!err)  {
			icap->rp_sche_bin = vmalloc(sche_fw->size);
			if (!icap->rp_sche_bin) {
				ICAP_ERR(icap, "Not enough mem for sched bin");
				ret = -ENOMEM;
				goto failed;
			}
			ICAP_INFO(icap, "stashed shared mb sche bin, len %ld", sche_fw->size);
			memcpy(icap->rp_sche_bin, sche_fw->data, sche_fw->size);
			icap->rp_sche_bin_len = sche_fw->size;
			release_firmware(sche_fw);
		}
	}


	section = get_axlf_section_hdr(icap, axlf, SCHED_FIRMWARE);
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
	}

	vfree(axlf);

	ICAP_INFO(icap, "write axlf to device successfully. len %ld", len);

	mutex_unlock(&icap->icap_lock);

	return len;

failed:
	icap_free_bins(icap);
	if (sche_fw)
		release_firmware(sche_fw);

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
