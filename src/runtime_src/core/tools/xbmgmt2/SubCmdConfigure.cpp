/**
 * Copyright (C) 2022 Xilinx, Inc
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
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
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

enum class mem_type {
    unknown= 0,
    ddr,
    hbm
};

// Presumably a struct in anticipation of more data
struct config
{
  std::string host;

  void write(std::ostream& ostr)
  {
    ostr << "host=" << host << std::endl;
  }
};

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

// So far, we only support the following configs, eg.
// [Device]
// mailbox_channel_disable = 0x20
// mailbox_channel_switch = 0
// xclbin_change = 1
// cahce_xclbin = 0
// we may support in the future, like,
// [Daemon]
// host_ip = x.x.x.x
static void load_config(const std::shared_ptr<xrt_core::device>& _dev, const std::string path)
{
  boost::property_tree::ptree ptRoot;
  boost::property_tree::ini_parser::read_ini(path, ptRoot);
  static boost::property_tree::ptree emptyTree;

  const boost::property_tree::ptree PtDevice =
    ptRoot.get_child("Device", emptyTree);

  if (PtDevice.empty())
    throw std::runtime_error("No [Device] section in the config file");

  for (auto& key : PtDevice) {
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
    throw std::runtime_error(boost::str(boost::format("'%s' is not a supported config entry") % key.first));
  }
}

static std::string
get_hostname()
{
  std::string hn;
#ifdef __GNUC__
  char hostname[256] = {0};
  gethostname(hostname, 256);
  hn = hostname;
#endif
  return hn;
}

static config
get_daemon_conf()
{
  config cfg;
  cfg.host = get_hostname();

  std::ifstream istr(config_file);
  if (!istr)
    return cfg;

  // Load persistent value, may overwrite default one
  for (std::string line; std::getline(istr, line);) {
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
  std::cout << "Daemon:" << std::endl;
  std::cout << "\t";
  cfg.write(std::cout);
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

  try {
    auto is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(device);
    auto is_recovery = xrt_core::device_query<xrt_core::query::is_recovery>(device);
    if (is_mfg || is_recovery) {
      std::cerr << "This operation is not supported with manufacturing image.\n";
      return;
    }
  }
  catch (const std::exception& ex) {
    std::cout << ex.what() << "\n";
  }

  try {
    auto sec_level = xrt_core::device_query<xrt_core::query::sec_level>(device);
    std::cout << boost::format("  %-33s: %s\n") % "Security level" % sec_level;
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for vck5000 
  }

  try {
    auto scaling_enabled = xrt_core::device_query<xrt_core::query::xmc_scaling_enabled>(device);
    std::cout << boost::format("  %-33s: %s\n") % "Runtime clock scaling enabled" % scaling_enabled;
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for u30
  }

  try {
    auto scaling_override = xrt_core::device_query<xrt_core::query::xmc_scaling_power_override>(device);
    std::cout << boost::format("  %-33s: %s\n") % "Scaling threshold power override" % scaling_override;
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for u30
  }

  try {
    auto scaling_override = xrt_core::device_query<xrt_core::query::xmc_scaling_temp_override>(device);
    std::cout << boost::format("  %-33s: %s\n") % "Scaling threshold temp override" % scaling_override;
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for u30
  }

  try {
    auto value = xrt_core::device_query<xrt_core::query::data_retention>(device);
    auto data_retention = xrt_core::query::data_retention::to_bool(value);
    std::cout << boost::format("  %-33s: %s\n") % "Data retention" % (data_retention ? "enabled" : "disabled");
  }
  catch (xrt_core::query::exception&) {
    //safe to ignore. These sysfs nodes are not present for vck5000 
  }

  std::cout << std::flush;
}

/*
 * helper function for option:daemon
 * change host name in config
 */
static void 
update_daemon_config(const std::string& host)
{
  XBU::sudo_or_throw("Updating daemon configuration requires sudo");
  auto cfg = get_daemon_conf();

  std::ofstream cfile(config_file);
  if (!cfile)
    throw xrt_core::system_error(EINVAL, "Missing '" + std::string(config_file) + "'.  Cannot update");

  cfg.host = host;
  cfg.write(cfile);
}

/*
 * helper function for option:device
 * update device configuration
 */
