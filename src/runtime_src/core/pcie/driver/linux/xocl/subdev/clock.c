/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
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

#include "../xocl_drv.h"

/* CLOCK_MAX_NUM_CLOCKS should be a concept from XCLBIN_ in the future */
#define	CLOCK_MAX_NUM_CLOCKS		4
#define	OCL_CLKWIZ_STATUS_OFFSET	0x4
#define	OCL_CLKWIZ_STATUS_MASK		0xffff
#define	OCL_CLKWIZ_STATUS_MEASURE_START	0x1
#define	OCL_CLKWIZ_STATUS_MEASURE_DONE	0x2
#define	OCL_CLKWIZ_CONFIG_OFFSET(n)	(0x200 + 4 * (n))
#define	OCL_CLK_FREQ_COUNTER_OFFSET	0x8
#define	OCL_CLK_FREQ_V5_COUNTER_OFFSET	0x10
#define	OCL_CLK_FREQ_V5_CLK0_ENABLED	0x10000
#define	CLOCK_DEFAULT_EXPIRE_SECS	1

/* REGs for ACAP Versal */
#define	OCL_CLKWIZ_INIT_CONFIG		0x14
#define	OCL_CLKWIZ_DIVCLK		0x380
#define	OCL_CLKWIZ_DIVCLK_TS		0x384
#define	OCL_CLKWIZ_CLKFBOUT		0x330
#define	OCL_CLKWIZ_CLKFBOUT_TS		0x334
#define	OCL_CLKWIZ_CLKFBOUT_FRACT	0x3fc
#define	OCL_CLKWIZ_CLKOUT0		0x338
#define	OCL_CLKWIZ_CLKOUT0_TS		0x33c

#define	CLK_MAX_VALUE		6400
#define	CLK_SHUTDOWN_BIT	0x1
#define	DEBUG_CLK_SHUTDOWN_BIT	0x2
#define	VALID_CLKSHUTDOWN_BITS	(CLK_SHUTDOWN_BIT|DEBUG_CLK_SHUTDOWN_BIT)

#define	CLK_ACAP_MAX_VALUE_FOR_O	4320
#define	CLK_ACAP_INPUT_FREQ		33.333
/* no float number in kernel, x/33.333 will be converted to x * 1000 / 33333) */
#define	CLK_ACAP_INPUT_FREQ_X_1000	33333

