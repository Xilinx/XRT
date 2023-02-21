/**
 * Copyright (C) 2016-2022 Xilinx, Inc
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
#include "config_reader.h"
#include "device.h"
#include "query_requests.h"
#include "system.h"
#include "utils.h"
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <boost/algorithm/string.hpp>

#ifdef _WIN32

# pragma warning( disable : 4189 4100 )
# pragma comment(lib, "Ws2_32.lib")
 /* need to link the lib for the following to work */
# define be32toh ntohl
#else
# include <unistd.h> // SUDO check
#endif

#define ALIGN(x, a)     (((x) + ((a) - 1)) & ~((a) - 1))
#define PALIGN(p, a)    ((char *)(ALIGN((unsigned long long)(p), (a))))
#define GET_CELL(p)     (p += 4, *((const uint32_t *)(p-4)))

namespace {

// ------ C O N S T A N T   V A R I A B L E S ---------------------------------
static const uint32_t FDT_BEGIN_NODE = 0x1;
static const uint32_t FDT_PROP = 0x3;
static const uint32_t FDT_END = 0x9;

// ------ L O C A L  F U N C T I O N S  A N D  S T R U C T S ------------------
struct fdt_header {
  uint32_t magic;
  uint32_t totalsize;
  uint32_t off_dt_struct;
  uint32_t off_dt_strings;
  uint32_t off_mem_rsvmap;
  uint32_t version;
  uint32_t last_comp_version;
  uint32_t boot_cpuid_phys;
  uint32_t size_dt_strings;
  uint32_t size_dt_struct;
};

inline unsigned int
bit(unsigned int lsh)
{
  return (0x1 << lsh);
}

static std::string
precision(double value, int p)
{
  std::stringstream stream;
  stream << std::fixed << std::setprecision(p) << value;
  return stream.str();
}

}

