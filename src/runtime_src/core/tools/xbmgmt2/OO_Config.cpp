/**
 * Copyright (C) 2020 Licensed under the Apache License, Version
 * 2.0 (the "License"). You may not use this file except in
 * compliance with the License. A copy of the License is located
 * at
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
#include "OO_Config.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/common/device.h"
#include "core/common/system.h"
#include "core/common/error.h"
#include "core/common/utils.h"
#include "core/common/query_requests.h"

// 3rd Party Library - Include Files
#include <boost/format.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <fstream>

// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

constexpr const char* config_file  = "/etc/msd.conf";

enum class config_type {
    security = 0,
    clk_scaling,
    threshold_power_override,
    reset,
};

enum class mem_type {
    unknown= 0,
    ddr,
    hbm, 
    
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
 * helper function for option:show
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
 * helper function for option:show
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
    std::cout << "\tSecurity level: ";
    auto sec_level = xrt_core::device_query<xrt_core::query::sec_level>(device);
    std::cout << sec_level << "\n";
  }
  catch (const std::exception& ex) {
    std::cout << ex.what() << "\n";
  }

  try {
    std::cout << "\tRuntime clock scaling enabled status: ";
    auto scaling_enabled = xrt_core::device_query<xrt_core::query::xmc_scaling_enabled>(device);
    std::cout << scaling_enabled << "\n";
  }
  catch (const std::exception& ex) {
    std::cout << ex.what() << "\n";
  }

  try {
    std::cout << "\tScaling threshold power override: ";
    auto scaling_override = xrt_core::device_query<xrt_core::query::xmc_scaling_override>(device);
    std::cout << scaling_override << "\n";
  }
  catch (const std::exception& ex) {
    std::cout << ex.what() << "\n";
  }

  try {
    std::cout << "\tData retention: ";
    auto value = xrt_core::device_query<xrt_core::query::data_retention>(device);
    auto data_retention = xrt_core::query::data_retention::to_bool(value);
    std::cout << (data_retention ? "enabled" : "disabled") << "\n";
  }
  catch (const std::exception& ex) {
    std::cout << ex.what() << "\n";
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
static void 
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
    xrt_core::device_update<xrt_core::query::xmc_scaling_override>(device, value);
    break;
  case config_type::reset:
    xrt_core::device_update<xrt_core::query::xmc_scaling_reset>(device, value);
    break;
  }
}

/*
 * helper function for option:en(dis)able_retention
 * set data retention
 */
static void 
memory_retention(xrt_core::device* device, mem_type, bool enable)
{
  XBU::sudo_or_throw("Updating memory retention requires sudo");
  auto value = xrt_core::query::data_retention::value_type(enable);
  xrt_core::device_update<xrt_core::query::data_retention>(device, value);
}

} // namespace


// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Config::OO_Config( const std::string &_longName, bool _isHidden)
    : OptionOptions(_longName, _isHidden, "Utility to modify the memory configuration(s)")
    , m_devices({})
    , m_help(false)
    , m_daemon(false)
    , m_host("")
    , m_security("")
    , m_clk_scale("")
    , m_power_override("")
    , m_cs_reset("")
    , m_show(false)
    , m_ddr(false)
    , m_hbm(false)
    , m_retention("")

{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_devices)>(&m_devices)->multitoken(), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("retention", boost::program_options::value<decltype(m_retention)>(&m_retention),"Enables / Disables memory retention.  Valid values are: [ENABLE | DISABLE]")
    ("ddr", boost::program_options::bool_switch(&m_ddr), "Enable DDR memory for retention")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_optionsHidden.add_options()
    ("daemon", boost::program_options::bool_switch(&m_daemon), "<add description>")
    ("host", boost::program_options::value<decltype(m_host)>(&m_host), "ip or hostname for peer")
    ("security", boost::program_options::value<decltype(m_security)>(&m_security), "<add description>")
    ("runtime_clk_scale", boost::program_options::value<decltype(m_clk_scale)>(&m_clk_scale), "<add description>")
    ("cs_threshold_power_override", boost::program_options::value<decltype(m_power_override)>(&m_power_override), "<add description>")
    ("cs_reset", boost::program_options::value<decltype(m_cs_reset)>(&m_cs_reset), "<add description>")
    ("showx", boost::program_options::bool_switch(&m_show), "<add description>")
    ("hbm", boost::program_options::bool_switch(&m_hbm), "<add description>")
  ;
}

