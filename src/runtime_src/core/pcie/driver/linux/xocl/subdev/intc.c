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
 *
 * If current interrupt mode is ERT.
 * To determine the interrupt sources, read ERT_STATUS_REGISTER_ADDR and check
 * which bit is set.
 *
 * If current interrupt mode is CU.
 * To determin the interrupt source, read ISR and check which bit is set.
 */

#define INTR_NUM  4
#define INTR_SRCS 32
#define MAX_TRY 3

/* ERT Interrupt Status Register offsets */
static u32 eisr[4] = {
	ERT_STATUS_REGISTER_ADDR0 - ERT_STATUS_REGISTER_ADDR,
	ERT_STATUS_REGISTER_ADDR1 - ERT_STATUS_REGISTER_ADDR,
	ERT_STATUS_REGISTER_ADDR2 - ERT_STATUS_REGISTER_ADDR,
	ERT_STATUS_REGISTER_ADDR3 - ERT_STATUS_REGISTER_ADDR
};

/* AXI INTC register layout, based on PG099 */
struct axi_intc {
	u32	isr;
	u32	ipr;
	u32	ier;
	u32	iar;
	u32	sie;
	u32	cie;
	u32	ivr;
	u32	mer;
} __attribute__((packed));
#define reg_addr(base, reg) \
	&(((struct axi_intc *)base)->reg)

static char *res_cu_intc[INTR_NUM] = {
	RESNAME_INTC_CU_00,
	RESNAME_INTC_CU_01,
	RESNAME_INTC_CU_02,
	RESNAME_INTC_CU_03
};

static char *csr_intr_alias[INTR_NUM] = {
	ERT_SCHED_INTR_ALIAS_00,
	ERT_SCHED_INTR_ALIAS_01,
	ERT_SCHED_INTR_ALIAS_02,
	ERT_SCHED_INTR_ALIAS_03
};

struct intr_info {
	irqreturn_t (*handler)(int irq, void *arg);
	int   intr_id;
	void *arg;
	bool  enabled;
};

#define ERT_CSR_TYPE 0
#define AXI_INTC_TYPE 1
/* A metadata contains details of a MSI-X interrupt
 * intr: MSI-x irq number
 * isr:  Interrupt status register
 * info: Information of each interrupt source
 */
struct intr_metadata {
	xdev_handle_t		 xdev;
	int			 intr;
	int			 type;
	u32 __iomem		*isr;
	struct intr_info	*info[INTR_SRCS];
	u32			 enabled_cnt;
	u32			 cnt;
	u32			 blanking;
	u32			 ienabled;
	u32			 disabled_state;
};

/* The details for intc sub-device.
 * It holds all resources and understand hardware.
 */
struct xocl_intc {
	struct platform_device	*pdev;
	u32			 mode;
	/* ERT to host interrupt */
	struct intr_metadata	 ert[INTR_NUM];
	void __iomem		*csr_base;
	/* CU to host interrupt */
	struct intr_metadata	 cu[INTR_NUM];
};

static ssize_t
intc_stat_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_intc *intc = platform_get_drvdata(pdev);
	ssize_t sz = 0;
	int i = 0;

	for (i = 0; i < INTR_NUM; i++) {
		sz += sprintf(buf+sz, "CSR[%d] %d\n", i, intc->ert[i].cnt);
	}

	for (i = 0; i < INTR_NUM; i++) {
		sz += sprintf(buf+sz, "CU INTC[%d] %d\n", i, intc->cu[i].cnt);
	}

	return sz;
}
static DEVICE_ATTR_RO(intc_stat);

static ssize_t
intc_blanking_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_intc *intc = platform_get_drvdata(pdev);
	ssize_t sz = 0;
	int i;

	for (i = 0; i < INTR_NUM; i++) {
		sz += sprintf(buf+sz, "CSR[%d] %d\n", i, intc->ert[i].blanking);
	}

	for (i = 0; i < INTR_NUM; i++) {
		sz += sprintf(buf+sz, "CU INTC[%d] %d\n", i, intc->cu[i].blanking);
	}

	return sz;
}

static ssize_t
intc_blanking_store(struct device *dev, struct device_attribute *attr,
		    const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct xocl_intc *intc = platform_get_drvdata(pdev);
	int blanking;
	int i;

	if (kstrtos32(buf, 10, &blanking) == -EINVAL)
		return -EINVAL;

	if (blanking != 0)
		blanking = 1;

	for (i = 0; i < INTR_NUM; i++) {
		intc->ert[i].blanking = blanking;
		intc->cu[i].blanking = blanking;
	}

	return count;
}
static DEVICE_ATTR(intc_blanking, 0644, intc_blanking_show, intc_blanking_store);

