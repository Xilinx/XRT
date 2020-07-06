// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver XCLBIN parser
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 */

#include "xrt_xclbin.h"
#include "xocl_drv.h"

struct xocl_xclbin_ops {
	int (*xclbin_pre_download)(xdev_handle_t xdev, void *args);
	int (*xclbin_download)(xdev_handle_t xdev, void *args);
	int (*xclbin_post_download)(xdev_handle_t xdev, void *args);
};

struct xclbin_arg {
	xdev_handle_t 		xdev;
	struct axlf 		*xclbin;
	struct xocl_subdev 	*urpdevs;
	int 			num_dev;
};

static int versal_xclbin_pre_download(xdev_handle_t xdev, void *args)
{
	struct xclbin_arg *arg = (struct xclbin_arg *)args;
	struct axlf *xclbin = arg->xclbin;
	void *metadata = NULL;	
	uint64_t size;
	int ret = 0;

	ret = xrt_xclbin_get_section(xclbin, PARTITION_METADATA, &metadata, &size);
	if (ret)
		return ret;

	if (metadata) {
		arg->num_dev = xocl_fdt_parse_blob(xdev, metadata,
		    size, &(arg->urpdevs));
		vfree(metadata);
	}
	xocl_subdev_destroy_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);

	return ret;
}

static int versal_xclbin_download(xdev_handle_t xdev, void *args)
{
	struct xclbin_arg *arg = (struct xclbin_arg *)args;
	int ret = 0;

	BUG_ON(!arg->xclbin);

	xocl_axigate_freeze(xdev, XOCL_SUBDEV_LEVEL_PRP);

	/* download bitstream */
	ret = xocl_xfer_versal_download_axlf(xdev, arg->xclbin);

	xocl_axigate_free(xdev, XOCL_SUBDEV_LEVEL_PRP);

	return ret;
}

static int versal_xclbin_post_download(xdev_handle_t xdev, void *args)
{
	struct xclbin_arg *arg = (struct xclbin_arg *)args;
	int i, ret = 0;

	if (arg->num_dev) {
		for (i = 0; i < arg->num_dev; i++)
			(void) xocl_subdev_create(xdev, &(arg->urpdevs[i].info));
		xocl_subdev_create_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);
	}

	return ret;

}

static struct xocl_xclbin_ops versal_ops = {
	.xclbin_pre_download 	= versal_xclbin_pre_download,
	.xclbin_download 	= versal_xclbin_download,
	.xclbin_post_download 	= versal_xclbin_post_download,
};

#if 0
/* Place holder for icap callbacks */
static int icap_xclbin_pre_download(xdev_handle_t xdev, void *args)
{ return 0; }
static int icap_xclbin_download(xdev_handle_t xdev, void *args)
{ return 0; }
static int icap_xclbin_post_download(xdev_handle_t xdev, void *args)
{ return 0; }

static struct xocl_xclbin_ops icap_ops = {
	.xclbin_pre_download 	= icap_xclbin_pre_download,
	.xclbin_download 	= icap_xclbin_download,
	.xclbin_post_download 	= icap_xclbin_post_download,
};

/*
 *Future enhancement for binding info table to of_device_id with
 *device-tree compatible
 */
static const struct xocl_xclbin_info icap_info = {
	.ops = &icap_ops;
};

static const struct xocl_xclbin_info versal_info = {
	.ops = &versal_ops;
};
#endif

static int xocl_xclbin_download_impl(xdev_handle_t xdev, const void *xclbin,
	struct xocl_xclbin_ops *ops)
{
	/* args are simular, thus using the same pattern among all ops*/
	struct xclbin_arg args = {
		.xdev = xdev,
		.xclbin = (struct axlf *)xclbin,
		.num_dev = 0,
	};
	int ret = 0;	

	/* Step1: call pre download callback */
	if (ops->xclbin_pre_download) {
		ret = ops->xclbin_pre_download(xdev, &args);
		if (ret)
			goto done;
	}

	/* Step2: there must be a download callback */
	if (!ops->xclbin_download) {
		ret = -EINVAL;
		goto done;
	}
	ret = ops->xclbin_download(xdev, &args);
	if (ret)
		goto done;

	/* Step3: call post download callback */
	if (ops->xclbin_post_download) {
		ret = ops->xclbin_post_download(xdev, &args);
	}

done:	
	return ret;
}

int xocl_xclbin_download(xdev_handle_t xdev, const void *xclbin)
{
	if (XOCL_DSA_IS_VERSAL(xdev))
		return xocl_xclbin_download_impl(xdev, xclbin, &versal_ops);
	else
		/* TODO: return xocl_xclbin_download_impl(xdev, xclbin, &icap_ops); */
		return xocl_icap_download_axlf(xdev, xclbin);
}
