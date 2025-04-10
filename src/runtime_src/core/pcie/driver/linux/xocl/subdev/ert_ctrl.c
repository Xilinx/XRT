/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022 Advanced Micro Devices, Inc.
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
#include "xrt_drv.h"

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

#define ERT_CTRL_CMD_TIMEOUT	msecs_to_jiffies(8 * 1000)

#define ERT_CTRL_ADD_NUM_ERT_XGQ	4

#define CQ_STATUS_ADDR 0x58

#define CTRL_XGQ_SLOT_SIZE          512

/* XGQ IP offsets */
#define XGQ_SQ_REG		0x0
#define XGQ_CQ_REG		0x100

#define MAX_CU_XGQ		256
#define SHELL_NOT_SUPP_LEGACY(ec) (ec->ec_xgq_ips != NULL)

static uint16_t	g_ctrl_xgq_cid;
struct xocl_drv_private ert_ctrl_xgq_drv_priv;

struct ert_ctrl_xgq_cu {
	int			 ecxc_id;
	resource_size_t		 ecxc_xgq_reg;
	resource_size_t		 ecxc_xgq_range;
	int			 ecxc_xgq_irq;

	void __iomem		*ecxc_xgq_base;
};

struct ert_ctrl {
	struct kds_ert		 ec_ert;
	struct platform_device	*ec_pdev;
	void __iomem		*ec_cq_base;
	uint32_t		 ec_cq_range;

	uint32_t		 ec_version;
	uint32_t		 ec_connected;
	struct xgq		 ec_ctrl_xgq;
	struct mutex		 ec_xgq_lock;

	struct ert_ctrl_xgq_cu	*ec_xgq_ips;
	size_t			 ec_num_xgq_ips;
	/* ERT XGQ instances for CU */
	void			**ec_exgq;
	uint32_t		 ec_exgq_capacity;

	uint64_t		timestamp;
	uint32_t		cq_read_single;
	uint32_t		cq_write_single;
	uint32_t		cu_read_single;
	uint32_t		cu_write_single;


	uint32_t		h2h_access;
	uint32_t		d2h_access;
	uint32_t		h2d_access;
	uint32_t		d2d_access;
	uint32_t		d2cu_access;
	uint32_t		data_integrity;	
};

static void ert_ctrl_submit_exit_cmd(struct ert_ctrl *ec);

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

static ssize_t mb_sleep_store(struct device *dev,
	struct device_attribute *da, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct ert_ctrl *ec = dev_get_drvdata(dev);
	u32 go_sleep;

	if (kstrtou32(buf, 10, &go_sleep) == -EINVAL || go_sleep > 2) {
		xocl_err(&to_platform_device(dev)->dev,
			"usage: echo 0 or 1 > mb_sleep");
		return -EINVAL;
	}

	if (go_sleep) {
		xocl_gpio_cfg(xdev, MB_WAKEUP_CLR);
		ert_ctrl_submit_exit_cmd(ec);
		xocl_gpio_cfg(xdev, MB_SLEEP);
	} else
		xocl_gpio_cfg(xdev, MB_WAKEUP);

	return count;
}

static ssize_t mb_sleep_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);

	return sprintf(buf, "%d", xocl_gpio_cfg(xdev, MB_STATUS));
}
static DEVICE_ATTR_RW(mb_sleep);

static ssize_t
clock_timestamp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ert_ctrl *ec = dev_get_drvdata(dev);

	return sprintf(buf, "%lld", ec->timestamp);
};
static DEVICE_ATTR_RO(clock_timestamp);

static ssize_t
cq_read_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ert_ctrl *ec = dev_get_drvdata(dev);

	return sprintf(buf, "%d", ec->cq_read_single);
};
static DEVICE_ATTR_RO(cq_read_cnt);

static ssize_t
cq_write_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ert_ctrl *ec = dev_get_drvdata(dev);

	return sprintf(buf, "%d", ec->cq_write_single);
};
static DEVICE_ATTR_RO(cq_write_cnt);

static ssize_t
cu_read_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ert_ctrl *ec = dev_get_drvdata(dev);

	return sprintf(buf, "%d", ec->cu_read_single);
};
static DEVICE_ATTR_RO(cu_read_cnt);

