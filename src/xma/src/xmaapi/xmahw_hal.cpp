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
#include <vector>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <xclhal2.h>
//#include <xclbin.h>
#include "app/xmaerror.h"
#include "lib/xmaxclbin.h"
#include "lib/xmahw_hal.h"
#include "lib/xmahw_private.h"

#define xma_logmsg(f_, ...) printf((f_), ##__VA_ARGS__)

typedef struct XmaHALDevice
{
    xclDeviceHandle    handle;
    xclDeviceInfo2     info;
} XmaHALDevice;

/* Private helper functions */
static int get_device_list(XmaHALDevice   *xlnx_devices,
                           uint32_t       *device_count);

static void set_hw_cfg(uint32_t        device_count,
                       XmaHALDevice   *xlnx_devices,
                       XmaHwCfg       *hwcfg);

static int load_xclbin_to_device(xclDeviceHandle dev_handle, const char *xclbin_mem);

static int get_max_dev_id(XmaSystemCfg *systemcfg);

int get_device_list(XmaHALDevice   *xlnx_devices,
                    uint32_t       *device_count)
{
    int32_t      rc = 0;
    uint32_t     i;

    *device_count = xclProbe();
    for (i = 0; i < *device_count; i++)
    {
        xlnx_devices[i].handle = xclOpen(i, NULL, XCL_QUIET);
        printf("get_device_list xclOpen handle = %p\n",
            xlnx_devices[i].handle);
        rc = xclGetDeviceInfo2(xlnx_devices[i].handle, &xlnx_devices[i].info);
        if (rc != 0)
        {
            xma_logmsg("xclGetDeviceInfo2 failed for device id: %d, rc=%d\n",
                        i, rc);
            break;
        }
    }

    return rc;
}

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
        hwhal->dev_handle = xlnx_devices[i].handle;
        hwcfg->devices[i].handle = hwhal;
    }
}

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

/* Public function implementation */
int hal_probe(XmaHwCfg *hwcfg)
{
    XmaHALDevice   xlnx_devices[MAX_XILINX_DEVICES];
    uint32_t       device_count;

    xma_logmsg("Using HAL layer\n");

    /* There can be up to 16 Xilinx devices in a platform */
    if (get_device_list(xlnx_devices, &device_count) != 0)
        return XMA_ERROR;

    /* Populate the XmaHwCfg */
    set_hw_cfg(device_count, xlnx_devices, hwcfg);

    return XMA_SUCCESS;
}

bool hal_is_compatible(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg)
{
    int32_t num_devices_requested = 0;
    int32_t i;
    int32_t max_dev_id;

    max_dev_id = get_max_dev_id(systemcfg);

    /* Get number of devices requested in configuration */
    for (i = 0; i < systemcfg->num_images; i++)
        num_devices_requested += systemcfg->imagecfg[i].num_devices;

    /* Check number of devices requested is not greater than number in HW */
    if (num_devices_requested > hwcfg->num_devices ||
        max_dev_id > (hwcfg->num_devices - 1))
    {
        xma_logmsg("Requested %d devices but only %d devices found\n",
                   num_devices_requested, hwcfg->num_devices);
        xma_logmsg("Max device id specified in YAML cfg %d\n", max_dev_id);
        return false;
    }

    /* For each of the requested devices, check that the DSA name matches */
    for (i = 0; i < num_devices_requested; i++)
    {
        if (strcmp(systemcfg->dsa, hwcfg->devices[i].dsa) != 0)
        {
            xma_logmsg("DSA mismatch: requested %s found %s\n",
                       systemcfg->dsa, hwcfg->devices[i].dsa);
            return false;
        }
    }

    return true;
}

bool hal_configure(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg, bool hw_configured)
{
    std::string   xclbinpath = systemcfg->xclbinpath;
    XmaXclbinInfo info;
    int32_t ddr_table[] = {0, 3, 1, 2};

    /* Download the requested image to the associated device */
    /* Make sure to program the reference clock prior to download */
    for (int32_t i = 0; i < systemcfg->num_images; i++)
    {
        std::string xclbin = systemcfg->imagecfg[i].xclbin;
        std::string xclfullname = xclbinpath + "/" + xclbin;
        char *buffer = xma_xclbin_file_open(xclfullname.c_str());
        if (!buffer)
        {
            xma_logmsg("Could not open xclbin file %s\n",
                       xclfullname.c_str());
            return false;
        }
        int32_t rc = xma_xclbin_info_get(buffer, &info);
        if (rc != 0)
        {
            xma_logmsg("Could not get info for xclbin file %s\n",
                       xclfullname.c_str());
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
                     x < systemcfg->imagecfg[i].kernelcfg[k].instances;
                     x++, t++)
                {
                    strcpy((char*)hwcfg->devices[dev_id].kernels[t].name,
                       (const char*)info.ip_layout[t].kernel_name);
                    hwcfg->devices[dev_id].kernels[t].base_address =
                       info.ip_layout[t].base_addr;
                    int32_t ddr_bank = systemcfg->imagecfg[i].kernelcfg[k].ddr_map[x];
                    hwcfg->devices[dev_id].kernels[t].ddr_bank = ddr_table[ddr_bank];
                    printf("ddr_table value = %d\n",
                        hwcfg->devices[dev_id].kernels[t].ddr_bank);
                }
            }
            if (hw_configured)
                continue;
            /* Download xclbin first */
            rc = load_xclbin_to_device(hal->dev_handle, buffer);
            if (rc != 0)
            {
                xma_logmsg("Could not download xclbin file %s to device %d\n",
                           xclfullname.c_str(),
                           systemcfg->imagecfg[i].device_id_map[d]);
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
