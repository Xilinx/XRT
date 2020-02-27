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

enum configType {
    CONFIG_SECURITY = 0,
    CONFIG_CLK_SCALING,
    CONFIG_CS_THRESHOLD_POWER_OVERRIDE,
};

enum memType {
    DDR = 0,
    HBM, 
    UNKNOWN
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
update_daemon_config(const std::string host)
{
  std::cout << "update config with " << host;
}

/*
 * helper function for option:device
 * update device configuration
 */
static void 
update_device_conf(std::shared_ptr<xrt_core::device>& dev, 
  const std::string lvl, configType config_type)
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
    , daemon(false)
    , host("")
    , security("")
    , clk_scale("")
    , power_override("")
    , show(false)
    , ddr(false)
    , hbm(false)
    , enable_retention(false)
    , disable_retention(false)

{
  m_optionsDescription.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("daemon", boost::program_options::bool_switch(&daemon), "<add description>")
    ("host", boost::program_options::value<decltype(host)>(&host), "ip or hostname for peer")
    ("security", boost::program_options::value<decltype(security)>(&security), "<add description>")
    ("runtime_clk_scale", boost::program_options::value<decltype(clk_scale)>(&clk_scale), "<add description>")
    ("cs_threshold_power_override", boost::program_options::value<decltype(power_override)>(&power_override), "<add description>")
    ("show", boost::program_options::bool_switch(&show), "<add description>")
    ("enable_retention", boost::program_options::bool_switch(&enable_retention), "<add description>")
    ("disable_retention", boost::program_options::bool_switch(&disable_retention), "<add description>")
    ("ddr", boost::program_options::bool_switch(&ddr), "<add description>")
    ("hbm", boost::program_options::bool_switch(&hbm), "<add description>")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
OO_Config::execute(const SubCmdOptions& _options) const
{

  XBU::verbose("SubCommand option: config");

  XBU::verbose("Option(s):");
  for (auto & aString : _options) {
    std::string msg = "   ";
    msg += aString;
    XBU::verbose(msg);
  }

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
  if(m_help || (m_device.empty() && !daemon)) { 
    printHelp();
    return;
  }
  
  // parse device indices
  std::vector<uint16_t> device_indices;  
  // XBU::parse_device_indices(device_indices, device); [fix in XBU, else, won't work change it to "ALL"]
  
  //Option:show
  if(show) {
    XBU::verbose("Sub command: --show");
    //show daemon config
    if(daemon)
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
  if(daemon) {
    XBU::verbose("Sub command: --daemon");
    if(host.empty())
      throw xrt_core::error("Please specify ip or hostname for peer");
    update_daemon_config(host);
    return;
  }

  //Option:device
  if(!m_device.empty()) {
    XBU::verbose("Sub command: --device");
    //update security
    if (!security.empty()) {
      for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        update_device_conf(dev, security, CONFIG_SECURITY);
      }
    }

    //clock scaling
    if (!clk_scale.empty()) {
      for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        update_device_conf(dev, clk_scale, CONFIG_CLK_SCALING);
      }
    }
    
    //update threshold power override
    if (!power_override.empty()) {
      for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        update_device_conf(dev, power_override, CONFIG_CS_THRESHOLD_POWER_OVERRIDE);
      }
    }
  return;
  }

  //Option:enable_retention
  if(enable_retention) {
    XBU::verbose("Sub command: --enable_retention");
    memType mem_type = UNKNOWN; 
    if(ddr)
      mem_type = DDR;
    else if (hbm)
      mem_type = HBM;
    else
      throw xrt_core::error("Please specify memory type: ddr or hbm");

    for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        memory_retention(dev, mem_type, true);
      }
    return;
  }

  //Option:disable_retention
  if(disable_retention) {
    XBU::verbose("Sub command: --disable_retention");
    memType mem_type = UNKNOWN; 
    if(ddr)
      mem_type = DDR;
    else if (hbm)
      mem_type = HBM;
    else
      throw xrt_core::error("Please specify memory type: ddr or hbm");

    for(auto idx : device_indices) {
        auto dev = xrt_core::get_mgmtpf_device(idx);
        memory_retention(dev, mem_type, false);
      }
    return;
  }
}

