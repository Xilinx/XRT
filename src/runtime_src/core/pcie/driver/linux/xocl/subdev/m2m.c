// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx CU driver for memory to memory BO copy
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 */

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/ioctl.h>

#include "../xocl_drv.h"
#include "../userpf/common.h"
#include "xrt_cu.h"

/* IOCTL interfaces */
#define XOCL_M2M_MAGIC 0x4d324d // "M2M"

#define	M2M_ERR(m2m, fmt, arg...)	\
	xocl_err(&(m2m)->m2m_pdev->dev, fmt "\n", ##arg)
#define	M2M_WARN(m2m, fmt, arg...)	\
	xocl_warn(&(m2m)->m2m_pdev->dev, fmt "\n", ##arg)
#define	M2M_INFO(m2m, fmt, arg...)	\
	xocl_info(&(m2m)->m2m_pdev->dev, fmt "\n", ##arg)
#define	M2M_DBG(m2m, fmt, arg...)	\
	xocl_dbg(&(m2m)->m2m_pdev->dev, fmt "\n", ##arg)

/* This is the real register map for the copy bo CU */
struct start_copybo_cu_cmd {
  uint32_t src_addr_lo;      /* low 32 bit of src addr */
  uint32_t src_addr_hi;      /* high 32 bit of src addr */
  uint32_t src_bo_hdl;       /* src bo handle */
  uint32_t dst_addr_lo;      /* low 32 bit of dst addr */
  uint32_t dst_addr_hi;      /* high 32 bit of dst addr */
  uint32_t dst_bo_hdl;       /* dst bo handle */
  uint32_t size_lo;          /* size of bus width in bytes, low 32 bit */
  uint32_t size_hi;          /* size of bus width in bytes, high 32 bit */
};

struct xocl_m2m {
	struct platform_device 	*m2m_pdev;
	struct xrt_cu 		m2m_cu;
	struct mutex 		m2m_lock;
	struct completion	m2m_irq_complete;
	int 			m2m_polling;
	u32			m2m_intr_base;
	u32			m2m_intr_num;
};

static void  get_host_bank(struct platform_device *pdev, u64 *addr, u64 *size, u8 *used)
{
	struct xocl_dev *xdev = xocl_get_xdev(pdev);
	struct xocl_m2m *m2m = platform_get_drvdata(pdev);
	
	if (!XDEV(xdev)->fdt_blob) {
		/*
		 * This is for aws case where the shell is not raptor, but
		 * xclbin is.
		 * In this case, the host mem info (addr, size) should be
		 * available in memory topology, although they may be not
		 * used.
		 * We have to changed the 'used' to True so that slavebridge
		 * can help program the host mem and m2m can help to copy the
		 * BO  
		 */
		if (!*addr || !*size) {
			M2M_ERR(m2m, "invalid host mem info in mem topology");
			return;
		}
		*used = 1;
	} else {
		int ret = xocl_fdt_get_hostmem(xdev, XDEV(xdev)->fdt_blob, addr, size);
		if (!ret) {
			*used = 1;
			*size = *size >> 10;
		}
	}
}

static int copy_bo(struct platform_device *pdev, uint64_t src_paddr,
	uint64_t dst_paddr, uint32_t src_bo_hdl, uint32_t dst_bo_hdl,
	uint32_t size)
{
	struct xocl_m2m *m2m = platform_get_drvdata(pdev);
	struct xrt_cu *xcu = &m2m->m2m_cu;
	struct start_copybo_cu_cmd cmd;

	M2M_DBG(m2m, "dst %llx, src %llx, size %x", dst_paddr, src_paddr, size);
	/* Note: dst_paddr has been adjusted with offset */
	if ((dst_paddr % KDMA_BLOCK_SIZE) ||
	    (src_paddr % KDMA_BLOCK_SIZE) ||
	    (size % KDMA_BLOCK_SIZE)) {
		M2M_ERR(m2m, "cannot use KDMA. dst: %s, src: %s, size: %s",
		    (dst_paddr % KDMA_BLOCK_SIZE) ? "not 64-byte aligned" : "aligned",
		    (src_paddr % KDMA_BLOCK_SIZE) ? "not 64-byte aligned" : "aligned",
		    (size % KDMA_BLOCK_SIZE) ? "not 64-byte aligned" : "aligned");
		return -EINVAL;
	}

	cmd.src_addr_lo = (uint32_t)src_paddr;
	cmd.src_addr_hi = (src_paddr >> 32) & 0xFFFFFFFF;
	cmd.dst_addr_lo = (uint32_t)dst_paddr;
	cmd.dst_addr_hi = (dst_paddr >> 32) & 0xFFFFFFFF;
	cmd.src_bo_hdl = src_bo_hdl;
	cmd.dst_bo_hdl = dst_bo_hdl;
	cmd.size_lo = size / KDMA_BLOCK_SIZE;
	cmd.size_hi = 0;

	mutex_lock(&m2m->m2m_lock);
	if (!xrt_cu_get_credit(xcu)) {
		M2M_ERR(m2m, "cu is busy");
		mutex_unlock(&m2m->m2m_lock);
		return -EBUSY;
	}

	xrt_cu_config(xcu, (u32 *)&cmd, sizeof(cmd), 0);
	xrt_cu_start(xcu);

	while (true) {
		xrt_cu_check(xcu);
	
		if (xcu->done_cnt || xcu->ready_cnt) {
			xrt_cu_put_credit(xcu, xcu->ready_cnt);
			xcu->ready_cnt = 0;
			xcu->done_cnt = 0;
			break;
		}

		if (m2m->m2m_polling) {
			ndelay(100);
		} else {
			wait_for_completion_interruptible(
			    &m2m->m2m_irq_complete);
		}
	}
	mutex_unlock(&m2m->m2m_lock);

	return 0;
}

/* Interrupt handler for m2m subdev */
static irqreturn_t m2m_irq_handler(int irq, void *arg)
{
	struct xocl_m2m *m2m = (struct xocl_m2m *)arg;

	/* notify pending thread continue */
	if (m2m && !m2m->m2m_polling) {
		/* clear intr for enabling next intr */
		(void) xrt_cu_clear_intr(&m2m->m2m_cu);
		complete(&m2m->m2m_irq_complete);
	} else if (m2m) {
		M2M_INFO(m2m, "unhandled irq %d", irq);
	}

	return IRQ_HANDLED;
}

/* sysfs for m2m subdev */
static ssize_t polling_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct xocl_m2m *m2m = platform_get_drvdata(to_platform_device(dev));
	u32 val = 0;

	if (kstrtou32(buf, 10, &val) == -EINVAL)
		return -EINVAL;

	mutex_lock(&m2m->m2m_lock);
	m2m->m2m_polling = val ? 1 : 0;
	mutex_unlock(&m2m->m2m_lock);

	return count;
}

static ssize_t polling_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct xocl_m2m *m2m = platform_get_drvdata(to_platform_device(dev));
	ssize_t cnt = 0;

	mutex_lock(&m2m->m2m_lock);
	cnt += sprintf(buf + cnt, "%d\n", m2m->m2m_polling);
	mutex_unlock(&m2m->m2m_lock);

	return cnt;
}
static DEVICE_ATTR(polling, 0644, polling_show, polling_store);

static struct attribute *m2m_attrs[] = {
	&dev_attr_polling.attr,
	NULL,
};

static struct attribute_group m2m_attr_group = {
	.attrs = m2m_attrs,
};

static struct xocl_m2m_funcs m2m_ops = {
	.copy_bo = copy_bo,
	.get_host_bank = get_host_bank,
};

struct xocl_drv_private m2m_priv = {
	.ops = &m2m_ops,
};

struct platform_device_id m2m_id_table[] = {
	{ XOCL_DEVNAME(XOCL_M2M), (kernel_ulong_t)&m2m_priv },
	{ },
};

static int __m2m_remove(struct platform_device *pdev)
{
	struct xocl_dev *xdev = xocl_get_xdev(pdev);
	struct xocl_m2m	*m2m;
	int i;

	m2m = platform_get_drvdata(pdev);
	if (!m2m) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}
	
	if (!m2m->m2m_polling)
		xrt_cu_disable_intr(&m2m->m2m_cu, CU_INTR_DONE);

	/* disable intr */
	for (i = 0; i < m2m->m2m_intr_num; i++) {
		xocl_user_interrupt_config(xdev, m2m->m2m_intr_base + i, false);
		xocl_user_interrupt_reg(xdev, m2m->m2m_intr_base + i, NULL, NULL);
	}

	xrt_cu_hls_fini(&m2m->m2m_cu);

	if (m2m->m2m_cu.res)
		vfree(m2m->m2m_cu.res);

	sysfs_remove_group(&pdev->dev.kobj, &m2m_attr_group);
	mutex_destroy(&m2m->m2m_lock);

	platform_set_drvdata(pdev, NULL);

	M2M_INFO(m2m, "successfully removed M2M subdev");
	devm_kfree(&pdev->dev, m2m);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void m2m_remove(struct platform_device *pdev)
{
	__m2m_remove(pdev);
}
#else
#define m2m_remove __m2m_remove
#endif

static int m2m_probe(struct platform_device *pdev)
{
	struct xocl_dev *xdev = xocl_get_xdev(pdev);
	struct xocl_m2m *m2m = NULL;
	struct resource *res;
	int ret = 0, i = 0;

	m2m = devm_kzalloc(&pdev->dev, sizeof(*m2m), GFP_KERNEL);
	if (!m2m)
		return -ENOMEM;

	platform_set_drvdata(pdev, m2m);
	m2m->m2m_pdev = pdev;
	mutex_init(&m2m->m2m_lock);

	/* init m2m cu based on iores of kdma */
	m2m->m2m_cu.dev = XDEV2DEV(xdev);
	m2m->m2m_cu.res = vzalloc(sizeof (struct resource *) * 1);
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		M2M_ERR(m2m, "no IO resource");
		goto failed;
	}
	if (!strncmp(res->name, NODE_KDMA_CTRL, strlen(NODE_KDMA_CTRL))) {
		M2M_INFO(m2m, "CU start 0x%llx\n", res->start);
		m2m->m2m_cu.res[0] = res;
	}
	xrt_cu_hls_init(&m2m->m2m_cu);

	/* init condition veriable */
	init_completion(&m2m->m2m_irq_complete);

	m2m->m2m_polling = 1;
	/* init interrupt vector number based on iores of kdma */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res) {
		m2m->m2m_intr_base = res->start;
		m2m->m2m_intr_num = res->end - res->start + 1;
		m2m->m2m_polling = 0;
	}

	for (i = 0; i < m2m->m2m_intr_num; i++) {
		xocl_user_interrupt_reg(xdev, m2m->m2m_intr_base + i, m2m_irq_handler, m2m);
		xocl_user_interrupt_config(xdev, m2m->m2m_intr_base + i, true);
	}

	ret = sysfs_create_group(&pdev->dev.kobj, &m2m_attr_group);
	if (ret) {
		M2M_ERR(m2m, "create m2m attrs failed: %d", ret);
		goto failed;
	}

	if (m2m->m2m_polling)
		xrt_cu_disable_intr(&m2m->m2m_cu, CU_INTR_DONE);
	else
		xrt_cu_enable_intr(&m2m->m2m_cu, CU_INTR_DONE);

	M2M_INFO(m2m, "Initialized M2M subdev, polling (%d)", m2m->m2m_polling);
	return 0;

failed:
	(void) m2m_remove(pdev);
	return ret;
}

static struct platform_driver	m2m_driver = {
	.probe		= m2m_probe,
	.remove		= m2m_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_M2M),
	},
	.id_table = m2m_id_table,
};

int __init xocl_init_m2m(void)
{
	return platform_driver_register(&m2m_driver);
}

void xocl_fini_m2m(void)
{
	platform_driver_unregister(&m2m_driver);
}
