// SPDX-License-Identifier: GPL-2.0
/*
 * Xilinx Kernel Driver XCLBIN parser
 *
 * Copyright (C) 2020 Xilinx, Inc.
 *
 * Authors: David Zhang <davidzha@xilinx.com>
 */

#ifndef _XOCL_XCLBIN_H
#define	_XOCL_XCLBIN_H

#if 0
/* for icap user to preserve xclbin data */
struct xocl_xclbin {
	xuid_t				xclbin_uuid;
	int				xclbin_refcnt;
	usigned long			xclbin_clock_freq_topology_len;
	struct clock_freq_topology	*xclbin_clock_freq_topology;
	struct mem_topology		*xclbin_mem_topology;
	struct ip_layout		*xclbin_ip_layout;
	struct debug_ip_layout		*xclbin_debug_layout;
	struct connectivity		*xclbin_connectivity;
	void				*xclbin_partition_metadata;
	uint64_t			xclbin_max_host_mem_aperture;
}

init xocl_xclbin_init(xdev_handle_t xdev);
void xocl_xclbin_fini(xdev_handle_t xdev);
#endif

int xocl_xclbin_download(xdev_handle_t xdev, const void *xclbin);

#endif