static ssize_t
name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "intc\n");
}
static DEVICE_ATTR_RO(name);

static struct attribute *intc_attrs[] = {
	&dev_attr_intc_stat.attr,
	&dev_attr_intc_blanking.attr,
	&dev_attr_name.attr,
	NULL,
};

static const struct attribute_group intc_attrgroup = {
	.attrs = intc_attrs,
};

static void handle_pending(struct intr_metadata *data, u32 pending)
{
	u32 index;

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
			info->handler(info->intr_id, info->arg);

		pending ^= 1 << index;
	};
}

static inline u32 intc_get_isr(struct intr_metadata *data)
{
	u32 pending;

	pending = (data->type == ERT_CSR_TYPE)?
		ioread32(data->isr) :
		ioread32(reg_addr(data->isr, isr));
	return pending;
}

static void intc_polling(struct intr_metadata *data, int max_try)
{
	u32 pending;

	do {
		pending = intc_get_isr(data);
		handle_pending(data, pending);
		if (data->type == AXI_INTC_TYPE)
			iowrite32(pending, reg_addr(data->isr, iar));

		max_try--;
	} while (max_try > 0);
}

/**
 * intc_isr()
 */
static irqreturn_t intc_isr(int irq, void *arg)
{
	struct intr_metadata *data = arg;
	u32 pending;
	u32 enable_mask;

	data->cnt++;

	if (data->blanking) {
		/* xocl_user_interrupt_config() is thread safe */
		xocl_user_interrupt_config(data->xdev, irq, false);
		intc_polling(data, MAX_TRY);
		xocl_user_interrupt_config(data->xdev, irq, true);

		pending = intc_get_isr(data);
		handle_pending(data, pending);
		if (data->type == AXI_INTC_TYPE)
			iowrite32(pending, reg_addr(data->isr, iar));

		return IRQ_HANDLED;
	}

	/* AXI INTC is configured as high level interrupt input/output
	 * But XDMA IP is rising edge sencitive.
	 * In this case, if input interrupt is still high, write to IAR register
	 * could not clear interrupt (output keep high)
	 * One way is to disable interrupt then clear it.
	 */
	if (data->type == AXI_INTC_TYPE)
		iowrite32(data->ienabled, reg_addr(data->isr, cie));

	pending = intc_get_isr(data);
	handle_pending(data, pending);

	if (data->type == AXI_INTC_TYPE) {
		iowrite32(pending, reg_addr(data->isr, iar));
		/* The handler could disable its interrupt */
		enable_mask = data->ienabled & ~data->disabled_state;
		iowrite32(enable_mask, reg_addr(data->isr, sie));
	}

	return IRQ_HANDLED;
}

static char *intc_mode(struct xocl_intc *intc)
{
	switch (intc->mode) {
	case ERT_INTR:	return "ERT interrupt";
	case CU_INTR:	return "CU interrupt";
	default:	return "unknown";
	}
}

static int request_intr(struct platform_device *pdev, int id,
			irqreturn_t (*handler)(int irq, void *arg),
			void *arg, int mode)
{
	struct xocl_intc *intc = platform_get_drvdata(pdev);
	struct intr_metadata *data;
	struct intr_info *info;
	int data_idx = id / INTR_SRCS;
	int intr_src = id % INTR_SRCS;

	if (data_idx >= INTR_NUM) {
		INTC_ERR(intc, "Interrupt ID out-of-range");
		return -EINVAL;
	}

	if (mode == ERT_INTR)
		data = &intc->ert[data_idx];
	else
		data = &intc->cu[data_idx];

	if (data->info[intr_src] && handler)
		return -EBUSY;

	/* register handler */
	if (handler) {
		info = kzalloc(sizeof(struct intr_info), GFP_KERNEL);
		info->handler = handler;
		info->intr_id = id;
		info->arg = arg;
		info->enabled = false;
		data->info[intr_src] = info;
		return 0;
	}

	/* unregister handler */
	if (data->info[intr_src]) {
		kfree(data->info[intr_src]);
		data->info[intr_src] = NULL;
	}

	return 0;
}

