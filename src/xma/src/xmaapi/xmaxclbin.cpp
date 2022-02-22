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
#include <fstream>
#include <stdexcept>
#include "xclbin.h"
#include "app/xmaerror.h"
#include "app/xmalogger.h"
#include "lib/xmaxclbin.h"
#include "app/xma_utils.hpp"
#include "lib/xma_utils.hpp"

#define XMAAPI_MOD "xmaxclbin"

std::vector<char> xma_xclbin_file_open(const std::string& xclbin_name)
{
    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Loading %s ", xclbin_name.c_str());

    std::ifstream infile(xclbin_name, std::ios::binary | std::ios::ate);
    if (!infile) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Failed to open xclbin file");
        throw std::runtime_error("Failed to open xclbin file");
    }
    std::streamsize xclbin_size = infile.tellg();
    infile.seekg(0, std::ios::beg);

    std::vector<char> xclbin_buffer;
    try {
        xclbin_buffer.reserve(xclbin_size);
    } catch (const std::bad_alloc& ex) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not allocate buffer for file %s ", xclbin_name.c_str());
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Buffer allocation error: %s ", ex.what());
        throw;
    } catch (...) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not allocate buffer for xclbin file %s ", xclbin_name.c_str());
        throw;
    }
    infile.read(xclbin_buffer.data(), xclbin_size);
    if (infile.gcount() != xclbin_size) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Unable to read full xclbin file %s ", xclbin_name.c_str());
        throw std::runtime_error("Unable to read full xclbin file");
    }

    return xclbin_buffer;
}

//Extract info form xrt::xclbin
int xma_xclbin_info_get(const std::string& xclbin_name, XmaXclbinInfo *info)
{
    std::vector<std::string> cu_vec;
    memset(info->ip_ddr_mapping, 0, sizeof(info->ip_ddr_mapping));
    auto xclbin = xrt::xclbin(xclbin_name);
    const auto& memidx_encoding = xrt_core::xclbin_int::get_membank_encoding(xclbin);
    for (auto& kernel : xclbin.get_kernels()) {
        //get CU's of each kernel object
        //iterate over CU's to get arguments
        for (const auto& cu : kernel.get_cus())
        {
            auto krnl_inst_name = cu.get_name();
            auto itr = std::find_if(cu_vec.begin(), cu_vec.end(),
                [&krnl_inst_name](auto& cu_name) {
                    return krnl_inst_name == cu_name;
                });
            if (itr == cu_vec.end())
                cu_vec.push_back(cu.get_name());
        }
    }
    uint64_t ip_idx = 0;
    uint64_t tmp_ddr_map = 0;
    for (auto& ip_node : xclbin.get_ips()) {
        auto ip_name = ip_node.get_name();
        auto itr = std::find_if(cu_vec.begin(), cu_vec.end(),
            [&ip_name](auto& inst) {
                return ip_name == inst;
            });
        if (itr != cu_vec.end()) {
            info->ip_vec.push_back(ip_name);
            // collect the memory connections for each IP argument
            std::vector<encoded_bitset<MAX_DDR_MAP>> connections;
            std::unordered_map<int32_t, int32_t> arg_to_mem_info;
            for (const auto& arg : ip_node.get_args()) {
                auto argidx = arg.get_index();

                for (const auto& mem : arg.get_mems()) {
                    auto memidx = mem.get_index();
                    // disregard memory indices that do not map to a memory mapped bank
                    // this could be streaming connections
                    if (memidx_encoding.at(memidx) == std::numeric_limits<size_t>::max())
                        continue;
                    auto size = argidx + 1;
                    const std::vector<size_t>* encoding = &memidx_encoding;
                    if (connections.size() >= size)
                        return XMA_ERROR;
                    connections.resize(size, encoded_bitset<MAX_DDR_MAP>{encoding});
                    connections[argidx].set(memidx);

                    if(connections[argidx].test(memidx))
                        arg_to_mem_info.emplace(argidx, memidx);

                    tmp_ddr_map = 1;
                    tmp_ddr_map = tmp_ddr_map << memidx;
                    info->ip_ddr_mapping[ip_idx] |= tmp_ddr_map;
                }
            }
            info->ip_arg_connections.push_back(arg_to_mem_info);
            ip_idx++;
        }
    }
    return XMA_SUCCESS;
}

//Allow default ddr_bank of -1; When CU is not connected to any ddr
int xma_xclbin_map2ddr(uint64_t bit_map, int32_t* ddr_bank, bool has_mem_grps)
{
    //64 bits based on MAX_DDR_MAP = 64
    int ddr_bank_idx = 0;
    if (!has_mem_grps) {
        while (bit_map != 0) {
            if (bit_map & 1) {
                *ddr_bank = ddr_bank_idx;
                return XMA_SUCCESS;
            }
            ddr_bank_idx++;
            bit_map = bit_map >> 1;
        }
    } else {//For Memory Groups use last group as default memory group; For HBM groups
        ddr_bank_idx = 63;
        uint64_t tmp_int = 1ULL << 63;
        while (bit_map != 0) {
            if (bit_map & tmp_int) {
                *ddr_bank = ddr_bank_idx;
                return XMA_SUCCESS;
            }
            ddr_bank_idx--;
            bit_map = bit_map << 1;
        }
    }
    *ddr_bank = -1;
    return XMA_ERROR;
}