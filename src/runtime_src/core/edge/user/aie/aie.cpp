/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include "aie.h"
#include "core/common/error.h"
#include "aie_event.h"
#ifndef __AIESIM__
#include "core/common/message.h"
#include "core/edge/user/shim.h"
#include "xaiengine/xlnx-ai-engine.h"
#include <sys/ioctl.h>
#include <sys/mman.h>
#endif

#include <cerrno>
#include <iostream>

namespace zynqaie {

XAie_InstDeclare(DevInst, &ConfigPtr);   // Declare global device instance

Aie::Aie(const std::shared_ptr<xrt_core::device>& device)
{
    XAie_SetupConfig(ConfigPtr, HW_GEN, XAIE_BASE_ADDR, XAIE_COL_SHIFT,
        XAIE_ROW_SHIFT, XAIE_NUM_COLS, XAIE_NUM_ROWS,
        XAIE_SHIM_ROW, XAIE_RESERVED_TILE_ROW_START,
        XAIE_RESERVED_TILE_NUM_ROWS, XAIE_AIE_TILE_ROW_START,
        XAIE_AIE_TILE_NUM_ROWS);

#ifndef __AIESIM__
    auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

    /* TODO get partition id and uid from XCLBIN or PDI */
    uint32_t partition_id = 1;
    uint32_t uid = 0;
    drm_zocl_aie_fd aiefd = { partition_id, uid, 0 };
    int ret = drv->getPartitionFd(aiefd);
    if (ret)
        throw xrt_core::error(ret, "Create AIE failed. Can not get AIE fd");
    fd = aiefd.fd;

    ConfigPtr.PartProp.Handle = fd;
#endif

    AieRC rc;
    if ((rc = XAie_CfgInitialize(&DevInst, &ConfigPtr)) != XAIE_OK)
        throw xrt_core::error(-EINVAL, "Failed to initialize AIE configuration: " + std::to_string(rc));
    devInst = &DevInst;

    /* Initialize PLIO metadata */
    for (auto& plio : xrt_core::edge::aie::get_plios(device.get()))
        plios.emplace_back(std::move(plio));

    /* Initialize graph GMIO metadata */
    for (auto& gmio : xrt_core::edge::aie::get_gmios(device.get()))
        gmios.emplace_back(std::move(gmio));

    /*
     * Initialize AIE shim DMA on column base if there is one for
     * this column.
     */
    numCols = XAIE_NUM_COLS;
    shim_dma.resize(numCols);
    for (auto& gmio : gmios) {
        if (gmio.shim_col > numCols)
            throw xrt_core::error(-EINVAL, "GMIO " + gmio.name + " shim column " + std::to_string(gmio.shim_col) + " does not exist");

        auto dma = &shim_dma.at(gmio.shim_col);
        XAie_LocType shimTile = XAie_TileLoc(gmio.shim_col, 0);

        if (!dma->configured) {
            XAie_DmaDescInit(devInst, &(dma->desc), shimTile);
            dma->configured = true;
        }

        auto chan = gmio.channel_number;
        /* type 0: GM->AIE; type 1: AIE->GM */
        XAie_DmaDirection dir = gmio.type == 0 ? DMA_MM2S : DMA_S2MM;
        uint8_t pch = CONVERT_LCHANL_TO_PCHANL(chan);
        XAie_DmaChannelEnable(devInst, shimTile, pch, dir);
        XAie_DmaSetAxi(&(dma->desc), 0, gmio.burst_len, 0, 0, 0);

        XAie_DmaGetMaxQueueSize(devInst, shimTile, &(dma->maxqSize));
        for (int i = 0; i < dma->maxqSize; ++i) {
            /*
             * 16 BDs are allocated to 4 channels.
             * Channel0: BD0~BD3
             * Channel1: BD4~BD7
             * Channel2: BD8~BD11
             * Channel3: BD12~BD15
             */
            int bd_num = chan * dma->maxqSize + i;
            BD bd;
            bd.bd_num = bd_num;
            dma->dma_chan[chan].idle_bds.push(bd);
        }
    }

    Resources::AIE::initialize(XAIE_NUM_COLS, XAIE_NUM_ROWS);
}

Aie::~Aie()
{
#ifndef __AIESIM__
  if (devInst)
    XAie_Finish(devInst);
#endif
}

XAie_DevInst* Aie::getDevInst()
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "AIE is not initialized");

  return devInst;
}

