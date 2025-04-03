/*
 * Processor System manager for Alveo board.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors: Min.Ma@xilinx.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include "../xocl_drv.h"
#include "mgmt-ioctl.h"

#define MAX_RETRY       50
#define RETRY_INTERVAL  100       //ms

#define MAX_WAIT        12
#define WAIT_INTERVAL   5000	//ms

/**
 * ps reset and por are controlled by reg offset 0
 * bit 31: reset controller enable bit, 1 is active
 * bit 3-2: ps reset issue bits
 * bit 1-0: por issue bits.
 * For both types, bit 31 needs to be set.
 * And for qor, just setting the 2 bits does nothing, the controller will
 * wait for the signal trigger by pcie reset. So for xrt, the sequence for
 * qor is,
 * 1. set bit 31, and bit1-0,
 * 2. set pcie reset bit.
 */
#define RESET_REG_0     0x0
#define RESET_ENABLE    0x80000000
#define PS_RESET        0xc
#define POR_RESET       0x3

/*
 * register at offset 0xc, upper 16 bits are being used for the watchdog
 * purpose, lower 16 bits are being used to show reset status
 *
 * Among the 16 bits for watchdog, the upper 8 bits are used as counter and
 * the lower 8 bits are used to show the state of individual part.
 *
 * The counter will be increased by 1 every check and gone back to 0 once
 * overflow. This happens only when each piece we are monitoring is healthy.
 *
 * The pieces we are monitoring so far include,
 * skd,
 * cmc,
 * cq thread
 * sched thread
 *
 */
#define RESET_REG_C	0xC
/* watchdog freq should be same to that defined in zocl_watchdog.h */
#define ZOCL_WATCHDOG_FREQ (3000)

#define ERT_READY_MASK  0x8
#define RES_DONE_MASK   0x4
#define RES_TYPE_MASK   0x3
#define COUNTER_MASK		0xff000000
#define RESET_MASK		0xffff
#define SKD_BIT_SHIFT		16
#define CMC_BIT_SHIFT		17
#define CQ_THD_BIT_SHIFT	18
#define SCHED_THD_BIT_SHIFT	19
#define COUNTER_BITS_SHIFT	24

#define SK_RESET	0x1

#define READ_REG32(ps, off)	\
	XOCL_READ_REG32(ps->base_addr + off)
#define WRITE_REG32(ps, val, off)       \
	XOCL_WRITE_REG32(val, ps->base_addr + off)

struct xocl_ps {
	struct platform_device	*pdev;
	void __iomem		*base_addr;
	struct mutex		 ps_lock;
	bool			sysfs_created;
};

/*
 * Processor system reset supports 3 type of reset
 * Host set reset type in the scratchpad register then wait PS
 * set reset done bit.
 */
static void ps_reset(struct platform_device *pdev, int type)
{
	struct xocl_ps *ps;
	u32 reg;
	int retry = 0;

	xocl_info(&pdev->dev, "Reset Processor System...");
	ps = platform_get_drvdata(pdev);
	if (!ps)
		return;

	mutex_lock(&ps->ps_lock);
	/* Set reset type in scratchpad register */
	switch(type) {
	case 1:
		xocl_info(&pdev->dev, "Soft Kernel reset...");
		reg = READ_REG32(ps, RESET_REG_C);
		reg = (reg & ~RES_TYPE_MASK) | SK_RESET;
		WRITE_REG32(ps, reg, RESET_REG_C);
		break;
	case 2:
		xocl_info(&pdev->dev, "PS reset...");
		reg = READ_REG32(ps, RESET_REG_0);
		reg |= (RESET_ENABLE | PS_RESET);
		WRITE_REG32(ps, reg, RESET_REG_0);
		/* clear ERT ready bits */
		reg = READ_REG32(ps, RESET_REG_C);
		reg &= (~ERT_READY_MASK);
		WRITE_REG32(ps, reg, RESET_REG_C);
		goto done;
	case 3:
		xocl_info(&pdev->dev, "POR reset...");
		/*
		 * don't set POR bits here since firewall may have been tripped
		 * and the registers are not accessible here
		 */
		goto done;
	default:
		xocl_info(&pdev->dev, "Unknown reset type");
	}

	do {
		reg = READ_REG32(ps, RESET_REG_C);
		msleep(RETRY_INTERVAL);
	} while (retry++ < MAX_RETRY && !(reg & RES_DONE_MASK));

	if (retry >= MAX_RETRY) {
		xocl_err(&pdev->dev, "Reset time out");
		mutex_unlock(&ps->ps_lock);
		return;
	}

	/* Clear reset done bit */
	reg &= ~RES_DONE_MASK;
	WRITE_REG32(ps, reg, RESET_REG_C);
done:
	mutex_unlock(&ps->ps_lock);
}

