/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Author(s):
 *        Lizhi Hou <lizhih@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include "zocl_drv.h"
#include "zocl_ert_intc.h"
#include "zocl_xgq.h"
#include "zocl_xclbin.h"

#define ZRPU_CHANNEL_NAME "zocl_rpu_channel"

#define ZCHAN2PDEV(chan)		((chan)->pdev)
#define ZCHAN2DEV(chan)			(&ZCHAN2PDEV(chan)->dev)
#define zchan_err(chan, fmt, args...)	zocl_err(ZCHAN2DEV(chan), fmt"\n", ##args)
#define zchan_info(chan, fmt, args...)	zocl_info(ZCHAN2DEV(chan), fmt"\n", ##args)
#define zchan_dbg(chan, fmt, args...)	zocl_dbg(ZCHAN2DEV(chan), fmt"\n", ##args)

/* reserve 4k shared memory for RPU outband communication */
#define ZRPU_CHANNEL_READY		0
#define ZRPU_CHANNEL_XGQ_OFF		4

/* hardcode XGQ buffer from offset 4K, size is 4K, too */
#define ZRPU_CHANNEL_XGQ_BUFFER		4096
#define ZRPU_CHANNEL_XGQ_BUFFER_SIZE	4096
#define ZRPU_CHANNEL_XGQ_SLOT_SIZE	1024

#define MAX_LOG_LEN 80
#define MAX_PAGE_SIZE 4096
static char info_buf[MAX_LOG_LEN] = { 0 };
static char log_buf[MAX_PAGE_SIZE] = { 0 };

struct zocl_rpu_data_entry {
	struct list_head	entry_list;
	char			*data_entry;
	size_t			data_size;
};

struct zocl_rpu_channel {
	struct platform_device *pdev;
	struct platform_device *intc_pdev;
	void __iomem *mem_base;
	void __iomem *xgq_base;
	void *xgq_hdl;
	u64 mem_start;
	size_t mem_size;
	struct list_head	data_list;
};

static inline void reg_write(void __iomem *base, u64 off, u32 val)
{
	iowrite32(val, base + off);
}

static inline u32 reg_read(void __iomem *base, u64 off)
{
	return ioread32(base + off);
}

static ssize_t ready_store(struct device *dev, struct device_attribute *da,
	const char *buf, size_t count)
{
	struct zocl_rpu_channel *chan = (struct zocl_rpu_channel *)dev_get_drvdata(dev);
	u32 val;

	if (kstrtou32(buf, 10, &val) < 0 || val != 1) {
		zchan_err(chan, "invalid input %d\n", val);
		return -EINVAL;
	}

	reg_write(chan->mem_base, ZRPU_CHANNEL_READY, 1);

	return count;
}
static DEVICE_ATTR_WO(ready);

static struct attribute *zrpu_channel_attrs[] = {
	&dev_attr_ready.attr,
	NULL,
};

static const struct attribute_group zrpu_channel_attrgroup = {
	.attrs = zrpu_channel_attrs,
};

static const struct of_device_id zocl_rpu_channel_of_match[] = {
	{ .compatible = "xlnx,rpu-channel", },
	{ /* end of table */ },
};

#define ZCHAN_CMD_HANDLER_VER_MAJOR	1
#define ZCHAN_CMD_HANDLER_VER_MINOR	0

typedef void (*cmd_handler)(struct zocl_rpu_channel *chan, struct xgq_cmd_sq_hdr *cmd,
			    struct xgq_com_queue_entry *resp);

static void init_resp(struct xgq_com_queue_entry *resp, u16 cid, u32 rcode)
{
	memset(resp, 0, sizeof(*resp));
	resp->hdr.cid = cid;
	resp->hdr.cstate = XGQ_CMD_STATE_COMPLETED;
	resp->rcode = rcode;
}

static void zchan_cmd_identify(struct zocl_rpu_channel *chan, struct xgq_cmd_sq_hdr *cmd,
			       struct xgq_com_queue_entry *resp)
{
	struct xgq_cmd_resp_identify *r = (struct xgq_cmd_resp_identify *)resp;

