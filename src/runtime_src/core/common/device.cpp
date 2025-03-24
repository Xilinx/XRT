// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc.  All rights reserved.
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#define XCL_DRIVER_DLL_EXPORT  // in same dll as exported xrt apis
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#define XRT_API_SOURCE         // in same dll as coreutil
#include "device.h"
#include "config_reader.h"
#include "debug.h"
#include "error.h"
#include "query_requests.h"
#include "utils.h"
#include "xclbin_parser.h"
#include "xclbin_swemu.h"

#include "core/include/xrt/detail/ert.h"
#include "core/include/xrt/detail/xclbin.h"
#include "core/include/xrt/deprecated/xrt.h"
#include "core/include/xrt/xrt_uuid.h"
#include "core/include/xrt/experimental/xrt_xclbin.h"

#include "core/common/api/hw_queue.h"
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
  hw_queue::finish(this);
}

bool
device::
is_nodma() const
{
  std::lock_guard lk(m_mutex);
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

bool
device::
get_ex_error_support() const
{
  std::lock_guard lk(m_mutex);
  if (m_ex_error_support != std::nullopt)
    return *m_ex_error_support;

  try {
    auto ex_error_support = xrt_core::device_query<xrt_core::query::xocl_errors_ex>(this);
    m_ex_error_support = xrt_core::query::xocl_errors_ex::to_bool(ex_error_support);
  }
  catch (const std::exception&) {
    m_ex_error_support = false;
  }

  return *m_ex_error_support;
}

uuid
device::
get_xclbin_uuid() const
{
  // This function assumes only one xclbin loaded into the
  // default slot 0.
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

// Registering an xclbin has one entry point (this one) only.
// Shim level registering is not exposed to end-user application.
// Naming of "record" as in record_xclbin is to compensate for
// virtual register_xclbin which is defined by shim.
void
device::
record_xclbin(const xrt::xclbin& xclbin)
{
  try {
    register_xclbin(xclbin); // shim level registration
  }
  catch (const not_supported_error&) {
    // Shim does not support register xclbin, meaning it
    // doesn't support multi-xclbin, so just take the
    // load_xclbin flow.
    load_xclbin(xclbin);
    return;
  }

  std::lock_guard lk(m_mutex);
  m_xclbins.insert(xclbin);

  // For single xclbin case, where shim doesn't implement
  // kds_cu_info, we need the current xclbin stored here
  // as a temporary 'global'.  This variable is used when
  // update_cu_info() is called and query:kds_cu_info is not
  // implemented
  m_xclbin = xclbin;
}

// Unfortunately there are two independent entry points to load an
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
}

xrt::xclbin
device::
get_xclbin(const uuid& xclbin_id) const
{
  std::lock_guard lk(m_mutex);

  // Allow access to xclbin in process of loading via device::load_xclbin
  if (xclbin_id && xclbin_id == m_xclbin.get_uuid())
    return m_xclbin;

  if (xclbin_id)
    return m_xclbins.get(xclbin_id);

  // Single xclbin case
  return m_xclbin;
}

// Update cached xclbin data based on data queried from driver. This
// function can be called by multiple threads. One entry point is
// via register_axlf, another is through open_context.  For the latter,
// opening a CU context on the device can update the xclbin cached data,
// when/if driver determines that the requested xclbin cannot be
// shared and must be loaded into a new slot.
void
device::
update_xclbin_info()
{
  // Update cached slot xclbin uuid mapping
  std::lock_guard lk(m_mutex);
  try {
    // [slot, xclbin_uuid]+
    auto xclbin_slot_info = xrt_core::device_query<xrt_core::query::xclbin_slots>(this);
    m_xclbins.reset(xrt_core::query::xclbin_slots::to_map(xclbin_slot_info));
  }
  catch (const query::no_such_key&) {
    // device does not support multiple xclbins, assume slot 0
    // for current xclbin
    m_xclbins.reset(std::map<slot_id, xrt::uuid>{{0, get_xclbin_uuid()}});
  }
}

// Compute CU sort order, kernel driver zocl and xocl now assign and
// control the sort order, which is accessible via a query request.
// For emulation old xclbin_parser::get_cus is used.
void
device::
update_cu_info()
{
  try {
    // Lock is scoped to try block to ensure lock is released before
    // reaching outer catch block, where get_axlf_section() cannot be
    // called with lock held as it in turn calls get_xclbin(uuid).
    std::lock_guard lk(m_mutex);
    m_cus.clear();
    m_cu2idx.clear();
    m_scu2idx.clear();
    // Regular CUs
    auto cudata = xrt_core::device_query<xrt_core::query::kds_cu_info>(this);

    // TODO: m_cus is legacy, need to fix for multiple slots
    std::sort(cudata.begin(), cudata.end(), [](const auto& d1, const auto& d2) { return d1.index < d2.index; });
    std::transform(cudata.begin(), cudata.end(), std::back_inserter(m_cus), [](const auto& d) { return d.base_addr; });

    for (auto& d : cudata) {
      auto& cu2idx = m_cu2idx[d.slot_index];
      cu2idx.emplace(std::move(d.name), cuidx_type{d.index});
    }

    // Soft kernels, not an error if query doesn't exist (edge)
    try {
      auto scudata = xrt_core::device_query<xrt_core::query::kds_scu_info>(this);
      for (auto& d : scudata) {
        auto& cu2idx = m_scu2idx[d.slot_index];
        cu2idx.emplace(std::move(d.name), cuidx_type{d.index});
      }
    }
    catch (const query::no_such_key&) {
    }
  }
  catch (const query::no_such_key&) {
    // This code path only works for single xclbin case.
    // It assumes that m_xclbin is the single xclbin and that
    // there is only one default slot with number 0.
    auto ip_layout = get_axlf_section<const ::ip_layout*>(IP_LAYOUT);
    if (ip_layout != nullptr) {
      // Aquire lock again before updating m_cus and m_cu2idx
      std::lock_guard lk(m_mutex);
      m_cus = xclbin::get_cus(ip_layout);
      m_cu2idx[0u] = xclbin::get_cu_indices(ip_layout); // default slot 0
    }
  }
}

// This function is called after an axlf has been succesfully loaded
// by the shim layer API xclLoadXclBin().  Since xclLoadXclBin() can
// be called explicitly by end-user code, the callback is necessary in
// order to register current axlf as the xrt::xclbin data member.
void
device::
register_axlf(const axlf* top)
{
  xrt::uuid xid{top->m_header.uuid};

  // Update xclbin caching from [slot, xclbin_uuid]+ data
  update_xclbin_info();

  // Update cu caching from [slot, uuid, cuidx]+ data
  update_cu_info();

  // Do not recreate the xclbin if m_xclbin is set, which implies the
  // xclbin is loaded via xrt::device::load_xclbin where the user
  // application constructed the xclbin.  For all pratical purposes
  // m_xclbin is a temporary 'global' variable to work-around dual
  // entry points for xclbin loading.  However, for backwards
  // compatibility m_xclbin represents the last loaded xclbin and
  // will continue to work for single xclbin use-cases.
  if (!m_xclbin || m_xclbin.get_uuid() != xid)
    m_xclbin = xrt::xclbin{top};

  // Record the xclbin
  std::lock_guard lk(m_mutex);
  m_xclbins.insert(m_xclbin);
}

std::pair<const char*, size_t>
device::
get_axlf_section(axlf_section_kind section, const uuid& xclbin_id) const
{
  auto xclbin = get_xclbin(xclbin_id);

  if (!xclbin)
    return {nullptr, 0};

  return xrt_core::xclbin_int::get_axlf_section(xclbin, section);
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
  auto xclbin = get_xclbin(xclbin_id);

  if (!xclbin)
    return std::vector<std::pair<const char*, size_t>>();

  return xrt_core::xclbin_int::get_axlf_sections(xclbin, section);
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
get_cus() const
{
  // This function returns a reference to internal data
  // that is modified when an xclbin is loaded.  Normally
  // not an issue since only single xclbin is supported
  // when using this API.
  if (m_cu2idx.size() > 1)
    throw error(std::errc::not_supported, "multiple xclbins not supported");

  return m_cus;
}

cuidx_type
device::
get_cuidx(slot_id slot, const std::string& cuname) const
{
  std::lock_guard lk(m_mutex);
  auto slot_itr = m_cu2idx.find(slot);
  if (slot_itr == m_cu2idx.end()) {
    // CU doesn't exists in cu mapping. Now check for scu mapping 
    slot_itr = m_scu2idx.find(slot);
    if (slot_itr == m_scu2idx.end())
      throw error(EINVAL, "No such compute unit '" + cuname + "'");
  }

  const auto& cu2idx = (*slot_itr).second;
  auto cu_itr = cu2idx.find(cuname);
  if (cu_itr == cu2idx.end())
    throw error(EINVAL, "No such compute unit '" + cuname + "'");
  return (*cu_itr).second;
}

cuidx_type
device::
get_cuidx(slot_id slot, const std::string& cuname)
{
  // Leverage const member function
  const auto cdev = static_cast<const device*>(this);
  try {
    return cdev->get_cuidx(slot, cuname);
  }
  catch (const xrt_core::error&) {
    update_cu_info();
    return cdev->get_cuidx(slot, cuname);
  }
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
get_ert_slots(const uuid& xclbin_id) const
{
  auto xml =  get_axlf_section(EMBEDDED_METADATA, xclbin_id);
  if (!xml.first)
    throw error(EINVAL, "No xml metadata in xclbin");
  return get_ert_slots(xml.first, xml.second);
}

} // xrt_core
