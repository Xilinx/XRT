// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020-2021, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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

// This file implements XRT APIs as declared in
// core/include/experimental/xrt_aie.h -- end user APIs
// core/include/experimental/xrt_graph.h -- end user APIs
// core/include/xcl_graph.h -- shim level APIs
#include "core/include/experimental/xrt_aie.h"
#include "core/include/experimental/xrt_bo.h"
#include "core/include/xcl_graph.h"

#include "core/include/experimental/xrt_device.h"
#include "core/common/api/device_int.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/message.h"

namespace xrt {

class graph_impl {
private:
  std::shared_ptr<xrt_core::device> device;
  xclGraphHandle handle;

public:
  graph_impl(const std::shared_ptr<xrt_core::device>& dev, xclGraphHandle ghdl)
    : device(dev)
    , handle(ghdl)
  {}

  ~graph_impl()
  {
    device->close_graph(handle);
  }

  xclGraphHandle
  get_handle() const
  {
    return handle;
  }

  void
  reset()
  {
    device->reset_graph(handle);
  }

  uint64_t
  get_timestamp()
  {
    return (device->get_timestamp(handle));
  }

  void
  run(int iterations)
  {
    device->run_graph(handle, iterations);
  }

  int
  wait(int timeout)
  {
    return (device->wait_graph_done(handle, timeout));
  }

  void
  wait(uint64_t cycle)
  {
    device->wait_graph(handle, cycle);
  }

  void
  suspend()
  {
    device->suspend_graph(handle);
  }

  void
  resume()
  {
    device->resume_graph(handle);
  }

  void
  end(uint64_t cycle)
  {
    device->end_graph(handle, cycle);
  }

  void
  update_rtp(const char* port, const char* buffer, size_t size)
  {
    device->update_graph_rtp(handle, port, buffer, size);
  }

  void
  read_rtp(const char* port, char* buffer, size_t size)
  {
    device->read_graph_rtp(handle, port, buffer, size);
  }
};

}

namespace {

// C-API Graph handles are inserted to this map.
// Note: xrtGraphClose must be explicitly called before xclClose.
static std::map<xrtGraphHandle, std::shared_ptr<xrt::graph_impl>> graph_cache;

static std::shared_ptr<xrt::graph_impl>
open_graph(xrtDeviceHandle dhdl, const uuid_t xclbin_uuid, const char* graph_name, xrt::graph::access_mode am)
{
  auto device = xrt_core::device_int::get_core_device(dhdl);
  auto handle = device->open_graph(xclbin_uuid, graph_name, am);
  auto ghdl = std::make_shared<xrt::graph_impl>(device, handle);
  return ghdl;
}

static std::shared_ptr<xrt::graph_impl>
open_graph(xclDeviceHandle dhdl, const xrt::uuid& xclbin_id, const std::string& name, xrt::graph::access_mode am)
{
  auto device = xrt_core::get_userpf_device(dhdl);
  auto handle = device->open_graph(xclbin_id.get(), name.c_str(), am);
  auto ghdl = std::make_shared<xrt::graph_impl>(device, handle);
  return ghdl;
}

static std::shared_ptr<xrt::graph_impl>
get_graph_hdl(xrtGraphHandle graph_handle)
{
  auto itr = graph_cache.find(graph_handle);
  if (itr == graph_cache.end())
    throw xrt_core::error(-EINVAL, "No such graph handle");
  return (*itr).second;
}

static void
close_graph(xrtGraphHandle hdl)
{
  if (graph_cache.erase(hdl) == 0)
    throw std::runtime_error("Unexpected internal error");
}

static void
sync_aie_bo(xrtDeviceHandle dhdl, xrtBufferHandle bohdl, const char *gmio_name, xclBOSyncDirection dir, size_t size, size_t offset)
{
  auto device = xrt_core::device_int::get_core_device(dhdl);
  auto bo = xrt::bo(bohdl);
  device->sync_aie_bo(bo, gmio_name, dir, size, offset);
}

static void
reset_aie(xrtDeviceHandle dhdl)
{
  auto device = xrt_core::device_int::get_core_device(dhdl);
  device->reset_aie();
}

static void
sync_aie_bo_nb(xrtDeviceHandle dhdl, xrtBufferHandle bohdl, const char *gmio_name, xclBOSyncDirection dir, size_t size, size_t offset)
{
  auto device = xrt_core::device_int::get_core_device(dhdl);
  auto bo = xrt::bo(bohdl);
  device->sync_aie_bo_nb(bo, gmio_name, dir, size, offset);
}

static void
wait_gmio(xrtDeviceHandle dhdl, const char *gmio_name)
{
  auto device = xrt_core::device_int::get_core_device(dhdl);
  device->wait_gmio(gmio_name);
}

static int
start_profiling(xrtDeviceHandle dhdl, int option, const char *port1_name, const char *port2_name, uint32_t value)
{
  auto device = xrt_core::device_int::get_core_device(dhdl);
  return device->start_profiling(option, port1_name, port2_name, value);
}

static uint64_t
read_profiling(xrtDeviceHandle dhdl, int phdl)
{
  auto device = xrt_core::device_int::get_core_device(dhdl);
  return device->read_profiling(phdl);
}

static void
stop_profiling(xrtDeviceHandle dhdl, int phdl)
{
  auto device = xrt_core::device_int::get_core_device(dhdl);
  return device->stop_profiling(phdl);
}

inline void
send_exception_message(const char* msg)
{
  xrt_core::message::send(xrt_core::message::severity_level::error, "XRT", msg);
}

}

