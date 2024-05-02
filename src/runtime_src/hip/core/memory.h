// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_memory_h
#define xrthip_memory_h

#include "core/common/device.h"
#include "device.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_ext.h"
#include "xrt/config.h"
#include "xrt/device/hal.h"
#include "xrt/util/range.h"

namespace xrt::core::hip
{
  enum class memory_type : int
  {
    host = 0,
    device,
    managed,
    registered,
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

    // allocate device memory
    memory(std::shared_ptr<xrt::core::hip::device> dev, size_t sz);

    // allocate host memory
    memory(std::shared_ptr<xrt::core::hip::device> dev, size_t sz, unsigned int flags);

    // allocate from user host buffer
    memory(std::shared_ptr<xrt::core::hip::device> dev, size_t sz, void *host_mem, unsigned int flags);
    
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

  private:
    std::shared_ptr<device>  m_device;
    size_t m_size;
    memory_type m_type;
    unsigned int m_flags;
    xrt::bo m_bo;

    void
    init_xrt_bo();
  };

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
      return (lhs.address + lhs.size - 1  < rhs.address);
    }
  };
  
  using addr_map = std::map<address_range_key, std::shared_ptr<memory>, address_sz_key_compare>;
  
  class memory_database
  {
  private:
    addr_map m_addr_map;
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
  
    std::pair<std::shared_ptr<xrt::core::hip::memory>, size_t>
    get_hip_mem_from_addr(void* addr);
  
    std::pair<std::shared_ptr<xrt::core::hip::memory>, size_t>
    get_hip_mem_from_addr(const void* addr);
  };
  
} // xrt::core::hip

#endif
