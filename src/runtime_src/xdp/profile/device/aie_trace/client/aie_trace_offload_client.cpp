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

AIETraceOffload::~AIETraceOffload()
{
  stopOffload();
  if (offloadThread.joinable())
    offloadThread.join();
}


bool AIETraceOffload::initReadTrace()
{
  std::cout << "Inside init read trace!" << std::endl;
  buffers.clear();
  buffers.resize(numStream);

  // uint8_t  memIndex = 0;
//   if (isPLIO) {
//     memIndex = deviceIntf->getAIETs2mmMemIndex(0); // all the AIE Ts2mm s will have same memory index selected
//   } else {
//     memIndex = 0;  // for now

// /*
//  * XRT_X86_BUILD is set only for x86 builds
//  * Only compile this on edge+versal build
//  */
// #if defined (XRT_ENABLE_AIE) && ! defined (XRT_X86_BUILD)
//   gmioDMAInsts.clear();
//   gmioDMAInsts.resize(numStream);
// #endif
//   }

  checkCircularBufferSupport();
  constexpr uint32_t DATA_SIZE = 65536;
  constexpr std::uint64_t DDR_AIE_ADDR_OFFSET = std::uint64_t{0x80000000};
  
  try {
    mKernel = xrt::kernel(context, "XDP_KERNEL");  
  } catch (std::exception &e){
    std::stringstream msg;
    msg << "Unable to find XDP_KERNEL kernel from hardware context. Failed to configure AIE Trace Offloading." << e.what() ;
    xrt_core::message::send(severity_level::warning, "XRT", msg.str());
    return false;
  }


  for(uint64_t i = 0; i < 1; ++i) {
    std::cout << "Allocating stream in  read trace!" << std::endl;
    inp_bo = xrt::bo(context.get_device(), DATA_SIZE * sizeof(uint32_t), XRT_BO_FLAGS_HOST_ONLY, mKernel.group_id(0));

    // buffers[i].bufId = deviceIntf->allocTraceBuf(bufAllocSz, memIndex);
    // if (!buffers[i].bufId) {
    //   bufferInitialized = false;
    //   return bufferInitialized;
    // }

    // // Data Mover will write input stream to this address
    // uint64_t bufAddr = deviceIntf->getTraceBufDeviceAddr(buffers[i].bufId);

    // std::string tracemsg = "Allocating trace buffer of size " + std::to_string(bufAllocSz) + " for AIE Stream " + std::to_string(i);
    // xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT", tracemsg.c_str());

    // std::cout << "Buffer Address: " << bufAddr << std::endl;

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

    //Start recording the transaction
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    // AieRC RC;
    XAie_LocType loc;
    XAie_DmaDesc DmaDesc2;

    loc = XAie_TileLoc(1, 0);
    uint8_t s2mm_ch_id = 1;
    uint8_t s2mm_bd_id = 4 * s2mm_ch_id;

    // uint8_t mm2s_bd_id = 2;
    // uint8_t mm2s_ch_id = 0;

    printf("Configuring AIE...\n");
    XAie_StartTransaction(&aieDevInst, XAIE_TRANSACTION_DISABLE_AUTO_FLUSH);

    // S2MM BD
    RC = XAie_DmaDescInit(&aieDevInst, &DmaDesc2, loc);
    RC = XAie_DmaSetAddrLen(&DmaDesc2, inp_bo.address() + DDR_AIE_ADDR_OFFSET, DATA_SIZE * sizeof(uint32_t));
    RC = XAie_DmaEnableBd(&DmaDesc2);
    RC = XAie_DmaSetAxi(&DmaDesc2, 0U, 4U, 0U, 0U, 0U);
    RC = XAie_DmaWriteBd(&aieDevInst, &DmaDesc2, loc, s2mm_bd_id);

    printf("Enabling channels....\n");
    RC = XAie_DmaChannelPushBdToQueue(&aieDevInst, loc, s2mm_ch_id, DMA_S2MM, s2mm_bd_id);
    // RC = XAie_DmaChannelPushBdToQueue(&aieDevInst, loc, mm2s_ch_id, DMA_MM2S, mm2s_bd_id);

    RC = XAie_DmaChannelEnable(&aieDevInst, loc, s2mm_ch_id, DMA_S2MM);
    // RC = XAie_DmaChannelEnable(&aieDevInst, loc, mm2s_ch_id, DMA_MM2S);


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

    // VPDatabase* db = VPDatabase::Instance();
    // TraceGMIO*  traceGMIO = (db->getStaticInfo()).getTraceGMIO(deviceId, i);

    // ZYNQ::shim *drv = ZYNQ::shim::handleCheck(deviceHandle);
    // if(!drv) {
    //   bufferInitialized = false;
    //   return bufferInitialized;
    // }
    // zynqaie::Aie* aieObj = drv->getAieArray();

    // XAie_DevInst* devInst = aieObj->getDevInst();

    // gmioDMAInsts[i].gmioTileLoc = XAie_TileLoc(traceGMIO->shimColumn, 0);

    // int driverStatus = XAIE_OK;
    // driverStatus = XAie_DmaDescInit(devInst, &(gmioDMAInsts[i].shimDmaInst), gmioDMAInsts[i].gmioTileLoc);
    // if(XAIE_OK != driverStatus) {
    //   throw std::runtime_error("Initialization of DMA Descriptor failed while setting up SHIM DMA channel for GMIO Trace offload");
    // }

    // // channelNumber: (0-S2MM0,1-S2MM1,2-MM2S0,3-MM2S1)
    // // Enable shim DMA channel, need to start first so the status is correct
    // uint16_t channelNumber = (traceGMIO->channelNumber > 1) ? (traceGMIO->channelNumber - 2) : traceGMIO->channelNumber;
    // XAie_DmaDirection dir = (traceGMIO->channelNumber > 1) ? DMA_MM2S : DMA_S2MM;

    // XAie_DmaChannelEnable(devInst, gmioDMAInsts[i].gmioTileLoc, channelNumber, dir);

    // // Set AXI burst length
    // XAie_DmaSetAxi(&(gmioDMAInsts[i].shimDmaInst), 0, traceGMIO->burstLength, 0, 0, 0);

    // XAie_MemInst memInst;
    // XAie_MemCacheProp prop = XAIE_MEM_CACHEABLE;
    // xclBufferExportHandle boExportHandle = deviceIntf->exportTraceBuf(buffers[i].bufId);
    // if(XRT_NULL_BO_EXPORT == boExportHandle) {
    //   throw std::runtime_error("Unable to export BO while attaching to AIE Driver");
    // }
    // XAie_MemAttach(devInst,  &memInst, 0, 0, 0, prop, boExportHandle);

    // char* vaddr = reinterpret_cast<char *>(mmap(NULL, bufAllocSz, PROT_READ | PROT_WRITE, MAP_SHARED, boExportHandle, 0));
    // XAie_DmaSetAddrLen(&(gmioDMAInsts[i].shimDmaInst), (uint64_t)vaddr, bufAllocSz);

    // XAie_DmaEnableBd(&(gmioDMAInsts[i].shimDmaInst));

    // // For trace, use bd# 0 for S2MM0, use bd# 4 for S2MM1
    // int bdNum = channelNumber * 4;
    // // Write to shim DMA BD AxiMM registers
    // XAie_DmaWriteBd(devInst, &(gmioDMAInsts[i].shimDmaInst), gmioDMAInsts[i].gmioTileLoc, bdNum);

    // // Enqueue BD
    // XAie_DmaChannelPushBdToQueue(devInst, gmioDMAInsts[i].gmioTileLoc, channelNumber, dir, bdNum);

    
  }
  bufferInitialized = true;
  return bufferInitialized;
}

