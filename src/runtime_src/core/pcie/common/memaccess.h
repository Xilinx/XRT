/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Author: Sonal Santan
 * Simple command line utility to inetract with SDX PCIe devices
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
#ifndef MEMACCESS_H
#define MEMACCESS_H

#include "core/pcie/common/dmatest.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <numeric>

#include <cstring>
#include <cstddef>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "core/common/memalign.h"
#include "core/common/utils.h"
#include "core/common/device.h"

#include "xclbin.h"

namespace xrt_core {
  class memaccess {
    uint64_t mDDRSize;
    uint64_t mDataAlignment;
  public:
    memaccess(uint64_t aDDRSize, uint64_t aDataAlignment) :
              mDDRSize(aDDRSize), mDataAlignment (aDataAlignment) {}

    struct mem_bank_t {
      uint64_t m_base_address;
      uint64_t m_size;
      int m_index;
      uint8_t m_type;
      mem_bank_t (uint64_t aAddr, uint64_t aSize, int aIndex, uint8_t aType) : m_base_address(aAddr), m_size(aSize), m_index(aIndex), m_type(aType) {}
    };

    std::map <int, std::string> bankEnumStringMap = {
      { MEM_DDR3, "DDR3" },
      { MEM_DDR4, "DDR4" },
      { MEM_DRAM, "DRAM" },
      { MEM_STREAMING, "MEM_STREAMING" },
      { MEM_PREALLOCATED_GLOB, "MEM_PREALLOCATED_GLOB" },
      { MEM_ARE, "MEM_ARE" },
      { MEM_HBM, "HBM" },
      { MEM_BRAM, "BRAM" },
      { MEM_URAM, "URAM" },
      { MEM_STREAMING_CONNECTION, "MEM_STREAMING_CONNECTION" },
      { MEM_HOST, "MEM_HOST" },
    };

    /*
     * getDDRBanks()
     *
     * Get the addrs and size of each DDR bank
     * Sort the vector based on start address.
     * Returns number of banks.
     */
    std::vector<mem_bank_t> getDDRBanks(const device* device)
    {
      std::vector<mem_bank_t> aBanks;

      auto mt_raw = xrt_core::device_query<xrt_core::query::mem_topology_raw>(device);
      auto map = reinterpret_cast<const mem_topology*>(mt_raw.data());

      for( int i = 0; i < map->m_count; i++ ) {
          // If a memory bank is in use and is not a streaming bank emplace it into the bank list
          if( map->m_mem_data[i].m_used && map->m_mem_data[i].m_type != MEM_STREAMING )
              aBanks.emplace_back( map->m_mem_data[i].m_base_address, map->m_mem_data[i].m_size*1024, i, map->m_mem_data[i].m_type );
      }

      std::sort (aBanks.begin(), aBanks.end(),
                  [] (const mem_bank_t& a, const mem_bank_t& b) {return (a.m_base_address < b.m_base_address);});

      return aBanks;
    }

    /*
     * readBank()
     *
     * Read from specified address, specified size within a bank
     * Caller's responsibility to do sanity checks. No sanity checks done here
     */
    void readBank(const device* device, std::ofstream& aOutFile, uint64_t aStartAddr, uint64_t aSize) 
    {
      // Allocate a buffer to hold the read data
      auto buf = xrt_core::aligned_alloc(getpagesize(), aSize);
      if (!buf)
        throw std::runtime_error("readBank: Failed to allocate aligned buffer");
      std::memset(buf.get(), 0, aSize);

      // Read the data in from the device
      auto guard = xrt_core::utils::ios_restore(std::cout);
      // We are given only the status not the number of bytes read
      if (xclUnmgdPread(device->get_device_handle(), 0, buf.get(), aSize, aStartAddr) < 0) {
        std::cerr << boost::format("ERROR: (%s) reading 0x%x bytes from DDR/HBM/PLRAM at offset 0x%x\n") % strerror(errno) % aSize % aStartAddr;
        throw xrt_core::error(std::errc::operation_canceled);
      }

      // Write the received data into the output file
      aOutFile.write(reinterpret_cast<const char*>(buf.get()), aSize);
      if ((aOutFile.rdstate() & std::ifstream::failbit) != 0)
        throw std::runtime_error("readBank: Error writing to output file\n");

      std::cout << boost::format("INFO: Read size 0x%x bytes from address 0x%x\n") % aSize % aStartAddr;
    }

