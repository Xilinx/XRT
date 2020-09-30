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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "xrt.h"
#include "app/xmaerror.h"
#include "app/xmalogger.h"
#include "lib/xmaxclbin.h"
#include "lib/xmahw_private.h"
#include <dlfcn.h>
#include <iostream>
#include <bitset>
#include "ert.h"

//#define xma_logmsg(f_, ...) printf((f_), ##__VA_ARGS__)
#define XMAAPI_MOD "xmahw_hal"

using namespace std;

int load_xclbin_to_device(xclDeviceHandle dev_handle, const char *buffer)
{
    int rc;

    printf("load_xclbin_to_device handle = %p\n", dev_handle);
    rc = xclLoadXclBin(dev_handle, (const xclBin*)buffer);
    if (rc != 0)
        printf("xclLoadXclBin failed rc=%d\n", rc);

    return rc;
}

/* Public function implementation */
int hal_probe(XmaHwCfg *hwcfg)
{
    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Using HAL layer\n");
    if (hwcfg == NULL) 
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "ERROR: hwcfg is NULL\n");
        return XMA_ERROR;
    }

    hwcfg->num_devices = xclProbe();
    if (hwcfg->num_devices < 1) 
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "ERROR: No Xilinx device found\n");
        return XMA_ERROR;
    }

    return XMA_SUCCESS;
}

bool hal_is_compatible(XmaHwCfg *hwcfg, XmaXclbinParameter *devXclbins, int32_t num_parms)
{
    return true;
}