	init_resp(resp, cmd->cid, 0);

	r->major = ZCHAN_CMD_HANDLER_VER_MAJOR;
	r->minor = ZCHAN_CMD_HANDLER_VER_MINOR;
}

static int zchan_cmd_log_page_info(struct zocl_rpu_channel *chan, u32 add_off, u32 size,
	u32 *total_countp)
{
	u32 count = 0;
	u32 total_count = 0;
	int ret = 0;
	
	count = snprintf(info_buf, sizeof(info_buf), "ZOCL Version:");
	if (count > size) {
		zchan_err(chan, "message is trunked to %d len", total_count);
		ret = -EINVAL;
		goto done;
	}
	memcpy_toio(chan->mem_base + add_off, info_buf, count);
	total_count += count;

	count = snprintf(info_buf, sizeof(info_buf), "%s%s%s\n",
		XRT_DRIVER_VERSION, ", ", XRT_HASH);
	if (total_count + count > size) {
		zchan_err(chan, "message is trunked to %d len", total_count);
		ret = -EINVAL;
		goto done;
	}
	memcpy_toio(chan->mem_base + add_off + total_count, info_buf, count);
	total_count += count;

	count = snprintf(info_buf, sizeof(info_buf), "ZOCL Build Date:");
	if (total_count + count > size) {
		zchan_err(chan, "message is trunked to %d len", total_count);
		ret = -EINVAL;
		goto done;
	}
	memcpy_toio(chan->mem_base + add_off + total_count, info_buf, count);
	total_count += count;

	count = snprintf(info_buf, sizeof(info_buf), "%s\n", XRT_HASH_DATE);
	if (total_count + count > size) {
		zchan_err(chan, "message is trunked to %d len", total_count);
		ret = -EINVAL;
		goto done;
	}
	memcpy_toio(chan->mem_base + add_off + total_count, info_buf, count);
	total_count += count;

done:
	*total_countp = total_count;
	return ret;
}


/*
 * add code here copying data back into chan->mem_base.
 * note: copy data start from logs data + offset instead of data + 0
 *       size is the request size, copy data within this range
 *       set return value as data total size copied out.
 *
 */
static int zchan_cmd_log_apu_log(struct zocl_rpu_channel *chan, u32 add_off, u32 size,
	u32 offset, u32 *total_countp)
{
	u32 count = 0;
	struct file *fp;
	loff_t fp_size = 0;
	loff_t fp_offset = 0;
	int ret = 0;
	u32 remain_size = 0;

	fp = filp_open("/var/log/messages", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		*total_countp = 0;
		pr_err("Can't open /var/log/messages\n");
		return -EINVAL;
	}

	fp_size = i_size_read(file_inode(fp));
	fp_offset = offset;
	remain_size = fp_size - offset;
	// Send chunks of log
	if (remain_size >= size) {
		ret = kernel_read(fp, &log_buf, size, &fp_offset);
		if (!ret) {
			pr_err("Can't read from /var/log/messages\n");
			*total_countp = 0;
			filp_close(fp, NULL);
			return -EINVAL;
		}
		count = size;
	} else {
		if(remain_size > 0) {
			ret = kernel_read(fp, &log_buf, remain_size, &fp_offset);
			if (!ret) {
				pr_err("Can't read from /var/log/messages\n");
				*total_countp = 0;
				filp_close(fp, NULL);
				return -EINVAL;
			}
		}
		count = remain_size;
	}
	if (count > 0)
		memcpy_toio(chan->mem_base + add_off, log_buf, count);
	filp_close(fp, NULL);

	*total_countp = count;
	return 0;
}

