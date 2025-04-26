// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "profile_object.h"
#include "core/edge/user/shim.h"

static inline
std::string value_or_empty(const char* s)
{
  return s == nullptr ? "" : s;
}

namespace zynqaie {

profile_object::
profile_object(ZYNQ::shim* shim, std::shared_ptr<aie_array> array)
  : m_shim{shim},
    m_aie_array{std::move(array)},
    m_profile_id{invalid_profile_id}
{}

int
profile_object::
start(int option, const char* port1Name, const char* port2Name, uint32_t value)
{
  auto device = xrt_core::get_userpf_device(m_shim);

  if (!m_aie_array->is_context_set()) {
    m_aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }
  m_profile_id = m_aie_array->start_profiling(option, value_or_empty(port1Name), value_or_empty(port2Name), value);
  return m_profile_id;
}

uint64_t
profile_object::
read()
{
  if (m_profile_id == invalid_profile_id)
    return 0; // dont throw any exception/error 
 
  auto device = xrt_core::get_userpf_device(m_shim);

  if (!m_aie_array->is_context_set()) {
    m_aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }
  return m_aie_array->read_profiling(m_profile_id);
}

void
profile_object::
stop()
{
  if (m_profile_id == invalid_profile_id)
    return; // dont throw any exception/error 

  auto device = xrt_core::get_userpf_device(m_shim);

  if (!m_aie_array->is_context_set()) {
    m_aie_array->open_context(device.get(), xrt::aie::access_mode::primary);
  }
  m_aie_array->stop_profiling(m_profile_id);
  m_profile_id = invalid_profile_id;
}

} //namespace zynqaie
