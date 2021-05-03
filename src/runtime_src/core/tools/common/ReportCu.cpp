/**
 * Copyright (C) 2021 Xilinx, Inc
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
#include <boost/algorithm/string.hpp>
#include "ReportCu.h"
#include "core/common/query_requests.h"
#include "core/common/device.h"
#include "core/common/utils.h"
#include "ps_kernel.h"

namespace qr = xrt_core::query;

enum class cu_type {
  PL,
  PS
};

static std::string 
enum_to_str(cu_type type) {
  switch(type) {
    case cu_type::PL:
      return "PL";
    case cu_type::PS:
      return "PS";
    default:
      break;
  }
  return "UNLNOWN";
}

boost::property_tree::ptree
get_cu_status(uint32_t cu_status)
{
  boost::property_tree::ptree pt;
  std::vector<std::string> bit_set;

  if (cu_status & 0x1)
    bit_set.push_back("START");
  if (cu_status & 0x2)
    bit_set.push_back("DONE");
  if (cu_status & 0x4)
    bit_set.push_back("IDLE");
  if (cu_status & 0x8)
    bit_set.push_back("READY");
  if (cu_status & 0x10)
    bit_set.push_back("RESTART");

  pt.put("bit_mask",	boost::str(boost::format("0x%x") % cu_status));
  boost::property_tree::ptree ptSt_arr;
  for(auto& str : bit_set)
    ptSt_arr.push_back(std::make_pair("", boost::property_tree::ptree(str)));

  if (!ptSt_arr.empty())
    pt.add_child( std::string("bits_set"), ptSt_arr);

  return pt;
}

static void
schedulerUpdateStat(xrt_core::device *device)
{
  try {
    // lock xclbin
    auto uuid = xrt::uuid(xrt_core::device_query<xrt_core::query::xclbin_uuid>(device));
    device->open_context(uuid.get(), std::numeric_limits<unsigned int>::max(), true);
    auto at_exit = [] (auto device, auto uuid) { device->close_context(uuid.get(), std::numeric_limits<unsigned int>::max()); };
    xrt_core::scope_guard<std::function<void()>> g(std::bind(at_exit, device, uuid));
    
    device->update_scheduler_status();
  }
  catch (const std::exception&) {
    // xclbin_lock failed, safe to ignore
  }
}

boost::property_tree::ptree
populate_cus(const xrt_core::device *device)
{
  schedulerUpdateStat(const_cast<xrt_core::device *>(device));
  boost::property_tree::ptree pt;
  std::vector<char> ip_buf;
  std::vector<std::tuple<uint64_t, uint32_t, uint32_t>> cu_stats; // tuple <base_addr, usage, status>

  try {
    ip_buf = xrt_core::device_query<qr::ip_layout_raw>(device);
    cu_stats = xrt_core::device_query<qr::kds_cu_info>(device);
  } catch (const std::exception& ex){
    pt.put("error_msg", ex.what());
    return pt;
  }

  if(ip_buf.empty() || cu_stats.empty()) {
    return pt;
  }

  const ip_layout *layout = reinterpret_cast<const ip_layout*>(ip_buf.data());
  for (int i = 0; i < layout->m_count; i++) {
    if (layout->m_ip_data[i].m_type != IP_KERNEL)
      continue;

    for(auto& stat : cu_stats) {
      uint64_t base_addr = std::get<0>(stat);
      if (layout->m_ip_data[i].m_base_address == base_addr) {
        uint32_t usage = std::get<1>(stat);
        uint32_t status = std::get<2>(stat);

        boost::property_tree::ptree ptCu;
        ptCu.put( "name",			layout->m_ip_data[i].m_name);
        ptCu.put( "base_address",		boost::str(boost::format("0x%x") % base_addr));
        ptCu.put( "usage",			usage);
        ptCu.add_child( std::string("status"),	get_cu_status(status));
        pt.push_back(std::make_pair("", ptCu));
      }
    }
  }

  return pt;
}

int 
getPSKernels(std::vector<ps_kernel_data> &psKernels, const xrt_core::device *device)
{
  std::vector<char> buf = xrt_core::device_query<xrt_core::query::ps_kernel>(device);
  if (buf.empty())
    return 0;
  const ps_kernel_node *map = reinterpret_cast<ps_kernel_node *>(buf.data());
  if(map->pkn_count < 0)
    return -EINVAL;

  for (unsigned int i = 0; i < map->pkn_count; i++)
    psKernels.emplace_back(map->pkn_data[i]);

  return 0;
}

boost::property_tree::ptree
populate_cus_new(const xrt_core::device *device)
{
  schedulerUpdateStat(const_cast<xrt_core::device *>(device));
  
  boost::property_tree::ptree pt;
  using cu_data_type = qr::kds_cu_stat::data_type;
  using scu_data_type = qr::kds_scu_stat::data_type;
  std::vector<cu_data_type> cu_stats;
  std::vector<scu_data_type> scu_stats;

  try {
    cu_stats  = xrt_core::device_query<qr::kds_cu_stat>(device);
    scu_stats = xrt_core::device_query<qr::kds_scu_stat>(device);
  } catch (const std::exception& ex) {
    pt.put("error_msg", ex.what());
    return pt;
  }

  for (auto& stat : cu_stats) {
    boost::property_tree::ptree ptCu;
    ptCu.put( "name",           stat.name);
    ptCu.put( "base_address",   boost::str(boost::format("0x%x") % stat.base_addr));
    ptCu.put( "usage",          stat.usages);
    ptCu.put( "type", enum_to_str(cu_type::PL));
    ptCu.add_child( std::string("status"),	get_cu_status(stat.status));
    pt.push_back(std::make_pair("", ptCu));
  }

  std::vector<ps_kernel_data> psKernels;
  if (getPSKernels(psKernels, device) < 0) {
    std::cout << "WARNING: 'ps_kernel' invalid. Has the PS kernel been loaded? See 'xbutil program'.\n";
    return pt;
  }

  uint32_t psk_inst = 0;
  uint32_t num_scu = 0;
  boost::property_tree::ptree pscu_list;
  for (auto& stat : scu_stats) {
    boost::property_tree::ptree ptCu;
    std::string scu_name = "Illegal";
    if (psk_inst >= psKernels.size()) {
      scu_name = stat.name; 
      //This means something is wrong
      //scu_name e.g. kernel_vcu_encoder:scu_34
    } else {
      scu_name = psKernels.at(psk_inst).pkd_sym_name;
      scu_name.append("_");
      scu_name.append(std::to_string(num_scu));
      //scu_name e.g. kernel_vcu_encoder_2
    }
    ptCu.put( "name",           scu_name);
    ptCu.put( "base_address",   "0x0");
    ptCu.put( "usage",          stat.usages);
    ptCu.put( "type", enum_to_str(cu_type::PS));
    ptCu.add_child( std::string("status"),	get_cu_status(stat.status));
    pt.push_back(std::make_pair("", ptCu));

    if (psk_inst >= psKernels.size()) {
      continue;
    }
    num_scu++;
    if (num_scu == psKernels.at(psk_inst).pkd_num_instances) {
      //Handled all instances of a PS Kernel, so next is a new PS Kernel
      num_scu = 0;
      psk_inst++;
    }
  }

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
  uint32_t kds_mode;

  // sysfs attribute kds_mode: 1 - new KDS; 0 - old KDS
  try {
    kds_mode = xrt_core::device_query<qr::kds_mode>(_pDevice);
  } catch (...){
    // When kds_mode doesn't present, xocl driver supports old KDS
    kds_mode = 0;
  }

  // There can only be 1 root node
  if (kds_mode == 0) // Old kds
      _pt.add_child("compute_units", populate_cus(_pDevice));
  else // new kds
      _pt.add_child("compute_units", populate_cus_new(_pDevice));
}

void 
ReportCu::writeReport( const xrt_core::device* /*_pDevice*/,
                       const boost::property_tree::ptree& _pt, 
                       const std::vector<std::string>& /*_elementsFilter*/,
                       std::ostream & _output) const
{
  boost::property_tree::ptree empty_ptree;
  boost::format cuFmt("    %-8s%-30s%-16s%-8s%-8s\n");

  //check if a valid CU report is generated
  const boost::property_tree::ptree& pt_cu = _pt.get_child("compute_units", empty_ptree);
  if(pt_cu.empty())
    return;

  _output << "Compute Units" << std::endl;
  _output << "  PL Compute Units" << std::endl;
  _output << cuFmt % "Index" % "Name" % "Base_Address" % "Usage" % "Status";
  try {
    int index = 0;
    for(auto& kv : pt_cu) {
      const boost::property_tree::ptree& cu = kv.second;
      if(cu.get<std::string>("type").compare("PL") != 0)
        continue;
      std::string cu_status = cu.get_child("status").get<std::string>("bit_mask");
      uint32_t status_val = std::stoul(cu_status, nullptr, 16);
      _output << cuFmt % index++ %
	      cu.get<std::string>("name") % cu.get<std::string>("base_address") %
	      cu.get<std::string>("usage") % xrt_core::utils::parse_cu_status(status_val);
    }
  }
  catch( std::exception const& e) {
    _output << "ERROR: " <<  e.what() << std::endl;
  }
  _output << std::endl;

  //PS kernel report
  _output << "  PS Compute Units" << std::endl;
  _output << cuFmt % "Index" % "Name" % "Base_Address" % "Usage" % "Status";
  try {
    int index = 0;
    for(auto& kv : pt_cu) {
      const boost::property_tree::ptree& cu = kv.second;
      if(cu.get<std::string>("type").compare("PS") != 0)
        continue;
      std::string cu_status = cu.get_child("status").get<std::string>("bit_mask");
      uint32_t status_val = std::stoul(cu_status, nullptr, 16);
      _output << cuFmt % index++ %
	      cu.get<std::string>("name") % cu.get<std::string>("base_address") %
	      cu.get<std::string>("usage") % xrt_core::utils::parse_cu_status(status_val);
    }
  }
  catch( std::exception const& e) {
    _output << "ERROR: " <<  e.what() << std::endl;
  }

  _output << std::endl;
}