static void zchan_cmd_log_page(struct zocl_rpu_channel *chan, struct xgq_cmd_sq_hdr *cmd,
	struct xgq_com_queue_entry *resp)
{
	struct xgq_cmd_sq *sq = (struct xgq_cmd_sq *)cmd;
	struct xgq_cmd_cq *cq = (struct xgq_cmd_cq *)resp;
	u32 add_off = sq->log_payload.address;
	u32 size = sq->log_payload.size;
	u16 pid = sq->log_payload.pid;
	u32 offset = sq->log_payload.offset;
	u32 total_count = 0;
	int ret = 0;

	zchan_dbg(chan, "addr_off 0x%x, size %d, offset %d, pid %d",
		   add_off, size, offset, pid);

	switch (pid) {
	case XGQ_CMD_LOG_INFO:
		ret = zchan_cmd_log_page_info(chan, add_off, size, &total_count);
		break;
	case XGQ_CMD_LOG_APU_LOG:
		ret = zchan_cmd_log_apu_log(chan, add_off, size, offset, &total_count);
		break;
	default:
		zchan_err(chan, "unsupported pid: %d", pid);
		ret = -EINVAL;
		total_count = 0;
	}

	init_resp(resp, cmd->cid, ret);
	cq->cq_log_payload.count = total_count;
	return;
}

static void zchan_cmd_load_xclbin(struct zocl_rpu_channel *chan, struct xgq_cmd_sq_hdr *cmd,
	struct xgq_com_queue_entry *resp)
{
	struct xgq_cmd_sq *sq = (struct xgq_cmd_sq *)cmd;
	struct xgq_cmd_cq *cq = (struct xgq_cmd_cq *)resp;
	u32 address_offset = sq->xclbin_payload.address;
	u32 size = sq->xclbin_payload.size;
	u32 remain_size = sq->xclbin_payload.remain_size;
	u32 slot_id = sq->xclbin_payload.priv;
	struct zocl_rpu_data_entry *entry = NULL;
	void __iomem *src = chan->mem_base + address_offset;
	int ret = 0;

	zchan_info(chan, "addr_off 0x%x, size %d, remain %d",
		address_offset, size, remain_size);

	/*
	 * if remain_size is not 0, this is not last pkt.
	 *
	 * every pkt will be added to linkedlist.
	 * if this is last pkt, dump linked list to whole xclbin.
	 */
	entry = vmalloc(sizeof(struct zocl_rpu_data_entry));
	if (entry == NULL) {
		zchan_err(chan, "no memory");
		ret = -ENOMEM;
		goto fail;
	}
	entry->data_size = size;
	entry->data_entry = vmalloc(size);
	memcpy_fromio(entry->data_entry, src, size);

	list_add_tail(&entry->entry_list, &chan->data_list);

	/* remain_size 0 indicates this is the last pkt */
	if (remain_size == 0) {
		struct list_head *pos = NULL, *next = NULL;
		struct zocl_rpu_data_entry *elem = NULL;
		size_t total_size = 0;
		char *total_data = NULL;
		char *cur = NULL;
		int ret;

		list_for_each_safe(pos, next, &chan->data_list) {
			elem = list_entry(pos, struct zocl_rpu_data_entry, entry_list);
			total_size += elem->data_size;
		}

		total_data = vmalloc(total_size);
		if (total_data == NULL) {
			zchan_err(chan, "no memory");
			ret = -ENOMEM;
			goto fail;
		}

		cur = total_data;
		pos = NULL;
		next = NULL;
		list_for_each_safe(pos, next, &chan->data_list) {
			elem = list_entry(pos, struct zocl_rpu_data_entry, entry_list);
			
			memcpy(cur, elem->data_entry, elem->data_size);
			cur += elem->data_size;

			list_del(pos);
			vfree(elem->data_entry);
			vfree(elem);
		}
		zchan_info(chan, "total size: %ld list empty %d",
			   total_size, list_empty(&chan->data_list));
		INIT_LIST_HEAD(&chan->data_list);

		ret = zocl_xclbin_load_pskernel(zocl_get_zdev(), total_data, slot_id);
		if (ret)
			zchan_err(chan, "failed to cache xclbin: %d", ret);

		vfree(total_data);
	}

