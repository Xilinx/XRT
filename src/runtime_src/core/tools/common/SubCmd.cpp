/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmd.h"

#include <boost/format.hpp>
#include <iostream>

#include "core/common/error.h"
#include "XBHelpMenusCore.h"
#include "XBUtilities.h"
#include "XBUtilitiesCore.h"
namespace XBU = XBUtilities;
namespace po = boost::program_options;

SubCmd::SubCmd(const std::string &_name, const std::string &_shortDescription)
  : m_commonOptions("Common Options")
  , m_hiddenOptions("Hidden Options")
  , m_executableName("")
  , m_subCmdName(_name)
  , m_shortDescription(_shortDescription)
  , m_longDescription("")
  , m_isHidden(false)
  , m_isDeprecated(false)
  , m_isPreliminary(false)
  , m_defaultDeviceValid(true)
{
  // Empty
}

void
SubCmd::printHelp(bool removeLongOptDashes, const std::string &customHelpSection) const
{
  XBUtilities::report_subcommand_help(m_executableName,
                                      m_subCmdName,
                                      m_longDescription,
                                      m_exampleSyntax,
                                      m_commonOptions,
                                      m_hiddenOptions,
                                      m_globalOptions,
                                      m_positionals,
                                      m_subOptionOptions,
                                      removeLongOptDashes,
                                      customHelpSection);
}

std::vector<std::string>
SubCmd::process_arguments(po::variables_map &vm,
                          const SubCmdOptions &_options,
                          const po::options_description &common_options,
                          const po::options_description &hidden_options,
                          const po::positional_options_description &positionals,
                          const SubOptionOptions &suboptions,
                          bool validate_arguments) const
{
  po::options_description all_options("All Options");
  all_options.add(common_options);
  all_options.add(hidden_options);

  for (const auto &subCmd : suboptions)
    all_options.add_options()(subCmd->optionNameString().c_str(), subCmd->description().c_str());

  try {
    po::command_line_parser parser(_options);
    const auto options = XBU::process_arguments(vm, parser, all_options, positionals, validate_arguments);

    // Validate that only one suboption was selected if any exist
    for (size_t source_option = 0; source_option < suboptions.size(); ++source_option)
      for (size_t comparison_option = source_option + 1; comparison_option < suboptions.size(); ++comparison_option)
        conflictingOptions(vm, suboptions[source_option]->longName(), suboptions[comparison_option]->longName());

    return options;

  } catch (boost::program_options::error &e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

std::vector<std::string>
SubCmd::process_arguments(boost::program_options::variables_map &vm,
                          const SubCmdOptions &_options,
                          bool validate_arguments) const
{
  return process_arguments( vm,
                            _options,
                            m_commonOptions,
                            m_hiddenOptions,
                            m_positionals,
                            m_subOptionOptions,
                            validate_arguments);
}

void
SubCmd::conflictingOptions(const boost::program_options::variables_map &_vm,
                           const std::string &_opt1,
                           const std::string &_opt2) const
{
  if (_vm.count(_opt1)
      && !_vm[_opt1].defaulted()
      && _vm.count(_opt2)
      && !_vm[_opt2].defaulted())
    XBUtilities::throw_cancel(boost::format("Mutually exclusive options: '%s' and '%s'") % _opt1 % _opt2);
}

void
SubCmd::addSubOption(std::shared_ptr<OptionOptions> option)
{
  option->setExecutable(getExecutableName());
  option->setCommand(getName());
  m_subOptionOptions.emplace_back(option);
}

std::shared_ptr<OptionOptions>
SubCmd::checkForSubOption(const boost::program_options::variables_map &vm) const
{
  std::shared_ptr<OptionOptions> option;
  // Loop through the available sub options searching for a name match
  for (auto &subOO : m_subOptionOptions) {
    if (vm.count(subOO->longName()) != 0) {
      // Store the matched option if no other match has been found
      if (!option)
        option = subOO;
      // XRT will not accept more than one suboption per invocation
      // Throw an exception if more than one suboption is found within the command options
      else
        XBUtilities::throw_cancel(boost::format("Mutually exclusive option selected: %s %s") % subOO->longName() % option->longName());
    }
  }
  return option;
}
