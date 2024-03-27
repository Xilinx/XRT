// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#ifdef _WIN32
#include <Windows.h>
#include <Memoryapi.h>
#else
#include <sys/mman.h>
#endif

#include "core/common/unistd.h"
#include "device.h"
#include "memory.h"
#include "hip/config.h"
#include "hip/hip_runtime_api.h"

namespace xrt::core::hip
{

  memory::memory(std::shared_ptr<xrt::core::hip::device> dev, size_t sz)
      : m_device(std::move(dev)), m_size(sz), m_type(memory_type::hip_memory_type_device), m_hip_flags(0), m_host_mem(nullptr), m_bo(nullptr), m_sync_host_mem_required(false)
  {
    assert(m_device);

    // TODO: support non-npu device that may require delayed xrt::bo allocation until xrt kernel is created
    init_xrt_bo();
  }

  memory::memory(std::shared_ptr<xrt::core::hip::device> dev, size_t sz, unsigned int flags)
      : m_device(std::move(dev)), m_size(sz), m_type(memory_type::hip_memory_type_host), m_hip_flags(flags), m_host_mem(nullptr), m_bo(nullptr), m_sync_host_mem_required(false)
  {
    assert(m_device);

    switch (m_hip_flags)
    {
    case hipHostMallocDefault:
    case hipHostMallocPortable:
      // allocate pinned memory on host only, xrt::bo object will not be allocated.
      m_host_mem = reinterpret_cast<unsigned char *>(aligned_alloc(xrt_core::getpagesize(), m_size));
      assert(m_host_mem);
      lock_pages(m_host_mem, m_size);
      break;

    case hipHostMallocMapped:
      init_xrt_bo();
      m_host_mem = reinterpret_cast<unsigned char*>(m_bo->map());
      break;

    case hipHostMallocWriteCombined:
      init_xrt_bo();
      m_host_mem = reinterpret_cast<unsigned char *>(m_bo->map());
      lock_pages(m_host_mem, m_size);
      break;

    default:
      break;
    }
  }

  void
  memory::lock_pages(void *addr, size_t size)
  {
#ifdef _WIN32
    VirtualLock(addr, size);
#else
    mlock(addr, size);
#endif
  }

  void
  memory::init_xrt_bo()
  {
    auto xrt_device = m_device->get_xrt_device();
    m_bo = std::make_shared<xrt::ext::bo>(xrt_device, m_size);
  }

  void
  memory::validate()
  {
    // validate() is requied only on non-npu device that require delayed xrt::bo allocation until xrt kernel is created
    assert(m_type == memory_type::hip_memory_type_device);

    if (m_bo == nullptr)
    {
      auto xrt_device = m_device->get_xrt_device();
      m_bo = std::make_shared<xrt::bo>(xrt_device, m_size, XRT_BO_FLAGS_HOST_ONLY, m_group);

      if (m_sync_host_mem_required == true)
      {
        m_bo->write(m_host_mem, m_size, 0);
        m_bo->sync(XCL_BO_SYNC_BO_TO_DEVICE);
        m_sync_host_mem_required = false;
      }
    }
  }

  void
  memory::sync(sync_direction drtn)
  {
    assert(m_bo);

    if (m_sync_host_mem_required)    
    {
      switch (drtn)
      {
        case sync_direction::sync_from_host_to_device:
          m_bo->write(m_host_mem, m_size, 0);
          m_bo->sync(XCL_BO_SYNC_BO_TO_DEVICE);
          break;

        case sync_direction::sync_from_device_to_host:
          m_bo->sync(XCL_BO_SYNC_BO_FROM_DEVICE);
          m_bo->read(m_host_mem, m_size, 0);
          break;

        default:
          break;
      };
    }
  }

  void
  memory::free_mem()
  {
    if (m_type != memory_type::hip_memory_type_registered &&
        m_host_mem != nullptr)
    {
      free(m_host_mem);
    }
  }

  void*
  memory::get_addr(address_type type)
  {
    switch (type)
    {
    case address_type::hip_address_type_device:
      return get_device_addr();
      break;

    case address_type::hip_address_type_host:
      return get_host_addr();
      break;

    default:
      assert(0);
      break;
    };
    return nullptr;
  }

  void*
  memory::get_device_addr()
  {
    if (m_bo != nullptr)
    {
      return reinterpret_cast<void *>(m_bo->address());
    }
    return nullptr;
  }

  void
  memory::copy_from(const xrt::core::hip::memory *src, size_t size, size_t src_offset, size_t offset)
  {
    auto src_bo = src->get_xrt_bo();
    assert(src_bo);
    if (m_bo != nullptr)
    {
      m_bo->copy(*src_bo, size, src_offset, offset);
      m_bo->sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }
    else
    {
      src->copy_to(m_host_mem, size, offset, src_offset);
      m_sync_host_mem_required = true;
    }
  }

  void
  memory::copy_from(const void *host_src, size_t size, size_t src_offset, size_t offset)
  {
    auto src_hip_mem = memory_database::instance().get_hip_mem_from_host_addr(host_src);
    if (src_hip_mem != nullptr &&
        src_hip_mem->get_type() == memory_type::hip_memory_type_host)
    {
        // pinned hip mem
        assert(src_hip_mem->get_hip_flags() == hipHostMallocDefault || src_hip_mem->get_hip_flags() == hipHostMallocPortable);

        // TODO: get better performance by avoiding two step copy in case of copying from pinned host mem
    }

    const unsigned char *src_ptr = reinterpret_cast<const unsigned char *>(host_src);
    src_ptr += src_offset;
    if (m_bo != nullptr)
    {
      m_bo->write(src_ptr, size, offset);
      m_bo->sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }
    else
    {
      memcpy(m_host_mem, src_ptr, size);
      m_sync_host_mem_required = true;
    }
  }