static ssize_t
cu_write_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ert_ctrl *ec = dev_get_drvdata(dev);

	return sprintf(buf, "%d", ec->cu_write_single);
};
static DEVICE_ATTR_RO(cu_write_cnt);

static ssize_t
data_integrity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ert_ctrl *ec = dev_get_drvdata(dev);
	uint32_t ret = 1;

	ret &= ec->h2h_access;
	ret &= ec->d2h_access;
	ret &= ec->h2d_access;
	ret &= ec->d2d_access;
	ret &= ec->d2cu_access;
	ret &= ec->data_integrity;

	return sprintf(buf, "%d", ret);
};
static DEVICE_ATTR_RO(data_integrity);

static ssize_t
xgq_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ert_ctrl *ec = dev_get_drvdata(dev);
	ssize_t sz = 0;
	int i;

	for (i = 0; i < ec->ec_exgq_capacity; i++) {
		if (ec->ec_exgq[i] == NULL)
			continue;

		sz += xocl_xgq_dump_info(ec->ec_exgq[i], buf+sz, PAGE_SIZE - sz);
	}

	return sz;
};
static DEVICE_ATTR_RO(xgq_info);

static struct attribute *ert_ctrl_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_mb_sleep.attr,
	&dev_attr_clock_timestamp.attr,
	&dev_attr_cq_read_cnt.attr,
	&dev_attr_cq_write_cnt.attr,
	&dev_attr_cu_read_cnt.attr,
	&dev_attr_cu_write_cnt.attr,
	&dev_attr_data_integrity.attr,
	&dev_attr_xgq_info.attr,
	NULL,
};

static ssize_t
cq_bin_show(struct file *filp, struct kobject *kobj,
	    struct bin_attribute *attr, char *buf,
	    loff_t offset, size_t count)
{
	struct ert_ctrl *ec;
	struct device *dev = container_of(kobj, struct device, kobj);
	ssize_t nread = 0;
	size_t size = 0;

	ec = (struct ert_ctrl *)dev_get_drvdata(dev);
	if (!ec || !ec->ec_cq_base)
		return nread;

	size = ec->ec_cq_range;
	if (offset >= size)
		goto done;

	if (offset + count < size)
		nread = count;
	else
		nread = size - offset;

	xocl_memcpy_fromio(buf, ec->ec_cq_base + offset, nread);

done:
	return nread;
}

static struct bin_attribute cq_attr = {
	.attr = {
		.name ="cq_bin",
		.mode = 0444
	},
	.read = cq_bin_show,
	.write = NULL,
	.size = 0
};

static struct bin_attribute *ert_ctrl_bin_attrs[] = {
	&cq_attr,
	NULL,
};

static const struct attribute_group ert_ctrl_attrgroup = {
	.attrs = ert_ctrl_attrs,
	.bin_attrs = ert_ctrl_bin_attrs,
};

static inline uint32_t ert_ctrl_read32(void __iomem *addr)
{
	return ioread32(addr);
}

static inline void ert_ctrl_write32(uint32_t val, void __iomem *addr)
{
	return iowrite32(val, addr);
}

static void ert_ctrl_init_access_test(struct ert_ctrl *ec)
{
	ec->h2h_access = 1;
	ec->d2h_access = 1;
	ec->h2d_access = 1;
	ec->d2d_access = 1;
	ec->d2cu_access = 1;
	ec->data_integrity  = 1;
}
static inline void ert_ctrl_pre_process(struct ert_ctrl *ec, enum xgq_cmd_opcode opcode, void *slot_addr)
{
	int offset = 0;
	u32 val, clear = 0;
	u32 cnt = 10000000;

	switch(opcode) {
	case XGQ_CMD_OP_DATA_INTEGRITY:
		ert_ctrl_init_access_test(ec);
		for (offset = sizeof(struct xgq_cmd_data_integrity); offset < CTRL_XGQ_SLOT_SIZE; offset+=4) {
			iowrite32(HOST_RW_PATTERN, (void __iomem *)(slot_addr + offset));

			val = ioread32((void __iomem *)(slot_addr + offset));

			if (val !=HOST_RW_PATTERN) {
				ec->h2h_access = 0;
				EC_ERR(ec, "Host <-> Host data integrity failed\n");
				break;
			}

		}
		iowrite32(clear, slot_addr + offsetof(struct xgq_cmd_data_integrity, draft));
		iowrite32(cnt, slot_addr + offsetof(struct xgq_cmd_data_integrity, rw_count));
		break;
	default:
		break;
	}
}


