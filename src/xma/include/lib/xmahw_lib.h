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
#ifndef _XMA_HW_LIB_H_
#define _XMA_HW_LIB_H_

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
//#include "lib/xmacfg.h"
#include "lib/xmalimits_lib.h"
#include "app/xmahw.h"
#include "app/xmaparam.h"
#include "plg/xmasess.h"
#include "xrt.h"
#include <atomic>
#include <vector>
#include <memory>
#include <map>

#define MIN_EXECBO_POOL_SIZE      16
#define MAX_EXECBO_BUFF_SIZE      4096// 4KB
#define MAX_KERNEL_REGMAP_SIZE    4032//Some space used by ert pkt
#define MAX_REGMAP_ENTRIES        1024//Int32 entries; So 4B x 1024 = 4K Bytes

//#ifdef __cplusplus
//extern "C" {
//#endif

/**
 *  @file
 */

/**
 * @addtogroup xmahw
 * @{
 */
constexpr std::uint64_t signature = 0xF42F1F8F4F2F1F0F;

typedef struct XmaBufferObjPrivate
{
    void*    dummy;
    uint64_t size;
    uint64_t paddr;
    int32_t  bank_index;
    int32_t  dev_index;
    uint64_t boHandle;
    bool     device_only_buffer;
    xclDeviceHandle dev_handle;
    uint32_t reserved[4];

  XmaBufferObjPrivate() {
   dummy = NULL;
   size = 0;
   bank_index = -1;
   dev_index = -1;
   dev_handle = NULL;
   device_only_buffer = false;
   boHandle = 0;
  }
} XmaBufferObjPrivate;

typedef struct XmaHwKernel
{
    uint8_t     name[MAX_KERNEL_NAME];
    bool        in_use;
    int32_t     cu_index;
    uint64_t    base_address;
    //uint64_t bitmap based on MAX_DDR_MAP=64
    uint64_t    ip_ddr_mapping;
    int32_t     default_ddr_bank;
    std::map<int32_t, int32_t> CU_arg_to_mem_info;// arg# -> ddr_bank#

    uint32_t    cu_mask0;
    uint32_t    cu_mask1;
    uint32_t    cu_mask2;
    uint32_t    cu_mask3;
    int32_t    regmap_max;
    //For execbo:
    int32_t     kernel_complete_count;
    //std::unique_ptr<std::atomic<bool>> kernel_complete_locked;

    uint32_t    reg_map[MAX_REGMAP_ENTRIES];//4KB = 4B x 1024; Supported Max regmap of 4032 Bytes only in xmaplugin.cpp; execBO size is 4096 = 4KB in xmahw_hal.cpp
    //pthread_mutex_t *lock;
    std::unique_ptr<std::atomic<bool>> reg_map_locked;
    int32_t         locked_by_session_id;
    XmaSessionType locked_by_session_type;
    bool soft_kernel;
    bool kernel_channels;
    uint32_t     max_channel_id;
    void*   private_do_not_use;

    //bool             have_lock;
    uint32_t    reserved[16];

  XmaHwKernel(): reg_map_locked(new std::atomic<bool>) {
    in_use = false;
    cu_index = -1;
    regmap_max = -1;
    default_ddr_bank = -1;
    ip_ddr_mapping = 0;
    cu_mask0 = 0;
    cu_mask1 = 0;
    cu_mask2 = 0;
    cu_mask3 = 0;
    kernel_complete_count = 0;
    soft_kernel = false;
    kernel_channels = false;
    max_channel_id = 0;
    //*kernel_complete_locked = false;
    *reg_map_locked = false;
    locked_by_session_id = -100;
    private_do_not_use = NULL;
  }
} XmaHwKernel;

typedef struct XmaHwDevice
{
    //char        dsa[MAX_DSA_NAME];
    xclDeviceHandle    handle;
    xclDeviceInfo2     info;
    //For execbo:
    uint32_t           dev_index;
    uuid_t             uuid; 
    uint32_t           number_of_cus;
    uint32_t           number_of_mem_banks;
    //bool        in_use;
    //XmaHwKernel kernels[MAX_KERNEL_CONFIGS];
    std::vector<XmaHwKernel> kernels;

    std::unique_ptr<std::atomic<bool>> execbo_locked;
    std::vector<uint32_t> kernel_execbo_handle;
    std::vector<char*> kernel_execbo_data;//execBO size is 4096 in xmahw_hal.cpp
    std::vector<bool> kernel_execbo_inuse;
    std::vector<int32_t> kernel_execbo_cu_index;
    int32_t    num_execbo_allocated;

  XmaHwDevice(): execbo_locked(new std::atomic<bool>) {
    //in_use = false;
    dev_index = -1;
    number_of_cus = 0;
    *execbo_locked = false;
    number_of_mem_banks = 0;
    num_execbo_allocated = -1;
    handle = NULL;
  }
} XmaHwDevice;

typedef struct XmaHwCfg
{
    int32_t     num_devices;
    //XmaHwDevice devices[MAX_XILINX_DEVICES];
    std::vector<XmaHwDevice> devices;

  XmaHwCfg() {
    num_devices = -1;
  }
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
bool xma_hw_is_compatible(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg);
 */
bool xma_hw_is_compatible(XmaHwCfg *hwcfg, XmaXclbinParameter *devXclbins, int32_t num_parms);

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
bool xma_hw_configure(XmaHwCfg *hwcfg, XmaSystemCfg *systemcfg, bool hw_cfg_status);
 */
bool xma_hw_configure(XmaHwCfg *hwcfg, XmaXclbinParameter *devXclbins, int32_t num_parms);

/**
 *  @}
 */

//#ifdef __cplusplus
//}
//#endif

#endif
