// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "tools/common/JSONConfigurable.h"
#include "core/common/error.h"

// Utilities
#include "tools/common/XBHelpMenusCore.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "tools/common/XBHelpMenus.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>
#include <regex>

std::vector<boost::property_tree::ptree>
JSONConfigurable::parse_configuration_tree(
    const std::vector<std::string>& targets,
    const boost::property_tree::ptree& configuration)
{
    std::vector<boost::property_tree::ptree> output;
    try {
        for (const std::string& target: targets) {
            for (const auto& config : configuration) {
                const auto& pt = config.second;
                if (!target.compare(pt.get<std::string>("name")))
                    output.push_back(pt);
            }
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(e.what());
    }
    return output;
}
