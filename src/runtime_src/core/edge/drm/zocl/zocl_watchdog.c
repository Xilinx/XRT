/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */
#include "zocl_drv.h"
#include "zocl_watchdog.h"

#define watchdog_err(pdev, fmt, args...)  \
	zocl_err(&pdev->dev, fmt"\n", ##args)
#define watchdog_info(pdev, fmt, args...)  \
	zocl_info(&pdev->dev, fmt"\n", ##args)
#define watchdog_dbg(pdev, fmt, args...)  \
	zocl_dbg(&pdev->dev, fmt"\n", ##args)

/*
 * do nothing so far
 */ 
static void watchdog_init(struct zocl_watchdog_dev *watchdog)
{
}

/*
 * reset counter part to 0.
 * 0 has special meaning -- nothing is monitored
 */ 
static void watchdog_fini(struct zocl_watchdog_dev *watchdog)
{
	u32 val;
	val = ioread32(watchdog->base + WATCHDOG_OFFSET);
	val &= RESET_MASK;
	iowrite32(val, watchdog->base + WATCHDOG_OFFSET);
}

/*
 * if everyting is running, increase the counter by 1, set to 1 if overflow 
 * otherwise, just set the state bit for each individual being monitored
 */ 
static void watchdog_config(struct zocl_watchdog_dev *watchdog,
	struct watchdog_cfg cfg)
{
	u32 val;
	u8 counter;
	val = ioread32(watchdog->base + WATCHDOG_OFFSET);
	counter = (val & COUNTER_MASK) >> COUNTER_BITS_SHIFT;

	/*
	 * counter 0 has special meaning. So if just started,
	 * set counter as 1
	 */
	if (!counter)
		counter++;

	/* only update counter when everything is running */
	if (cfg.skd_run && cfg.cmc_run && cfg.cq_thread_run &&
		cfg.sched_thread_run) {
		if (counter == (COUNTER_MASK >> COUNTER_BITS_SHIFT))
			counter = 1;
		else
			counter++;
	}
	val = (counter << COUNTER_BITS_SHIFT) +
	       (val & RESET_MASK) +
	       (cfg.skd_run ? 1 << SKD_BIT_SHIFT : 0) +
	       (cfg.cmc_run ? 1 << CMC_BIT_SHIFT : 0) +
	       (cfg.cq_thread_run ? 1 << CQ_THD_BIT_SHIFT : 0) +
	       (cfg.sched_thread_run ? 1 << SCHED_THD_BIT_SHIFT : 0);
	iowrite32(val, watchdog->base + WATCHDOG_OFFSET);
}

static struct zocl_watchdog_ops watchdog_ops = {
	.init         = watchdog_init,
	.fini         = watchdog_fini,
	.config       = watchdog_config,
};

static const struct of_device_id zocl_watchdog_of_match[] = {
	{ .compatible = "xlnx,reset_ps",
	  .data = &watchdog_ops,
	},
	{ /* end of table */ },
};

MODULE_DEVICE_TABLE(of, zocl_watchdog_of_match);

static int zocl_watchdog_probe(struct platform_device *pdev)
{
	struct zocl_watchdog_dev *watchdog;
	const struct of_device_id *id;
	struct resource *res;
	const struct zocl_watchdog_ops *ops;
	void __iomem *map;

	id = of_match_node(zocl_watchdog_of_match, pdev->dev.of_node);
	watchdog_info(pdev, "Probing for %s", id->compatible);

	watchdog = devm_kzalloc(&pdev->dev, sizeof(*watchdog), GFP_KERNEL);
	watchdog->pdev = pdev;

	ops = of_device_get_match_data(&pdev->dev);
	watchdog->ops = ops;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	map = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(map)) {
		watchdog_err(pdev, "Failed to map reset controller HW registers:"
			" %0lx", PTR_ERR(map));
		return PTR_ERR(map);
	}
	watchdog->base = map;
	watchdog_info(pdev, "IP(reset controller) IO start %lx, end %lx",
	      (unsigned long)res->start, (unsigned long)res->end);

	platform_set_drvdata(pdev, watchdog);
	return 0;
}

static int zocl_watchdog_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver zocl_watchdog_driver = {
	.driver = {
		.name = ZOCL_WATCHDOG_NAME,
		.of_match_table = zocl_watchdog_of_match,
	},
	.probe  = zocl_watchdog_probe,
	.remove = zocl_watchdog_remove,
};