void
OO_Config::execute(const SubCmdOptions& _options) const
{

  XBU::verbose("SubCommand option: config");

  XBU::verbose("Option(s):");
  for (auto & aString : _options)
    XBU::verbose(std::string(" ") + aString);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::options_description all("All Options");
    all.add(m_optionsDescription);
    all.add(m_optionsHidden);
    //po::store(po::command_line_parser(_options).options(m_optionsDescription).run(), vm);
    po::store(po::command_line_parser(_options).options(all).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp();
    return;
  }

  // Check the options
  // Help
  if (m_help) {
    printHelp();
    return;
  }


  // Check the options
  if (!m_retention.empty()) {
    boost::algorithm::to_upper(m_retention);
    if ((m_retention != "ENABLE") && 
        (m_retention != "DISABLE")) {
      std::cerr << "ERROR: Invalidate '--retention' option: " << m_retention << std::endl;
      printHelp();
      return;
    }
  }

  if (m_devices.empty() && !m_daemon)  {
    std::cerr << "ERROR: If the daemon is to be used (e.g., set to true) then a device must also be declared." << std::endl;
    printHelp();
    return;
  }

  // -- process option: device -----------------------------------------------
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  for (const auto & deviceName : m_devices) 
    deviceNames.insert(boost::algorithm::to_lower_copy(deviceName));
  
  XBU::collect_devices(deviceNames, false /*inUserDomain*/, deviceCollection);

  //Option:show
  if (m_show) {
    XBU::verbose("Sub command: --show");
    //show daemon config
    if(m_daemon)
      show_daemon_conf();

    //show device config
    for (const auto& dev : deviceCollection)
      show_device_conf(dev.get());

    return;
  }

  //-- process option: daemon -----------------------------------------------
  if (m_daemon) {
    XBU::verbose("Sub command: --daemon");
    if(m_host.empty())
      throw xrt_core::error("Please specify ip or hostname for peer");

    update_daemon_config(m_host);
    return;
  }

  //-- process option: device -----------------------------------------------
  if (!m_devices.empty()) {
    XBU::verbose("Sub command: --device");
    //update security
    if (!m_security.empty())
      for (const auto& dev : deviceCollection)
        update_device_conf(dev.get(), m_security, config_type::security);

    //clock scaling
    if (!m_clk_scale.empty())
      for (const auto& dev : deviceCollection)
        update_device_conf(dev.get(), m_clk_scale, config_type::clk_scaling);
    
    //update threshold power override
    if (!m_power_override.empty())
      for (const auto& dev : deviceCollection)
        update_device_conf(dev.get(), m_power_override, config_type::threshold_power_override);

    //cs_reset
    if (!m_cs_reset.empty())
      for (const auto& dev : deviceCollection)
        update_device_conf(dev.get(), m_cs_reset, config_type::reset);

    //  enable/disable_retention
    if (!m_retention.empty()) {
      auto mem = mem_type::unknown; 
      if(m_ddr)
        mem = mem_type::ddr;
      else if (m_hbm)
        mem = mem_type::hbm;
      else
        throw xrt_core::system_error(EINVAL, "Please specify memory type: ddr or hbm");

      bool enableRetention = (m_retention == "ENABLE");
      for (const auto& dev : deviceCollection)
        memory_retention(dev.get(), mem, enableRetention);
    }
  }
}

