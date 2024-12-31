/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2019 Xilinx, Inc. All rights reserved.
 *
 * Authors: Larry Liu <yliu@xilinx.com>
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

#define	MBV_ERR(mbv, fmt, arg...)    \
    xocl_err(&mbv->mbv_pdev->dev, fmt "\n", ##arg)
#define	MBV_INFO(mbv, fmt, arg...)    \
    xocl_info(&mbv->mbv_pdev->dev, fmt "\n", ##arg)

#define	STATUS_EMPTY	(1 << 0)
#define	STATUS_FULL	(1 << 1)
#define	STATUS_STA	(1 << 2)
#define	STATUS_RTA	(1 << 3)

/*
 * Mailbox IP register layout
 */
struct mailbox_reg {
	u32			mbr_wrdata;
	u32			mbr_resv1;
	u32			mbr_rddata;
	u32			mbr_resv2;
	u32			mbr_status;
	u32			mbr_error;
	u32			mbr_sit;
	u32			mbr_rit;
	u32			mbr_is;
	u32			mbr_ie;
	u32			mbr_ip;
	u32			mbr_ctrl;
} __attribute__((packed));

struct mailbox_versal {
	struct platform_device	*mbv_pdev;
	struct mailbox_reg	*mbv_regs;
	u32 			mbv_irq;
	irqreturn_t             (*mbv_irq_handler)(void *arg);
	void 			*mbv_irq_arg;

};

static inline void mailbox_versal_reg_wr(struct mailbox_versal *mbv,
		u32 *reg, u32 val)
{
	iowrite32(val, reg);
}

static inline u32 mailbox_versal_reg_rd(struct mailbox_versal *mbv, u32 *reg)
{
	u32 val = ioread32(reg);

	return val;
}

/* Interrupt context */
static int mailbox_versal_set(struct platform_device *pdev, u32 data)
{
	return 0;
}

/* Interrupt context */
static int mailbox_versal_get(struct platform_device *pdev, u32 *data)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);
	u32 st;

	st = mailbox_versal_reg_rd(mbv, &mbv->mbv_regs->mbr_status);
	if (st & STATUS_EMPTY)
		return -ENOMSG;

	*data = mailbox_versal_reg_rd(mbv, &mbv->mbv_regs->mbr_rddata);

	return 0;
}

static int mailbox_versal_intr_enable(struct platform_device *pdev)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);
	u32 is;

	/* set interrupt threshold for receive, 2^0=1 pkg will trigger intr */
	mailbox_versal_reg_wr(mbv, &mbv->mbv_regs->mbr_rit, 0);
	/* clear pending interrupt */
	is = mailbox_versal_reg_rd(mbv, &mbv->mbv_regs->mbr_is);
	mailbox_versal_reg_wr(mbv, &mbv->mbv_regs->mbr_is, is);

	/* enable receive interrupt */
	mailbox_versal_reg_wr(mbv, &mbv->mbv_regs->mbr_ie, 2);

	/* reset TX/RX channel. */
	mailbox_versal_reg_wr(mbv, &mbv->mbv_regs->mbr_ctrl, 0x3);

	return 0;
}

static int mailbox_versal_intr_disable(struct platform_device *pdev)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);

	/* clear interrupt enable register */
	mailbox_versal_reg_wr(mbv, &mbv->mbv_regs->mbr_ie, 0);
	/* clear interrupt threshold for receive */
	mailbox_versal_reg_wr(mbv, &mbv->mbv_regs->mbr_rit, 0);

	return 0;
}

static int mailbox_versal_handle_intr(struct platform_device *pdev)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);
	u32 is = mailbox_versal_reg_rd(mbv, &mbv->mbv_regs->mbr_is);

	/* Acknowledge all existing is in mailbox */
	while (is) {
		mailbox_versal_reg_wr(mbv, &mbv->mbv_regs->mbr_is, is);
		is = mailbox_versal_reg_rd(mbv, &mbv->mbv_regs->mbr_is);
	}

	return 0;
}

/* Handle all pending callbacks */
static void mailbox_versal_handle_pending(struct mailbox_versal *mbv)
{
	if (mbv && mbv->mbv_irq_handler)
		mbv->mbv_irq_handler(mbv->mbv_irq_arg);
}

