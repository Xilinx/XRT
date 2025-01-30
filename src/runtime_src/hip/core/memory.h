// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrthip_memory_h
#define xrthip_memory_h

#include "core/common/device.h"
#include "core/common/unistd.h"
#include "device.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/experimental/xrt_ext.h"

namespace xrt::core::hip
{
  // memory_handle - opaque memory handle
  using memory_handle = uint64_t;

  enum class memory_type : int
  {
    host = 0,
    device,
    managed,
    registered,
    sub,
    invalid
  };

  enum class address_type : int
  {
    host = 0,
    device
  };

  class memory
  {

  public:

    // Constructor for dummy memory
    memory(memory_type type, size_t sz=0)
      : m_bo(), m_device(nullptr), m_size(sz), m_type(type), m_flags(0)
    {
    }

    // allocate device memory
    memory(device* dev, size_t sz);

    // allocate host memory
    memory(device* dev, size_t sz, unsigned int flags);

    // allocate from user host buffer
    memory(device* dev, size_t sz, void *host_mem, unsigned int flags);
    
    void*
    get_address();
    
    void*
    get_device_address() const;

    void
    write(const void *src, size_t size, size_t src_offset = 0, size_t offset = 0);

    void
    read(void *dst, size_t size, size_t dst_offset = 0, size_t offset = 0); 
    
    void
    sync(xclBOSyncDirection);

    void
    copy(const memory& src, size_t sz, size_t src_offset = 0, size_t dst_offset = 0);

    const xrt::bo&
    get_xrt_bo() const
    {
      return m_bo;
    }

    unsigned int
    get_flags() const
    {
      return m_flags;
    }

    memory_type
    get_type() const
    {
      return m_type;
    }
    
    size_t
    get_size() const
    {
      return m_size;
    }

  protected:
    xrt::bo m_bo;

  private:
    device*  m_device;
    size_t m_size;
    memory_type m_type;
    unsigned int m_flags;

    void
    init_xrt_bo();
  };

  // sub_memory
  class sub_memory : public memory
  {
  public:

    // constructor for dummy sub_memory
    sub_memory(size_t sz = 0)
      : memory(memory_type::sub, sz)
    {
    }

    void
    init(std::shared_ptr<memory> parent, size_t size, size_t offset)
    {
      m_parent = parent;
      m_bo = xrt::bo(parent->get_xrt_bo(), size, offset);
    }

  private:
    std::shared_ptr<memory> m_parent;
  };

  // address_range_key is used for look up hip memory objects via an offseted address
  class address_range_key
  {
  public:
    address_range_key() : address(0), size(0) {}
    address_range_key(uint64_t addr, size_t sz) : address(addr), size(sz) {}

    uint64_t    address;
    size_t      size;
  };

  struct address_sz_key_compare
  {
    bool operator() (const address_range_key& lhs, const address_range_key& rhs) const
    {
      // The keys a and b are equivalent by definition when neither a < b nor b < a is true
      if (lhs.address == rhs.address)
        return false;
      
      // Example to explain why "-1" is mandatory in below calculation
      // if address is 0x4000 and size is 0x100 . If another address is 0x4100
      // a < b retruns false because 0x4000+0x100 < 0x4100
      // b < a returns false because 0x4100+0x100 < 0x4000
      // if we add "-1" then a<b returns true.
      return (lhs.address + lhs.size < 1) || (lhs.address + lhs.size - 1  < rhs.address);
    }
  };
  
  ////////////////////////////////////////////////////////////////////////////////////////////////
  using addr_map = std::map<address_range_key, std::shared_ptr<memory>, address_sz_key_compare>;
  
  class memory_database
  {
  private:
    addr_map m_addr_map; // address lookup for regular xrt::bo
    std::map<memory_handle, std::shared_ptr<sub_memory>> m_sub_mem_cache; // sub_memory lookup via handle
    std::mutex m_mutex;

  protected:
    memory_database();
  
    static memory_database* m_memory_database;
  
  public:
    ~memory_database();
  
    static memory_database&
    instance();
  
    void
    insert(uint64_t addr, size_t size, std::shared_ptr<memory> hip_mem);

    void
    remove(uint64_t addr);

    memory_handle
    insert_sub_mem(std::shared_ptr<sub_memory> sub_mem);

    std::shared_ptr<sub_memory>
    get_sub_mem_from_handle(memory_handle h);

    std::pair<std::shared_ptr<xrt::core::hip::memory>, size_t>
    get_hip_mem_from_addr(void* addr);
  
    std::pair<std::shared_ptr<xrt::core::hip::memory>, size_t>
    get_hip_mem_from_addr(const void* addr);
  };

  // helper function to get page aligned size;
  inline size_t
  get_page_aligned_size(size_t sz)
  {
    size_t page_size = xrt_core::getpagesize();
    size_t aligned_size = ((sz + page_size) / page_size) * page_size;
    return aligned_size;
  }

} // xrt::core::hip

#endif
