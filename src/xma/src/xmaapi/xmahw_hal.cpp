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
//#include <vector>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "xrt.h"
#include "app/xmaerror.h"
#include "app/xmalogger.h"
#include "lib/xmaxclbin.h"
//#include "lib/xmahw_hal.h"
#include "lib/xmahw_private.h"
#include <dlfcn.h>
#include <iostream>
#include <bitset>
#include "ert.h"

//#define xma_logmsg(f_, ...) printf((f_), ##__VA_ARGS__)
#define XMAAPI_MOD "xmahw_hal"

using namespace std;
const uint64_t mNullBO = 0xffffffff;

/*
static void set_hw_cfg(uint32_t        device_count,
                       XmaHALDevice   *xlnx_devices,
                       XmaHwCfg       *hwcfg);
*/
//static int get_max_dev_id(XmaSystemCfg *systemcfg);
/*Sarab: This is redundant now
void set_hw_cfg(uint32_t        device_count,
                XmaHALDevice   *xlnx_devices,
                XmaHwCfg       *hwcfg)
{
    XmaHwHAL *hwhal = NULL;
    uint32_t   i;

    hwcfg->num_devices = device_count;

    for (i = 0; i < device_count; i++)
    {
        strcpy(hwcfg->devices[i].dsa, xlnx_devices[i].info.mName);
        hwhal = (XmaHwHAL*)malloc(sizeof(XmaHwHAL));
        memset(hwhal, 0, sizeof(XmaHwHAL));
        hwhal->dev_index = xlnx_devices[i].dev_index;//For execbo
        hwhal->dev_handle = xlnx_devices[i].handle;
        hwcfg->devices[i].handle = hwhal;
    }
}
*/
int load_xclbin_to_device(xclDeviceHandle dev_handle, const char *buffer)
{
    int rc;

    rc = xclLockDevice(dev_handle);
    if (rc != 0)
    {
        printf("Failed to lock device\n");
        return rc;
    }

    printf("load_xclbin_to_device handle = %p\n", dev_handle);
    rc = xclLoadXclBin(dev_handle, (const xclBin*)buffer);
    if (rc != 0)
        printf("xclLoadXclBin failed rc=%d\n", rc);

    return rc;
}

/*Sarab: Remove yaml system cfg stuff
int get_max_dev_id(XmaSystemCfg *systemcfg)
{
    int max_dev_id = -1;
    int i, d;

    for (i = 0; i < systemcfg->num_images; i++)
        for (d = 0; d < systemcfg->imagecfg[i].num_devices; d++)
            if (systemcfg->imagecfg[i].device_id_map[d] > max_dev_id)
                max_dev_id = systemcfg->imagecfg[i].device_id_map[d];

    return max_dev_id;
}
*/
 
/* Public function implementation */
int hal_probe(XmaHwCfg *hwcfg)
{
    xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Using HAL layer\n");
    if (hwcfg == NULL) 
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "ERROR: hwcfg is NULL\n");
        return XMA_ERROR;
    }

    //int32_t      rc = 0;

    hwcfg->num_devices = xclProbe();
    if (hwcfg->num_devices < 1) 
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "ERROR: No Xilinx device found\n");
        return XMA_ERROR;
    }
    /* Sarab: This is redundant now. Populate the XmaHwCfg */
    //set_hw_cfg(device_count, xlnx_devices, hwcfg);

    return XMA_SUCCESS;
}

/*Sarab: Remove yaml system cfg stuff 
bool hal_is_compatible(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg)
*/
bool hal_is_compatible(XmaHwCfg *hwcfg, XmaXclbinParameter *devXclbins, int32_t num_parms)
{
    /*
    int32_t num_devices_requested = 0;
    int32_t i;
    int32_t max_dev_id;

    max_dev_id = get_max_dev_id(systemcfg);

    /--* Get number of devices requested in configuration *--/
    for (i = 0; i < systemcfg->num_images; i++)
        num_devices_requested += systemcfg->imagecfg[i].num_devices;

    /--* Check number of devices requested is not greater than number in HW *--/
    if (num_devices_requested > hwcfg->num_devices ||
        max_dev_id > (hwcfg->num_devices - 1))
    {
        xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Requested %d devices but only %d devices found\n",
                   num_devices_requested, hwcfg->num_devices);
        xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "Max device id specified in YAML cfg %d\n", max_dev_id);
        return false;
    }

    /--* For each of the requested devices, check that the DSA name matches *--/
    for (i = 0; i < num_devices_requested; i++)
    {
        if (strcmp(systemcfg->dsa, hwcfg->devices[i].dsa) != 0)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Shell mismatch: requested %s found %s\n",
                       systemcfg->dsa, hwcfg->devices[i].dsa);
            return false;
        }
    }
*/
    return true;
}