static int config_intr(struct platform_device *pdev, int id, bool en, int mode)
{
	struct xocl_intc *intc = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct intr_metadata *data;
	struct intr_info *info;
	int data_idx = id / INTR_SRCS;
	int intr_src = id % INTR_SRCS;

	if (data_idx >= INTR_NUM) {
		INTC_ERR(intc, "Interrupt ID out-of-range");
		return -EINVAL;
	}

	if (mode == ERT_INTR)
		data = &intc->ert[data_idx];
	else
		data = &intc->cu[data_idx];

	info = data->info[intr_src];

	if (!info)
		return -EINVAL;

	if (info->enabled == en)
		return 0;

	info->enabled = en;
	(en)? data->enabled_cnt++ : data->enabled_cnt--;

	if (mode != intc->mode)
		return 0;

	if (en && data->enabled_cnt == 1)
		xocl_user_interrupt_config(xdev, data->intr, true);
	else if (!en && data->enabled_cnt == 0)
		xocl_user_interrupt_config(xdev, data->intr, false);

	if (intc->mode == ERT_INTR)
		return 0;

	iowrite32(0x3, reg_addr(data->isr, mer));
	/* For CU intc, configure sie/cie register */
	if (en) {
		if (!(data->disabled_state & (1 << intr_src)))
			return 0;
		data->disabled_state &= ~(1 << intr_src);
		iowrite32((1 << intr_src), reg_addr(data->isr, sie));
	} else {
		if (data->disabled_state & (1 << intr_src))
			return 0;
		data->disabled_state |= (1 << intr_src);
		iowrite32((1 << intr_src), reg_addr(data->isr, cie));
	}

	return 0;
}

static int csr_read32(struct platform_device *pdev, u32 off)
{
	struct xocl_intc *intc = platform_get_drvdata(pdev);

	return ioread32(intc->csr_base + off);
}

static void csr_write32(struct platform_device *pdev, u32 val, u32 off)
{
	struct xocl_intc *intc = platform_get_drvdata(pdev);

	iowrite32(val, intc->csr_base + off);
}

static void __iomem *get_csr_base(struct platform_device *pdev)
{
	struct xocl_intc *intc = platform_get_drvdata(pdev);

	if (!intc->csr_base)
		return NULL;
	return intc->csr_base;
}

static int sel_ert_intr(struct platform_device *pdev, int mode)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_intc *intc = platform_get_drvdata(pdev);
	struct intr_metadata *data;
	int i, j;

	if (intc->mode == mode)
		return 0;

	/* Check if all interrupts are disabled in previous mode */
	for (i = 0; i < INTR_NUM; i++) {
		data = (mode == CU_INTR) ? &intc->ert[i] : &intc->cu[i];
		if (data->enabled_cnt)
			return -EBUSY;
		xocl_user_interrupt_reg(xdev, data->intr, NULL, NULL);
	}

	for (i = 0; i < INTR_NUM; i++) {
		data = (mode == CU_INTR) ? &intc->cu[i] : &intc->ert[i];

		xocl_user_interrupt_reg(xdev, data->intr, intc_isr, data);
		xocl_user_interrupt_config(xdev, data->intr, false);

		if (!data->enabled_cnt)
			continue;

		xocl_user_interrupt_config(xdev, data->intr, true);

		if (mode == ERT_INTR)
			continue;

		iowrite32(0x3, reg_addr(data->isr, mer));
		for (j = 0; j < INTR_SRCS; j++) {
			if (!data->info[j] || !data->info[j]->enabled)
				continue;

			iowrite32((1 << j), reg_addr(data->isr, sie));
		}

		data->ienabled = ioread32(reg_addr(data->isr, ier));
	}

	intc->mode = mode;
	INTC_INFO(intc, "Switch to %s interrupt mode",
		  (intc->mode == ERT_INTR)? "ERT" : "CU");
	return 0;
}

static inline int
get_legacy_res(struct platform_device *pdev, struct xocl_intc *intc)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct intr_metadata *data;
	struct resource *res;
	int num_irq;
	int i;

	/* There should be 1 IORESOURCE_MEM and 1 IORESOURCE_IRQ */
	intc->csr_base = xocl_devm_ioremap_res(pdev, 0);
	if (!intc->csr_base) {
		INTC_ERR(intc, "Did not get CSR resource");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		INTC_ERR(intc, "Did not get IRQ resource");
		return -EINVAL;
	}
	/* For all PCIe platform, CU/ERT interrupts are contiguous */
	num_irq = res->end - res->start + 1;
	if (num_irq != INTR_NUM) {
		INTC_ERR(intc, "Got %d irqs", num_irq);
		return -EINVAL;
	}

	for (i = 0; i < INTR_NUM; i++) {
		data = &intc->ert[i];
		data->intr = res->start + i;
		data->isr = intc->csr_base + eisr[i];
		xocl_user_interrupt_reg(xdev, data->intr, intc_isr, data);
		/* disable interrupt */
		xocl_user_interrupt_config(xdev, data->intr, false);
		data->xdev = xdev;
		data->type = ERT_CSR_TYPE;
		data->blanking = 1;
	}

	return 0;
}

/* The ep_ert_sched_00 has 4 irqs. The irq order is related to
 * the 4 status registers.
 * But there is no guarantee that the irq resource ordering is
 * same as the irq ordering in device tree.
 * Use interrupt alias name is safe.
 */
