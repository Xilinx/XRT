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
#include "SubCmdConfigure.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files

// System - Include Files
#include <iostream>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

SubCmdConfigure::SubCmdConfigure(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("configure",
             "Advanced options for configuring a device")
{
  const std::string longDescription = "Advanced options for configuring a device";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdConfigure::execute(const SubCmdOptions& _options) const
{
    XBU::verbose("SubCommand: configure");
    // -- Retrieve and parse the subcommand options -----------------------------
    // Common options
    std::vector<std::string> devices;
    std::string path = "";
    std::string retention = "";
    bool ddr = false;
    bool help = false;
    // Hidden options
    bool daemon = false;
    std::string host = "";
    std::string security = "";
    std::string clk_scale = "";
    std::string power_override = "";
    std::string temp_override = "";
    std::string cs_reset = "";
    bool show = false;
    bool hbm = false;

    po::options_description commonOptions("Common Options");
    commonOptions.add_options()
        ("device,d", boost::program_options::value<decltype(devices)>(&devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
        ("input", boost::program_options::value<decltype(path)>(&path),"INI file with the memory configuration")
        ("retention", boost::program_options::value<decltype(retention)>(&retention),"Enables / Disables memory retention.  Valid values are: [ENABLE | DISABLE]")
        ("ddr", boost::program_options::bool_switch(&ddr), "Enable DDR memory for retention")
        ("help,h", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    ;

    po::options_description hiddenOptions("Hidden Options");
    hiddenOptions.add_options()
        ("daemon", boost::program_options::bool_switch(&daemon), "<add description>")
        ("host", boost::program_options::value<decltype(host)>(&host), "ip or hostname for peer")
        ("security", boost::program_options::value<decltype(security)>(&security), "<add description>")
        ("runtime_clk_scale", boost::program_options::value<decltype(clk_scale)>(&clk_scale), "<add description>")
        ("cs_threshold_power_override", boost::program_options::value<decltype(power_override)>(&power_override), "<add description>")
        ("cs_threshold_temp_override", boost::program_options::value<decltype(temp_override)>(&temp_override), "<add description>")
        ("cs_reset", boost::program_options::value<decltype(cs_reset)>(&cs_reset), "<add description>")
        ("showx", boost::program_options::bool_switch(&show), "<add description>")
        ("hbm", boost::program_options::bool_switch(&hbm), "<add description>")
    ;

    boost::program_options::positional_options_description positionalOptions;
    positionalOptions.
        add("input", 1 /* max_count */)
    ;

    po::options_description allOptions("All Options");
    allOptions.add(commonOptions);
    allOptions.add(hiddenOptions);

    std::cerr << "ERROR: Please specify a valid option to configure the device" << "\n\n";
    printHelp(commonOptions, hiddenOptions);
    throw xrt_core::error(std::errc::operation_canceled);
}