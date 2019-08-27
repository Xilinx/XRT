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
#include <stdio.h>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "xclbin.h"
#include "app/xmaerror.h"
#include "app/xmalogger.h"
#include "lib/xmaxclbin.h"
#include "core/common/config_reader.h"

#define XMAAPI_MOD "xmaxclbin"

/* Private function */
static int get_xclbin_iplayout(char *buffer, XmaXclbinInfo *xclbin_info);
static int get_xclbin_mem_topology(char *buffer, XmaXclbinInfo *xclbin_info);
static int get_xclbin_connectivity(char *buffer, XmaXclbinInfo *xclbin_info);

char *xma_xclbin_file_open(const char *xclbin_name)
{
    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Loading %s\n", xclbin_name);

    std::ifstream file(xclbin_name, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    char *buffer = (char*)malloc(size);
    if (buffer == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not allocate buffer for file %s\n", xclbin_name);
        return NULL;
    }
    if (!file.read(buffer, size))
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not read file %s\n", xclbin_name);
        free(buffer);
        buffer = NULL;
    }

    return buffer;
}

static int get_xclbin_iplayout(char *buffer, XmaXclbinInfo *xclbin_info)
{
    axlf *xclbin = reinterpret_cast<axlf *>(buffer);

    const axlf_section_header *ip_hdr = xclbin::get_axlf_section(xclbin, IP_LAYOUT);
    if (ip_hdr)
    {
        char *data = &buffer[ip_hdr->m_sectionOffset];
        const ip_layout *ipl = reinterpret_cast<ip_layout *>(data);
        //XmaIpLayout* layout = xclbin_info->ip_layout;
        xclbin_info->number_of_kernels = 0;
        xclbin_info->number_of_hardware_kernels = 0;
        bool dataflow_ini = xrt_core::config::get_feature_toggle("Runtime.kernel_channels");
        uint32_t j = 0;
        for (int i = 0; i < ipl->m_count; i++)
        {
            if (ipl->m_ip_data[i].m_type != IP_KERNEL)
                continue;

            if (j == MAX_XILINX_KERNELS) {
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA supports max of only %d kernels per device\n", MAX_XILINX_KERNELS);
                return XMA_ERROR;
            }
            memset(xclbin_info->ip_layout[j].kernel_name, 0, MAX_KERNEL_NAME);
            std::string str_tmp1 = std::string((char*)ipl->m_ip_data[i].m_name);
            str_tmp1.copy((char*)xclbin_info->ip_layout[j].kernel_name, MAX_KERNEL_NAME-1);
            /*
            memcpy(xclbin_info->ip_layout[j].kernel_name,
                   ipl->m_ip_data[i].m_name, MAX_KERNEL_NAME);
            */
            //layout[j].base_addr = ipl->m_ip_data[i].m_base_address;
            xclbin_info->ip_layout[j].base_addr = ipl->m_ip_data[i].m_base_address;
            if (((ipl->m_ip_data[i].properties & IP_CONTROL_MASK) >> IP_CONTROL_SHIFT) == AP_CTRL_CHAIN) {
                if (dataflow_ini) {
                    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "kernel \"%s\" is a dataflow kernel. channel_id will be handled by XMA. host app and plugins should not use reserved channle_id registers\n", str_tmp1.c_str());
                    xclbin_info->ip_layout[j].dataflow_kernel = true;
                } else {
                    xma_logmsg(XMA_WARNING_LOG, XMAAPI_MOD, "kernel \"%s\" is a dataflow kernel. Use dataflow xrt.ini setting to enable handling of channel_id by XMA. Treatng it as legacy dataflow kernel and channels to be managed by host app and plugins\n", str_tmp1.c_str());
                    xclbin_info->ip_layout[j].dataflow_kernel = false;
                }
            } else {
                xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "kernel \"%s\" is a legacy kernel. Channels to be managed by host app and plugins\n", str_tmp1.c_str());
                xclbin_info->ip_layout[j].dataflow_kernel = false;
            }
            xclbin_info->ip_layout[j].soft_kernel = false;

            /*            
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index = %d, kernel name = %s, base_addr = %lx\n",
                    j, layout[j].kernel_name, layout[j].base_addr);
            */
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index = %d, kernel name = %s, base_addr = %lx\n",
                    j, xclbin_info->ip_layout[j].kernel_name, xclbin_info->ip_layout[j].base_addr);
            j++;
        }
        xclbin_info->number_of_hardware_kernels = j;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "Num of hardware kernels on this device = %d\n", j);
        uint32_t num_soft_kernels = 0;
        //Handle soft kernels just like another hardware IP_Layout kernel
        //soft kernels to follow hardware kernels. so soft kenrel index will start after hardware kernels
        //const axlf_section_header *soft_kernel_hdr = nullptr;
        for (const axlf_section_header *soft_kernel_hdr = xclbin::get_axlf_section(xclbin, SOFT_KERNEL); soft_kernel_hdr != nullptr; soft_kernel_hdr = xclbin::get_axlf_section_next(xclbin, soft_kernel_hdr, SOFT_KERNEL)) {
            char *data = &buffer[soft_kernel_hdr->m_sectionOffset];
            const soft_kernel *sk_data = reinterpret_cast<soft_kernel *>(data);
            if (num_soft_kernels + sk_data->m_num_instances == MAX_XILINX_SOFT_KERNELS) {
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA supports max of only %d soft kernels per device\n", MAX_XILINX_SOFT_KERNELS);
                return XMA_ERROR;
            }

            std::string str_tmp1 = std::string((char*)&buffer[soft_kernel_hdr->m_sectionOffset + sk_data->mpo_name]);
            std::string str_tmp2 = std::string((char*)&buffer[soft_kernel_hdr->m_sectionOffset + sk_data->mpo_version]);
            std::string str_tmp3 = std::string((char*)&buffer[soft_kernel_hdr->m_sectionOffset + sk_data->mpo_symbol_name]);
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "soft kernel name = %s, version = %d, symbol name = %s, num of instances = %d\n", str_tmp1.c_str(), str_tmp2.c_str(), str_tmp3.c_str(), sk_data->m_num_instances);
            for (uint32_t i = 0; i < sk_data->m_num_instances; i++) {
                memset(xclbin_info->ip_layout[j].kernel_name, 0, MAX_KERNEL_NAME);
                str_tmp1 = std::string((char*)&buffer[soft_kernel_hdr->m_sectionOffset + sk_data->mpo_name]);
                str_tmp1 += "_";
                str_tmp1 += std::to_string(i);
                str_tmp1.copy((char*)xclbin_info->ip_layout[j].kernel_name, MAX_KERNEL_NAME-1);
                xclbin_info->ip_layout[j].soft_kernel = true;
                xclbin_info->ip_layout[j].base_addr = 0;

                xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index = %d, soft kernel name = %s\n", j, xclbin_info->ip_layout[j].kernel_name);

                j++;
                num_soft_kernels++;
            }
        }
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "Num of soft kernels on this device = %d\n", num_soft_kernels);

        xclbin_info->number_of_kernels = j;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "Num of total kernels on this device = %d\n", xclbin_info->number_of_kernels);
    }
    else
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not find IP_LAYOUT in xclbin ip_hdr=%p\n", ip_hdr);
        return XMA_ERROR;
    }

    uuid_copy(xclbin_info->uuid, xclbin->m_header.uuid); 

    return XMA_SUCCESS;
}