#define	CLOCK_ERR(clock, fmt, arg...)	\
	xocl_err(&(clock)->clock_pdev->dev, fmt "\n", ##arg)
#define	CLOCK_WARN(clock, fmt, arg...)	\
	xocl_warn(&(clock)->clock_pdev->dev, fmt "\n", ##arg)
#define	CLOCK_INFO(clock, fmt, arg...)	\
	xocl_info(&(clock)->clock_pdev->dev, fmt "\n", ##arg)
#define	CLOCK_DBG(clock, fmt, arg...)	\
	xocl_dbg(&(clock)->clock_pdev->dev, fmt "\n", ##arg)

/* spec definition of ucs_control_status channel1 */
struct ucs_control_status_ch1 {
	unsigned int shutdown_clocks_latched:1;
	unsigned int reserved1:15;
	unsigned int clock_throttling_average:14;
	unsigned int reserved2:2;
};

/* spec definition of ACAP Versal */
struct acap_clkfbout {
	u32 clkfbout_dt		:8;
	u32 clkfbout_edge	:1;
	u32 clkfbout_en		:1;
	u32 clkfbout_mx		:2;
	u32 clkfbout_prediv2	:1;
	u32 reserved		:19;
};

struct acap_clkfbout_ts {
	u32 clkfbout_lt		:8;
	u32 clkfbout_ht		:8;
	u32 reserved		:16;
};

struct acap_clkout0 {
	u32 clkout0_dt		:8;
	u32 clkout0_edge	:1;
	u32 clkout0_mx		:2;
	u32 clkout0_prediv2	:1;
	u32 clkout0_used	:1;
	u32 clkout0_p5en	:1;
	u32 clkout0_start_h	:1;
	u32 clkout0_p5_edge	:1;
	u32 reserved		:16;
};

struct acap_clkout0_ts {
	u32 clkout0_lt		:8;
	u32 clkout0_ht		:8;
	u32 reserved		:16;
};

struct acap_divclk {
	u32 deskew_dly_2nd	:6;
	u32 deskew_dly_en_2nd	:1;
	u32 deskew_dly_path_2nd	:1;
	u32 deskew_en_2nd	:1;
	u32 direct_path_cntrl	:1;
	u32 divclk_edge		:1;
	u32 reserved		:21;
};

struct acap_divclk_ts {
	u32 divclk_lt		:8;
	u32 divclk_ht		:8;
	u32 reserved		:16;
};

struct acap_clkfbout_fract {
	u32 clkfbout_fract_alg	:1;
	u32 clkfbout_fract_en	:1;
	u32 clkfbout_fract_order:1;
	u32 clkfbout_fract_seed	:2;
	u32 skew_sel		:6;
	u32 reserved		:21;
};

struct xocl_iores_map clock_res_map[] = {
	{ RESNAME_CLKWIZKERNEL1, CLOCK_IORES_CLKWIZKERNEL1 },
	{ RESNAME_CLKWIZKERNEL2, CLOCK_IORES_CLKWIZKERNEL2 },
	{ RESNAME_CLKWIZKERNEL3, CLOCK_IORES_CLKWIZKERNEL3 },
	{ RESNAME_CLKFREQ_K1_K2, CLOCK_IORES_CLKFREQ_K1_K2},
	{ RESNAME_CLKFREQ_HBM, CLOCK_IORES_CLKFREQ_HBM },
	{ RESNAME_CLKFREQ_K1, CLOCK_IORES_CLKFREQ_K1},
	{ RESNAME_CLKFREQ_K2, CLOCK_IORES_CLKFREQ_K2},
	{ RESNAME_CLKSHUTDOWN, CLOCK_IORES_CLKSHUTDOWN },
	{ RESNAME_UCS_CONTROL_STATUS, CLOCK_IORES_UCS_CONTROL_STATUS},
};

struct clock {
	struct platform_device  *clock_pdev;
	void __iomem 		*clock_base_address[CLOCK_IORES_MAX]; 
	struct mutex 		clock_lock;
	void __iomem		*clock_ucs_control_status;
	/* Below are legacy iores fields, keep unchanged until necessary */
	void __iomem		*clock_bases[CLOCK_MAX_NUM_CLOCKS];
	unsigned short		clock_ocl_frequency[CLOCK_MAX_NUM_CLOCKS];
	struct clock_freq_topology *clock_freq_topology_p;
	unsigned long		clock_freq_topology_length;
	void __iomem		*clock_freq_counter;
	void __iomem		*clock_freq_counters[CLOCK_MAX_NUM_CLOCKS];
};

static inline void __iomem *
clock_iores_get_base(struct clock *clock, int id)
{
	return clock->clock_base_address[id];
}

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

	XOCL_WRITE_REG32(val, reg);
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

static unsigned find_matching_freq_config(unsigned freq,
	const struct xclmgmt_ocl_clockwiz *table, int size)
{
	unsigned start = 0;
	unsigned end = size - 1;
	unsigned idx = size - 1;

	if (freq < table[0].ocl)
		return 0;

	if (freq > table[size - 1].ocl)
		return size - 1;

	while (start < end) {
		if (freq == table[idx].ocl)
			break;
		if (freq < table[idx].ocl)
			end = idx;
		else
			start = idx + 1;
		idx = start + (end - start) / 2;
	}
	if (freq < table[idx].ocl)
		idx--;

	return idx;
}

static unsigned find_matching_freq(unsigned freq,
	const struct xclmgmt_ocl_clockwiz *freq_table, int freq_table_size)
{
	int idx = find_matching_freq_config(freq, freq_table, freq_table_size);

	return freq_table[idx].ocl;
}

static unsigned int clock_get_freq_counter_khz_impl(struct clock *clock, int idx)
{
	u32 freq = 0, status;
	int times = 10;

	BUG_ON(idx > CLOCK_MAX_NUM_CLOCKS);
	BUG_ON(!mutex_is_locked(&clock->clock_lock));

	if (clock->clock_freq_counter && idx < 2) {
		reg_wr(clock->clock_freq_counter,
			OCL_CLKWIZ_STATUS_MEASURE_START);
		while (times != 0) {
			status = reg_rd(clock->clock_freq_counter);
			if ((status & OCL_CLKWIZ_STATUS_MASK) ==
				OCL_CLKWIZ_STATUS_MEASURE_DONE)
				break;
			mdelay(1);
			times--;
		};
		if ((status & OCL_CLKWIZ_STATUS_MASK) ==
			OCL_CLKWIZ_STATUS_MEASURE_DONE)
			freq = reg_rd(clock->clock_freq_counter + OCL_CLK_FREQ_COUNTER_OFFSET + idx*sizeof(u32));
		return freq;
	}

	if (clock->clock_freq_counters[idx]) {
		reg_wr(clock->clock_freq_counters[idx],
			OCL_CLKWIZ_STATUS_MEASURE_START);
		while (times != 0) {
			status =
			    reg_rd(clock->clock_freq_counters[idx]);
			if ((status & OCL_CLKWIZ_STATUS_MASK) ==
				OCL_CLKWIZ_STATUS_MEASURE_DONE)
				break;
			mdelay(1);
			times--;
		};
		if ((status & OCL_CLKWIZ_STATUS_MASK) ==
			OCL_CLKWIZ_STATUS_MEASURE_DONE) {
			freq = (status & OCL_CLK_FREQ_V5_CLK0_ENABLED) ?
				reg_rd(clock->clock_freq_counters[idx] + OCL_CLK_FREQ_V5_COUNTER_OFFSET) :
				reg_rd(clock->clock_freq_counters[idx] + OCL_CLK_FREQ_COUNTER_OFFSET);
		}
	}
	return freq;
}

/* For ACAP Versal, we read from freq counter directly */
static unsigned short clock_get_freq_acap(struct clock *clock, int idx)
{
	u32 freq_counter = 0;
	if (clock->clock_freq_counters[idx]) {
		freq_counter = clock_get_freq_counter_khz_impl(clock, idx);
		freq_counter /= 1000; /* KHZ */
	}

	return freq_counter;
}

static unsigned short clock_get_freq_ultrascale(struct clock *clock, int idx)
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

	BUG_ON(!mutex_is_locked(&clock->clock_lock));

	base = clock->clock_bases[idx];
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
		CLOCK_ERR(clock, "clockwiz 0 divider");
		return 0;
	}
	freq = (input * mul0) / div0;
	return freq;
}

static unsigned short clock_get_freq_impl(struct clock *clock, int idx)
{
	xdev_handle_t xdev = xocl_get_xdev(clock->clock_pdev);

	return XOCL_DSA_IS_VERSAL(xdev) ?
	    clock_get_freq_acap(clock, idx) :
	    clock_get_freq_ultrascale(clock, idx);
}

static inline int clock_wiz_busy(struct clock *clock, int idx, int cycle,
	int interval)
{
	u32 val = 0;
	int count;

	val = reg_rd(clock->clock_bases[idx] + OCL_CLKWIZ_STATUS_OFFSET);
	for (count = 0; val != 1 && count < cycle; count++) {
		mdelay(interval);
		val = reg_rd(clock->clock_bases[idx] + OCL_CLKWIZ_STATUS_OFFSET);
	}
	if (val != 1) {
		CLOCK_ERR(clock, "clockwiz(%d) is (%u) busy after %d ms",
		    idx, val, cycle * interval);
		return -ETIMEDOUT;
	}

	return 0;
}

static inline unsigned int floor_acap_o(int freq)
{
	return (CLK_ACAP_MAX_VALUE_FOR_O / freq);
}

/*
 * Kernel compiler even has disabled SSE(floating caculation) for preprocessor,
 * we need a simple math to count floor without losing too much accuracy.
 * formula: (O * freq / 33.333)
 */
static inline unsigned int floor_acap_m(int freq)
{
	return (floor_acap_o(freq) * freq * 1000 / CLK_ACAP_INPUT_FREQ_X_1000);
}

/*
 * Based on Clocking Wizard Versal ACAP, section Dynamic Reconfiguration
 * through AXI4-Lite
 */
static int clock_ocl_freqscaling_acap(struct clock *clock, bool force,
	u32 *curr_freq, int level)
{
	int i;
	int err = 0;
	u32 val;
	struct acap_divclk 		*divclk;
	struct acap_divclk_ts 		*divclk_ts;
	struct acap_clkfbout_fract 	*fract;
	struct acap_clkfbout 		*clkfbout;
	struct acap_clkfbout_ts 	*clkfbout_ts;
	struct acap_clkout0 		*clkout0;
	struct acap_clkout0_ts 		*clkout0_ts;
	unsigned int M, O;

	BUG_ON(!mutex_is_locked(&clock->clock_lock));

	for (i = 0; i < CLOCK_MAX_NUM_CLOCKS; ++i) {
		/*
		 * A value of zero means skip scaling for this clock index.
		 * Note: for ULP clock, we will reset old value again, thus
		 *       we save old value into the request, and then
		 *       continue the setting for every non zero request.
		 */
		if (!clock->clock_ocl_frequency[i])
			continue;

		/* skip if the io does not exist */
		if (!clock->clock_bases[i])
			continue;


		CLOCK_INFO(clock,
		    "Clock: %d, Current: %d MHz, New: %d Mhz,  Force: %d",
		    i, curr_freq[i], clock->clock_ocl_frequency[i], force);

		/*
		 * If current frequency is in the same step as the
		 * requested frequency then nothing to do.
		 */
		if (!force && curr_freq[i] == clock->clock_ocl_frequency[i]) {
			CLOCK_INFO(clock, "current freq and new freq are the "
			    "same, skip updating.");
			continue;
		}

		err = clock_wiz_busy(clock, i, 20, 50);
		if (err)
			break;
		/*
		 * Simplified formula for ACAP clock wizard.
		 * 1) Set DIVCLK_EDGE, DIVCLK_LT and DIVCLK_HT to 0;
		 * 2) Set CLKFBOUT_FRACT_EN to 0;
		 * 3) O = floor(4320/freq_req), M = floor((O*freq_req)/33.333);
		 * 4) CLKFBOUT_EDGE = if M%2 write 0x17, else write 0x16
		 * 5) CLKFBOUT_LT_HT = (M-M%2)/2_(M-M%2)/2
		 * 6) check CLKOUT0_PREDIV2, CLKOUT0_P5EN == 0
		 * 7) CLKOUT0_EDGE O%2 write 0x13, else write 0x12
		 * 8) CLKOUT0_LT_HT = (O-(O%2))/2
		 */
		/* Step 1) */
		val = reg_rd(clock->clock_bases[i] + OCL_CLKWIZ_DIVCLK);
		divclk = (struct acap_divclk *)&val;
		divclk->divclk_edge = 0;
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_DIVCLK, val);

		val = reg_rd(clock->clock_bases[i] + OCL_CLKWIZ_DIVCLK_TS);
		divclk_ts = (struct acap_divclk_ts *)&val;
		divclk_ts->divclk_lt = 0;
		divclk_ts->divclk_ht = 0;
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_DIVCLK_TS, val);

		/* Step 2) */
		val = reg_rd(clock->clock_bases[i] + OCL_CLKWIZ_CLKFBOUT_FRACT);
		fract = (struct acap_clkfbout_fract *)&val;
		fract->clkfbout_fract_en = 0;
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_CLKFBOUT_FRACT, val);

		/* Step 3) */
		O = floor_acap_o(clock->clock_ocl_frequency[i]);
		M = floor_acap_m(clock->clock_ocl_frequency[i]);

		/* Step 4) */
		val = reg_rd(clock->clock_bases[i] + OCL_CLKWIZ_CLKFBOUT);
		clkfbout = (struct acap_clkfbout *)&val;
		clkfbout->clkfbout_edge = (M % 2) ? 1 : 0;
		clkfbout->clkfbout_en = 1;
		clkfbout->clkfbout_mx = 1;
		clkfbout->clkfbout_prediv2 = 1;
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_CLKFBOUT, val);

		/* Step 5) */
		val = 0;
		clkfbout_ts = (struct acap_clkfbout_ts *)&val;
		clkfbout_ts->clkfbout_lt = (M - (M % 2)) / 2;
		clkfbout_ts->clkfbout_ht = (M - (M % 2)) / 2;
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_CLKFBOUT_TS, val);

		/* Step 6, 7) */
		val = reg_rd(clock->clock_bases[i] + OCL_CLKWIZ_CLKOUT0);
		clkout0 = (struct acap_clkout0 *)&val;
		clkout0->clkout0_edge = (O % 2) ? 1 : 0;
		clkout0->clkout0_mx = 1;
		clkout0->clkout0_used = 1;
		clkout0->clkout0_prediv2 = 0;
		clkout0->clkout0_p5en = 0;
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_CLKOUT0, val);

		/* Step 8) */
		val = 0;
		clkout0_ts = (struct acap_clkout0_ts *)&val;
		clkout0_ts->clkout0_lt = (O - (O % 2)) / 2;
		clkout0_ts->clkout0_ht = (O - (O % 2)) / 2;
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_CLKOUT0_TS, val);

		/* init the freq change */
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_INIT_CONFIG, 0x3);
		err = clock_wiz_busy(clock, i, 100, 100);
		if (err)
			break;
	}

	CLOCK_INFO(clock, "returns %d", err);
	return err;
}

