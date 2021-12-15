/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
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
#include "kds_command.h"
#include "xgq_cmd_ert.h"
#include "../xgq_xocl_plat.h"
#include "xocl_xgq.h"

#define	EC_ERR(ec, fmt, arg...)	\
	xocl_err(&(ec)->ec_pdev->dev, fmt "\n", ##arg)
#define	EC_WARN(ec, fmt, arg...)	\
	xocl_warn(&(ec)->ec_pdev->dev, fmt "\n", ##arg)
#define	EC_INFO(ec, fmt, arg...)	\
	xocl_info(&(ec)->ec_pdev->dev, fmt "\n", ##arg)
#define	EC_DBG(ec, fmt, arg...)	\
	xocl_dbg(&(ec)->ec_pdev->dev, fmt "\n", ##arg)

/* The first word of the CQ is control XGQ version.
 * It determines how to find SQ tail and CQ tail pointers.
 */
#define ERT_CTRL_VER_OFFSET	0x0

/* Control XGQ version 1.0 macros */
#define ERT_CTRL_XGQ_VER1	0x00010000
#define ERT_CTRL_SQ_TAIL_OFF	0x4
#define ERT_CTRL_CQ_TAIL_OFF	0x8

#define ERT_CTRL_CMD_TIMEOUT	msecs_to_jiffies(2 * 1000)

#define ERT_CTRL_ADD_NUM_ERT_XGQ	4

#define CQ_STATUS_ADDR 0x58

static uint16_t	g_ctrl_xgq_cid;

struct ert_ctrl {
	struct kds_ert		 ec_ert;
	struct platform_device	*ec_pdev;
	void __iomem		*ec_cq_base;
	uint32_t		 ec_cq_range;

	uint32_t		 ec_version;
	uint32_t		 ec_connected;
	struct xgq		 ec_ctrl_xgq;
	struct mutex		 ec_xgq_lock;

	/* ERT XGQ instances for CU */
	void		       **ec_exgq;
	uint32_t		 ec_exgq_capacity;
};

static ssize_t
status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ert_ctrl *ec = dev_get_drvdata(dev);
	ssize_t sz = 0;

	sz += scnprintf(buf+sz, PAGE_SIZE - sz,
		       "Version: 0x%x\n", ec->ec_version);
	sz += scnprintf(buf+sz, PAGE_SIZE - sz,
		       "Connected: %d\n", ec->ec_connected);

	return sz;
};
static DEVICE_ATTR_RO(status);

static struct attribute *ert_ctrl_attrs[] = {
	&dev_attr_status.attr,
	NULL,
};

static const struct attribute_group ert_ctrl_attrgroup = {
	.attrs = ert_ctrl_attrs,
};

static inline uint32_t ert_ctrl_read32(void __iomem *addr)
{
	return ioread32(addr);
}

static inline void ert_ctrl_write32(uint32_t val, void __iomem *addr)
{
	return iowrite32(val, addr);
}

static void ert_ctrl_submit(struct kds_ert *ert, struct kds_command *xcmd)
{
	struct ert_ctrl *ec = container_of(ert, struct ert_ctrl, ec_ert);
	struct xgq_cmd_sq_hdr *sq_hdr;
	u64 timeout = 0;
	u64 slot_addr = 0;
	bool is_timeout = false;
	int ret = 0;

	mutex_lock(&ec->ec_xgq_lock);

	ret = xgq_produce(&ec->ec_ctrl_xgq, &slot_addr);
	if (ret) {
		EC_ERR(ec, "XGQ produce failed: %d", ret);
		xcmd->status = KDS_ERROR;
		xcmd->cb.notify_host(xcmd, xcmd->status);
		xcmd->cb.free(xcmd);
		mutex_unlock(&ec->ec_xgq_lock);
		return;
	}

	sq_hdr = xcmd->info;
	sq_hdr->cid = g_ctrl_xgq_cid++;
	memcpy_toio((void __iomem *)slot_addr, xcmd->info, xcmd->isize);
	xgq_notify_peer_produced(&ec->ec_ctrl_xgq);

	timeout = jiffies + ERT_CTRL_CMD_TIMEOUT;
	while (!is_timeout) {
		msleep(100);

		ret = xgq_consume(&ec->ec_ctrl_xgq, &slot_addr);
		if (!ret) {
			memcpy_fromio(xcmd->response, (void __iomem *)slot_addr, xcmd->response_size);
			xgq_notify_peer_consumed(&ec->ec_ctrl_xgq);
			break;
		}

		is_timeout = (timeout < jiffies) ? true : false;
	}
	mutex_unlock(&ec->ec_xgq_lock);

	if (is_timeout) {
		xcmd->status = KDS_TIMEOUT;
		xcmd->cb.notify_host(xcmd, xcmd->status);
	} else {
		xcmd->status = KDS_COMPLETED;
		xcmd->cb.notify_host(xcmd, xcmd->status);
	}
	xcmd->cb.free(xcmd);
}

static bool ert_ctrl_abort_sync(struct kds_ert *ert, struct kds_client *client, int cu_idx)
{
	return true;
}

static inline int ert_ctrl_alloc_ert_xgq(struct ert_ctrl *ec, int num)
{
	void *tmp;

	if (num <= ec->ec_exgq_capacity)
		return 0;

	tmp = kzalloc(sizeof(void *) * num, GFP_KERNEL);
	if (!tmp)
		return -ENOMEM;

	if (!ec->ec_exgq_capacity) {
		ec->ec_exgq = tmp;
		ec->ec_exgq_capacity = num;
		return 0;
	}

	memcpy(tmp, ec->ec_exgq, sizeof(void *) * ec->ec_exgq_capacity);
	kfree(ec->ec_exgq);
	ec->ec_exgq = tmp;
	ec->ec_exgq_capacity = num;

	return 0;
}

static int ert_ctrl_legacy_init(struct ert_ctrl *ec)
{
	struct xocl_subdev_info subdev_info = XOCL_DEVINFO_COMMAND_QUEUE;
	xdev_handle_t xdev = xocl_get_xdev(ec->ec_pdev);
	struct xocl_ert_cq_privdata priv = {0};
	int err = 0;

	priv.cq_base = ec->ec_cq_base;
	priv.cq_range = ec->ec_cq_range;

	subdev_info.priv_data = &priv;
	subdev_info.data_len = sizeof(priv);
	err = xocl_subdev_create(xdev, &subdev_info);
	if (err) {
		EC_INFO(ec, "Can't create command queue subdev");
		return err;
	}

	EC_INFO(ec, "Legacy ERT mode connected");
	return 0;
}

static void ert_ctrl_legacy_fini(struct ert_ctrl *ec)
{
	xdev_handle_t xdev = xocl_get_xdev(ec->ec_pdev);

	xocl_subdev_destroy_by_id(xdev, XOCL_SUBDEV_COMMAND_QUEUE);

	EC_INFO(ec, "Legacy ERT mode disconnected");
	return;
}

static int ert_ctrl_xgq_init(struct ert_ctrl *ec)
{
	xdev_handle_t xdev = xocl_get_xdev(ec->ec_pdev);
	int ret = 0;

	ret = xgq_attach(&ec->ec_ctrl_xgq, 0, 0, (u64)ec->ec_cq_base+4, 0, 0);
	if (ret) {
		EC_ERR(ec, "Ctrl XGQ attach failed, ret %d", ret);
		return -ENODEV;
	}

	ec->ec_ert.submit = ert_ctrl_submit;
	ec->ec_ert.abort_sync = ert_ctrl_abort_sync;
	xocl_kds_init_ert(xdev, &ec->ec_ert);

	mutex_init(&ec->ec_xgq_lock);
	EC_INFO(ec, "XGQ based ERT firmware connected");
	return 0;
}

static void ert_ctrl_xgq_fini(struct ert_ctrl *ec)
{
	xdev_handle_t xdev = xocl_get_xdev(ec->ec_pdev);
	int i = 0;

	for (i = 0; i < ec->ec_exgq_capacity; i++) {
		if (ec->ec_exgq[i] == NULL)
			continue;

		xocl_xgq_fini(ec->ec_exgq[i]);
		ec->ec_exgq[i] = NULL;
	}

	xocl_kds_fini_ert(xdev);
	mutex_destroy(&ec->ec_xgq_lock);
	EC_INFO(ec, "XGQ based ERT firmware disconnected");
	return;
}

static int ert_ctrl_connect(struct platform_device *pdev)
{
	struct ert_ctrl *ec = platform_get_drvdata(pdev);
	int err = 0;

	if (ec->ec_connected)
		return -EBUSY;

	ec->ec_version = ert_ctrl_read32(ec->ec_cq_base + ERT_CTRL_VER_OFFSET);
	switch (ec->ec_version) {
	case ERT_CTRL_XGQ_VER1:
		EC_INFO(ec, "Connect XGQ based ERT firmware");
		err = ert_ctrl_xgq_init(ec);
		break;
	default:
		EC_INFO(ec, "Connect Legacy ERT firmware");
		err = ert_ctrl_legacy_init(ec);
	}
	if (err) {
		EC_ERR(ec, "connect error %d", err);
		return err;
	}

	ec->ec_connected = 1;
	return 0;
}

static void ert_ctrl_disconnect(struct platform_device *pdev)
{
	struct ert_ctrl *ec = platform_get_drvdata(pdev);

	if (!ec->ec_connected)
		return;

	switch (ec->ec_version) {
	case ERT_CTRL_XGQ_VER1:
		EC_INFO(ec, "Disconnect XGQ based ERT firmware");
		ert_ctrl_xgq_fini(ec);
		break;
	default:
		EC_INFO(ec, "Disconnect Legacy ERT firmware");
		ert_ctrl_legacy_fini(ec);
	}

	ec->ec_connected = 0;
	return;
}

static int ert_ctrl_is_version(struct platform_device *pdev, u32 major, u32 minor)
{
	struct ert_ctrl *ec = platform_get_drvdata(pdev);
	u32 version = 0;

	major &= 0xFFFF;
	minor &= 0xFFFF;

	version = (major << 16) + minor;

	return (ec->ec_version == version) ? 1 : 0;
}

static u64 ert_ctrl_get_base(struct platform_device *pdev)
{
	struct ert_ctrl *ec = platform_get_drvdata(pdev);

	return (u64)ec->ec_cq_base;
}

static void *ert_ctrl_setup_xgq(struct platform_device *pdev, int id, u64 offset)
{
	struct ert_ctrl *ec = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xocl_xgq_info xx_info = {0};
	int ret = 0;

	if (id >= ec->ec_exgq_capacity) {
		ret = ert_ctrl_alloc_ert_xgq(ec, id + ERT_CTRL_ADD_NUM_ERT_XGQ);
		if (ret)
			return ERR_PTR(ret);
	}

	if (ec->ec_exgq[id])
		goto done;

	/* TODO: Need setup XGQ IP or setup in memory XGQ */

	/* Setup in memory XGQ */
	xx_info.xi_id = id;
	xx_info.xi_addr = (u64)ec->ec_cq_base + offset;
	xx_info.xi_sq_prod_int = xocl_intc_get_csr_base(xdev) + CQ_STATUS_ADDR;
	ec->ec_exgq[id] = xocl_xgq_init(&xx_info);
	if (IS_ERR(ec->ec_exgq[id])) {
		EC_ERR(ec, "Initial xocl XGQ failed");
		return ec->ec_exgq[id];
	}

done:
	return ec->ec_exgq[id];
}

static int ert_ctrl_remove(struct platform_device *pdev)
{
	struct ert_ctrl	*ec = NULL;
	void *hdl = NULL;

	ec = platform_get_drvdata(pdev);
	if (!ec) {
		EC_ERR(ec, "ec is null");
		return -EINVAL;
	}

	if (ec->ec_connected)
		ert_ctrl_disconnect(pdev);

	if (ec->ec_cq_base)
		iounmap(ec->ec_cq_base);

	if (ec->ec_exgq)
		kfree(ec->ec_exgq);

	xocl_drvinst_release(ec, &hdl);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

static int ert_ctrl_probe(struct platform_device *pdev)
{
	struct ert_ctrl	*ec = NULL;
	struct resource *res = NULL;
	void *hdl = NULL;
	int err = 0;

	ec = xocl_drvinst_alloc(&pdev->dev, sizeof(struct ert_ctrl));
	if (!ec)
		return -ENOMEM;

	ec->ec_pdev = pdev;
	platform_set_drvdata(pdev, ec);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		EC_ERR(ec, "failed to get memory resource\n");
		err = -EINVAL;
		goto cq_init_failed;
	}
	EC_INFO(ec, "CQ %pR", res);

	ec->ec_cq_range = res->end - res->start + 1;
	ec->ec_cq_base = ioremap_wc(res->start, ec->ec_cq_range);
	if (!ec->ec_cq_base) {
		EC_ERR(ec, "failed to map CQ\n");
		err = -ENOMEM;
		goto cq_init_failed;
	}

	if (sysfs_create_group(&pdev->dev.kobj, &ert_ctrl_attrgroup))
		EC_ERR(ec, "Not able to create sysfs group");

	return 0;

cq_init_failed:
	xocl_drvinst_release(ec, &hdl);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return err;
}

static struct xocl_ert_ctrl_funcs ert_ctrl_ops = {
	.connect	= ert_ctrl_connect,
	.disconnect	= ert_ctrl_disconnect,
	.is_version	= ert_ctrl_is_version,
	.get_base	= ert_ctrl_get_base,
	.setup_xgq	= ert_ctrl_setup_xgq,
	/*
	.attach_xgq	= ert_ctrl_attach_xgq,
	.detach_xgq	= ert_ctrl_detach_xgq,
	.produce_xgq	= ert_ctrl_produce_xgq,
	.consume_xgq	= ert_ctrl_consume_xgq,
	*/
};

struct xocl_drv_private ert_ctrl_drv_priv = {
	.ops		= &ert_ctrl_ops,
	.fops		= NULL,
	.dev		= -1,
	.cdev_name	= NULL,
};

struct platform_device_id ert_ctrl_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ERT_CTRL), (kernel_ulong_t)&ert_ctrl_drv_priv },
	{},
};

static struct platform_driver ert_ctrl_driver = {
	.probe		= ert_ctrl_probe,
	.remove		= ert_ctrl_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_ERT_CTRL),
	},
	.id_table	= ert_ctrl_id_table,
};

int __init xocl_init_ert_ctrl(void)
{
	return platform_driver_register(&ert_ctrl_driver);
}

void xocl_fini_ert_ctrl(void)
{
	platform_driver_unregister(&ert_ctrl_driver);
}