void
Aie::
sync_bo(xrtBufferHandle bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Can't sync BO: AIE is not initialized");

  auto gmio = std::find_if(gmios.begin(), gmios.end(),
            [gmioName](gmio_type it) { return it.name.compare(gmioName) == 0; });

  if (gmio == gmios.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  submit_sync_bo(bo, gmio, dir, size, offset);

  ShimDMA *dmap = &shim_dma.at(gmio->shim_col);
  auto chan = gmio->channel_number;
  auto shim_tile = XAie_TileLoc(gmio->shim_col, 0);
  XAie_DmaDirection gmdir = gmio->type == 0 ? DMA_MM2S : DMA_S2MM;

  wait_sync_bo(dmap, chan, shim_tile, gmdir, 0);
}

void
Aie::
sync_bo_nb(xrtBufferHandle bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Can't sync BO: AIE is not initialized");

  auto gmio = std::find_if(gmios.begin(), gmios.end(),
            [gmioName](gmio_type it) { return it.name.compare(gmioName) == 0; });

  if (gmio == gmios.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  submit_sync_bo(bo, gmio, dir, size, offset);
}

void
Aie::
wait_gmio(const std::string& gmioName)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Can't wait GMIO: AIE is not initialized");

  auto gmio = std::find_if(gmios.begin(), gmios.end(),
            [gmioName](gmio_type it) { return it.name.compare(gmioName) == 0; });

  if (gmio == gmios.end())
    throw xrt_core::error(-EINVAL, "Can't wait GMIO: GMIO name not found");

  ShimDMA *dmap = &shim_dma.at(gmio->shim_col);
  auto chan = gmio->channel_number;
  auto shim_tile = XAie_TileLoc(gmio->shim_col, 0);
  XAie_DmaDirection gmdir = gmio->type == 0 ? DMA_MM2S : DMA_S2MM;

  wait_sync_bo(dmap, chan, shim_tile, gmdir, 0);
}

void
Aie::
submit_sync_bo(xrtBufferHandle bo, std::vector<gmio_type>::iterator& gmio, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  switch (dir) {
  case XCL_BO_SYNC_BO_GMIO_TO_AIE:
    if (gmio->type != 0)
      throw xrt_core::error(-EINVAL, "Sync BO direction does not match GMIO type");
    break;
  case XCL_BO_SYNC_BO_AIE_TO_GMIO:
    if (gmio->type != 1)
      throw xrt_core::error(-EINVAL, "Sync BO direction does not match GMIO type");
    break;
  default:
    throw xrt_core::error(-EINVAL, "Can't sync BO: unknown direction.");
  }

  if (size & XAIEDMA_SHIM_TXFER_LEN32_MASK != 0)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: size is not 32 bits aligned.");

  ShimDMA *dmap = &shim_dma.at(gmio->shim_col);
  auto chan = gmio->channel_number;
  auto shim_tile = XAie_TileLoc(gmio->shim_col, 0);
  XAie_DmaDirection gmdir = gmio->type == 0 ? DMA_MM2S : DMA_S2MM;
  uint32_t pchan = CONVERT_LCHANL_TO_PCHANL(chan);

  /* Find a free BD. Busy wait until we get one. */
  while (dmap->dma_chan[chan].idle_bds.empty()) {
    uint8_t npend;
    XAie_DmaGetPendingBdCount(devInst, shim_tile, pchan, gmdir, &npend);

    int num_comp = dmap->maxqSize - npend;

    /* Pending BD is completed by order per Shim DMA spec. */
    for (int i = 0; i < num_comp; ++i) {
      BD bd = dmap->dma_chan[chan].pend_bds.front();
      dmap->dma_chan[chan].pend_bds.pop();
      dmap->dma_chan[chan].idle_bds.push(bd);
    }
  }

  BD_scope bd_scope(dmap->dma_chan[chan].idle_bds.front(), this);
  auto& bd = bd_scope.get();
  dmap->dma_chan[chan].idle_bds.pop();
  prepare_bd(bd, bo);

#ifndef __AIESIM__
  XAie_DmaSetAddrLen(&(dmap->desc), (uint64_t)(bd.vaddr + offset), size);
#else
  XAie_DmaSetAddrLen(&(dmap->desc), (uint64_t)(xrtBOAddress(bo) + offset), size);
#endif

  /* Set BD lock */
  auto acq_lock = XAie_LockInit(bd.bd_num, XAIE_LOCK_WITH_NO_VALUE);
  auto rel_lock = XAie_LockInit(bd.bd_num, XAIE_LOCK_WITH_NO_VALUE);
  XAie_DmaSetLock(&(dmap->desc), acq_lock, rel_lock);

  XAie_DmaEnableBd(&(dmap->desc));

  /* Write BD */
  XAie_DmaWriteBd(devInst, &(dmap->desc), shim_tile, bd.bd_num);

  /* Enqueue BD */
  XAie_DmaChannelPushBdToQueue(devInst, shim_tile, pchan, gmdir, bd.bd_num);
  dmap->dma_chan[chan].pend_bds.push(bd);
}

void
Aie::
wait_sync_bo(ShimDMA *dmap, uint32_t chan, XAie_LocType& tile, XAie_DmaDirection gmdir, uint32_t timeout)
{
  while (XAie_DmaWaitForDone(devInst, tile, CONVERT_LCHANL_TO_PCHANL(chan), gmdir, timeout) != XAIE_OK);

  while (!dmap->dma_chan[chan].pend_bds.empty()) {
    BD bd = dmap->dma_chan[chan].pend_bds.front();
    dmap->dma_chan[chan].pend_bds.pop();
    dmap->dma_chan[chan].idle_bds.push(bd);
  }
}

void
Aie::
prepare_bd(BD& bd, xrtBufferHandle& bo)
{
#ifndef __AIESIM__
  auto buf_fd = xrtBOExport(bo);
  if (buf_fd == XRT_NULL_BO_EXPORT)
    throw xrt_core::error(-errno, "Sync AIE Bo: fail to export BO.");
  bd.buf_fd = buf_fd;

  auto ret = ioctl(fd, AIE_ATTACH_DMABUF_IOCTL, buf_fd);
  if (ret)
    throw xrt_core::error(-errno, "Sync AIE Bo: fail to attach DMA buf.");

  auto bosize = xrtBOSize(bo);
  bd.size = bosize;

  bd.vaddr = reinterpret_cast<char *>(mmap(NULL, bosize, PROT_READ | PROT_WRITE, MAP_SHARED, buf_fd, 0));
#endif
}

void
Aie::
clear_bd(BD& bd)
{
#ifndef __AIESIM__
  munmap(bd.vaddr, bd.size);
  bd.vaddr = nullptr;
  auto ret = ioctl(fd, AIE_DETACH_DMABUF_IOCTL, bd.buf_fd);
  if (ret)
    throw xrt_core::error(-errno, "Sync AIE Bo: fail to detach DMA buf.");
  close(bd.buf_fd);
#endif
}

void
Aie::
reset(const xrt_core::device* device)
{
#ifndef __AIESIM__
    if (!devInst)
        throw xrt_core::error(-EINVAL, "Can't Reset AIE: AIE is not initialized");

    XAie_Finish(devInst);
    devInst = nullptr;

    auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

    /* TODO get partition id and uid from XCLBIN or PDI */
    uint32_t partition_id = 1;

    drm_zocl_aie_reset reset = { partition_id };
    int ret = drv->resetAIEArray(reset);
    if (ret)
        throw xrt_core::error(ret, "Fail to reset AIE Array");
#endif
}

int
Aie::
start_profiling(int option, const std::string& port1_name, const std::string& port2_name, uint32_t value)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Start profiling fails: AIE is not initialized");

  switch (option) {

  case IO_TOTAL_STREAM_RUNNING_TO_IDLE_CYCLE:
    return start_profiling_run_idle(port1_name);

  case IO_STREAM_START_TO_BYTES_TRANSFERRED_CYCLES:
    return start_profiling_start_bytes(port1_name, value);

  case IO_STREAM_START_DIFFERENCE_CYCLES:
    return start_profiling_diff_cycles(port1_name, port2_name);

  case IO_STREAM_RUNNING_EVENT_COUNT:
    return start_profiling_event_count(port1_name);

  default:
    throw xrt_core::error(-EINVAL, "Start profiling fails: unknown profiling option.");
  }
}

