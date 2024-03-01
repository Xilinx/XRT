// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __XBHelpMenusCore_h_
#define __XBHelpMenusCore_h_

// Include files
// Please keep these to the bare minimum
#include "SubCmd.h"

#include <string>
#include <vector>
#include <utility> // Pair template
#include <boost/program_options.hpp>

// ----------------------- T Y P E D E F S -----------------------------------
using SubCmdsCollection = std::vector<std::shared_ptr<SubCmd>>;

namespace XBUtilities {
  void
  report_commands_help( const std::string& _executable,
                        const std::string& _description,
                        const boost::program_options::options_description& _optionDescription,
                        const boost::program_options::options_description& _optionHidden,
                        const SubCmdsCollection& _subCmds);

  void
  report_subcommand_help( const std::string& _executableName,
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
                          const std::string& customHelpSection = "",
                          const std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>>& commandConfig =
                            std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>>(),
                          const std::string& deviceClass = "");

  void
    report_option_help( const std::string & _groupName, 
                        const boost::program_options::options_description& _optionDescription,
                        const bool _bReportParameter = true,
                        const bool removeLongOptDashes = false,
                        const std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>>& device_options =
                          std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>>(),
                        const std::string& deviceClass = "");

  std::string
    create_usage_string( const boost::program_options::options_description &_od,
                         const boost::program_options::positional_options_description & _pod,
                         bool removeLongOptDashes = false);

  std::vector<std::string>
    process_arguments( boost::program_options::variables_map& vm,
                       boost::program_options::command_line_parser& parser,
                       const boost::program_options::options_description& options,
                       const boost::program_options::positional_options_description& positionals,
                       bool validate_arguments);
};

#endif