/*
 * Based on Clocking Wizard v5.1, section Dynamic Reconfiguration
 * through AXI4-Lite
 * Note: this is being protected by write_lock which is atomic context,
 *       we should only use n[m]delay instead of n[m]sleep.
 *       based on Linux doc of timers, mdelay may not be exactly accurate
 *       on non-PC devices.
 */
static int clock_ocl_freqscaling_ultrascale(struct clock *clock, bool force,
	u32 *curr_freq, int level)
{
	u32 config;
	int i;
	u32 val = 0;
	unsigned idx = 0;
	long err = 0;

	BUG_ON(!mutex_is_locked(&clock->clock_lock));

	/* explicitly force clock update for ULP */
	if (level == XOCL_SUBDEV_LEVEL_URP)
		force = true;

	for (i = 0; i < CLOCK_MAX_NUM_CLOCKS; ++i) {

		/* A value of zero means skip scaling for this clock index */
		if (!clock->clock_ocl_frequency[i])
			continue;

		/* skip if the io does not exist */
		if (!clock->clock_bases[i])
			continue;

		idx = find_matching_freq_config(clock->clock_ocl_frequency[i],
		    frequency_table, ARRAY_SIZE(frequency_table));

		CLOCK_INFO(clock,
		    "Clock: %d, Current: %d MHz, New: %d Mhz,  Force: %d",
		    i, curr_freq[i], clock->clock_ocl_frequency[i], force);

		/*
		 * If current frequency is in the same step as the
		 * requested frequency then nothing to do.
		 */
		if (!force && (find_matching_freq_config(curr_freq[i],
		    frequency_table, ARRAY_SIZE(frequency_table)) == idx)) {
			CLOCK_INFO(clock, "current freq and new freq are the "
			    "same, skip updating.");
			continue;
		}

		err = clock_wiz_busy(clock, i, 20, 50);
		if (err)
			break;

		config = frequency_table[idx].config0;
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_CONFIG_OFFSET(0),
			config);
		config = frequency_table[idx].config2;
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_CONFIG_OFFSET(2),
			config);
		mdelay(10);
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_CONFIG_OFFSET(23),
			0x00000007);
		mdelay(1);
		reg_wr(clock->clock_bases[i] + OCL_CLKWIZ_CONFIG_OFFSET(23),
			0x00000002);

		CLOCK_INFO(clock, "clockwiz waiting for locked signal");

		err = clock_wiz_busy(clock, i, 100, 100);
		if (err) {
			CLOCK_ERR(clock, "clockwiz MMCM/PLL did not lock, "
				"restoring the original configuration");
			/* restore the original clock configuration */
			reg_wr(clock->clock_bases[i] +
				OCL_CLKWIZ_CONFIG_OFFSET(23), 0x00000004);
			mdelay(10);
			reg_wr(clock->clock_bases[i] +
				OCL_CLKWIZ_CONFIG_OFFSET(23), 0x00000000);
			err = -ETIMEDOUT;
			break;
		}
		val = reg_rd(clock->clock_bases[i] +
			OCL_CLKWIZ_CONFIG_OFFSET(0));
		CLOCK_INFO(clock, "clockwiz CONFIG(0) 0x%x", val);
		val = reg_rd(clock->clock_bases[i] +
			OCL_CLKWIZ_CONFIG_OFFSET(2));
		CLOCK_INFO(clock, "clockwiz CONFIG(2) 0x%x", val);
	}

	CLOCK_INFO(clock, "returns %ld", err);
	return err;
}