namespace xrt_core { namespace utils {

std::string
get_hostname()
{
  boost::property_tree::ptree pt_os_info;
  xrt_core::get_os_info(pt_os_info);
  return pt_os_info.get("hostname", "");
}

std::string
parse_cu_status(unsigned int val)
{
  char delim = '(';
  std::string status;
  if (val == std::numeric_limits<uint32_t>::max()) //Crashed soft kernel status is -1
    status = "(CRASHED)";
  else if (val == 0x0)
    status = "(--)";
  else {
    if (val & CU_AP_START) {
      status += delim;
      status += "START";
      delim = '|';
    }
    if (val & CU_AP_DONE) {
      status += delim;
      status += "DONE";
      delim = '|';
    }
    if (val & CU_AP_IDLE) {
      status += delim;
      status += "IDLE";
      delim = '|';
    }
    if (val & CU_AP_READY) {
      status += delim;
      status += "READY";
      delim = '|';
    }
    if (val & CU_AP_CONTINUE) {
      status += delim;
      status += "RESTART";
      delim = '|';
    }
    if (status.size())
      status += ')';
    else
      status = "(UNKNOWN)";
  }
  return status;
}

std::string
parse_cmc_status(unsigned int val)
{
  char delim = '(';
  std::string status;
  if (!val) {
    status += delim;
    status += "GOOD";
    delim = '|';
  }
  if (val & bit(0)) {
    status += delim;
    status += "SINGLE_SENSOR_UPDATE_ERR";
    delim = '|';
  }
  if (val & bit(1)) {
    status += delim;
    status += "MULTIPLE_SENSOR_UPDATE_ERR";
    delim = '|';
  }
  if (status.size())
    status += ')';
  else
    status = "(UNDEFINED_ERR)";
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

std::string
format_base10_shiftdown3(uint64_t value)
{
  constexpr double decimal_shift = 1000.0;
  constexpr int digit_precision = 3;
  return precision(static_cast<double>(value) / decimal_shift, digit_precision);
}

std::string
format_base10_shiftdown6(uint64_t value)
{
  constexpr double decimal_shift = 1000000.0;
  constexpr int digit_precision = 6;
  return precision(static_cast<double>(value) / decimal_shift, digit_precision);
}

std::string
format_base10_shiftdown(uint64_t value, int decimal, int digit_precision)
{
  double decimal_shift = std::pow(10, decimal);
  return precision(static_cast<double>(value) * decimal_shift, digit_precision);
}

uint64_t
issue_id()
{
  static std::atomic<uint64_t> id {0} ;
  return id++;
}

bool
load_host_trace()
{
  // This function is called from all the different XRT layers when
  // determining if a profiling plugin should be loaded, so it could be called
  // multiple times, but should only return true once.  The first layer
  // to check the host_trace flag would load that layer's tracing plugin.
  //
  // For example, an OpenCL host application will call this function from the
  // OpenCL profiling callbacks, the Native XRT profiling callbacks, and
  // the HAL level profiling callbacks, but only the call from the OpenCL layer
  // should actually load a tracing plugin.

  static std::mutex loadLock;
  static bool loaded = false;
  std::lock_guard<std::mutex> lock(loadLock);

  bool result = xrt_core::config::get_host_trace() && !loaded;
  loaded = true;
  return result;
}

static const std::map<std::string, std::string> clock_map = {
  {"DATA_CLK", "Data"},
  {"KERNEL_CLK", "Kernel"},
  {"SYSTEM_CLK", "System"},
};

std::string 
parse_clock_id(const std::string& id)
{
  auto clock_str = clock_map.find(id);
  return clock_str != clock_map.end() ? clock_str->second : "N/A";
}

uint64_t
mac_addr_to_value(std::string mac_addr)
{
  boost::erase_all(mac_addr, ":");
  return std::stoull(mac_addr, nullptr, 16);
}

std::string
value_to_mac_addr(const uint64_t mac_addr_value)
{
  // Any bits higher than position 48 will be ignored
  // If any are set throw an error as they cannot be placed into the mac address
  if ((mac_addr_value & 0xFFFF000000000000) != 0){
    std::string err_msg = boost::str(boost::format("Mac address exceed IP4 maximum value: 0x%1$X") % mac_addr_value);
    throw std::runtime_error(err_msg);
  }

  std::string mac_addr = boost::str(boost::format("%02X:%02X:%02X:%02X:%02X:%02X")
                                          % ((mac_addr_value >> (5 * 8)) & 0xFF)
                                          % ((mac_addr_value >> (4 * 8)) & 0xFF)
                                          % ((mac_addr_value >> (3 * 8)) & 0xFF)
                                          % ((mac_addr_value >> (2 * 8)) & 0xFF)
                                          % ((mac_addr_value >> (1 * 8)) & 0xFF)
                                          % ((mac_addr_value >> (0 * 8)) & 0xFF));

  return mac_addr;
}

std::vector<std::string>
get_uuids(const void* dtbuf)
{
  std::vector<std::string> uuidsvec;
  struct fdt_header* bph = (struct fdt_header*)dtbuf;
  uint32_t version = be32toh(bph->version);
  uint32_t off_dt = be32toh(bph->off_dt_struct);
  const char* p_struct = (const char*)dtbuf + off_dt;
  uint32_t off_str = be32toh(bph->off_dt_strings);
  const char* p_strings = (const char*)dtbuf + off_str;
  const char* p, * s;
  uint32_t tag;
  int sz;

  p = p_struct;
  uuidsvec.clear();
  while ((tag = be32toh(GET_CELL(p))) != FDT_END) {
    if (tag == FDT_BEGIN_NODE) {
        s = p;
        p = PALIGN(p + strlen(s) + 1, 4);
        continue;
    }
    if (tag != FDT_PROP)
        continue;

    sz = be32toh(GET_CELL(p));
    s = p_strings + be32toh(GET_CELL(p));
    if (version < 16 && sz >= 8)
        p = PALIGN(p, 8);

    if (!strcmp(s, "logic_uuid")) {
        uuidsvec.insert(uuidsvec.begin(), std::string(p));
    }
    else if (!strcmp(s, "interface_uuid")) {
        uuidsvec.push_back(std::string(p));
    }
    p = PALIGN(p + sz, 4);
  }
  return uuidsvec;
}

}} // utils, xrt_core