static int get_xclbin_mem_topology(char *buffer, XmaXclbinInfo *xclbin_info)
{
    //int rc = XMA_SUCCESS;
    axlf *xclbin = reinterpret_cast<axlf *>(buffer);

    const axlf_section_header *ip_hdr = xclbin::get_axlf_section(xclbin, MEM_TOPOLOGY);
    if (ip_hdr)
    {
        char *data = &buffer[ip_hdr->m_sectionOffset];
        const mem_topology *mem_topo = reinterpret_cast<mem_topology *>(data);
        XmaMemTopology *topology = xclbin_info->mem_topology;
        xclbin_info->number_of_mem_banks = mem_topo->m_count;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "MEM TOPOLOGY - %d banks\n",xclbin_info->number_of_mem_banks);
        if (xclbin_info->number_of_mem_banks > MAX_DDR_MAP) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA supports max of only 64 mem banks\n");
            return XMA_ERROR;
        }
        for (int i = 0; i < mem_topo->m_count; i++)
        {
            topology[i].m_type = mem_topo->m_mem_data[i].m_type;
            topology[i].m_used = mem_topo->m_mem_data[i].m_used;
            topology[i].m_size = mem_topo->m_mem_data[i].m_size;
            topology[i].m_base_address = mem_topo->m_mem_data[i].m_base_address;
            //m_tag is 16 chars
            memcpy(topology[i].m_tag, mem_topo->m_mem_data[i].m_tag, 16*sizeof(unsigned char));
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index=%d, tag=%s, type = %d, used = %d, size = %lx, base = %lx\n",
                   i,topology[i].m_tag, topology[i].m_type, topology[i].m_used,
                   topology[i].m_size, topology[i].m_base_address);
        }
    }
    else
    {
        printf("Could not find MEM_TOPOLOGY in xclbin ip_hdr=%p\n", ip_hdr);
        return XMA_ERROR;
    }

    return XMA_SUCCESS;
}