uint64_t
Aie::
read_profiling(int phdl)
{
  uint64_t value = 0;

  std::vector<Resources::AcquiredResource>& acquiredResourcesForThisHandle = eventRecords[phdl].acquiredResources;

  Resources::AcquiredResource& acquiredResource = acquiredResourcesForThisHandle[0];
  XAie_ModuleType XAieModuleType = AIEResourceModuletoXAieModuleTypeMap[acquiredResource.module];

  if (acquiredResource.resource == Resources::performance_counter)
    XAie_PerfCounterGet(devInst, acquiredResource.loc, XAieModuleType, acquiredResource.id, (u32*)(&value));
  else
    throw xrt_core::error(-EAGAIN, "Can't read profiling: The acquired resources order does not match the profiling option.");

  return value;
}

void
Aie::
stop_profiling(int phdl)
{
  if (phdl < eventRecords.size() && eventRecords[phdl].option >= 0) {
    std::vector<Resources::AcquiredResource>& acquiredResourcesForThisHandle = eventRecords[phdl].acquiredResources;
    for (int i = 0; i < acquiredResourcesForThisHandle.size(); i++) {
      Resources::AcquiredResource& acquiredResource = acquiredResourcesForThisHandle[i];
      XAie_ModuleType XAieModuleType = AIEResourceModuletoXAieModuleTypeMap[acquiredResource.module];

      if (acquiredResource.resource == Resources::performance_counter) {
        u8 counterId = acquiredResource.id;

        XAie_PerfCounterReset(devInst, acquiredResource.loc, XAieModuleType, counterId);
        XAie_PerfCounterResetControlReset(devInst, acquiredResource.loc, XAieModuleType, counterId);

        if (acquiredResource.module == Resources::pl_module)
          Resources::AIE::getShimTile(acquiredResource.loc.Col)->plModule.releasePerformanceCounter(phdl, counterId);
        else if (acquiredResource.module == Resources::core_module)
          Resources::AIE::getAIETile(acquiredResource.loc.Col, acquiredResource.loc.Row - 1)->coreModule.releasePerformanceCounter(phdl, counterId);
      } else if (acquiredResource.resource == Resources::stream_switch_event_port) {
        u8 eventPortId = acquiredResource.id;

        XAie_EventSelectStrmPortReset(devInst, acquiredResource.loc, eventPortId);

        if (acquiredResource.module == Resources::pl_module)
          Resources::AIE::getShimTile(acquiredResource.loc.Col)->plModule.releaseStreamEventPort(phdl, eventPortId);
      }
    }
  }
}

