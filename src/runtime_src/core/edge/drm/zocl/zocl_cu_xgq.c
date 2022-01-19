/* SPDX-License-Identifier: GPL-2.0 OR Apache-2.0 */
/*
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 *
 * Author(s):
 *        Max Zhen <maxz@xilinx.com>
 *
 * This file is dual-licensed; you may select either the GNU General Public
 * License version 2 or Apache License, Version 2.0.
 */

#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include "zocl_util.h"
#include "zocl_drv.h"
#include "zocl_xgq.h"
#include "zocl_cu_xgq.h"
/* CU XGQ driver name. */
#define ZCU_XGQ_NAME "zocl_cu_xgq"

#define ZCU_XGQ2PDEV(zcu_xgq)			((zcu_xgq)->zxc_pdev)
#define ZCU_XGQ2DEV(zcu_xgq)			(&ZCU_XGQ2PDEV(zcu_xgq)->dev)
#define zcu_xgq_err(zcu_xgq, fmt, args...)	zocl_err(ZCU_XGQ2DEV(zcu_xgq), fmt"\n", ##args)
#define zcu_xgq_info(zcu_xgq, fmt, args...)	zocl_info(ZCU_XGQ2DEV(zcu_xgq), fmt"\n", ##args)
#define zcu_xgq_dbg(zcu_xgq, fmt, args...)	zocl_dbg(ZCU_XGQ2DEV(zcu_xgq), fmt"\n", ##args)

#define ZCU_XGQ_MAX_SLOT_SIZE	1024
#define ZCU_XGQ_FAST_PATH(zcu_xgq)		((zcu_xgq)->zxc_num_cu == 1)
//#define ZCU_XGQ_FAST_PATH(zcu_xgq)		(false)

//#define ZCU_XGQ_DEBUG

static void zcu_xgq_cmd_handler(struct platform_device *pdev, struct xgq_cmd_sq_hdr *cmd);

#ifdef ZCU_XGQ_DEBUG
#include <linux/circ_buf.h>

struct log_ring {
	char			*lr_buf;
	size_t			lr_size;
	size_t			lr_head;
	size_t			lr_tail;
};

int lr_init(struct log_ring *lr, size_t size)
{
	lr->lr_buf = vzalloc(size);
	if (!lr->lr_buf)
		return -ENOMEM;

	lr->lr_size = size;
	lr->lr_head = 0;
	lr->lr_tail = 0;
	return 0;
}

void lr_fini(struct log_ring *lr)
{
	vfree(lr->lr_buf);
}

void lr_produce(struct log_ring *lr, char *log, size_t size)
{
	size_t head = lr->lr_head;
	size_t tail = lr->lr_tail;
	size_t space = CIRC_SPACE(head, tail, lr->lr_size);
	/* return min(head pointer to the end of buffer, CIRC_SPACE()) */
	size_t space_to_end = CIRC_SPACE_TO_END(head, tail, lr->lr_size);

	if (!lr || !lr->lr_buf || !log)
		return;

	if (size > space) {
		tail = (tail + size - space) & (lr->lr_size - 1);
		space = CIRC_SPACE(head, tail, lr->lr_size);
		space_to_end = CIRC_SPACE_TO_END(head, tail, lr->lr_size);
		lr->lr_tail = tail;
	}
	/* Copy data to buffer. Depende on if it cross the end of the ring
	 * buffer, there are two situations.
	 */
	if (space_to_end != space) {
		memcpy(lr->lr_buf + head, log, space_to_end);
		memcpy(lr->lr_buf, log + space_to_end, size - space_to_end);
	} else
		memcpy(lr->lr_buf + head, log, size);

	 /* Update head/tail pointer. We might overwrite oldest data, this is
	  * why tail pointer needs updat as well
	  */
	lr->lr_head = (lr->lr_head + size) & (lr->lr_size - 1);
}

