// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "OO_AieClockFreq.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"

namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/algorithm/string/join.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
namespace qr = xrt_core::query;

// System - Include Files
#include <iostream>
#include <math.h>

static double
to_megaHz(uint64_t value)
{
  const auto div = pow(10, 6);
  return static_cast<double>(value)/div;
}

static double
get_aie_part_freq(const std::shared_ptr<xrt_core::device>& device, uint32_t part_id)
{
  double freq = 0;
  try {
    freq = to_megaHz(xrt_core::device_query<qr::aie_get_freq>(device, part_id));
  }
  catch (const std::exception &e) {
    std::cerr << boost::format("ERROR: Failed to read clock frequency of AIE partition(%d)\n %s\n") % part_id % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  return freq;
}

static void
set_aie_part_freq(const std::shared_ptr<xrt_core::device>& device, uint32_t part_id, const std::string& setFreq)
{
  uint64_t freq = 0;
  try {
    //convert freq to hertz(Hz)
    freq = XBUtilities::string_to_base_units(setFreq, XBUtilities::unit::Hertz);
  }
  catch(const xrt_core::error&) {
    std::cerr << "Freq value provided with 'set' option is invalid. Please specify proper units and rerun" << std::endl;
    std::cerr << "eg: 'B', 'K', 'M', 'G' " << std::endl;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Display frequency before setting
  std::cout << boost::format("INFO: Clock frequency of AIE partition(%d) before setting is: %.2f MHz\n") % part_id % get_aie_part_freq(device, part_id);

  // Try to set frequency
  try {
    bool status = xrt_core::device_query<qr::aie_set_freq>(device, part_id, freq);
    if(status) {
      std::cout << boost::format("INFO: Setting clock freq of AIE partition(%d) is successful\n") %  part_id;
      std::cout << boost::format("Running clock freq of AIE partition(%d) is: %.2f MHz\n") % part_id % get_aie_part_freq(device, part_id);
    }
    else
      throw std::runtime_error("AIE driver call to set freq failed");
  }
  catch (const std::exception& e){
    std::cerr << boost::format("ERROR: Setting the AIE partition(%d) clock frequency to %s failed, %s\n") % part_id % setFreq % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }
}

// ----- C L A S S   M E T H O D S -------------------------------------------
OO_AieClockFreq::OO_AieClockFreq( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "AIE clock frequency operations" )
    , m_device("")
    , m_partition_id(1)
    , m_get(false)
    , m_setFreq("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("partition,p", po::value<decltype(m_partition_id)>(&m_partition_id), "The Partition id of AIE")
    ("set,s", po::value<decltype(m_setFreq)>(&m_setFreq), "Frequency value (Hz) to set given AIE partition to (eg: 100K, 312.5M, 5G)")
    ("get,g", po::bool_switch(&m_get), "Read the frequency of given AIE partition")
    ("help,h", po::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
OO_AieClockFreq::execute(const SubCmdOptions& _options) const
{

  XBU::verbose("SubCommand option: AIE Clock");

  XBU::verbose("Option(s):");
  for (auto & aString : _options)
    XBU::verbose(" " + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;
  process_arguments(vm, _options);

  // Exit if neither action or device specified
  if(m_help || m_device.empty()) {
    printHelp();
    return;
  }

  // Check if set/get is used
  if(!m_get && m_setFreq.length() == 0) {
    std::cerr << "ERROR: Missing 'set' or 'get' option" << std::endl;
    std::cerr << "please use any one of set/get and rerun" << std::endl;
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Check if partition_id is provided else print Warning!
  if(!vm.count("partition"))
      std::cout << "WARNING: 'partition' option is not provided, using default partition id value '1'" << std::endl;

  // Find device of interest
  std::shared_ptr<xrt_core::device> device;

  try {
    device = XBU::get_device(boost::algorithm::to_lower_copy(m_device), true /*inUserDomain*/);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Do operations on the device collected
  if(m_get) {
    double freq_part = get_aie_part_freq(device, m_partition_id);
    std::cout << boost::format("INFO: Clock frequency of AIE partition(%d) is: %.2f MHz\n") % m_partition_id % freq_part ;
    return;
  }

  if(!m_setFreq.empty())
    set_aie_part_freq(device, m_partition_id, m_setFreq);
}