void
Aie::
get_profiling_config(const std::string& port_name, XAie_LocType& out_shim_tile, XAie_StrmPortIntf& out_mode, uint8_t& out_stream_id)
{
  auto gmio = std::find_if(gmios.begin(), gmios.end(),
            [&port_name](auto& it) { return it.name.compare(port_name) == 0; });

  // For PLIO inside graph, there is no name property.
  // So we need to match logical name too
  auto plio = std::find_if(plios.begin(), plios.end(),
            [&port_name](auto& it) { return it.name.compare(port_name) == 0; });
  if (plio == plios.end()) {
    plio = std::find_if(plios.begin(), plios.end(),
            [&port_name](auto& it) { return it.logical_name.compare(port_name) == 0; });
  }

  if (gmio == gmios.end() && plio == plios.end())
    throw xrt_core::error(-EINVAL, "Can't start profiling: port name '" + port_name + "' not found");

  if (gmio != gmios.end() && plio != plios.end())
    throw xrt_core::error(-EINVAL, "Can't start profiling: ambiguous port name '" + port_name + "'");

  XAie_LocType shim_tile;
  XAie_StrmPortIntf mode;
  uint8_t stream_id;
  if (gmio != gmios.end()) {
    shim_tile = XAie_TileLoc(gmio->shim_col, 0);
    /* type 0: GM->AIE; type 1: AIE->GM */
    mode = gmio->type == 1 ? XAIE_STRMSW_MASTER : XAIE_STRMSW_SLAVE;
    stream_id = gmio->stream_id;
  } else {
    shim_tile = XAie_TileLoc(plio->shim_col, 0);
    mode = plio->is_master ? XAIE_STRMSW_MASTER: XAIE_STRMSW_SLAVE;
    stream_id = plio->stream_id;
  }

  out_shim_tile = shim_tile;
  out_mode = mode;
  out_stream_id = stream_id;
}

