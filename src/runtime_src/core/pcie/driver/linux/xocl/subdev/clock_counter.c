/*
 * Copyright (C) 2020-2021 Xilinx, Inc. All rights reserved.
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

#define	CLOCK_COUNTER_MAX_RES		4
#define	OCL_CLKWIZ_STATUS_MASK		0xffff
#define	OCL_CLKWIZ_STATUS_MEASURE_START	0x1
#define	OCL_CLKWIZ_STATUS_MEASURE_DONE	0x2
#define	OCL_CLK_FREQ_COUNTER_OFFSET	0x8
#define	OCL_CLK_FREQ_V5_COUNTER_OFFSET	0x10
#define	OCL_CLK_FREQ_V5_CLK0_ENABLED	0x10000

#define	CLOCK_C_ERR(clock, fmt, arg...)	\
	xocl_err(&(clock)->cc_pdev->dev, fmt "\n", ##arg)
#define	CLOCK_C_WARN(clock, fmt, arg...)	\
	xocl_warn(&(clock)->cc_pdev->dev, fmt "\n", ##arg)
#define	CLOCK_C_INFO(clock, fmt, arg...)	\
	xocl_info(&(clock)->cc_pdev->dev, fmt "\n", ##arg)
#define	CLOCK_C_DBG(clock, fmt, arg...)	\
	xocl_dbg(&(clock)->cc_pdev->dev, fmt "\n", ##arg)

enum {
	CLOCK_IORES_CLKFREQ_K1 = 0,
	CLOCK_IORES_CLKFREQ_K2,
	CLOCK_IORES_CLKFREQ_K1_K2,
	CLOCK_IORES_CLKFREQ_HBM,
	CLOCK_IORES_MAX,
};

struct xocl_iores_map clock_counter_res_map[] = {
	{ RESNAME_CLKFREQ_K1, CLOCK_IORES_CLKFREQ_K1},
	{ RESNAME_CLKFREQ_K2, CLOCK_IORES_CLKFREQ_K2},
	{ RESNAME_CLKFREQ_K1_K2, CLOCK_IORES_CLKFREQ_K1_K2},
	{ RESNAME_CLKFREQ_HBM, CLOCK_IORES_CLKFREQ_HBM },
};

struct clock_counter {
	struct platform_device  *cc_pdev;
	void __iomem 		*cc_base_address[CLOCK_IORES_MAX]; 
	struct mutex 		cc_lock;
	/* Below are legacy iores fields, keep unchanged until necessary */
	void __iomem		*cc_bases[CLOCK_COUNTER_MAX_RES];
	void __iomem		*cc_freq_counter;
	void __iomem		*cc_freq_counters[CLOCK_COUNTER_MAX_RES];
};

