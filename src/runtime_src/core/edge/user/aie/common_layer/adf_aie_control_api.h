/**
* Copyright (C) 2022 Xilinx, Inc
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

#include "adf_api_message.h"
#include <memory>
extern "C"
{
#include "xaiengine/xaiegbl.h"
}

namespace adf
{
class config_manager
{
public:
  config_manager(XAie_DevInst* dev_inst, size_t num_reserved_rows, bool broadcast_enable_core);
  XAie_DevInst*
  get_dev()
  {
    return m_aie_dev;
  }

  size_t
  get_num_reserved_rows()
  {
    return m_num_reserved_rows;
  }

  bool
  get_broadcast_enable_core()
  {
    return m_broadcast_enable_core;
  }

private:
  XAie_DevInst* m_aie_dev;
  size_t m_num_reserved_rows;
  bool m_broadcast_enable_core;
};

class dma_api
{
public:
 /// AIE2 DMA Buffer descriptor.
 /// Data types in this class are considered to match AIE driver.
  struct buffer_descriptor
  {
    /// Address in bytes
    uint64_t address = 0;
    /// Length in bytes
    uint32_t length = 0;
    /// D0, D1, D2, D3(memory tile only) stepsize in 32-bit word
    std::vector<uint32_t> stepsize;
    /// D0, D1, D2(memory tile only) wrap in 32-bit word
    std::vector<uint32_t> wrap;
    /// D0, D1, D2 zero-before and zero-after in 32-bit word
    std::vector<std::pair<uint32_t, uint32_t>> padding;
    /// Enable adding packet header at start of transfer. MM2S only.
    bool enable_packet = false;
    /// Packet ID. MM2S only.
    uint8_t packet_id = 0;
    /// Out of order BD ID
    uint8_t out_of_order_bd_id = 0;
    /// TLAST suppress. Memory tile only. MM2S only.
    bool tlast_suppress = false;
    /// Iteration stepsize in 32-bit word
    uint32_t iteration_stepsize = 0;
    /// Iteration wrap
    uint16_t iteration_wrap = 0;
    /// Iteration current
    uint8_t iteration_current = 0;
    /// Enable compression for MM2S or enable decompression for S2MM. AIE tile and memory tile only.
    bool enable_compression = false;
    /// Enable lock acquire
    bool lock_acq_enable = false;
    /// Lock acquire value (signed). acq_ge if less than 0. acq_eq if larger than or equal to 0.
    int8_t lock_acq_value = 0;
    /// Lock id to acquire
    uint16_t lock_acq_id = 0;
    /// Lock release value (signed). 0: do not release a lock.
    int8_t lock_rel_value = 0;
    /// Lock id to release
    uint16_t lock_rel_id = 0;
    /// Continue with next BD
    bool use_next_bd = false;
    /// Next BD ID
    uint16_t next_bd = 0;
    /// AXI burst length. Shim tile only. In binary format 00: BLEN = 4 (64B), 01: BLEN = 8 (128B), 10: BLEN = 16 (256B), 11: Undefined
    uint8_t burst_length = 4;
  };

  /// Configure BD, wait task queue space, then enqueue task.
  /// @param tileType 0 (adf::tile_type::aie_tile), 1 (adf::tile_type::shim_tile), 2 (adf::tile_type::memory_tile)
  /// @param column AIE array column
  /// @param row AIE array row relative to tileType
  /// @param dir 0 (XAie_DmaDirection::DMA_S2MM), 1 (XAie_DmaDirection::DMA_MM2S)
  err_code configureBdWaitQueueEnqueueTask(int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel, uint32_t repeatCount, bool enableTaskCompleteToken, std::vector<uint16_t> bdIds, std::vector<dma_api::buffer_descriptor> bdParams);

  err_code configureBD(int tileType, uint8_t column, uint8_t row, uint16_t bdId, const dma_api::buffer_descriptor& bdParam);
  err_code enqueueTask(int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel, uint32_t repeatCount, bool enableTaskCompleteToken, uint16_t startBdId);
  err_code waitDMAChannelTaskQueue(int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel);
  err_code waitDMAChannelDone(int tileType, uint8_t column, uint8_t row, int dir, uint8_t channel);
  err_code updateBDAddress(int tileType, uint8_t column, uint8_t row, uint16_t bdId, uint64_t address);
  err_code updateBDAddressLin(XAie_MemInst* memInst , uint8_t column, uint8_t row, uint16_t bdId, uint64_t offset);

  std::shared_ptr<config_manager> get_config()
  {
    return config;
  }

  dma_api(std::shared_ptr<config_manager> cfg): config(std::move(cfg))
  {
  }
private:
  std::shared_ptr<config_manager> config;
};

class lock_api
{
public:
  /// @param tileType 0 (adf::tile_type::aie_tile), 1 (adf::tile_type::shim_tile), 2 (adf::tile_type::memory_tile)
  /// @param column AIE array column
  /// @param row AIE array row relative to tileType
  err_code initializeLock(int tileType, uint8_t column, uint8_t row, uint16_t lockId, int8_t initVal);
  err_code acquireLock(int tileType, uint8_t column, uint8_t row, uint16_t lockId, int8_t acqVal);
  err_code releaseLock(int tileType, uint8_t column, uint8_t row, uint16_t lockId, int8_t relVal);
  std::shared_ptr<config_manager> get_config()
  {
    return config;
  }

  lock_api(std::shared_ptr<config_manager> cfg): config(std::move(cfg))
  {
  }

private:
  std::shared_ptr<config_manager> config;

};
}
