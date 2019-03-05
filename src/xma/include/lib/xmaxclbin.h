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
#include "lib/xmacfg.h"
#include "lib/xmahw.h"
#include "lib/xmalimits.h"
#include "xclbin.h"
#include "mgmt-ioctl.h"


typedef struct XmaIpLayout
{
    uint8_t      kernel_name[MAX_KERNEL_NAME];
    uint64_t     base_addr;
    uint32_t     reserved[16];
} XmaIpLayout;

typedef struct XmaXclbinInfo
{
    char        xclbin_name[PATH_MAX + NAME_MAX];
    uint16_t    freq_list[MAX_KERNEL_FREQS];
    XmaIpLayout ip_layout[MAX_KERNEL_CONFIGS];
    //For execbo:
    uint32_t    num_ips;
    uuid_t      uuid;
    uint32_t    reserved[32];
} XmaXclbinInfo;

char *xma_xclbin_file_open(const char *xclbin_name);
int xma_xclbin_info_get(char *buffer, XmaXclbinInfo *info);

#endif