static inline void __iomem *
clock_counter_iores_get_base(struct clock_counter *clock_c, int id)
{
	return clock_c->cc_base_address[id];
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

static u32 clock_counter_get_freq_impl(struct clock_counter *clock_c,
	int idx)
{
	u32 freq = 0, status;
	int times = 10;
	void __iomem *base;
	xdev_handle_t xdev = xocl_get_xdev(clock_c->cc_pdev);

	BUG_ON(idx > CLOCK_COUNTER_MAX_RES);
	BUG_ON(!mutex_is_locked(&clock_c->cc_lock));

	if (clock_c->cc_freq_counter && idx < 2) {
		base = clock_c->cc_freq_counter;
		/* Versal ACAP doesn't support write */
		if (!XOCL_DSA_IS_VERSAL(xdev))
			reg_wr(base, OCL_CLKWIZ_STATUS_MEASURE_START);

		while (times != 0) {
			status = reg_rd(base);
			if ((status & OCL_CLKWIZ_STATUS_MASK) ==
				OCL_CLKWIZ_STATUS_MEASURE_DONE)
				break;
			mdelay(1);
			times--;
		};
		if ((status & OCL_CLKWIZ_STATUS_MASK) ==
			OCL_CLKWIZ_STATUS_MEASURE_DONE)
			freq = reg_rd(base + OCL_CLK_FREQ_COUNTER_OFFSET +
				idx * sizeof(u32));
		return freq;
	}

	if (clock_c->cc_freq_counters[idx]) {
		base = clock_c->cc_freq_counters[idx];
		/* Versal ACAP doesn't support write */
		if (!XOCL_DSA_IS_VERSAL(xdev))
			reg_wr(base, OCL_CLKWIZ_STATUS_MEASURE_START);

		while (times != 0) {
			status = reg_rd(base);
			if ((status & OCL_CLKWIZ_STATUS_MASK) ==
				OCL_CLKWIZ_STATUS_MEASURE_DONE)
				break;
			mdelay(1);
			times--;
		};
		if ((status & OCL_CLKWIZ_STATUS_MASK) ==
			OCL_CLKWIZ_STATUS_MEASURE_DONE) {
			freq = (status & OCL_CLK_FREQ_V5_CLK0_ENABLED) ?
				reg_rd(base + OCL_CLK_FREQ_V5_COUNTER_OFFSET) :
				reg_rd(base + OCL_CLK_FREQ_COUNTER_OFFSET);
		}
	}
	return freq;
}

static int clock_counter_get_freq(struct platform_device *pdev,
	u32 *value, int id)
{
	struct clock_counter *clock_c = platform_get_drvdata(pdev);

	if (id > CLOCK_COUNTER_MAX_RES) {
		CLOCK_C_ERR(clock_c, "id %d cannot be greater than %d",
		    id, CLOCK_COUNTER_MAX_RES);
		return -EINVAL;
	}

	mutex_lock(&clock_c->cc_lock);
	*value = clock_counter_get_freq_impl(clock_c, id);
	mutex_unlock(&clock_c->cc_lock);

	CLOCK_C_INFO(clock_c, "khz: %d", *value);
	return 0;
}

/* there are some iores have not been defined in neither xsabin nor xclbin */
static void clock_counter_prev_refresh_addrs(struct clock_counter *clock_c)
{
	xdev_handle_t xdev = xocl_get_xdev(clock_c->cc_pdev);

	mutex_lock(&clock_c->cc_lock);

	clock_c->cc_freq_counter =
		xocl_iores_get_base(xdev, IORES_CLKFREQ_K1_K2);
	CLOCK_C_INFO(clock_c, "freq_k1_k2 @ %lx",
			(unsigned long)clock_c->cc_freq_counter);

	clock_c->cc_freq_counters[2] =
		xocl_iores_get_base(xdev, IORES_CLKFREQ_HBM);
	CLOCK_C_INFO(clock_c, "freq_hbm @ %lx",
			(unsigned long)clock_c->cc_freq_counters[2]);

	mutex_unlock(&clock_c->cc_lock);

	CLOCK_C_INFO(clock_c, "done.");
}

static void clock_counter_iores_update_base(struct clock_counter *clock_c,
	void __iomem **resource, int id, bool force_update)
{
	char *res_name = xocl_res_id2name(clock_counter_res_map,
	    ARRAY_SIZE(clock_counter_res_map), id);

	if (*resource && !force_update) {
		CLOCK_C_INFO(clock_c, "%s has been set to %lx already.",
		    res_name ? res_name : "", (unsigned long)(*resource));
		return;
	}

	*resource = clock_counter_iores_get_base(clock_c, id);
	CLOCK_C_INFO(clock_c, "%s @ %lx", res_name ? res_name : "",
	    (unsigned long)(*resource));
}

/* when iores has been loaded from xsabin or xclbin */
static int clock_counter_post_refresh_addrs(struct clock_counter *clock_c)
{
	int err = 0;

	mutex_lock(&clock_c->cc_lock);

	clock_counter_iores_update_base(clock_c,
	    &clock_c->cc_freq_counter, CLOCK_IORES_CLKFREQ_K1_K2, false);

	clock_counter_iores_update_base(clock_c,
	    &clock_c->cc_freq_counters[0], CLOCK_IORES_CLKFREQ_K1, true);

	clock_counter_iores_update_base(clock_c,
	    &clock_c->cc_freq_counters[1], CLOCK_IORES_CLKFREQ_K2, true);

	clock_counter_iores_update_base(clock_c,
	    &clock_c->cc_freq_counters[2], CLOCK_IORES_CLKFREQ_HBM, false);

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

	mutex_unlock(&clock_c->cc_lock);

	CLOCK_C_INFO(clock_c, "ret %d", err);
	return err;
}

static ssize_t clock_counter_freqs_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct clock_counter *clock_c = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;
	int i;

	mutex_lock(&clock_c->cc_lock);
	for (i = 0; i < CLOCK_COUNTER_MAX_RES; i++) {
		u32 freq_counter = 0;
		freq_counter = clock_counter_get_freq_impl(clock_c, i);
		cnt += sprintf(buf + cnt, "%d\n", DIV_ROUND_CLOSEST(freq_counter, 1000));
	}

	mutex_unlock(&clock_c->cc_lock);
	return cnt;
}
static DEVICE_ATTR_RO(clock_counter_freqs);