    /*
     * readWriteHelper()
     *
     * Sanity check the user's Start Address and Size against the mem topology
     * If the start address is 0 (ie. unspecified by user) change it to the first available address
     * If the size is 0 (ie. unspecified by user) change it to the maximum available size
     * Fill the vector with the available banks
     * Set the iterator to the bank containing the start address
     * returns the number of banks the start address and size going to span
     * return -1 in case of any sanity check failures
     */
    int readWriteHelper (const device* device, uint64_t& aStartAddr, uint64_t& aSize,
                std::vector<mem_bank_t>& vec_banks, std::vector<mem_bank_t>::iterator& startbank) 
    {
      vec_banks = getDDRBanks(device);
      //Find the first memory bank with valid size since vec_banks is sorted
      auto validBank =  std::find_if(vec_banks.begin(), vec_banks.end(),
                  [](const mem_bank_t item) {return (item.m_size);});

      if (validBank == vec_banks.end()) {
        std::cerr << "ERROR: Couldn't find valid memory banks\n";
        throw xrt_core::error(std::errc::operation_canceled);
      }

      //if given start address is 0 then choose start address to be the lowest address available
      uint64_t startAddr = (aStartAddr == 0) ? validBank->m_base_address : aStartAddr;
      // Update reference
      aStartAddr = startAddr;

      //Sanity check start address
      startbank = std::find_if(vec_banks.begin(), vec_banks.end(),
                  [startAddr](const mem_bank_t& item) {return (startAddr >= item.m_base_address && startAddr < (item.m_base_address+item.m_size));});

      if (startbank == vec_banks.end()) {
        std::cerr << boost::format("ERROR: Start address 0x%x is not valid\n") % startAddr;
        throw xrt_core::error(std::errc::operation_canceled);
      }

      //Sanity check access size
      uint64_t availableSize = std::accumulate(startbank, vec_banks.end(), (uint64_t)0,
              [](uint64_t result, const mem_bank_t& obj) {return (result + obj.m_size);}) ;

      availableSize -= (startAddr - startbank->m_base_address);
      if (aSize > availableSize) {
        std::cerr << boost::format("ERROR: Cannot access %d bytes of memory from start address 0x%x\n") % aSize % startAddr;
        throw xrt_core::error(std::errc::operation_canceled);
      }

      //if given size is 0, then the end Address is the max address of the unused bank
      uint64_t size = (aSize == 0) ? availableSize : aSize;
      // Update reference
      aSize = size;

      //Find the number of banks this read/write straddles, this is just for better messaging
      int bankcnt = 0;
      for(auto it = startbank; it!=vec_banks.end(); ++it) {
        uint64_t available_bank_size;
        if (it != startbank)
          available_bank_size = it->m_size;
        else
          available_bank_size = it->m_size - (startAddr - it->m_base_address);

        if (size != 0) {
          uint64_t accesssize = (size > available_bank_size) ? (uint64_t) available_bank_size : size;
          ++bankcnt;
          size -= accesssize;
        }
        else {
          break;
        }
      }
      return bankcnt;
    }

    int read(const device* device, std::string aFilename, uint64_t aStartAddr = 0, uint64_t aSize = 0)
    {
      std::vector<mem_bank_t> vec_banks;
      uint64_t startAddr = aStartAddr;
      uint64_t size = aSize;
      std::vector<mem_bank_t>::iterator startbank;
      int bankcnt = 0;

      //Sanity check the address and size against the mem topology
      if ((bankcnt = readWriteHelper(device, startAddr, size, vec_banks, startbank)) == -1) {
        return -1;
      }

      std::ofstream outFile(aFilename, std::ofstream::out | std::ofstream::binary | std::ofstream::app);

      size_t count = size;
      for(auto it = startbank; it!=vec_banks.end(); ++it) {
        uint64_t available_bank_size;
        if (it != startbank) {
          startAddr = it->m_base_address;
          available_bank_size = it->m_size;
        }
        else 
          available_bank_size = it->m_size - (startAddr - it->m_base_address);

        if (size != 0) {
          if (it->m_type > bankEnumStringMap.size()) {
            std::cout << boost::format("Error: Invalid Bank type (%d) received\n") % it->m_type;
            return -1;
          }
          std::string bank_name = bankEnumStringMap[it->m_type];
          std::cout << boost::format("INFO: Reading %llu bytes from bank %s address 0x%x\n") % size % bankEnumStringMap[it->m_type] % startAddr;
          uint64_t readsize = (size > available_bank_size) ? (uint64_t) available_bank_size : size;
          readBank(device, outFile, startAddr, readsize);
          size -= readsize;
        }
        else {
          break;
        }
      }

      outFile.close();
      std::cout << "INFO: Read data saved in file: " << aFilename << "; Num of bytes: " << std::dec << count-size << " bytes " << std::endl;
      return size;
    }

    int write(const device* device, uint64_t aStartAddr, uint64_t aSize, char *srcBuf) {
      void *buf = 0;
      uint64_t endAddr;
      uint64_t size;
      uint64_t blockSize = aSize; //0x20000;//128KB
      if (xrt_core::posix_memalign(&buf, getpagesize(), blockSize))
        return -1;

      endAddr = aSize == 0 ? mDDRSize : aStartAddr + aSize;
      size = endAddr-aStartAddr;

      // Use plain POSIX open/pwrite/close.
      std::cout << "INFO: Writing DDR/HBM/PLRAM with " << size << " bytes from file, "
                << " from address 0x" << std::hex << aStartAddr << std::dec << std::endl;

      uint64_t count = size;
      uint64_t incr;
      memcpy(buf, srcBuf, aSize);
      for(uint64_t phy=aStartAddr; phy<endAddr; phy+=incr) {
        incr = (count >= blockSize) ? blockSize : count;
        if (xclUnmgdPwrite(device->get_device_handle(), 0, buf, incr, phy) < 0) {
          //error
          std::cout << "Error (" << strerror (errno) << ") writing 0x" << std::hex << incr << " bytes to DDR/HBM/PLRAM at offset 0x" << phy << std::dec << std::endl;
          free(buf);
          return -1;
        }
        count -= incr;
      }

      free(buf);
      if (count != 0) {
        std::cout << "Error! Written " << size-count << " bytes, requested " << size << std::endl;
        return -1;
      }
      return count;
    }
  };
}

#endif /* MEMACCESS_H */