	init_resp(resp, cmd->cid, 0);
	cq->cq_xclbin_payload.count = size;
	return;
fail:
	/* if list is not empty, clean up memory and re-init list */
	if (list_empty(&chan->data_list)) {
		struct list_head *pos = NULL, *next = NULL;
		struct zocl_rpu_data_entry *elem = NULL;

		list_for_each_safe(pos, next, &chan->data_list) {
			elem = list_entry(pos, struct zocl_rpu_data_entry, entry_list);
			list_del(pos);
			vfree(elem->data_entry);
			vfree(elem);
		}
		INIT_LIST_HEAD(&chan->data_list);
	}

	init_resp(resp, cmd->cid, ret);
	return;
}

static void zchan_cmd_default_handler(struct zocl_rpu_channel *chan, struct xgq_cmd_sq_hdr *cmd,
				      struct xgq_com_queue_entry *resp)
{
	zchan_err(chan, "Unknown cmd: %d", cmd->opcode);
	init_resp(resp, cmd->cid, -ENOTTY);
}

struct zchan_ops {
	u32 op;
	char *name;
	cmd_handler handler;
} zchan_op_table[] = {
	{ XGQ_CMD_OP_IDENTIFY, "XGQ_CMD_OP_IDENTIFY", zchan_cmd_identify },
	{ XGQ_CMD_OP_LOAD_XCLBIN, "XGQ_CMD_OP_LOAD_XCLBIN", zchan_cmd_load_xclbin },
	{ XGQ_CMD_OP_GET_LOG_PAGE, "XGQ_CMD_OP_GET_LOG_PAGE", zchan_cmd_log_page },
};

static inline const struct zchan_ops *opcode2op(u32 op)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(zchan_op_table); i++) {
		if (zchan_op_table[i].op == op)
			return &zchan_op_table[i];
	}
	return NULL;
}

static inline const char *opcode2name(u32 opcode)
{
	const struct zchan_ops *op = opcode2op(opcode);

	return op ? op->name : "UNKNOWN_CMD";
}

static inline cmd_handler opcode2handler(u32 opcode)
{
	const struct zchan_ops *op = opcode2op(opcode);

	return op ? op->handler : NULL;
}

/* All channel command is run-to-complete, no async process is supported. */
static void zchan_cmd_handler(struct platform_device *pdev, struct xgq_cmd_sq_hdr *cmd)
{
	struct zocl_rpu_channel *chan = platform_get_drvdata(pdev);
	u32 op = cmd->opcode;
	cmd_handler func = opcode2handler(op);
	struct xgq_com_queue_entry r = {};

	zchan_dbg(chan, "%s received", opcode2name(op));
	if (func)
		func(chan, cmd, &r);
	else
		zchan_cmd_default_handler(chan, cmd, &r);
	zxgq_send_response(chan->xgq_hdl, &r);
	kfree(cmd);
}

