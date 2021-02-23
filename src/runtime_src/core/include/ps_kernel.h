/*
 * Copyright (C) 2021 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _PS_KERNEL_H_
#define _PS_KERNEL_H_

/*
 * This header file contains data structure for xrt PS kernel metadata. This
 * file is included in user space utilities and kernel drivers. The data
 * structure is used to describe PS kernel which is written by driver
 * and read by utility.
 */

#define	PS_KERNEL_NAME_LENGTH	20

struct ps_kernel_data {
	char		pkd_sym_name[PS_KERNEL_NAME_LENGTH];
	uint32_t	pkd_num_instances;
};

struct ps_kernel_node {
	uint32_t		pkn_count;
	struct ps_kernel_data	pkn_data[1];
};

#endif /* _PS_KERNEL_H_ */
