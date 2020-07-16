/*
 * Copyright (C) 2020 Xilinx, Inc. All rights reserved.
 *
 * Authors: Min Ma <minm@xilinx.com>
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
/* To get register address of CSR */
#include "ert.h"

#define INTC_INFO(intc, fmt, arg...)	\
	xocl_info(&intc->pdev->dev, fmt "\n", ##arg)
#define INTC_ERR(intc, fmt, arg...)	\
	xocl_err(&intc->pdev->dev, fmt "\n", ##arg)
#define INTC_DBG(intc, fmt, arg...)	\
	xocl_dbg(&intc->pdev->dev, fmt "\n", ##arg)

/* The purpose of this driver is to manage the CU/ERT interrupts on an Alveo
 * board. It provides:
 *	interrupt register function for CU and ERT sub-devices.
 *	interrupt to ERT function
 *
 * Based on current hardware implementation, the driver supports 4 PCIe MSI-x
 * interrupts. For each interrupt, there are 32 different sources.
 * If current interrupt mode is ERT.
 * To determine the interrupt sources, read ERT_STATUS_REGISTER_ADDR and check
 * which bit is set.
 *
 * If current interrupt mode is CU.
 * (TBD)...
 */

extern int kds_mode;

#define INTR_NUM  4
#define INTR_SRCS 32

/* ERT Interrupt Status Register offsets */
static u32 eisr[4] = {
	ERT_STATUS_REGISTER_ADDR0 - ERT_STATUS_REGISTER_ADDR,
	ERT_STATUS_REGISTER_ADDR1 - ERT_STATUS_REGISTER_ADDR,
	ERT_STATUS_REGISTER_ADDR2 - ERT_STATUS_REGISTER_ADDR,
	ERT_STATUS_REGISTER_ADDR3 - ERT_STATUS_REGISTER_ADDR
};

struct intr_info {
	irqreturn_t (*handler)(int irq, void *arg);
	void *arg;
	bool  enabled;
};

/* A metadata contains details of a MSI-X interrupt
 * intr: MSI-x irq number
 * isr:  Interrupt status register
 * info: Information of each interrupt source
 */
struct intr_metadata {
	u32			 intr;
	u32 __iomem		*isr;
	struct intr_info	*info[INTR_SRCS];
};

/* The details for intc sub-device.
 * It holds all resources and understand hardware.
 */
struct xocl_intc {
	struct platform_device	*pdev;
	struct intr_metadata	 data[INTR_NUM];
	u32			 data_num;
	void __iomem		*csr_base;
	u32			 csr_size;
	/* TODO: support CU to host interrupt after we got hardware spec */
};

/**
 * intc_csr_isr() - Handler of ERT to host interrupts
 */
irqreturn_t intc_csr_isr(int irq, void *arg)
{
	struct intr_metadata *data = arg;
	u32 pending;
	u32 index;

	/* EISR is clear on read */
	pending = ioread32(data->isr);

	/* Iterate all interrupt sources. If it is set, handle it */
	for (index = 0; pending != 0; index++) {
		struct intr_info *info;

		if (!(pending & (1 << index)))
			continue;

		info = data->info[index];
		/* If a bit is set but without handler, it probably is
		 * a bug on hardware or ERT firmware.
		 */
		if (info && info->enabled && info->handler)
			info->handler(index, info->arg);

		pending ^= 1 << index;
	};

	return IRQ_HANDLED;
}

static int request_intr(struct platform_device *pdev, int intr_id,
			irqreturn_t (*handler)(int irq, void *arg),
			void *arg)
{
	struct xocl_intc *intc = platform_get_drvdata(pdev);
	struct intr_metadata *data;
	struct intr_info *info;
	int data_idx = intr_id / INTR_SRCS;
	int intr_src = intr_id % INTR_SRCS;

	if (data_idx >= intc->data_num) {
		INTC_ERR(intc, "Interrupt ID out-of-range");
		return -EINVAL;
	}

	data = &intc->data[data_idx];

	if (data->info[intr_src] && handler)
		return -EBUSY;

	if (handler) {
		info = vzalloc(sizeof(struct intr_info));
		info->handler = handler;
		info->arg = arg;
		info->enabled = false;
		data->info[intr_src] = info;
		return 0;
	}

	if (data->info[intr_src]) {
		vfree(data->info[intr_src]);
		data->info[intr_src] = NULL;
	}

	return 0;
}

static int config_intr(struct platform_device *pdev, int intr_id, bool en)
{
	struct xocl_intc *intc = platform_get_drvdata(pdev);
	struct intr_metadata *data;
	struct intr_info *info;
	int data_idx = intr_id / INTR_SRCS;
	int intr_src = intr_id % INTR_SRCS;

	if (data_idx >= intc->data_num) {
		INTC_ERR(intc, "Interrupt ID out-of-range");
		return -EINVAL;
	}

	data = &intc->data[data_idx];
	info = data->info[intr_src];

	if (!info)
		return -EINVAL;

	info->enabled = en;

	return 0;
}

static int intc_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_intc *intc = NULL;
	struct resource *res;
	struct intr_metadata *data;
	void *hdl;
	u32 irq;
	int ret;
	int i;

	if (!kds_mode)
		goto out;

	intc = xocl_drvinst_alloc(&pdev->dev, sizeof(*intc));
	if (!intc)
		return -ENOMEM;

	platform_set_drvdata(pdev, intc);
	intc->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		INTC_ERR(intc, "Did not get CSR resource");
		ret = -EINVAL;
		goto err;
	}
	intc->csr_size = res->end - res->start + 1;
	intc->csr_base = ioremap_nocache(res->start, intc->csr_size);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		INTC_ERR(intc, "Did not get IRQ resource");
		ret = -EINVAL;
		goto err1;
	}
	/* For all PCIe platform, CU/ERT interrupts are contiguous */
	intc->data_num = res->end - res->start + 1;
	if (intc->data_num > INTR_NUM) {
		INTC_ERR(intc, "Too many interrupts in the resource");
		return -EINVAL;
	}

	for (i = 0; i < intc->data_num; i++) {
		irq = res->start + i;
		data = &intc->data[i];
		data->intr = irq;
		data->isr = intc->csr_base + eisr[i];
		xocl_user_interrupt_reg(xdev, irq, intc_csr_isr, data);
		/* enable interrupt */
		xocl_user_interrupt_config(xdev, irq, true);
	}
out:
	return 0;

err1:
	iounmap(intc->csr_base);
err:
	xocl_drvinst_release(intc, &hdl);
	xocl_drvinst_free(hdl);
	return ret;
}