/* Wait Processor system goes into ready status */
static int ps_wait(struct platform_device *pdev)
{
	struct xocl_ps *ps;
	u32 reg;
	int retry = 0;
	int ret = 0;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	xocl_info(&pdev->dev, "Wait Processor System ready...");
	ps = platform_get_drvdata(pdev);
	if (!ps)
		return -ENODEV;

	mutex_lock(&ps->ps_lock);
	reg = READ_REG32(ps, RESET_REG_C);
	while(!(reg & ERT_READY_MASK) && retry++ < MAX_WAIT) {
		reg = READ_REG32(ps, RESET_REG_C);
		msleep(WAIT_INTERVAL);
	}

	if (retry >= MAX_WAIT) {
		xocl_err(&pdev->dev, "PS wait time out");
		ret = -ETIME;
	} else {
		xocl_info(&pdev->dev, "Processor System ready in %d retries",
			retry);
	}

	/* set POR bits again after reset */
	if (xocl_subdev_is_vsec(xdev)) {
		reg = READ_REG32(ps, RESET_REG_0);
		reg |= (RESET_ENABLE | POR_RESET);
		WRITE_REG32(ps, reg, RESET_REG_0);
	}

	mutex_unlock(&ps->ps_lock);
	return ret;
}

static void ps_check_healthy(struct platform_device *pdev)
{
	struct xocl_ps *ps;
	u32 reg0, reg;

	ps = platform_get_drvdata(pdev);
	if (!ps)
		return;

	mutex_lock(&ps->ps_lock);
	reg0 = READ_REG32(ps, RESET_REG_C);
	msleep_interruptible(ZOCL_WATCHDOG_FREQ);
	reg = READ_REG32(ps, RESET_REG_C);
	mutex_unlock(&ps->ps_lock);

	/* healthy */
	if ((reg & COUNTER_MASK) &&
		(reg0 & COUNTER_MASK) != (reg & COUNTER_MASK))
		return;

	/* not healthy */
	if (!(reg & COUNTER_MASK)) {
		xocl_warn(&pdev->dev, "ps: zocl is not loaded");
		return;
	}

	if (!(reg & (1 << SKD_BIT_SHIFT)))
		xocl_warn(&pdev->dev, "ps: skd is not running");

	if (!(reg & (1 << CMC_BIT_SHIFT)))
		xocl_warn(&pdev->dev, "ps: cmc is not running");

	if (!(reg & (1 << CQ_THD_BIT_SHIFT)))
		xocl_warn(&pdev->dev, "ps: cq thread is not running");

	if (!(reg & (1 << SCHED_THD_BIT_SHIFT)))
		xocl_warn(&pdev->dev, "ps: sched thread is not running");
}

static struct xocl_ps_funcs ps_ops = {
	.reset		= ps_reset,
	.wait		= ps_wait,
	.check_healthy	= ps_check_healthy,
};

static ssize_t ps_ready_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_ps *ps = platform_get_drvdata(to_platform_device(dev));
	ssize_t count = 0;
	u32 reg;

	mutex_lock(&ps->ps_lock);
	reg = READ_REG32(ps, RESET_REG_C);
	mutex_unlock(&ps->ps_lock);

	if (reg & ERT_READY_MASK)
		count = sprintf(buf, "1\n");
	else
		count = sprintf(buf, "0\n");

	return count;
}
static DEVICE_ATTR_RO(ps_ready);