static void ert_ctrl_post_process(struct ert_ctrl *ec, enum xgq_cmd_opcode opcode, void *resp)
{
	struct xgq_cmd_resp_clock_calib *clock_calib;
	struct xgq_cmd_resp_access_valid *access_valid;
	struct xgq_cmd_resp_data_integrity *integrity;

	switch(opcode) {
	case XGQ_CMD_OP_CLOCK_CALIB:
		clock_calib = (struct xgq_cmd_resp_clock_calib *)resp;
		ec->timestamp = clock_calib->timestamp;
		break;
	case XGQ_CMD_OP_ACCESS_VALID:
		access_valid = (struct xgq_cmd_resp_access_valid *)resp;
		ec->cq_read_single = access_valid->cq_read_single;
		ec->cq_write_single = access_valid->cq_write_single;
		ec->cu_read_single = access_valid->cu_read_single;
		ec->cu_write_single = access_valid->cu_write_single;
		break;
	case XGQ_CMD_OP_DATA_INTEGRITY:
		integrity = (struct xgq_cmd_resp_data_integrity *)resp;
		ec->h2d_access = integrity->h2d_access;
		ec->d2d_access = integrity->d2d_access;
		ec->d2cu_access = integrity->d2cu_access;
		ec->data_integrity = integrity->data_integrity;
		break;
	default:
		break;
	}
}

static inline void ert_ctrl_queue_integrity_test(uint64_t slot_addr)
{
	uint32_t cnt = 10000000;

	while (--cnt) {
		u32 pattern = 0xFFFFFFFF;
		if (cnt % 2)
			pattern = 0xFFFFFFFF;
		else
			pattern = 0x0;
		
		iowrite32(pattern, (void __iomem *)(slot_addr + offsetof(struct xgq_cmd_data_integrity, draft)));
	}
	iowrite32(cnt, (void __iomem *)(slot_addr + offsetof(struct xgq_cmd_data_integrity, rw_count)));
}

static inline void ert_ctrl_device_to_host_integrity_test(struct ert_ctrl *ec, uint64_t slot_addr)
{
	u32 offset = 0, val = 0;

	for (offset = sizeof(struct xgq_cmd_data_integrity); offset < CTRL_XGQ_SLOT_SIZE; offset+=4) {
		iowrite32(DEVICE_RW_PATTERN, (void __iomem *)(slot_addr + offset));

		val = ioread32((void __iomem *)(slot_addr + offset));

		if (val !=DEVICE_RW_PATTERN) {
			ec->d2h_access = 0;
			EC_ERR(ec, "Device -> Host data integrity failed\n");
			break;
		}

	}
}

static inline
int __ert_ctrl_submit(struct ert_ctrl *ec, struct xgq_cmd_sq_hdr *req, uint32_t req_size,
			struct xgq_com_queue_entry *resp, uint32_t resp_size)
{
	struct xgq_com_queue_entry cq_hdr;
	u64 timeout = 0, slot_addr = 0;
	bool is_timeout = false;
	int ret = 0;

	mutex_lock(&ec->ec_xgq_lock);
	ret = xgq_produce(&ec->ec_ctrl_xgq, &slot_addr);
	if (ret) {
		EC_ERR(ec, "XGQ produce failed: %d", ret);
		mutex_unlock(&ec->ec_xgq_lock);
		return ret;
	}
	req->cid = g_ctrl_xgq_cid++;

	ert_ctrl_pre_process(ec, req->opcode, (void *)slot_addr);
	memcpy_toio((void __iomem *)slot_addr, req, req_size);