static int intc_remove(struct platform_device *pdev)
{
	struct xocl_intc *intc = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	void *hdl;
	int i;

	if (!kds_mode)
		goto out;

	for (i = 0; i < intc->data_num; i++) {
		/* disable interrupt */
		xocl_user_interrupt_config(xdev, intc->data[i].intr, false);
		xocl_user_interrupt_reg(xdev, intc->data[i].intr, NULL, NULL);
	}

	xocl_drvinst_release(intc, &hdl);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);
out:
	return 0;
}

static struct xocl_intc_funcs intc_ops = {
	.request_intr = request_intr,
	.config_intr  = config_intr,
	/* TODO: add CU/ERT mode switch op */
};

struct xocl_drv_private intc_priv = {
	.ops = &intc_ops,
};

struct platform_device_id intc_id_table[] = {
	{ XOCL_DEVNAME(XOCL_INTC), (kernel_ulong_t)&intc_priv },
	{ },
};

static struct platform_driver intc_driver = {
	.probe		= intc_probe,
	.remove		= intc_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_INTC),
	},
	.id_table = intc_id_table,
};

int __init xocl_init_intc(void)
{
	return platform_driver_register(&intc_driver);
}

void xocl_fini_intc(void)
{
	platform_driver_unregister(&intc_driver);
}
