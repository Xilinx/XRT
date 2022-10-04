// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdConfigure.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
#include "core/common/system.h"
#include "core/common/utils.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>

constexpr const char* config_file  = "/etc/msd.conf";

enum class config_type {
    security = 0,
    clk_scaling,
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
    case config_type::clk_scaling:
      os << "runtime clock scaling";
      break;
    case config_type::threshold_power_override:
      os << "threshold power override";
      break;
    case config_type::threshold_temp_override:
      os << "threshold temp override";
      break;
    case config_type::reset:
      os << "clock scaling option reset";
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

SubCmdConfigure::SubCmdConfigure(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("configure",
             "Advanced options for configuring a device")
{
  const std::string long_description = "Advanced options for configuring a device";
  setLongDescription(long_description);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

static void load_config(const std::shared_ptr<xrt_core::device>& _dev, const std::string path)
{
  boost::property_tree::ptree pt_root;
  boost::property_tree::ini_parser::read_ini(path, pt_root);
  static boost::property_tree::ptree empty_tree;

  const boost::property_tree::ptree& pt_device = pt_root.get_child("Device", empty_tree);

  if (pt_device.empty())
    throw std::runtime_error(boost::str(boost::format("No [Device] section in the config file. Config File: %s") % path));

  for (const auto& key : pt_device) {
    if (!key.first.compare("mailbox_channel_disable")) {
      xrt_core::device_update<xrt_core::query::config_mailbox_channel_disable>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    if (!key.first.compare("mailbox_channel_switch")) {
      xrt_core::device_update<xrt_core::query::config_mailbox_channel_switch>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    if (!key.first.compare("xclbin_change")) {
      xrt_core::device_update<xrt_core::query::config_xclbin_change>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    if (!key.first.compare("cache_xclbin")) {
      xrt_core::device_update<xrt_core::query::cache_xclbin>(_dev.get(), key.second.get_value<std::string>());
      continue;
    }
    bool is_versal = xrt_core::device_query<xrt_core::query::is_versal>(_dev);
    if (is_versal) {
      try {
        if (!key.first.compare("scaling_enabled")) {
          xrt_core::device_update<xrt_core::query::xgq_scaling_enabled>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
        if (!key.first.compare("scaling_power_override")) {
          xrt_core::device_update<xrt_core::query::xgq_scaling_power_override>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
        if (!key.first.compare("scaling_temp_override")) {
          xrt_core::device_update<xrt_core::query::xgq_scaling_temp_override>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
      } catch(const xrt_core::query::exception&) {}
    } else {
      try {
        if (!key.first.compare("scaling_enabled")) {
          xrt_core::device_update<xrt_core::query::xmc_scaling_enabled>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
        if (!key.first.compare("scaling_power_override")) {
          xrt_core::device_update<xrt_core::query::xmc_scaling_power_override>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
        if (!key.first.compare("scaling_temp_override")) {
          xrt_core::device_update<xrt_core::query::xmc_scaling_temp_override>(_dev.get(), key.second.get_value<std::string>());
          continue;
        }
      } catch(const xrt_core::query::exception&) {}
    }
    throw std::runtime_error(boost::str(boost::format("'%s' is not a supported config entry") % key.first));
  }
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

  bool is_mfg = false;
  bool is_recovery = false;
  try {
    is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(device);
    is_recovery = xrt_core::device_query<xrt_core::query::is_recovery>(device);
  }
  catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
  }

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

  std::string scaling_enabled = not_supported;
  try {
    scaling_enabled = xrt_core::device_query<xrt_core::query::xmc_scaling_enabled>(device);
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for u30 and vck5000
  }
  std::cout << node_data_format % "Runtime clock scaling enabled" % scaling_enabled;

  std::string scaling_power_override = not_supported;
  try {
    scaling_power_override = xrt_core::device_query<xrt_core::query::xmc_scaling_power_override>(device);
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for u30 and vck5000
  }
  std::cout << node_data_format % "Scaling threshold power override" % scaling_power_override;

  std::string scaling_temp_override = not_supported;
  try {
    scaling_temp_override = xrt_core::device_query<xrt_core::query::xmc_scaling_temp_override>(device);
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for u30 and vck5000
  }
  std::cout << node_data_format % "Scaling threshold temp override" % scaling_temp_override;

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
    if (boost::filesystem::remove(config_file))
      std::cout << boost::format("Succesfully removed the Daemon configuration file.\n");
    else
      std::cout << boost::format("WARNING: Daemon configuration file does not exist.\n");
  } catch (const boost::filesystem::filesystem_error &e) {
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
      case config_type::clk_scaling:
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

/*
 * helper function for option:en(dis)able_retention
 * set data retention
 */
static void 
memory_retention(xrt_core::device* device, bool enable)
{
  XBU::sudo_or_throw("Updating memory retention requires sudo");
  try {
    auto value = xrt_core::query::data_retention::value_type(enable);
    xrt_core::device_update<xrt_core::query::data_retention>(device, value);
  } catch (const std::exception&) {
    std::cerr << boost::format("ERROR: Device does not support memory retention\n\n");
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

void
SubCmdConfigure::execute(const SubCmdOptions& _options) const
{
    XBU::verbose("SubCommand: configure");
    // -- Retrieve and parse the subcommand options -----------------------------
    // Common options
    std::string device_str;
    std::string path;
    std::string retention;
    bool help = false;
    // Hidden options
    bool daemon = false;
    bool purge  = false;
    std::string host;
    std::string security;
    std::string clk_scale;
    std::string power_override;
    std::string temp_override;
    std::string cs_reset;
    bool showx = false;

    // Options previously under the load config command
    po::options_description loadConfigOptions("Load Config Options");
    loadConfigOptions.add_options()
      ("input", boost::program_options::value<decltype(path)>(&path),"INI file with the memory configuration")
    ;

    // Options previously under the config command
    po::options_description configOptions("Config Options");
    configOptions.add_options()
      ("retention", boost::program_options::value<decltype(retention)>(&retention),"Enables / Disables memory retention.  Valid values are: [ENABLE | DISABLE]")
    ;

    po::options_description commonOptions("Common Options");
    commonOptions.add_options()
        ("device,d", boost::program_options::value<decltype(device_str)>(&device_str), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
        ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    ;

    commonOptions.add(loadConfigOptions);
    commonOptions.add(configOptions);

    // Hidden options previously under the config command
    po::options_description configHiddenOptions("Hidden Options");
    configHiddenOptions.add_options()
        ("daemon", boost::program_options::bool_switch(&daemon), "Update the device daemon configuration")
        ("purge", boost::program_options::bool_switch(&purge), "Remove the daemon configuration file")
        ("host", boost::program_options::value<decltype(host)>(&host), "IP or hostname for device peer")
        ("security", boost::program_options::value<decltype(security)>(&security), "Update the security level for the device")
        ("runtime_clk_scale", boost::program_options::value<decltype(clk_scale)>(&clk_scale), "Enable/disable the device runtime clock scaling")
        ("cs_threshold_power_override", boost::program_options::value<decltype(power_override)>(&power_override), "Update the power threshold in watts")
        ("cs_threshold_temp_override", boost::program_options::value<decltype(temp_override)>(&temp_override), "Update the temperature threshold in celsius")
        ("cs_reset", boost::program_options::value<decltype(cs_reset)>(&cs_reset), "Reset all scaling options")
        ("showx", boost::program_options::bool_switch(&showx), "Display the device configuration settings")
    ;

    // Parse sub-command ...
    po::variables_map vm;

    process_arguments(vm, _options, commonOptions, configHiddenOptions);

    // Ensure mutual exclusion amongst the load config and config options
    // TODO Once all of the config options are incorporated into the load config file
    // the config options should be removed
    for (const auto& loadConfigOption : loadConfigOptions.options()) {
      // For common options
      for (const auto& configOption : configOptions.options())
        conflictingOptions(vm, loadConfigOption->long_name(), configOption->long_name());

      // For hidden options
      for (const auto& configHiddenOption : configHiddenOptions.options())
        conflictingOptions(vm, loadConfigOption->long_name(), configHiddenOption->long_name());
    }

    // Check the options
    // -- process "help" option -----------------------------------------------
    if (help) {
        printHelp(commonOptions, configHiddenOptions);
        return;
    }

    // Non-device options
    // Remove the daemon config file
    if (purge) {
        XBU::verbose("Sub command: --purge");
        remove_daemon_config();
        return;
    }

    // Update daemon
    if (daemon) {
        XBU::verbose("Sub command: --daemon");
        update_daemon_config(host);
        return;
    }

    // Find device of interest
    std::shared_ptr<xrt_core::device> device;

    try {
        device = XBU::get_device(boost::algorithm::to_lower_copy(device_str), false /*inUserDomain*/);
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

    // Load Config commands
    // -- process "input" option -----------------------------------------------
    if (!path.empty()) {
        if (!boost::filesystem::exists(path)) {
            std::cerr << boost::format("ERROR: Input file does not exist: '%s'") % path << "\n\n";
            throw xrt_core::error(std::errc::operation_canceled);
        }

        if(boost::filesystem::extension(path).compare(".ini") != 0) {
            std::cerr << boost::format("ERROR: Input file should be an INI file: '%s'") % path << "\n\n";
            throw xrt_core::error(std::errc::operation_canceled);
        }

        try {
            load_config(device, path);
            std::cout << "Config has been successfully loaded" << std::endl;
            return;
        } catch (const std::runtime_error& e) {
            // Catch only the exceptions that we have generated earlier
            std::cout << boost::format("ERROR: %s\n") % e.what();
            throw xrt_core::error(std::errc::operation_canceled);
        }
    }

    // Config commands
    // Option:showx
    if (showx) {
        XBU::verbose("Sub command: --showx");
        if(daemon)
            show_daemon_conf();

        show_device_conf(device.get());
        return;
    }

    // Keeps track if any parameter was updated to prevent no option printout/error
    bool is_something_updated = false;

    // Update security
    if (!security.empty())
        is_something_updated = update_device_conf(device.get(), security, config_type::security);

    // Clock scaling
    if (!clk_scale.empty())
        is_something_updated = update_device_conf(device.get(), clk_scale, config_type::clk_scaling);
    
    // Update threshold power override
    if (!power_override.empty())
        is_something_updated = update_device_conf(device.get(), power_override, config_type::threshold_power_override);

    // Update threshold temp override
    if (!temp_override.empty())
        is_something_updated = update_device_conf(device.get(), temp_override, config_type::threshold_temp_override);

    // cs_reset?? TODO needs better comment
    if (!cs_reset.empty())
        is_something_updated = update_device_conf(device.get(), cs_reset, config_type::reset);

    // Enable/Disable Retention
    if (!retention.empty()) {
        // Validate the given retention string
        boost::algorithm::to_upper(retention);
        bool enableRetention = boost::iequals(retention, "ENABLE");
        bool disableRetention = boost::iequals(retention, "DISABLE");
        if (!enableRetention && !disableRetention) {
            std::cerr << "ERROR: Invalidate '--retention' option: " << retention << std::endl;
            printHelp(commonOptions, configHiddenOptions);
            throw xrt_core::error(std::errc::operation_canceled);
        }

        memory_retention(device.get(), enableRetention);
        // Memory retention was updated!
        is_something_updated = true;
    }

    if (!is_something_updated) {
        std::cerr << "ERROR: Please specify a valid option to configure the device" << "\n\n";
        printHelp(commonOptions, configHiddenOptions);
        throw xrt_core::error(std::errc::operation_canceled);
    }
}
