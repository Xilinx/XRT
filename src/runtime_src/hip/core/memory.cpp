// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.

#ifdef _WIN32
#include <Windows.h>
#include <Memoryapi.h>
#else
#include <sys/mman.h>
#endif

#include "core/common/unistd.h"
#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "device.h"
#include "memory.h"

// TODO: use xrt::ext::bo in stead of xrt::bo
#define USE_XRT_EXT_BO 0 // USE_XRT_EXT_BO=0 works on XRT sw_emu, both USE_XRT_EXT_BO=0 and USE_XRT_EXT_BO=1 works on npu

namespace xrt::core::hip
{

  memory::memory(size_t sz, std::shared_ptr<xrt::core::hip::device> dev)
      : m_size(sz), m_type(memory_type::hip_memory_type_device), m_hip_flags(0), m_host_mem(nullptr), m_device(dev), m_bo(nullptr), m_upload_from_host_mem_required(false), m_sync_host_mem_required(false)
  {
    assert(m_device);
    init_xrt_bo();
  }

  memory::memory(size_t sz, unsigned int flags, std::shared_ptr<xrt::core::hip::device> dev)
      : m_size(sz), m_type(memory_type::hip_memory_type_host), m_hip_flags(flags), m_host_mem(nullptr), m_device(dev), m_bo(nullptr), m_upload_from_host_mem_required(false), m_sync_host_mem_required(false)
  {
    assert(m_device);
    init_xrt_bo();
  }

  void
  memory::lock_pages(void* addr, size_t size)
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
    #if (USE_XRT_EXT_BO)
    {
        void* maddr = nullptr;
        if (m_bo == nullptr)
        {
          auto xrt_device = m_device->get_xrt_device();
          switch (m_type)
          {
          case memory_type::hip_memory_type_device:
            m_bo = std::make_shared<xrt::ext::bo>(xrt_device, m_size);
            break;

          case memory_type::hip_memory_type_host:
            m_bo = std::make_shared<xrt::ext::bo>(xrt_device, m_size);
            maddr = m_bo->map();
            lock_pages(maddr, m_size);
            break;

          case memory_type::hip_memory_type_registered:
            m_bo = std::make_shared<xrt::ext::bo>(xrt_device, m_size);
            break;

          case memory_type::hip_memory_type_managed:
            // TODO: implement managed memory once xdna-driver support Linux HMM
            m_bo = std::make_shared<xrt::ext::bo>(xrt_device, m_size);
            break;

          default:
            break;
          };
        }
    }
    #else
      // if xrt::ext::bo is not used , a host mem copy is created to store user data before xrt::bo is created
      m_host_mem = reinterpret_cast<unsigned char *>(aligned_alloc(xrt_core::getpagesize(), m_size));    
    #endif //USE_XRT_EXT_BO
  }

  void
  memory::validate()
  {
    #if !(USE_XRT_EXT_BO)
    {
      void* maddr = nullptr;
      if (m_bo == nullptr)
      {
        // TODO: use correct XRT_BO_FLAGS on different type of device
        auto xrt_device = m_device->get_xrt_device();
        switch (m_type)
        {
        case memory_type::hip_memory_type_device:
          m_bo = std::make_shared<xrt::bo>(xrt_device, m_size, XRT_BO_FLAGS_HOST_ONLY, m_group);
          break;

        case memory_type::hip_memory_type_host:
          m_bo = std::make_shared<xrt::bo>(xrt_device, m_size, XRT_BO_FLAGS_HOST_ONLY, m_group);
          maddr = m_bo->map();
          lock_pages(maddr, m_size);
          break;

        case memory_type::hip_memory_type_registered:
          m_bo = std::make_shared<xrt::bo>(xrt_device, m_size, XRT_BO_FLAGS_HOST_ONLY, m_group);
          m_sync_host_mem_required = true;
          break;

        default:
          break;
        };
      }

      if (m_upload_from_host_mem_required == true)
      {
        m_bo->write(m_host_mem, m_size, 0);
        m_bo->sync(XCL_BO_SYNC_BO_TO_DEVICE);
        m_upload_from_host_mem_required = false;
      }
    }
    #endif // ! USE_XRT_EXT_BO
  }

  void
  memory::pre_kernel_run_sync_host_mem()
  {
    assert(m_bo);

    if (m_sync_host_mem_required)
    {
      m_bo->write(m_host_mem, m_size, 0);
      m_bo->sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }
  }
  void
  memory::post_kernel_run_sync_host_mem()
  {
    assert(m_bo);
        
    if (m_sync_host_mem_required)
    {
      m_bo->sync(XCL_BO_SYNC_BO_FROM_DEVICE);
      m_bo->read(m_host_mem, m_size, 0);
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

  void *
  memory::get_host_addr()
  {
    if (m_bo == nullptr ||
        m_type == memory_type::hip_memory_type_registered)
    {
      // user ptr BO is not supported on NPU Linux driver
      m_sync_host_mem_required = true;
      return m_host_mem;
    }
    return m_bo->map();
  }

  void *
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
      m_upload_from_host_mem_required = true;
    }
  }

  void
  memory::copy_from(const void *host_src, size_t size, size_t src_offset, size_t offset)
  {
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
      m_upload_from_host_mem_required = true;
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
  memory_database *memory_database::m_memory_database = nullptr;

  memory_database *memory_database::GetInstance()
  {
    if (m_memory_database == nullptr)
    {
      m_memory_database = new memory_database();
    }
    return m_memory_database;
  }

  memory_database::memory_database()
      : m_hostAddrMap(), m_devAddrMap()
  {
  }

  memory_database::~memory_database()
  {
    m_hostAddrMap.clear();
    m_devAddrMap.clear();
  }

  void
  memory_database::insert_host_addr(void *host_addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem)
  {
    m_hostAddrMap.insert(std::pair(address_range_key(reinterpret_cast<uint64_t>(host_addr), size), hip_mem));
  }

  void
  memory_database::delete_host_addr(void *host_addr)
  {
    m_hostAddrMap.erase(address_range_key(reinterpret_cast<uint64_t>(host_addr), 0));
  }

  void
  memory_database::insert_device_addr(uint64_t dev_addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem)
  {
    m_devAddrMap.insert(std::pair(address_range_key(dev_addr, size), hip_mem));
  }

  void
  memory_database::delete_device_addr(uint64_t dev_addr)
  {
    m_devAddrMap.erase(address_range_key(dev_addr, 0));
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
    XRT_HIP_ADDR_MAP_ITR itr = m_hostAddrMap.find(address_range_key(reinterpret_cast<uint64_t>(host_addr), 0));
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
    XRT_HIP_ADDR_MAP_ITR itr = m_hostAddrMap.find(address_range_key(reinterpret_cast<uint64_t>(host_addr), 0));
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
    XRT_HIP_ADDR_MAP_ITR itr = m_devAddrMap.find(address_range_key(reinterpret_cast<uint64_t>(dev_addr), 0));
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
    XRT_HIP_ADDR_MAP_ITR itr = m_devAddrMap.find(address_range_key(reinterpret_cast<uint64_t>(dev_addr), 0));
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
