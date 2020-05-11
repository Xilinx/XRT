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

/* Private function */
static int get_xclbin_iplayout(const char *buffer, XmaXclbinInfo *xclbin_info);
static int get_xclbin_mem_topology(const char *buffer, XmaXclbinInfo *xclbin_info);
static int get_xclbin_connectivity(const char *buffer, XmaXclbinInfo *xclbin_info);

std::vector<char> xma_xclbin_file_open(const std::string& xclbin_name)
{
    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Loading %s ", xclbin_name.c_str());

    std::ifstream infile(xclbin_name, std::ios::binary | std::ios::ate);
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

static int32_t kernel_max_channel_id(const ip_data* ip, std::string kernel_channels)
{
  if (kernel_channels.empty())
    return -1;

  std::string knm = std::string(reinterpret_cast<const char*>(ip->m_name));
  knm = knm.substr(0,knm.find(":"));

  auto pos1 = kernel_channels.find("{"+knm+":");
  if (pos1 == std::string::npos)
    return -1;

  auto pos2 = kernel_channels.find("}",pos1);
  if (pos2 == std::string::npos || pos2 < pos1+knm.size()+2)
    return -2;

  auto ctxid_str = kernel_channels.substr(pos1+knm.size()+2,pos2);
  auto ctxid = std::stoi(ctxid_str);
  if (ctxid < 0 || ctxid > 31)
    return -3;
  
  return ctxid;
}

static int get_xclbin_iplayout(const char *buffer, XmaXclbinInfo *xclbin_info)
{
    const axlf *xclbin = reinterpret_cast<const axlf *>(buffer);

    const axlf_section_header *ip_hdr = xclbin::get_axlf_section(xclbin, IP_LAYOUT);
    if (ip_hdr)
    {
        const char *data = &buffer[ip_hdr->m_sectionOffset];
        const ip_layout *ipl = reinterpret_cast<const ip_layout *>(data);
        xclbin_info->number_of_kernels = 0;
        xclbin_info->number_of_hardware_kernels = 0;
        std::string kernel_channels_info = xrt_core::config::get_kernel_channel_info();
        xclbin_info->cu_addrs_sorted = xrt_core::xclbin::get_cus(ipl, false);
        bool has_cuisr = xrt_core::xclbin::get_cuisr(xclbin);
        if (!has_cuisr) {
            xma_logmsg(XMA_WARNING_LOG, XMAAPI_MOD, "One or more CUs do not support interrupt. Use RTL Wizard or Vitis for xclbin creation ");
        }
        auto& xma_ip_layout = xclbin_info->ip_layout;
        for (int i = 0; i < ipl->m_count; i++) {
            if (ipl->m_ip_data[i].m_type != IP_KERNEL)
                continue;

            if (xma_ip_layout.size() == MAX_XILINX_KERNELS) {
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA supports max of only %d kernels per device ", MAX_XILINX_KERNELS);
                throw std::runtime_error("XMA supports max of only " + std::to_string(MAX_XILINX_KERNELS) + " kernels per device");
            }

            XmaIpLayout temp_ip_layout;
            temp_ip_layout.base_addr = ipl->m_ip_data[i].m_base_address;
            temp_ip_layout.kernel_name = std::string((char*)ipl->m_ip_data[i].m_name);

            auto args = xrt_core::xclbin::get_kernel_arguments(xclbin, temp_ip_layout.kernel_name);
            temp_ip_layout.arg_start = -1;
            temp_ip_layout.regmap_size = -1;
            if (args.size() > 0) {
                temp_ip_layout.arg_start = args[0].offset;
                auto& last = args.back();
                temp_ip_layout.regmap_size = last.offset + last.size;
                if (temp_ip_layout.arg_start < 0x10) {
                    xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "kernel %s doesn't meet argument register map spec of HLS/RTL Wizard kernels ", temp_ip_layout.kernel_name.c_str());
                    throw std::runtime_error("kernel doesn't meet argument register map spec of HLS/RTL Wizard kernels");
                }
            } else {
                std::string knm = temp_ip_layout.kernel_name.substr(0,temp_ip_layout.kernel_name.find(":"));
                args = xrt_core::xclbin::get_kernel_arguments(xclbin, knm);
                if (args.size() > 0) {
                    temp_ip_layout.arg_start = args[0].offset;
                    auto& last = args.back();
                    temp_ip_layout.regmap_size = last.offset + last.size;
                    if (temp_ip_layout.arg_start < 0x10) {
                        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "kernel %s doesn't meet argument register map spec of HLS/RTL Wizard kernels ", temp_ip_layout.kernel_name.c_str());
                        throw std::runtime_error("kernel doesn't meet argument register map spec of HLS/RTL Wizard kernels");
                    }
                }
            }
            temp_ip_layout.kernel_channels = false;
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index = %d, kernel name = %s, base_addr = %lx ",
                    xma_ip_layout.size(), temp_ip_layout.kernel_name.c_str(), temp_ip_layout.base_addr);
            if (temp_ip_layout.regmap_size > MAX_KERNEL_REGMAP_SIZE) {
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "kernel %s register map size exceeds max limit. regmap_size: %d, max regmap_size: %d . Will use only max regmap_size", temp_ip_layout.kernel_name.c_str(), temp_ip_layout.regmap_size, MAX_KERNEL_REGMAP_SIZE);
                //DRM IPs have registers at high offset
                temp_ip_layout.regmap_size = MAX_KERNEL_REGMAP_SIZE;
                //throw std::runtime_error("kernel regmap exceed's max size");
            }
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "%s:- arg_start: 0x%x, regmap_size: 0x%x", temp_ip_layout.kernel_name.c_str(), temp_ip_layout.arg_start, temp_ip_layout.regmap_size);
            auto cu_data = xrt_core::xclbin::get_cus(ipl, temp_ip_layout.kernel_name);
            if (cu_data.size() > 0) {
                if (((cu_data[0]->properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT) == AP_CTRL_CHAIN) {
                    int32_t max_channel_id = kernel_max_channel_id(cu_data[0], kernel_channels_info);
                    if (max_channel_id >= 0) {
                        xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "kernel \"%s\" is a dataflow kernel. channel_id will be handled by XMA. host app and plugins should not use reserved channle_id registers. Max channel_id is: %d ", temp_ip_layout.kernel_name.c_str(), max_channel_id);
                        temp_ip_layout.kernel_channels = true;
                        temp_ip_layout.max_channel_id = (uint32_t)max_channel_id;
                    } else {
                        if (max_channel_id == -1) {
                            xma_logmsg(XMA_WARNING_LOG, XMAAPI_MOD, "kernel \"%s\" is a dataflow kernel. Use kernel_channels xrt.ini setting to enable handling of channel_id by XMA. Treatng it as legacy dataflow kernel and channels to be managed by host app and plugins ", temp_ip_layout.kernel_name.c_str());
                        } else if (max_channel_id == -2) {
                            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "kernel \"%s\" is a dataflow kernel.  xrt.ini kernel_channels setting has incorrect format. setting found is: %s ", temp_ip_layout.kernel_name.c_str(), kernel_channels_info.c_str());
                            throw std::runtime_error("Incorrect dataflow kernel ini setting");
                        } else if (max_channel_id == -3) {
                            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "kernel \"%s\" is a dataflow kernel.  xrt.ini kernel_channels setting only supports channel_ids from 0 to 31. setting found is: %s ", temp_ip_layout.kernel_name.c_str(), kernel_channels_info.c_str());
                            throw std::runtime_error("Incorrect dataflow kernel ini setting");
                        }
                    }
                } else {
                    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "kernel \"%s\" is a legacy kernel. Channels to be managed by host app and plugins ", temp_ip_layout.kernel_name.c_str());
                }
            } else {
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "No CU for kernel %s in xclbin", temp_ip_layout.kernel_name.c_str());
                throw std::runtime_error("Unexpected error. CU not found in xclbin");
            }
            temp_ip_layout.soft_kernel = false;
            
            xma_ip_layout.emplace_back(std::move(temp_ip_layout));
        }

        xclbin_info->number_of_hardware_kernels = xma_ip_layout.size();
        if (xclbin_info->number_of_hardware_kernels != xclbin_info->cu_addrs_sorted.size()) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Num of hardware kernels on this device = %d. But num of sorted kernels = %d", xclbin_info->number_of_hardware_kernels, xclbin_info->cu_addrs_sorted.size());
            throw std::runtime_error("Unable to get sorted kernel list");
        }
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "Num of hardware kernels on this device = %d ", xclbin_info->number_of_hardware_kernels);
        uint32_t num_soft_kernels = 0;
        //Handle soft kernels just like another hardware IP_Layout kernel
        //soft kernels to follow hardware kernels. so soft kenrel index will start after hardware kernels
        auto soft_kernels = xrt_core::xclbin::get_softkernels(xclbin);
        for (auto& sk: soft_kernels) {
            if (num_soft_kernels + sk.ninst == MAX_XILINX_SOFT_KERNELS) {
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA supports max of only %d soft kernels per device ", MAX_XILINX_SOFT_KERNELS);
                throw std::runtime_error("XMA supports max of only " + std::to_string(MAX_XILINX_SOFT_KERNELS) + " soft kernels per device");
            }
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "soft kernel name = %s, version = %s, symbol name = %s, num of instances = %d ", sk.mpo_name.c_str(), sk.mpo_version.c_str(), sk.symbol_name.c_str(), sk.ninst);

            XmaIpLayout temp_ip_layout;
            std::string str_tmp1;
            for (uint32_t i = 0; i < sk.ninst; i++) {
                str_tmp1 = std::string(sk.mpo_name);
                str_tmp1 += "_";
                str_tmp1 += std::to_string(i);
                temp_ip_layout.kernel_name = str_tmp1;

                temp_ip_layout.soft_kernel = true;
                temp_ip_layout.base_addr = 0;
                temp_ip_layout.arg_start = -1;
                temp_ip_layout.regmap_size = -1;
                xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index = %d, soft kernel name = %s ", xma_ip_layout.size(), temp_ip_layout.kernel_name.c_str());

                xma_ip_layout.emplace_back(std::move(temp_ip_layout));
                num_soft_kernels++;
            }
        }
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "Num of soft kernels on this device = %d ", num_soft_kernels);

        xclbin_info->number_of_kernels = xma_ip_layout.size();
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "Num of total kernels on this device = %d ", xclbin_info->number_of_kernels);

        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "  ");
        const axlf_section_header *xml_hdr = xclbin::get_axlf_section(xclbin, EMBEDDED_METADATA);
        if (xml_hdr) {
            char *xml_data = const_cast<char*>(&buffer[xml_hdr->m_sectionOffset]);
            uint64_t xml_size = xml_hdr->m_sectionSize;
            if (xml_size > 0 && xml_size < 500000) {
                xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "XML MetaData is:");
                xma_core::utils::streambuf xml_streambuf(xml_data, xml_size);
                std::istream xml_stream(&xml_streambuf);
                std::string line;
                while(std::getline(xml_stream, line)) {
                    xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "%s", line.c_str());
                }
            }
        } else {
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "XML MetaData is missing");
        }
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "  ");
        const axlf_section_header *kv_hdr = xclbin::get_axlf_section(xclbin, KEYVALUE_METADATA);
        if (kv_hdr) {
            char *kv_data = const_cast<char*>(&buffer[kv_hdr->m_sectionOffset]);
            uint64_t kv_size = kv_hdr->m_sectionSize;
            if (kv_size > 0 && kv_size < 200000) {
                xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "Key-Value MetaData is:");
                xma_core::utils::streambuf kv_streambuf(kv_data, kv_size);
                std::istream kv_stream(&kv_streambuf);
                std::string line;
                while(std::getline(kv_stream, line)) {
                    uint32_t lsize = line.size();
                    uint32_t pos = 0;
                    while (pos < lsize) {
                        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "%s", line.substr(pos, MAX_KERNEL_NAME).c_str());
                        pos += MAX_KERNEL_NAME;
                    }
                }
            }
        } else {
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "Key-Value Data is not present in xclbin");
        }
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "  ");
    }
    else
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not find IP_LAYOUT in xclbin ip_hdr=%p ", ip_hdr);
        throw std::runtime_error("Could not find IP_LAYOUT in xclbin");
    }

    uuid_copy(xclbin_info->uuid, xclbin->m_header.uuid); 

    return XMA_SUCCESS;
}

