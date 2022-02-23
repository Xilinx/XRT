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
#include "core/common/device.h"
#include <dlfcn.h>
#include <iostream>
#include <bitset>
#include "ert.h"

//#define xma_logmsg(f_, ...) printf((f_), ##__VA_ARGS__)
#define XMAAPI_MOD "xmahw_hal"

using namespace std;

/* Public function implementation */
int hal_probe(XmaHwCfg *hwcfg)
{
    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Using HAL layer\n");
    if (hwcfg == nullptr)
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "ERROR: hwcfg is nullptr\n");
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
    if (hwcfg == nullptr) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "hwcfg is nullptr\n");
        return false;
    }

    if (num_parms > hwcfg->num_devices) {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Num of Xilinx device is less than num of XmaXclbinParameters as input\n");
        return false;
    }

    /* Download the requested image to the associated device */
    for (int32_t i = 0; i < num_parms; i++) {
        XmaXclbinInfo info;
        int32_t dev_index = devXclbins[i].device_id;
        if (dev_index >= hwcfg->num_devices || dev_index < 0) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Illegal dev_index for xclbin to load into. dev_index = %d\n",
                       dev_index);
            return false;
        }
        if (devXclbins[i].xclbin_name == nullptr) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "No xclbin provided for dev_index = %d\n",
                       dev_index);
            return false;
        }
        std::string xclbin_file = std::string(devXclbins[i].xclbin_name);
        std::vector<char> xclbin_buffer = xma_xclbin_file_open(xclbin_file);
        char *buffer = xclbin_buffer.data();

        int32_t rc = xma_xclbin_info_get(xclbin_file, &info);
        if (rc != XMA_SUCCESS)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not get info for xclbin file %s\n",
                xclbin_file.c_str());
            return false;
        }

        hwcfg->devices.emplace_back(XmaHwDevice{});
        XmaHwDevice& dev_tmp1 = hwcfg->devices.back();
        dev_tmp1.xrt_device = xrt::device(dev_index);
        if (dev_tmp1.xrt_device.get_handle()->get_device_handle() == nullptr){
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Unable to open device  id: %d\n", dev_index);
            return false;
        }
        dev_tmp1.dev_index = dev_index;
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "Device handle = %p\n", dev_tmp1.xrt_device.get_handle()->get_device_handle());   

        /* Always attempt download xclbin */     
        try {
            dev_tmp1.xrt_device.load_xclbin((const xclBin*)buffer);
        }
         catch (const xrt_core::system_error&) {
             xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not download xclbin file %s to device %d\n",
                 xclbin_file.c_str(), dev_index);
             return false;
         }    

        auto xclbin_ax = reinterpret_cast<const axlf*>(buffer);
        uuid_copy(dev_tmp1.uuid, xclbin_ax->m_header.uuid);
        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\nFor device id: %d; CUs are:", dev_index);

        for (int32_t d = 0; d < (int32_t)info.ip_vec.size(); d++) {
            dev_tmp1.kernels.emplace_back(XmaHwKernel{});
            XmaHwKernel& tmp1 = dev_tmp1.kernels.back();
            memset((void*)tmp1.name, 0x0, MAX_KERNEL_NAME);
            info.ip_vec[d].copy((char*)tmp1.name, MAX_KERNEL_NAME - 1);
            tmp1.cu_index = (int32_t)d;
            //Allow default ddr_bank of -1; When CU is not connected to any ddr
            xma_xclbin_map2ddr(info.ip_ddr_mapping[d], &tmp1.default_ddr_bank, info.has_mem_groups);
            //XMA now supports multiple DDR Banks per Kernel
            tmp1.ip_ddr_mapping = info.ip_ddr_mapping[d];
            tmp1.CU_arg_to_mem_info = info.ip_arg_connections[d];
            if (tmp1.default_ddr_bank < 0) {
                xma_logmsg(XMA_WARNING_LOG, XMAAPI_MOD, "\tCU# %d - %s - default DDR bank: NONE", d, (char*)tmp1.name);
            }
            else {
                xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "\tCU# %d - %s - default DDR bank:%d", d, (char*)tmp1.name, tmp1.default_ddr_bank);
            }
        }
        dev_tmp1.number_of_hardware_kernels = dev_tmp1.kernels.size();
        uint32_t num_soft_kernels = 0;
        //Handle soft kernels just like another hardware IP_Layout kernel
        //soft kernels to follow hardware kernels. so soft kenrel index will start after hardware kernels
        auto soft_kernels = xrt_core::xclbin::get_softkernels(xclbin_ax);
        for (auto& sk : soft_kernels) {
            if (num_soft_kernels + sk.ninst > MAX_XILINX_SOFT_KERNELS) {
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA supports max of only %d soft kernels per device ", MAX_XILINX_SOFT_KERNELS);
                throw std::runtime_error("XMA supports max of only " + std::to_string(MAX_XILINX_SOFT_KERNELS) + " soft kernels per device");
            }
            xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "soft kernel name = %s, version = %s, symbol name = %s, num of instances = %d ", sk.mpo_name.c_str(), sk.mpo_version.c_str(), sk.symbol_name.c_str(), sk.ninst);
            std::string str_tmp1;
            for (uint32_t ind = 0; ind < sk.ninst; ind++) {
                dev_tmp1.kernels.emplace_back(XmaHwKernel{});
                XmaHwKernel& tmp1 = dev_tmp1.kernels.back();
                memset((void*)tmp1.name, 0x0, MAX_KERNEL_NAME);
                str_tmp1 = std::string(sk.mpo_name);
                str_tmp1 += "_";
                str_tmp1 += std::to_string(ind);
                str_tmp1.copy((char*)tmp1.name, MAX_KERNEL_NAME - 1);             
                tmp1.soft_kernel = true;
                tmp1.default_ddr_bank = 0;
                tmp1.cu_index = dev_tmp1.number_of_hardware_kernels + ind;
                num_soft_kernels++;
            }
        }

        dev_tmp1.number_of_cus = dev_tmp1.kernels.size();     
        if (dev_tmp1.number_of_cus > MAX_XILINX_KERNELS + MAX_XILINX_SOFT_KERNELS) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not download xclbin file %s to device %d\n",
                xclbin_file.c_str(), dev_index);
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA & XRT supports max of %d CUs but xclbin has %d number of CUs\n", MAX_XILINX_KERNELS + MAX_XILINX_SOFT_KERNELS, dev_tmp1.number_of_cus);
            return false;
        }
        
        if (dev_tmp1.number_of_hardware_kernels > 0) {
            //Avoid virtual cu context as it takes 40 millisec
            //dev_tmp1.xrt_device.get_handle()->open_context(info.uuid, dev_tmp1.kernels[0].cu_index_ert, true);
            dev_tmp1.kernels[0].context_opened = true;
        } else {
            //Opening virtual CU context as some applications may use soft kernels only
            //But this takes 40 millisec
            try {
                dev_tmp1.xrt_device.get_handle()->open_context(dev_tmp1.uuid, -1, true);
            }
            catch (const xrt_core::system_error&) {
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
