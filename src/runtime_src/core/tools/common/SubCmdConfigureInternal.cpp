// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2021-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdConfigureInternal.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "core/common/system.h"
#include "core/common/utils.h"
namespace XBU = XBUtilities;

#include "common/system.h"
#include "common/device.h"

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <filesystem>
#include <fstream>
#include <iostream>

#include "common/system.h"
#include "common/device.h"
#include <boost/format.hpp>
#include <map>

constexpr const char* config_file  = "/etc/msd.conf";

enum class config_type {
    security = 0,
    clk_throttling,
    threshold_power_override,
    threshold_temp_override,
    reset
};

std::ostream&
operator<<(std::ostream& os, const config_type& value)
{
  switch (value) {
    case config_type::security:
      os << "security";
      break;
    case config_type::clk_throttling:
      os << "clock throttling";
      break;
    case config_type::threshold_power_override:
      os << "threshold power override";
      break;
    case config_type::threshold_temp_override:
      os << "threshold temp override";
      break;
    case config_type::reset:
      os << "clock throttling option reset";
      break;
    default:
      throw std::runtime_error("Configuration missing enumeration conversion");
  }
  return os;
}

enum class mem_type {
    unknown= 0,
    ddr,
    hbm
};

// Presumably a struct in anticipation of more data
struct config
{
  std::string host;

  explicit config() {}

  friend std::ostream& 
  operator<<(std::ostream& ostr, const struct config& cfg)
  {
    ostr << boost::str(boost::format("host=%s") % cfg.host);
    return ostr;
  }
};

static po::options_description configHiddenOptions("Hidden Config Options");
static  boost::property_tree::ptree m_configurations;

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdConfigureInternal::SubCmdConfigureInternal(bool _isHidden, bool _isDepricated, bool _isPreliminary, bool _isUserDomain, const boost::property_tree::ptree& configurations)
    : SubCmd("configure", 
             _isUserDomain ? "Device and host configuration" : "Advanced options for configuring a device")
    , m_isUserDomain(_isUserDomain)
{
  const std::string longDescription = _isUserDomain ? "Device and host configuration." : "Advanced options for configuring a device";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  for (const auto& option : optionOptionsCollection)
    addSubOption(option);

  m_commandConfig = configurations;
}

static config
get_daemon_conf()
{
  config cfg;
  cfg.host = xrt_core::utils::get_hostname();

  std::ifstream istr(config_file);
  if (!istr)
    return cfg;

  // Load persistent value, may overwrite default one
  std::string line;
  while (std::getline(istr, line)) {
    auto pos = line.find('=', 0);
    if (pos == std::string::npos)
      throw xrt_core::system_error(EIO, "Bad daemon config file line '" + line + "'");
    auto key = line.substr(0, pos++);
    auto value = line.substr(pos);

    if (key == "host") 
      cfg.host = value;
  }

  return cfg;
}

/*
 * helper function for option: showx
 * shows daemon config
 */
static void 
show_daemon_conf()
{
  auto cfg = get_daemon_conf();
  std::cout << "Daemon:\n";
  std::cout << boost::str(boost::format("  %s\n") % cfg);
}

/*
 * helper function for option: showx
 * shows device config
 */
