// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

#include "core/edge/user/shim.h"
#include "core/common/error.h"
#include "graph_api.h"

static inline
std::string value_or_empty(const char* s)
{
  return s == nullptr ? "" : s;
}

namespace graph_api {

//may be we call aie array as an argument to these functions instead of hw_ctx, so that we dont have put lot of nul checks and all
void
aie_open_context(xclDeviceHandle handle, xrt::aie::access_mode am)
{
  auto device = xrt_core::get_userpf_device(handle);
  auto drv = ZYNQ::shim::handleCheck(device->get_device_handle());

  int ret = drv->openAIEContext(am);
  if (ret)
    throw xrt_core::error(ret, "Fail to open AIE context");

  drv->setAIEAccessMode(am);
}

void
sync_bo_aie(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, 
                size_t size, size_t offset, zynqaie::Aie* aie_array)
{
  auto device = xrt_core::get_userpf_device(handle);

  if (not aie_array->is_context_set()) {
    aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }

  auto bosize = bo.size();

  if (offset + size > bosize)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: exceed BO boundary.");

  aie_array->sync_bo(bo, gmioName, dir, size, offset);
}

void
sync_bo_aie_nb(xclDeviceHandle handle, xrt::bo& bo, const char *gmioName, enum xclBOSyncDirection dir, 
                   size_t size, size_t offset, zynqaie::Aie* aie_array)
{
  auto device = xrt_core::get_userpf_device(handle);

  if (not aie_array->is_context_set()) {
    aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }

  auto bosize = bo.size();

  if (offset + size > bosize)
    throw xrt_core::error(-EINVAL, "Sync AIE Bo fails: exceed BO boundary.");

  aie_array->sync_bo_nb(bo, gmioName, dir, size, offset);
}

void
reset_aie_array(xclDeviceHandle handle, zynqaie::Aie* aie_array)
{
  auto device = xrt_core::get_userpf_device(handle);

  if (not aie_array->is_context_set()) {
    aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }
  aie_array->reset(device.get());
}

void
gmio_wait(xclDeviceHandle handle, const char *gmioName, zynqaie::Aie* aie_array)
{
  auto device = xrt_core::get_userpf_device(handle);

  if (not aie_array->is_context_set()) {
    aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }

  aie_array->wait_gmio(gmioName);
}

int
start_profiling(xclDeviceHandle handle, int option, const char* port1Name, 
                  const char* port2Name, uint32_t value, zynqaie::Aie* aie_array)
{
  auto device = xrt_core::get_userpf_device(handle);

  if (not aie_array->is_context_set()) {
    aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }
  return aie_array->start_profiling(option, value_or_empty(port1Name), value_or_empty(port2Name), value);
}

uint64_t
read_profiling(xclDeviceHandle handle, int phdl, zynqaie::Aie* aie_array)
{
  auto device = xrt_core::get_userpf_device(handle);

  if (not aie_array->is_context_set()) {
    aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }
  return aie_array->read_profiling(phdl);
}

void
stop_profiling(xclDeviceHandle handle, int phdl, zynqaie::Aie* aie_array)
{
  auto device = xrt_core::get_userpf_device(handle);

  if (not aie_array->is_context_set()) {
    aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }
  return aie_array->stop_profiling(phdl);
}

} // graph_api
