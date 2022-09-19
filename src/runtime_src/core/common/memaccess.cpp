/**
 * Copyright (C) 2016-2021 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
 *
 * Simple command line utility to interact with PCIe devices
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

// System includes
#include <iostream>
#include <numeric>
#include <vector>

// Local includes
#include "memalign.h"
#include "query_requests.h"
#include "utils.h"
#include "core/common/unistd.h"

namespace {
// Holds the parsed data from the memory topology object
struct mem_bank_t
{
  uint64_t m_base_address;
  uint64_t m_size;
  std::string m_tag;
  mem_bank_t(const struct mem_data& data)
  : m_base_address(data.m_base_address)
    , m_size(data.m_size * 1024) // In memory topology struct size is stored as KB. We convert to bytes for easy referencing
    , m_tag(reinterpret_cast<const char*>(data.m_tag))
  {}
};

// Get all the DDR banks that are in use within the device
static std::vector<mem_bank_t>
get_ddr_banks(const xrt_core::device* device)
{
  std::vector<mem_bank_t> banks;

  auto mt_raw = xrt_core::device_query<xrt_core::query::mem_topology_raw>(device);
  auto map = reinterpret_cast<const mem_topology*>(mt_raw.data());

  // Iterate through all the existing memory banks. If they
  // are in use and not streaming types add store the relevant information
  std::for_each(map->m_mem_data, map->m_mem_data + map->m_count,
    [&banks](const auto& mem) {
      if (mem.m_used && (mem.m_type != MEM_STREAMING))
        banks.emplace_back(mem);
    });

  // Sort banks based on their starting address
  // This is useful later on for processing
  std::sort (banks.begin(), banks.end(),
    [] (const mem_bank_t& a, const mem_bank_t& b) {
      return a.m_base_address < b.m_base_address;
    });

  return banks;
}

static uint64_t
get_starting_address(const std::vector<mem_bank_t>& vec_banks, const uint64_t start_addr)
{
  auto valid_bank =  std::find_if(vec_banks.begin(), vec_banks.end(),
    [](const mem_bank_t& item) {
      return item.m_size;
    });

  if (valid_bank == vec_banks.end())
    throw xrt_core::error(std::errc::operation_canceled, "ERROR: Couldn't find valid memory banks");

  // If start address is 0 choose a start address of the first available memory bank
  // The first available memory bank may not have a base address of 0
  if (start_addr == 0)
    return valid_bank->m_base_address;
  
  return start_addr;
}

static std::vector<mem_bank_t>::iterator
get_starting_bank(std::vector<mem_bank_t>& vec_banks, const uint64_t start_addr)
{
  // Sanity check start address
  auto start_bank = std::find_if(vec_banks.begin(), vec_banks.end(),
    [=](const mem_bank_t& item) {
      return ((start_addr >= item.m_base_address) && (start_addr < (item.m_base_address+item.m_size)));
    });

  if (start_bank == vec_banks.end()) {
    auto error_msg = boost::format("Start address 0x%x is not valid") % start_addr;
    throw xrt_core::error(std::errc::operation_canceled, error_msg.str());
  }

  return start_bank;
}

static uint64_t
get_available_memory_size(std::vector<mem_bank_t>& vec_banks, std::vector<mem_bank_t>::iterator& start_bank, const uint64_t start_addr) 
{
  // Validate the amount of accessable memory
  uint64_t available_size = std::accumulate(start_bank, vec_banks.end(), (uint64_t)0,
    [](uint64_t result, const mem_bank_t& obj) {
      return result + obj.m_size;
    }) ;

  available_size -= start_addr - start_bank->m_base_address;

  return available_size;
}

enum class operation_type {
  read,
  write
};

// Ensure safe access into a device's memory banks based on memory
// bank boundary and if the bank is in use
static void 
perform_memory_action(xrt_core::device* device, xrt_core::aligned_ptr_type& buf, const uint64_t start_addr, const uint64_t size, operation_type action)
{
  auto vec_banks = get_ddr_banks(device);
  auto validated_start_addr = get_starting_address(vec_banks, start_addr);
  auto start_bank = get_starting_bank(vec_banks, validated_start_addr);
  auto available_size = get_available_memory_size(vec_banks, start_bank, validated_start_addr);

  // Validate the size of the memory operation
  if (size > available_size) {
    auto err_msg = boost::format("Cannot access %d bytes of memory from start address 0x%x\n") % size % start_addr;
    throw xrt_core::error(std::errc::operation_canceled, err_msg.str());
  }

  auto validated_size = size;
  // If no size is specified for the read. Read all available memory
  if ((size == 0) && (action == operation_type::read))
    validated_size = available_size;

  uint64_t current_addr = validated_start_addr;
  size_t remaining_bytes_to_see = size;
  size_t bytes_seen = 0;

  // continue to operate as long as there are bytes left to see or we run out of banks
  for (auto it = start_bank; (it != vec_banks.end()) && (remaining_bytes_to_see > 0); ++it) {
    // Validate the amount of memory the current memory bank has
    uint64_t available_bank_size = 0;
    if (it != start_bank) {
      current_addr = it->m_base_address;
      available_bank_size = it->m_size;
    }
    else 
      available_bank_size = it->m_size - (current_addr - it->m_base_address);

    // If the available bank size is less than the source buffer see what bytes we are able to and move to the next bank
    uint64_t bytes_to_edit = std::min(available_bank_size, validated_size);

    // Update the buffer index based on how far we have read
    void* current_buffer_location = static_cast<char *>(buf.get()) + bytes_seen;
    boost::format err_fmt("%s: Code : %d - %s %u bytes from %s(0x%x)");
    err_fmt % __func__;
    switch (action) {
      case operation_type::read:
        try {
          device->unmgd_pread(current_buffer_location, bytes_to_edit, validated_start_addr);
        } catch (const std::exception&) {
          const auto err_msg = err_fmt % errno % "reading" % bytes_to_edit % it->m_tag % current_addr;
          throw xrt_core::error(std::errc::operation_canceled, err_msg.str());
        }
        break;
      case operation_type::write:
        try {
          device->unmgd_pwrite(current_buffer_location, bytes_to_edit, validated_start_addr);
        } catch (const std::exception&) {
          const auto err_msg = err_fmt % errno % "writing" % bytes_to_edit % it->m_tag % current_addr;
          throw xrt_core::error(std::errc::operation_canceled, err_msg.str());
        }
        break;
    }
    remaining_bytes_to_see -= bytes_to_edit;
    bytes_seen += bytes_to_edit;
  }

  if (remaining_bytes_to_see > 0) {
    auto err_msg = boost::format("Warning: Saw %llu bytes. Requested %llu bytes") % bytes_seen % size;
    throw std::runtime_error(err_msg.str());
  }
}

} // Empty namespace

namespace xrt_core {

std::vector<char>
device_mem_read(device* device, const uint64_t start_addr, const uint64_t size)
{
  // Allocate a buffer to hold the read data
  auto buf = xrt_core::aligned_alloc(xrt_core::getpagesize(), size);
  if (!buf)
    throw std::runtime_error("read_banks: Failed to allocate aligned buffer");
  std::memset(buf.get(), 0, size);

  // Read from the device
  perform_memory_action(device, buf, start_addr, size, operation_type::read);

  // Format the read data into the return object
  std::vector<char> data(size);
  std::memcpy(data.data(), buf.get(), data.size());
  return data;
}

void
device_mem_write(device* device, const uint64_t start_addr, const std::vector<char>& src) 
{
  // Prepare the data to write
  auto buf = xrt_core::aligned_alloc(xrt_core::getpagesize(), src.size());
  if (!buf)
    throw std::runtime_error("write_banks: Failed to allocate aligned buffer");
  std::memcpy(buf.get(), src.data(), src.size());

  // Write to the device
  perform_memory_action(device, buf, start_addr, src.size(), operation_type::write);
}

} // xrt_core namespace