//////////////////////////////////////////////////////////////
// xrt_graph C++ API implementations (xrt_graph.h)
//////////////////////////////////////////////////////////////
namespace xrt {

graph::
graph(const xrt::device& device, const xrt::uuid& xclbin_id, const std::string& name)
  : handle(open_graph(device, xclbin_id, name, xrt::graph::access_mode::primary))
{}

void
graph::
reset() const
{
  handle->reset();
}

uint64_t
graph::
get_timestamp() const
{
  return (handle->get_timestamp());
}

void
graph::
run(uint32_t iterations)
{
  handle->run(iterations);
}

void
graph::
wait(std::chrono::milliseconds timeout_ms)
{
  if (timeout_ms.count() == 0)
    handle->wait(static_cast<uint64_t>(0));
  else
    handle->wait(static_cast<int>(timeout_ms.count()));
}

void
graph::
wait(uint64_t cycles)
{
  handle->wait(cycles);
}

void
graph::
suspend()
{
  handle->suspend();
}

void
graph::
resume()
{
  handle->resume();
}

void
graph::
end(uint64_t cycles)
{
  handle->end(cycles);
}

void
graph::
update_port(const std::string& port_name, const void* value, size_t bytes)
{
  handle->update_rtp(port_name.c_str(), reinterpret_cast<const char*>(value), bytes);
}

void
graph::
read_port(const std::string& port_name, void* value, size_t bytes)
{
  handle->read_rtp(port_name.c_str(), reinterpret_cast<char *>(value), bytes);
}

} // namespace xrt

////////////////////////////////////////////////////////////////
// xrt_aie API implementations (xrt_aie.h, xrt_graph.h)
////////////////////////////////////////////////////////////////

