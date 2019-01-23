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
#ifndef _XMA_HW_H_
#define _XMA_HW_H_

#include <stdint.h>
#include <stdbool.h>
#include "lib/xmacfg.h"
#include "lib/xmalimits.h"

#define MAX_EXECBO_POOL_SIZE      16

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @file
 */

/**
 * @addtogroup xmahw
 * @{
 */

typedef void   *XmaHwHandle;

typedef struct XmaHwKernel
{
    uint8_t     name[MAX_KERNEL_NAME];
    bool        in_use;
    int32_t     instance;
    uint64_t    base_address;
    uint32_t    ddr_bank;
    //For execbo:
    void*       kernel_cmd_queue;
    void*       kernel_cmd_completion_queue;
    uint32_t    kernel_execbo_handle[MAX_EXECBO_POOL_SIZE];
    char*       kernel_execbo_data[MAX_EXECBO_POOL_SIZE];
    bool        kernel_execbo_inuse[MAX_EXECBO_POOL_SIZE];
    uint32_t     reserved[16];
} XmaHwKernel;

typedef struct XmaHwSession
{
    void    *dev_handle;
    uint64_t base_address;
    uint32_t ddr_bank;
    //For execbo:
    uint32_t         dev_index;
    XmaHwKernel      kernel_info;
    uint32_t         reserved[16];
} XmaHwSession;

typedef struct XmaHwDevice
{
    char        dsa[MAX_DSA_NAME];
    XmaHwHandle handle;
    bool        in_use;
    XmaHwKernel kernels[MAX_KERNEL_CONFIGS];
} XmaHwDevice;

typedef struct XmaHwCfg
{
    int32_t     num_devices;
    XmaHwDevice devices[MAX_XILINX_DEVICES];
} XmaHwCfg;

/**
 *  @brief Probe the Hardware and populate the XmaHwCfg
 *
 *  This function probes the hardware present in the system
 *  and populates the corresponding data structures with the
 *  current state of the hardware.
 *
 *  @param hwcfg Pointer to an XmaHwCfg structure that will
 *               hold the results of the hardware probe.
 *               On failure, the contents of this structure
 *               are undefined.
 *
 *  @return          0 on success
 *                  -1 on failure
 */
int xma_hw_probe(XmaHwCfg *hwcfg);

/**
 *  @brief Check compatibility of the system and HW configurations
 *
 *  This function verifies that the system configuration provided
 *  in a text file and parsed by the function @ref xma_cfg_parse()
 *  are compatible such that it is safe to configure the hardware
 *  using the supplied system configuration.
 *
 *  @param hwcfg     Pointer to an XmaHwCfg structure that was
 *                   populated by calling the @ref xma_hw_probe()
 *                   function.
 *  @param systemcfg Pointer to an XmaSystemCfg structure that
 *                   was populated by calling the @ref xma_cfg_parse()
                     function.
 *
 *  @return          TRUE on success
 *                   FALSE on failure
 */
bool xma_hw_is_compatible(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg);

/**
 *  @brief Configure HW using system configuration
 *
 *  This function downloads the hardware as instructed by the
 *  system configuration.  This function should succeed if the
 *  @ref xma_hw_is_compatible() function was called and returned
 *  TRUE.  It is possible for this function to fail if a HW
 *  failure occurs.
 *
 *  @param hwcfg     Pointer to an XmaHwCfg structure that was
 *                   populated by calling the @ref xma_hw_probe()
 *                   function.
 *  @param systemcfg Pointer to an XmaSystemCfg structure that
 *                   was populated by calling the @ref xma_cfg_parse()
 *                    function.
 *
 *  @param hw_cfg_status Has hardware already been configured previously?
 *
 *  @return          TRUE on success
 *                   FALSE on failure
 */
bool xma_hw_configure(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg, bool hw_cfg_status);

/**
 *  @}
 */

#ifdef __cplusplus
}
#endif

#endif