  void
  memory::copy_to(void *host_dst, size_t size, size_t dst_offset, size_t offset) const
  {
    unsigned char *dst_ptr = reinterpret_cast<unsigned char *>(host_dst);
    dst_ptr += dst_offset;
    if (m_bo != nullptr)
    {
      m_bo->sync(XCL_BO_SYNC_BO_FROM_DEVICE);
      m_bo->read(dst_ptr, size, offset);
    }
    else
    {
      memcpy(dst_ptr, m_host_mem, size);
    }
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  memory_database* memory_database::m_memory_database = nullptr;

  memory_database& memory_database::instance()
  {
    if (!m_memory_database)
    {
      static memory_database mem_db;
    }
    return *m_memory_database;
  }

  memory_database::memory_database()
      : m_hostAddrMap(), m_devAddrMap()
  {
    if (m_memory_database)
    {
      throw std::runtime_error
        ("Multiple instances of hip memory_database detected, only one\n"
        "can be loaded at any given time.");
    }
    m_memory_database = this;
  }

  memory_database::~memory_database()
  {
    m_hostAddrMap.clear();
    m_devAddrMap.clear();
  }

  void
  memory_database::insert_host_addr(void *host_addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem)
  {
    m_hostAddrMap.insert({address_range_key(reinterpret_cast<uint64_t>(host_addr), size), hip_mem});
  }

  void
  memory_database::delete_host_addr(void *host_addr)
  {
    m_hostAddrMap.erase(address_range_key(reinterpret_cast<uint64_t>(host_addr), 0));
  }

  void
  memory_database::insert_device_addr(uint64_t dev_addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem)
  {
    m_devAddrMap.insert({address_range_key(dev_addr, size), hip_mem});
  }

  void
  memory_database::delete_device_addr(uint64_t dev_addr)
  {
    m_devAddrMap.erase(address_range_key(dev_addr, 0));
  }

  void
  memory_database::insert_addr(address_type type, uint64_t addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem)
  {
    switch (type)
    {
    case address_type::hip_address_type_device:
      m_devAddrMap.insert({address_range_key(addr, size), hip_mem});
      break;
    case address_type::hip_address_type_host:
      m_hostAddrMap.insert({address_range_key(addr, size), hip_mem});
      break;

    default:
      break;
    };
  }

  void
  memory_database::delete_addr(uint64_t addr)
  {
    m_devAddrMap.erase(address_range_key(addr, 0));
    m_hostAddrMap.erase(address_range_key(addr, 0));
  }

  std::shared_ptr<xrt::core::hip::memory>
  memory_database::get_hip_mem_from_host_addr(void *host_addr)
  {
    auto itr = m_hostAddrMap.find(address_range_key(reinterpret_cast<uint64_t>(host_addr), 0));
    if (itr == m_hostAddrMap.end())
    {
      return nullptr;
    }
    else
    {
      return itr->second;
    }
  }

  std::shared_ptr<const xrt::core::hip::memory>
  memory_database::get_hip_mem_from_host_addr(const void *host_addr)
  {
    auto itr = m_hostAddrMap.find(address_range_key(reinterpret_cast<uint64_t>(host_addr), 0));
    if (itr == m_hostAddrMap.end())
    {
      return nullptr;
    }
    else
    {
      return itr->second;
    }
  }

  std::shared_ptr<xrt::core::hip::memory>
  memory_database::get_hip_mem_from_device_addr(void *dev_addr)
  {
    auto itr = m_devAddrMap.find(address_range_key(reinterpret_cast<uint64_t>(dev_addr), 0));
    if (itr == m_devAddrMap.end())
    {
      return nullptr;
    }
    else
    {
      return itr->second;
    }
  }

  std::shared_ptr<const xrt::core::hip::memory>
  memory_database::get_hip_mem_from_device_addr(const void *dev_addr)
  {
    auto itr = m_devAddrMap.find(address_range_key(reinterpret_cast<uint64_t>(dev_addr), 0));
    if (itr == m_devAddrMap.end())
    {
      return nullptr;
    }
    else
    {
      return itr->second;
    }
  }

  std::shared_ptr<xrt::core::hip::memory>
  memory_database::get_hip_mem_from_addr(void *addr)
  {
    auto hip_mem = get_hip_mem_from_device_addr(addr);
    if (hip_mem != nullptr)
    {
      return hip_mem;
    }
    hip_mem = get_hip_mem_from_host_addr(addr);
    return hip_mem;
  }

  std::shared_ptr<const xrt::core::hip::memory>
  memory_database::get_hip_mem_from_addr(const void *addr)
  {
    auto hip_mem = get_hip_mem_from_device_addr(addr);
    if (hip_mem != nullptr)
    {
      return hip_mem;
    }
    hip_mem = get_hip_mem_from_host_addr(addr);
    return hip_mem;
  }

} // namespace xrt::core::hip
