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
#include "app/xmabuffers.h"
#include "plg/xmasess.h"
#include "xrt.h"
#include <atomic>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstring>
#include <array>
#include <random>
#include <chrono>
#include <list>
#include <condition_variable>

#define MAX_EXECBO_BUFF_SIZE      4096// 4KB
#define MAX_KERNEL_REGMAP_SIZE    4032//Some space used by ert pkt
#define MAX_REGMAP_ENTRIES        1024//Int32 entries; So 4B x 1024 = 4K Bytes

//const uint64_t mNullBO = 0xffffffff;

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

/* Forward declaration */
typedef struct XmaHwDevice XmaHwDevice;

typedef struct XmaCUCmdObjPrivate
{
    //uint32_t cmd_id1;//Serial roll-over counter;
    //cmd1 is key of the map
    int32_t cmd_id2;//Random number
    int32_t   cu_id;
    int32_t   execbo_id;
    bool      cmd_finished;

  XmaCUCmdObjPrivate() {
    cmd_id2 = 0;
    cu_id = -1;
    execbo_id = -1;
    cmd_finished = false;
  }
} XmaCUCmdObjPrivate;

typedef struct XmaHwExecBO
{
    xclBufferHandle    handle;
    char*       data;//execBO size is 4096 in xmahw_hal.cpp
    bool        in_use;
    int32_t     cu_index;
    int32_t     session_id;
    uint32_t    cu_cmd_id1;//Counter
    int32_t     cu_cmd_id2;//Random num

  XmaHwExecBO() {
    in_use = false;
    handle = NULLBO;
    data = NULL;
    cu_index = -1;
    cu_cmd_id1 = 0;
    cu_cmd_id2 = 0;
    session_id = -1;
  }
} XmaHwExecBO;

typedef struct XmaBufferPool
{
    std::list<XmaBufferObj>   buffers_busy;
    std::vector<XmaBufferObj>  buffers_free;
    std::atomic<bool> pool_locked;
    uint64_t buffer_size;
    int32_t  bank_index;
    int32_t  dev_index;
    bool     device_only_buffer;
    std::atomic<uint32_t> num_buffers;
    std::atomic<uint32_t> num_free_buffers;
    uint32_t reserved[4];

  XmaBufferPool() {
   pool_locked = false;
   num_buffers = 0;
   num_free_buffers = 0;
   buffer_size = 0;
   bank_index = -1;
   dev_index = -1;
   device_only_buffer = false;
  }
} XmaBufferPool;

typedef struct XmaBufferPoolObjPrivate
{
    void*    dummy;
    uint64_t buffer_size;
    int32_t  bank_index;
    int32_t  dev_index;
    XmaBufferPool* pool_ptr;
    bool     device_only_buffer;
    uint32_t reserved[4];

  XmaBufferPoolObjPrivate() {
   dummy = NULL;
   buffer_size = 0;
   bank_index = -1;
   dev_index = -1;
   pool_ptr = nullptr;
   device_only_buffer = false;
  }
} XmaBufferPoolObjPrivate;

typedef struct XmaHwSessionPrivate
{
    void            *dev_handle;
    XmaHwKernel     *kernel_info;
    //For execbo:
    std::atomic<uint32_t>  kernel_complete_count;
    std::atomic<uint32_t>  kernel_complete_total;
    XmaHwDevice     *device;
    std::unordered_map<uint32_t, XmaCUCmdObjPrivate> CU_cmds;//Use execbo lock when accessing this map
    std::atomic<uint32_t> num_cu_cmds;
    std::atomic<uint32_t> num_cu_cmds_avg;
    std::atomic<uint32_t> num_cu_cmds_avg_tmp;
    std::atomic<uint32_t> num_samples;
    std::atomic<uint32_t> cmd_busy;
    std::atomic<uint32_t> cmd_idle;
    std::atomic<uint32_t> cmd_busy_ticks;
    std::atomic<uint32_t> cmd_idle_ticks;
    std::atomic<uint32_t> cmd_busy_ticks_tmp;
    std::atomic<uint32_t> cmd_idle_ticks_tmp;
    std::atomic<bool> slowest_element;
    std::mutex m_mutex;
    std::condition_variable work_item_done_1plus;//Use with xma_plg_work_item_done
    std::condition_variable execbo_is_free; //Use with xma_plg_schedule_work_item and xma_plg_schedule_cu_cmd
    std::condition_variable kernel_done_or_free;//Use with xma_plg_cu_cmd_status; CU completion is must every outstanding cmd;
    xclBufferHandle  last_execbo_handle;

    std::vector<uint32_t> execbo_lru;
    std::vector<uint32_t> execbo_to_check;
    bool     using_work_item_done;
    bool     using_cu_cmd_status;
    std::atomic<bool> execbo_locked;
    std::vector<XmaHwExecBO> kernel_execbos;
    int32_t    num_execbo_allocated;
    std::list<XmaBufferPool>   buffer_pools;

    uint32_t reserved[4];

  XmaHwSessionPrivate() {
    dev_handle = NULL;
    kernel_info = NULL;
    kernel_complete_count = 0;
    kernel_complete_total = 0;
    device = NULL;
    num_cu_cmds = 0;
    num_cu_cmds_avg = 0;
    num_cu_cmds_avg_tmp = 0;
    num_samples = 0;
    cmd_busy = 0;
    cmd_idle = 0;
    cmd_busy_ticks = 0;
    cmd_idle_ticks = 0;
    cmd_busy_ticks_tmp = 0;
    cmd_idle_ticks_tmp = 0;
    execbo_locked = false;
    num_execbo_allocated = -1;
    using_work_item_done = false;
    using_cu_cmd_status = false;
    slowest_element = false;
    last_execbo_handle = NULLBO;
  }
} XmaHwSessionPrivate;

