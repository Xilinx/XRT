/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
 * 
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
#include "core/pcie/common/memaccess.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <numeric>

#include <unistd.h>

#include "core/common/memalign.h"
#include "core/common/utils.h"
#include "core/common/query_requests.h"

#include "core/include/xrt.h"
#include "xclbin.h"

struct mem_bank_t {
  uint64_t m_base_address;
  uint64_t m_size;
  int m_index;
  uint8_t m_type;
  mem_bank_t (uint64_t addr, uint64_t size, int index, uint8_t type) : m_base_address(addr), m_size(size), m_index(index), m_type(type) {}
};

static std::map <int, std::string> bank_enum_string_map = {
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

static uint64_t
get_ddr_mem_size(const xrt_core::device * device)
{
  auto ddr_size = xrt_core::device_query<xrt_core::query::rom_ddr_bank_size_gb>(device);
  auto ddr_bank_count = xrt_core::device_query<xrt_core::query::rom_ddr_bank_count_max>(device);
  
  // convert ddr_size from GB to bytes
  // return the result in KB
  // TODO user xbutil:: convert function here
  return (ddr_size << 30) * ddr_bank_count / (1024 * 1024);
};

static std::vector<mem_bank_t>
get_ddr_banks(const xrt_core::device* device)
{
  std::vector<mem_bank_t> banks;

  auto mt_raw = xrt_core::device_query<xrt_core::query::mem_topology_raw>(device);
  auto map = reinterpret_cast<const mem_topology*>(mt_raw.data());

  for( int i = 0; i < map->m_count; i++ ) {
      // If a memory bank is in use and is not a streaming bank emplace it into the bank list
      if( map->m_mem_data[i].m_used && map->m_mem_data[i].m_type != MEM_STREAMING )
          banks.emplace_back( map->m_mem_data[i].m_base_address, map->m_mem_data[i].m_size*1024, i, map->m_mem_data[i].m_type );
  }

  std::sort (banks.begin(), banks.end(),
              [] (const mem_bank_t& a, const mem_bank_t& b) {return (a.m_base_address < b.m_base_address);});

  return banks;
}

static void
read_banks(const xrt_core::device* device, std::ofstream& output_file, uint64_t start_addr, uint64_t size) 
{
  // Allocate a buffer to hold the read data
  auto buf = xrt_core::aligned_alloc(getpagesize(), size);
  if (!buf)
    throw std::runtime_error("read_banks: Failed to allocate aligned buffer");
  std::memset(buf.get(), 0, size);

  // Read the data in from the device
  auto guard = xrt_core::utils::ios_restore(std::cout);
  // We are given only the status not the number of bytes read
  if (xclUnmgdPread(device->get_device_handle(), 0, buf.get(), size, start_addr) < 0) {
    std::cerr << boost::format("ERROR: (%s) reading 0x%x bytes from DDR/HBM/PLRAM at offset 0x%x\n") % strerror(errno) % size % start_addr;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Write the received data into the output file
  output_file.write(reinterpret_cast<const char*>(buf.get()), size);
  if ((output_file.rdstate() & std::ifstream::failbit) != 0)
    throw std::runtime_error("read_banks: Error writing to output file\n");

  std::cout << boost::format("INFO: Read size 0x%x bytes from address 0x%x\n") % size % start_addr;
}

/*
  * read_write_helper()
  *
  * Sanity check the user's Start Address and Size against the mem topology
  * If the start address is 0 (ie. unspecified by user) change it to the first available address
  * If the size is 0 (ie. unspecified by user) change it to the maximum available size
  * Fill the vector with the available banks
  * Set the iterator to the bank containing the start address
  */
static void
read_write_helper (const xrt_core::device* device, uint64_t& start_addr, uint64_t& size,
            std::vector<mem_bank_t>& vec_banks, std::vector<mem_bank_t>::iterator& start_bank) 
{
  vec_banks = get_ddr_banks(device);
  //Find the first memory bank with valid size since vec_banks is sorted
  auto valid_bank =  std::find_if(vec_banks.begin(), vec_banks.end(),
              [](const mem_bank_t item) {return (item.m_size);});

  if (valid_bank == vec_banks.end()) {
    std::cerr << "ERROR: Couldn't find valid memory banks\n";
    throw xrt_core::error(std::errc::operation_canceled);
  }

  //if given start address is 0 then choose start address to be the lowest address available
  uint64_t validated_start_addr = (start_addr == 0) ? valid_bank->m_base_address : start_addr;
  // Update reference
  start_addr = validated_start_addr;

  //Sanity check start address
  start_bank = std::find_if(vec_banks.begin(), vec_banks.end(),
              [validated_start_addr](const mem_bank_t& item) {return (validated_start_addr >= item.m_base_address && validated_start_addr < (item.m_base_address+item.m_size));});

  if (start_bank == vec_banks.end()) {
    std::cerr << boost::format("ERROR: Start address 0x%x is not valid\n") % validated_start_addr;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  //Sanity check access size
  uint64_t available_size = std::accumulate(start_bank, vec_banks.end(), (uint64_t)0,
          [](uint64_t result, const mem_bank_t& obj) {return (result + obj.m_size);}) ;

  available_size -= (validated_start_addr - start_bank->m_base_address);
  if (size > available_size) {
    std::cerr << boost::format("ERROR: Cannot access %d bytes of memory from start address 0x%x\n") % size % validated_start_addr;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  //if given size is 0, then the end Address is the max address of the unused bank
  uint64_t validate_size = (size == 0) ? available_size : size;
  // Update reference
  size = validate_size;
}

namespace xrt_core {
  int
  device_mem_read(const device* device, std::string filename, uint64_t start_addr, uint64_t size)
  {
    std::vector<mem_bank_t> vec_banks;
    uint64_t current_addr = start_addr;
    std::vector<mem_bank_t>::iterator start_bank;

    //Sanity check the address and size against the mem topology
    read_write_helper(device, current_addr, size, vec_banks, start_bank);

    std::ofstream out_file(filename, std::ofstream::out | std::ofstream::binary | std::ofstream::app);

    size_t count = size;
    for(auto it = start_bank; it!=vec_banks.end(); ++it) {
      uint64_t available_bank_size;
      if (it != start_bank) {
        current_addr = it->m_base_address;
        available_bank_size = it->m_size;
      }
      else 
        available_bank_size = it->m_size - (current_addr - it->m_base_address);

      if (size != 0) {
        if (it->m_type > bank_enum_string_map.size()) {
          std::cout << boost::format("Error: Invalid Bank type (%d) received\n") % it->m_type;
          return -1;
        }
        std::string bank_name = bank_enum_string_map[it->m_type];
        uint64_t read_size = (size > available_bank_size) ? (uint64_t) available_bank_size : size;
        std::cout << boost::format("INFO: Reading %llu bytes from bank %s address 0x%x. %llu bytes remaining.\n") % read_size % bank_enum_string_map[it->m_type] % current_addr % size;
        read_banks(device, out_file, current_addr, read_size);
        size -= read_size;
      }
      else {
        break;
      }
    }

    out_file.close();
    std::cout << boost::format("INFO: Read data saved in file: %s; Number of bytes: %d bytes\n") % filename % (count - size);
    return size;
  }

  int
  device_mem_write(const device* device, uint64_t start_addr, uint64_t size, char *src_buf) {
    void *buf = 0;
    uint64_t block_size = size;
    if (xrt_core::posix_memalign(&buf, getpagesize(), block_size) != 0)
      throw std::runtime_error("device_mem_write: Failed to align memory buffer");

    uint64_t endAddr = (size == 0) ? get_ddr_mem_size(device) : (start_addr + size);
    size = endAddr - start_addr;

    // Use plain POSIX open/pwrite/close.
    std::cout << boost::format("INFO: Writing DDR/HBM/PLRAM with %llu bytes at address 0x%x\n") % size % start_addr;

    uint64_t count = size;
    uint64_t incr;
    memcpy(buf, src_buf, size);
    for (uint64_t phy=start_addr; phy < endAddr; phy+=incr) {
      incr = (count >= block_size) ? block_size : count;
      if (xclUnmgdPwrite(device->get_device_handle(), 0, buf, incr, phy) < 0) {
        std::cerr << boost::format("ERROR: (%s) writing 0x%x bytes to DDR/HBM/PLRAM at offset 0x%x\n") % strerror (errno) % incr % phy;
        free(buf);
        throw xrt_core::error(std::errc::operation_canceled);
      }
      count -= incr;
    }

    free(buf);
    if (count != 0) {
      std::cerr << boost::format("ERROR: Written %llu bytes. Requested %llu bytes") % (size - count) % size;
      throw xrt_core::error(std::errc::operation_canceled);
    }

    return count;
  }
}