ssize_t lr_consume(struct log_ring *lr, char *buf, size_t size)
{
	size_t head = lr->lr_head;
	size_t tail = lr->lr_tail;
	size_t cnt = CIRC_CNT(head, tail, lr->lr_size);
	/* return min(count to the end of buffer, CIRC_CNT()) */
	size_t cnt_to_end = CIRC_CNT_TO_END(head, tail, lr->lr_size);
	size_t nread = 0;

	if (!lr || !lr->lr_buf || !buf)
		return 0;

	if (size < cnt)
		nread = size;
	else
		nread = cnt;

	if (nread <= cnt_to_end) {
		memcpy(buf, lr->lr_buf + tail, nread);
		lr->lr_tail += nread;
	} else {
		/* Cross the end of the buffer, two times of copy */
		memcpy(buf, lr->lr_buf + tail, cnt_to_end);
		memcpy(buf + cnt_to_end, lr->lr_buf, nread - cnt_to_end);
		lr->lr_tail = nread - cnt_to_end;
	}

	return nread;
}
#endif

struct zocl_cu_xgq {
	struct platform_device	*zxc_pdev;
	struct zocl_cu_xgq_info	*zxc_pdata;

	void			*zxc_zxgq_hdl;
	void			*zxc_client_hdl;
	struct drm_zocl_dev	*zxc_zdev;

	struct mutex		zxc_lock;
	u32			zxc_cu_idx;
	size_t			zxc_num_cu;

	u32			zxc_irq;
	void __iomem		*zxc_ring;
	size_t			zxc_ring_size;
	void __iomem		*zxc_sq_prod;
	void __iomem		*zxc_cq_prod;
	void __iomem		*zxc_cq_prod_int;
#ifdef ZCU_XGQ_DEBUG
	struct log_ring		zxc_log;
#endif
};

static ssize_t
debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct zocl_cu_xgq *zcu_xgq = (struct zocl_cu_xgq *)dev_get_drvdata(dev);
	struct kds_client *client;
	ssize_t sz;

	sz = sprintf(buf, "zcu_xgq %p\n", zcu_xgq);

	client = zcu_xgq->zxc_client_hdl;
	sz += sprintf(buf+sz, "s_cnt %ld\n", client_stat_read(client, s_cnt[0]));
	sz += sprintf(buf+sz, "c_cnt %ld\n", client_stat_read(client, c_cnt[0]));

	return sz;
}
static DEVICE_ATTR_RO(debug);

static struct attribute *zcu_xgq_attrs[] = {
	&dev_attr_debug.attr,
	NULL,
};

static ssize_t
xgq_ring(struct file *filp, struct kobject *kobj,
	 struct bin_attribute *attr, char *buf,
	 loff_t offset, size_t count)
{
	struct zocl_cu_xgq *zcu_xgq;
	struct device *dev = container_of(kobj, struct device, kobj);
	ssize_t nread = 0;
	size_t size = 0;

	zcu_xgq = (struct zocl_cu_xgq *)dev_get_drvdata(dev);
	if (!zcu_xgq || !zcu_xgq->zxc_ring)
		return 0;

	size = zcu_xgq->zxc_ring_size;
	if (offset >= size)
		goto done;

	if (offset + count < size)
		nread = count;
	else
		nread = size - offset;

	memcpy_fromio(buf, zcu_xgq->zxc_ring + offset, nread);

done:
	return nread;
}

static struct bin_attribute ring_attr = {
	.attr = {
		.name ="xgq_ring",
		.mode = 0444
	},
	.read = xgq_ring,
	.write = NULL,
	.size = 0
};

#ifdef ZCU_XGQ_DEBUG
static ssize_t
cmd_log_show(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf,
	loff_t offset, size_t count)
{
	struct zocl_cu_xgq *zcu_xgq;
	struct device *dev = container_of(kobj, struct device, kobj);

	zcu_xgq = (struct zocl_cu_xgq *)dev_get_drvdata(dev);
	if (!zcu_xgq)
		return 0;

	return lr_consume(&zcu_xgq->zxc_log, buf, count);
}

static struct bin_attribute log_attr = {
	.attr = {
		.name ="cmd_log",
		.mode = 0444
	},
	.read = cmd_log_show,
	.write = NULL,
	.size = 0
};
#endif

static struct bin_attribute *zcu_xgq_bin_attrs[] = {
	&ring_attr,
#ifdef ZCU_XGQ_DEBUG
	&log_attr,
#endif
	NULL,
};

static const struct attribute_group zcu_xgq_attrgroup = {
	.attrs = zcu_xgq_attrs,
	.bin_attrs = zcu_xgq_bin_attrs,
};

static inline void reg_write(void __iomem  *addr, u32 val)
{
	iowrite32(val, addr);
}

