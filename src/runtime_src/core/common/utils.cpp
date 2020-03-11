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
bdf2index(const std::string& bdfstr)
{
  auto n = std::count(bdfstr.begin(), bdfstr.end(), ':');

  std::stringstream s(bdfstr);
  uint16_t dom = 0, b = 0, d = 0, f = 0;
  char dummy;

  if (n == 2)
    s >> std::hex >> dom >> dummy;
  s >> std::hex >> b >> dummy >> d >> dummy >> f;

  if ((n != 1 && n != 2) || s.fail())
    throw std::runtime_error("Bad BDF string '" + bdfstr + "'");

  auto devices = xrt_core::get_total_devices(false).first;
  for (uint16_t i = 0; i < devices; i++) {
    auto device = get_mgmtpf_device(i);
    auto bdf = device_query<query::pcie_bdf>(device);
    if (b == std::get<0>(bdf) && d == std::get<1>(bdf) && f == std::get<2>(bdf))
      return i;
  }

  throw std::runtime_error("No mgmt PF found for '" + bdfstr + "'");
}

}} // utils, xrt_core
