/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef XRT_CORE_MEM_GROUP_H
#define XRT_CORE_MEM_GROUP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * WARNING: This structure has been shared between userspace and driver.
 * Any modification to this will affect both parties. Please update
 * with extra caution.
 */

/****   MEMORY GROUP SECTION ****/
#define XCL_MEM_GROUP_MAX 128

/* Stores each groups information */
struct xcl_mem_group_info {
    uint32_t l_bank_idx;	/* Low memory Bank index */
    uint64_t l_start_addr;
    uint32_t h_bank_idx;	/* High memory Bank index */
    uint64_t h_end_addr;
};

struct xcl_mem_group {
    int32_t g_count;
    struct xcl_mem_group_info *m_group[XCL_MEM_GROUP_MAX];
};

/* memory mapping information for groups */
struct xcl_mem_map_info {
    int32_t cu_id;
    int32_t arg_id;
    int32_t grp_id;
};

struct xcl_mem_map {
    int32_t m_count;
    struct xcl_mem_map_info *m_map[XCL_MEM_GROUP_MAX];
};

struct xcl_mem_connectivity {
    struct xcl_mem_map     *mem_map;
    struct xcl_mem_group   *mem_group;
};

#ifdef __cplusplus
}
#endif

#endif
