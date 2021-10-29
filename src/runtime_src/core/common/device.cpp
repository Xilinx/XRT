/**
 * Copyright (C) 2019-2021 Xilinx, Inc
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
#define XCL_DRIVER_DLL_EXPORT  // in same dll as exported xrt apis
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "device.h"
#include "config_reader.h"
#include "debug.h"
#include "error.h"
#include "query_requests.h"
#include "utils.h"
#include "xclbin_parser.h"
#include "xclbin_swemu.h"

#include "core/include/ert.h"
#include "core/include/xrt.h"
#include "core/include/xclbin.h"
#include "core/include/xrt/xrt_uuid.h"
#include "core/include/experimental/xrt_xclbin.h"

#include "core/common/api/xclbin_int.h"

#include <boost/format.hpp>
#include <functional>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#pragma warning ( disable : 4996 )
#endif

namespace xrt_core {

device::
device(id_type device_id)
  : m_device_id(device_id)
{
  XRT_DEBUGF("xrt_core::device::device(0x%x) idx(%d)\n", this, device_id);
}

device::
~device()
{
  // virtual must be declared and defined
  XRT_DEBUGF("xrt_core::device::~device(0x%x) idx(%d)\n", this, m_device_id);
}

bool
device::
is_nodma() const
{
  if (m_nodma != boost::none)
    return *m_nodma;

  try {
    auto nodma = xrt_core::device_query<xrt_core::query::nodma>(this);
    m_nodma = xrt_core::query::nodma::to_bool(nodma);
  }
  catch (const std::exception&) {
    m_nodma = false;
  }

  return *m_nodma;
}

uuid
device::
get_xclbin_uuid() const
{
  try {
    auto uuid_str =  device_query<query::xclbin_uuid>(this);
    return uuid(uuid_str);
  }
  catch (const query::no_such_key&) {
  }

  // Emulation mode likely, just return m_xclbin_uuid which reflects
  // the uuid of the xclbin loaded by this process.
  return m_xclbin ? m_xclbin.get_uuid() : uuid{};
}

// Unforunately there are two independent entry points into loading an
// xclbin.  One is this function via xrt::device::load_xclbin(), the
// other is xclLoadXclBin(). The two entrypoints converge in
// register_axlf() upon successful xclbin loading. It is possible for
// register_axlf() to be called without the call originating from this
// function, so special managing of m_xclbin data member is required.
void
device::
load_xclbin(const xrt::xclbin& xclbin)
{
  try {
    m_xclbin = xclbin;
    load_axlf(xclbin.get_axlf());
  }
  catch (const std::exception&) {
    m_xclbin = {};
    throw;
  }
}

void
device::
load_xclbin(const uuid& xclbin_id)
{
  auto uuid_loaded = get_xclbin_uuid();
  if (uuid_compare(uuid_loaded.get(), xclbin_id.get()))
    throw error(ENODEV, "specified xclbin is not loaded");

//#ifdef XRT_ENABLE_AIE
  auto xclbin_full = xrt_core::device_query<xrt_core::query::xclbin_full>(this);
  if (xclbin_full.empty())
    throw error(ENODEV, "no cached xclbin data");

  const axlf* top = reinterpret_cast<axlf *>(xclbin_full.data());
  try {
    // set before register_axlf is called via load_axlf_meta
    m_xclbin = xrt::xclbin{top};
    load_axlf_meta(m_xclbin.get_axlf());
  }
  catch (const std::exception&) {
    m_xclbin = {};
    throw;
  }
//#else
  //throw error(ENOTSUP, "load xclbin by uuid is not supported");
//#endif
}

xrt::xclbin
device::
get_xclbin(const uuid& xclbin_id) const
{
  if (xclbin_id && xclbin_id != m_xclbin.get_uuid())
    throw error(EINVAL, "xclbin id mismatch");

  return m_xclbin;
}

// This function is called after an axlf has been succesfully loaded
// by the shim layer API xclLoadXclBin().  Since xclLoadXclBin() can
// be called explicitly by end-user code, the callback is necessary in
// order to register current axlf as the xrt::xclbin data member.
void
device::
register_axlf(const axlf* top)
{
  if (!m_xclbin || m_xclbin.get_uuid() != uuid(top->m_header.uuid))
      m_xclbin = xrt::xclbin{top};

  // Compute CU sort order, kernel driver zocl and xocl now assign and
  // control the sort order, which is accessible via a query request.
  // For emulation old xclbin_parser::get_cus is used.
  m_cus.clear();
  try {
    auto cudata = xrt_core::device_query<xrt_core::query::kds_cu_stat>(this);
    std::sort(cudata.begin(), cudata.end(), [](const auto& d1, const auto& d2) { return d1.index < d2.index; });
    std::transform(cudata.begin(), cudata.end(), std::back_inserter(m_cus), [](const auto& d) { return d.base_addr; });
  }
  catch (const query::no_such_key&) {
    m_cus = xclbin::get_cus(get_axlf_section<const ::ip_layout*>(IP_LAYOUT));
  }
}

std::pair<const char*, size_t>
device::
get_axlf_section(axlf_section_kind section, const uuid& xclbin_id) const
{
  if (xclbin_id && xclbin_id != m_xclbin.get_uuid())
    throw error(EINVAL, "xclbin id mismatch");

  if (!m_xclbin)
    return {nullptr, 0};

  return xrt_core::xclbin_int::get_axlf_section(m_xclbin, section);
}

std::pair<const char*, size_t>
device::
get_axlf_section_or_error(axlf_section_kind section, const uuid& xclbin_id) const
{
  auto ret = get_axlf_section(section, xclbin_id);
  if (ret.first != nullptr)
    return ret;
  throw error(EINVAL, "no such xclbin section");
}

std::vector<std::pair<const char*, size_t>>
device::
get_axlf_sections(axlf_section_kind section, const uuid& xclbin_id) const
{

  if (xclbin_id && (xclbin_id != m_xclbin.get_uuid()))
    throw error(EINVAL, "xclbin id mismatch");

  if (!m_xclbin)
    return std::vector<std::pair<const char*, size_t>>();

  return xrt_core::xclbin_int::get_axlf_sections(m_xclbin, section);
}

std::vector<std::pair<const char*, size_t>>
device::
get_axlf_sections_or_error(axlf_section_kind section, const uuid& xclbin_id) const
{
  auto ret = get_axlf_sections(section, xclbin_id);
  if (!ret.empty())
    return ret;
  throw error(EINVAL, "no such xclbin section");
}

device::memory_type
device::
get_memory_type(size_t memidx) const
{
  auto mem_topology = get_axlf_section<const ::mem_topology*>(ASK_GROUP_TOPOLOGY);
  if (!mem_topology || mem_topology->m_count < static_cast<int32_t>(memidx))
    throw error(EINVAL, "invalid memory bank index");

  const auto& mem = mem_topology->m_mem_data[memidx];
  auto mtype = static_cast<memory_type>(mem.m_type);
  if (mtype != memory_type::dram)
    return mtype;
  return (strncmp(reinterpret_cast<const char*>(mem.m_tag), "HOST[0]", 7) == 0)
    ? memory_type::host
    : mtype;
}

const std::vector<uint64_t>&
device::
get_cus(const uuid& xclbin_id) const
{
  if (xclbin_id && xclbin_id != m_xclbin.get_uuid())
    throw error(EINVAL, "xclbin id mismatch");

  return m_cus;
}

std::pair<size_t, size_t>
device::
get_ert_slots(const char* xml_data, size_t xml_size) const
{
  const size_t max_slots = 128;  // TODO: get from device driver
  const size_t min_slots = 16;   // TODO: get from device driver
  size_t cq_size = ERT_CQ_SIZE;  // TODO: get from device driver

  // xrt.ini overrides all (defaults to 0)
  if (auto size = config::get_ert_slotsize()) {
    // 128 slots max (4 status registers)
    if ((cq_size / size) > max_slots)
      throw error(EINVAL, "invalid slot size '" + std::to_string(size) + "' in xrt.ini");
    return std::make_pair(cq_size/size, size);
  }

  // Determine number of slots needed, bounded by
  //  - minimum 2 concurrently scheduled CUs, plus 1 reserved slot
  //  - minimum min_slots
  //  - maximum max_slots
  auto num_cus = xrt_core::xclbin::get_cus(xml_data, xml_size).size();
  auto slots = std::min(max_slots, std::max(min_slots, (num_cus * 2) + 1));

  // Required slot size bounded by max of
  //  - number of slots needed
  //  - max cu_size per xclbin
  auto size = std::max(cq_size / slots, xrt_core::xclbin::get_max_cu_size(xml_data, xml_size));
  slots = cq_size / size;

  // Round desired slots to minimum 32, 64, 96, 128 (status register boundary)
  if (slots > 16) {
    auto idx = ((slots - 1) / 32); // 32 bit status register idx to handle slots
    slots = (idx + 1) * 32;        // round up
  }

  return std::make_pair(slots, cq_size/slots);
}

std::pair<size_t, size_t>
device::
get_ert_slots() const
{
  auto xml =  get_axlf_section(EMBEDDED_METADATA);
  if (!xml.first)
    throw error(EINVAL, "No xml metadata in xclbin");
  return get_ert_slots(xml.first, xml.second);
}

} // xrt_core
