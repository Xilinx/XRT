/*
 * A GEM style device manager for PCIe based OpenCL accelerators.
 *
 * Copyright (C) 2018 Xilinx, Inc. All rights reserved.
 *
 * Authors: Chien-Wei Lan <chienwei@xilinx.com>
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
#include "mgmt-ioctl.h"

/* Registers are defined in pg150-ultrascale-memory-ip.pdf:
 * AXI4-Lite Slave Control/Status Register Map
 */

struct xcl_mig_cache {
	u64			cache_expire_secs;
	ktime_t			cache_expires;
	struct xcl_mig_ecc	cache[0];
};

static void set_mig_cache_data(struct xcl_mig_cache *mig_cache, struct xcl_mig_ecc *mig_ecc)
{
	memcpy(&mig_cache->cache, mig_ecc, sizeof(struct xcl_mig_ecc)*MAX_M_COUNT);
	mig_cache->cache_expires = ktime_add(ktime_get_boottime(),
		ktime_set(mig_cache->cache_expire_secs, 0));
}

static void mig_cache_read_from_peer(struct platform_device *pdev)
{
	xdev_handle_t xdev = xocl_get_xdev(pdev);
	struct xcl_mig_cache *mig_cache = platform_get_drvdata(pdev);
	struct mailbox_subdev_peer subdev_peer = {0};
	struct xcl_mig_ecc *mig_ecc = NULL;
	size_t resp_len = sizeof(struct xcl_mig_ecc)*MAX_M_COUNT;
	size_t data_len = sizeof(struct mailbox_subdev_peer);
	struct mailbox_req *mb_req = NULL;
	size_t reqlen = sizeof(struct mailbox_req) + data_len;

	mb_req = vmalloc(reqlen);
	if (!mb_req)
		return;

	mig_ecc = vzalloc(resp_len);
	if (!mig_ecc)
		return;

	mb_req->req = MAILBOX_REQ_PEER_DATA;
	subdev_peer.entry_size = sizeof(struct xcl_mig_ecc);
	subdev_peer.kind = MIG_ECC;
	subdev_peer.entries = MAX_M_COUNT;

	memcpy(mb_req->data, &subdev_peer, data_len);

	(void) xocl_peer_request(xdev,
		mb_req, reqlen, mig_ecc, &resp_len, NULL, NULL);
	set_mig_cache_data(mig_cache, mig_ecc);

	vfree(mig_ecc);
	vfree(mb_req);
}

static int mig_cache_get_data(struct platform_device *pdev, void *buf)
{
	struct xcl_mig_cache *mig_cache = platform_get_drvdata(pdev);
	struct xcl_mig_ecc *mig_ecc = (struct xcl_mig_ecc *)buf;
	ktime_t now = ktime_get_boottime();
	enum MEM_TYPE mem_type = mig_ecc->mem_type;
	uint64_t memidx = mig_ecc->mem_idx;
	int i, ret = -ENODATA;

	if (ktime_compare(now, mig_cache->cache_expires) > 0)
		mig_cache_read_from_peer(pdev);

	for (i = 0; i < MAX_M_COUNT; ++i) {
		struct xcl_mig_ecc *cur = &mig_cache->cache[i];

		if (cur->mem_type != mem_type)
			continue;
		if (cur->mem_idx != memidx)
			continue;
		memcpy(mig_ecc, cur, sizeof(struct xcl_mig_ecc));
		ret = 0;
		break;
	}
	return ret;
}

static struct xocl_mig_cache_funcs mig_cache_ops = {
	.get_data	= mig_cache_get_data,
};

static int mig_cache_probe(struct platform_device *pdev)
{
	struct xcl_mig_cache *mig_cache;

	mig_cache = devm_kzalloc(&pdev->dev, sizeof(struct xcl_mig_cache)+sizeof(struct xcl_mig_ecc)*MAX_M_COUNT, GFP_KERNEL);

	if (!mig_cache)
		return -ENOMEM;

	platform_set_drvdata(pdev, mig_cache);

	xocl_subdev_register(pdev, XOCL_SUBDEV_MIG_CACHE, &mig_cache_ops);
	mig_cache->cache_expire_secs = 1;
	return 0;
}


static int mig_cache_remove(struct platform_device *pdev)
{
	struct xcl_mig_cache *mig_cache;

	mig_cache = platform_get_drvdata(pdev);
	if (!mig_cache) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, mig_cache);
	return 0;
}

struct platform_device_id mig_cache_id_table[] = {
	{ XOCL_MIG_CACHE, 0 },
	{ },
};

static struct platform_driver	mig_cache_driver = {
	.probe		= mig_cache_probe,
	.remove		= mig_cache_remove,
	.driver		= {
		.name = XOCL_MIG_CACHE,
	},
	.id_table = mig_cache_id_table,
};

int __init xocl_init_mig_cache(void)
{
	return platform_driver_register(&mig_cache_driver);
}

void xocl_fini_mig_cache(void)
{
	platform_driver_unregister(&mig_cache_driver);
}
