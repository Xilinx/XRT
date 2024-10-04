// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#define XRT_API_SOURCE         // in same dll as api
#include "capture.h"
#include "logger.h"

#include <iostream>

namespace xtx = xrt::tools::xbtracer;

// NOLINTNEXTLINE(cert-err58-cpp)
static const xtx::xrt_ftbl& dtbl = xtx::xrt_ftbl::get_instance();

/*
 * HW Context Class instrumented methods
 * */
namespace xrt {

std::vector<char> serialize_cfg_param(const xrt::hw_context::cfg_param_type& cfg_param)
{
  std::vector<char> serialized_cfg_param;
  for (const auto& kv : cfg_param)
  {
    // Serialize the key
    uint32_t key_size = static_cast<uint32_t>(kv.first.size()); // NOLINT (hicpp-use-auto)
    serialized_cfg_param.insert(serialized_cfg_param.end(),
      reinterpret_cast<const char*>(&key_size), // NOLINT (cppcoreguidelines-pro-type-reinterpret-cast)
      reinterpret_cast<const char*>(&key_size) + sizeof(uint32_t)); // NOLINT (cppcoreguidelines-pro-type-reinterpret-cast)
    serialized_cfg_param.insert(serialized_cfg_param.end(),
      kv.first.begin(), kv.first.end());

    // Serialize the value
    uint32_t value = kv.second;
    serialized_cfg_param.insert(serialized_cfg_param.end(),
      reinterpret_cast<const char*>(&value), // NOLINT (cppcoreguidelines-pro-type-reinterpret-cast)
      reinterpret_cast<const char*>(&value) + sizeof(uint32_t)); // NOLINT (cppcoreguidelines-pro-type-reinterpret-cast)
  }
  return serialized_cfg_param;
}

hw_context::hw_context(const xrt::device& device, const xrt::uuid& xclbin_id,
                       const cfg_param_type& cfg_param)
{
  auto func = "xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, const xrt::hw_context::cfg_param_type&)";
  auto serialized_cfg_param = serialize_cfg_param(cfg_param); // Serialize cfg_param
  // Wrap the serialized data in a membuf
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-reinterpret-cast)
  xtx::membuf serialized_cfg_param_buf(reinterpret_cast<unsigned char*>(serialized_cfg_param.data()),
    serialized_cfg_param.size());
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.hw_context.ctor_frm_cfg, this, device,
    xclbin_id, cfg_param);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func, device.get_handle().get(), xclbin_id.to_string().c_str(),
    serialized_cfg_param_buf);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

hw_context::hw_context(const xrt::device& device, const xrt::uuid& xclbin_id,
                       access_mode mode)
{
  auto func = "xrt::hw_context::hw_context(const xrt::device&, const xrt::uuid&, xrt::hw_context::access_mode)";
  XRT_TOOLS_XBT_CALL_CTOR(dtbl.hw_context.ctor_frm_mode, this, device,
    xclbin_id, mode);
  /* As pimpl will be updated only after ctor call*/
  XRT_TOOLS_XBT_FUNC_ENTRY(func,
    device.get_handle().get(), xclbin_id.to_string().c_str(), (int)mode);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}

void hw_context::update_qos(const cfg_param_type& qos)
{
  auto func = "xrt::hw_context::update_qos(const xrt::hw_context::cfg_param_type&)";
  // Serialize the qos parameter
  auto serialized_qos = serialize_cfg_param(qos);
  // Cast char* to unsigned char* for membuf
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-reinterpret-cast)
  xtx::membuf serialized_qos_buf(reinterpret_cast<unsigned char*>(serialized_qos.data()),
    serialized_qos.size());
  XRT_TOOLS_XBT_FUNC_ENTRY(func, serialized_qos_buf);
  XRT_TOOLS_XBT_CALL_METD(dtbl.hw_context.update_qos, qos);
  XRT_TOOLS_XBT_FUNC_EXIT(func);
}
}  // namespace xrt