//bool hal_configure(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg, bool hw_configured)
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
        std::string xclbin = std::string(devXclbins[i].xclbin_name);
        int32_t dev_index = devXclbins[i].device_id;
        if (dev_index >= hwcfg->num_devices || dev_index < 0) {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Illegal dev_index for xclbin to load into. dev_index = %d\n",
                       dev_index);
            return false;
        }
        char *buffer = xma_xclbin_file_open(xclbin.c_str());
        if (!buffer)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not open xclbin file %s\n",
                       xclbin.c_str());
            return false;
        }
        int32_t rc = xma_xclbin_info_get(buffer, &info);
        if (rc != XMA_SUCCESS)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not get info for xclbin file %s\n",
                       xclbin.c_str());
            free(buffer);
            return false;
        }

        hwcfg->devices.emplace_back(XmaHwDevice{});

        XmaHwDevice& dev_tmp1 = hwcfg->devices.back();
        //dev_tmp1.kernels.reserve(LIKELY_KERNEL_CONFIGS); Reserving below based on number of CUs

        dev_tmp1.handle = xclOpen(dev_index, NULL, XCL_QUIET);
        if (dev_tmp1.handle == NULL){
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Unable to open device  id: %d\n", dev_index);
            free(buffer);
            return false;
        }
        dev_tmp1.dev_index = dev_index;
        xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "xclOpen handle = %p\n",
            dev_tmp1.handle);
        rc = xclGetDeviceInfo2(dev_tmp1.handle, &dev_tmp1.info);
        if (rc != 0)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "xclGetDeviceInfo2 failed for device id: %d, rc=%d\n", dev_index, rc);
            free(buffer);
            return false;
        }

        /* Always attempt download xclbin */
        rc = load_xclbin_to_device(dev_tmp1.handle, buffer);
        if (rc != 0) {
            free(buffer);
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not download xclbin file %s to device %d\n",
                        xclbin.c_str(), dev_index);
            return false;
        }
        uuid_copy(dev_tmp1.uuid, info.uuid); 
        dev_tmp1.number_of_cus = info.number_of_kernels;
        dev_tmp1.number_of_mem_banks = info.number_of_mem_banks;
        if (dev_tmp1.number_of_cus > MAX_XILINX_KERNELS + MAX_XILINX_SOFT_KERNELS) {
            free(buffer);
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not download xclbin file %s to device %d\n",
                        xclbin.c_str(), dev_index);
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA & XRT supports max of %d CUs but xclbin has %d number of CUs\n", MAX_XILINX_KERNELS + MAX_XILINX_SOFT_KERNELS, dev_tmp1.number_of_cus);
            return false;
        }
        if (dev_tmp1.number_of_mem_banks > MAX_DDR_MAP) {
            free(buffer);
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA supports max of only %d mem banks\n", MAX_DDR_MAP);
            return false;
        }
        dev_tmp1.kernels.reserve(dev_tmp1.number_of_cus);
        dev_tmp1.ddrs.reserve(dev_tmp1.number_of_mem_banks);

        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\nFor device id: %d; DDRs are:", dev_index);
        for (uint32_t d = 0; d < info.number_of_mem_banks; d++) {
            dev_tmp1.ddrs.emplace_back(XmaHwMem{});
            XmaHwMem& tmp1 = dev_tmp1.ddrs.back();
            //tmp1.name = std::string((char*)info.mem_topology[d].m_tag);
            memset((void*)tmp1.name, 0x0, MAX_KERNEL_NAME);
            std::string str_tmp1 = std::string((char*)info.mem_topology[d].m_tag);
            str_tmp1.copy((char*)tmp1.name, MAX_KERNEL_NAME-1);

            tmp1.base_address = info.mem_topology[d].m_base_address;
            tmp1.size_kb = info.mem_topology[d].m_size;
            tmp1.size_mb = tmp1.size_kb / 1024;
            tmp1.size_gb = tmp1.size_mb / 1024;
            if (info.mem_topology[d].m_used == 1 &&
                tmp1.size_kb != 0 &&
                (info.mem_topology[d].m_type == MEM_TYPE::MEM_DDR3 || 
                info.mem_topology[d].m_type == MEM_TYPE::MEM_DDR4 ||
                info.mem_topology[d].m_type == MEM_TYPE::MEM_DRAM ||
                info.mem_topology[d].m_type == MEM_TYPE::MEM_HBM)
                ) {
                tmp1.in_use = true;
                xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tMEM# %d - %s - size: %d KB", d, (char*)tmp1.name, tmp1.size_kb);
            } else {
                xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tMEM# %d - %s - Unused/Unused Type", d, (char*)tmp1.name);

            }
        }

        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\nFor device id: %d; CUs are:", dev_index);
        for (uint32_t d = 0; d < info.number_of_kernels; d++) {
            dev_tmp1.kernels.emplace_back(XmaHwKernel{});
            XmaHwKernel& tmp1 = dev_tmp1.kernels.back();
            memset((void*)tmp1.name, 0x0, MAX_KERNEL_NAME);
            std::string str_tmp1 = std::string((char*)info.ip_layout[d].kernel_name);
            str_tmp1.copy((char*)tmp1.name, MAX_KERNEL_NAME-1);

            tmp1.base_address = info.ip_layout[d].base_addr;
            tmp1.cu_index = (int32_t)d;
            if (info.ip_layout[d].soft_kernel) {
                tmp1.soft_kernel = true;
                tmp1.default_ddr_bank = 0;
            } else {
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
                    XmaAXLFConnectivity *xma_conn = &info.connectivity[c];
                    if (xma_conn->m_ip_layout_index == (int32_t)d) {
                        tmp1.CU_arg_to_mem_info.emplace(xma_conn->arg_index, xma_conn->mem_data_index);
                        //Assume that this mem is definetly in use
                        if ((uint32_t)xma_conn->mem_data_index < dev_tmp1.number_of_mem_banks && xma_conn->mem_data_index > 0) {
                            dev_tmp1.ddrs[xma_conn->mem_data_index].in_use = true;
                        }
                    }
                }

                if (tmp1.default_ddr_bank < 0) {
                    xma_logmsg(XMA_WARNING_LOG, XMAAPI_MOD,"\tCU# %d - %s - default DDR bank: NONE", d, (char*)tmp1.name);
                } else {
                    xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"\tCU# %d - %s - default DDR bank:%d", d, (char*)tmp1.name, tmp1.default_ddr_bank);
                }
                if (xclOpenContext(dev_tmp1.handle, info.uuid, d, true) != 0) {
                    free(buffer);
                    xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Failed to open context to this CU\n");
                    return false;
                }
            }
        }

        std::bitset<MAX_XILINX_KERNELS> cu_mask;
        uint64_t base_addr1 = 0;
        for (uint32_t d1 = 0; d1 < info.number_of_hardware_kernels; d1++) {
            base_addr1 = dev_tmp1.kernels[d1].base_address;
            //uint64_t cu_mask = 1;
            cu_mask.reset();
            cu_mask.set(0);

            for (uint32_t d2 = 0; d2 < info.number_of_kernels; d2++) {
                if (d1 != d2) {
                    if (dev_tmp1.kernels[d2].base_address < base_addr1) {
                        cu_mask = cu_mask << 1;
                    }
                }
            }
            //dev_tmp1.kernels[d1].cu_mask0 = cu_mask & 0xFFFFFFFF;
            //dev_tmp1.kernels[d1].cu_mask1 = ((uint64_t)(cu_mask >> 32)) & 0xFFFFFFFF;
            dev_tmp1.kernels[d1].cu_mask0 = cu_mask.to_ulong();
            cu_mask = cu_mask >> 32;
            dev_tmp1.kernels[d1].cu_mask1 = cu_mask.to_ulong();
            cu_mask = cu_mask >> 32;
            dev_tmp1.kernels[d1].cu_mask2 = cu_mask.to_ulong();
            cu_mask = cu_mask >> 32;
            dev_tmp1.kernels[d1].cu_mask3 = cu_mask.to_ulong();
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

        int32_t num_execbo = 0;
        if (dev_tmp1.number_of_cus > MIN_EXECBO_POOL_SIZE) {
            num_execbo = dev_tmp1.number_of_cus;
        } else {
            num_execbo = MIN_EXECBO_POOL_SIZE;
        }
        dev_tmp1.kernel_execbos.reserve(num_execbo);
        dev_tmp1.num_execbo_allocated = num_execbo;
        for (int32_t d = 0; d < num_execbo; d++) {
            uint32_t  bo_handle;
            int       execBO_size = MAX_EXECBO_BUFF_SIZE;
            //uint32_t  execBO_flags = (1<<31);
            char     *bo_data;
            bo_handle = xclAllocBO(dev_tmp1.handle, 
                                    execBO_size, 
                                    0, 
                                    XCL_BO_FLAGS_EXECBUF);
            if (!bo_handle || bo_handle == mNullBO) 
            {
                free(buffer);
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Unable to create bo for cu start\n");
                return false;
            }
            bo_data = (char*)xclMapBO(dev_tmp1.handle, bo_handle, true);
            memset((void*)bo_data, 0x0, execBO_size);

            dev_tmp1.kernel_execbos.emplace_back(XmaHwExecBO{});
            XmaHwExecBO& dev_execbo = dev_tmp1.kernel_execbos.back();
            dev_execbo.handle = bo_handle;
            dev_execbo.data = bo_data;
        }

        free(buffer);
    }

    return true;
}

XmaHwInterface hw_if = {
    .probe         = hal_probe,
    .is_compatible = hal_is_compatible,
    .configure     = hal_configure
};