	xgq_notify_peer_produced(&ec->ec_ctrl_xgq);

	if (req->opcode == XGQ_CMD_OP_DATA_INTEGRITY)
		ert_ctrl_queue_integrity_test(slot_addr);

	timeout = jiffies + ERT_CTRL_CMD_TIMEOUT;
	while (!is_timeout) {
		usleep_range(100, 200);

		ret = xgq_consume(&ec->ec_ctrl_xgq, &slot_addr);
		if (!ret) {
			memcpy_fromio(resp, (void __iomem *)slot_addr, resp_size);
			memcpy_fromio(&cq_hdr, (void __iomem *)slot_addr, sizeof(cq_hdr));
			xgq_notify_peer_consumed(&ec->ec_ctrl_xgq);
			break;
		}

		is_timeout = (timeout < jiffies) ? true : false;

		if (is_timeout)
			ret = -ETIMEDOUT;
	}

	if (req->opcode == XGQ_CMD_OP_DATA_INTEGRITY)
		ert_ctrl_device_to_host_integrity_test(ec, slot_addr);

	mutex_unlock(&ec->ec_xgq_lock);

	if (!ret)
		ert_ctrl_post_process(ec, req->opcode, &cq_hdr);

	return ret;
}
static void ert_ctrl_submit(struct kds_ert *ert, struct kds_command *xcmd)
{
	struct ert_ctrl *ec = container_of(ert, struct ert_ctrl, ec_ert);
	int ret = 0;

	ret = __ert_ctrl_submit(ec, xcmd->info, xcmd->isize, xcmd->response, xcmd->response_size);

	if (!ret) {
		xcmd->status = KDS_COMPLETED;
	} else {
		if (ret == -ETIMEDOUT)
			xcmd->status = KDS_TIMEOUT;
		else if (ret == -ENOSPC)
			xcmd->status = KDS_ERROR;
	}
	xcmd->cb.notify_host(xcmd, xcmd->status);
	xcmd->cb.free(xcmd);
}