static inline u32 reg_read(void __iomem *addr)
{
	return ioread32(addr);
}

static void __iomem *zcu_xgq_map_res(struct zocl_cu_xgq *zcu_xgq, const char *name, size_t *szp)
{
	struct resource *res;
	void __iomem *map;

	res = platform_get_resource_byname(ZCU_XGQ2PDEV(zcu_xgq), IORESOURCE_MEM, name);
	if (!res) {
		zcu_xgq_err(zcu_xgq, "res not found: %s", name);
		return NULL;
	}
	zcu_xgq_info(zcu_xgq, "%s range: %pR", name, res);

	map = devm_ioremap(ZCU_XGQ2DEV(zcu_xgq), res->start, res->end - res->start + 1);
	if (IS_ERR(map)) {
		zcu_xgq_err(zcu_xgq, "Failed to map res: %s: %ld", name, PTR_ERR(map));
		return NULL;
	}

	if (szp)
		*szp = res->end - res->start + 1;
	return map;
}

static void zcu_xgq_init_xgq(struct zocl_cu_xgq *zcu_xgq)
{
	struct zocl_xgq_init_args arg = {};

	arg.zxia_pdev = zcu_xgq->zxc_pdev;
	arg.zxia_irq = zcu_xgq->zxc_irq;
	arg.zxia_ring = zcu_xgq->zxc_ring;
	arg.zxia_ring_size = zcu_xgq->zxc_ring_size;
	arg.zxia_ring_slot_size = zcu_xgq->zxc_pdata->zcxi_slot_size;
	arg.zxia_intc_pdev = zcu_xgq->zxc_pdata->zcxi_intc_pdev;
	if (ZCU_XGQ_MAX_SLOT_SIZE < arg.zxia_ring_slot_size)
		arg.zxia_ring_slot_size = ZCU_XGQ_MAX_SLOT_SIZE;
	arg.zxia_sq_prod = zcu_xgq->zxc_sq_prod;
	arg.zxia_cq_prod = zcu_xgq->zxc_cq_prod;
	arg.zxia_cq_prod_int = zcu_xgq->zxc_cq_prod_int;
	arg.zxia_cmd_handler = zcu_xgq->zxc_pdata->zcxi_echo_mode ? NULL : zcu_xgq_cmd_handler;
	arg.zxia_simple_cmd_hdr = ZCU_XGQ_FAST_PATH(zcu_xgq);

	/* Init CU XGQ */
	zcu_xgq->zxc_zxgq_hdl = zxgq_init(&arg);
	if (!zcu_xgq->zxc_zxgq_hdl)
		zcu_xgq_err(zcu_xgq, "failed to initialize CU XGQ");
}

static void zcu_xgq_fini_xgq(struct zocl_cu_xgq *zcu_xgq)
{
	if (zcu_xgq->zxc_zxgq_hdl)
		zxgq_fini(zcu_xgq->zxc_zxgq_hdl);
	zcu_xgq->zxc_zxgq_hdl = NULL;
}

static int zcu_xgq_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct zocl_cu_xgq *zcu_xgq = devm_kzalloc(&pdev->dev, sizeof(*zcu_xgq), GFP_KERNEL);
	int ret;

	if (!zcu_xgq)
		return -ENOMEM;
	zcu_xgq->zxc_pdev = pdev;

	res = platform_get_resource_byname(ZCU_XGQ2PDEV(zcu_xgq), IORESOURCE_IRQ, ZCX_RES_IRQ);
	if (!res) {
		zcu_xgq_err(zcu_xgq, "failed to find CU XGQ IRQ"); 
		return -EINVAL;
	}
	zcu_xgq->zxc_irq = res->start;
	zcu_xgq_info(zcu_xgq, "CU XGQ IRQ: %d", zcu_xgq->zxc_irq); 

	zcu_xgq->zxc_pdata = dev_get_platdata(ZCU_XGQ2DEV(zcu_xgq));
	BUG_ON(zcu_xgq->zxc_pdata == NULL);

	zcu_xgq->zxc_ring = zcu_xgq_map_res(zcu_xgq, ZCX_RES_RING, &zcu_xgq->zxc_ring_size);
	if (!zcu_xgq->zxc_ring)
		return -EINVAL;

	ret = zocl_create_client(ZCU_XGQ2DEV(zcu_xgq), &zcu_xgq->zxc_client_hdl);
	if (ret)
		return ret;

	zcu_xgq->zxc_sq_prod = zcu_xgq_map_res(zcu_xgq, ZCX_RES_SQ_PROD, NULL);
	zcu_xgq->zxc_cq_prod = zcu_xgq_map_res(zcu_xgq, ZCX_RES_CQ_PROD, NULL);
	zcu_xgq->zxc_cq_prod_int = zcu_xgq_map_res(zcu_xgq, ZCX_RES_CQ_PROD_INT, NULL);

	zcu_xgq->zxc_zdev = zocl_get_zdev();
	mutex_init(&zcu_xgq->zxc_lock);
	platform_set_drvdata(pdev, zcu_xgq);