static irqreturn_t mailbox_versal_isr(int irq, void *arg)
{
	struct mailbox_versal *mbv = (struct mailbox_versal *)arg;

	mailbox_versal_handle_intr(mbv->mbv_pdev);

	/*
	 * handle pending will call into callback handler
	 * the handler supposed to be faster and non-blocked
	 */
	mailbox_versal_handle_pending(mbv);

	return IRQ_HANDLED;
}

/*
 * probe has two steps:
 *   1) get user_to_ert resources and register interrupt to msix;
 *   2) enable mailbox interrupt
 */
static int mailbox_versal_intr_probe(struct platform_device *pdev)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct resource *res = NULL, dyn_res;
	int ret;

	ret = xocl_subdev_get_resource(xdev, NODE_MAILBOX_USER_TO_ERT,
		IORESOURCE_IRQ, &dyn_res);
	if (ret) {
		MBV_ERR(mbv, "failed to acquire intr resource");
		return -EINVAL;
	}

	res = &dyn_res;
	BUG_ON(!res);

	ret = xocl_user_interrupt_reg(xdev, res->start, mailbox_versal_isr, mbv);
	if (ret) {
		return ret;
	}
	ret = xocl_user_interrupt_config(xdev, res->start, true);
	BUG_ON(ret != 0);

	mbv->mbv_irq = res->start;

	MBV_INFO(mbv, "intr resource: %d", mbv->mbv_irq);
	return mailbox_versal_intr_enable(pdev);
}

static int mailbox_versal_intr_remove(struct platform_device *pdev)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	mailbox_versal_intr_disable(pdev);

	(void) xocl_user_interrupt_config(xdev, mbv->mbv_irq, false);
	(void) xocl_user_interrupt_reg(xdev, mbv->mbv_irq, NULL, mbv);

	mbv->mbv_irq = -1;

	return 0;
}

static int mailbox_versal_request_intr(struct platform_device *pdev,
			irqreturn_t (*handler)(void *arg),
			void *arg)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);

	if (mbv->mbv_irq_handler != NULL) {
		MBV_ERR(mbv, "mbv_irq_handler is alreay requested.");
		return -EINVAL;
	}

	mbv->mbv_irq_handler = handler;
	mbv->mbv_irq_arg = arg;

	return 0;
}

static int mailbox_versal_free_intr(struct platform_device *pdev)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);

	mbv->mbv_irq_handler = NULL;
	mbv->mbv_irq_arg = NULL;

	return 0;
}

static struct xocl_mailbox_versal_funcs mailbox_versal_ops = {
	.set		= mailbox_versal_set,
	.get		= mailbox_versal_get,
	.request_intr   = mailbox_versal_request_intr,
	.free_intr      = mailbox_versal_free_intr,
};

static int mailbox_versal_remove(struct platform_device *pdev)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);

	mailbox_versal_intr_remove(pdev);

	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_release(mbv, NULL);

	return 0;
}

static int mailbox_versal_probe(struct platform_device *pdev)
{
	struct mailbox_versal *mbv = NULL;
	struct resource *res;
	int ret;

	mbv = xocl_drvinst_alloc(&pdev->dev, sizeof(struct mailbox_versal));
	if (!mbv)
		return -ENOMEM;
	platform_set_drvdata(pdev, mbv);
	mbv->mbv_pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	mbv->mbv_regs = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!mbv->mbv_regs) {
		MBV_ERR(mbv, "failed to map in registers");
		ret = -EIO;
		goto failed;
	}

	mailbox_versal_intr_probe(pdev);

	return 0;

failed:
	mailbox_versal_remove(pdev);
	return ret;
}

struct xocl_drv_private mailbox_versal_priv = {
	.ops = &mailbox_versal_ops,
	.dev = -1,
};

struct platform_device_id mailbox_versal_id_table[] = {
	{ XOCL_DEVNAME(XOCL_MAILBOX_VERSAL),
	    (kernel_ulong_t)&mailbox_versal_priv },
	{ },
};

static struct platform_driver	mailbox_versal_driver = {
	.probe		= mailbox_versal_probe,
	.remove		= mailbox_versal_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_MAILBOX_VERSAL),
	},
	.id_table = mailbox_versal_id_table,
};

int __init xocl_init_mailbox_versal(void)
{
	return platform_driver_register(&mailbox_versal_driver);
}

void xocl_fini_mailbox_versal(void)
{
	platform_driver_unregister(&mailbox_versal_driver);
}
