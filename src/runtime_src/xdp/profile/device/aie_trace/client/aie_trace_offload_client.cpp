/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#define XDP_SOURCE

#include <iostream>

#include "core/common/message.h"
#include "core/include/xrt/xrt_kernel.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info/aie_constructs.h"
#include "xdp/profile/device/aie_trace/aie_trace_logger.h"
#include "xdp/profile/device/aie_trace/client/aie_trace_offload_client.h"
#include "xdp/profile/device/device_intf.h"
#include "core/common/message.h"


#include "xdp/profile/plugin/aie_trace/win/transactions/op_types.h"
#include "xdp/profile/plugin/aie_trace/win/transactions/op_buf.hpp"
#include "xdp/profile/plugin/aie_trace/win/transactions/op_init.hpp"

constexpr std::uint64_t CONFIGURE_OPCODE = std::uint64_t{2};
constexpr uint32_t DATA_SIZE = 65536;

namespace xdp {
using severity_level = xrt_core::message::severity_level;

AIETraceOffload::AIETraceOffload
  ( void* handle, uint64_t id
  , DeviceIntf* dInt
  , AIETraceLogger* logger
  , bool isPlio
  , uint64_t totalSize
  , uint64_t numStrm
  , xrt::hw_context context
  , std::shared_ptr<AieTraceMetadata>(metadata)
  )
  : deviceHandle(handle)
  , deviceId(id)
  , deviceIntf(dInt)
  , traceLogger(logger)
  , isPLIO(isPlio)
  , totalSz(totalSize)
  , numStream(numStrm)
  , traceContinuous(false)
  , offloadIntervalUs(0)
  , bufferInitialized(false)
  , offloadStatus(AIEOffloadThreadStatus::IDLE)
  , mEnCircularBuf(false)
  , mCircularBufOverwrite(false)
  , context(context)
  , metadata(metadata)
{
  bufAllocSz = deviceIntf->getAlignedTraceBufSize(totalSz, static_cast<unsigned int>(numStream));
  mReadTrace = std::bind(&AIETraceOffload::readTraceGMIO, this, std::placeholders::_1);
}

bool
AIETraceOffload::
initReadTrace()
{
  std::cout << "Inside init read trace!" << std::endl;
  buffers.clear();
  buffers.resize(numStream);

  constexpr std::uint64_t DDR_AIE_ADDR_OFFSET = std::uint64_t{0x80000000};
  
  try {
    mKernel = xrt::kernel(context, "XDP_KERNEL");  
  } catch (std::exception &e){
    std::stringstream msg;
    msg << "Unable to find XDP_KERNEL kernel from hardware context. Failed to configure AIE Trace Offloading." << e.what() ;
    xrt_core::message::send(severity_level::warning, "XRT", msg.str());
    return false;
  }

  XAie_Config cfg { 
    metadata->getAIEConfigMetadata("hw_gen").get_value<uint8_t>(),               //hw_gen
    metadata->getAIEConfigMetadata("base_address").get_value<uint64_t>(),        //xaie_base_addr
    metadata->getAIEConfigMetadata("column_shift").get_value<uint8_t>(),         //xaie_col_shift
    metadata->getAIEConfigMetadata("row_shift").get_value<uint8_t>(),            //xaie_row_shift
    metadata->getAIEConfigMetadata("num_rows").get_value<uint8_t>(),             //xaie_num_rows,
    metadata->getAIEConfigMetadata("num_columns").get_value<uint8_t>(),          //xaie_num_cols,
    metadata->getAIEConfigMetadata("shim_row").get_value<uint8_t>(),             //xaie_shim_row,
    metadata->getAIEConfigMetadata("reserved_row_start").get_value<uint8_t>(),   //xaie_res_tile_row_start,
    metadata->getAIEConfigMetadata("reserved_num_rows").get_value<uint8_t>(),    //xaie_res_tile_num_rows,
    metadata->getAIEConfigMetadata("aie_tile_row_start").get_value<uint8_t>(),   //xaie_aie_tile_row_start,
    metadata->getAIEConfigMetadata("aie_tile_num_rows").get_value<uint8_t>(),    //xaie_aie_tile_num_rows
    {0}                                                                          //PartProp
  };

  auto RC = XAie_CfgInitialize(&aieDevInst, &cfg);
  if (RC != XAIE_OK) {
    xrt_core::message::send(severity_level::warning, "XRT", "AIE Driver Initialization Failed.");
    return false;
  }

  for (uint64_t i = 0; i < 1; ++i) {
    std::cout << "Allocating stream in  read trace!" << std::endl;
    inp_bo = xrt::bo(context.get_device(), DATA_SIZE * sizeof(uint32_t), XRT_BO_FLAGS_HOST_ONLY, mKernel.group_id(0));
    std::cout << "INPUT BO ADDRESS: " << inp_bo.address() + DDR_AIE_ADDR_OFFSET << std::endl;
    std::cout << "DATA SIZE: " << DATA_SIZE * sizeof(uint32_t) << std::endl;

    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    // AieRC RC;
    // Todo: get this from aie metadata
    XAie_LocType loc;
    XAie_DmaDesc DmaDesc;
    loc = XAie_TileLoc(4, 0);
    uint8_t s2mm_ch_id = 1;
    uint8_t s2mm_bd_id = 15;

    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);
 
    // S2MM BD
    RC = XAie_DmaDescInit(&aieDevInst, &DmaDesc, loc);
    RC = XAie_DmaSetAddrLen(&DmaDesc, inp_bo.address() + DDR_AIE_ADDR_OFFSET, DATA_SIZE * sizeof(uint32_t));
    RC = XAie_DmaEnableBd(&DmaDesc);
    RC = XAie_DmaSetAxi(&DmaDesc, 0U, 8U, 0U, 0U, 0U);
    RC = XAie_DmaWriteBd(&aieDevInst, &DmaDesc, loc, s2mm_bd_id);

    //printf("Enabling channels....\n");
    RC = XAie_DmaChannelPushBdToQueue(&aieDevInst, loc, s2mm_ch_id, DMA_S2MM, s2mm_bd_id);
    RC = XAie_DmaChannelEnable(&aieDevInst, loc, s2mm_ch_id, DMA_S2MM);

    uint8_t *txn_ptr = XAie_ExportSerializedTransaction(&aieDevInst, 1, 0);
    op_buf instr_buf;
    instr_buf.addOP(transaction_op(txn_ptr));
    xrt::bo instr_bo;

    // Configuration bo
    try {
      instr_bo = xrt::bo(context.get_device(), instr_buf.ibuf_.size(), XCL_BO_FLAGS_CACHEABLE, mKernel.group_id(1));
    } catch (std::exception &e){
      std::stringstream msg;
      msg << "Unable to create instruction buffer for AIE Trace transaction. Not configuring AIE Trace Offloading. " << e.what() << std::endl;
      xrt_core::message::send(severity_level::warning, "XRT", msg.str());
      return false;
    }

    instr_bo.write(instr_buf.ibuf_.data());
    instr_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    auto run = mKernel(CONFIGURE_OPCODE, instr_bo, instr_bo.size()/sizeof(int), 0, 0, 0, 0);
    run.wait2();

    xrt_core::message::send(severity_level::info, "XRT", "Successfully scheduled AIE Trace Offloading Transaction Buffer.");

    // Must clear aie state
    XAie_ClearTransaction(&aieDevInst);
    
  }
  bufferInitialized = true;
  return bufferInitialized;
}

void
AIETraceOffload::
readTraceGMIO(bool /*final*/)
{
  syncAndLog(0);
}

uint64_t
AIETraceOffload::
syncAndLog(uint64_t index)
{
  inp_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
  auto in_bo_map = inp_bo.map<uint32_t*>();
  if (!in_bo_map)
    return 0;

  // Log nBytes of trace
  traceLogger->addAIETraceData(index, (void*)in_bo_map, DATA_SIZE * sizeof(uint32_t), true);
  return inp_bo.size();
}

/*
 * Implement these later for continuous offload
 */

AIETraceOffload::
~AIETraceOffload()
{}

void
AIETraceOffload::
startOffload()
{}

bool
AIETraceOffload::
keepOffloading()
{
  return false;
}

void
AIETraceOffload::
stopOffload()
{}

void
AIETraceOffload::
offloadFinished()
{}

void
AIETraceOffload::
endReadTrace()
{}

uint64_t
AIETraceOffload::
searchWrittenBytes(void* /*buf*/, uint64_t /*bytes*/)
{
  return 0;
}

}