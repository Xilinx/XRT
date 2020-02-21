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

	struct mutex		mbv_lock;
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

static int mailbox_versal_set(struct platform_device *pdev, u32 data)
{
	return 0;
}

static int mailbox_versal_get(struct platform_device *pdev, u32 *data)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);
	u32 st;

	mutex_lock(&mbv->mbv_lock);

	st = mailbox_versal_reg_rd(mbv, &mbv->mbv_regs->mbr_status);
	if (st & STATUS_EMPTY) {
		mutex_unlock(&mbv->mbv_lock);
		return -ENOMSG;
	}

	*data = mailbox_versal_reg_rd(mbv, &mbv->mbv_regs->mbr_rddata);

	mutex_unlock(&mbv->mbv_lock);

	return 0;
}

static struct xocl_mailbox_versal_funcs mailbox_versal_ops = {
	.set		= mailbox_versal_set,
	.get		= mailbox_versal_get,
};

static int mailbox_versal_remove(struct platform_device *pdev)
{
	struct mailbox_versal *mbv = platform_get_drvdata(pdev);

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

	mutex_init(&mbv->mbv_lock);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	mbv->mbv_regs = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!mbv->mbv_regs) {
		MBV_ERR(mbv, "failed to map in registers");
		ret = -EIO;
		goto failed;
	}

	/* Reset both RX channel and RX channel */
	mailbox_versal_reg_wr(mbv, &mbv->mbv_regs->mbr_ctrl, 0x3);

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
