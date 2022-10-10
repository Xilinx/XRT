/**
 * Copyright (C) 2020-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef __XBHelpMenusCore_h_
#define __XBHelpMenusCore_h_

// Include files
// Please keep these to the bare minimum
#include <boost/program_options.hpp>
#include <string>
#include <utility>  // Pair template
#include <vector>

#include "SubCmd.h"

// ----------------------- T Y P E D E F S -----------------------------------
using SubCmdsCollection = std::vector<std::shared_ptr<SubCmd>>;

namespace XBUtilities
{
void
report_commands_help(const std::string& _executable,
                     const std::string& _description,
                     const boost::program_options::options_description& _optionDescription,
                     const boost::program_options::options_description& _optionHidden,
                     const SubCmdsCollection& _subCmds);

void
report_subcommand_help(const std::string& _executableName,
                       const std::string& _subCommand,
                       const std::string& _description,
                       const std::string& _extendedHelp,
                       const boost::program_options::options_description& _optionDescription,
                       const boost::program_options::options_description& _optionHidden,
                       const boost::program_options::options_description& _globalOptions,
                       const boost::program_options::positional_options_description& _positionalDescription =
                           boost::program_options::positional_options_description(),
                       const SubCmd::SubOptionOptions& _subOptionOptions = SubCmd::SubOptionOptions(),
                       bool removeLongOptDashes = false,
                       const std::string& customHelpSection = "");

void
report_option_help(const std::string& _groupName,
                   const boost::program_options::options_description& _optionDescription,
                   const boost::program_options::positional_options_description& _positionalDescription,
                   bool _bReportParameter = true,
                   bool removeLongOptDashes = false);

std::string
create_usage_string(const boost::program_options::options_description& _od,
                    const boost::program_options::positional_options_description& _pod,
                    bool removeLongOptDashes = false);

std::vector<std::string>
process_arguments(boost::program_options::variables_map& vm,
                  boost::program_options::command_line_parser& parser,
                  const boost::program_options::options_description& options,
                  const boost::program_options::positional_options_description& positionals,
                  bool validate_arguments);
};  // namespace XBUtilities

#endif