static struct attribute *clock_counter_attrs[] = {
	&dev_attr_clock_counter_freqs.attr,
	NULL,
};

static struct attribute_group clock_counter_attr_group = {
	.attrs = clock_counter_attrs,
};

static struct xocl_clock_counter_funcs clock_counter_ops = {
	.get_freq_counter = clock_counter_get_freq,
};

static int __clock_counter_remove(struct platform_device *pdev)
{
	struct clock_counter *clock_c;

	clock_c = platform_get_drvdata(pdev);
	if (!clock_c) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	sysfs_remove_group(&pdev->dev.kobj, &clock_counter_attr_group);
	mutex_destroy(&clock_c->cc_lock);

	platform_set_drvdata(pdev, NULL);

	CLOCK_C_INFO(clock_c, "successfully removed Clock Counter subdev");
	devm_kfree(&pdev->dev, clock_c);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void clock_counter_remove(struct platform_device *pdev)
{
	__clock_counter_remove(pdev);
}
#else
#define clock_counter_remove __clock_counter_remove
#endif

static int clock_counter_probe(struct platform_device *pdev)
{
	struct clock_counter *clock_c = NULL;
	struct resource *res;
	int ret, i, id;

	clock_c = devm_kzalloc(&pdev->dev, sizeof(*clock_c), GFP_KERNEL);
	if (!clock_c)
		return -ENOMEM;

	platform_set_drvdata(pdev, clock_c);
	clock_c->cc_pdev = pdev;
	mutex_init(&clock_c->cc_lock);

	/*
	 * We hope there is no more exception anymore, some special case for
	 * clock counter ioresources are not defined in either xsabin nor xclbin
	 */
	clock_counter_prev_refresh_addrs(clock_c);

	for (i = 0, res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		res;
		res = platform_get_resource(pdev, IORESOURCE_MEM, ++i)) {
		id = xocl_res_name2id(clock_counter_res_map,
			ARRAY_SIZE(clock_counter_res_map), res->name);
		if (id >= 0) {
			clock_c->cc_base_address[id] =
				ioremap_nocache(res->start,
				res->end - res->start + 1);
			if (!clock_c->cc_base_address[id]) {
				CLOCK_C_ERR(clock_c, "map base %pR failed", res);
				ret = -EINVAL;
				goto failed;
			} else {
				CLOCK_C_INFO(clock_c, "res[%d] %s mapped @ %lx",
				    i, res->name,
				    (unsigned long)clock_c->cc_base_address[id]);
			}
		}
	}
	ret = clock_counter_post_refresh_addrs(clock_c);
	if (ret)
		goto failed;

	ret = sysfs_create_group(&pdev->dev.kobj, &clock_counter_attr_group);
	if (ret) {
		CLOCK_C_ERR(clock_c, "create clock attrs failed: %d", ret);
		goto failed;
	}

	CLOCK_C_INFO(clock_c, "successfully initialized Clock subdev");
	return 0;

failed:
	(void) clock_counter_remove(pdev);
	return ret;
}

struct xocl_drv_private clock_counter_priv = {
	.ops = &clock_counter_ops,
};

struct platform_device_id clock_counter_id_table[] = {
	{ XOCL_DEVNAME(XOCL_CLOCK_COUNTER), (kernel_ulong_t)&clock_counter_priv },
	{ },
};

static struct platform_driver clock_counter_driver = {
	.probe		= clock_counter_probe,
	.remove		= clock_counter_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_CLOCK_COUNTER),
	},
	.id_table = clock_counter_id_table,
};

int __init xocl_init_clock_counter(void)
{
	return platform_driver_register(&clock_counter_driver);
}

void xocl_fini_clock_counter(void)
{
	platform_driver_unregister(&clock_counter_driver);
}
