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

#include "core/edge/user/shim.h"
#include "core/common/message.h"
#ifndef __HWEM__
#include "core/edge/user/plugin/xdp/aie_trace.h"
#include "core/edge/user/plugin/xdp/aie_profile.h"
#endif
#include "core/common/error.h"

#include <cstring>
#include <map>
#include <iostream>
#include <chrono>
#include <cerrno>

extern "C"
{
#include <xaiengine.h>
}

typedef xclDeviceHandle xrtDeviceHandle;

static inline
std::string value_or_empty(const char* s)
{
  return s == nullptr ? "" : s;
}

namespace api {

void
xclAIEOpenContext(xclDeviceHandle handle, xrt::aie::access_mode am)
{
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  int ret = drv->openAIEContext(am);
  if (ret)
    throw xrt_core::error(ret, "Fail to open AIE context");

  drv->setAIEAccessMode(am);
}

void
xclSyncBOAIE(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");
  auto aieArray = drv->getAieArray();

  if (!aieArray->is_context_set()) {
    aieArray->open_context(device.get(), xrt::aie::access_mode::primary);
  }

  auto bosize = bo.size();

  if (offset + size > bosize)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: exceed BO boundary.");

  aieArray->sync_bo(bo, gmioName, dir, size, offset);
}

void
xclSyncBOAIENB(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");
  auto aieArray = drv->getAieArray();

  if (!aieArray->is_context_set()) {
    aieArray->open_context(device.get(), xrt::aie::access_mode::primary);
  }

  auto bosize = bo.size();

  if (offset + size > bosize)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: exceed BO boundary.");

  aieArray->sync_bo_nb(bo, gmioName, dir, size, offset);
}

void
xclGMIOWait(xclDeviceHandle handle, const char *gmioName)
{
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");
  auto aieArray = drv->getAieArray();

  if (!aieArray->is_context_set()) {
    aieArray->open_context(device.get(), xrt::aie::access_mode::primary);
  }

  aieArray->wait_gmio(gmioName);
}

void
xclResetAieArray(xclDeviceHandle handle)
{
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");
  auto aieArray = drv->getAieArray();

  if (!aieArray->is_context_set()) {
    aieArray->open_context(device.get(), xrt::aie::access_mode::primary);
  }
  aieArray->reset(device.get());
}

int
xclStartProfiling(xclDeviceHandle handle, int option, const char* port1Name, const char* port2Name, uint32_t value)
{
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");

  auto aieArray = drv->getAieArray();

  if (!aieArray->is_context_set()) {
    aieArray->open_context(device.get(), xrt::aie::access_mode::primary);
  }

  return aieArray->start_profiling(option, value_or_empty(port1Name), value_or_empty(port2Name), value);
}

uint64_t
xclReadProfiling(xclDeviceHandle handle, int phdl)
{
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");

  auto aieArray = drv->getAieArray();

  if (!aieArray->is_context_set()) {
    aieArray->open_context(device.get(), xrt::aie::access_mode::primary);
  }

  return aieArray->read_profiling(phdl);
}

void
xclStopProfiling(xclDeviceHandle handle, int phdl)
{
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  if (!drv->isAieRegistered())
    throw xrt_core::error(-EINVAL, "No AIE presented");

  auto aieArray = drv->getAieArray();

  if (!aieArray->is_context_set()) {
    aieArray->open_context(device.get(), xrt::aie::access_mode::primary);
  }

  return aieArray->stop_profiling(phdl);
}

} // api


////////////////////////////////////////////////////////////////
// Shim level Graph API implementations (xcl_graph.h)
////////////////////////////////////////////////////////////////
int
xclAIEOpenContext(xclDeviceHandle handle, xrt::aie::access_mode am)
{
  try {
    api::xclAIEOpenContext(handle, am);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}

int
xclSyncBOAIE(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    api::xclSyncBOAIE(handle, bo, gmioName, dir, size, offset);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}

int
xclResetAIEArray(xclDeviceHandle handle)
{
  try {
    api::xclResetAieArray(handle);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}

////////////////////////////////////////////////////////////////
// Exposed for Vitis aietools as extensions to xrt_aie.h
////////////////////////////////////////////////////////////////
/**
 * xclSyncBOAIENB() - Transfer data between DDR and Shim DMA channel
 *
 * @handle:          Handle to the device
 * @bohdl:           BO handle.
 * @gmioName:        GMIO port name
 * @dir:             GM to AIE or AIE to GM
 * @size:            Size of data to synchronize
 * @offset:          Offset within the BO
 *
 * Return:          0 on success, -1 on error.
 *
 * Synchronize the buffer contents between GMIO and AIE.
 * Note: Upon return, the synchronization is submitted or error out
 */
int
xclSyncBOAIENB(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, size_t size, size_t offset)
{
  try {
    api::xclSyncBOAIENB(handle, bo, gmioName, dir, size, offset);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}

/**
 * xclGMIOWait() - Wait a shim DMA channel to be idle for a given GMIO port
 *
 * @handle:          Handle to the device
 * @gmioName:        GMIO port name
 *
 * Return:          0 on success, -1 on error.
 */
int
xclGMIOWait(xclDeviceHandle handle, const char *gmioName)
{
  try {
    api::xclGMIOWait(handle, gmioName);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}

int
xclStartProfiling(xclDeviceHandle handle, int option, const char* port1Name, const char* port2Name, uint32_t value)
{
  try {
    return api::xclStartProfiling(handle, option, port1Name, port2Name, value);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}

uint64_t
xclReadProfiling(xclDeviceHandle handle, int phdl)
{
  try {
    return api::xclReadProfiling(handle, phdl);
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return std::numeric_limits<uint64_t>::max();
}

int
xclStopProfiling(xclDeviceHandle handle, int phdl)
{
  try {
    api::xclStopProfiling(handle, phdl);
    return 0;
  }
  catch (const xrt_core::error& ex) {
    xrt_core::send_exception_message(ex.what());
    errno = ex.get();
  }
  catch (const std::exception& ex) {
    xrt_core::send_exception_message(ex.what());
  }
  return -1;
}
