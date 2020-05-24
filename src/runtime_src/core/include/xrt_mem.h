/*
 * Copyright (C) 2019, Xilinx Inc - All rights reserved.
 * Xilinx Runtime (XRT) APIs
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

#ifndef _XRT_MEM_H_
#define _XRT_MEM_H_


#ifdef __cplusplus
extern "C" {
#endif

/**
 * XCL BO Flags bits layout
 *
 * bits  0 ~ 15: DDR BANK index
 * bits 24 ~ 31: BO flags
 */

#define XRT_BO_FLAGS_MEMIDX_MASK	(0xFFFFFFUL)
#define	XCL_BO_FLAGS_CACHEABLE		(1 << 24)
#define	XCL_BO_FLAGS_SVM		(1 << 27)
#define	XCL_BO_FLAGS_DEV_ONLY		(1 << 28)
#define	XCL_BO_FLAGS_HOST_ONLY		(1 << 29)
#define	XCL_BO_FLAGS_P2P		(1 << 30)
#define	XCL_BO_FLAGS_EXECBUF		(1 << 31)

/**
 * This is the legacy usage of XCL DDR Flags.
 *
 * byte-0 lower 4 bits for DDR Flags are one-hot encoded
 */
enum xclDDRFlags {
    XCL_DEVICE_RAM_BANK0 = 0x00000000,
    XCL_DEVICE_RAM_BANK1 = 0x00000002,
    XCL_DEVICE_RAM_BANK2 = 0x00000004,
    XCL_DEVICE_RAM_BANK3 = 0x00000008,
};

/****   MEMORY GROUP SECTION ****/
#define XCL_MEM_GROUP_MAX 128 
struct xcl_mem_map_info {
    uint32_t cu_id;
    uint32_t arg_id;
    uint32_t grp_id;
};

struct xcl_mem_map {
    int32_t m_count;
    struct xcl_mem_map_info m_map[XCL_MEM_GROUP_MAX];
};

struct xcl_mem_group_info {
    uint32_t l_bank_idx;
    uint64_t l_start_addr;
    uint32_t h_bank_idx;
    uint64_t h_end_addr;
};

struct xcl_mem_group {
    int32_t g_count;
    struct xcl_mem_group_info m_group[XCL_MEM_GROUP_MAX];
};

struct xcl_mem_group_map {
    struct xcl_mem_map *mMap;
    struct xcl_mem_group *mGroup;
};

#ifdef __cplusplus
}
#endif
#endif
