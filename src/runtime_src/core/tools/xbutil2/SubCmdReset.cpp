// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022-2026 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "xrt.h"
#include "SubCmdReset.h"
#include "core/common/query_requests.h"
#include "core/common/smi/smi.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>

// ----- C L A S S   M E T H O D S -------------------------------------------
static bool
supports_flr_reset(const xrt_core::device* dev)
{
  const auto pcie_id = xrt_core::device_query_default<xrt_core::query::pcie_id>(dev, {});
  xrt_core::smi::smi_hardware_config smi_hrdw;
  const auto hw = smi_hrdw.get_hardware_type(pcie_id);
  if (smi_hrdw.get_family(hw) != xrt_core::smi::smi_hardware_config::hardware_family::npu3)
    return false;

  const auto variant = xrt_core::smi::smi_hardware_config::get_npu3_variant(hw);
  return variant == xrt_core::smi::smi_hardware_config::npu3_variant::vf ||
         variant == xrt_core::smi::smi_hardware_config::npu3_variant::pf;
}

static void
flr_reset_device(xrt_core::device* dev)
{
  const auto bdf = xrt_core::query::pcie_bdf::to_string(
    xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));

  const auto triggered =
    xrt_core::device_query_default<xrt_core::query::flr_reset>(dev, false);

  if (triggered)
    std::cout << boost::format("Successfully triggered FLR reset on Device[%s]\n") % bdf;
  else
    std::cout << "Unable to perform FLR reset" << std::endl;
}

static void
pretty_print_action_list(xrt_core::device* dev, xrt_core::query::reset_type reset)
{
  std::cout << boost::format("Performing '%s' on '%s'\n") % reset.get_name()
                  % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));

  if (!reset.get_warning().empty())
    std::cout << "WARNING: " << reset.get_warning() << std::endl;
}

static void
reset_device(xrt_core::device* dev, const xrt_core::query::reset_type& reset)
{
  if (reset.get_key() == xrt_core::query::reset_key::flr) {
    flr_reset_device(dev);
    return;
  }

  // On Alveo, only user reset is supported
  if (reset.get_key() != xrt_core::query::reset_key::user)
    throw xrt_core::error(-EINVAL, "reset not supported");

  if (xrt_core::device_query<xrt_core::query::rom_vbnv>(dev).find("_u30_") != std::string::npos) {
    // u30 reset relies on working SC and SN info. SN is read and saved
    // when FPGA is ready. so even if there is firewall trip now, we expect
    // to be able to get S/N again
    // Having SN info available also implies there is a working SC

    std::string sn;
    sn = xrt_core::device_query<xrt_core::query::xmc_serial_num>(dev);
    if (sn.empty()) {
      throw xrt_core::error(-EINVAL,"Reset relies on S/N, but S/N can't be read from SC");
    }
    std::cout << "Card level reset. This will reset all FPGAs on the card." << std::endl;
  }
  //xocl reset is done through ioctl 
  dev->user_reset(XCL_USER_RESET);
  
  std::cout << boost::format("Successfully reset Device[%s]\n") 
    % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(dev));
}

static void
supported(std::string resetType) {
  if (resetType != "user" && resetType != "flr")
    throw xrt_core::error(-ENODEV, "reset not supported");
}

static void
add_reset_options(po::options_description& opts,
                  std::string* device,
                  std::string* reset_type,
                  bool* help,
                  const std::string& type_desc,
                  const std::string& type_default)
{
  opts.add_options()
    ("device,d", po::value<std::string>(device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("type,t", po::value<std::string>(reset_type)->notifier(supported)->implicit_value(type_default), type_desc.c_str())
    ("help", po::bool_switch(help), "Help to use this sub-command")
  ;
}

// NPU3 VF/PF SMI config supplies the flr type text. Other devices keep user reset.
static std::optional<std::pair<std::string, std::string>>
configured_reset_type(const boost::property_tree::ptree& config)
{
  const auto subcommands = config.get_child_optional("subcommands");
  if (!subcommands)
    return std::nullopt;

  for (const auto& sub : *subcommands) {
    if (sub.second.get<std::string>("name", "") != "reset")
      continue;
    const auto options = sub.second.get_child_optional("options");
    if (!options)
      return std::nullopt;
    for (const auto& opt : *options) {
      if (opt.second.get<std::string>("name", "") != "type")
        continue;
      return std::make_pair(opt.second.get<std::string>("description", ""),
                            opt.second.get<std::string>("default_value", ""));
    }
  }
  return std::nullopt;
}

SubCmdReset::SubCmdReset(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("reset", 
             "Resets the given device")
    , m_device("")
    , m_resetType("user")
    , m_help(false)
{
  const std::string longDescription = "Resets the given device.";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);

  m_commonOptions.add_options()
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest.")
    ("type,t", boost::program_options::value<decltype(m_resetType)>(&m_resetType)->notifier(supported)->implicit_value("user"), "The type of reset to perform. Types resets available:\n"
                                                                       "  user         - Hot reset (default)\n"
                                                                       /*"  aie          - Reset Aie array\n"*/
                                                                       /*"  kernel       - Kernel communication links\n"*/
                                                                       /*"  scheduler    - Scheduler\n"*/
                                                                       /*"  clear-fabric - Clears the accleration fabric with the\n"*/
                                                                       /*"                 shells verify.xclbin image.\n"*/
                                                                       /*"  memory       - Clears the memory block."*/)
    ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
SubCmdReset::setOptionConfig(const boost::property_tree::ptree& config)
{
  const auto type_opt = configured_reset_type(config);
  if (!type_opt || type_opt->second.empty())
    return;

  std::destroy_at(&m_commonOptions);
  new (&m_commonOptions) po::options_description("Common Options");
  add_reset_options(m_commonOptions, &m_device, &m_resetType, &m_help, type_opt->first, type_opt->second);
}

void
SubCmdReset::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: reset");

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  // Check to see if help was requested or no command was found
  if (m_help) {
    printHelp();
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  // Find device of interest
  std::shared_ptr<xrt_core::device> device;
  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  const bool flr_dev = supports_flr_reset(device.get());
  if (flr_dev)
    XBU::sudo_or_throw("Root privileges are required to reset this device");

  // ensure correct reset type on correct device family when type is not specified
  const std::string reset_name = (flr_dev && vm.count("type") == 0) ? "flr" : m_resetType;
  if ((flr_dev && reset_name != "flr") || (!flr_dev && reset_name != "user"))
    throw xrt_core::error(-EINVAL, "reset not supported");

  xrt_core::query::reset_type type = XBU::str_to_reset_obj(reset_name);
  pretty_print_action_list(device.get(), type);

  // Ask user for permission
  if(!XBU::can_proceed(XBU::getForce()))
    throw xrt_core::error(std::errc::operation_canceled);

  //perform reset action
  try {
    reset_device(device.get(), type);
  } catch(const xrt_core::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    std::cout << boost::format("Reset failed on Device[%s]\n") 
                 % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  } catch (std::exception& ex) {
    std::cerr << "ERROR:" << ex.what() << std::endl;
    std::cout << boost::format("Reset failed on Device[%s]\n") 
                 % xrt_core::query::pcie_bdf::to_string(xrt_core::device_query<xrt_core::query::pcie_bdf>(device));
  }

}

