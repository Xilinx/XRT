// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmd.h"

#include <iostream>
#include <boost/format.hpp>

#include "common/device.h"
#include "core/common/error.h"
#include "XBUtilities.h"
#include "XBUtilitiesCore.h"
#include "XBHelpMenusCore.h"
namespace XBU = XBUtilities;
namespace po = boost::program_options;

SubCmd::SubCmd(const std::string & _name, 
               const std::string & _shortDescription)
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
SubCmd::printHelpInternal(const bool removeLongOptDashes,
                          const std::string& customHelpSection,
                          const std::string& deviceClass,
                          const boost::program_options::options_description& commonOptions,
                          const boost::program_options::options_description& hiddenOptions) const
{
  const auto& configs = JSONConfigurable::parse_configuration_tree(m_commandConfig);
  const auto& device_suboptions = JSONConfigurable::extract_subcmd_config<JSONConfigurable, OptionOptions>(m_subOptionOptions, configs, m_subCmdName, std::string("suboption"));

  XBUtilities::report_subcommand_help(m_executableName,
                                      m_subCmdName,
                                      m_longDescription,
                                      m_exampleSyntax,
                                      commonOptions,
                                      hiddenOptions,
                                      m_globalOptions,
                                      m_positionals,
                                      m_subOptionOptions,
                                      removeLongOptDashes,
                                      customHelpSection,
                                      device_suboptions,
                                      deviceClass);
}

void 
SubCmd::printHelp(const boost::program_options::options_description& commonOptions,
                  const boost::program_options::options_description& hiddenOptions,
                  const std::string& deviceClass,
                  const bool removeLongOptDashes,
                  const std::string& customHelpSection) const
{
  printHelpInternal(removeLongOptDashes, customHelpSection, deviceClass, commonOptions, hiddenOptions);
}

void 
SubCmd::printHelp(const bool removeLongOptDashes,
                  const std::string& customHelpSection,
                  const std::string& deviceClass) const
{
  printHelpInternal(removeLongOptDashes, customHelpSection, deviceClass, m_commonOptions, m_hiddenOptions);
}

std::vector<std::string> 
SubCmd::process_arguments( po::variables_map& vm,
                           const SubCmdOptions& _options,
                           const po::options_description& common_options,
                           const po::options_description& hidden_options,
                           const po::positional_options_description& positionals,
                           const SubOptionOptions& suboptions,
                           bool validate_arguments) const
{
  po::options_description all_options("All Options");
  all_options.add(common_options);
  all_options.add(hidden_options);

  for (const auto& subCmd : suboptions)
    all_options.add_options()(subCmd->optionNameString().c_str(), subCmd->description().c_str());

  try {
    po::command_line_parser parser(_options);
    const auto options = XBU::process_arguments(vm, parser, all_options, positionals, validate_arguments);

    // Validate that only one suboption was selected if any exist
    for (size_t source_option = 0; source_option < suboptions.size(); ++source_option)
      for (size_t comparison_option = source_option + 1; comparison_option < suboptions.size(); ++comparison_option)
        conflictingOptions(vm, suboptions[source_option]->longName(), suboptions[comparison_option]->longName());

    return options;

  } catch (boost::program_options::error& e) {
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

std::vector<std::string>
SubCmd::process_arguments(boost::program_options::variables_map& vm,
                          const SubCmdOptions& _options,
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
SubCmd::conflictingOptions( const boost::program_options::variables_map& _vm, 
                            const std::string &_opt1, const std::string &_opt2) const
{
  if ( _vm.count(_opt1.c_str())  
       && !_vm[_opt1.c_str()].defaulted() 
       && _vm.count(_opt2.c_str()) 
       && !_vm[_opt2.c_str()].defaulted()) {
    std::string errMsg = boost::str(boost::format("Mutually exclusive options: '%s' and '%s'") % _opt1 % _opt2);
    throw std::logic_error(errMsg);
  }
}

void
SubCmd::addSubOption(std::shared_ptr<OptionOptions> option)
{
  option->setExecutable(getExecutableName());
  option->setCommand(getName());
  m_subOptionOptions.emplace_back(option);
}

std::shared_ptr<OptionOptions>
SubCmd::checkForSubOption(const boost::program_options::variables_map& vm, const std::string& deviceClass) const
{
  SubOptionOptions all_options = validateConfigurables<OptionOptions>(deviceClass, std::string("suboption"), m_subOptionOptions);

  std::shared_ptr<OptionOptions> option;
  // Loop through the available sub options searching for a name match
  for (auto& subOO : all_options) {
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

void 
SubCmd::setOptionConfig(const boost::property_tree::ptree &/*config*/) {
  /*Stub. Should be implemented by derived classes on per need basis*/
}
