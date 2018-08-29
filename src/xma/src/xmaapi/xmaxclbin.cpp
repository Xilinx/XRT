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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include "app/xmaerror.h"
#include "lib/xmaxclbin.h"

#define xma_logmsg(f_, ...) printf((f_), ##__VA_ARGS__)

/* Private function */
static int get_xclbin_iplayout(char *buffer, XmaIpLayout *layout);
static int get_xclbin_ipfreqs(char *buffer, uint16_t *freq_list);

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
    int rc;

    rc = get_xclbin_iplayout(buffer, info->ip_layout);
    if (rc != 0)
        return rc;

    rc = get_xclbin_ipfreqs(buffer, info->freq_list);
    if (rc != 0)
        return rc;

    return XMA_SUCCESS;
}

static int get_xclbin_iplayout(char *buffer, XmaIpLayout *layout)
{
    int rc = 0;
    xclmgmt_ioc_bitstream_axlf obj = {reinterpret_cast<axlf *>(buffer)};

    const axlf_section_header *ip_hdr = xclbin::get_axlf_section(obj.xclbin,
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
        rc = XMA_ERROR;

    return rc;
}

static int get_xclbin_ipfreqs(char *buffer,  uint16_t *freq_list)
{
    int rc = 0;
    xclmgmt_ioc_bitstream_axlf obj = {reinterpret_cast<axlf *>(buffer)};

    const axlf_section_header *em_hdr = xclbin::get_axlf_section(obj.xclbin,
                                                                EMBEDDED_METADATA);
    if (em_hdr)
    {
        char *data = &buffer[em_hdr->m_sectionOffset];
        xmlDocPtr doc = xmlReadMemory(data, em_hdr->m_sectionSize,
                                      "noname.xml", NULL, 0);
        if (doc == NULL)
        {
            xma_logmsg("XMA xclbin: Could not parse embedded metadata\n");
            rc = XMA_ERROR;
        }
        else
        {
            xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
            const xmlChar* xpathExpr = (const xmlChar*)"//clock";
            xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExpr,
                                                                xpathCtx);
            if (xpathObj == NULL)
            {
                xma_logmsg("XMA xclbin: Could not evaluate XPath\n");
                rc = XMA_ERROR;
            }
            else
            {
                for (int32_t i = 0; i < xpathObj->nodesetval->nodeNr; i++)
                {
                    xmlNode *node = xpathObj->nodesetval->nodeTab[i];
                    xmlChar *freq = xmlGetProp(node,
                                              (const xmlChar*)"frequency");
                    xma_logmsg("XMA xclbin: string freq[%d]=%s\n", i, freq);
                    freq_list[i] = (int)(atof((const char*)freq));
                    xma_logmsg("XMA xclbin: number freq[%d]=%d\n", i, freq_list[i]);
                }
            }
        }
    }
    else
        rc = XMA_ERROR;

    return rc;
}
