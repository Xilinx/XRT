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
#define XRT_CORE_COMMON_SOURCE
#include "memaccess.h"

#include <iostream>
#include <numeric>
#include <vector>

#include "memalign.h"
#include "query_requests.h"
#include "utils.h"
#include "unistd.h"

// TODO add tag in here for alter use...
struct mem_bank_t {
  uint64_t m_base_address;
  uint64_t m_size;
  int m_index;
  uint8_t m_type;
  mem_bank_t (uint64_t addr, uint64_t size, int index, uint8_t type) : m_base_address(addr), m_size(size), m_index(index), m_type(type) {}
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

/*
  * validate_address_and_size()
  *
  * Sanity check the user's Start Address and Size against the mem topology
  * If the start address is 0 (ie. unspecified by user) change it to the first available address
  * If the size is 0 (ie. unspecified by user) change it to the maximum available size
  * Fill the vector with the available banks
  * Set the iterator to the bank containing the start address
  */
static void
validate_address_and_size(xrt_core::device* device, uint64_t& start_addr, uint64_t& size,
            std::vector<mem_bank_t>& vec_banks, std::vector<mem_bank_t>::iterator& start_bank) 
{
  // Validate the memory banks
  vec_banks = get_ddr_banks(device);
  //Find the first memory bank with valid size since vec_banks is sorted
  auto valid_bank =  std::find_if(vec_banks.begin(), vec_banks.end(),
              [](const mem_bank_t item) {return (item.m_size);});

  if (valid_bank == vec_banks.end()) {
    std::cerr << "ERROR: Couldn't find valid memory banks\n";
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Validate the start address
  // If given start address is 0 choose a start address of the first available memory bank
  // Update reference
  start_addr = (start_addr == 0) ? valid_bank->m_base_address : start_addr;

  //Sanity check start address
  start_bank = std::find_if(vec_banks.begin(), vec_banks.end(),
              [start_addr](const mem_bank_t& item) {return (start_addr >= item.m_base_address && start_addr < (item.m_base_address+item.m_size));});

  if (start_bank == vec_banks.end()) {
    std::cerr << boost::format("ERROR: Start address 0x%x is not valid\n") % start_addr;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // Validate the amount of accessable memory
  uint64_t available_size = std::accumulate(start_bank, vec_banks.end(), (uint64_t)0,
          [](uint64_t result, const mem_bank_t& obj) {return (result + obj.m_size);}) ;

  available_size -= (start_addr - start_bank->m_base_address);
  if (size > available_size) {
    std::cerr << boost::format("ERROR: Cannot access %d bytes of memory from start address 0x%x\n") % size % start_addr;
    throw xrt_core::error(std::errc::operation_canceled);
  }

  // If no size is supplied the end address is the ending address of the last available memory bank
  // Update the input reference to reflect the change
  size = (size == 0) ? available_size : size;
}

enum class OperationType {
  read,
  write
};

static void 
perform_memory_action(xrt_core::device* device, xrt_core::aligned_ptr_type& buf, uint64_t start_addr, uint64_t size, OperationType action)
{
  std::vector<mem_bank_t> vec_banks;
  std::vector<mem_bank_t>::iterator start_bank;

  // Sanity check the address and size against the mem topology
  validate_address_and_size(device, start_addr, size, vec_banks, start_bank);

  uint64_t current_addr = start_addr;
  size_t remaining_bytes_to_see = size;
  size_t bytes_seen = 0;
  // continue to read as long as there are bytes left to read or we run out of banks
  for(auto it = start_bank; (it != vec_banks.end()) && (remaining_bytes_to_see > 0); ++it) {
    // Validate the amount of memory the current memory bank has
    uint64_t available_bank_size = 0;
    if (it != start_bank) {
      current_addr = it->m_base_address;
      available_bank_size = it->m_size;
    }
    else 
      available_bank_size = it->m_size - (current_addr - it->m_base_address);

    // If the available bank size is less than the source buffer write what we are able to and move to the next bank
    uint64_t bytes_to_edit = std::min(available_bank_size, size);

    // TODO Clear all flags in cout?? Why??
    auto guard = xrt_core::utils::ios_restore(std::cout);
    // Update the buffer index based on how far we have read
    void* current_buffer_location = static_cast<char *>(buf.get()) + bytes_seen;
    switch (action) {
      case OperationType::read:
        device->unmgd_pread(current_buffer_location, bytes_to_edit, start_addr);
        break;
      case OperationType::write:
      
        device->unmgd_pwrite(current_buffer_location, bytes_to_edit, start_addr);
        break;
      default:
        throw std::runtime_error("perform_memory_action: Invalid action");
    }
    remaining_bytes_to_see -= bytes_to_edit;
    bytes_seen += bytes_to_edit;
  }

  if (remaining_bytes_to_see > 0)
      throw std::runtime_error(boost::str(boost::format("Warning: Saw %llu bytes. Requested %llu bytes") % bytes_seen % size));
}

namespace xrt_core {
  std::vector<char>
  device_mem_read(device* device, uint64_t start_addr, uint64_t size)
  {
    // Allocate a buffer to hold the read data
    auto buf = xrt_core::aligned_alloc(xrt_core::getpagesize(), size);
    if (!buf)
      throw std::runtime_error("read_banks: Failed to allocate aligned buffer");
    std::memset(buf.get(), 0, size);

    // Read from the device
    perform_memory_action(device, buf, start_addr, size, OperationType::read);

    // Format the read data into the return object
    std::vector<char> data(size);
    std::memcpy(data.data(), buf.get(), size);
    return data;
  }

  void
  device_mem_write(device* device, uint64_t start_addr, std::vector<char>& src) 
  {
    // Prepare the data to write
    auto buf = xrt_core::aligned_alloc(xrt_core::getpagesize(), src.size());
    if (!buf)
      throw std::runtime_error("write_banks: Failed to allocate aligned buffer");
    std::memcpy(buf.get(), src.data(), src.size());

    // Write to the device
    perform_memory_action(device, buf, start_addr, src.size(), OperationType::write);
  }
}
