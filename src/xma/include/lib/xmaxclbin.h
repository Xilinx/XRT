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
//#include "lib/xmacfg.h"
#include "lib/xmahw_lib.h"
#include "lib/xmalimits_lib.h"
#include "xclbin.h"
//#include "core/pcie/driver/linux/include/mgmt-ioctl.h"


typedef struct XmaIpLayout
{
    uint8_t      kernel_name[MAX_KERNEL_NAME];
    uint64_t     base_addr;
    bool         soft_kernel;
    uint32_t     reserved[16];
} XmaIpLayout;

typedef struct XmaMemTopology
{
    uint8_t       m_type;
    uint8_t       m_used;
    uint64_t      m_size;
    uint64_t      m_base_address;
    unsigned char m_tag[16];
} XmaMemTopology;

typedef struct XmaAXLFConnectivity
{
    int32_t arg_index;
    int32_t m_ip_layout_index;
    int32_t mem_data_index;
} XmaAXLFConnectivity;

typedef struct XmaXclbinInfo
{
    char                xclbin_name[PATH_MAX + NAME_MAX];
    uint16_t            freq_list[MAX_KERNEL_FREQS];
    XmaIpLayout         ip_layout[MAX_XILINX_KERNELS];
    //TODO HHS Change the limits to be appropriate
    XmaMemTopology      mem_topology[MAX_DDR_MAP];
    XmaAXLFConnectivity connectivity[MAX_CONNECTION_ENTRIES];
    uint32_t            number_of_kernels;
    uint32_t            number_of_mem_banks;
    uint32_t            number_of_connections;
    //uint64_t bitmap based on MAX_DDR_MAP=64
    uint64_t            ip_ddr_mapping[MAX_XILINX_KERNELS];
    //For execbo:
    //uint32_t    num_ips;
    uuid_t      uuid;
    uint32_t    reserved[32];
} XmaXclbinInfo;

char *xma_xclbin_file_open(const char *xclbin_name);
int xma_xclbin_info_get(char *buffer, XmaXclbinInfo *info);
int xma_xclbin_map2ddr(uint64_t bit_map, int* ddr_bank);
#endif