int
Aie::
start_profiling_run_idle(const std::string& port_name)
{
  int handle = -1;

  XAie_LocType shim_tile;
  XAie_StrmPortIntf mode;
  uint8_t stream_id;
  get_profiling_config(port_name, shim_tile, mode, stream_id);

  int handleId = eventRecords.size();
  int eventPortId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestStreamEventPort(handleId);
  int counterId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestPerformanceCounter(handleId);
  if (counterId >= 0 && eventPortId >= 0) {
    XAie_EventSelectStrmPort(devInst, shim_tile, (uint8_t)eventPortId, mode, SOUTH, stream_id);
    XAie_PerfCounterControlSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], XAIETILE_EVENT_SHIM_PORT_IDLE[eventPortId]);
    eventRecords.push_back( { IO_TOTAL_STREAM_RUNNING_TO_IDLE_CYCLE,
                { { shim_tile, Resources::pl_module, Resources::performance_counter, (size_t)counterId },
                { shim_tile, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId } } } );
    handle = handleId;
  } else {
    if (counterId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releasePerformanceCounter(handleId, counterId);
    if (eventPortId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releaseStreamEventPort(handleId, eventPortId);
    throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request performance counter or stream switch event port resources.");
  }

  return handle;
}

int
Aie::
start_profiling_start_bytes(const std::string& port_name, uint32_t value)
{
  int handle = -1;

  XAie_LocType shim_tile;
  XAie_StrmPortIntf mode;
  uint8_t stream_id;

  get_profiling_config(port_name, shim_tile, mode, stream_id);

  int handleId = eventRecords.size();
  int eventPortId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestStreamEventPort(handleId);
  int counterId0 = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestPerformanceCounter(handleId);
  int counterId1 = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestPerformanceCounter(handleId);

  if (counterId0 >= 0 && counterId1 >= 0 && eventPortId >= 0) {
    XAie_EventSelectStrmPort(devInst, shim_tile, (uint8_t)eventPortId, mode, SOUTH, stream_id);
    XAie_PerfCounterEventValueSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId1, value / 4);
    XAie_PerfCounterControlSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId0, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], XAIE_EVENT_PERF_CNT_1_PL);
    XAie_PerfCounterControlSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId1, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId], XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId]);
    eventRecords.push_back( { IO_STREAM_START_TO_BYTES_TRANSFERRED_CYCLES,
                { { shim_tile, Resources::pl_module, Resources::performance_counter, (size_t)counterId0 },
                { shim_tile, Resources::pl_module, Resources::performance_counter, (size_t)counterId1 },
                { shim_tile, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId } } } );

    handle = handleId;
  } else {
    if (counterId0 >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releasePerformanceCounter(handleId, counterId0);
    if (counterId1 >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releasePerformanceCounter(handleId, counterId1);
    if (eventPortId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releaseStreamEventPort(handleId, eventPortId);
    throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request performance counter or stream switch event port resources.");
  }

  return handle;
}

int
Aie::
start_profiling_diff_cycles(const std::string& port1_name, const std::string& port2_name)
{
  int handle = -1;

  XAie_LocType shim_tile1;
  XAie_StrmPortIntf mode1;
  uint8_t stream_id1;
  XAie_LocType shim_tile2;
  XAie_StrmPortIntf mode2;
  uint8_t stream_id2;

  get_profiling_config(port1_name, shim_tile1, mode1, stream_id1);
  get_profiling_config(port2_name, shim_tile2, mode2, stream_id2);

  int handleId = eventRecords.size();
  int eventPortId1 = Resources::AIE::getShimTile(shim_tile1.Col)->plModule.requestStreamEventPort(handleId);
  int counterId1 = Resources::AIE::getShimTile(shim_tile1.Col)->plModule.requestPerformanceCounter(handleId);
  int eventPortId2 = Resources::AIE::getShimTile(shim_tile2.Col)->plModule.requestStreamEventPort(handleId);
  int counterId2 = Resources::AIE::getShimTile(shim_tile2.Col)->plModule.requestPerformanceCounter(handleId);

  if (counterId1 >= 0 && eventPortId1 >= 0 && counterId2 >= 0 && eventPortId2 >= 0) {
    if (shim_tile1.Col == shim_tile2.Col) {
      XAie_EventSelectStrmPort(devInst, shim_tile1, (uint8_t)eventPortId1, mode1, SOUTH, stream_id1);
      XAie_PerfCounterControlSet(devInst, shim_tile1, XAIE_PL_MOD, (uint8_t)counterId1, XAIE_EVENT_USER_EVENT_0_PL, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId1]);
      XAie_EventSelectStrmPort(devInst, shim_tile2, (uint8_t)eventPortId2, mode2, SOUTH, stream_id2);
      XAie_PerfCounterControlSet(devInst, shim_tile2, XAIE_PL_MOD, (uint8_t)counterId2, XAIE_EVENT_USER_EVENT_0_PL, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId2]);
      XAie_EventGenerate(devInst, shim_tile1, XAIE_PL_MOD, XAIE_EVENT_USER_EVENT_0_PL);
      eventRecords.push_back({ IO_STREAM_START_DIFFERENCE_CYCLES,
                  { { shim_tile1, Resources::pl_module, Resources::performance_counter, (size_t)counterId1 },
                  { shim_tile2, Resources::pl_module, Resources::performance_counter, (size_t)counterId2 },
                  { shim_tile1, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId1 },
                  { shim_tile2, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId2 } } });
      handle = handleId;
    } else {
      int westShimColumn = (shim_tile1.Col < shim_tile2.Col) ? shim_tile1.Col : shim_tile2.Col;
      int eastShimColumn = (shim_tile1.Col < shim_tile2.Col) ? shim_tile2.Col : shim_tile1.Col;
      int numBcastShimColumns = eastShimColumn - westShimColumn + 1;
      int broadcastId = -1;

      std::vector<std::vector<short>> eventBroadcastResourcesOnShimColumns(numBcastShimColumns);
      for (int i = 0; i < numBcastShimColumns; i++)
        eventBroadcastResourcesOnShimColumns[i] = Resources::AIE::getShimTile(westShimColumn + i)->plModule.availableEventBroadcast();
      int largestBroadcastIndexAvailableForAllShimColumns;
      for (largestBroadcastIndexAvailableForAllShimColumns = NUM_EVENT_BROADCASTS - 1; largestBroadcastIndexAvailableForAllShimColumns >= 0;
                  largestBroadcastIndexAvailableForAllShimColumns--) {
        bool allAvailable = true;
        for (int i = 0; i < numBcastShimColumns; i++) {
          if (eventBroadcastResourcesOnShimColumns[i][largestBroadcastIndexAvailableForAllShimColumns] != -1) {
            allAvailable = false;
            break;
          }
        }
        if (allAvailable)
          break;
      }
      broadcastId = largestBroadcastIndexAvailableForAllShimColumns;

      if (broadcastId >= 0) {
        for (int i = 0; i < numBcastShimColumns; i++)
          Resources::AIE::getShimTile(westShimColumn + i)->plModule.requestEventBroadcast(handleId, broadcastId);
      }

      if (broadcastId >= 0) {
        XAie_EventSelectStrmPort(devInst, shim_tile1, (uint8_t)eventPortId1, mode1, SOUTH, stream_id1);
        XAie_PerfCounterControlSet(devInst, shim_tile1, XAIE_PL_MOD, (uint8_t)counterId1, XAIE_EVENT_USER_EVENT_0_PL, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId1]);
        XAie_EventSelectStrmPort(devInst, shim_tile2, (uint8_t)eventPortId2, mode2, SOUTH, stream_id2);
        XAie_PerfCounterControlSet(devInst, shim_tile2, XAIE_PL_MOD, (uint8_t)counterId2, XAIETILE_EVENT_SHIM_BROADCAST_A[broadcastId], XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId2]);

        u16 bcastMask = (1 << broadcastId);
        XAie_LocType westTileLoc = XAie_TileLoc(westShimColumn, 0);
        XAie_EventBroadcastBlockMapDir(devInst, westTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, bcastMask, XAIE_EVENT_BROADCAST_WEST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);
        XAie_EventBroadcastBlockMapDir(devInst, westTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, bcastMask, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);

        for (int i = 1; i < numBcastShimColumns - 1; i++) {
          XAie_LocType intermediateTileLoc = XAie_TileLoc(westShimColumn + i, 0);
          XAie_EventBroadcastBlockMapDir(devInst, intermediateTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, bcastMask, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);
          XAie_EventBroadcastBlockMapDir(devInst, intermediateTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_B, bcastMask, XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);
        }

        XAie_LocType eastTileLoc = XAie_TileLoc(eastShimColumn, 0);
        XAie_EventBroadcastBlockMapDir(devInst, eastTileLoc, XAIE_PL_MOD, XAIE_EVENT_SWITCH_A, bcastMask, XAIE_EVENT_BROADCAST_EAST | XAIE_EVENT_BROADCAST_NORTH | XAIE_EVENT_BROADCAST_SOUTH);

        XAie_EventBroadcast(devInst, shim_tile1, XAIE_PL_MOD, (uint8_t)broadcastId, XAIE_EVENT_USER_EVENT_0_PL);
        XAie_EventGenerate(devInst, shim_tile1, XAIE_PL_MOD, XAIE_EVENT_USER_EVENT_0_PL);

        eventRecords.push_back({ IO_STREAM_START_DIFFERENCE_CYCLES,
                    { { shim_tile1, Resources::pl_module, Resources::performance_counter, (size_t)counterId1 },
                    { shim_tile2, Resources::pl_module, Resources::performance_counter, (size_t)counterId2 },
                    { shim_tile1, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId1 },
                    { shim_tile2, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId2 },
                    { shim_tile1, Resources::pl_module, Resources::event_broadcast, (size_t)broadcastId } } });

        handle = handleId;
      } else
        throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request event broadcast resources across shim tiles.");
    }
  } else
    throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request performance counter or stream switch event port resources.");

  return handle;
}

