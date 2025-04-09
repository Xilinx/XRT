// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver XCLBIN parser
 *
 * Copyright (C) 2020-2021 Xilinx, Inc.
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 */

#include "xocl_xclbin.h"

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
	uint32_t 		slot_id;
};

static int versal_xclbin_pre_download(xdev_handle_t xdev, void *args)
{
	struct xclbin_arg *arg = (struct xclbin_arg *)args;
	struct axlf *xclbin = arg->xclbin;
	void *metadata = NULL;
	uint64_t size;
	int ret = 0;

	/* PARTITION_METADATA is not present for FLAT shells */
	if (xclbin->m_header.m_mode == XCLBIN_FLAT)
		return 0;

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
	struct axlf *xclbin = arg->xclbin;
	int ret = 0;

	BUG_ON(!arg->xclbin);

	if (xclbin->m_header.m_mode == XCLBIN_FLAT) {
		xocl_info(&XDEV(xdev)->pdev->dev,
		    "xclbin is generated for flat shell, dont need to load PDI");
		return ret;
	}

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
		const struct axlf_section_header *hdr =
		    xrt_xclbin_get_section_hdr(arg->xclbin, CLOCK_FREQ_TOPOLOGY);
		struct clock_freq_topology *topo;

		for (i = 0; i < arg->num_dev; i++) {
			(void) xocl_subdev_create(xdev, &(arg->urpdevs[i].info));
			xocl_subdev_dyn_free(arg->urpdevs + i);
		}
		xocl_subdev_create_by_level(xdev, XOCL_SUBDEV_LEVEL_URP);

		if (hdr) {
		        if (hdr->m_sectionSize < sizeof(struct clock_freq_topology)) {
                            return -EINVAL;
		        }
			/* after download, update clock freq */
			topo = (struct clock_freq_topology *)
			    (((char *)(arg->xclbin)) + hdr->m_sectionOffset);
			ret = xocl_clock_freq_scaling_by_topo(xdev, topo, 0);
		}
	}

	if (arg->urpdevs)
		kfree(arg->urpdevs);

	return ret;
}

static int mpsoc_xclbin_pre_download(xdev_handle_t xdev, void *args)
{
	return 0;
}

static int mpsoc_xclbin_download(xdev_handle_t xdev, void *args)
{
	struct xclbin_arg *arg = (struct xclbin_arg *)args;
	int ret;

	ret = xocl_xfer_versal_download_axlf(xdev, arg->xclbin);

	return ret;
}

static int mpsoc_xclbin_post_download(xdev_handle_t xdev, void *args)
{
	return 0;
}

static int xgq_xclbin_pre_download(xdev_handle_t xdev, void *args)
{
	return 0;
}

static int xgq_xclbin_download(xdev_handle_t xdev, void *args)
{
	struct xclbin_arg *arg = (struct xclbin_arg *)args;
	int ret;

	ret = xocl_xgq_download_axlf_slot(xdev, arg->xclbin, arg->slot_id);

	return ret;
}

static int xgq_xclbin_post_download(xdev_handle_t xdev, void *args)
{
	struct xclbin_arg *arg = (struct xclbin_arg *)args;
	const struct axlf_section_header *hdr =
	    xrt_xclbin_get_section_hdr(arg->xclbin, CLOCK_FREQ_TOPOLOGY);
	struct clock_freq_topology *topo;
	int ret = 0;

	if (hdr) {
                if (hdr->m_sectionSize < sizeof(struct clock_freq_topology)) {
	           return -EINVAL;
                }
		/* after download, update clock freq */
		topo = (struct clock_freq_topology *)
		    (((char *)(arg->xclbin)) + hdr->m_sectionOffset);
		ret = xocl_xgq_clk_scaling_by_topo(xdev, topo, 1);
	}

	return ret;
}

static struct xocl_xclbin_ops versal_ops = {
	.xclbin_pre_download 	= versal_xclbin_pre_download,
	.xclbin_download 	= versal_xclbin_download,
	.xclbin_post_download 	= versal_xclbin_post_download,
};

static struct xocl_xclbin_ops mpsoc_ops = {
	.xclbin_pre_download 	= mpsoc_xclbin_pre_download,
	.xclbin_download 	= mpsoc_xclbin_download,
	.xclbin_post_download 	= mpsoc_xclbin_post_download,
};

static struct xocl_xclbin_ops xgq_ops = {
	.xclbin_pre_download 	= xgq_xclbin_pre_download,
	.xclbin_download 	= xgq_xclbin_download,
	.xclbin_post_download 	= xgq_xclbin_post_download,
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
	uint32_t slot_id, struct xocl_xclbin_ops *ops)
{
	/* args are simular, thus using the same pattern among all ops*/
	struct xclbin_arg args = {
		.xdev = xdev,
		.xclbin = (struct axlf *)xclbin,
		.num_dev = 0,
		.slot_id = slot_id,
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

int xocl_xclbin_download(xdev_handle_t xdev, const void *xclbin, uint32_t slot_id)
{
	int rval = 0;

	xocl_info(&XDEV(xdev)->pdev->dev,"slot_id = %d", slot_id);
	if (XOCL_DSA_IS_VERSAL(xdev)) {
		rval = xocl_xclbin_download_impl(xdev, xclbin, slot_id, &xgq_ops);
		/* Legacy shell doesn't have xgq resources */
		if (rval == -ENODEV)
			return xocl_xclbin_download_impl(xdev, xclbin, slot_id,
					&versal_ops);
	} else {
		/*
		 * TODO:
		 * return xocl_xclbin_download_impl(xdev, xclbin, &icap_ops);
		 */
		rval = xocl_icap_download_axlf(xdev, xclbin, slot_id);
		if (!rval && XOCL_DSA_IS_MPSOC(xdev))
			rval = xocl_xclbin_download_impl(xdev, xclbin, slot_id,
					&mpsoc_ops);

		if (rval)
			xocl_icap_clean_bitstream(xdev, slot_id);
	}

	return rval;
}

enum MEM_TAG convert_mem_tag(const char *name)
{
	/* Don't trust m_type in xclbin, convert name to m_type instead.
	 * m_tag[i] = "HBM[0]" -> m_type = MEM_TAG_HBM
	 * m_tag[i] = "DDR[1]" -> m_type = MEM_TAG_DRAM
	 */
	enum MEM_TAG mem_tag = MEM_TAG_INVALID;

	if (!strncasecmp(name, "DDR", 3))
		mem_tag = MEM_TAG_DDR;
	else if (!strncasecmp(name, "PLRAM", 5))
		mem_tag = MEM_TAG_PLRAM;
	else if (!strncasecmp(name, "HBM", 3))
		mem_tag = MEM_TAG_HBM;
	else if (!strncasecmp(name, "bank", 4))
		mem_tag = MEM_TAG_DDR;
	else if (!strncasecmp(name, "HOST[0]", 7))
		mem_tag = MEM_TAG_HOST;

	return mem_tag;
}
