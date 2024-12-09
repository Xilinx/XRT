// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "OO_P2P.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "core/common/error.h"
#include "core/common/unistd.h"
#include "core/common/memalign.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "xrt/experimental/xrt-next.h"
#include "xrt/xrt_bo.h"
#include "xrt/deprecated/xrt.h"
#include "xrt/detail/xclbin.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <cctype>

#ifdef _WIN32
# pragma warning(disable : 4245)
#endif

namespace {

enum class action_type { enable, disable, validate };

action_type
string2action(std::string str)
{
  std::transform(str.begin(), str.end(), str.begin(),
                 [](auto c) { return static_cast<char>(std::tolower(c)); });

  if (str == "enable")
    return action_type::enable;
  else if (str == "disable")
    return action_type::disable;
  else if (str == "validate")
    return action_type::validate;
  else
    throw xrt_core::generic_error(EINVAL, "Invalid p2p action '" + str + "'");
}

static auto
p2p_config(xrt_core::device* device)
{
  try {
    auto cfg = xrt_core::device_query<xrt_core::query::p2p_config>(device);
    return cfg;
  }
  catch (const std::exception&) {
    throw xrt_core::system_error(ENOTSUP, "p2p is not supported");
  }
}

static void
p2p_enabled_or_error(xrt_core::device* device)
{
  auto cfg = p2p_config(device);

  long long bar = -1;
  long long rbar = -1;
  long long remap = -1;
  long long exp_bar = -1;

  for (auto& str : cfg) {
    // str is in key:value format obtained from p2p_config query
    auto pos = str.find(":");
    auto key = str.substr(0, pos);
    ++pos; // past ":"
    if (key == "bar")
      bar = std::stoll(str.substr(pos));
    else if (key == "exp_bar")
      exp_bar = std::stoll(str.substr(pos));
    else if (key == "rbar")
      rbar = std::stoll(str.substr(pos));
    else if (key == "remap")
      remap = std::stoll(str.substr(pos));
  }

  if (bar == -1)
    throw xrt_core::system_error(ENOTSUP,"p2p is not supported");
  if (rbar != -1 && rbar > bar)
    throw xrt_core::system_error(EIO,"Please WARM reboot to enable p2p");
  if (remap > 0 && remap != bar)
    throw xrt_core::system_error(EIO,"p2p remapper is not set correctly");
  if (bar == exp_bar)
    return;

  throw xrt_core::system_error(ENOTSUP,"p2p is not supported");
}

namespace p2ptest {

static void
fill_with_stride(char* begin, const char* end, char fill_byte)
{
  auto stride = xrt_core::getpagesize();
  if (std::distance(static_cast<const char*>(begin), end) % stride)
    throw xrt_core::system_error(EINVAL, "Range not an increment of stride: " + std::to_string(stride));
  for (auto itr = begin; itr != end; itr += stride)
    (*itr) = fill_byte;
}

static void
cmp_with_stride(const char* begin, const char* end, char fill_byte)
{
  auto stride = xrt_core::getpagesize();
  if (std::distance(begin, end) % stride)
    throw xrt_core::system_error(EINVAL, "Range not an increment of stride: " + std::to_string(stride));
  for (auto itr = begin; itr != end; itr += stride) {
    if ((*itr) != fill_byte) {
      auto fmt = boost::format("Error in p2p comparison, expected '0x%x' got '0x%x'") % fill_byte % (*itr);
      throw xrt_core::system_error(EIO, fmt.str());
    }
  }
}

static void
chunk(xrt_core::device* device, char* boptr, uint64_t dev_addr, uint64_t size)
{
  char byteA = 'A';
  char byteB = 'B';

  auto mem = xrt_core::aligned_alloc(xrt_core::getpagesize(), size);
  auto buf = static_cast<char*>(mem.get());

  fill_with_stride(buf, buf+size, byteA);
  try {
    device->unmgd_pwrite(buf, size, dev_addr);
  }
  catch (const std::exception&) {
    auto fmt = boost::format("Error writing 0x%x bytes to 0x%x") % size % dev_addr;
    throw xrt_core::system_error(EIO, fmt.str());
  }
  cmp_with_stride(boptr, boptr+size, byteA);

  fill_with_stride(boptr, boptr+size, byteB);
  try {
    device->unmgd_pread(buf, size, dev_addr);
  }
  catch (const std::exception&) {
    auto fmt = boost::format("Error reading 0x%x bytes from 0x%x") % size % dev_addr;
    throw xrt_core::system_error(EIO, fmt.str());
  }
  cmp_with_stride(buf, buf+size, byteB);
}

static void
bank(xrt_core::device* device, int memidx, uint64_t addr, uint64_t size)
{
  // Process the P2P buffer in 16 MB increments
  constexpr size_t chunk_size = 16 * 1024 * 1024;

  auto bo = xrt::bo(device->get_device_handle(), size, XCL_BO_FLAGS_P2P, memidx);
  auto boptr = bo.map<char*>();

  for (size_t c = 0, ci = 0; c < size; c += chunk_size, ++ci) {
    try {
      chunk(device, boptr + c, addr + c, chunk_size);
    }
    catch (const std::exception& ex) {
      auto fmt = boost::format("%s\nError p2p testing at offset 0x%x on memory index %d")
                               % ex.what() % c % memidx;
      throw xrt_core::system_error(EINVAL, fmt.str());
    }

    if (ci % (size / chunk_size / 16) == 0)
      std::cout << "." << std::flush;
  }
}

// p2ptest
static void
test(xrt_core::device* device)
{
  // lock xclbin
  auto uuid = xrt::uuid(xrt_core::device_query<xrt_core::query::xclbin_uuid>(device));
  device->open_context(uuid, -1, true);
  auto at_exit = [] (auto device, auto uuid) { device->close_context(uuid.get(), -1); };
  xrt_core::scope_guard<std::function<void()>> g(std::bind(at_exit, device, uuid));

  // p2p must be enabled
  p2p_enabled_or_error(device);

  // Get first memory bank supported by P2P
  auto mt_raw = xrt_core::device_query<xrt_core::query::mem_topology_raw>(device);
  auto mt = reinterpret_cast<const mem_topology*>(mt_raw.data());
  if (!mt)
    throw xrt_core::system_error(EINVAL, "mem_topology is invalid, cannot validate p2p");

  // support memory types
  // p2p is not supported for DDR on u280
  auto vbnv = xrt_core::device_query<xrt_core::query::rom_vbnv>(device);
  std::vector<std::string> supported = { "HBM", "bank" };
  if (vbnv.find("_U280_") == std::string::npos)
    supported.push_back("DDR");

  for (int32_t i = 0; i < mt->m_count; ++i) {
    if (!mt->m_mem_data[i].m_used)
      continue;

    auto nm = std::string(reinterpret_cast<const char *>(mt->m_mem_data[i].m_tag));
    auto itr = std::find_if(supported.begin(), supported.end(),[&nm](auto& s) { return nm.compare(0, s.size(), s) == 0; });
    if (itr == supported.end())
      continue;

    std::cout << "Performing p2p test on " << mt->m_mem_data[i].m_tag << " ";
    bank(device, i, mt->m_mem_data[i].m_base_address, mt->m_mem_data[i].m_size << 10);
    std::cout << std::endl;  // end progress dots
  }
}

} // p2ptest

void
p2p(xrt_core::device* device, action_type action, bool force)
{
  if (action == action_type::validate)
    p2ptest::test(device);

  if (action == action_type::enable) {
    XBU::sudo_or_throw("Root privileges required to enable p2p");
    device->p2p_enable(force);
  }

  if (action == action_type::disable) {
    XBU::sudo_or_throw("Root privileges required to disable p2p");
    device->p2p_disable(force);
  }
}

} // namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_P2P::OO_P2P( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Controls P2P functionality")
    , m_device("")
    , m_action("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("action", boost::program_options::value<decltype(m_action)>(&m_action)->required(), "Action to perform: ENABLE, DISABLE, or VALIDATE")
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.add("action", 1);
}

void
OO_P2P::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand option: p2p");

  XBU::verbose("Option(s):");
  for (auto & aString : _options)
    XBU::verbose(std::string(" ") + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;

  process_arguments(vm, _options);

  if (m_help) {
    printHelp();
    return;
  }

  // Validate the correct action value is used
  action_type action;
  try {
    action = string2action(m_action);
  } catch (const xrt_core::generic_error &e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }


  // Validate the correct action value is used
  if (m_device.empty()) {
    std::cerr << boost::format("ERROR: A device needs to be specified.\n");
    throw xrt_core::error(std::errc::operation_canceled);
  }


  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  try {
    p2p(device.get(), action, XBU::getForce());
  } catch (const xrt_core::system_error& ex) {
    std::cerr << "ERROR: " << ex.what() << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Print success message for the user
  switch (action) {
    case action_type::enable:
      std::cout <<  "Please WARM reboot the machine to enable P2P now.\n";
      break;

    case action_type::disable:
      std::cout << "Please WARM reboot the machine to disable P2P now.\n";
      break;

    case action_type::validate:
      std::cout << "P2P validated successfully.\n";
  }
}