static bool 
update_device_conf(xrt_core::device* device, const std::string& value, config_type cfg)
{
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
  auto value = xrt_core::query::data_retention::value_type(enable);
  xrt_core::device_update<xrt_core::query::data_retention>(device, value);
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
    bool help = false;
    // Hidden options
    bool daemon = false;
    std::string host = "";
    std::string security = "";
    std::string clk_scale = "";
    std::string power_override = "";
    std::string temp_override = "";
    std::string cs_reset = "";
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
        ("device,d", boost::program_options::value<decltype(devices)>(&devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
        ("help,h", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    ;

    commonOptions.add(loadConfigOptions);
    commonOptions.add(configOptions);

    // Hidden options previously under the config command
    po::options_description configHiddenOptions("Hidden Options");
    configHiddenOptions.add_options()
        ("daemon", boost::program_options::bool_switch(&daemon), "<add description>")
        ("host", boost::program_options::value<decltype(host)>(&host), "ip or hostname for peer")
        ("security", boost::program_options::value<decltype(security)>(&security), "<add description>")
        ("runtime_clk_scale", boost::program_options::value<decltype(clk_scale)>(&clk_scale), "<add description>")
        ("cs_threshold_power_override", boost::program_options::value<decltype(power_override)>(&power_override), "<add description>")
        ("cs_threshold_temp_override", boost::program_options::value<decltype(temp_override)>(&temp_override), "<add description>")
        ("cs_reset", boost::program_options::value<decltype(cs_reset)>(&cs_reset), "<add description>")
        ("showx", boost::program_options::bool_switch(&showx), "<add description>")
    ;

    po::options_description hiddenOptions("Hidden Options");
    hiddenOptions.add(configHiddenOptions);

    po::options_description allOptions("All Options");
    allOptions.add(commonOptions);
    allOptions.add(hiddenOptions);

    // Parse sub-command ...
    po::variables_map vm;

    try {
        po::store(po::command_line_parser(_options).options(allOptions).run(), vm);
        po::notify(vm); // Can throw
    } catch (po::error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
        printHelp(commonOptions, hiddenOptions);
        throw xrt_core::error(std::errc::operation_canceled);
    }

    // Ensure mutual exclusion amongst the load config and config options
    // TODO Once all of the config options are incorporated into the load config file
    // the config options should be removed
    for (auto& loadConfigOption : loadConfigOptions.options()) {
      // For common options
      for (auto& configOption : configOptions.options()) {
        conflictingOptions(vm, loadConfigOption->long_name(), configOption->long_name());
      }
      // For hidden options
      for (auto& configHiddenOption : configHiddenOptions.options()) {
        conflictingOptions(vm, loadConfigOption->long_name(), configHiddenOption->long_name());
      }
    }

    // Check the options
    // -- process "help" option -----------------------------------------------
    if (help) {
        printHelp(commonOptions, hiddenOptions);
        return;
    }

    // -- process "device" option -----------------------------------------------
    if(devices.empty()) {
        std::cerr << "ERROR: Please specify a single device using --device option" << "\n";
        printHelp(commonOptions, hiddenOptions);
        throw xrt_core::error(std::errc::operation_canceled);
    }

    // Collect all of the devices of interest
    std::set<std::string> deviceNames;
    xrt_core::device_collection deviceCollection;
    for (const auto & deviceName : devices) 
        deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));

    try {
        XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);
    } catch (const std::runtime_error& e) {
        // Catch only the exceptions that we have generated earlier
        std::cerr << boost::format("ERROR: %s\n") % e.what();
        throw xrt_core::error(std::errc::operation_canceled);
    }

    // enforce 1 device specification
    if(deviceCollection.size() != 1) {
        std::cerr << "ERROR: Please specify a single device. Multiple devices are not supported" << "\n\n";
        printHelp(commonOptions, hiddenOptions);
        throw xrt_core::error(std::errc::operation_canceled);
    }

    std::shared_ptr<xrt_core::device>& workingDevice = deviceCollection[0];

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
            load_config(workingDevice, path);
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

        show_device_conf(workingDevice.get());
        return;
    }

    // Update daemon
    if (daemon) {
        XBU::verbose("Sub command: --daemon");
        if(host.empty())
            throw xrt_core::error("Please specify ip or hostname for peer");

        update_daemon_config(host);
        return;
    }

    // Keeps track if any parameter was updated to prevent no option printout/error
    bool is_something_updated = false;

    // Update security
    if (!security.empty())
        is_something_updated = update_device_conf(workingDevice.get(), security, config_type::security);

    // Clock scaling
    if (!clk_scale.empty())
        is_something_updated = update_device_conf(workingDevice.get(), clk_scale, config_type::clk_scaling);
    
    // Update threshold power override
    if (!power_override.empty())
        is_something_updated = update_device_conf(workingDevice.get(), power_override, config_type::threshold_power_override);

    // Update threshold temp override
    if (!temp_override.empty())
        is_something_updated = update_device_conf(workingDevice.get(), temp_override, config_type::threshold_temp_override);

    // cs_reset?? TODO needs better comment
    if (!cs_reset.empty())
        is_something_updated = update_device_conf(workingDevice.get(), cs_reset, config_type::reset);

    // Enable/Disable Retention
    if (!retention.empty()) {
        // Validate the given retention string
        boost::algorithm::to_upper(retention);
        bool enableRetention = boost::iequals(retention, "ENABLE");
        bool disableRetention = boost::iequals(retention, "DISABLE");
        if (!enableRetention && !disableRetention) {
            std::cerr << "ERROR: Invalidate '--retention' option: " << retention << std::endl;
            printHelp(commonOptions, hiddenOptions);
            throw xrt_core::error(std::errc::operation_canceled);
        }

        memory_retention(workingDevice.get(), enableRetention);
        // Memory retention was updated!
        is_something_updated = true;
    }

    if (!is_something_updated) {
        std::cerr << "ERROR: Please specify a valid option to configure the device" << "\n\n";
        printHelp(commonOptions, hiddenOptions);
        throw xrt_core::error(std::errc::operation_canceled);
    }
}