static int clock_ocl_freqscaling_impl(struct clock *clock, bool force,
	u32 *curr_freq, int level)
{
	xdev_handle_t xdev = xocl_get_xdev(clock->clock_pdev);

	return XOCL_DSA_IS_VERSAL(xdev) ?
	    clock_ocl_freqscaling_acap(clock, force, curr_freq, level) :
	    clock_ocl_freqscaling_ultrascale(clock, force, curr_freq, level);
}

static int clock_update_freqs_request(struct clock *clock, unsigned short *freqs,
	int num_freqs)
{
	xdev_handle_t xdev = xocl_get_xdev(clock->clock_pdev);
	int i;
	u32 val;

	for (i = 0; i < min(CLOCK_MAX_NUM_CLOCKS, num_freqs); ++i) {
		if (freqs[i] == 0)
			continue;

		if (!clock->clock_bases[i])
			continue;

		val = reg_rd(clock->clock_bases[i] +
			OCL_CLKWIZ_STATUS_OFFSET);
		if ((val & 0x1) == 0) {
			CLOCK_ERR(clock, "clockwiz %d is busy", i);
			return -EBUSY;
		}
	}

	memcpy(clock->clock_ocl_frequency, freqs,
		sizeof(*freqs) * min(CLOCK_MAX_NUM_CLOCKS, num_freqs));

	if (CLOCK_DEV_LEVEL(xdev) <= XOCL_SUBDEV_LEVEL_PRP)
		return 0;

	/* For ULP level clock, we should also reset all existing freqs */
	for (i = 0; i < CLOCK_MAX_NUM_CLOCKS; i++) {
		if (clock->clock_ocl_frequency[i] != 0)
			continue;
		clock->clock_ocl_frequency[i] = clock_get_freq_impl(clock, i);
	}

	return 0;
}

