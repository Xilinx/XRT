/**
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 * Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.
 */
#ifndef PS_KERNEL_H_
#define PS_KERNEL_H_

#ifdef _WIN32
# pragma warning( push )
# pragma warning( disable : 4200 )
#endif

/*
 * This header file contains data structure for xrt PS kernel metadata. This
 * file is included in user space utilities and kernel drivers. The data
 * structure is used to describe PS kernel which is written by driver
 * and read by utility.
 */

#define	PS_KERNEL_NAME_LENGTH	20
#define	PS_KERNEL_REG_OFFSET	4

struct ps_kernel_data {
	char		pkd_sym_name[PS_KERNEL_NAME_LENGTH];
	uint32_t	pkd_num_instances;
};

struct ps_kernel_node {
	uint32_t		pkn_count;
	struct ps_kernel_data	pkn_data[];
};

#ifdef _WIN32
# pragma warning( pop )
#endif

#endif /* _PS_KERNEL_H_ */
