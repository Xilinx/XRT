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
//#include <xclbin.h>
#include "app/xmaerror.h"
#include "lib/xmaxclbin.h"

#define xma_logmsg(f_, ...) printf((f_), ##__VA_ARGS__)

/* Private function */
static int get_xclbin_iplayout(char *buffer, XmaIpLayout *layout);

char *xma_xclbin_file_open(const char *xclbin_name)
{
    xma_logmsg("Loading %s\n", xclbin_name);

    std::ifstream file(xclbin_name, std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    char *buffer = (char*)malloc(size);
    if (!file.read(buffer, size))
    {
        xma_logmsg("Could not read file %s\n", xclbin_name);
        free(buffer);
        buffer = NULL;
    }

    return buffer;
}

int xma_xclbin_info_get(char *buffer, XmaXclbinInfo *info)
{
    return get_xclbin_iplayout(buffer, info->ip_layout);
}

static int get_xclbin_iplayout(char *buffer, XmaIpLayout *layout)
{
    //int rc = XMA_SUCCESS;
    axlf *xclbin = reinterpret_cast<axlf *>(buffer);

    const axlf_section_header *ip_hdr = xclbin::get_axlf_section(xclbin,
                                                                IP_LAYOUT);
    if (ip_hdr)
    {
        char *data = &buffer[ip_hdr->m_sectionOffset];
        const ip_layout *ipl = reinterpret_cast<ip_layout *>(data);
        for (int i = 0; i < ipl->m_count; i++)
        {
            memcpy(layout[i].kernel_name,
                   ipl->m_ip_data[i].m_name, MAX_KERNEL_NAME);
            layout[i].base_addr = ipl->m_ip_data[i].m_base_address;
            printf("kernel name = %s, base_addr = %lx\n",
                    layout[i].kernel_name,
                    layout[i].base_addr);
        }
    }
    else
    {
        printf("Could not find IP_LAYOUT in xclbin ip_hdr=%p\n", ip_hdr);
        //rc = XMA_ERROR;
        return XMA_ERROR;
    }

    return XMA_SUCCESS;
}
