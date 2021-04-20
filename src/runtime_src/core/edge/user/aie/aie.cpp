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

#include "aie.h"
#include "core/common/error.h"
#include "common_layer/fal_util.h"
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
    adf::driver_config driver_config = xrt_core::edge::aie::get_driver_config(device.get());

    XAie_SetupConfig(ConfigPtr,
        driver_config.hw_gen,
        driver_config.base_address,
        driver_config.column_shift,
        driver_config.row_shift,
        driver_config.num_columns,
        driver_config.num_rows,
        driver_config.shim_row,
        driver_config.reserved_row_start,
        driver_config.reserved_num_rows,
        driver_config.aie_tile_row_start,
        driver_config.aie_tile_num_rows);

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

    access_mode = drv->getAIEAccessMode();

    ConfigPtr.PartProp.Handle = fd;
#endif

    AieRC rc;
    if ((rc = XAie_CfgInitialize(&DevInst, &ConfigPtr)) != XAIE_OK)
        throw xrt_core::error(-EINVAL, "Failed to initialize AIE configuration: " + std::to_string(rc));
    devInst = &DevInst;

    adf::aiecompiler_options aiecompiler_options = xrt_core::edge::aie::get_aiecompiler_options(device.get());
    adf::config_manager::initialize(devInst, driver_config.reserved_num_rows, aiecompiler_options.broadcast_enable_core);

    fal_util::initialize(devInst); //resource manager initialization
    
    /* Initialize PLIO metadata */
    plio_configs = xrt_core::edge::aie::get_plios(device.get());

    /* Initialize gmio api instances */
    gmio_configs = xrt_core::edge::aie::get_gmios(device.get());
    for (auto config_itr = gmio_configs.begin(); config_itr != gmio_configs.end(); config_itr++)
    {
        auto p_gmio_api = std::make_shared<adf::gmio_api>(&config_itr->second);
        p_gmio_api->configure();
        gmio_apis[config_itr->first] = p_gmio_api;
    }
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
open_context(const xrt_core::device* device, xrt::aie::access_mode am)
{
#ifndef __AIESIM__
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  auto current_am = drv->getAIEAccessMode();
  if (current_am != xrt::aie::access_mode::none)
    throw xrt_core::error(-EBUSY, "Can not change current AIE access mode");

  int ret = drv->openAIEContext(am);
  if (ret)
      throw xrt_core::error(ret, "Fail to open AIE context");

  drv->setAIEAccessMode(am);
  access_mode = am;
#endif
}

bool
Aie::
is_context_set()
{
  return (access_mode != xrt::aie::access_mode::none);
}

