/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#ifndef _ZOCL_WATCHDOG_H_
#define _ZOCL_WATCHDOG_H_

#include <linux/of.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

/* watchdog subdev driver name */
#define ZOCL_WATCHDOG_NAME "zocl_watchdog"

/*
 * will the cmc process name change?
 * or
 * do we even need to monitor cmc?
 */
#define CMC "xilinx-cmc"

/* check every 3s */
#define ZOCL_WATCHDOG_FREQ (3000)

/*
 * register at ps_reset_controller offset 0xc, upper 16 bits
 * are being used for the watchdog purpose
 * 
 * Among the 16 bits, the upper 8 bits are used as counter and the lower
 * 8 bits are used to show the state of individual part.
 *
 * The counter will be increased by 1 every check and gone back to 1 once
 * overflow. This happens only when each piece we are monitoring is healthy.
 * a 0 in counter means nothing is being monitored, say during ps reboot
 *
 * The pieces we are monitoring so far include,
 * skd,
 * cmc,
 * cq thread
 * sched thread
 *
 * So far when zocl is running in ert mode, we don't support kds. If in future
 * we do support kds, which means, we may not need sched thread and create
 * per cu thread, and since the per cu thread is created when the xclbin is
 * created, the watchdog thread may not know those thread before hand.
 * In that case, we may need to introduce a list linking all threads being
 * monitored -- the link itself can be dynamically changed.
 */
#define WATCHDOG_OFFSET		0xc
#define COUNTER_MASK		0xff000000
#define RESET_MASK		0xffff
#define SKD_BIT_SHIFT		16
#define CMC_BIT_SHIFT		17
#define CQ_THD_BIT_SHIFT	18
#define SCHED_THD_BIT_SHIFT	19
#define COUNTER_BITS_SHIFT	24

extern struct platform_driver zocl_watchdog_driver;
struct zocl_watchdog_dev;

struct watchdog_cfg {
	bool skd_run;
	bool cmc_run;
	bool cq_thread_run;
	bool sched_thread_run;
};

struct zocl_watchdog_ops {
	void (*init)(struct zocl_watchdog_dev *watchdog);
	void (*fini)(struct zocl_watchdog_dev *watchdog);
	void (*config)(struct zocl_watchdog_dev *watchdog,
		       struct watchdog_cfg cfg);
};

struct zocl_watchdog_dev {
	struct platform_device       *pdev;
	void __iomem                 *base;
	const struct zocl_watchdog_ops    *ops;
};
#endif
