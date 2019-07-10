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
#include "lib/xmahw_hal.h"
#include "lib/xmahw_private.h"
#include <dlfcn.h>
#include <iostream>
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
 
int32_t create_contexts(xclDeviceHandle handle, XmaXclbinInfo &info)
{
    for (uint32_t i = 0; i < info.num_ips; i++)
    {
        if (xclOpenContext(handle, info.uuid, i, true) != 0)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Failed to open context\n");
            return XMA_ERROR;
        }
    }

    return XMA_SUCCESS;
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

    int32_t      rc = 0;

    hwcfg->num_devices = xclProbe();
    if (hwcfg->num_devices < 1) 
    {
        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "ERROR: No Xilinx device found\n");
        return XMA_ERROR;
    }
    for (int32_t i = 0; i < hwcfg->num_devices; i++)
    {
        hwcfg->devices.emplace_back(XmaHwDevice{});

        XmaHwDevice& tmp1 = hwcfg->devices.back();
        tmp1.kernels.reserve(MAX_KERNEL_CONFIGS);

        tmp1.handle = xclOpen(i, NULL, XCL_QUIET);
        tmp1.dev_index = i;
        xma_logmsg(XMA_INFO_LOG, XMAAPI_MOD, "get_device_list xclOpen handle = %p\n",
            tmp1.handle);
        rc = xclGetDeviceInfo2(tmp1.handle, &tmp1.info);
        if (rc != 0)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "xclGetDeviceInfo2 failed for device id: %d, rc=%d\n",
                        i, rc);
            return XMA_ERROR;
        }
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
{/*Sarab: Remove yaml system cfg stuff
    std::string   xclbinpath = systemcfg->xclbinpath;
    XmaXclbinInfo info;

    /--* Download the requested image to the associated device *--/
    for (int32_t i = 0; i < systemcfg->num_images; i++)
    {
        std::string xclbin = systemcfg->imagecfg[i].xclbin;
        std::string xclfullname = xclbinpath + "/" + xclbin;
        char *buffer = xma_xclbin_file_open(xclfullname.c_str());
        if (!buffer)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not open xclbin file %s\n",
                       xclfullname.c_str());
            return false;
        }
        int32_t rc = xma_xclbin_info_get(buffer, &info);
        if (rc != XMA_SUCCESS)
        {
            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not get info for xclbin file %s\n",
                       xclfullname.c_str());
            free(buffer);
            return false;
        }

        for (int32_t d = 0; d < systemcfg->imagecfg[i].num_devices; d++)
        {
            int32_t dev_id = systemcfg->imagecfg[i].device_id_map[d];
            XmaHwHAL *hal = (XmaHwHAL*)hwcfg->devices[dev_id].handle;

            for (int32_t k = 0, t = 0;
                 t < MAX_KERNEL_CONFIGS &&
                 k < systemcfg->imagecfg[i].num_kernelcfg_entries; k++)
            {
                for (int32_t x = 0;
                     t < MAX_KERNEL_CONFIGS &&
                     x < systemcfg->imagecfg[i].kernelcfg[k].instances;
                     x++, t++)
                {
                    strcpy((char*)hwcfg->devices[dev_id].kernels[t].name,
                       (const char*)info.ip_layout[t].kernel_name);
                    xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD,"[%d] %s \t",
                               t, hwcfg->devices[dev_id].kernels[t].name);
                    hwcfg->devices[dev_id].kernels[t].base_address =
                       info.ip_layout[t].base_addr;
                    int32_t ip_ddr_map = info.ip_ddr_mapping[t];
                    int num_ddr_used = 0;
                    int ddr_banks[MAX_DDR_MAP] = {-1};
                    xma_xclbin_map2ddr(ip_ddr_map, ddr_banks, &num_ddr_used);
                    for(int d=0; d < num_ddr_used; d++)
                    {
                        xma_logmsg(XMA_DEBUG_LOG, XMAAPI_MOD, "[%d] ddr_table value-xclbin = %d\n",
                        t, ddr_banks[d]);
                    }
                    //HHS currently the support is just for 1 Bank per 1 Kernel support
                    hwcfg->devices[dev_id].kernels[t].ddr_bank = ddr_banks[0];
                }
            }

            /--* Always attempt download xclbin *--/
            rc = load_xclbin_to_device(hal->dev_handle, buffer);
            if (rc != 0)
            {
                free(buffer);
                xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Could not download xclbin file %s to device %d\n",
                           xclfullname.c_str(),
                           systemcfg->imagecfg[i].device_id_map[d]);
                return false;
            }

            /--* Create all kernel contexts on the device *--/
            rc = create_contexts(hal->dev_handle, info);
            if (rc != XMA_SUCCESS)
            {
                free(buffer);
                return false;
	    }

            //Setup execbo for use with kernel commands
            for (int32_t k = 0, t = 0;
                t < MAX_KERNEL_CONFIGS &&
                k < systemcfg->imagecfg[i].num_kernelcfg_entries; k++) 
            {
                for (int32_t x = 0;
                     x < systemcfg->imagecfg[i].kernelcfg[k].instances;
                     x++, t++) 
                {
                    bool found = false;
                    uint32_t cu_bit_mask = 1;
                    for (uint32_t i_ips = 0; i_ips < info.num_ips; i_ips++) 
                    {
                        if (info.ip_layout[i_ips].base_addr == 
                            hwcfg->devices[dev_id].kernels[t].base_address) 
                        {
                            found = true;
                        } else if (info.ip_layout[i_ips].base_addr <
                            hwcfg->devices[dev_id].kernels[t].base_address) {
                            cu_bit_mask = cu_bit_mask << 1;
                        }
                    }
                    if (!found) 
                    {
                        free(buffer);
                        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "CU not found. Couldn't create cu_cmd execbo\n");
                        return false;
                    }
                    if (cu_bit_mask == 0) 
                    {
                        free(buffer);
                        xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "XMA library doesn't support more than 32 CUs\n");
                        return false;
                    }
                    for (int i_execbo = 0; i_execbo < MAX_EXECBO_POOL_SIZE; i_execbo++) 
                    {
                        uint32_t  bo_handle;
                        int       execBO_size = MAX_EXECBO_BUFF_SIZE;
                        uint32_t  execBO_flags = (1<<31);
                        char     *bo_data;
                        bo_handle = xclAllocBO(hal->dev_handle, 
                                               execBO_size, 
                                               0, 
                                               execBO_flags);
                        if (!bo_handle || bo_handle == mNullBO) 
                        {
                            free(buffer);
                            xma_logmsg(XMA_ERROR_LOG, XMAAPI_MOD, "Unable to create bo for cu start\n");
                            return false;
                        }
                        bo_data = (char*)xclMapBO(hal->dev_handle, bo_handle, true);
                        memset((void*)bo_data, 0x0, execBO_size);
                        hwcfg->devices[dev_id].kernels[t].kernel_execbo_handle[i_execbo] = 
                            bo_handle;
                        hwcfg->devices[dev_id].kernels[t].kernel_execbo_data[i_execbo] = 
                            bo_data;
                        hwcfg->devices[dev_id].kernels[t].kernel_execbo_inuse[i_execbo] = 
                            false;
                        ert_start_kernel_cmd* cu_start_cmd = (ert_start_kernel_cmd*) bo_data;
                        cu_start_cmd->state = ERT_CMD_STATE_NEW;
                        cu_start_cmd->opcode = ERT_START_CU;
                        cu_start_cmd->cu_mask = cu_bit_mask;
                    }
                }
            }
        }
        free(buffer);
    }
*/
    return true;
}

XmaHwInterface hw_if = {
    .probe         = hal_probe,
    .is_compatible = hal_is_compatible,
    .configure     = hal_configure
};
