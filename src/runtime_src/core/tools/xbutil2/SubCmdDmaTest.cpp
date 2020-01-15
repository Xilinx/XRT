/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include "SubCmdDmaTest.h"
#include "common/system.h"
#include "common/device.h"
#include "core/pcie/common/dmatest.h"

#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
#include <boost/range/iterator_range.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <iterator>

// =============================================================================
namespace {

static void
dmatest(const std::shared_ptr<xrt_core::device>& device, size_t block_size, bool verbose)
{
  if (block_size == 0)
      block_size = 256 * 1024 * 1024; // Default block size

  auto ddr_mem_size = xrt_core::query_device<uint64_t>(device, xrt_core::device::QR_ROM_DDR_BANK_SIZE);

  if (verbose)
    std::cout << "Total DDR size: " << ddr_mem_size << " MB\n";

  // get DDR bank count from mem_topology if possible
  auto membuf = xrt_core::query_device<std::vector<char>>(device, xrt_core::device::QR_MEM_TOPOLOGY_RAW);
  auto mem_topo = reinterpret_cast<const mem_topology*>(membuf.data());
  if (membuf.empty() || mem_topo->m_count == 0)
    throw std::runtime_error
      ("WARNING: 'mem_topology' invalid, unable to perform DMA Test. Has the "
       "bitstream been loaded?  See 'xbutil program' to load a specific "
       "xclbin file or run 'xbutil validate' to use the xclbins "
       "provided with this card.");

  if (verbose)
    std::cout << "Reporting from mem_topology:" << std::endl;

  //for (auto itrint32_t i = 0; i < map->m_count; i++) {
  for (auto& mem : boost::make_iterator_range(mem_topo->m_mem_data, mem_topo->m_mem_data + mem_topo->m_count)) {
    auto midx = std::distance(mem_topo->m_mem_data, &mem);
    if (mem.m_type == MEM_STREAMING)
      continue;

    if (!mem.m_used)
      continue;

    if (verbose)
      std::cout << "Data Validity & DMA Test on " << mem.m_tag << "\n";

    for(unsigned int sz = 1; sz <= 256; sz *= 2) {
#if 0
      auto result = memwriteQuiet(mem.m_base_address, sz, 'J');
      if (result < 0)
        throw xrt_core::error(result, "DMATest failed mem write");
      result = memreadCompare(mem.m_base_address, sz, 'J', false);
      if (result < 0)
        throw xrt_core::error(result, "DMATest failed mem write");
#endif
    }

    xcldev::DMARunner runner(device->get_device_handle(), block_size, static_cast<unsigned int>(midx));
    if (int ret = runner.run())
      throw xrt_core::error(ret,"DMATest failed");

  }
}

} // namespace


// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdDmaTest::SubCmdDmaTest(bool _isHidden, bool _isDepricated, bool _isPreliminary)
    : SubCmd("dmatest", 
             "Runs a DMA test on a given device")
{
  const std::string longDescription = "<add long description>";
  setLongDescription(longDescription);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
}

void
SubCmdDmaTest::execute(const SubCmdOptions& _options) const
// References: dmatest [-d card] [-b [0x]block_size_KB]
//             Run DMA test on card 1 with 32 KB blocks of buffer
//               xbutil dmatest -d 1 -b 0x2000
{
  XBU::verbose("SubCommand: dmatest");
  // -- Retrieve and parse the subcommand options -----------------------------
  xrt_core::device::id_type card = 0;
  std::string sBlockSizeKB;
  bool help = false;

  po::options_description dmaTestDesc("dmatest options");
  dmaTestDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<decltype(card)>(&card), "Card to be examined")
    (",b", boost::program_options::value<std::string>(&sBlockSizeKB), "Block Size KB")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(dmaTestDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    printHelp(dmaTestDesc);

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    printHelp(dmaTestDesc);
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  auto device = xrt_core::get_userpf_device(card);
  auto block_size = std::strtoll(sBlockSizeKB.c_str(), nullptr, 0);
  bool verbose = true;
  dmatest(device, block_size, verbose);
}