static void 
show_device_conf(xrt_core::device* device)
{
  auto bdf_raw = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
  auto bdf = xrt_core::query::pcie_bdf::to_string(bdf_raw);

  std::cout << bdf << "\n";

  bool is_mfg = xrt_core::device_query_default<xrt_core::query::is_mfg>(device, false);
  bool is_recovery = xrt_core::device_query_default<xrt_core::query::is_recovery>(device, false);

  if (is_mfg || is_recovery)
    throw xrt_core::error(std::errc::operation_canceled, "This operation is not supported with manufacturing image.\n");

  boost::format node_data_format("  %-33s: %s\n");
  std::string not_supported = "Not supported";
  std::string sec_level = not_supported;
  try {
    sec_level = std::to_string(xrt_core::device_query<xrt_core::query::sec_level>(device));
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for vck5000 
  }
  std::cout << node_data_format % "Security level" % sec_level;

  bool throttling_enabled = false;
  bool throttling_supported = true;
  try {
    throttling_enabled = xrt_core::device_query<xrt_core::query::xmc_scaling_enabled>(device);
  }
  catch (xrt_core::query::exception&) {
    throttling_supported = false;
  }
  std::cout << node_data_format % "Clock Throttling enabled" % (throttling_supported ? (throttling_enabled ? "true" : "false") : not_supported);

  std::string throttling_power_override = xrt_core::device_query_default<xrt_core::query::xmc_scaling_power_override>(device, not_supported);
  std::cout << node_data_format % "Throttling threshold power override" % throttling_power_override;

  std::string throttling_temp_override = xrt_core::device_query_default<xrt_core::query::xmc_scaling_temp_override>(device, not_supported);
  std::cout << node_data_format % "Throttling threshold temp override" % throttling_temp_override;

  std::string data_retention_string = not_supported;
  try {
    auto value = xrt_core::device_query<xrt_core::query::data_retention>(device);
    auto data_retention = xrt_core::query::data_retention::to_bool(value);
    data_retention_string = (data_retention ? "enabled" : "disabled");
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for vck5000 
  }
  std::cout << node_data_format % "Data retention" % data_retention_string;

  std::cout << std::flush;
}

/*
 * helper function for option:purge
 * remove the daemon config file
 */
static void 
remove_daemon_config()
{
  XBU::sudo_or_throw("Removing Daemon configuration file requires sudo");
  
  std::cout << boost::format("Removing Daemon configuration file \"%s\"\n") % config_file;
  if(!XBU::can_proceed(XBU::getForce()))
    throw xrt_core::error(std::errc::operation_canceled);

  try {
    if (std::filesystem::remove(config_file))
      std::cout << boost::format("Succesfully removed the Daemon configuration file.\n");
    else
      std::cout << boost::format("WARNING: Daemon configuration file does not exist.\n");
  } catch (const std::filesystem::filesystem_error &e) {
      std::cerr << boost::format("ERROR: %s\n") % e.what();
      throw xrt_core::error(std::errc::operation_canceled);
  }
}

/*
 * helper function for option:daemon
 * change host name in config
 */
static void 
update_daemon_config(const std::string& host = "")
{
  XBU::sudo_or_throw("Updating daemon configuration requires sudo");
  auto cfg = get_daemon_conf();

  std::ofstream cfile(config_file);
  if (!cfile)
    throw xrt_core::system_error(std::errc::invalid_argument, "Missing '" + std::string(config_file) + "'.  Cannot update");

  if(!host.empty())
    cfg.host = host;
  // update the configuration file
  cfile << boost::str(boost::format("%s\n") % cfg);
  std::cout << boost::format("Successfully updated the Daemon configuration.\n");
}

/*
 * helper function for option:device
 * update device configuration
 */
static bool 
update_device_conf(xrt_core::device* device, const std::string& value, config_type cfg)
{
  XBU::sudo_or_throw("Updating device configuration requires sudo");
  try {
    switch(cfg) {
      case config_type::security:
        xrt_core::device_update<xrt_core::query::sec_level>(device, value);
        break;
      case config_type::clk_throttling:
        xrt_core::device_update<xrt_core::query::xmc_scaling_enabled>(device, value);
        break;
      case config_type::threshold_power_override:
        xrt_core::device_update<xrt_core::query::xmc_scaling_power_override>(device, value);
        break;
      case config_type::threshold_temp_override:
        xrt_core::device_update<xrt_core::query::xmc_scaling_temp_override>(device, value);
        break;
      case config_type::reset:
        xrt_core::device_update<xrt_core::query::xmc_scaling_reset>(device, value);
        break;
    }
  } catch (const std::exception&) {
    std::cerr << boost::format("ERROR: Device does not support %s\n\n") % cfg;
    throw xrt_core::error(std::errc::operation_canceled);
  }
  return true;
}