static ssize_t ps_watchdog_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_ps *ps = platform_get_drvdata(to_platform_device(dev));
	ssize_t count = 0;
	u32 reg0, reg;
	bool alive = false;

	mutex_lock(&ps->ps_lock);
	reg0 = READ_REG32(ps, RESET_REG_C);
	msleep(ZOCL_WATCHDOG_FREQ);
	reg = READ_REG32(ps, RESET_REG_C);
	mutex_unlock(&ps->ps_lock);

	if ((reg & COUNTER_MASK) &&
		(reg0 & COUNTER_MASK) != (reg & COUNTER_MASK))
		alive = true;
	if (alive)
		count = sprintf(buf, "ps healthy: 1\n");
	else
		count = sprintf(buf, "ps healthy: 0\n");

	/*
	 * counter 0 means the watchdong thread exits. eg. ps reboot,
	 * zocl unload, etc. In this case, don't show other info
	 */
	if (!(reg & COUNTER_MASK))
		return count;

	if (reg & (1 << SKD_BIT_SHIFT))
		count += sprintf(buf + count, "skd: running\n");
	else
		count += sprintf(buf + count, "skd: not running\n");

	if (reg & (1 << CMC_BIT_SHIFT))
		count += sprintf(buf + count, "cmc: running\n");
	else
		count += sprintf(buf + count, "cmc: not running\n");

	if (reg & (1 << CQ_THD_BIT_SHIFT))
		count += sprintf(buf + count, "cq thread: running\n");
	else
		count += sprintf(buf + count, "cq thread: not running\n");

	if (reg & (1 << SCHED_THD_BIT_SHIFT))
		count += sprintf(buf + count, "sched thread: running\n");
	else
		count += sprintf(buf + count, "sched thread: not running\n");

	return count;
}
static DEVICE_ATTR_RO(ps_watchdog);

static struct attribute *ps_attrs[] = {
	&dev_attr_ps_ready.attr,
	&dev_attr_ps_watchdog.attr,
	NULL,
};

static struct attribute_group ps_attr_group = {
	.attrs = ps_attrs,
};

static void ps_sysfs_destroy(struct xocl_ps *ps)
{
	if (!ps->sysfs_created)
		return;

	sysfs_remove_group(&ps->pdev->dev.kobj, &ps_attr_group);
	ps->sysfs_created = false;
}

static int ps_sysfs_create(struct xocl_ps *ps)
{
	int ret;

	if (ps->sysfs_created)
		return 0;

	ret = sysfs_create_group(&ps->pdev->dev.kobj, &ps_attr_group);
	if (ret) {
		xocl_err(&ps->pdev->dev, "create ps attrs failed: 0x%x", ret);
		return ret;
	}
	ps->sysfs_created = true;

	return 0;
}

static int __ps_remove(struct platform_device *pdev)
{
	struct xocl_ps *ps;

	ps = platform_get_drvdata(pdev);
	if (!ps)
		return -EINVAL;

	ps_sysfs_destroy(ps);
	if (ps->base_addr)
		iounmap(ps->base_addr);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, ps);

	mutex_destroy(&ps->ps_lock);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void ps_remove(struct platform_device *pdev)
{
	__ps_remove(pdev);
}
#else
#define ps_remove __ps_remove
#endif

struct xocl_drv_private ps_priv = {
	.ops = &ps_ops,
};

static int ps_probe(struct platform_device *pdev)
{
	struct xocl_ps *ps;
	struct resource *res;
	int ret = 0;
	u32 reg;
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	ps = devm_kzalloc(&pdev->dev, sizeof(*ps), GFP_KERNEL);
	if (!ps) {
		xocl_err(&pdev->dev, "out of memory");
		return -ENOMEM;
	}

	ps->pdev = pdev;
	platform_set_drvdata(pdev, ps);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		  res->start, res->end);
	ps->base_addr = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!ps->base_addr) {
		ret = -EIO;
		xocl_err(&pdev->dev, "Map iomem failed");
		goto failed;
	}

	/* set POR bits during probe */
	if (xocl_subdev_is_vsec(xdev)) {
		reg = READ_REG32(ps, RESET_REG_0);
		reg |= (RESET_ENABLE | POR_RESET);
		WRITE_REG32(ps, reg, RESET_REG_0);
	}

	mutex_init(&ps->ps_lock);

	ret = ps_sysfs_create(ps);
	if (ret)
		goto failed;

	return 0;
failed:
	ps_remove(pdev);
	return ret;
};

struct platform_device_id ps_id_table[] = {
	{ XOCL_DEVNAME(XOCL_PS), (kernel_ulong_t)&ps_priv },
	{ },
};

static struct platform_driver ps_driver = {
	.probe		= ps_probe,
	.remove		= ps_remove,
	.driver		= {
		.name = "xocl_ps",
	},
	.id_table = ps_id_table,
};

int __init xocl_init_ps(void)
{
	return platform_driver_register(&ps_driver);
}

void xocl_fini_ps(void)
{
	platform_driver_unregister(&ps_driver);
}