void
Aie::
sync_bo(xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Can't sync BO: AIE is not initialized");

  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't sync BO");

  auto gmio_itr = gmio_apis.find(gmioName);
  if (gmio_itr == gmio_apis.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");
  
  auto gmio_config_itr = gmio_configs.find(gmioName);
  if (gmio_config_itr == gmio_configs.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  submit_sync_bo(bo, gmio_itr->second, gmio_config_itr->second, dir, size, offset);
  gmio_itr->second->wait();
}

void
Aie::
sync_bo_nb(xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Can't sync BO: AIE is not initialized");

  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't sync BO");

  auto gmio_itr = gmio_apis.find(gmioName);
  if (gmio_itr == gmio_apis.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");
  
  auto gmio_config_itr = gmio_configs.find(gmioName);
  if (gmio_config_itr == gmio_configs.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");
  
  submit_sync_bo(bo, gmio_itr->second, gmio_config_itr->second, dir, size, offset);
}

void
Aie::
wait_gmio(const std::string& gmioName)
{
  if (!devInst)
    throw xrt_core::error(-EINVAL, "Can't wait GMIO: AIE is not initialized");

  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't wait gmio");

  auto gmio_itr = gmio_apis.find(gmioName);
  if (gmio_itr == gmio_apis.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");
    
  gmio_itr->second->wait();
}

void
Aie::
submit_sync_bo(xrt::bo& bo, std::shared_ptr<adf::gmio_api>& gmio_api, adf::gmio_config& gmio_config, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  switch (dir) {
  case XCL_BO_SYNC_BO_GMIO_TO_AIE:
    if (gmio_config.type != 0)
      throw xrt_core::error(-EINVAL, "Sync BO direction does not match GMIO type");
    break;
  case XCL_BO_SYNC_BO_AIE_TO_GMIO:
    if (gmio_config.type != 1)
      throw xrt_core::error(-EINVAL, "Sync BO direction does not match GMIO type");
    break;
  default:
    throw xrt_core::error(-EINVAL, "Can't sync BO: unknown direction.");
  }

  if (size & XAIEDMA_SHIM_TXFER_LEN32_MASK != 0)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: size is not 32 bits aligned.");
    
  BD bd;
  prepare_bd(bd, bo);
#ifndef __AIESIM__
  gmio_api->enqueueBD((uint64_t)bd.vaddr + offset, size);
#else
  gmio_api->enqueueBD((uint64_t)bo.address() + offset, size);
#endif
  clear_bd(bd);
}

void
Aie::
prepare_bd(BD& bd, xrt::bo& bo)
{
#ifndef __AIESIM__
  auto buf_fd = bo.export_buffer();
  if (buf_fd == XRT_NULL_BO_EXPORT)
    throw xrt_core::error(-errno, "Sync AIE Bo: fail to export BO.");
  bd.buf_fd = buf_fd;

  auto ret = ioctl(fd, AIE_ATTACH_DMABUF_IOCTL, buf_fd);
  if (ret)
    throw xrt_core::error(-errno, "Sync AIE Bo: fail to attach DMA buf.");

  auto bosize = bo.size();
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

  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't reset AIE");

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

  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't do profiling");

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
  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't do profiling");

  uint64_t value = 0;
  if (eventRecords.size() > phdl)
    value = adf::profiling::read(eventRecords[phdl].acquiredResources, eventRecords[phdl].option == IO_STREAM_START_DIFFERENCE_CYCLES);
  else
    throw xrt_core::error(-EAGAIN, "Read profiling failed: invalid handle.");
  return value;
}

void
Aie::
stop_profiling(int phdl)
{
  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't do profiling");
  if (eventRecords.size() > phdl)
    adf::profiling::stop(eventRecords[phdl].acquiredResources);
  else
    throw xrt_core::error(-EINVAL, "Stop profiling failed: invalid handle.");
}

adf::shim_config
Aie::
get_shim_config(const std::string& port_name)
{
  auto gmio = gmio_configs.find(port_name);

  // For PLIO inside graph, there is no name property.
  // So we need to match logical name too
  auto plio = plio_configs.find(port_name);
  if (plio == plio_configs.end()) {
    plio = std::find_if(plio_configs.begin(), plio_configs.end(),
            [&port_name](auto& it) { return it.second.logicalName.compare(port_name) == 0; });
  }

  if (gmio == gmio_configs.end() && plio == plio_configs.end())
    throw xrt_core::error(-EINVAL, "Can't start profiling: port name '" + port_name + "' not found");

  if (gmio != gmio_configs.end() && plio != plio_configs.end())
    throw xrt_core::error(-EINVAL, "Can't start profiling: ambiguous port name '" + port_name + "'");

  if (gmio != gmio_configs.end()) {
    return adf::shim_config(&gmio->second);
  } else {
    return adf::shim_config(&plio->second);
  }
}

int
Aie::
start_profiling_run_idle(const std::string& port_name)
{
  int handle = -1;
  std::vector<std::shared_ptr<xaiefal::XAieRsc>> acquiredResources;
  if (adf::profiling::profile_stream_running_to_idle_cycles(get_shim_config(port_name), acquiredResources) == adf::err_code::ok)
  {
    handle = eventRecords.size();
    eventRecords.push_back({ IO_TOTAL_STREAM_RUNNING_TO_IDLE_CYCLE, acquiredResources });
  }
  return handle;
}

int
Aie::
start_profiling_start_bytes(const std::string& port_name, uint32_t value)
{
  int handle = -1;
  std::vector<std::shared_ptr<xaiefal::XAieRsc>> acquiredResources;
  if (adf::profiling::profile_stream_start_to_transfer_complete_cycles(get_shim_config(port_name), value, acquiredResources) == adf::err_code::ok)
  {
    handle = eventRecords.size();
    eventRecords.push_back({ IO_STREAM_START_TO_BYTES_TRANSFERRED_CYCLES, acquiredResources });
  }
  return handle;
}

int
Aie::
start_profiling_diff_cycles(const std::string& port1_name, const std::string& port2_name)
{
  int handle = -1;
  std::vector<std::shared_ptr<xaiefal::XAieRsc>> acquiredResources;
  if (adf::profiling::profile_start_time_difference_btw_two_streams(get_shim_config(port1_name), get_shim_config(port2_name), acquiredResources) == adf::err_code::ok)
  {
    handle = eventRecords.size();
    eventRecords.push_back({ IO_STREAM_START_DIFFERENCE_CYCLES, acquiredResources });
  }
  return handle;
}

int
Aie::
start_profiling_event_count(const std::string& port_name)
{
  int handle = -1;
  std::vector<std::shared_ptr<xaiefal::XAieRsc>> acquiredResources;
  if (adf::profiling::profile_stream_running_event_count(get_shim_config(port_name), acquiredResources) == adf::err_code::ok)
  {
    handle = eventRecords.size();
    eventRecords.push_back({ IO_STREAM_RUNNING_EVENT_COUNT, acquiredResources });
  }
  return handle;
}

}