#ifdef ZCU_XGQ_DEBUG
	ret = lr_init(&zcu_xgq->zxc_log, 4 * 1024 * 1024);
	if (ret)
		zcu_xgq_info(zcu_xgq, "create ZCU_XGQ log buffer failed");
#endif

	ret = sysfs_create_group(&pdev->dev.kobj, &zcu_xgq_attrgroup);
	if (ret)
		zcu_xgq_err(zcu_xgq, "create ZCU_XGQ attrs failed: %d", ret);
	return 0;
}

static int zcu_xgq_remove(struct platform_device *pdev)
{
	struct zocl_cu_xgq *zcu_xgq = platform_get_drvdata(pdev);

	zcu_xgq_info(zcu_xgq, "Removing %s", ZCU_XGQ_NAME);

	sysfs_remove_group(&pdev->dev.kobj, &zcu_xgq_attrgroup);

#ifdef ZCU_XGQ_DEBUG
	lr_fini(&zcu_xgq->zxc_log);
#endif

	mutex_destroy(&zcu_xgq->zxc_lock);
	if (zcu_xgq->zxc_client_hdl)
		zocl_destroy_client(zcu_xgq->zxc_client_hdl);
	zcu_xgq_fini_xgq(zcu_xgq);
	return 0;
}

static const struct platform_device_id zocl_cu_xgq_id_match[] = {
	{ CU_XGQ_DEV_NAME, 0 },
	{ /* end of table */ },
};

struct platform_driver zocl_cu_xgq_driver = {
	.driver = {
		.name = ZCU_XGQ_NAME,
	},
	.probe  = zcu_xgq_probe,
	.remove = zcu_xgq_remove,
	.id_table = zocl_cu_xgq_id_match,
};

int zcu_xgq_assign_cu(struct platform_device *pdev, u32 cu_idx)
{
	int rc;
	struct zocl_cu_xgq *zcu_xgq = platform_get_drvdata(pdev);

	mutex_lock(&zcu_xgq->zxc_lock);
	zcu_xgq->zxc_num_cu++;
	/* For optimization when there is only 1 CU. */
	zcu_xgq->zxc_cu_idx = cu_idx;
	rc = zocl_add_context_kernel(zcu_xgq->zxc_zdev, zcu_xgq->zxc_client_hdl,
				     cu_idx, CU_CTX_SHARED);
	if (!rc) {
		/* Re-init xgq since we may have > 1 CU assigned so can't use fast path anymore. */
		zcu_xgq_fini_xgq(zcu_xgq);
		zcu_xgq_init_xgq(zcu_xgq);
	}
	mutex_unlock(&zcu_xgq->zxc_lock);

	zcu_xgq_info(zcu_xgq, "CU[%d] assigned", cu_idx);
	return rc;
}

int zcu_xgq_unassign_cu(struct platform_device *pdev, u32 cu_idx)
{
	int rc;
	struct zocl_cu_xgq *zcu_xgq;

	BUG_ON(pdev == NULL);
	zcu_xgq = platform_get_drvdata(pdev);

	BUG_ON(zcu_xgq == NULL);
	mutex_lock(&zcu_xgq->zxc_lock);
	zcu_xgq->zxc_num_cu--;
	rc = zocl_del_context_kernel(zcu_xgq->zxc_zdev, zcu_xgq->zxc_client_hdl, cu_idx);
	mutex_unlock(&zcu_xgq->zxc_lock);
	return rc;
}