typedef struct XmaBufferObjPrivate
{
    void*    dummy;
    uint64_t size;
    uint64_t paddr;
    int32_t  bank_index;
    int32_t  dev_index;
    xclBufferHandle boHandle;
    std::atomic<int32_t> ref_cnt;
    bool     device_only_buffer;
    xclDeviceHandle dev_handle;
    uint32_t reserved[4];

  XmaBufferObjPrivate() {
   dummy = NULL;
   size = 0;
   bank_index = -1;
   dev_index = -1;
   ref_cnt = 0;
   dev_handle = NULL;
   device_only_buffer = false;
   boHandle = 0;
  }
} XmaBufferObjPrivate;

typedef struct XmaHwKernel
{
    uint8_t     name[MAX_KERNEL_NAME];
    bool        context_opened;
    bool        in_use;
    int32_t     cu_index;
    uint64_t    base_address;
    //uint64_t bitmap based on MAX_DDR_MAP=64
    uint64_t    ip_ddr_mapping;
    int32_t     default_ddr_bank;
    std::unordered_map<int32_t, int32_t> CU_arg_to_mem_info;// arg# -> ddr_bank#

    int32_t     cu_index_ert;
    uint32_t    cu_mask0;
    uint32_t    cu_mask1;
    uint32_t    cu_mask2;
    uint32_t    cu_mask3;

    bool soft_kernel;
    bool kernel_channels;
    uint32_t     max_channel_id;
    int32_t      arg_start;
    int32_t      regmap_size;
    bool         is_shared;

    //No need of atomic as only one thread is using below variables
    uint32_t num_sessions;
    uint32_t num_cu_cmds_avg;
    uint32_t num_cu_cmds_avg_tmp;
    uint32_t num_samples;
    uint32_t cu_busy;
    uint32_t cu_idle;
    uint32_t cu_busy_tmp;
    uint32_t num_samples_tmp;

    uint32_t    reserved[16];

  XmaHwKernel() {
   std::memset(name, 0, sizeof(name));
    in_use = false;
    context_opened = false;
    cu_index = -1;
    default_ddr_bank = -1;
    ip_ddr_mapping = 0;
    cu_index_ert = -1;
    cu_mask0 = 0;
    cu_mask1 = 0;
    cu_mask2 = 0;
    cu_mask3 = 0;
    soft_kernel = false;
    kernel_channels = false;
    max_channel_id = 0;
    arg_start = -1;
    regmap_size = -1;
    is_shared = false;
    num_sessions = 0;
    num_cu_cmds_avg = 0;
    num_cu_cmds_avg_tmp = 0;
    num_samples = 0;
    cu_busy = 0;
    cu_idle = 0;
    cu_busy_tmp = 0;
    num_samples_tmp = 0;
  }
} XmaHwKernel;

typedef struct XmaHwMem
{
    bool        in_use;
    uint64_t    base_address;
    uint64_t    size_kb;
    uint32_t    size_mb;
    uint32_t    size_gb;
    uint8_t     name[MAX_KERNEL_NAME];

    uint32_t    reserved[16];

  XmaHwMem() {
    std::memset(name, 0, sizeof(name));
    in_use = false;
    base_address = 0;
    size_kb = 0;
    size_mb = 0;
    size_gb = 0;
  }
} XmaHwMem;

typedef struct XmaHwDevice
{
    //char        dsa[MAX_DSA_NAME];
    xclDeviceHandle    handle;
    xclDeviceInfo2     info;
    uint32_t           dev_index;
    uuid_t             uuid; 
    uint32_t           number_of_cus;
    uint32_t           number_of_hardware_kernels;
    uint32_t           number_of_mem_banks;
    std::vector<XmaHwKernel> kernels;
    std::vector<XmaHwMem> ddrs;

    uint32_t    cu_cmd_id1;//Counter
    uint32_t    cu_cmd_id2;//Counter
    std::mt19937 mt_gen;
    std::uniform_int_distribution<int32_t> rnd_dis;

    uint32_t    reserved[16];

  XmaHwDevice(): rnd_dis(-97986387, 97986387) {
    dev_index = -1;
    number_of_cus = 0;
    number_of_hardware_kernels = 0;
    number_of_mem_banks = 0;
    handle = NULL;
    cu_cmd_id1 = 0;
    cu_cmd_id2 = 0;
    std::random_device rd;
    uint32_t tmp_int = time(0);
    std::seed_seq seed_seq{rd(), tmp_int};
    mt_gen = std::mt19937(seed_seq);
  }
} XmaHwDevice;

typedef struct XmaHwCfg
{
    int32_t     num_devices;
    std::vector<XmaHwDevice> devices;

    uint32_t    reserved[16];

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