/*
 * Freeze has to be called and succeeded to perform gate reatled operation!
 */
static int clock_freeze_axi_gate(struct clock *clock, int level)
{
	xdev_handle_t xdev = xocl_get_xdev(clock->clock_pdev);
	int err;

	BUG_ON(!mutex_is_locked(&clock->clock_lock));

	if (level <= XOCL_SUBDEV_LEVEL_PRP)
		err = xocl_axigate_freeze(xdev, XOCL_SUBDEV_LEVEL_PRP);
	else
		err = xocl_axigate_reset(xdev, XOCL_SUBDEV_LEVEL_PRP);

	CLOCK_INFO(clock, "level %d returns %d", level, err);
	return err;
}

static int clock_free_axi_gate(struct clock *clock, int level)
{
	xdev_handle_t xdev = xocl_get_xdev(clock->clock_pdev);
	int err = 0;

	BUG_ON(!mutex_is_locked(&clock->clock_lock));

	if (level <= XOCL_SUBDEV_LEVEL_PRP) {
		xocl_axigate_free(xdev, XOCL_SUBDEV_LEVEL_PRP);
	} else {
		if (!clock->clock_ucs_control_status) {
			CLOCK_ERR(clock, "URP clock has no %s\n",
				RESNAME_UCS_CONTROL_STATUS);
			err = -EEXIST;
			goto done;
		}
		/* enable kernel clocks */
		CLOCK_INFO(clock, "Enable kernel clocks ucs control");
		msleep(10);
		reg_wr(clock->clock_ucs_control_status +
			XOCL_RES_OFFSET_CHANNEL2, 0x1);
	}

done:
	CLOCK_INFO(clock, "level %d returns %d", level, err);
	return err;
}

/*
 * Legacy flow:
 *   1) freeze axigate
 *   2) set clocks
 *   3) free axigate
 *
 * 2RP flow:
 *   1) reset axigate, clear all clocks and status.
 *   2) reset clocks, including previous clocks
 *   3) enable ucs_controll
 *   4) wait for hbm calibration done
 *
 * Note:
 * Violate this flow will cause random firewall trip.
 */
static int clock_ocl_freqscaling(struct clock *clock, bool force, int level)
{
	int i, err = 0;
	u32 curr[CLOCK_MAX_NUM_CLOCKS] = { 0 };

	/* Read current clock freq before freeze/toggle axi gate */
	for (i = 0; i < CLOCK_MAX_NUM_CLOCKS; i++)
		curr[i] = clock_get_freq_impl(clock, i);

	err = clock_freeze_axi_gate(clock, level);
	if (!err) {
		err = clock_ocl_freqscaling_impl(clock, force, curr, level);
		clock_free_axi_gate(clock, level);
	}

	CLOCK_INFO(clock, "level: %d return: %d", level, err);
	return err;
}

static int set_freqs(struct clock *clock, unsigned short *freqs, int num_freqs)
{
	xdev_handle_t xdev = xocl_get_xdev(clock->clock_pdev);
	int err;

	BUG_ON(!mutex_is_locked(&clock->clock_lock));

	err = clock_update_freqs_request(clock, freqs, num_freqs);
	if (!err)
		err = clock_ocl_freqscaling(clock, false, CLOCK_DEV_LEVEL(xdev));

	CLOCK_INFO(clock, "returns %d", err);
	return err;
}