static void init_resp(struct xgq_com_queue_entry *resp, u16 cid, u32 rcode)
{
	memset(resp, 0, sizeof(*resp));
	resp->hdr.cid = cid;
	resp->hdr.cstate = XGQ_CMD_STATE_COMPLETED;
	resp->rcode = rcode;
}

static inline void
zcu_xgq_cmd_complete(struct platform_device *pdev, struct xgq_cmd_sq_hdr *cmd, int ret)
{
	struct xgq_com_queue_entry r;
	struct zocl_cu_xgq *zcu_xgq = platform_get_drvdata(pdev);

	if (likely(ret == 0 && ZCU_XGQ_FAST_PATH(zcu_xgq))) {
		zxgq_send_response(zcu_xgq->zxc_zxgq_hdl, NULL);
	} else {
		init_resp(&r, cmd->cid, ret);
		zxgq_send_response(zcu_xgq->zxc_zxgq_hdl, &r);
	}
	kfree(cmd);
}

static void zcu_xgq_cmd_notify(struct kds_command *xcmd, int status)
{
	struct zocl_cu_xgq *zcu_xgq = (struct zocl_cu_xgq *)xcmd->priv;
	struct xgq_cmd_sq_hdr *cmd = xcmd->info;

	xcmd->info = NULL;
	zcu_xgq_cmd_complete(ZCU_XGQ2PDEV(zcu_xgq), cmd, 0);

	if (xcmd->cu_idx >= 0)
		client_stat_inc(xcmd->client, c_cnt[xcmd->cu_idx]);
}

static inline void
zcu_xgq_cmd_start_cuidx(struct zocl_cu_xgq *zcu_xgq, struct xgq_cmd_sq_hdr *cmd)
{
	struct kds_command *xcmd;
	int mask_idx;
	int bit_idx;

#if 0
	zcu_xgq_cmd_complete(ZCU_XGQ2PDEV(zcu_xgq), cmd, 0);
	return;
#endif
	xcmd = kds_alloc_command(zcu_xgq->zxc_client_hdl, 0);
	if (!xcmd) {
		zcu_xgq_cmd_complete(ZCU_XGQ2PDEV(zcu_xgq), cmd, -ENOMEM);
		return;
	}

	xcmd->info = cmd;
	xcmd->payload_type = XGQ_CMD;

	xcmd->cb.notify_host = zcu_xgq_cmd_notify;
	xcmd->cb.free = kds_free_command;
	xcmd->priv = zcu_xgq;
	xcmd->type = KDS_CU;
	xcmd->opcode = OP_START;
	xcmd->response_size = 0;

	if (ZCU_XGQ_FAST_PATH(zcu_xgq)) {
		mask_idx = zcu_xgq->zxc_cu_idx / 32;
		bit_idx = zcu_xgq->zxc_cu_idx % 32;
	} else {
		mask_idx = cmd->cu_idx / 32;
		bit_idx = cmd->cu_idx % 32;
	}
	xcmd->cu_mask[mask_idx] = 1 << bit_idx;
	if (mask_idx < 4) {
		xcmd->num_mask = mask_idx + 1;
	} else {
		xcmd->num_mask = 0;
	}

	kds_add_command(&zcu_xgq->zxc_zdev->kds, xcmd);
}

static void zcu_xgq_cmd_default(struct zocl_cu_xgq *zcu_xgq, struct xgq_cmd_sq_hdr *cmd)
{
	zcu_xgq_err(zcu_xgq, "Unknown cmd: %d", cmd->opcode);
	zcu_xgq_cmd_complete(ZCU_XGQ2PDEV(zcu_xgq), cmd, -ENOTTY);
}

static void zcu_xgq_cmd_handler(struct platform_device *pdev, struct xgq_cmd_sq_hdr *cmd)
{
	struct zocl_cu_xgq *zcu_xgq = platform_get_drvdata(pdev);

#ifdef ZCU_XGQ_DEBUG
	lr_produce(&zcu_xgq->zxc_log, (char *)cmd, sizeof(*cmd));
#endif

	switch (cmd->opcode) {
	case XGQ_CMD_OP_START_CUIDX:
		zcu_xgq_dbg(zcu_xgq, "XGQ_CMD_OP_START_CUIDX received");
		zcu_xgq_cmd_start_cuidx(zcu_xgq, cmd);
		break;
	default:
		zcu_xgq_cmd_default(zcu_xgq, cmd);
		break;
	}
}