static int get_xclbin_mem_topology(const char *buffer, XmaXclbinInfo *xclbin_info)
{
    const axlf *xclbin = reinterpret_cast<const axlf *>(buffer);

    const axlf_section_header *ip_hdr = xclbin::get_axlf_section(xclbin, MEM_TOPOLOGY);
    if (ip_hdr)
    {
        const char *data = &buffer[ip_hdr->m_sectionOffset];
        const mem_topology *mem_topo = reinterpret_cast<const mem_topology *>(data);
        auto& xma_mem_topology = xclbin_info->mem_topology;

        xclbin_info->number_of_mem_banks = mem_topo->m_count;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "MEM TOPOLOGY - %d banks ",xclbin_info->number_of_mem_banks);
        if (xclbin_info->number_of_mem_banks > MAX_DDR_MAP) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA supports max of only %d mem banks ", MAX_DDR_MAP);
            throw std::runtime_error("XMA supports max of only " + std::to_string(MAX_DDR_MAP) + " mem banks");
        }
        for (int i = 0; i < mem_topo->m_count; i++)
        {
            XmaMemTopology temp_mem_topology;
            temp_mem_topology.m_type = mem_topo->m_mem_data[i].m_type;
            temp_mem_topology.m_used = mem_topo->m_mem_data[i].m_used;
            temp_mem_topology.m_size = mem_topo->m_mem_data[i].m_size;
            temp_mem_topology.m_base_address = mem_topo->m_mem_data[i].m_base_address;
            //m_tag is 16 chars
            temp_mem_topology.m_tag = std::string((char*)mem_topo->m_mem_data[i].m_tag);
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index=%d, tag=%s, type = %d, used = %d, size = %lx, base = %lx ",
                   i,temp_mem_topology.m_tag.c_str(), temp_mem_topology.m_type, temp_mem_topology.m_used,
                   temp_mem_topology.m_size, temp_mem_topology.m_base_address);
            xma_mem_topology.emplace_back(std::move(temp_mem_topology));
        }
    }
    else
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not find MEM TOPOLOGY in xclbin ip_hdr=%p ", ip_hdr);
        throw std::runtime_error("Could not find MEM TOPOLOGY in xclbin file");
    }

    return XMA_SUCCESS;
}