static int set_and_verify_freqs(struct clock *clock, unsigned short *freqs,
	int num_freqs)
{
	int i;
	int err;
	u32 clock_freq_counter, request_in_khz, tolerance, lookup_freq;

	BUG_ON(!mutex_is_locked(&clock->clock_lock));

	err = set_freqs(clock, freqs, num_freqs);
	if (err)
		goto done;

	for (i = 0; i < min(CLOCK_MAX_NUM_CLOCKS, num_freqs); ++i) {
		if (!freqs[i])
			continue;

		lookup_freq = find_matching_freq(freqs[i], frequency_table,
		    ARRAY_SIZE(frequency_table));
		clock_freq_counter = clock_get_freq_counter_khz_impl(clock, i);
		request_in_khz = lookup_freq*1000;
		tolerance = lookup_freq*50;

		if (tolerance < abs(clock_freq_counter-request_in_khz)) {
			CLOCK_ERR(clock, "Frequency is higher than tolerance value, request %u"
					"khz, actual %u khz",request_in_khz, clock_freq_counter);
			err = -EDOM;
			break;
		}
	}

done:
	return err;
}

static int clock_freq_scaling(struct platform_device *pdev, bool force)
{
	struct clock *clock = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(clock->clock_pdev);
	int err;

	mutex_lock(&clock->clock_lock);
	err =  clock_ocl_freqscaling(clock, force, CLOCK_DEV_LEVEL(xdev));
	mutex_unlock(&clock->clock_lock);

	CLOCK_INFO(clock, "ret: %d.", err);
	return err;
}

static int clock_update_freq(struct platform_device *pdev,
	unsigned short *freqs, int num_freqs, int verify)
{
	struct clock *clock = platform_get_drvdata(pdev);
	int err;

	mutex_lock(&clock->clock_lock);
	err = verify ?
	    set_and_verify_freqs(clock, freqs, num_freqs) :
	    set_freqs(clock, freqs, num_freqs);
	mutex_unlock(&clock->clock_lock);

	CLOCK_INFO(clock, "verify: %d ret: %d.", verify, err);
	return err;
}

static int clock_get_freq_counter_khz(struct platform_device *pdev,
	unsigned int *value, int id)
{
	struct clock *clock = platform_get_drvdata(pdev);

	if (id > CLOCK_MAX_NUM_CLOCKS) {
		CLOCK_ERR(clock, "id %d cannot be greater than %d",
		    id, CLOCK_MAX_NUM_CLOCKS);
		return -EINVAL;
	}

	mutex_lock(&clock->clock_lock);
	*value = clock_get_freq_counter_khz_impl(clock, id);
	mutex_unlock(&clock->clock_lock);

	CLOCK_INFO(clock, "khz: %d", *value);
	return 0;
}

static int clock_get_freq_by_id(struct platform_device *pdev,
	unsigned int region, unsigned short *freq, int id)
{
	struct clock *clock = platform_get_drvdata(pdev);

	/* For now, only PR region 0 is supported. */
	if (region != 0) {
		CLOCK_ERR(clock, "only PR region 0 is supported");
		return -EINVAL;
	}

	if (id >= CLOCK_MAX_NUM_CLOCKS) {
		CLOCK_ERR(clock, "id %d cannot be greater than %d",
		    id, CLOCK_MAX_NUM_CLOCKS);
		return -EINVAL;
	}

	mutex_lock(&clock->clock_lock);
	*freq = clock_get_freq_impl(clock, id);
	mutex_unlock(&clock->clock_lock);

	CLOCK_INFO(clock, "freq = %hu", *freq);
	return 0;
}

static int clock_get_freq(struct platform_device *pdev,
	unsigned int region, unsigned short *freqs, int num_freqs)
{
	int i;
	struct clock *clock = platform_get_drvdata(pdev);

	/* For now, only PR region 0 is supported. */
	if (region != 0) {
		CLOCK_ERR(clock, "only PR region 0 is supported");
		return -EINVAL;
	}

	mutex_lock(&clock->clock_lock);
	for (i = 0; i < min(CLOCK_MAX_NUM_CLOCKS, num_freqs); i++)
		freqs[i] = clock_get_freq_impl(clock, i);
	mutex_unlock(&clock->clock_lock);

	CLOCK_INFO(clock, "done.");
	return 0;
}

