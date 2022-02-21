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
#include <algorithm>
#include <bitset>
#include "core/include/experimental/xrt_xclbin.h"
#include "core/common/api/xclbin_int.h"

 // struct encoded_bitset - Sparse bit set
   //
   // Used to represent compressed mem_topology indidices of an xclbin.
   // Many entries are unused and can be ignored, yet section size
   // (indices) can be arbitrary long.  The encoding is a mapping from
   // original index to compressed index.
   //
   // Using this encoded bitset allows a smaller sized std::bitset
   // to be used for representing memory connectivity, where as a
   // uncompressed bitset would require 1000s of entries.
template <size_t size>
class encoded_bitset
{
public:
    encoded_bitset() = default;

    // Encoding is represented using a vector  that maps
    // the original index to the encoded (compressed) index.
    explicit
        encoded_bitset(const std::vector<size_t>* enc)
        : m_encoding(enc)
    {}

    void
        set(size_t idx)
    {
        m_bitset.set(m_encoding ? m_encoding->at(idx) : idx);
    }

    bool
        test(size_t idx) const
    {
        return m_bitset.test(m_encoding ? m_encoding->at(idx) : idx);
    }

private:
    const std::vector<size_t>* m_encoding = nullptr;
    std::bitset<size> m_bitset;
};

typedef struct XmaXclbinInfo
{
    bool                has_mem_groups;
    uint64_t            ip_ddr_mapping[MAX_XILINX_KERNELS];
    std::vector<std::string> ip_vec;
    std::vector<std::unordered_map<int32_t, int32_t>> ip_arg_connections;// arg# -> ddr_bank#
} XmaXclbinInfo;

std::vector<char> xma_xclbin_file_open(const std::string& xclbin_name);
int xma_xclbin_info_get(const std::string& xclbin_name, XmaXclbinInfo* info);
int xma_xclbin_map2ddr(uint64_t bit_map, int* ddr_bank, bool has_mem_grps);
#endif
