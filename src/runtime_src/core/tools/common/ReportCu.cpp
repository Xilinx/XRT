/**
 * Copyright (C) 2020 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "ReportCu.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include "core/common/utils.h"

namespace qr = xrt_core::query;

enum class cu_stat : unsigned short {
  usage = 0,
  addr,
  stat
};

/*
 * xclbin locking
 */
struct xclbin_lock
{
  xclDeviceHandle m_handle;
  xuid_t m_uuid;

  xclbin_lock(const xrt_core::device *_dev)
    : m_handle(_dev->get_device_handle())
  {
    auto xclbinid = xrt_core::device_query<xrt_core::query::xclbin_uuid>(_dev);

    uuid_parse(xclbinid.c_str(), m_uuid);

    if (uuid_is_null(m_uuid))
      throw std::runtime_error("'uuid' invalid, please re-program xclbin.");

    if (xclOpenContext(m_handle, m_uuid, std::numeric_limits<unsigned int>::max(), true))
      throw std::runtime_error("'Failed to lock down xclbin");
  }

  ~xclbin_lock(){
    xclCloseContext(m_handle, m_uuid, std::numeric_limits<unsigned int>::max());
  }
};

void
schedulerUpdateStat(const xrt_core::device *device)
{
    try {
      xclbin_lock xb_lock(device);
      xrt_core::device_query<qr::scheduler_update_stat>(device);
    }
    catch (const std::exception&) {
      // xclbin_lock failed, safe to ignore
    }
}

uint32_t 
parseComputeUnitStat(const std::vector<std::string>& custat, uint32_t offset, cu_stat kind) 
{
  uint32_t ret = 0;

  for (auto& line : custat) {
    uint32_t ba = 0, cnt = 0, sta = 0;
    std::sscanf(line.c_str(), "CU[@0x%x] : %d status : %d", &ba, &cnt, &sta);

    if (offset != ba)
      continue;

    if (kind == cu_stat::usage)
      ret = cnt;
    else if (kind == cu_stat::stat)
      ret = sta;

    return ret;
  }

  return ret;
}

boost::property_tree::ptree
populate_cus(const xrt_core::device *device, const std::string& desc)
{
  if (!std::getenv("XCL_SKIP_CU_READ"))
    schedulerUpdateStat(device);

  boost::property_tree::ptree pt;
  std::vector<char> ip_buf;
  std::vector<std::string> cu_stats;

  pt.put("description", desc);
  
  try {
    ip_buf = xrt_core::device_query<qr::ip_layout_raw>(device);
    cu_stats = xrt_core::device_query<qr::kds_custat>(device);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
  }

  if(ip_buf.empty() || cu_stats.empty())
    return pt;

  const ip_layout *layout = (ip_layout *)ip_buf.data();
  boost::property_tree::ptree ptCu_array;
  for (int i = 0; i < layout->m_count; i++) {
    if (layout->m_ip_data[i].m_type != IP_KERNEL)
      continue;

    uint32_t status = parseComputeUnitStat(cu_stats, layout->m_ip_data[i].m_base_address, cu_stat::stat);
    uint32_t usage = parseComputeUnitStat(cu_stats, layout->m_ip_data[i].m_base_address, cu_stat::usage);
    boost::property_tree::ptree ptCu;
    ptCu.put( "name",         layout->m_ip_data[i].m_name);
    ptCu.put( "base_address", layout->m_ip_data[i].m_base_address);
    ptCu.put( "usage",        usage);
    ptCu.put( "status",       xrt_core::utils::parse_cu_status(status));
    ptCu_array.push_back(std::make_pair("", ptCu));
  }

  pt.add_child( std::string("board.compute_unit"), ptCu_array);

  return pt;
}

void
ReportCu::getPropertyTreeInternal(const xrt_core::device * _pDevice, 
                                              boost::property_tree::ptree &_pt) const
{
  // Defer to the 20202 format.  If we ever need to update JSON data, 
  // Then update this method to do so.
  getPropertyTree20202(_pDevice, _pt);
}

void 
ReportCu::getPropertyTree20202( const xrt_core::device * _pDevice, 
                                           boost::property_tree::ptree &_pt) const
{
  // There can only be 1 root node
  _pt.add_child("cus", populate_cus(_pDevice, "Compute Units"));
}

void 
ReportCu::writeReport( const xrt_core::device * _pDevice,
                                  const std::vector<std::string> & /*_elementsFilter*/, 
                                  std::iostream & _output) const
{
  boost::property_tree::ptree _pt;
  boost::property_tree::ptree empty_ptree;
  getPropertyTreeInternal(_pDevice, _pt);
 
  _output << boost::format("%s\n") % _pt.get<std::string>("cus.description");
  int index = 0;
  try {
    for (auto& v : _pt.get_child("cus.board.compute_unit")) {
      std::string name, base_addr, usage, status;
      for (auto& subv : v.second) {
        if (subv.first == "name") {
          name = subv.second.get_value<std::string>();
        } else if (subv.first == "base_address") {
          base_addr = subv.second.get_value<std::string>();
        } else if (subv.first == "usage") {
          usage = subv.second.get_value<std::string>();
        } else if (subv.first == "status") {
          status = subv.second.get_value<std::string>();
        }
      }
      _output << boost::format(" [CU - %2d]\n") % index;
      _output << boost::format("   \"name\": %-16s\n") % name;
      _output << boost::format("   \"base_address\": %-16s\n") % base_addr;
      _output << boost::format("   \"usage\": %-16s\n") % usage;
      _output << boost::format("   \"status\": %-16s\n") % status;
      index++;
    }
  }
  catch( std::exception const&) {
      // eat the exception, probably bad path
  }

  _output << std::endl;
}
