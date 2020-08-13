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
#include "OO_Clock.h"
#include "tools/common/XBUtilities.h"
#include "core/common/device.h"
#include "core/common/query_requests.h"
#include "xclbin.h"
namespace XBU = XBUtilities;


// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/algorithm/string/join.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <algorithm>
#include <system_error>

namespace {

static std::vector<uint16_t>
clock_freqs(const xrt_core::device* device)
{
  std::vector<uint16_t> freqs_mhz;

  auto freqs_str = xrt_core::device_query<xrt_core::query::clock_freqs_mhz>(device);
  std::transform(freqs_str.begin(), freqs_str.end(), std::back_inserter(freqs_mhz),
                 [](auto& freq_str_mhz) { return static_cast<uint16_t>(std::stoul(freq_str_mhz)); });
  return freqs_mhz;
}

static std::vector<std::string>
clock_names(const clock_freq_topology* cft)
{
  std::vector<std::string> names;
  auto end = cft->m_clock_freq + cft->m_count;
  std::transform(cft->m_clock_freq, end, std::back_inserter(names),
                 [](auto& cf) { return cf.m_name; });
  return names;
}

static size_t
clock_index_or_throw(const clock_freq_topology* cft, const std::string& clock)
{
  auto end = cft->m_clock_freq + cft->m_count;
  auto itr = std::find_if(cft->m_clock_freq, end,
       [&clock](auto& cf) {
          return strncmp(clock.c_str(), cf.m_name, clock.size()) == 0;
       });

  if (itr != end)
    return std::distance(cft->m_clock_freq, itr);

  // Throw message with available clocks
  auto clocks = clock_names(cft);
  std::string msg = "No such clock '" + clock + "'.  Available clocks are ";
  msg.append(boost::join(clocks, ", "));
  throw xrt_core::system_error(EINVAL, msg);
}

static void
data_retention_and_error(const xrt_core::device* device)
{
  if (xrt_core::device_query<xrt_core::query::data_retention>(device))
    throw xrt_core::system_error(EPERM, "Data retention is enabled, can't change clock frequency");
}

static void
reclock(xrt_core::device* device, const std::string& clock, uint16_t freq)
{
  XBU::sudo_or_throw("Reclocking requires sudo");
  XBU::can_proceed_or_throw("Memory data may be lost after xbutil clock","");
  data_retention_and_error(device);

  auto raw = xrt_core::device_query<xrt_core::query::clock_freq_topology_raw>(device);
  auto cft = reinterpret_cast<const clock_freq_topology*>(raw.data());
  if (!cft)
    throw std::runtime_error("No clocks to change, make sure xclbin is loaded");

  auto idx = clock_index_or_throw(cft, clock);

  auto freqs = clock_freqs(device);
  if (freqs.size() <= idx)
    throw std::runtime_error("Unexpected error: xclbin clock mismatch");

  freqs[idx] = freq;
  device->reclock(freqs.data());
}

} // namespace

// ----- C L A S S   M E T H O D S -------------------------------------------

OO_Clock::OO_Clock( const std::string &_longName, bool _isHidden)
    : OptionOptions(_longName, _isHidden, "Set the frequency on the named clock")
    , m_device("")
    , m_clockName("")
    , m_clockFreq("")
    , m_help(false)
{
  setExtendedHelp("A list of available clocks can be found in the 'examine' command.");

  m_optionsDescription.add_options()
    ("name", boost::program_options::value<decltype(m_clockName)>(&m_clockName)->required(), "Name of the clock")
    ("frequency", boost::program_options::value<decltype(m_clockFreq)>(&m_clockFreq)->required(), "Frequency to set the clock to")
    ("device,d", boost::program_options::value<decltype(m_device)>(&m_device), "The Bus:Device.Function (e.g., 0000:d8:00.0) device of interest")
    ("help,h", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  m_positionalOptions.
    add("name", 1 /* max_count */).
    add("frequency", 1 /* max_count */)
  ;
}

void
OO_Clock::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand option: clock");

  XBU::verbose("Option(s):");
  for (auto & aString : _options)
    XBU::verbose(std::string(" ") + aString);

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(m_optionsDescription).positional(m_positionalOptions).run(), vm);
    po::notify(vm); // Can throw
  }
  catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << "\n\n";
    printHelp();
    throw; // Re-throw exception
  }

  //exit if neither daemon or device is specified
  if(m_help || (m_device.empty() && m_device.empty())) {
    printHelp();
    return;
  }

  // Change frequency for specified clock
  for (auto& device : XBU::collect_devices(m_device, true))
    reclock(device.get(), m_clockName, static_cast<uint16_t>(stoul(m_clockFreq)));
}