int
Aie::
start_profiling_event_count(const std::string& port_name)
{
  int handle = -1;

  XAie_LocType shim_tile;
  XAie_StrmPortIntf mode;
  uint8_t stream_id;

  get_profiling_config(port_name, shim_tile, mode, stream_id);

  int handleId = eventRecords.size();
  int eventPortId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestStreamEventPort(handleId);
  int counterId = Resources::AIE::getShimTile(shim_tile.Col)->plModule.requestPerformanceCounter(handleId);

  if (counterId >= 0 && eventPortId >= 0) {
    XAie_EventSelectStrmPort(devInst, shim_tile, (uint8_t)eventPortId, mode, SOUTH, stream_id);
    XAie_PerfCounterControlSet(devInst, shim_tile, XAIE_PL_MOD, (uint8_t)counterId, XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId],                                     XAIETILE_EVENT_SHIM_PORT_RUNNING[eventPortId]);
    eventRecords.push_back({ IO_STREAM_RUNNING_EVENT_COUNT,
                { { shim_tile, Resources::pl_module, Resources::performance_counter, (size_t)counterId },
                { shim_tile, Resources::pl_module, Resources::stream_switch_event_port, (size_t)eventPortId } } });
    handle = handleId;
  } else {
    if (counterId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releasePerformanceCounter(handleId, counterId);
    if (eventPortId >= 0)
      Resources::AIE::getShimTile(shim_tile.Col)->plModule.releaseStreamEventPort(handleId, eventPortId);
    throw xrt_core::error(-EAGAIN, "Can't start profiling: Failed to request performance counter or stream switch event port resources.");
  }

  return handle;
}

}