static void ert_ctrl_submit_exit_cmd(struct ert_ctrl *ec)
{
	struct xgq_cmd_sq_hdr sq_hdr;
	int ret = 0;

	sq_hdr.opcode = XGQ_CMD_OP_EXIT;
	sq_hdr.state = 1;

	ret = __ert_ctrl_submit(ec, &sq_hdr, sizeof(sq_hdr), NULL, 0);
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

	if (num > MAX_CU_XGQ)
		return -EINVAL;

	tmp = kzalloc(sizeof(ec->ec_exgq[0]) * num, GFP_KERNEL);
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

static void ert_ctrl_unset_xgq(struct platform_device *pdev, void *xgq_handle)
{
	struct ert_ctrl *ec = platform_get_drvdata(pdev);
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct ert_ctrl_xgq_cu  *xgq_ips = NULL;
	int xgq_id = xocl_get_xgq_id(xgq_handle);

	BUG_ON(xgq_id == -EINVAL);

	/* Check whether any further CUs/SCUs are still refering this XGQ.
	 * If yes, then don't cleanup and return from here.
	 */
	if (xocl_decr_xgq_ref_cnt(ec->ec_exgq[xgq_id]) != 0)
		return;

	xgq_ips = &ec->ec_xgq_ips[xgq_id];
	if ((xgq_id < ec->ec_num_xgq_ips) && xgq_ips) {
		xocl_user_interrupt_config(xdev, xgq_ips->ecxc_xgq_irq, false);
		xocl_user_interrupt_reg(xdev, xgq_ips->ecxc_xgq_irq, NULL, NULL);
	}

	if (ec->ec_exgq[xgq_id]) {
		if (!ec->ec_xgq_ips) {
			xocl_intc_ert_config(xdev, xgq_id, false);
			xocl_intc_ert_request(xdev, xgq_id, NULL, NULL);
		}
		
		ec->ec_exgq[xgq_id] = NULL;
	}
}

static int ert_ctrl_xgq_init(struct ert_ctrl *ec)
{
	xdev_handle_t xdev = xocl_get_xdev(ec->ec_pdev);
	int ret = 0;

	ret = xgq_attach(&ec->ec_ctrl_xgq, 0, 0, (u64)ec->ec_cq_base+4, 0, 0);
	if (ret) {
		EC_ERR(ec, "Ctrl XGQ attach failed, ret %d", ret);
		return ret;
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

		xocl_decr_xgq_ref_cnt(ec->ec_exgq[i]);
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
		return 0;

	ec->ec_version = ert_ctrl_read32(ec->ec_cq_base + ERT_CTRL_VER_OFFSET);
	switch (ec->ec_version) {
	case ERT_CTRL_XGQ_VER1:
		EC_INFO(ec, "Connect XGQ based ERT firmware");
		err = ert_ctrl_xgq_init(ec);
		break;
	default:
		if (SHELL_NOT_SUPP_LEGACY(ec)) {
			err = -ENODEV;
			break;
		}
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

	/* If this XGQ is already setup, skip */
	if (ec->ec_exgq[id])
		goto done;

	if (ec->ec_xgq_ips) {
		/* XGQ IP presented */
		xx_info.xi_id = id;
		xx_info.xi_addr = (u64)ec->ec_cq_base + offset;
		/* No need to write to a specific register to trigger interrupt.
		 * Write to SQ produce register will trigger interrupt.
		 */
		xx_info.xi_sq_prod_int = NULL;
		xx_info.xi_sq_prod = ec->ec_xgq_ips[id].ecxc_xgq_base + XGQ_SQ_REG;
		xx_info.xi_cq_prod = ec->ec_xgq_ips[id].ecxc_xgq_base + XGQ_CQ_REG;
	} else {
		/* Setup in memory XGQ */
		xx_info.xi_id = id;
		xx_info.xi_addr = (u64)ec->ec_cq_base + offset;
		xx_info.xi_sq_prod_int = xocl_intc_get_csr_base(xdev) + CQ_STATUS_ADDR;
		xx_info.xi_sq_prod = NULL;
		xx_info.xi_cq_prod = NULL;
	}
	ec->ec_exgq[id] = xocl_xgq_init(&xx_info);
	if (IS_ERR(ec->ec_exgq[id])) {
		void *err_ret = ec->ec_exgq[id];

		ec->ec_exgq[id] = NULL;
		EC_ERR(ec, "Initial xocl XGQ failed");
		return err_ret;
	}

	/* Setup CU XGQ interrupt */
	if (ec->ec_xgq_ips) {
		xocl_user_interrupt_reg(xdev,  ec->ec_xgq_ips[id].ecxc_xgq_irq, xgq_isr, ec->ec_exgq[id]);
		xocl_user_interrupt_config(xdev, ec->ec_xgq_ips[id].ecxc_xgq_irq, true);
	} else {
		xocl_intc_ert_request(xdev, id, xgq_isr, ec->ec_exgq[id]);
		xocl_intc_ert_config(xdev, id, true);
	}

done:
	xocl_incr_xgq_ref_cnt(ec->ec_exgq[id]);
	return ec->ec_exgq[id];
}

static void ert_ctrl_dump_xgq(struct platform_device *pdev)
{
	struct ert_ctrl *ec = platform_get_drvdata(pdev);
	struct xgq_header *xgq_hdr = ec->ec_cq_base + 0x4;
	int i;

	printk("(Debug) Dump control XGQ header info\n");
	printk("magic: 0x%x\n", xgq_hdr->xh_magic);
	printk("version: 0x%x\n", xgq_hdr->xh_version);
	printk("slot_num: 0x%x\n", xgq_hdr->xh_slot_num);
	printk("sq_offset: 0x%x\n", xgq_hdr->xh_sq_offset);
	printk("cq_offset: 0x%x\n", xgq_hdr->xh_cq_offset);
	printk("sq_slot_size: 0x%x\n", xgq_hdr->xh_sq_slot_size);
	printk("sq_produced: %d\n", xgq_hdr->xh_sq_produced);
	printk("sq_consumed: %d\n", xgq_hdr->xh_sq_consumed);
	printk("cq_produced: %d\n", xgq_hdr->xh_cq_produced);
	printk("cq_consumed: %d\n", xgq_hdr->xh_cq_consumed);

	printk("(Debug) Dump control XGQ sq slots\n");
	for (i = 0; i < xgq_hdr->xh_slot_num; i++) {
		u32 *data;

		data = (u32 *)(((char *)xgq_hdr) + xgq_hdr->xh_sq_offset + i * xgq_hdr->xh_sq_slot_size);
		printk("slots(%d)", i);
		print_hex_dump(KERN_INFO, "raw data: ", DUMP_PREFIX_OFFSET,
				16, 4, data, xgq_hdr->xh_sq_slot_size, true);
	}

	printk("(Debug) Dump control XGQ cq slots\n");
	for (i = 0; i < xgq_hdr->xh_slot_num; i++) {
		u32 *data;

		data = (u32 *)(((char *)xgq_hdr) + xgq_hdr->xh_cq_offset + i * 16);
		printk("slots(%d)", i);
		print_hex_dump(KERN_INFO, "raw data: ", DUMP_PREFIX_OFFSET,
				16, 4, data, 16, true);
	}

	/* special value at 0x600 */
	print_hex_dump(KERN_INFO, "raw data: ", DUMP_PREFIX_OFFSET,
		       16, 4, ec->ec_cq_base + 0x600, 48, true);
}

static int ert_ctrl_xgq_ip_init(struct platform_device *pdev)
{
	struct ert_ctrl	*ec = platform_get_drvdata(pdev);
	struct resource *res = NULL;
	int i = 0;

	EC_DBG(ec, "XGQ IPs and Ring buffer model");
	res = xocl_get_iores_byname(pdev, RESNAME_XGQ_USER_RING);
	if (!res) {
		EC_ERR(ec, "failed to get %s", RESNAME_XGQ_USER_RING);
		return -EINVAL;
	}
	EC_INFO(ec, "Ring buffer %pR", res);

	ec->ec_cq_range = res->end - res->start + 1;
	ec->ec_cq_base = devm_ioremap(&pdev->dev, res->start, ec->ec_cq_range);
	if (!ec->ec_cq_base) {
		EC_ERR(ec, "failed to map %s", RESNAME_XGQ_USER_RING);
		return -ENOMEM;
	}

	/* Handle all of the XGQ IPs */
	ec->ec_num_xgq_ips = xocl_count_iores_byname(pdev, RESNAME_XGQ_USER_SQ);
	ec->ec_xgq_ips = devm_kzalloc(&pdev->dev, sizeof(struct ert_ctrl_xgq_cu) * ec->ec_num_xgq_ips, GFP_KERNEL);
	if (!ec->ec_xgq_ips) {
		EC_ERR(ec, "failed to allocate ec_xgq_ips");
		return -ENOMEM;
	}

	for (i = 0; i < ec->ec_num_xgq_ips; i++) {
		struct ert_ctrl_xgq_cu *xgq_ip;

		xgq_ip = &ec->ec_xgq_ips[i];
		res = xocl_get_iores_with_idx_byname(pdev, RESNAME_XGQ_USER_SQ, i);
		if (!res) {
			EC_ERR(ec, "failed to get %s", RESNAME_XGQ_USER_SQ);
			return -EINVAL;
		}
		EC_INFO(ec, "XGQ IP %pR", res);

		xgq_ip->ecxc_xgq_irq = xocl_get_irq_with_idx_byname(pdev, RESNAME_XGQ_USER_SQ, i);
		xgq_ip->ecxc_xgq_reg = res->start;
		xgq_ip->ecxc_xgq_range = res->end - res->start + 1;
		xgq_ip->ecxc_xgq_base = devm_ioremap(&pdev->dev, res->start, xgq_ip->ecxc_xgq_range);
		if (!xgq_ip->ecxc_xgq_base)
			return -ENOMEM;
	}

	/* TODO: Should sort XGQ IP by address to make sure the indexing the
	 * same as device side
	 */

	return 0;
}

static int ert_ctrl_cq_init(struct platform_device *pdev)
{
	struct ert_ctrl	*ec = platform_get_drvdata(pdev);
	struct resource *res = NULL;

	EC_DBG(ec, "CSR registers and Command Queue model");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		EC_ERR(ec, "failed to get memory resource");
		return -EINVAL;
	}
	EC_INFO(ec, "CQ %pR", res);

	ec->ec_cq_range = res->end - res->start + 1;
	ec->ec_cq_base = devm_ioremap(&pdev->dev, res->start, ec->ec_cq_range);
	if (!ec->ec_cq_base) {
		EC_ERR(ec, "failed to map CQ");
		return -ENOMEM;
	}

	ec->ec_xgq_ips = NULL;
	ec->ec_num_xgq_ips = 0;

	return 0;
}

static int __ert_ctrl_remove(struct platform_device *pdev)
{
	struct ert_ctrl	*ec = NULL;
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct ert_ctrl_xgq_cu  *xgq_ips = NULL;
	void *hdl = NULL;
	int i = 0;

	ec = platform_get_drvdata(pdev);
	if (!ec)
		return -EINVAL;

	for (i = 0; i < ec->ec_num_xgq_ips; i++) {
		xgq_ips = &ec->ec_xgq_ips[i];

		xocl_user_interrupt_config(xdev, xgq_ips->ecxc_xgq_irq, false);
		xocl_user_interrupt_reg(xdev, xgq_ips->ecxc_xgq_irq, NULL, NULL);
	}

	if (ec->ec_connected)
		ert_ctrl_disconnect(pdev);

	if (ec->ec_exgq)
		kfree(ec->ec_exgq);

	xocl_drvinst_release(ec, &hdl);
	platform_set_drvdata(pdev, NULL);
	xocl_drvinst_free(hdl);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void ert_ctrl_remove(struct platform_device *pdev)
{
	__ert_ctrl_remove(pdev);
}
#else
#define ert_ctrl_remove __ert_ctrl_remove
#endif

static int ert_ctrl_probe(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	bool ert_on = xocl_ert_on(xdev);
	struct ert_ctrl	*ec = NULL;
	void *hdl = NULL;
	int err = 0;

	/* If XOCL_DSAFLAG_MB_SCHE_OFF is set, we should not probe */
	if (!ert_on) {
		xocl_warn(&pdev->dev, "Disable ERT flag is set");
		return -ENODEV;
	}

	ec = xocl_drvinst_alloc(&pdev->dev, sizeof(struct ert_ctrl));
	if (!ec)
		return -ENOMEM;

	ec->ec_pdev = pdev;
	platform_set_drvdata(pdev, ec);

	if (XOCL_GET_DRV_PRI(pdev) == &ert_ctrl_xgq_drv_priv)
		err = ert_ctrl_xgq_ip_init(pdev);
	else
		err = ert_ctrl_cq_init(pdev);
	if (err)
		goto init_failed;

	/* At this point, we are not able to attach control XGQ, since we don't
	 * know if XGQ ERT is ready or not. We cannot wait for it.
	 * The attach control XGQ can happen later by calling ert_ctrl_connect.
	 */

	if (sysfs_create_group(&pdev->dev.kobj, &ert_ctrl_attrgroup))
		EC_ERR(ec, "Not able to create sysfs group");

	return 0;

init_failed:
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
	.unset_xgq	= ert_ctrl_unset_xgq,
	.dump_xgq	= ert_ctrl_dump_xgq,
};

struct xocl_drv_private ert_ctrl_drv_priv = {
	.ops		= &ert_ctrl_ops,
	.fops		= NULL,
	.dev		= -1,
	.cdev_name	= NULL,
};

struct xocl_drv_private ert_ctrl_xgq_drv_priv = {
	.ops		= &ert_ctrl_ops,
	.fops		= NULL,
	.dev		= -1,
	.cdev_name	= NULL,
};

struct platform_device_id ert_ctrl_id_table[] = {
	{ XOCL_DEVNAME(XOCL_ERT_CTRL), (kernel_ulong_t)&ert_ctrl_drv_priv },
	{ XOCL_DEVNAME(XOCL_ERT_CTRL_VERSAL), (kernel_ulong_t)&ert_ctrl_xgq_drv_priv },
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
