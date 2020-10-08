/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author: Hem C Neema
 * Simple command line utility to inetract with SDX PCIe devices
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
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "utils.h"
#include "system.h"
#include "device.h"
#include "query_requests.h"
#include <string>
#include <atomic>
#include <cstdint>
#include <limits>
#include <boost/algorithm/string.hpp>

namespace {

inline unsigned int
bit(unsigned int lsh)
{
  return (0x1 << lsh);
}

}

namespace xrt_core { namespace utils {

std::string
parse_cu_status(unsigned int val)
{
  char delim = '(';
  std::string status;
  if (val & 0x1) {
    status += delim;
    status += "START";
    delim = '|';
  }
  if (val & 0x2) {
    status += delim;
    status += "DONE";
    delim = '|';
  }
  if (val & 0x4) {
    status += delim;
    status += "IDLE";
    delim = '|';
  }
  if (val & 0x8) {
    status += delim;
    status += "READY";
    delim = '|';
  }
  if (val & 0x10) {
    status += delim;
    status += "RESTART";
    delim = '|';
  }
  if (status.size())
    status += ')';
  else if (val == 0x0)
    status = "(--)";
  else
    status = "(UNKNOWN)";
  return status;
}

std::string
parse_firewall_status(unsigned int val)
{
  char delim = '(';
  std::string status;
  // Read channel error
  if (val & bit(0)) {
    status += delim;
    status += "READ_RESPONSE_BUSY";
    delim = '|';
  }
  if (val & bit(1)) {
    status += delim;
    status += "RECS_ARREADY_MAX_WAIT";
    delim = '|';
  }
  if (val & bit(2)) {
    status += delim;
    status += "RECS_CONTINUOUS_RTRANSFERS_MAX_WAIT";
    delim = '|';
  }
  if (val & bit(3)) {
    status += delim;
    status += "ERRS_RDATA_NUM";
    delim = '|';
  }
  if (val & bit(4)) {
    status += delim;
    status += "ERRS_RID";
    delim = '|';
  }
  // Write channel error
  if (val & bit(16)) {
    status += delim;
    status += "WRITE_RESPONSE_BUSY";
    delim = '|';
  }
  if (val & bit(17)) {
    status += delim;
    status += "RECS_AWREADY_MAX_WAIT";
    delim = '|';
  }
  if (val & bit(18)) {
    status += delim;
    status += "RECS_WREADY_MAX_WAIT";
    delim = '|';
  }
  if (val & bit(19)) {
    status += delim;
    status += "RECS_WRITE_TO_BVALID_MAX_WAIT";
    delim = '|';
  }
  if (val & bit(20)) {
    status += delim;
    status += "ERRS_BRESP";
    delim = '|';
  }
  if (status.size())
    status += ')';
  else if (val == 0x0)
    status = "(GOOD)";
  else
    status = "(UNKNOWN)";
  return status;
}

std::string
parse_dna_status(unsigned int val)
{
  char delim = '(';
  std::string status;
  if (val & bit(0)) {
    status += delim;
    status += "PASS";
    delim = '|';
  }
  else{
    status += delim;
    status += "FAIL";
    delim = '|';
  }
  if (status.size())
    status += ')';
  else
    status = "(UNKNOWN)";
  return status;
}

std::string
unit_convert(size_t size)
{
  int i = 0, bit_shift=6;
  std::string ret, unit[8]={"Byte", "KB", "MB", "GB", "TB", "PB", "EB", "ZB"};
  if(size < 64)
    return std::to_string(size)+" "+unit[i];
  if(!(size & (size-1)))
    bit_shift = 0;
  while( (size>>bit_shift) !=0 && i<8){
    ret = std::to_string(size);
    size >>= 10;
    i++;
  }
  return ret+" "+unit[i-1];
}

uint16_t
bdf2index(const std::string& bdfstr, bool _inUserDomain)
{
  std::vector<std::string> tokens; 
  boost::split(tokens, bdfstr, boost::is_any_of(":")); 
  int radix = 16;
  uint16_t bus = 0, dev = 0, func = std::numeric_limits<uint16_t>::max();

  //throw an eror if no devices are present
  uint64_t devices = _inUserDomain ? xrt_core::get_total_devices(true).first : xrt_core::get_total_devices(false).first;
  if(devices == 0) 
    throw std::runtime_error("No devices found");
  
  //check if edge, return the first device
  uint16_t d = 0;
  auto device = _inUserDomain ? get_userpf_device(d) : get_mgmtpf_device(d);
  auto bdf = device_query<query::pcie_bdf>(device);
  if(std::get<0>(bdf) == 0 && std::get<1>(bdf) == 0 && std::get<2>(bdf) == 0) {
    return 0;
  }

  // check if we have 2-3 tokens: domain, bus, device.function
  // domain is optional
  if(tokens.size() == 2 || tokens.size() == 3) {
    int tok_pos = tokens.size() == 3 ? 1 : 0; //if domain is specified, skip it for now
    bus = static_cast<uint16_t>(std::stoi(std::string(tokens[tok_pos]), nullptr, radix));
    //check if func was specified. func is optional
    auto pos_of_func = tokens[tok_pos+1].find('.');
    if(pos_of_func != std::string::npos) {
      dev = static_cast<uint16_t>(std::stoi(std::string(tokens[tok_pos+1].substr(0, pos_of_func)), nullptr, radix));
      func = static_cast<uint16_t>(std::stoi(std::string(tokens[tok_pos+1].substr(pos_of_func+1)), nullptr, radix));
    }
    else{
      dev = static_cast<uint16_t>(std::stoi(std::string(tokens[tok_pos+1]), nullptr, radix));
    }
  }
  else {
    throw std::runtime_error(boost::str(boost::format("Invalid BDF '%s'. Please spcify the BDF using 'DDDD:BB:DD.F' format") % bdfstr));
  }

  for (uint16_t i = 0; i < devices; i++) {
    auto device = _inUserDomain ? get_userpf_device(i) : get_mgmtpf_device(i);
    auto bdf = device_query<query::pcie_bdf>(device);

    //if the user specifies func, compare
    //otherwise safely ignore
    auto cmp_func = [bdf](uint16_t func) 
    {
      if (func != std::numeric_limits<uint16_t>::max())
        return func == std::get<2>(bdf);
      return true;
    };

    if (bus == std::get<0>(bdf) && dev == std::get<1>(bdf) && cmp_func(func))
      return i;
  }

  throw std::runtime_error("No user or mgmt PF found for '" + bdfstr + "'");
}

uint64_t
issue_id()
{
  static std::atomic<uint64_t> id {0} ;
  return id++;
}

}} // utils, xrt_core