void AIETraceOffload::endReadTrace()
{
  // // reset
  // for (uint64_t i = 0; i < numStream ; ++i) {
  // if (!buffers[i].bufId)
  //   continue;

  // deviceIntf->freeTraceBuf(buffers[i].bufId);
  // buffers[i].bufId = 0;
  // }
  // bufferInitialized = false;
}

void AIETraceOffload::readTraceGMIO(bool final)
{
  std::cout << "Inside read Trace GMIO" << std::endl;

  // // Keep it low to save bandwidth
  // constexpr uint64_t chunk_512k = 0x80000;

  // for (uint64_t index = 0; index < 1; ++index) {
  //   auto& bd = buffers[index];
  //   if (bd.offloadDone)
  //     continue;

  //   // read one chunk or till the end of buffer
  //   auto chunkEnd = bd.offset + chunk_512k;
  //   if (final || chunkEnd > bufAllocSz)
  //     chunkEnd = bufAllocSz;
  //   bd.usedSz = chunkEnd;

  //   bd.offset += syncAndLog(index);
  // }
  (void)final;
  syncAndLog(0);
}

uint64_t AIETraceOffload::syncAndLog(uint64_t index)
{
  std::cout << "Inside Sync and Log " << index << std::endl;
  // auto& bd = buffers[index];

  // if (bd.offset >= bd.usedSz)
  //   return 0;

  // // Amount of newly written trace
  // uint64_t nBytes = bd.usedSz - bd.offset;

  // // Sync to host
  // auto start = std::chrono::steady_clock::now();
  // void* hostBuf = deviceIntf->syncTraceBuf(bd.bufId, bd.offset, nBytes);
  // auto end = std::chrono::steady_clock::now();
  // debug_stream
  //   << "ts2mm_" << index << " : bytes : " << nBytes << " "
  //   << "sync: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "µs "
  //   << std::hex << "from 0x" << bd.offset << " to 0x"
  //   << bd.usedSz << std::dec << std::endl;

  // if (!hostBuf) {
  //   bd.offloadDone = true;
  //   return 0;
  // }

  // // Find amount of non-zero data in buffer
  // if (!isPLIO)
  //   nBytes = searchWrittenBytes(hostBuf, nBytes);

  // // check for full buffer
  // if ((bd.offset + nBytes >= bufAllocSz) && !mEnCircularBuf) {
  //   bd.isFull = true;
  //   bd.offloadDone = true;
  // }

  inp_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

  auto in_bo_map = inp_bo.map<void*>();
  std::cout << "Synced From Device" << std::endl;

  // Log nBytes of trace
  traceLogger->addAIETraceData(0, in_bo_map, inp_bo.size(), false);
  return inp_bo.size();
}

