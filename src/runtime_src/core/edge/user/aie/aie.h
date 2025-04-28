/**
 * Copyright (C) 2020-2021 Xilinx, Inc
 * Author(s): Larry Liu
 * ZNYQ XRT Library layered on top of ZYNQ zocl kernel driver
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

#ifndef xrt_core_edge_user_aie_h
#define xrt_core_edge_user_aie_h

#include <memory>
#include <queue>
#include <vector>

#include "core/common/device.h"
#include "core/edge/common/aie_parser.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_aie.h"
#include "common_layer/adf_api_config.h"
#include "common_layer/adf_runtime_api.h"
#include "common_layer/adf_profiling_api.h"
extern "C" {
#include <xaiengine.h>
}

//#define XAIEGBL_NOC_DMASTA_STARTQ_MAX 4
#define XAIEDMA_SHIM_MAX_NUM_CHANNELS 4
#define XAIEDMA_SHIM_TXFER_LEN32_MASK 3

#define CONVERT_LCHANL_TO_PCHANL(l_ch) (l_ch > 1 ? l_ch - 2 : l_ch)

enum xrtProfilingOption {
  IO_TOTAL_STREAM_RUNNING_TO_IDLE_CYCLE = 0,
  IO_STREAM_START_TO_BYTES_TRANSFERRED_CYCLES,
  IO_STREAM_START_DIFFERENCE_CYCLES,
  IO_STREAM_RUNNING_EVENT_COUNT
};

namespace zynqaie {

struct aie_bd {
  uint16_t bd_num;
  int      buf_fd;
#ifdef __AIESIM__
  char     *vaddr;
  size_t   size;
#else
  XAie_MemInst mem_inst;
#endif
};

struct dma_channel {
  std::queue<aie_bd> idle_bds;
  std::queue<aie_bd> pend_bds;
};

struct shim_dma {
  XAie_DmaDesc desc;
  dma_channel dma_chan[XAIEDMA_SHIM_MAX_NUM_CHANNELS];
  bool configured;
  uint8_t maxq_size;
};

struct event_record {
  int option;
  std::vector<std::shared_ptr<xaiefal::XAieRsc>> acquired_resources;
};

class aie_array {
public:
  ~aie_array();
  aie_array(const std::shared_ptr<xrt_core::device>& device);

  aie_array(const std::shared_ptr<xrt_core::device>& device, const zynqaie::hwctx_object* hwctx_obj);

  std::vector<shim_dma> shim_dmas;   // shim DMA // not used anymore, should be cleanedup

  /* This is the collections of gmios that are used. */
  std::unordered_map<std::string, adf::gmio_config> gmio_configs;
  std::unordered_map<std::string, std::shared_ptr<adf::gmio_api>> gmio_apis;

  std::unordered_map<std::string, adf::plio_config> plio_configs;

  std::unordered_map<std::string, adf::external_buffer_config> external_buffer_configs;

  XAie_DevInst *get_dev();

  void
  open_context(const xrt_core::device* device, xrt::aie::access_mode am);

  void
  open_context(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx_obj, xrt::aie::access_mode am);

  bool
  is_context_set();

  void
  sync_external_buffer(std::vector<xrt::bo>& bo, adf::external_buffer_config& ebuf_config, enum xclBOSyncDirection dir, size_t size, size_t offset);

  bool
  status_external_buffer(adf::external_buffer_config& ebuf_config);

  void
  wait_external_buffer(adf::external_buffer_config& ebuf_config);

  void
  sync_bo(std::vector<xrt::bo>& bos, const char *dmaID, enum xclBOSyncDirection dir, size_t size, size_t offset);

  std::pair<size_t, size_t>
  sync_bo_nb(std::vector<xrt::bo>& bos, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset);

  bool
  async_status(const std::string& gmioName, uint16_t bdNum, uint32_t bdInstance);

  void
  wait_gmio(const std::string& gmioName);

  void
  reset(const xrt_core::device* device, uint32_t hw_context_id, uint32_t partition_id);

  int
  start_profiling(int option, const std::string& port1_name, const std::string& port2_name, uint32_t value);

  uint64_t
  read_profiling(int phdl);

  void
  stop_profiling(int phdl);

  void
  prepare_bd(aie_bd& bd, xrt::bo& bo);

  void
  clear_bd(aie_bd& bd);

  bool
  find_gmio(const std::string & port_name);

  bool
  find_external_buffer(const std::string & port_name);

  std::shared_ptr<adf::config_manager>
  get_config()
  {
    return m_config;
  }

private:
  int num_cols;
  int fd;
  xrt::aie::access_mode access_mode = xrt::aie::access_mode::none;

  XAie_DevInst* dev_inst;         // AIE Device Instance pointer

    // XAie_InstDeclare(DevInst, &ConfigPtr) is the interface
    // to initialize DevInst by the AIE driver. But it does not
    // work here because we can not make it as a member of Aie
    // class to maintain its life cylce. So we declair it here.
    //
    // Note: need to evolve when XAie_InstDecalare() evolves.
  XAie_DevInst dev_inst_obj;

  std::vector<event_record> event_records;
  std::shared_ptr<adf::config_manager> m_config;

  std::pair<size_t, size_t>
  submit_sync_bo(xrt::bo& bo, std::shared_ptr<adf::gmio_api>& gmio, adf::gmio_config& gmio_config, enum xclBOSyncDirection dir, size_t size, size_t offset);

  adf::shim_config
  get_shim_config(const std::string& port_name);

  int
  start_profiling_run_idle(const std::string& port_name);

  int
  start_profiling_start_bytes(const std::string& port_name, uint32_t value);

  int
  start_profiling_diff_cycles(const std::string& port1_name, const std::string& port2_name);

  int
  start_profiling_event_count(const std::string& port_name);
};

}

#endif