static int clock_status_check(struct platform_device *pdev, bool *latched)
{	
	struct clock *clock = platform_get_drvdata(pdev);
	void __iomem *shutdown_clk =
		clock_iores_get_base(clock, CLOCK_IORES_CLKSHUTDOWN);
	void __iomem *ucs_control_status =
		clock_iores_get_base(clock, CLOCK_IORES_UCS_CONTROL_STATUS);
	uint32_t status;
	int err = 0;

	mutex_lock(&clock->clock_lock);

	if (shutdown_clk) {
		status = reg_rd(shutdown_clk);
		/* BIT0:latch bit, BIT1:Debug bit */
		if (!(status & (~VALID_CLKSHUTDOWN_BITS))) {
			*latched = status & CLK_SHUTDOWN_BIT;
			if (*latched) {
				CLOCK_ERR(clock, "Compute-Unit clocks have "
				    "been stopped! Power or Temp may exceed "
				    "limits, notify peer");
			}
		}
	} else if (ucs_control_status) {
		struct ucs_control_status_ch1 *ucs_status_ch1;

		/* this must be a R2.0 system */
		status = reg_rd(ucs_control_status + XOCL_RES_OFFSET_CHANNEL1);
		ucs_status_ch1 = (struct ucs_control_status_ch1 *)&status;
		if (ucs_status_ch1->shutdown_clocks_latched) {
			CLOCK_ERR(clock, "Critical temperature or power event, "
			    "kernel clocks have been stopped, run "
			    "'xbutil valiate -q' to continue. "
			    "See AR 73398 for more details.");
			/* explicitly indicate reset should be latched */
			*latched = true;
		} else if (ucs_status_ch1->clock_throttling_average > CLK_MAX_VALUE) {
			CLOCK_ERR(clock, "kernel clocks %d exceeds "
			    "expected maximum value %d.",
			    ucs_status_ch1->clock_throttling_average, CLK_MAX_VALUE);
		} else if (ucs_status_ch1->clock_throttling_average) {
			CLOCK_ERR(clock, "kernel clocks throttled at %d%%.",
			    (ucs_status_ch1->clock_throttling_average /
			    (CLK_MAX_VALUE / 100)));
		}
	}

	mutex_unlock(&clock->clock_lock);

	/* do not output status log here, this function might be called every 5s */
	return err;
}

/* there are some iores have not been defined in neither xsabin nor xclbin */
static void clock_prev_refresh_addrs(struct clock *clock)
{
	xdev_handle_t xdev = xocl_get_xdev(clock->clock_pdev);

	mutex_lock(&clock->clock_lock);

	clock->clock_freq_counter =
		xocl_iores_get_base(xdev, IORES_CLKFREQ_K1_K2);
	CLOCK_INFO(clock, "freq_k1_k2 @ %lx",
			(unsigned long)clock->clock_freq_counter);

	clock->clock_freq_counters[2] =
		xocl_iores_get_base(xdev, IORES_CLKFREQ_HBM);
	CLOCK_INFO(clock, "freq_hbm @ %lx",
			(unsigned long)clock->clock_freq_counters[2]);

	mutex_unlock(&clock->clock_lock);

	CLOCK_INFO(clock, "done.");
}

static void clock_iores_update_base(struct clock *clock,
	void __iomem **resource, int id, bool force_update)
{
	char *res_name = xocl_res_id2name(clock_res_map,
	    ARRAY_SIZE(clock_res_map), id);

	if (*resource && !force_update) {
		CLOCK_INFO(clock, "%s has been set to %lx already.",
		    res_name ? res_name : "", (unsigned long)(*resource));
		return;
	}

	*resource = clock_iores_get_base(clock, id);
	CLOCK_INFO(clock, "%s @ %lx", res_name ? res_name : "",
	    (unsigned long)(*resource));
}

/* when iores has been loaded from xsabin or xclbin */
static int clock_post_refresh_addrs(struct clock *clock)
{
	int err = 0;

	mutex_lock(&clock->clock_lock);

	clock_iores_update_base(clock,
	    &clock->clock_bases[0], CLOCK_IORES_CLKWIZKERNEL1, true);

	clock_iores_update_base(clock,
	    &clock->clock_bases[1], CLOCK_IORES_CLKWIZKERNEL2, true);

	clock_iores_update_base(clock,
	    &clock->clock_bases[2], CLOCK_IORES_CLKWIZKERNEL3, true);

	clock_iores_update_base(clock,
	    &clock->clock_freq_counter, CLOCK_IORES_CLKFREQ_K1_K2, false);

	clock_iores_update_base(clock,
	    &clock->clock_freq_counters[0], CLOCK_IORES_CLKFREQ_K1, true);

	clock_iores_update_base(clock,
	    &clock->clock_freq_counters[1], CLOCK_IORES_CLKFREQ_K2, true);

	clock_iores_update_base(clock,
	    &clock->clock_freq_counters[2], CLOCK_IORES_CLKFREQ_HBM, false);

	clock_iores_update_base(clock,
	    &clock->clock_ucs_control_status, CLOCK_IORES_UCS_CONTROL_STATUS, true);

	/*
	 * Note: we are data driven, as long as ucs_control_status is present,
	 *       operations will be performed.
	 *       With new 2RP flow, clocks are all moved to ULP.  We assume
	 *       there is not any clock left in PLP in this case.
	 * Note: disable clock scaling during probe for ULP, because this will
	 *       happen only when newer xclbin has been downloaded, we will
	 *       always reset frequence by using data in xclbin.
	 *       when driver is reloaded but xclbin is not downloaded yet,
	 *       no clock data.
	 *
	 * Example of reset clock: enable only when we have to, because
	 *       this requires mig_calibration which will take few seconds.
	 *  if (clock->clock_ucs_control_status)
         *	err = clock_ocl_freqscaling(clock, true, XOCL_SUBDEV_LEVEL_URP);
	 */

	mutex_unlock(&clock->clock_lock);

	CLOCK_INFO(clock, "ret %d", err);
	return err;
}

static uint64_t clock_get_data_nolock(struct platform_device *pdev,
	enum data_kind kind)
{
	struct clock *clock = platform_get_drvdata(pdev);
	uint64_t target = 0;