bool AIETraceOffload::isTraceBufferFull()
{
  for (auto& buf: buffers) {
    if (buf.isFull)
      return true;
  }

  return false;
}

void AIETraceOffload::checkCircularBufferSupport()
{
  // mEnCircularBuf = xrt_core::config::get_aie_trace_settings_reuse_buffer();
  // if (!mEnCircularBuf)
  //   return;

  // // gmio not supported
  // if (!isPLIO) {
  //   mEnCircularBuf = false;
  //   xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", AIE_TRACE_WARN_REUSE_GMIO);
  //   return;
  // }

  // // old datamover not supported for PLIO
  // if (!deviceIntf->supportsCircBufAIE()) {
  //   mEnCircularBuf = false;
  //   return;
  // }

  // // check for periodic offload
  // if (!continuousTrace()) {
  //   mEnCircularBuf = false;
  //   xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", AIE_TRACE_WARN_REUSE_PERIODIC);
  //   return;
  // }

  // // Warn if circular buffer settings not adequate
  // bool buffer_not_large_enough = (bufAllocSz < AIE_MIN_SIZE_CIRCULAR_BUF);
  // bool offload_not_fast_enough = (offloadIntervalUs > AIE_TRACE_REUSE_MAX_OFFLOAD_INT_US);
  // bool too_many_streams = (numStream > AIE_TRACE_REUSE_MAX_STREAMS);

  // if (buffer_not_large_enough || offload_not_fast_enough || too_many_streams) {
  //   std::stringstream msg;
  //   msg << AIE_TRACE_BUF_REUSE_WARN
  //       << "Requested Settings: "
  //       << "buffer_size/stream : " << bufAllocSz << ", "
  //       << "buffer_offload_interval_us : " << offloadIntervalUs << ", "
  //       << "trace streams : " << numStream;
  //   xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT", msg.str());
  // }

  // xrt_core::message::send(xrt_core::message::severity_level::info, "XRT", AIE_TRACE_CIRC_BUF_EN);
}

void AIETraceOffload::startOffload()
{
  // if (offloadStatus == AIEOffloadThreadStatus::RUNNING)
  //   return;

  // std::lock_guard<std::mutex> lock(statusLock);
  // offloadStatus = AIEOffloadThreadStatus::RUNNING;

  // offloadThread = std::thread(&AIETraceOffload::continuousOffload, this);
}

void AIETraceOffload::continuousOffload()
{
  // if (!bufferInitialized && !initReadTrace()) {
  //   offloadFinished();
  //   return;
  // }

  // while (keepOffloading()) {
  //   mReadTrace(false);
  //   std::this_thread::sleep_for(std::chrono::microseconds(offloadIntervalUs));
  // }

  // // Note: This will call flush and reset on datamover
  // mReadTrace(true);
  // endReadTrace();
  // offloadFinished();
}

bool AIETraceOffload::keepOffloading()
{
  std::lock_guard<std::mutex> lock(statusLock);
  return (AIEOffloadThreadStatus::RUNNING == offloadStatus);
}

void AIETraceOffload::stopOffload()
{
  std::lock_guard<std::mutex> lock(statusLock);
  if (AIEOffloadThreadStatus::STOPPED == offloadStatus)
    return;
  offloadStatus = AIEOffloadThreadStatus::STOPPING;
}

void AIETraceOffload::offloadFinished()
{
  std::lock_guard<std::mutex> lock(statusLock);
  if (AIEOffloadThreadStatus::STOPPED == offloadStatus)
    return;
  offloadStatus = AIEOffloadThreadStatus::STOPPED;
}

uint64_t AIETraceOffload::searchWrittenBytes(void* buf, uint64_t bytes)
{
  /*
   * Look For trace boundary using binary search.
   * Use Dword to be safe
   */
  auto words = static_cast<uint64_t *>(buf);
  uint64_t wordcount = bytes / TRACE_PACKET_SIZE;

  // indices
  int64_t low = 0;
  int64_t high = static_cast<int64_t>(wordcount) - 1;

  // Boundary at which trace ends and 0s begin
  uint64_t boundary = wordcount;

  while (low <= high) {
    int64_t mid = low + (high - low) / 2;
    if (!words[mid]) {
      boundary = mid;
      high = mid - 1;
    } else {
      low = mid + 1;
    }
  }

  uint64_t written = boundary * TRACE_PACKET_SIZE;

  debug_stream
    << "Found Boundary at 0x" << std::hex << written << std::dec << std::endl;

  return written;
}

}

