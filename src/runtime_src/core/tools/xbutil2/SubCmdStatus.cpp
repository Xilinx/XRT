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

const int8_t maxDebugIpType = 13;  // TRACE_S2MM_FULL

bool debugIpOpt[maxDebugIpType];
uint64_t debugIpNum[maxDebugIpType];
static const char* debugIpNames[maxDebugIpType] = {
  "unknown",
  "LAPC",
  "ILA",
  "AIM",
  "TraceFunnel",
  "TraceFifoLite",
  "TraceFifoFull",
  "AM",
  "ASM",
  "AxiStreamProtocolChecker",
  "TS2MM",
  "AxiDMA",
  "TS2MMFull"
};

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
  bool help = false;

  po::options_description statusDesc("status options");

enum DEBUG_IP_TYPE {
UNDEFINED = 0,
LAPC,
ILA,
AXI_MM_MONITOR,
AXI_TRACE_FUNNEL,
AXI_MONITOR_FIFO_LITE,
AXI_MONITOR_FIFO_FULL,
ACCEL_MONITOR,
AXI_STREAM_MONITOR,
AXI_STREAM_PROTOCOL_CHECKER,
TRACE_S2MM,
AXI_DMA,
TRACE_S2MM_FULL
};



  statusDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<unsigned int>(&card), "Card to be examined")
    (",aim", boost::program_options::bool_switch(&debugIpOpt[AXI_MM_MONITOR]), "Status of AXI Interface Monitor")
    (",accelmonitor", boost::program_options::bool_switch(&debugIpOpt[ACCEL_MONITOR]), "Status of Accelerator Monitor")
    (",asm", boost::program_options::bool_switch(&debugIpOpt[AXI_STREAM_MONITOR]), "Status of AXI Stream Monitor")
//    (",tracefunnel", boost::program_options::value<unsigned int>(&card), "")
//    (",monitorfifolite", boost::program_options::value<unsigned int>(&card), "")
//    (",monitorfifofull", boost::program_options::value<unsigned int>(&card), "")
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

  if (sectionSz == 0 || map->m_count <= 0) {
    std::cout << "INFO: Failed to find any debug IPs on the platform. "
         << "Ensure that a valid bitstream with debug IPs (AIM, LAPC) is successfully downloaded. \n";
    return;
  }

  std::cout << " Number of IPs found :: " << map->m_count;
  for(uint64_t i = 0; i < map->m_count; i++) {
    if (map->m_debug_ip_data[i].m_type > maxDebugIpType) {
      std::cout << "Found invalid IP in debug ip layout with type "
                << map->m_debug_ip_data[i].m_type << std::endl;
      return;
    }
    ++debugIpNum[map->m_debug_ip_data[i].m_type];
  }

  std::stringstream sstr;
  for(uint64_t i = 0; i < maxDebugIpType; i++) {
    if(0 == debugIpNum[i]) {
      continue;
    }
    sstr << debugIpNames[i] << "(" << debugIpNum[i] << ")  ";
  }

  std::cout << "INFO: xbutil2 status succeeded.\n";
}