	switch (kind) {
	case CLOCK_FREQ_0:
		target = clock_get_freq_impl(clock, 0);
		break;
	case CLOCK_FREQ_1:
		target = clock_get_freq_impl(clock, 1);
		break;
	case CLOCK_FREQ_2:
		target = clock_get_freq_impl(clock, 2);
		break;
	case FREQ_COUNTER_0:
		target = clock_get_freq_counter_khz_impl(clock, 0);
		break;
	case FREQ_COUNTER_1:
		target = clock_get_freq_counter_khz_impl(clock, 1);
		break;
	case FREQ_COUNTER_2:
		target = clock_get_freq_counter_khz_impl(clock, 2);
		break;
	default:
		break;
	}

	return target;
}

static uint64_t clock_get_data(struct platform_device *pdev,
	enum data_kind kind)
{
	struct clock *clock = platform_get_drvdata(pdev);
	uint64_t target = 0;

	mutex_lock(&clock->clock_lock);
	target = clock_get_data_nolock(pdev, kind);
	mutex_unlock(&clock->clock_lock);

	return target;
}

static ssize_t clock_freqs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct clock *clock = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;
	int i;
	u32 freq_counter, freq, request_in_khz, tolerance;

	mutex_lock(&clock->clock_lock);
	for (i = 0; i < CLOCK_MAX_NUM_CLOCKS; i++) {
		freq = clock_get_freq_impl(clock, i);

		if (clock->clock_freq_counter || clock->clock_freq_counters[i]) {
			freq_counter = clock_get_freq_counter_khz_impl(clock, i);
			request_in_khz = freq*1000;
			tolerance = freq*50;

			if (abs(freq_counter-request_in_khz) > tolerance)
				CLOCK_INFO(clock, "Frequency mismatch, Should be %u khz, Now is %ukhz", request_in_khz, freq_counter);
			cnt += sprintf(buf + cnt, "%d\n", DIV_ROUND_CLOSEST(freq_counter, 1000));
		} else
			cnt += sprintf(buf + cnt, "%d\n", freq);
	}

	mutex_unlock(&clock->clock_lock);
	return cnt;
}
static DEVICE_ATTR_RO(clock_freqs);

static struct attribute *clock_attrs[] = {
	&dev_attr_clock_freqs.attr,
	NULL,
};

static struct attribute_group clock_attr_group = {
	.attrs = clock_attrs,
};

static struct xocl_clock_funcs clock_ops = {
	.freq_scaling = clock_freq_scaling,
	.get_freq_counter_khz = clock_get_freq_counter_khz,
	.get_freq_by_id = clock_get_freq_by_id,
	.get_freq = clock_get_freq,
	.update_freq = clock_update_freq,
	.clock_status = clock_status_check,
	.get_data = clock_get_data,
};

static int clock_remove(struct platform_device *pdev)
{
	struct clock *clock;

	clock = platform_get_drvdata(pdev);
	if (!clock) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &clock_attr_group);
	mutex_destroy(&clock->clock_lock);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, clock);

	CLOCK_INFO(clock, "successfully removed Clock subdev");
	return 0;
}

static int clock_probe(struct platform_device *pdev)
{
	struct clock *clock = NULL;
	struct resource *res;
	int ret, i, id;

	clock = devm_kzalloc(&pdev->dev, sizeof(*clock), GFP_KERNEL);
	if (!clock)
		return -ENOMEM;

	platform_set_drvdata(pdev, clock);
	clock->clock_pdev = pdev;
	mutex_init(&clock->clock_lock);

	clock_prev_refresh_addrs(clock);

	for (i = 0, res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		res;
		res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {
		id = xocl_res_name2id(clock_res_map, ARRAY_SIZE(clock_res_map),
			res->name);
		if (id >= 0) {
			clock->clock_base_address[id] =
				ioremap_nocache(res->start,
				res->end - res->start + 1);
			if (!clock->clock_base_address[id]) {
				CLOCK_ERR(clock, "map base %pR failed", res);
				ret = -EINVAL;
				goto failed;
			} else {
				CLOCK_INFO(clock, "res[%d] %s mapped @ %lx",
				    i, res->name,
				    (unsigned long)clock->clock_base_address[id]);
			}
		}
	}
	ret = clock_post_refresh_addrs(clock);
	if (ret)
		goto failed;

	ret = sysfs_create_group(&pdev->dev.kobj, &clock_attr_group);
	if (ret) {
		CLOCK_ERR(clock, "create clock attrs failed: %d", ret);
		goto failed;
	}

	CLOCK_INFO(clock, "successfully initialized Clock subdev");
	return 0;

failed:
	(void) clock_remove(pdev);
	return ret;
}

struct xocl_drv_private clock_priv = {
	.ops = &clock_ops,
};

struct platform_device_id clock_id_table[] = {
	{ XOCL_DEVNAME(XOCL_CLOCK), (kernel_ulong_t)&clock_priv },
	{ },
};

static struct platform_driver	clock_driver = {
	.probe		= clock_probe,
	.remove		= clock_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_CLOCK),
	},
	.id_table = clock_id_table,
};

int __init xocl_init_clock(void)
{
	return platform_driver_register(&clock_driver);
}

void xocl_fini_clock(void)
{
	platform_driver_unregister(&clock_driver);
}