static int get_xclbin_connectivity(const char *buffer, XmaXclbinInfo *xclbin_info)
{
    const axlf *xclbin = reinterpret_cast<const axlf *>(buffer);

    const axlf_section_header *ip_hdr = xclbin::get_axlf_section(xclbin, CONNECTIVITY);
    if (ip_hdr)
    {
        const char *data = &buffer[ip_hdr->m_sectionOffset];
        const connectivity *axlf_conn = reinterpret_cast<const connectivity *>(data);
        auto& xma_connectivity = xclbin_info->connectivity;
        xclbin_info->number_of_connections = axlf_conn->m_count;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "CONNECTIVITY - %d connections ",xclbin_info->number_of_connections);
        for (int i = 0; i < axlf_conn->m_count; i++)
        {
            XmaAXLFConnectivity temp_conn;

            temp_conn.arg_index         = axlf_conn->m_connection[i].arg_index;
            temp_conn.m_ip_layout_index = axlf_conn->m_connection[i].m_ip_layout_index;
            temp_conn.mem_data_index    = axlf_conn->m_connection[i].mem_data_index;
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index = %d, arg_idx = %d, ip_idx = %d, mem_idx = %d ",
                     i, temp_conn.arg_index, temp_conn.m_ip_layout_index,
                     temp_conn.mem_data_index);
            xma_connectivity.emplace_back(std::move(temp_conn));
        }
    }
    else
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not find CONNECTIVITY in xclbin ip_hdr=%p ", ip_hdr);
        throw std::runtime_error("Could not find CONNECTIVITY in xclbin file");
    }

    return XMA_SUCCESS;
}