void
SubCmdConfigureInternal::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: configure");
  po::variables_map vm;
  SubCmdConfigureOptions options;
  // Used for the suboption arguments.
  const auto unrecognized_options = process_arguments(vm, _options, false);
  fill_option_values(vm, options);
  // Find the subOption
  auto optionOption = checkForSubOption(vm, XBU::get_device_class(options.m_device, m_isUserDomain));

  if (!optionOption && m_isUserDomain) {
    // No suboption print help
    if (options.m_help) {
      printHelp(false, "", XBU::get_device_class(options.m_device, m_isUserDomain));
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
    printHelp(false, "", XBU::get_device_class(options.m_device, m_isUserDomain));
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

  // Take care of executing hidden options for xbmgmt.
  if (!m_isUserDomain) {
    // -- process "help" option -----------------------------------------------
    if (options.m_help) {
      printHelp();
      return;
    }

    // Non-device options
    // Remove the daemon config file
    if (options.m_purge) {
      XBU::verbose("Sub command: --purge");
      remove_daemon_config();
      return;
    }

    // Update daemon
    if (options.m_daemon) {
      XBU::verbose("Sub command: --daemon");
      update_daemon_config(options.m_host);
      return;
    }

    // Find device of interest
    std::shared_ptr<xrt_core::device> device;

    try {
      device = XBU::get_device(boost::algorithm::to_lower_copy(options.m_device), false /*inUserDomain*/);
    } catch (const std::runtime_error& e) {
      // Catch only the exceptions that we have generated earlier
      std::cerr << boost::format("ERROR: %s\n") % e.what();
      throw xrt_core::error(std::errc::operation_canceled);
    }

    // If in factory mode the device is not ready for use
    if (xrt_core::device_query<xrt_core::query::is_mfg>(device.get())) {
      std::cout << boost::format("ERROR: Device is in factory mode and cannot be configured\n");
      throw xrt_core::error(std::errc::operation_canceled);
    }

    // Config commands
    // Option:m_showx
    if (options.m_showx) {
      XBU::verbose("Sub command: --showx");
      if(options.m_daemon)
        show_daemon_conf();

      show_device_conf(device.get());
      return;
    }

    // Keeps track if any parameter was updated to prevent no option printout/error
    bool is_something_updated = false;

    // Update security
    if (!options.m_security.empty())
      is_something_updated = update_device_conf(device.get(), options.m_security, config_type::security);

    // Clock throttling
    if (!options.m_clk_throttle.empty())
      is_something_updated = update_device_conf(device.get(), options.m_clk_throttle, config_type::clk_throttling);
    
    // Update threshold power override
    if (!options.m_power_override.empty())
      is_something_updated = update_device_conf(device.get(), options.m_power_override, config_type::threshold_power_override);

    // Update threshold temp override
    if (!options.m_temp_override.empty())
      is_something_updated = update_device_conf(device.get(), options.m_temp_override, config_type::threshold_temp_override);

    // m_ct_reset?? TODO needs better comment
    if (!options.m_ct_reset.empty())
      is_something_updated = update_device_conf(device.get(), options.m_ct_reset, config_type::reset);

    if (!is_something_updated) {
      std::cerr << "ERROR: Please specify a valid option to configure the device" << "\n\n";
      printHelp(false, "", XBU::get_device_class(options.m_device, m_isUserDomain));
      throw xrt_core::error(std::errc::operation_canceled);
    }
  }
}

void
SubCmdConfigureInternal::fill_option_values(const boost::program_options::variables_map& vm, SubCmdConfigureOptions& options) const
{
  options.m_device = vm.count("device") ? vm["device"].as<std::string>() : "";
  options.m_help = vm.count("help") ? vm["help"].as<bool>() : false;
  options.m_daemon = vm.count("daemon") ? vm["daemon"].as<bool>() : false;
  options.m_purge = vm.count("purge") ? vm["purge"].as<bool>() : false;
  options.m_host = vm.count("host") ? vm["host"].as<std::string>() : "";
  options.m_security = vm.count("security") ? vm["security"].as<std::string>() : "";
  options.m_clk_throttle = vm.count("clk_throttle") ? vm["clk_throttle"].as<std::string>() : "";
  options.m_power_override = vm.count("ct_threshold_power_override") ? vm["ct_threshold_power_override"].as<std::string>() : "";
  options.m_temp_override = vm.count("ct_threshold_temp_override") ? vm["ct_threshold_temp_override"].as<std::string>() : "";
  options.m_ct_reset = vm.count("ct_reset") ? vm["ct_reset"].as<std::string>() : "";
  options.m_showx = vm.count("showx") ? vm["showx"].as<bool>() : false;
}

void
SubCmdConfigureInternal::setOptionConfig(const boost::property_tree::ptree &config)
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
