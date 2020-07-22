/**
 * Copyright (C) 2019 Xilinx, Inc
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

#define XRT_CORE_COMMON_SOURCE
#include "device.h"
#include "error.h"
#include "utils.h"
#include "debug.h"
#include "query_requests.h"
#include "xclbin_parser.h"
#include "core/include/xrt.h"
#include "core/include/xclbin.h"
#include "core/include/ert.h"
#include <boost/format.hpp>
#include <string>
#include <iostream>
#include <fstream>

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

  // Emulation mode likely
  return uuid();
}

void
device::
register_axlf(const axlf* top)
{
  m_axlf_sections.clear();
  m_xclbin_uuid = uuid(top->m_header.uuid);
  axlf_section_kind kinds[] = {EMBEDDED_METADATA, AIE_METADATA, IP_LAYOUT, CONNECTIVITY, 
			ASK_GROUP_CONNECTIVITY, ASK_GROUP_TOPOLOGY, MEM_TOPOLOGY, DEBUG_IP_LAYOUT, SYSTEM_METADATA};
  for (auto kind : kinds) {
    auto hdr = ::xclbin::get_axlf_section(top, kind);
    if (!hdr) {
      /* If group topology or group connectivity doesn't exists then use the
       * mem topology or connectivity respectively section for the same.
       */
      if (kind == ASK_GROUP_TOPOLOGY)
        hdr = ::xclbin::get_axlf_section(top, MEM_TOPOLOGY);
      else if (kind == ASK_GROUP_CONNECTIVITY)
        hdr = ::xclbin::get_axlf_section(top, CONNECTIVITY);

      if (!hdr)
        continue;
    }
    auto section_data = reinterpret_cast<const char*>(top) + hdr->m_sectionOffset;
    std::vector<char> data{section_data, section_data + hdr->m_sectionSize};
    m_axlf_sections.emplace(kind , std::move(data));
  }

  // Build modified CONNECTIVITY and MEM_TOPOLOGY section based on memory group ids
  // Base groups off data from driver
}

std::pair<const char*, size_t>
device::
get_axlf_section(axlf_section_kind section, const uuid& xclbin_id) const
{
  if (xclbin_id && xclbin_id != m_xclbin_uuid)
    throw std::runtime_error("xclbin id mismatch");
  auto itr = m_axlf_sections.find(section);
  return itr != m_axlf_sections.end()
    ? std::make_pair((*itr).second.data(), (*itr).second.size())
    : std::make_pair(nullptr, 0);
}

std::pair<const char*, size_t>
device::
get_axlf_section_or_error(axlf_section_kind section, const uuid& xclbin_id) const
{
  auto ret = get_axlf_section(section, xclbin_id);
  if (ret.first != nullptr)
    return ret;
  throw std::runtime_error("no such xclbin section");
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
      throw std::runtime_error("invalid slot size '" + std::to_string(size) + "' in xrt.ini");
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
    throw std::runtime_error("No xml metadata in xclbin");
  return get_ert_slots(xml.first, xml.second);
}

} // xrt_core