int xma_xclbin_info_get(const char *buffer, XmaXclbinInfo *info)
{
    get_xclbin_mem_topology(buffer, info);
    get_xclbin_connectivity(buffer, info);
    get_xclbin_iplayout(buffer, info);

    memset(info->ip_ddr_mapping, 0, sizeof(info->ip_ddr_mapping));
    uint64_t tmp_ddr_map = 0;
    for(uint32_t c = 0; c < info->number_of_connections; c++)
    {
        auto& xma_conn = info->connectivity[c];
        tmp_ddr_map = 1;
        tmp_ddr_map = tmp_ddr_map << (xma_conn.mem_data_index);
        info->ip_ddr_mapping[xma_conn.m_ip_layout_index] |= tmp_ddr_map;
    }
    xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "CU DDR connections bitmap:");
    for(uint32_t i = 0; i < info->number_of_hardware_kernels; i++)
    {
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "\t%s - 0x%016llx ",info->ip_layout[i].kernel_name.c_str(), (unsigned long long)info->ip_ddr_mapping[i]);
    }

    return XMA_SUCCESS;
}

int xma_xclbin_map2ddr(uint64_t bit_map, int32_t* ddr_bank)
{
    //64 bits based on MAX_DDR_MAP = 64
    int ddr_bank_idx = 0;
    while (bit_map != 0)
    {
        if (bit_map & 1)
        {
            *ddr_bank = ddr_bank_idx;
            return XMA_SUCCESS;
        }
        ddr_bank_idx++;
        bit_map = bit_map >> 1;
    }
    *ddr_bank = -1;
    return XMA_ERROR;
}
