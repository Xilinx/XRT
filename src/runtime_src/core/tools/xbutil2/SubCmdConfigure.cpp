// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdConfigure.h"
#include "tools/common/OptionOptions.h"
#include "OO_HostMem.h"
#include "OO_P2P.h"
#include "OO_Performance.h"
#include "OO_Preemption.h"
#include "common/device.h"
#include "tools/common/XBUtilities.h"

namespace XBU = XBUtilities;
namespace po = boost::program_options;

SubCmdConfigure::SubCmdConfigure(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("configure", "Device and host configuration")
{
  const std::string longDescription =  "Device and host configuration.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_optionOptionsCollection = {
    {std::make_shared<OO_HostMem>("host-mem")},
    {std::make_shared<OO_P2P>("p2p")},
    {std::make_shared<OO_Performance>("pmode")},
    {std::make_shared<OO_Preemption>("force-preemption")} //hidden
  };

  for (const auto& option : m_optionOptionsCollection){
    option->setExecutable(getExecutableName());
    option->setCommand(getName());
  }
}

void
SubCmdConfigure::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: configure");
  po::variables_map vm;
  SubCmdConfigureOptions options;
  // Used for the suboption arguments.
  const auto unrecognized_options = process_arguments(vm, _options, false);
  fill_option_values(vm, options);
  // Find the subOption

  auto optionOption = checkForSubOption(vm, options);

  if (!optionOption) {
    // No suboption print help
    if (options.m_help) {
      printHelp();
      return;
    }
    // If help was not requested and additional options dont match we must throw to prevent
    // invalid positional arguments from passing through without warnings
    if (!unrecognized_options.empty()){
      std::string error_str;
      error_str.append("Unrecognized arguments:\n");
      for (const auto& option : unrecognized_options)
        error_str.append(boost::str(boost::format("  %s\n") % option));
      std::cerr << error_str <<std::endl;
    }
    else {
      std::cerr << "ERROR: Suboption missing" << std::endl;
    }
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  if (optionOption) {
    optionOption->setGlobalOptions(getGlobalOptions());
    optionOption->execute(_options);
    return;
  }

  // If no OptionOption was selected reprocess the arguments, but, validate
  // them to catch unwanted options
  process_arguments(vm, _options);
}

void
SubCmdConfigure::fill_option_values(const boost::program_options::variables_map& vm, SubCmdConfigureOptions& options) const
{
  options.m_device = vm.count("device") ? vm["device"].as<std::string>() : "";
  options.m_help = vm.count("help") ? vm["help"].as<bool>() : false;
  options.m_pmode = vm.count("pmode") ? vm["pmode"].as<std::string>() : "";
  options.m_force_preemption = vm.count("force-preemption") ? vm["force-preemption"].as<std::string>() : "";
}

void
SubCmdConfigure::setOptionConfig(const boost::property_tree::ptree &config)
{
  m_jsonConfig = SubCmdJsonObjects::JsonConfig(config.get_child("subcommands"), getName());
  try{
    m_jsonConfig.addProgramOptions(m_commonOptions, "common", getName());
    m_jsonConfig.addProgramOptions(m_hiddenOptions, "hidden", getName());
  } 
  catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
}

std::shared_ptr<OptionOptions>
SubCmdConfigure::checkForSubOption(const boost::program_options::variables_map& vm, const SubCmdConfigureOptions& options) const
{
  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  std::shared_ptr<OptionOptions> option;

  if (options.m_device.empty())
    return option;

  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(options.m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
  /* Filter out optionOptions applicable for a particular device/shim*/
  auto optionOptions = xrt_core::device_query<xrt_core::query::xrt_smi_lists>(device, xrt_core::query::xrt_smi_lists::type::configure_option_options);
  auto all_options = getOptionOptions(optionOptions);

  // Loop through the available sub options searching for a name match
  for (auto& subOO : all_options) {
    if (vm.count(subOO->longName()) != 0) {
      // Store the matched option if no other match has been found
      if (!option)
      {
        option = subOO;
      }
      // XRT will not accept more than one suboption per invocation
      // Throw an exception if more than one suboption is found within the command options
      else {
        XBUtilities::throw_cancel(boost::format("Mutually exclusive option selected: %s %s") % subOO->longName() % option->longName());
      }
    }
  }
  return option;
}

std::vector<std::shared_ptr<OptionOptions>> 
SubCmdConfigure::getOptionOptions(const xrt_core::smi::tuple_vector& options) const
{
  std::vector<std::shared_ptr<OptionOptions>> optionOptions;

  for (const auto& option : options) {
    auto it = std::find_if(m_optionOptionsCollection.begin(), m_optionOptionsCollection.end(),
               [&option](const std::shared_ptr<OptionOptions>& opt) {
                 return std::get<0>(option) == opt->getConfigName() && (std::get<2>(option) != "hidden" || XBU::getAdvance());
               });
    if (it != m_optionOptionsCollection.end()) {
      optionOptions.push_back(*it);
    }
  }
  
  return optionOptions;
} // getOptionOptions
