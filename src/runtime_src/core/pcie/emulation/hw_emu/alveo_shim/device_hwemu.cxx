// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
#include "device_hwemu.h"
#include "core/common/query_requests.h"
#include "core/common/xrt_profiling.h"
#include "core/pcie/emulation/common_em/query.h"
#include "shim.h"

#include <string>
#include <map>

namespace {

namespace query = xrt_core::query;
using key_type = query::key_type;
using qtype = std::underlying_type<query::key_type>::type;

static std::map<query::key_type, std::unique_ptr<query::request>> query_tbl;

struct device_query
{
  static uint32_t
  get(const xrt_core::device* device, key_type query_key)
  {
    xclhwemhal2::HwEmShim *drv = xclhwemhal2::HwEmShim::handleCheck(device->get_device_handle());
    if (!drv)
      return 0;
    return drv->deviceQuery(query_key);
  }
};

struct debug_ip_layout_path
{
  using result_type = xrt_core::query::debug_ip_layout_path::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    uint32_t size = std::any_cast<uint32_t>(param);
    std::string path;
    path.resize(size);

    // Get Debug Ip layout path
    xclGetDebugIPlayoutPath(device->get_user_handle(), const_cast<char*>(path.data()), size);
    return path;
  }
};

struct device_clock_freq_mhz {
  using result_type = xrt_core::query::device_clock_freq_mhz::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key)
  {
    return xclGetDeviceClockFreqMHz(device->get_user_handle());
  }
};

struct trace_buffer_info
{
  using result_type = xrt_core::query::trace_buffer_info::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    uint32_t input_samples = std::any_cast<uint32_t>(param);
    result_type buf_info;

    // Get trace buf size and trace samples
    xclGetTraceBufferInfo(device->get_user_handle(), input_samples, buf_info.samples, buf_info.buf_size);
    return buf_info;
  }
};

struct host_max_bandwidth_mbps
{
  using result_type = xrt_core::query::host_max_bandwidth_mbps::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    bool read = std::any_cast<bool>(param);

    // Get read/write host max bandwidth in MBps
    return read ? xclGetHostReadMaxBandwidthMBps(device->get_user_handle())
                : xclGetHostWriteMaxBandwidthMBps(device->get_user_handle());
  }
};

struct kernel_max_bandwidth_mbps
{
  using result_type = xrt_core::query::kernel_max_bandwidth_mbps::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    bool read = std::any_cast<bool>(param);

    // Get read/write host max bandwidth in MBps
    return read ? xclGetKernelReadMaxBandwidthMBps(device->get_user_handle())
                : xclGetKernelWriteMaxBandwidthMBps(device->get_user_handle());
  }
};

struct read_trace_data
{
  using result_type = xrt_core::query::read_trace_data::result_type;

  static result_type
  get(const xrt_core::device* device, key_type key, const std::any& param)
  {
    auto args = std::any_cast<xrt_core::query::read_trace_data::args>(param);

    result_type trace_buf;
    trace_buf.resize(args.buf_size);

    // read trace data
    xclReadTraceData(device->get_user_handle(), trace_buf.data(),
                     args.buf_size, args.samples, args.ip_base_addr, args.words_per_sample);
    return trace_buf;
  }
};

template <typename QueryRequestType, typename Getter>
struct function0_get : virtual QueryRequestType
{
  std::any
  get(const xrt_core::device* device) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k);
  }
};

template <typename QueryRequestType, typename Getter>
struct function1_get : virtual QueryRequestType
{
  std::any
  get(const xrt_core::device* device, const std::any& arg1) const
  {
    auto k = QueryRequestType::key;
    return Getter::get(device, k, arg1);
  }
};

template <typename QueryRequestType, typename Getter>
static void
emplace_func0_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function0_get<QueryRequestType, Getter>>());
}

template <typename QueryRequestType, typename Getter>
static void
emplace_func1_request()
{
  auto k = QueryRequestType::key;
  query_tbl.emplace(k, std::make_unique<function1_get<QueryRequestType, Getter>>());
}

static void
initialize_query_table()
{
  emplace_func0_request<query::clock_freqs_mhz, xclemulation::query::device_info>();
  emplace_func0_request<query::kds_numcdmas, xclemulation::query::device_info>();
  emplace_func0_request<query::pcie_bdf, xclemulation::query::device_info>();
  emplace_func0_request<query::m2m, device_query>();
  emplace_func0_request<query::nodma, device_query>();
  emplace_func0_request<query::rom_vbnv, xclemulation::query::device_info>();
  emplace_func1_request<query::debug_ip_layout_path, debug_ip_layout_path>();
  emplace_func0_request<query::device_clock_freq_mhz, device_clock_freq_mhz>();
  emplace_func1_request<query::trace_buffer_info, trace_buffer_info>();
  emplace_func1_request<query::host_max_bandwidth_mbps, host_max_bandwidth_mbps>();
  emplace_func1_request<query::kernel_max_bandwidth_mbps, kernel_max_bandwidth_mbps>();
  emplace_func1_request<query::read_trace_data, read_trace_data>();
}

struct X { X() { initialize_query_table(); } };
static X x;

}

namespace xrt_core { namespace hwemu {

const query::request&
device::
lookup_query(query::key_type query_key) const
{
  auto it = query_tbl.find(query_key);

  if (it == query_tbl.end())
    throw query::no_such_key(query_key);

  return *(it->second);
}

device::
device(handle_type device_handle, id_type device_id, bool user)
  : shim<device_pcie>(device_handle, device_id, user)
{
}

std::unique_ptr<buffer_handle>
device::
import_bo(pid_t pid, shared_handle::export_handle ehdl)
{
  if (pid == 0 || getpid() == pid)
    return xrt::shim_int::import_bo(get_device_handle(), ehdl);

  throw xrt_core::error(std::errc::not_supported, __func__);
}

void
device::
get_device_info(xclDeviceInfo2 *info)
{
  if (auto ret = xclGetDeviceInfo2(get_device_handle(), info))
    throw system_error(ret, "failed to get device info");
}

size_t
device::
get_device_timestamp()
{
  auto ret = xclGetDeviceTimestamp(get_device_handle());
  if (ret <= 0)
    throw system_error(ret, "failed to get device info");
  return ret;
}
}} // hwemu,xrt_core
