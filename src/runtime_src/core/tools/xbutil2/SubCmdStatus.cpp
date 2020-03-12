/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include "SubCmdStatus.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "xrt.h"
#include "core/common/xrt_profiling.h"
#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/error.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdStatus::SubCmdStatus(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("status", 
             "List the debug IPs available on the acceleration program loaded on the given device")
{
  const std::string longDescription = "<add long discription>";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdStatus::execute(const SubCmdOptions& _options) const
// Reference Command:  [-d card] [--debug_ip_name]
//                     Get status of AIM monitor IP on the xclbin loaded on card 1
//                     xbutil status -d 1 --aim
{
  XBU::verbose("SubCommand: status");
  // -- Retrieve and parse the subcommand options -----------------------------
  unsigned int card = 0;
  uint64_t region = 0;
  std::string xclbin;
  bool help = false;

  po::options_description statusDesc("status options");
  statusDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<unsigned int>(&card), "Card to be examined")
    (",aim", boost::program_options::value<unsigned int>(&card), "Status of AXI Interface Monitor")
    (",accelmonitor", boost::program_options::value<unsigned int>(&card), "Status of Accelerator Monitor")
//    (",tracefunnel", boost::program_options::value<unsigned int>(&card), "")
//    (",monitorfifolite", boost::program_options::value<unsigned int>(&card), "")
//    (",monitorfifofull", boost::program_options::value<unsigned int>(&card), "")
//    (",p", boost::program_options::value<std::string>(&xclbin), "The xclbin image to load")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(statusDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    xrt_core::send_exception_message(e.what(), "XBUTIL");
    printHelp(statusDesc);

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(statusDesc);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  XBU::verbose(boost::str(boost::format("  Card: %ld") % card));

  auto device = xrt_core::get_userpf_device(card);
  auto hdl = device->get_device_handle();

  size_t sz1 = 0, sectionSz = 0;
  // Get the size of full debug_ip_layout
  xclGetDebugIpLayout(hdl, nullptr, sz1, &sectionSz);
  // Allocate buffer to retrieve debug_ip_layout information from loaded xclbin
  std::vector<char> buffer(sectionSz);
  xclGetDebugIpLayout(hdl, buffer.data(), sectionSz, &sz1);
  
  auto map = reinterpret_cast<debug_ip_layout*>(buffer.data());

  std::cout << " Number of IPs :: " << map->m_count;

  std::cout << "INFO: xbutil2 status succeeded.\n";
}
