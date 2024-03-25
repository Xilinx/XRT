// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_memory_h
#define xrthip_memory_h

#include "core/common/device.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_ext.h"
#include "device.h"
#include "xrt/config.h"
#include "xrt/device/hal.h"
#include "xrt/util/range.h"

namespace xrt::core::hip
{
  enum class memory_type : int
  {
    hip_memory_type_host = 0,
    hip_memory_type_device,
    hip_memory_type_managed,
    hip_memory_type_registered,
    hip_memory_type_invalid
  };

  enum class address_type : int
  {
    hip_address_type_host = 0,
    hip_address_type_device
  };

  enum class sync_direction : int
  {
    sync_from_host_to_device = 0,
    sync_from_device_to_host
  };

  class memory
  {

  public:
    memory(std::shared_ptr<xrt::core::hip::device> dev)
      : m_device(std::move(dev)), m_size(0), m_type(memory_type::hip_memory_type_invalid), m_hip_flags(0), m_host_mem(nullptr), m_bo(nullptr), m_sync_host_mem_required(false)
    {
      assert(m_device);
      init_xrt_bo();
    }

    memory(std::shared_ptr<xrt::core::hip::device>  dev, size_t sz);

    memory(std::shared_ptr<xrt::core::hip::device> dev, size_t sz, unsigned int flags);

    // construct from user host buffer
    memory(std::shared_ptr<xrt::core::hip::device> dev, size_t sz, void *host_mem, unsigned int flags)
      : m_device(std::move(dev)), m_size(sz), m_type(memory_type::hip_memory_type_registered), m_hip_flags(flags), m_host_mem(reinterpret_cast<unsigned char *>(host_mem)), m_bo(nullptr), m_sync_host_mem_required(true)
    {
      assert(m_device);

      // user ptr BO is not supported on NPU Linux driver, hence sync between host_mem and internal xrt::bo object is required before and after kernel run
      // TODO: set m_sync_host_mem_required to true for device that support user ptr BO 
      init_xrt_bo();
    }

    ~memory()
    { 
      free_mem(); 
    }

    static void
    lock_pages(void* addr, size_t size);

    void
    validate();
    
    void
    sync(sync_direction);
    
    void
    copy_from(const xrt::core::hip::memory *src, size_t size, size_t src_offset = 0, size_t offset = 0);
    
    void
    copy_from(const void *host_src, size_t size, size_t src_offset = 0, size_t offset = 0);

    void
    copy_to(void *host_dst, size_t size, size_t dst_offset = 0, size_t offset = 0) const;

    void
    set_device(std::shared_ptr<xrt::core::hip::device> device)
    { 
      m_device = device; 
    }

    void*
    get_addr(address_type type);
    
    std::shared_ptr<xrt::bo>
    get_xrt_bo() const
    { 
      return m_bo; 
    }
    
    std::shared_ptr<xrt::bo>
    get_xrt_bo() 
    { 
      return m_bo; 
    }

    unsigned int
    get_hip_flags() const
    { 
      return m_hip_flags; 
    }

    memory_type
    get_type() const
    { 
      return m_type; 
    }

    memory_group
    get_group() const
    { 
      return m_group; 
    }
    
    void
    set_group(memory_group group)
    {
      m_group = group; 
    }
    
    std::shared_ptr<xrt::core::hip::device>
    get_device()
    {
      return m_device;
    }

  protected:
    void*
    get_host_addr()
    {
      return m_host_mem;
    }

    void*
    get_device_addr();

  private:
    std::shared_ptr<xrt::core::hip::device>  m_device;
    size_t m_size;
    memory_type m_type;
    unsigned int m_hip_flags; // hipHostMallocMapped etc.
    unsigned char *m_host_mem; // host copy to store user data
    std::shared_ptr<xrt::bo> m_bo;
    xrt::memory_group m_group;
    bool m_sync_host_mem_required; //true if sync between host copy and bo is required.

    void
    free_mem();

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

struct addre_sz_key_compare
{
    bool operator() (const address_range_key& lhs, const address_range_key& rhs) const
    {
        return ((lhs.address + lhs.size)  < rhs.address);
    }
};

using addr_map = std::map<address_range_key, std::shared_ptr<xrt::core::hip::memory>, addre_sz_key_compare>;

class memory_database
{
private:
    addr_map m_hostAddrMap;
    addr_map m_devAddrMap;

protected:
    memory_database();

    static memory_database* m_memory_database;

    void
    insert_host_addr(void* host_addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem);
    
    void
    delete_host_addr(void* host_addr);

    void
    insert_device_addr(uint64_t dev_addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem);
    
    void
    delete_device_addr(uint64_t dev_addr);

public:
    ~memory_database();

    static memory_database&
    instance();

    addr_map&
    get_hostaddr_map() 
    {
        return m_hostAddrMap;
    }

    void
    insert_addr(address_type type, uint64_t addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem);

    void
    delete_addr(uint64_t addr);

    std::shared_ptr<xrt::core::hip::memory>
    get_hip_mem_from_addr(void* addr);
    
    std::shared_ptr<const xrt::core::hip::memory>
    get_hip_mem_from_addr(const void* addr);

    std::shared_ptr<xrt::core::hip::memory>
    get_hip_mem_from_host_addr(void* host_addr);
    
    std::shared_ptr<const xrt::core::hip::memory>
    get_hip_mem_from_host_addr(const void* host_addr);

    std::shared_ptr<xrt::core::hip::memory>
    get_hip_mem_from_device_addr(void* dev_addr);
    
    std::shared_ptr<const xrt::core::hip::memory>
    get_hip_mem_from_device_addr(const void* dev_addr);
};


} // xrt::core::hip

#endif
