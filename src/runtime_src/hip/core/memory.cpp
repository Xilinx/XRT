// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#ifdef _WIN32
#include <Windows.h>
#include <Memoryapi.h>
#else
#include <sys/mman.h>
#endif

#include "common.h"
#include "device.h"
#include "hip/config.h"
#include "hip/hip_runtime_api.h"
#include "memory.h"

namespace xrt::core::hip
{

  memory::memory(device* dev, size_t sz)
      : m_device(dev),
	m_size(sz),
	m_type(memory_type::device),
	m_flags(0)
  {
    assert(m_device);

    // TODO: support non-npu device that may require delayed xrt::bo allocation until xrt kernel is created
    init_xrt_bo();
  }

  memory::memory(device* dev, size_t sz, void *host_mem, unsigned int flags)
      : m_device(dev), m_size(sz), m_type(memory_type::registered), m_flags(flags)
  {
    assert(m_device);

    // TODO: useptr is not supported in NPU.
    auto xrt_device = m_device->get_xrt_device();
    m_bo = xrt::ext::bo(xrt_device, host_mem, m_size);
  }

  memory::memory(device* dev, size_t sz, unsigned int flags)
      : m_device(std::move(dev)),
	m_size(sz),
	m_type(memory_type::host),
	m_flags(flags)
  {
    assert(m_device);

    switch (m_flags) {
      // TODO Need to create locked memory for Default and Portable flags. Creating a regular BO for now
      case hipHostMallocDefault:
      case hipHostMallocPortable:
      case hipHostMallocMapped: {
        init_xrt_bo();
        break;
      }
      case hipHostMallocWriteCombined: {
        // This is a workaround to create a buffer with cacheable flag if WriteComined flag is provided.
        // This gets used to create instruction buffer on NPU
        // TODO This would go away once xrt::elf flow is enabled
        auto xrt_device = m_device->get_xrt_device();
        m_bo = xrt::bo(xrt_device, m_size, xrt::bo::flags::cacheable, 1);
        break;
      }
      default:
        break;
    }
  }

  void*
  memory::get_address()
  {
    if (!m_bo)
      return nullptr;

    if (get_type() == memory_type::device || get_type() == memory_type::sub)
      return reinterpret_cast<void *>(m_bo.address());
    else if (get_type() == memory_type::host || get_type() == memory_type::registered)
      return m_bo.map();

    return nullptr;
  }

  void*
  memory::get_device_address() const
  {
    if (!m_bo)
      return nullptr;

    return reinterpret_cast<void *>(m_bo.address());
  }

  void
  memory::write(const void *src , size_t size, size_t src_offset, size_t offset)
  {
    auto src_hip_mem = memory_database::instance().get_hip_mem_from_addr(src).first;
    if (src_hip_mem && src_hip_mem->get_type() == memory_type::host) {
        // pinned hip mem
        assert(src_hip_mem->get_flags() == hipHostMallocDefault || src_hip_mem->get_flags() == hipHostMallocPortable);
    }
    // host memory
    auto src_ptr = reinterpret_cast<const unsigned char*>(src);
    src_ptr += src_offset;
    m_bo.write(src_ptr, size, offset);
    m_bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
  }

  void
  memory::read(void *dst, size_t size, size_t dst_offset, size_t offset)
  {
    auto dst_hip_mem = memory_database::instance().get_hip_mem_from_addr(dst).first;
    if (dst_hip_mem != nullptr && dst_hip_mem->get_type() == memory_type::host) {
        // pinned hip mem
        assert(dst_hip_mem->get_flags() == hipHostMallocDefault || dst_hip_mem->get_flags() == hipHostMallocPortable);
    }
    auto dst_ptr = reinterpret_cast<unsigned char *>(dst);
    dst_ptr += dst_offset;
    if (m_bo) {
      m_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
      m_bo.read(dst_ptr, size, offset);
    }
  }

  void
  memory::sync(xclBOSyncDirection direction)
  {
    assert(m_bo);
    m_bo.sync(direction);
  }

  void
  memory::copy(const memory& src, size_t sz, size_t src_offset, size_t dst_offset)
  {
    m_bo.copy(src.get_xrt_bo(), sz, src_offset, dst_offset);
  }

  void
  memory::init_xrt_bo()
  {
    auto xrt_device = m_device->get_xrt_device();
    m_bo = xrt::ext::bo(xrt_device, m_size);
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //we should override clang-tidy warning by adding NOLINT since m_memory_database is non-const parameter
  memory_database* memory_database::m_memory_database = nullptr; //NOLINT

  memory_database& memory_database::instance()
  {
    if (!m_memory_database)
      static memory_database mem_db;

    return *m_memory_database;
  }

  memory_database::memory_database()
      : m_addr_map(), m_sub_mem_cache(), m_mutex()
  {
    if (m_memory_database) {
      throw std::runtime_error
        ("Multiple instances of hip memory_database detected, only one\n"
        "can be loaded at any given time.");
    }
    m_memory_database = this;
  }

  memory_database::~memory_database()
  {
    m_addr_map.clear();
  }

  void
  memory_database::insert(uint64_t addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem)
  {
    std::lock_guard lock(m_mutex);
    m_addr_map.insert({address_range_key(addr, size), hip_mem});
  }

  void
  memory_database::remove(uint64_t addr)
  {
    std::lock_guard lock(m_mutex);

    m_sub_mem_cache.erase(addr);
    m_addr_map.erase(address_range_key(addr, 0));
  }

  memory_handle
  memory_database::insert_sub_mem(std::shared_ptr<sub_memory> sub_mem)
  {
    std::lock_guard lock(m_mutex);

    // TODO: hip memory allocated from memory pool need to return a valid pointer before
    //       actual allocation is done and user application may add an offset to this pointer,
    //       hence the need for returning a handles/addresses which does not overlap
    //       a more robust handle/pointer creation scheme might be neccessary 
    static auto curr_start = get_page_aligned_size(0x10000);
    size_t aligned_size = get_page_aligned_size(sub_mem->get_size());
    memory_handle h = curr_start;
    m_sub_mem_cache.insert({h, sub_mem});
    curr_start += aligned_size;
    return h;
  }

  std::shared_ptr<sub_memory>
  memory_database::get_sub_mem_from_handle(memory_handle h)
  {
    std::lock_guard lock(m_mutex);

    auto itr = m_sub_mem_cache.find(h);
    if (itr != m_sub_mem_cache.end())
      return itr->second;
    else
      return nullptr;
  }

  std::pair<std::shared_ptr<xrt::core::hip::memory>, size_t>
  memory_database::get_hip_mem_from_addr(void *addr)
  {
    std::lock_guard lock(m_mutex);

    auto itr = m_addr_map.find(address_range_key(reinterpret_cast<uint64_t>(addr), 0));
    if (itr == m_addr_map.end()) {
      return std::pair(nullptr, 0);
    }
    else {
      auto offset = reinterpret_cast<uint64_t>(addr) - itr->first.address;
      return {itr->second, offset};
    }
  }

  std::pair<std::shared_ptr<xrt::core::hip::memory>, size_t>
  memory_database::get_hip_mem_from_addr(const void *addr)
  {
    std::lock_guard lock(m_mutex);

    auto itr = m_addr_map.find(address_range_key(reinterpret_cast<uint64_t>(addr), 0));
    if (itr == m_addr_map.end()) {
      return std::pair(nullptr, 0);
    }
    else {
      auto offset = reinterpret_cast<uint64_t>(addr) - itr->first.address;
      return {itr->second, offset};
    }
  }

} // namespace xrt::core::hip