xrtGraphHandle
xrtGraphOpen(xrtDeviceHandle dev_handle, const uuid_t xclbin_uuid, const char* graph_name)
{
  try {
    auto hdl = open_graph(dev_handle, xclbin_uuid, graph_name, xrt::graph::access_mode::primary);
    graph_cache[hdl.get()] = hdl;
    return hdl.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

xrtGraphHandle
xrtGraphOpenExclusive(xrtDeviceHandle dev_handle, const uuid_t xclbin_uuid, const char* graph_name)
{
  try {
    auto hdl = open_graph(dev_handle, xclbin_uuid, graph_name, xrt::graph::access_mode::exclusive);
    graph_cache[hdl.get()] = hdl;
    return hdl.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

xrtGraphHandle
xrtGraphOpenShared(xrtDeviceHandle dev_handle, const uuid_t xclbin_uuid, const char* graph_name)
{
  try {
    auto hdl = open_graph(dev_handle, xclbin_uuid, graph_name, xrt::graph::access_mode::shared);
    graph_cache[hdl.get()] = hdl;
    return hdl.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
    return XRT_NULL_HANDLE;
  }
}

void
xrtGraphClose(xrtGraphHandle graph_hdl)
{
  try {
    close_graph(graph_hdl);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
  }
}

int
xrtGraphReset(xrtGraphHandle graph_hdl)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    hdl->reset();
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

uint64_t
xrtGraphTimeStamp(xrtGraphHandle graph_hdl)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    return hdl->get_timestamp();
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtGraphRun(xrtGraphHandle graph_hdl, int iterations)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    hdl->run(iterations);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtGraphWaitDone(xrtGraphHandle graph_hdl, int timeoutMilliSec)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    return hdl->wait(timeoutMilliSec);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtGraphWait(xrtGraphHandle graph_hdl, uint64_t cycle)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    hdl->wait(cycle);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtGraphSuspend(xrtGraphHandle graph_hdl)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    hdl->suspend();
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtGraphResume(xrtGraphHandle graph_hdl)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    hdl->resume();
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtGraphEnd(xrtGraphHandle graph_hdl, uint64_t cycle)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    hdl->end(cycle);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtGraphUpdateRTP(xrtGraphHandle graph_hdl, const char* port, const char* buffer, size_t size)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    hdl->update_rtp(port, buffer, size);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtGraphReadRTP(xrtGraphHandle graph_hdl, const char* port, char* buffer, size_t size)
{
  try {
    auto hdl = get_graph_hdl(graph_hdl);
    hdl->read_rtp(port, buffer, size);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtAIESyncBO(xrtDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
   return xrtSyncBOAIE(handle, bohdl, gmioName, dir, size, offset);
}

int
xrtSyncBOAIE(xrtDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    sync_aie_bo(handle, bohdl, gmioName, dir, size, offset);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

int
xrtAIEResetArray(xrtDeviceHandle handle)
{
  return xrtResetAIEArray(handle);
}

int
xrtResetAIEArray(xrtDeviceHandle handle)
{
  try {
    reset_aie(handle);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

////////////////////////////////////////////////////////////////
// Exposed for Cardano as extensions to xrt_aie.h
////////////////////////////////////////////////////////////////
/**
 * xrtSyncBOAIENB() - Transfer data between DDR and Shim DMA channel
 *
 * @handle:          Handle to the device
 * @bohdl:           BO handle.
 * @gmioName:        GMIO port name
 * @dir:             GM to AIE or AIE to GM
 * @size:            Size of data to synchronize
 * @offset:          Offset within the BO
 *
 * Return:          0 on success, or appropriate error number.
 *
 * Synchronize the buffer contents between GMIO and AIE.
 * Note: Upon return, the synchronization is submitted or error out
 */
int
xrtSyncBOAIENB(xrtDeviceHandle handle, xrtBufferHandle bohdl, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    sync_aie_bo_nb(handle, bohdl, gmioName, dir, size, offset);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

/**
 * xrtGMIOWait() - Wait a shim DMA channel to be idle for a given GMIO port
 *
 * @handle:          Handle to the device
 * @gmioName:        GMIO port name
 *
 * Return:          0 on success, or appropriate error number.
 */
int
xrtGMIOWait(xrtDeviceHandle handle, const char *gmioName)
{
  try {
    wait_gmio(handle, gmioName);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

/**
 * xrtAIEStartProfiling() - Start AIE performance profiling
 *
 * @handle:          Handle to the device
 * @option:          Profiling option.
 * @port1Name:       Profiling port 1 name
 * @port2Name:       Profiling port 2 name
 * @value:           The number of bytes to trigger the profiling event
 *
 * Return:         An integer profiling handle on success,
 *                 or appropriate error number.
 *
 * This function configures the performance counters in AI Engine by given
 * port names and value. The port names and value will have different
 * meanings on different options.
 *
 * Note: Currently, the only supported io profiling option is
 *       io_stream_running_event_count (GMIO and PLIO)
 */
int
xrtAIEStartProfiling(xrtDeviceHandle handle, int option, const char *port1Name, const char *port2Name, uint32_t value)
{
  try {
    return start_profiling(handle, option, port1Name, port2Name, value);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

/**
 * xrtAIEReadProfiling() - Read the current performance counter value
 *                         associated with the profiling handle.
 *
 * @handle:          Handle to the device
 * @pHandle:         Profiling handle.
 *
 * Return:         The performance counter value, or appropriate error number.
 */
uint64_t
xrtAIEReadProfiling(xrtDeviceHandle handle, int pHandle)
{
  try {
    return read_profiling(handle, pHandle);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}

/**
 * xrtAIEStopProfiling() - Stop the current performance profiling
 *                         associated with the profiling handle and
 *                         release the corresponding hardware resources.
 *
 * @handle:          Handle to the device
 * @pHandle:         Profiling handle.
 *
 * Return:         0 on success, or appropriate error number.
 */
int
xrtAIEStopProfiling(xrtDeviceHandle handle, int pHandle)
{
  try {
    stop_profiling(handle, pHandle);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    return ex.get();
  }
  catch (const std::exception& ex) {
    send_exception_message(ex.what());
    return -1;
  }
}