bool hal_configure(XmaHwCfg *hwcfg, XmaXclbinParameter *devXclbins, int32_t num_parms)
{
    XmaXclbinInfo info;
    if (hwcfg == NULL) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "hwcfg is NULL\n");
        return false;
    }

    if (num_parms > hwcfg->num_devices) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Num of Xilinx device is less than num of XmaXclbinParameters as input\n");
        return false;
    }

    /* Download the requested image to the associated device */
    for (int32_t i = 0; i < num_parms; i++) {
        int32_t dev_index = devXclbins[i].device_id;
        if (dev_index >= hwcfg->num_devices || dev_index < 0) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Illegal dev_index for xclbin to load into. dev_index = %d\n",
                       dev_index);
            return false;
        }
        if (devXclbins[i].xclbin_name == NULL) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "No xclbin provided for dev_index = %d\n",
                       dev_index);
            return false;
        }
        std::string xclbin = std::string(devXclbins[i].xclbin_name);
        std::vector<char> xclbin_buffer = xma_xclbin_file_open(xclbin);
        char *buffer = xclbin_buffer.data();

        int32_t rc = xma_xclbin_info_get(buffer, &info);
        if (rc != XMA_SUCCESS)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not get info for xclbin file %s\n",
                       xclbin.c_str());
            return false;
        }

        hwcfg->devices.emplace_back(XmaHwDevice{});

        XmaHwDevice& dev_tmp1 = hwcfg->devices.back();

        dev_tmp1.handle = xclOpen(dev_index, NULL, XCL_QUIET);
        if (dev_tmp1.handle == NULL){
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Unable to open device  id: %d\n", dev_index);
            return false;
        }

        dev_tmp1.dev_index = dev_index;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "xclOpen handle = %p\n", dev_tmp1.handle);
        /* This is adding to start delay
        rc = xclGetDeviceInfo2(dev_tmp1.handle, &dev_tmp1.info);
        if (rc != 0)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "xclGetDeviceInfo2 failed for device id: %d, rc=%d\n", dev_index, rc);
            return false;
        }
        */

        /* Always attempt download xclbin */
        rc = load_xclbin_to_device(dev_tmp1.handle, buffer);
        if (rc != 0) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not download xclbin file %s to device %d\n",
                        xclbin.c_str(), dev_index);
            return false;
        }

        uuid_copy(dev_tmp1.uuid, info.uuid); 
        dev_tmp1.number_of_cus = info.number_of_kernels;
        dev_tmp1.number_of_mem_banks = info.number_of_mem_banks;
        if (dev_tmp1.number_of_cus > MAX_XILINX_KERNELS + MAX_XILINX_SOFT_KERNELS) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not download xclbin file %s to device %d\n",
                        xclbin.c_str(), dev_index);
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA & XRT supports max of %d CUs but xclbin has %d number of CUs\n", MAX_XILINX_KERNELS + MAX_XILINX_SOFT_KERNELS, dev_tmp1.number_of_cus);
            return false;
        }
        if (dev_tmp1.number_of_mem_banks > MAX_DDR_MAP) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA supports max of only %d mem banks\n", MAX_DDR_MAP);
            return false;
        }
        dev_tmp1.kernels.reserve(dev_tmp1.number_of_cus);
        dev_tmp1.ddrs.reserve(dev_tmp1.number_of_mem_banks);
        dev_tmp1.number_of_hardware_kernels = info.number_of_hardware_kernels;

        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\nFor device id: %d; DDRs are:", dev_index);
        auto& xma_mem_topology = info.mem_topology;
        for (uint32_t d = 0; d < info.number_of_mem_banks; d++) {
            dev_tmp1.ddrs.emplace_back(XmaHwMem{});
            XmaHwMem& tmp1 = dev_tmp1.ddrs.back();
            memset((void*)tmp1.name, 0x0, MAX_KERNEL_NAME);
            xma_mem_topology[d].m_tag.copy((char*)tmp1.name, MAX_KERNEL_NAME-1);

            tmp1.base_address = xma_mem_topology[d].m_base_address;
            tmp1.size_kb = xma_mem_topology[d].m_size;
            tmp1.size_mb = tmp1.size_kb / 1024;
            tmp1.size_gb = tmp1.size_mb / 1024;
            if (xma_mem_topology[d].m_used == 1 &&
                tmp1.size_kb != 0 &&
                (xma_mem_topology[d].m_type == MEM_TYPE::MEM_DDR3 || 
                xma_mem_topology[d].m_type == MEM_TYPE::MEM_DDR4 ||
                xma_mem_topology[d].m_type == MEM_TYPE::MEM_DRAM ||
                xma_mem_topology[d].m_type == MEM_TYPE::MEM_HBM)
                ) {
                tmp1.in_use = true;
                xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tMEM# %d - %s - size: %lu KB", d, (char*)tmp1.name, tmp1.size_kb);
            } else {
                xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tMEM# %d - %s - Unused/Unused Type", d, (char*)tmp1.name);

            }
        }

        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\nFor device id: %d; CUs are:", dev_index);
        for (uint32_t d = 0; d < info.number_of_kernels; d++) {
            dev_tmp1.kernels.emplace_back(XmaHwKernel{});
            XmaHwKernel& tmp1 = dev_tmp1.kernels.back();
            memset((void*)tmp1.name, 0x0, MAX_KERNEL_NAME);
            info.ip_layout[d].kernel_name.copy((char*)tmp1.name, MAX_KERNEL_NAME-1);

            tmp1.base_address = info.ip_layout[d].base_addr;
            tmp1.cu_index = (int32_t)d;
            if (info.ip_layout[d].soft_kernel) {
                tmp1.soft_kernel = true;
                tmp1.default_ddr_bank = 0;
            } else {
                tmp1.arg_start = info.ip_layout[d].arg_start;
                tmp1.regmap_size = info.ip_layout[d].regmap_size;

                if (info.ip_layout[d].kernel_channels) {
                    tmp1.kernel_channels = true;
                    tmp1.max_channel_id = info.ip_layout[d].max_channel_id;
                }
                //Allow default ddr_bank of -1; When CU is not connected to any ddr
                xma_xclbin_map2ddr(info.ip_ddr_mapping[d], &tmp1.default_ddr_bank);

                //XMA now supports multiple DDR Banks per Kernel
                tmp1.ip_ddr_mapping = info.ip_ddr_mapping[d];
                for(uint32_t c = 0; c < info.number_of_connections; c++)
                {
                    auto& xma_conn = info.connectivity[c];
                    if (xma_conn.m_ip_layout_index == (int32_t)d) {
                        tmp1.CU_arg_to_mem_info.emplace(xma_conn.arg_index, xma_conn.mem_data_index);
                        //Assume that this mem is definetly in use
                        if ((uint32_t)xma_conn.mem_data_index < dev_tmp1.number_of_mem_banks && xma_conn.mem_data_index > 0) {
                            dev_tmp1.ddrs[xma_conn.mem_data_index].in_use = true;
                        }
                    }
                }

                if (tmp1.default_ddr_bank < 0) {
                    xma_logmsg(XMA_WARNING_LOG, XMAAPI_MOD,"\tCU# %d - %s - default DDR bank: NONE", d, (char*)tmp1.name);
                } else {
                    xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tCU# %d - %s - default DDR bank:%d", d, (char*)tmp1.name, tmp1.default_ddr_bank);
                }
                /* Not to open context on all CUs
                Will open during session_create
                if (xclOpenContext(dev_tmp1.handle, info.uuid, d, true) != 0) {
                    xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Failed to open context to this CU\n");
                    return false;
                }
                */
            }
        }

        std::bitset<MAX_XILINX_KERNELS> cu_mask;
        uint64_t base_addr1 = 0;
        uint32_t cu_index_ert = 0;
        for (uint32_t d1 = 0; d1 < info.number_of_hardware_kernels; d1++) {
            base_addr1 = dev_tmp1.kernels[d1].base_address;
            //uint64_t cu_mask = 1;
            cu_mask.reset();
            cu_mask.set(0);
            cu_index_ert = 0;

            for (uint32_t d2 = 0; d2 < info.number_of_hardware_kernels; d2++) {
                if (base_addr1 == info.cu_addrs_sorted[d2]) {
                    break;
                }
                cu_mask = cu_mask << 1;
                cu_index_ert++;
                /*
                if (d1 != d2) {
                    if (dev_tmp1.kernels[d2].base_address < base_addr1) {
                        cu_mask = cu_mask << 1;
                        cu_index_ert++;
                    }
                }
                */
            }
            dev_tmp1.kernels[d1].cu_index_ert = cu_index_ert;
            //dev_tmp1.kernels[d1].cu_mask0 = cu_mask & 0xFFFFFFFF;
            //dev_tmp1.kernels[d1].cu_mask1 = ((uint64_t)(cu_mask >> 32)) & 0xFFFFFFFF;
            dev_tmp1.kernels[d1].cu_mask0 = cu_mask.to_ulong();
            cu_mask = cu_mask >> 32;
            dev_tmp1.kernels[d1].cu_mask1 = cu_mask.to_ulong();
            cu_mask = cu_mask >> 32;
            dev_tmp1.kernels[d1].cu_mask2 = cu_mask.to_ulong();
            cu_mask = cu_mask >> 32;
            dev_tmp1.kernels[d1].cu_mask3 = cu_mask.to_ulong();
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tCU# %d - %s - cu_mask0: 0x%x", d1, (char*)dev_tmp1.kernels[d1].name, dev_tmp1.kernels[d1].cu_mask0);
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tCU# %d - %s - cu_mask1: 0x%x", d1, (char*)dev_tmp1.kernels[d1].name, dev_tmp1.kernels[d1].cu_mask1);
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tCU# %d - %s - cu_mask2: 0x%x", d1, (char*)dev_tmp1.kernels[d1].name, dev_tmp1.kernels[d1].cu_mask2);
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tCU# %d - %s - cu_mask3: 0x%x", d1, (char*)dev_tmp1.kernels[d1].name, dev_tmp1.kernels[d1].cu_mask3);
        }

        cu_mask.reset();
        cu_mask.set(0);
        std::bitset<MAX_XILINX_KERNELS> cu_mask_tmp;
        for (uint32_t d1 = info.number_of_hardware_kernels; d1 < info.number_of_kernels; d1++) {
            cu_mask_tmp = cu_mask;
            dev_tmp1.kernels[d1].cu_mask0 = cu_mask_tmp.to_ulong();
            cu_mask_tmp = cu_mask_tmp >> 32;
            dev_tmp1.kernels[d1].cu_mask1 = cu_mask_tmp.to_ulong();
            cu_mask_tmp = cu_mask_tmp >> 32;
            dev_tmp1.kernels[d1].cu_mask2 = cu_mask_tmp.to_ulong();
            cu_mask_tmp = cu_mask_tmp >> 32;
            dev_tmp1.kernels[d1].cu_mask3 = cu_mask_tmp.to_ulong();

            cu_mask = cu_mask << 1;
        }

        if (dev_tmp1.number_of_hardware_kernels > 0) {
            //Avoid virtual cu context as it takes 40 millisec
            xclOpenContext(dev_tmp1.handle, info.uuid, dev_tmp1.kernels[0].cu_index_ert, true);
            dev_tmp1.kernels[0].context_opened = true;
        } else {
            //Opening virtual CU context as some applications may use soft kernels only
            //But this takes 40 millisec
            if (xclOpenContext(dev_tmp1.handle, info.uuid, -1, true) != 0) {
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Failed to open virtual CU context\n");
                return false;
            }
        }
    }

    return true;
}

XmaHwInterface hw_if = {
    .probe         = hal_probe,
    .is_compatible = hal_is_compatible,
    .configure     = hal_configure
};
