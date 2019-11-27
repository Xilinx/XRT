/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include "XBUtilities.h"
namespace XBU = XBUtilities;

// 3rd Party Library - Include Files
#include <boost/program_options.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include "xclbin.h"
//#include "core/pcie/common/dmatest.h" //TODO: Enable it for DMATest
#include "common/device_core.h"

// ======= R E G I S T E R   T H E   S U B C O M M A N D ======================
#include "SubCmd.h"
static const unsigned int registerResult = 
                    register_subcommand("dmatest", 
                                        "Runs a DMA test on a given device",
                                        subCmdDmaTest);
// =============================================================================

// ------ L O C A L   F U N C T I O N S ---------------------------------------




// ------ F U N C T I O N S ---------------------------------------------------

int subCmdDmaTest(const std::vector<std::string> &_options)
// References: dmatest [-d card] [-b [0x]block_size_KB]
//             Run DMA test on card 1 with 32 KB blocks of buffer
//               xbutil dmatest -d 1 -b 0x2000
{
  XBU::verbose("SubCommand: dmatest");
  // -- Retrieve and parse the subcommand options -----------------------------
  uint64_t card = 0;
  uint64_t blockSizeKB = 0;
  std::string sBlockSizeKB;
  bool help = false;

  po::options_description dmaTestDesc("dmatest options");
  dmaTestDesc.add_options()
    ("help", boost::program_options::bool_switch(&help), "Help to use this sub-command")
    (",d", boost::program_options::value<uint64_t>(&card), "Card to be examined")
    (",b", boost::program_options::value<std::string>(&sBlockSizeKB), "Block Size KB")
  ;

  // Parse sub-command ...
  po::variables_map vm;

  try {
    po::store(po::command_line_parser(_options).options(dmaTestDesc).run(), vm);
    po::notify(vm); // Can throw
  } catch (po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cerr << dmaTestDesc << std::endl;

    // Re-throw exception
    throw;
  }

  // Check to see if help was requested or no command was found
  if (help == true)  {
    std::cout << dmaTestDesc << std::endl;
    return 0;
  }

  // -- Now process the subcommand --------------------------------------------
  blockSizeKB = stoi(sBlockSizeKB);

  XBU::verbose(XBU::format("      Card: %ld", card));
  XBU::verbose(XBU::format("Block Size: 0x%lx", blockSizeKB));


  if (blockSizeKB & (blockSizeKB - 1)) {
	  std::cerr << "ERROR: block size should be power of 2\n";
	  return -1;
  }

  if (blockSizeKB > 0x100000) {
	  std::cerr << "ERROR: block size cannot be greater than 0x100000 MB\n";
	  return -1;
  }

  blockSizeKB *= 1024; // convert kilo bytes to bytes

  if (blockSizeKB == 0)
	  blockSizeKB = 256 * 1024 * 1024; // Default block size, 256MB

  //TODO:1:
  // Get the handle to the devices
  const xrt_core::device_core &CoreDevice = xrt_core::device_core::instance();

  size_t ddr_mem_size = CoreDevice.get_ddr_mem_size(card);
  if (ddr_mem_size == -EINVAL)
	  return -EINVAL;

  std::cout << "Total DDR size: " << ddr_mem_size << " MB" << std::endl;
  std::cout << "DMATest TBD ..." << std::endl;
#if 0
  int result = 0;
  unsigned long long addr = 0x0;
  unsigned long long sz = 0x1;
  unsigned int pattern = 'J';
  size_t blockSize = 4096;

  // get DDR bank count from mem_topology if possible
  std::vector<char> buf;
  //dev->sysfs_get("icap", "mem_topology", errmsg, buf);
  //TODO:2:
  CoreDevice.get_mem_topology(card, buf);

  const mem_topology *map = (mem_topology *)buf.data();
  if(buf.empty() || map->m_count == 0) {
	  std::cout << "WARNING: 'mem_topology' invalid, "
		  << "unable to perform DMA Test. Has the bitstream been loaded? "
		  << "See 'xbutil program' to load a specific xclbin file or run "
		  << "'xbutil validate' to use the xclbins provided with this card."
		  << std::endl;
	  return -EINVAL;
  }

  std::cout << "Reporting from mem_topology:" << std::endl;

  for(int32_t i = 0; i < map->m_count; i++) {
	  if(map->m_mem_data[i].m_type == MEM_STREAMING)
		  continue;

	  if(map->m_mem_data[i].m_used) {
		  std::cout << "Data Validity & DMA Test on "
				  << map->m_mem_data[i].m_tag << "\n";

		  //What exactly is the use of this for loop()??
		  //I think it is for read/write tests on DDR banks?
		  //mem*() APIs are implemented in runtime_src/core/pcie/common/memaccess.h
		  /*
		  addr = map->m_mem_data[i].m_base_address;
		  for(unsigned sz = 1; sz <= 256; sz *= 2) {
			//TODO:3:
			  result = memwriteQuiet(addr, sz, pattern);
			  if( result < 0 )
				  return result;
			//TODO:4:
			  result = memreadCompare(addr, sz, pattern , false);
			  if( result < 0 )
				  return result;
		  }
		  */
			//TODO:5:DMARunner() present in runtime_src/core/pcie/common/dmatest.h
			//blockSize = 4096
		  DMARunner runner( m_handle, blockSize, i);
		  result = runner.run();
	  }
  }
  if (result != 0)
	  std::cout << "ERROR: xbutil validate failed." << std::endl;
  else
	  std::cout << "INFO: xbutil validate(dmatest) succeeded." << std::endl;
#endif

  return registerResult;
}

