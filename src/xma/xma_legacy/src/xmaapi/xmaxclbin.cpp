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

#define XMAAPI_MOD "xmaxclbin"

/* Private function */
static int get_xclbin_iplayout(char *buffer, XmaXclbinInfo *xclbin_info);
static int get_xclbin_mem_topology(char *buffer, XmaXclbinInfo *xclbin_info);
static int get_xclbin_connectivity(char *buffer, XmaXclbinInfo *xclbin_info);

static const axlf_section_header*
get_mem_topology(const axlf* xclbin)
{
  if (auto hdr = xclbin::get_axlf_section(xclbin, ASK_GROUP_TOPOLOGY))
    return hdr;

  return xclbin::get_axlf_section(xclbin, MEM_TOPOLOGY);
}

static const axlf_section_header*
get_connectivity(const axlf* xclbin)
{
  if (auto hdr = xclbin::get_axlf_section(xclbin, ASK_GROUP_CONNECTIVITY))
    return hdr;

  return xclbin::get_axlf_section(xclbin, CONNECTIVITY);
}

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
        XmaIpLayout* layout = xclbin_info->ip_layout;
        xclbin_info->number_of_kernels = 0;
        uint32_t j = 0;
        for (int i = 0; i < ipl->m_count; i++)
        {
            if (ipl->m_ip_data[i].m_type != IP_KERNEL)
                continue;
            memcpy(xclbin_info->ip_layout[j].kernel_name,
                   ipl->m_ip_data[i].m_name, MAX_KERNEL_NAME);
            layout[j].base_addr = ipl->m_ip_data[i].m_base_address;
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "index = %d, kernel name = %s, base_addr = %lx\n",
                    j, layout[j].kernel_name, layout[j].base_addr);
            j++;
        }
        xclbin_info->number_of_kernels = j;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "IP LAYOUT - %d kernels\n", xclbin_info->number_of_kernels);
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

    const axlf_section_header *ip_hdr = get_mem_topology(xclbin);
    if (ip_hdr)
    {
        char *data = &buffer[ip_hdr->m_sectionOffset];
        const mem_topology *mem_topo = reinterpret_cast<mem_topology *>(data);
        XmaMemTopology *topology = xclbin_info->mem_topology;
        xclbin_info->number_of_mem_banks = mem_topo->m_count;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "MEM TOPOLOGY - %d banks\n",xclbin_info->number_of_mem_banks);
        for (int i = 0; i < mem_topo->m_count; i++)
        {
            topology[i].m_type = mem_topo->m_mem_data[i].m_type;
            topology[i].m_used = mem_topo->m_mem_data[i].m_used;
            topology[i].m_size = mem_topo->m_mem_data[i].m_size;
            topology[i].m_base_address = mem_topo->m_mem_data[i].m_base_address;
            //HHS change limits from MAX_DDR_MAP = 16 if needed
            memcpy(topology[i].m_tag, mem_topo->m_mem_data[i].m_tag, MAX_DDR_MAP*sizeof(unsigned char));
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

    const axlf_section_header *ip_hdr = get_connectivity(xclbin);
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

    uint64_t map[MAX_KERNEL_CONFIGS] = {};
    for(uint32_t c = 0; c < info->number_of_connections; c++)
    {
        XmaAXLFConnectivity *xma_conn = &info->connectivity[c];
        // Adding one to mem_data_index is misleading as the bitset
        // position no longer prepresents the mem_data_index. The off
        // by one however is compensated by xam_xclbin_map2ddr()
        map[xma_conn->m_ip_layout_index] |= static_cast<uint64_t>(1) << (xma_conn->mem_data_index + 1);
    }
    memcpy(info->ip_ddr_mapping,map,MAX_KERNEL_CONFIGS*sizeof(uint64_t));
    xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "\nCONNECTIONS (bitmap 63<-0)\n");
    for(uint32_t i = 0; i < info->number_of_kernels; i++)
    {
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "%s - 0x%04x\n",info->ip_layout[i].kernel_name, info->ip_ddr_mapping[i]);
    }
    //For execbo:
    info->num_ips = info->number_of_kernels;
    return XMA_SUCCESS;
}

int xma_xclbin_map2ddr(uint64_t bit_map, int* ddr_banks, int* num_banks)
{
    //TODO HHS Based on uint64_t bitmap considering 64 DDRs as max
    int ddr_bank_idx = -1;
    int count = 0;
    while (bit_map != 0)
    {
        ddr_bank_idx++;
        if (bit_map & 1)
        {
            ddr_banks[count++]=ddr_bank_idx-1;
        }
        bit_map = bit_map >> 1;
    }
    *num_banks = count;
    return XMA_SUCCESS;
}