static inline int
intc_get_csr_irq(struct platform_device *pdev, int index)
{
	int i = 0;
	struct resource *r;
	char *res_name = RESNAME_ERT_SCHED;

	r = platform_get_resource(pdev, IORESOURCE_IRQ, i);

	while (r) {
		if (!strncmp(r->name, res_name, strlen(res_name)) &&
		    strnstr(r->name, csr_intr_alias[index], strlen(r->name)))
			return r->start;
		r = platform_get_resource(pdev, IORESOURCE_IRQ, ++i);
	};

	return -ENXIO;
}

static inline int
get_ssv3_res(struct platform_device *pdev, struct xocl_intc *intc)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct intr_metadata *data;
	int i;

	/* Resource for ERT interrupts */
	intc->csr_base = xocl_devm_ioremap_res_byname(pdev, RESNAME_ERT_SCHED);
	if (!intc->csr_base) {
		INTC_ERR(intc, "Did not get CSR resource");
		return -EINVAL;
	}
	for (i = 0; i < INTR_NUM; i++) {
		data = &intc->ert[i];
		data->xdev = xdev;
		data->type = ERT_CSR_TYPE;
		data->intr = intc_get_csr_irq(pdev, i);
		if (data->intr < 0) {
			INTC_ERR(intc, "Did not get IRQ resource");
			return data->intr;
		}
		data->isr = intc->csr_base + eisr[i];
	}

	/* Resource for CU interrupts */
	for (i = 0; i < INTR_NUM; i++) {
		data = &intc->cu[i];
		data->xdev = xdev;
		data->type = AXI_INTC_TYPE;
		data->isr = xocl_devm_ioremap_res_byname(pdev, res_cu_intc[i]);
		if (!data->isr) {
			INTC_ERR(intc, "Did not get CU INTC resource");
			return -EINVAL;
		}
		/* Set MER to allow hardware interrupt, based on PG099 */
		iowrite32(0x3, reg_addr(data->isr, mer));
		/* disable all interrupts */
		iowrite32(0x0, reg_addr(data->isr, ier));

		data->intr = xocl_get_irq_byname(pdev, res_cu_intc[i]);
		if (data->intr < 0) {
			INTC_ERR(intc, "Did not get IRQ resource");
			return data->intr;
		}
		/* ERT/CU interrupt irqs should be the same */
		if (data->intr != intc->ert[i].intr) {
			INTC_ERR(intc, "CU and ERT interrupt mismatch");
			return -EINVAL;
		}
	}

	/* Register interrupt handler */
	for (i = 0; i < INTR_NUM; i++) {
		intc->cu[i].blanking = 1;
		intc->ert[i].blanking = 1;
		if (intc->mode == CU_INTR)
			data = &intc->cu[i];
		else
			data = &intc->ert[i];

		xocl_user_interrupt_reg(xdev, data->intr, intc_isr, data);
		/* disable interrupt */
		xocl_user_interrupt_config(xdev, data->intr, false);
	}

	return 0;
}

static int intc_probe(struct platform_device *pdev)
{
	struct xocl_intc *intc = NULL;
	struct resource *res;
	void *hdl;
	int ret;

	intc = xocl_drvinst_alloc(&pdev->dev, sizeof(*intc));
	if (!intc)
		return -ENOMEM;

	platform_set_drvdata(pdev, intc);
	intc->pdev = pdev;

	/* Use ERT to host interrupt by default */
	intc->mode = ERT_INTR;

	/* For non SSv3 platform, there is only 1 IORESOURCE_MEM */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res)
		ret = get_ssv3_res(pdev, intc);
	else
		ret = get_legacy_res(pdev, intc);
	if (ret)
		goto err;

	INTC_INFO(intc, "Intc initialized, (%s) mode", intc_mode(intc));

	if (sysfs_create_group(&pdev->dev.kobj, &intc_attrgroup))
		INTC_ERR(intc, "Not able to create INTC sysfs group");

	return 0;

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

	for (i = 0; i < INTR_NUM; i++) {
		/* disable interrupt */
		xocl_user_interrupt_config(xdev, intc->ert[i].intr, false);
		xocl_user_interrupt_reg(xdev, intc->ert[i].intr, NULL, NULL);
	}

	(void) sysfs_remove_group(&pdev->dev.kobj, &intc_attrgroup);
	xocl_drvinst_release(intc, &hdl);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);
	return 0;
}

static struct xocl_intc_funcs intc_ops = {
	.request_intr	= request_intr,
	.config_intr	= config_intr,
	.sel_ert_intr	= sel_ert_intr,
	.get_csr_base	= get_csr_base,
	/* Below two ops only used in ERT sub-device polling mode(for debug) */
	.csr_read32	= csr_read32,
	.csr_write32	= csr_write32,
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
