/**
 * Copyright (C) 2021 Licensed under the Apache License, Version
 * 2.0 (the "License"). You may not use this file except in
 * compliance with the License. A copy of the License is located
 * at
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
#include "OO_BIST.h"
#include "tools/common/XBUtilities.h"
#include "core/common/error.h"
#include "core/common/unistd.h"
#include "core/common/memalign.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "experimental/xrt-next.h"
#include "experimental/xrt_bo.h"
#include "xrt.h"
#include "ert.h"
#include "xclbin.h"
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

enum class action_type { validate };

action_type
string2action(std::string str)
{
  std::transform(str.begin(), str.end(), str.begin(),
                 [](auto c) { return static_cast<char>(std::tolower(c)); });

  if (str == "validate")
    return action_type::validate;
  else
    throw xrt_core::generic_error(EINVAL, "Invalid bist action '" + str + "'");
}

namespace bist {

// bist
static void
validate(xrt_core::device* device)
{
  // lock xclbin
  auto uuid = xrt::uuid(xrt_core::device_query<xrt_core::query::xclbin_uuid>(device));
  device->open_context(uuid.get(), -1, true);
  auto at_exit = [] (auto device, auto uuid) { device->close_context(uuid.get(), -1); };
  xrt_core::scope_guard<std::function<void()>> g(std::bind(at_exit, device, uuid));

  auto bo = xrt::bo(device->get_device_handle(), 0x1000, XCL_BO_FLAGS_EXECBUF, 0);
  auto boptr = bo.map<char*>();

  std::memset(reinterpret_cast<ert_packet*>(boptr),0,0x1000);

  auto ecmd = reinterpret_cast<ert_clk_calib_cmd*>(boptr);
  ecmd->opcode = ERT_CLK_CALIB;
  ecmd->type = ERT_CTRL;
}

} // bist

void
bisttest(xrt_core::device* device, action_type action, bool force)
{
  if (action == action_type::validate)
    bist::validate(device);
}

} // namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_BIST::OO_BIST( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Controls BIST functionality")
    , m_devices({})
    , m_action("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_devices)>(&m_devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("action", boost::program_options::value<decltype(m_action)>(&m_action)->required(), "Action to perform: VALIDATE")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("action", 1 /* max_count */)
  ;
}

void
OO_BIST::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand option: bist");

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

  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).positional(m_positionalOptions).run(), vm);
    po::notify(vm); // Can throw
  }
  catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";

    printHelp();
    return;
  }

  // Exit if neither action or device specified
  if(m_help || (m_action.empty() || m_devices.empty())) {
    printHelp();
    return;
  }

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : m_devices) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));
  
  try {
    XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    return;
  }
  for (auto& device : deviceCollection)
    bisttest(device.get(), string2action(m_action), false);
}
