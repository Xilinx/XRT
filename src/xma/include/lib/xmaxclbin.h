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


typedef struct XmaIpLayout
{
    std::string  kernel_name;
    uint64_t     base_addr;
    bool         soft_kernel;
    bool         kernel_channels;
    uint32_t     max_channel_id;
    int32_t      arg_start;
    int32_t      regmap_size;
    uint32_t     reserved[16];
} XmaIpLayout;

typedef struct XmaMemTopology
{
    uint8_t       m_type;
    uint8_t       m_used;
    uint64_t      m_size;
    uint64_t      m_base_address;
    std::string   m_tag;
} XmaMemTopology;

typedef struct XmaAXLFConnectivity
{
    int32_t arg_index;
    int32_t m_ip_layout_index;
    int32_t mem_data_index;
} XmaAXLFConnectivity;

typedef struct XmaXclbinInfo
{
    std::string         xclbin_name;
    uint16_t            freq_list[MAX_KERNEL_FREQS];
    std::vector<XmaIpLayout> ip_layout;
    std::vector<uint64_t> cu_addrs_sorted;//Addrs sorted to determine cu masks
    std::vector<XmaMemTopology> mem_topology;
    std::vector<XmaAXLFConnectivity> connectivity;
    uint32_t            number_of_hardware_kernels;
    uint32_t            number_of_kernels;
    uint32_t            number_of_mem_banks;
    uint32_t            number_of_connections;
    bool                has_mem_groups;
    //uint64_t bitmap based on MAX_DDR_MAP=64
    uint64_t            ip_ddr_mapping[MAX_XILINX_KERNELS];
    //For execbo:
    //uint32_t    num_ips;
    uuid_t      uuid;
    uint32_t    reserved[32];
} XmaXclbinInfo;

std::vector<char> xma_xclbin_file_open(const std::string& xclbin_name);
int xma_xclbin_info_get(const char *buffer, XmaXclbinInfo *info);
int xma_xclbin_map2ddr(uint64_t bit_map, int* ddr_bank, bool has_mem_grps);
#endif
