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


// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------

namespace {

enum class configType {
    security = 0,
    clk_scaling,
    threshold_power_override,
};

enum class memType {
    unknown= 0,
    ddr,
    hbm, 
    
};

/*
 * helper function for option:show
 * shows daemon config
 */
static void 
show_daemon_conf()
{
  std::cout << "Show daemon config\n";
}

/*
 * helper function for option:show
 * shows device config
 */
static void 
show_device_conf(std::shared_ptr<xrt_core::device>& dev)
{
  dev=dev;
  std::cout << "Show device config\n";
}

/*
 * helper function for option:daemon
 * change host name in config
 */
static void 
update_daemon_config(const std::string& host)
{
  std::cout << "update config with " << host;
}

/*
 * helper function for option:device
 * update device configuration
 */
static void 
update_device_conf(std::shared_ptr<xrt_core::device>& dev, 
  const std::string& lvl, configType config_type)
{
  dev=dev;
  config_type=config_type;
  std::cout << "update device config with level " << lvl;
}

/*
 * helper function for option:en(dis)able_retention
 * set data retention
 */
static void 
memory_retention(std::shared_ptr<xrt_core::device>& dev, 
  memType type, bool enable)
{
  dev=dev;
  type=type;
  enable=enable;
  std::cout << "update memorty retention\n";
}

}
//end anonymous namespace


// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Config::OO_Config( const std::string &_longName)
    : OptionOptions(_longName, "<Add description>")
    , m_device("")
    , m_help(false)
    , m_daemon(false)
    , m_host("")
    , m_security("")
    , m_clk_scale("")
    , m_power_override("")
    , m_show(false)
    , m_ddr(false)
    , m_hbm(false)
    , m_enable_retention(false)
    , m_disable_retention(false)

{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("daemon", boost::program_options::bool_switch(&m_daemon), "<add description>")
    ("host", boost::program_options::value<decltype(m_host)>(&m_host), "ip or hostname for peer")
    ("security", boost::program_options::value<decltype(m_security)>(&m_security), "<add description>")
    ("runtime_clk_scale", boost::program_options::value<decltype(m_clk_scale)>(&m_clk_scale), "<add description>")
    ("cs_threshold_power_override", boost::program_options::value<decltype(m_power_override)>(&m_power_override), "<add description>")
    ("show", boost::program_options::bool_switch(&m_show), "<add description>")
    ("enable_retention", boost::program_options::bool_switch(&m_enable_retention), "<add description>")
    ("disable_retention", boost::program_options::bool_switch(&m_disable_retention), "<add description>")
    ("ddr", boost::program_options::bool_switch(&m_ddr), "<add description>")
    ("hbm", boost::program_options::bool_switch(&m_hbm), "<add description>")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
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
    po::store(po::command_line_parser(_options).options(m_optionsDescription).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp();
    throw; // Re-throw exception
  }

  //exit if neither daemon or device is specified
  if(m_help || (m_device.empty() && !m_daemon)) { 
    printHelp();
    return;
  }
  
  // parse device indices
  std::vector<uint16_t> device_indices;  
  XBU::parse_device_indices(device_indices, m_device);
  
  //Option:show
  if(m_show) {
    XBU::verbose("Sub command: --show");
    //show daemon config
    if(m_daemon)
      show_daemon_conf();

    //show device config
    if (!device_indices.empty()) {
      for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        show_device_conf(dev);
      }
    }
  return;
  }

  //Option:daemon
  if(m_daemon) {
    XBU::verbose("Sub command: --daemon");
    if(m_host.empty())
      throw xrt_core::error("Please specify ip or hostname for peer");
    update_daemon_config(m_host);
    return;
  }

  //Option:device
  if(!m_device.empty()) {
    XBU::verbose("Sub command: --device");
    //update security
    if (!m_security.empty()) {
      for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        update_device_conf(dev, m_security, configType::security);
      }
    }

    //clock scaling
    if (!m_clk_scale.empty()) {
      for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        update_device_conf(dev, m_clk_scale, configType::clk_scaling);
      }
    }
    
    //update threshold power override
    if (!m_power_override.empty()) {
      for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        update_device_conf(dev, m_power_override, configType::threshold_power_override);
      }
    }
  return;
  }

  //Option:enable_retention
  if(m_enable_retention) {
    XBU::verbose("Sub command: --enable_retention");
    memType mem_type = memType::unknown; 
    if(m_ddr)
      mem_type = memType::ddr;
    else if (m_hbm)
      mem_type = memType::hbm;
    else
      throw xrt_core::error("Please specify memory type: ddr or hbm");

    for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        memory_retention(dev, mem_type, true);
      }
    return;
  }

  //Option:disable_retention
  if(m_disable_retention) {
    XBU::verbose("Sub command: --disable_retention");
    memType mem_type = memType::unknown; 
    if(m_ddr)
      mem_type = memType::ddr;
    else if (m_hbm)
      mem_type = memType::hbm;
    else
      throw xrt_core::error("Please specify memory type: ddr or hbm");

    for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        memory_retention(dev, mem_type, false);
      }
    return;
  }
}

