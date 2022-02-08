/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
#ifndef _XMA_XCLBIN_H_
#define _XMA_XCLBIN_H_

#include <uuid/uuid.h>
#include <limits.h>
#include "lib/xmahw_lib.h"
#include "lib/xmalimits_lib.h"
#include "xclbin.h"


typedef struct XmaXclbinInfo
{
    bool                has_mem_groups;
    uint64_t            ip_ddr_mapping[MAX_XILINX_KERNELS];
    const connectivity* conn_axlf;
    const ip_layout* ip_axlf;
} XmaXclbinInfo;

std::vector<char> xma_xclbin_file_open(const std::string& xclbin_name);
int xma_xclbin_info_get(const char *buffer, XmaXclbinInfo *info);
int xma_xclbin_map2ddr(uint64_t bit_map, int* ddr_bank, bool has_mem_grps);
#endif
