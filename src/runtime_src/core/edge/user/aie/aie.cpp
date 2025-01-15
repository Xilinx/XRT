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
#include "core/common/message.h"
#include "core/edge/user/shim.h"
#include "xaiengine/xlnx-ai-engine.h"
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <cerrno>
#include <iostream>

namespace zynqaie {

aie_array::
aie_array(const std::shared_ptr<xrt_core::device>& device)
{
  dev_inst_obj = {0};
  dev_inst = nullptr;
  adf::driver_config driver_config = xrt_core::edge::aie::get_driver_config(device.get());

  XAie_SetupConfig(ConfigPtr,
      driver_config.hw_gen,
      driver_config.base_address,
      driver_config.column_shift,
      driver_config.row_shift,
      driver_config.num_columns,
      driver_config.num_rows,
      driver_config.shim_row,
      driver_config.mem_row_start,
      driver_config.mem_num_rows,
      driver_config.aie_tile_row_start,
      driver_config.aie_tile_num_rows);

  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  /* TODO get partition id and uid from XCLBIN or PDI */
  auto partition_id = xrt_core::edge::aie::full_array_id;
  drm_zocl_aie_fd aiefd = { 0 , partition_id, 0 , 0 };
  int ret = drv->getPartitionFd(aiefd);
  if (ret)
    throw xrt_core::error(ret, "Create AIE failed. Can not get AIE fd");
  fd = aiefd.fd;

  access_mode = drv->getAIEAccessMode();

  ConfigPtr.PartProp.Handle = fd;

  AieRC rc;
  if ((rc = XAie_CfgInitialize(&dev_inst_obj, &ConfigPtr)) != XAIE_OK)
    throw xrt_core::error(-EINVAL, "Failed to initialize AIE configuration: " + std::to_string(rc));
  dev_inst = &dev_inst_obj;

  adf::aiecompiler_options aiecompiler_options = xrt_core::edge::aie::get_aiecompiler_options(device.get());
  m_config = std::make_shared<adf::config_manager>(dev_inst, driver_config.mem_num_rows, aiecompiler_options.broadcast_enable_core);

  fal_util::initialize(dev_inst); //resource manager initialization

  /* Initialize PLIO metadata */
  plio_configs = xrt_core::edge::aie::get_plios(device.get());

  /* Initialize gmio api instances */
  gmio_configs = xrt_core::edge::aie::get_gmios(device.get());
  for (auto config_itr = gmio_configs.begin(); config_itr != gmio_configs.end(); config_itr++)
  {
    auto p_gmio_api = std::make_shared<adf::gmio_api>(&config_itr->second, m_config);
    p_gmio_api->configure();
    gmio_apis[config_itr->first] = p_gmio_api;
  }
  external_buffer_configs = xrt_core::edge::aie::get_external_buffers(device.get());
}

aie_array::
aie_array(const std::shared_ptr<xrt_core::device>& device, const zynqaie::hwctx_object* hwctx_obj)
{
  dev_inst_obj = {0};
  dev_inst = nullptr;
  adf::driver_config driver_config = xrt_core::edge::aie::get_driver_config(device.get(), hwctx_obj);

  XAie_SetupConfig(ConfigPtr,
      driver_config.hw_gen,
      driver_config.base_address,
      driver_config.column_shift,
      driver_config.row_shift,
      driver_config.num_columns,
      driver_config.num_rows,
      driver_config.shim_row,
      driver_config.mem_row_start,
      driver_config.mem_num_rows,
      driver_config.aie_tile_row_start,
      driver_config.aie_tile_num_rows);

  auto part_info = hwctx_obj->get_partition_info();
  if (part_info.partition_id != xrt_core::edge::aie::full_array_id) {
    AieRC rc1;
    if ((rc1 = XAie_SetupPartitionConfig(&dev_inst_obj, part_info.base_address, part_info.start_column, part_info.num_columns)) != XAIE_OK)
      throw xrt_core::error(-EINVAL, "Failed to setup AIE Partition: " + std::to_string(rc1));
  }

  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  auto partition_id = part_info.partition_id;
  auto hw_context_id = hwctx_obj->get_slotidx();
  drm_zocl_aie_fd aiefd = { hw_context_id , partition_id, 0 , 0 };

  //TODO: getparitionFd from driver instead of from shim
  if (auto ret = drv->getPartitionFd(aiefd))
    throw xrt_core::error(ret, "Create AIE failed. Can not get AIE fd");

  fd = aiefd.fd;

  ConfigPtr.PartProp.Handle = fd;

  AieRC rc;
  if ((rc = XAie_CfgInitialize(&dev_inst_obj, &ConfigPtr)) != XAIE_OK)
    throw xrt_core::error(-EINVAL, "Failed to initialize AIE configuration: " + std::to_string(rc));

  dev_inst = &dev_inst_obj;

  adf::aiecompiler_options aiecompiler_options = xrt_core::edge::aie::get_aiecompiler_options(device.get(), hwctx_obj);
  m_config = std::make_shared<adf::config_manager>(dev_inst, driver_config.mem_num_rows, aiecompiler_options.broadcast_enable_core);

  fal_util::initialize(dev_inst); //resource manager initialization
  
  /* Initialize PLIO metadata */
  plio_configs = xrt_core::edge::aie::get_plios(device.get(), hwctx_obj);

  /* Initialize gmio api instances */
  gmio_configs = xrt_core::edge::aie::get_gmios(device.get(), hwctx_obj);
  for (auto config_itr = gmio_configs.begin(); config_itr != gmio_configs.end(); config_itr++)
  {
    auto p_gmio_api = std::make_shared<adf::gmio_api>(&config_itr->second, m_config);
    p_gmio_api->configure();
    gmio_apis[config_itr->first] = p_gmio_api;
  }
  external_buffer_configs = xrt_core::edge::aie::get_external_buffers(device.get(), hwctx_obj);
}

aie_array::
~aie_array()
{
  if (dev_inst)
    XAie_Finish(dev_inst);
}

XAie_DevInst*
aie_array::
get_dev()
{
  if (!dev_inst)
    throw xrt_core::error(-EINVAL, "AIE is not initialized");

  return dev_inst;
}

void
aie_array::
open_context(const xrt_core::device* device, xrt::aie::access_mode am)
{
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  auto current_am = drv->getAIEAccessMode();
  if (current_am != xrt::aie::access_mode::none)
    throw xrt_core::error(-EBUSY, "Can not change current AIE access mode");

  int ret = drv->openAIEContext(am);
  if (ret)
    throw xrt_core::error(ret, "Fail to open AIE context");

  drv->setAIEAccessMode(am);
  access_mode = am;
}

void
aie_array::
open_context(const xrt_core::device* device, const zynqaie::hwctx_object* hwctx_obj, xrt::aie::access_mode am)
{
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  //TODO: replace openAIEContext with new function with parameters hwctx_obj, am
  if (auto ret = drv->openAIEContext(am))
    throw xrt_core::error(ret, "Fail to open AIE context");

  access_mode = am;
}

bool
aie_array::
is_context_set()
{
  return (access_mode != xrt::aie::access_mode::none);
}

void
aie_array::
sync_external_buffer(std::vector<xrt::bo>& bos, adf::external_buffer_config& config, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (config.shim_port_configs.empty())
    return;

  if (bos.size() !=  config.num_bufs)
    throw xrt_core::error(-EINVAL, "Can't sync BO: Required " +
			  std::to_string(config.num_bufs) + " buffers ,but you provided " + std::to_string(bos.size()) + " buffers");

  aie_bd bds[bos.size()];
  size_t counter = 0;
  for (auto& bd: bds) {
    prepare_bd(bd, bos[counter++]);
  }

  adf::dma_api dma_api_obj(m_config);
  for (const auto& port_config : config.shim_port_configs) {
    int start_bd = -1;
    for (const auto& shim_bd_info : port_config.shim_bd_infos) {
      auto buf_idx = shim_bd_info.buf_idx;
      dma_api_obj.updateBDAddressLin(&bds[buf_idx].mem_inst, port_config.shim_column, 0, static_cast<uint16_t>(shim_bd_info.bd_id), shim_bd_info.offset * 4);
      if (start_bd < 0)
        start_bd = shim_bd_info.bd_id;
    }
    dma_api_obj.enqueueTask(1, port_config.shim_column, 0, port_config.direction, port_config.channel_number, port_config.task_repetition, port_config.enable_task_complete_token, static_cast<uint16_t>(start_bd));
  }

  for (auto& bd :bds) {
    clear_bd(bd);
  }
}

void
aie_array::
wait_external_buffer(adf::external_buffer_config& config)
{
  // Dont wait for DMA to be done for the ping-pong buffer cases
  if (config.shim_port_configs.empty() || config.num_bufs == 2)
    return;

  adf::dma_api dma_api_obj(m_config);
  for (auto& port_config : config.shim_port_configs) {
    dma_api_obj.waitDMAChannelDone(1 /*adf::tile_type::shim_tile*/, port_config.shim_column, 0/*shim row*/, port_config.direction, port_config.channel_number);
  }
}

void
aie_array::
sync_bo(std::vector<xrt::bo>& bos, const char *port_name, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (!dev_inst)
    throw xrt_core::error(-EINVAL, "Can't sync BO: AIE is not initialized");

  if (bos.size() == 0)
    throw xrt_core::error(-EINVAL, "Can't sync BO: No global buffer is provided");

  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't sync BO");

  auto ebuf_itr = external_buffer_configs.find(port_name);
  if (ebuf_itr != external_buffer_configs.end()) {
    sync_external_buffer(bos, ebuf_itr->second, dir, size, offset);
    wait_external_buffer(ebuf_itr->second);
    return;
  }

  if (bos.size() > 1)
    throw xrt_core::error(-EINVAL, "Can't sync BO: morethan one buffers are not support for GMIO");

  auto bo = bos[0];
  auto gmio_itr = gmio_apis.find(port_name);
  if (gmio_itr == gmio_apis.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  auto gmio_config_itr = gmio_configs.find(port_name);
  if (gmio_config_itr == gmio_configs.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  submit_sync_bo(bo, gmio_itr->second, gmio_config_itr->second, dir, size, offset);
  gmio_itr->second->wait();
}

void
aie_array::
sync_bo_nb(std::vector<xrt::bo>& bos, const char *port_name, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  if (!dev_inst)
    throw xrt_core::error(-EINVAL, "Can't sync BO: AIE is not initialized");

  if (bos.empty())
    throw xrt_core::error(-EINVAL, "Can't sync BO: No global buffer is provided");

  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't sync BO");

  auto ebuf_itr = external_buffer_configs.find(port_name);
  if (ebuf_itr != external_buffer_configs.end()) {
    sync_external_buffer(bos, ebuf_itr->second, dir, size, offset);
    return;
  }

  if (bos.size() > 1)
    throw xrt_core::error(-EINVAL, "Can't sync BO: morethan one buffers are not support for GMIO");

  auto gmio_itr = gmio_apis.find(port_name);
  if (gmio_itr == gmio_apis.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  auto gmio_config_itr = gmio_configs.find(port_name);
  if (gmio_config_itr == gmio_configs.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  submit_sync_bo(bos[0], gmio_itr->second, gmio_config_itr->second, dir, size, offset);
}

void
aie_array::
wait_gmio(const std::string& port_name)
{
  if (!dev_inst)
    throw xrt_core::error(-EINVAL, "Can't wait GMIO: AIE is not initialized");

  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't wait gmio");

  auto ebuf_itr = external_buffer_configs.find(port_name);
  if (ebuf_itr != external_buffer_configs.end()) {
    wait_external_buffer(ebuf_itr->second);
    return;
  }

  auto gmio_itr = gmio_apis.find(port_name);
  if (gmio_itr == gmio_apis.end())
    throw xrt_core::error(-EINVAL, "Can't sync BO: GMIO name not found");

  gmio_itr->second->wait();
}

void
aie_array::
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
  aie_bd bd;
  prepare_bd(bd, bo);
  gmio_api->enqueueBD(&bd.mem_inst, offset, size);
  clear_bd(bd);
}

void
aie_array::
prepare_bd(aie_bd& bd, xrt::bo& bo)
{
  auto buf_fd = bo.export_buffer();
  if (buf_fd == XRT_NULL_BO_EXPORT)
    throw xrt_core::error(-errno, "Sync AIE Bo: fail to export BO.");
  bd.buf_fd = buf_fd;

  auto bo_size = bo.size();

  XAie_MemCacheProp prop = XAIE_MEM_NONCACHEABLE;
  XAie_MemAttach(dev_inst, &bd.mem_inst, 0, 0, bo_size, prop, buf_fd);
}

void
aie_array::
clear_bd(aie_bd& bd)
{
  XAie_MemDetach(&bd.mem_inst);
  /* we shouldnt close the buffer handle here. file handle gets closed in bo
   * destructor */
  //close(bd.buf_fd);
}

void
aie_array::
reset(const xrt_core::device* device, uint32_t hw_context_id, uint32_t partition_id)
{
  if (!dev_inst)
    throw xrt_core::error(-EINVAL, "Can't Reset AIE: AIE is not initialized");

  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't reset AIE");

  XAie_Finish(dev_inst);
  dev_inst = nullptr;

  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  drm_zocl_aie_reset reset = { hw_context_id , partition_id };
  int ret = drv->resetAIEArray(reset);
  if (ret)
    throw xrt_core::error(ret, "Fail to reset AIE Array");
}

int
aie_array::
start_profiling(int option, const std::string& port1_name, const std::string& port2_name, uint32_t value)
{
  if (!dev_inst)
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
aie_array::
read_profiling(int phdl)
{
  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't do profiling");

  uint64_t value = 0;
  if (event_records.size() > phdl)
    value = adf::profiling::read(get_dev(), event_records[phdl].acquired_resources, event_records[phdl].option == IO_STREAM_START_DIFFERENCE_CYCLES);
  else
    throw xrt_core::error(-EAGAIN, "Read profiling failed: invalid handle.");
  return value;
}

void
aie_array::
stop_profiling(int phdl)
{
  if (access_mode == xrt::aie::access_mode::shared)
    throw xrt_core::error(-EPERM, "Shared AIE context can't do profiling");
  if (event_records.size() > phdl)
    adf::profiling::stop(get_dev(), event_records[phdl].acquired_resources);
  else
    throw xrt_core::error(-EINVAL, "Stop profiling failed: invalid handle.");
}

adf::shim_config
aie_array::
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
aie_array::
start_profiling_run_idle(const std::string& port_name)
{
  int handle = -1;
  std::vector<std::shared_ptr<xaiefal::XAieRsc>> acquired_resources;
  if (adf::profiling::profile_stream_running_to_idle_cycles(get_dev(), get_shim_config(port_name), acquired_resources) == adf::err_code::ok)
  {
    handle = event_records.size();
    event_records.push_back({ IO_TOTAL_STREAM_RUNNING_TO_IDLE_CYCLE, acquired_resources });
  }
  return handle;
}

int
aie_array::
start_profiling_start_bytes(const std::string& port_name, uint32_t value)
{
  int handle = -1;
  std::vector<std::shared_ptr<xaiefal::XAieRsc>> acquired_resources;
  if (adf::profiling::profile_stream_start_to_transfer_complete_cycles(get_dev(), get_shim_config(port_name), value, acquired_resources) == adf::err_code::ok)
  {
    handle = event_records.size();
    event_records.push_back({ IO_STREAM_START_TO_BYTES_TRANSFERRED_CYCLES, acquired_resources });
  }
  return handle;
}

int
aie_array::
start_profiling_diff_cycles(const std::string& port1_name, const std::string& port2_name)
{
  int handle = -1;
  std::vector<std::shared_ptr<xaiefal::XAieRsc>> acquired_resources;
  if (adf::profiling::profile_start_time_difference_btw_two_streams(get_dev(), get_shim_config(port1_name), get_shim_config(port2_name), acquired_resources) == adf::err_code::ok)
  {
    handle = event_records.size();
    event_records.push_back({ IO_STREAM_START_DIFFERENCE_CYCLES, acquired_resources });
  }
  return handle;
}

int
aie_array::
start_profiling_event_count(const std::string& port_name)
{
  int handle = -1;
  std::vector<std::shared_ptr<xaiefal::XAieRsc>> acquired_resources;
  if (adf::profiling::profile_stream_running_event_count(get_dev(), get_shim_config(port_name), acquired_resources) == adf::err_code::ok)
  {
    handle = event_records.size();
    event_records.push_back({ IO_STREAM_RUNNING_EVENT_COUNT, acquired_resources });
  }
  return handle;
}

bool
aie_array::
find_gmio(const std::string& buffer_name)
{
  if (auto gmio_itr = gmio_configs.find(buffer_name) ; gmio_itr == gmio_configs.end())
    return false;

  return true;
}

bool
aie_array::
find_external_buffer(const std::string& buffer_name)
{
  if (auto ebuf_itr = external_buffer_configs.find(buffer_name); ebuf_itr == external_buffer_configs.end())
    return false;

  return true;
}
}
