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
#include "core/common/config_reader.h"
#include "core/common/xclbin_parser.h"
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

int xma_xclbin_info_get(const char *buffer, XmaXclbinInfo *info)
{
    const axlf* xclbin_ax = reinterpret_cast<const axlf*>(buffer);
    const axlf_section_header* ip_hdr = xclbin::get_axlf_section(xclbin_ax, IP_LAYOUT);
    if (!ip_hdr) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not find IP_LAYOUT in xclbin ip_hdr=%p ", ip_hdr);
        throw std::runtime_error("Could not find IP_LAYOUT in xclbin file");
    }
    const char* data = &buffer[ip_hdr->m_sectionOffset];
    info->ip_axlf = reinterpret_cast<const ip_layout*>(data);

    const axlf_section_header* conn_hdr = xrt_core::xclbin::get_axlf_section(xclbin_ax, ASK_GROUP_CONNECTIVITY);
    if (!conn_hdr) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not find CONNECTIVITY in xclbin conn_hdr=%p ", conn_hdr);
        throw std::runtime_error("Could not find CONNECTIVITY in xclbin file");
    }
    const char* conn_data = &buffer[conn_hdr->m_sectionOffset];
    info->conn_axlf = reinterpret_cast<const connectivity*>(conn_data);

    memset(info->ip_ddr_mapping, 0, sizeof(info->ip_ddr_mapping));
    uint64_t tmp_ddr_map = 0;
    for(uint32_t c = 0; c < (uint32_t)info->conn_axlf->m_count; c++)
    {
        tmp_ddr_map = 1;
        tmp_ddr_map = tmp_ddr_map << (info->conn_axlf->m_connection[c].mem_data_index);
        info->ip_ddr_mapping[info->conn_axlf->m_connection[c].m_ip_layout_index] |= tmp_ddr_map;
    }
    return XMA_SUCCESS;
}

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