static int zrpu_channel_probe(struct platform_device *pdev)
{
	const char *mem_res_name = "xlnx,xgq_buffer";
	const char *xgq_res_name = "xlnx,xgq_device";
	struct device_node *np = NULL;
	struct resource res = {};
	struct zocl_rpu_channel *chan;
	struct zocl_xgq_init_args xgq_arg = {};
	int ret;
	u32 irq;

	chan = devm_kzalloc(&pdev->dev, sizeof(*chan), GFP_KERNEL);
	if (!chan)
		return -ENOMEM;

	chan->pdev = pdev;
	platform_set_drvdata(pdev, chan);

	INIT_LIST_HEAD(&chan->data_list);

	/* Discover and init shared ring buffer. */
	chan->mem_base = zlib_map_phandle_res_by_name(ZCHAN2PDEV(chan), mem_res_name,
						      &chan->mem_start, &chan->mem_size);
	if (!chan->mem_base) {
		zchan_err(chan, "failed to find channel buffer");
		return -EINVAL;
	}
	reg_write(chan->mem_base, ZRPU_CHANNEL_XGQ_OFF, ZRPU_CHANNEL_XGQ_BUFFER);

	/* Discover and init XGQ. */
	ret = of_count_phandle_with_args(ZCHAN2DEV(chan)->of_node, xgq_res_name, NULL);
	if (ret <= 0) {
		zchan_err(chan, "failed to find RPU channel XGQ");
		return -EINVAL;
	}
	if (ret != 1) {
		zchan_info(chan, "found > 1 XGQs, only use the first one");
	}
	np = of_parse_phandle(ZCHAN2DEV(chan)->of_node, xgq_res_name, 0);
	if (!np) {
		zchan_err(chan, "failed to find node for XGQ");
		return -EINVAL;
	}
	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		zchan_err(chan, "failed to find res for XGQ: %d", ret);
		return -EINVAL;
	}
	irq = of_irq_get(np, 0);
	zchan_info(chan, "Found XGQ @ %pR on irq %d", &res, irq);
	chan->xgq_base = zlib_map_res(ZCHAN2DEV(chan), &res, NULL, NULL);
	if (!chan->xgq_base) {
		zchan_err(chan, "failed to map XGQ IP");
		return -EINVAL;
	}

	ret = sysfs_create_group(&ZCHAN2DEV(chan)->kobj, &zrpu_channel_attrgroup);
	if (ret) {
		zchan_err(chan, "failed to create sysfs: %d", ret);
		return ret;
	}

	/* Bringup INTC sub-dev to handle interrupts for this XGQ. */
	ret = zocl_ert_create_intc(ZCHAN2DEV(chan), &irq, 1, 0,
				   ERT_XGQ_INTC_DEV_NAME, &chan->intc_pdev);
	if (ret) {
		zchan_err(chan, "Failed to create xgq intc device: %d", ret);
		goto err_intc;
	}

	/* Bringup the XGQ. */
	xgq_arg.zxia_pdev = ZCHAN2PDEV(chan);
	xgq_arg.zxia_ring = chan->mem_base + ZRPU_CHANNEL_XGQ_BUFFER;
	xgq_arg.zxia_ring_size = ZRPU_CHANNEL_XGQ_BUFFER_SIZE;
	xgq_arg.zxia_ring_slot_size = ZRPU_CHANNEL_XGQ_SLOT_SIZE;
	/* There is only 1 irq in intc subdev. The irq_id is 0 */
	xgq_arg.zxia_irq = 0;
	xgq_arg.zxia_intc_pdev = chan->intc_pdev;
	xgq_arg.zxia_xgq_ip = chan->xgq_base;
	xgq_arg.zxia_cmd_handler = zchan_cmd_handler;
	chan->xgq_hdl = zxgq_init(&xgq_arg);
	if (!chan->xgq_hdl) {
		zchan_err(chan, "failed to initialize XGQ");
		goto err_xgq;
	}

	return 0;

err_xgq:
	zocl_ert_destroy_intc(chan->intc_pdev);
err_intc:
	sysfs_remove_group(&pdev->dev.kobj, &zrpu_channel_attrgroup);
	return -EINVAL;
};
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void zrpu_channel_remove(struct platform_device *pdev)
#else
static int zrpu_channel_remove(struct platform_device *pdev)
#endif
{
	struct zocl_rpu_channel *chan = platform_get_drvdata(pdev);

	if (chan->xgq_hdl)
		zxgq_fini(chan->xgq_hdl);
	zocl_ert_destroy_intc(chan->intc_pdev);
	sysfs_remove_group(&pdev->dev.kobj, &zrpu_channel_attrgroup);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	return 0;
#endif
};

struct platform_driver zocl_rpu_channel_driver = {
	.driver = {
		.name = ZRPU_CHANNEL_NAME,
		.of_match_table = zocl_rpu_channel_of_match,
	},
	.probe = zrpu_channel_probe,
	.remove = zrpu_channel_remove,
};
