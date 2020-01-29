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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "XBReport.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;
#include "common/system.h"

// 3rd Party Library - Include Files

// System - Include Files

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBReport;


// ------ F U N C T I O N S ---------------------------------------------------
void
XBReport::report_system_config()
{
  // -- Get the property tree
  boost::property_tree::ptree pt;
  xrt_core::get_os_info(pt);
  XBU::trace_print_tree("System", pt);

  XBU::message("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
  XBU::message("System Configuration");
  XBU::message(XBU::format("%-14s: %s", "OS Name",      pt.get<std::string>("sysname","N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "Release",      pt.get<std::string>("release", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "Version",      pt.get<std::string>("version", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "Machine",      pt.get<std::string>("machine", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "Glibc",        pt.get<std::string>("glibc", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "Distribution", pt.get<std::string>("linux", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "Now",          pt.get<std::string>("now", "N/A").c_str()));
}

void
XBReport::report_xrt_info()
{
  // -- Get the property tree
  boost::property_tree::ptree pt;
  xrt_core::get_xrt_info(pt);
  XBU::trace_print_tree("XRT", pt);

  XBU::message("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
  XBU::message("XRT Information");
  XBU::message(XBU::format("%-14s: %s", "Version",    pt.get<std::string>("build.version", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "Git Hash",   pt.get<std::string>("build.hash", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "Git Branch", pt.get<std::string>("build.branch", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "Build Date", pt.get<std::string>("build.date", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "XOCL",       pt.get<std::string>("xocl", "N/A").c_str()));
  XBU::message(XBU::format("%-14s: %s", "XCLMGMT",    pt.get<std::string>("xclmgmt", "N/A").c_str()));
}
