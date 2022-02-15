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
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "OO_AieClockFreq.h"
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

#define to_mega(x) x/1000000

// ----- C L A S S   M E T H O D S -------------------------------------------
OO_AieClockFreq::OO_AieClockFreq( const std::string &_longName, bool _isHidden )
    : OptionOptions(_longName, _isHidden, "Set/Get AIE Clock frequency" )
    , m_device("")
    , m_partition_id(1)
    , m_get(false)
    , m_freq("")
    , m_help(false)
{
  m_optionsDescription.add_options()
    ("device,d", po::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("partition,p", po::value<decltype(m_partition_id)>(&m_partition_id), "The Partition id of aie to set/get frequency")
    ("set,s", po::value<decltype(m_freq)>(&m_freq), "Set frequency value (hertz (Hz)) for given aie partition with units (eg: 100K, 312.5M, 5G)")
    ("get,g", po::bool_switch(&m_get), "Get frequency for given aie partition")
    ("help,h", po::bool_switch(&m_help), "Help to use this sub-command")
  ;
}

void
OO_AieClockFreq::execute(const SubCmdOptions& _options) const
{

  XBU::verbose("SubCommand option: clock");

  XBU::verbose("Option(s):");
  for (auto & aString : _options)
    XBU::verbose(std::string(" ") + aString);

  // Honor help option first
  if (std::find(_options.begin(), _options.end(), "--help") != _options.end()) {
    printHelp();
    return;
  }

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).positional(m_positionalOptions).run(), vm);
    po::notify(vm); // Can throw
  }
  catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    printHelp();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Exit if neither action or device specified
  if(m_help || m_device.empty()) {
    printHelp();
    return;
  }

  // Check if set/get is used
  if(!m_get && m_freq.length() == 0) {
    std::cerr << "ERROR: Neither `set` nor `get' is used" << std::endl;
    std::cerr << "please use any one of set/get and rerun" << std::endl;
    printHelp();
    return;
  }

  // Check if partition_id is provided else print Warning!
  if(!vm.count("partition"))
      std::cout << "WARNING: `partition` option is not provided, using default partition id value '1'" << std::endl;

  // Collect all of the devices of interest
  std::set<std::string> deviceNames;
  xrt_core::device_collection deviceCollection;
  deviceNames.insert(boost::algorithm::to_lower_copy(m_device));
  
  try {
    XBU::collect_devices(deviceNames, true /*inUserDomain*/, deviceCollection);
  } catch (const std::runtime_error& e) {
    // Catch only the exceptions that we have generated earlier
    std::cerr << boost::format("ERROR: %s\n") % e.what();
    throw xrt_core::error(std::errc::operation_canceled);
  }

  bool errorOccured = false;
  for (auto& device : deviceCollection) {
    try {
        if(m_get) {
            double freq_part = static_cast<double>(to_mega(xrt_core::device_query<qr::aie_get_freq>(device, m_partition_id)));
            std::cout << boost::format("INFO: Frequency value of aie partition_id %d is : %lu MHz\n") % m_partition_id % freq_part ;
        }
        if(m_freq.length() > 0) {
            uint64_t freq = 0;
            try {
		//convert freq to hertz(Hz)
                freq = XBUtilities::string_to_bytes(m_freq);
            }
            catch(const xrt_core::error&) {
                std::cerr << "Freq value provided with `set` option is invalid. Please specify proper units and rerun" << std::endl;
                std::cerr << "eg: 'B', 'K', 'M', 'G' " << std::endl;
                throw xrt_core::error(std::errc::operation_canceled);
            }

            xrt_core::device_query<qr::aie_set_freq_req>(device, m_partition_id, freq) ? 
            std::cout << boost::format("INFO: Frequency request for aie partition_id %d is submitted successfully\n") %  m_partition_id :
            std::cout << boost::format("INFO: Frequency request submission for aie partition_id %d failed\n") %  m_partition_id;
        }
    } catch (const std::exception& e){
      std::cerr << boost::format("ERROR: %s\n") % e.what();
      errorOccured = true;
    }
  }

  if (errorOccured)
    throw xrt_core::error(std::errc::operation_canceled);

}