static int get_xclbin_connectivity(char *buffer, XmaXclbinInfo *xclbin_info)
{
    //int rc = XMA_SUCCESS;
    axlf *xclbin = reinterpret_cast<axlf *>(buffer);

    const axlf_section_header *ip_hdr = xclbin::get_axlf_section(xclbin, CONNECTIVITY);
    if (ip_hdr)
    {
        char *data = &buffer[ip_hdr->m_sectionOffset];
        const connectivity *axlf_conn = reinterpret_cast<connectivity *>(data);
        XmaAXLFConnectivity *xma_conn = xclbin_info->connectivity;
        xclbin_info->number_of_connections = axlf_conn->m_count;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "CONNECTIVITY - %d connections\n",xclbin_info->number_of_connections);
        for (int i = 0; i < axlf_conn->m_count; i++)
        {
            xma_conn[i].arg_index         = axlf_conn->m_connection[i].arg_index;
            xma_conn[i].m_ip_layout_index = axlf_conn->m_connection[i].m_ip_layout_index;
            xma_conn[i].mem_data_index    = axlf_conn->m_connection[i].mem_data_index;
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index = %d, arg_idx = %d, ip_idx = %d, mem_idx = %d\n",
                     i, xma_conn[i].arg_index, xma_conn[i].m_ip_layout_index,
                     xma_conn[i].mem_data_index);
        }
    }
    else
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not find CONNECTIVITY in xclbin ip_hdr=%p\n", ip_hdr);
        return XMA_ERROR;
    }

    return XMA_SUCCESS;
}

int xma_xclbin_info_get(char *buffer, XmaXclbinInfo *info)
{
    int rc = 0;
    rc = get_xclbin_mem_topology(buffer, info);
    if(rc == XMA_ERROR)
        return rc;
    rc = get_xclbin_connectivity(buffer, info);
    if(rc == XMA_ERROR)
        return rc;
    rc = get_xclbin_iplayout(buffer, info);
    if(rc == XMA_ERROR)
        return rc;

    memset(info->ip_ddr_mapping, 0, sizeof(info->ip_ddr_mapping));
    for(uint32_t c = 0; c < info->number_of_connections; c++)
    {
        XmaAXLFConnectivity *xma_conn = &info->connectivity[c];
        info->ip_ddr_mapping[xma_conn->m_ip_layout_index] |= 1 << (xma_conn->mem_data_index);
    }
    xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "\nCU DDR connections bitmap:\n");
    for(uint32_t i = 0; i < info->number_of_hardware_kernels; i++)
    {
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "\t%s - 0x%04llx\n",info->ip_layout[i].kernel_name, info->ip_ddr_mapping[i]);
    }
    //For execbo:
    //info->num_ips = info->number_of_kernels;
    return XMA_SUCCESS;
}

int xma_xclbin_map2ddr(uint64_t bit_map, int* ddr_bank)
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
    return XMA_ERROR;
}
