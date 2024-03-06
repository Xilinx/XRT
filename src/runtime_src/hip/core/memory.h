// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_memory_h
#define xrthip_memory_h

#include "xrt/config.h"
#include "xrt/device/hal.h"
#include "xrt/util/range.h"
#include "core/common/device.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_ext.h"
#include "device.h"

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

  class memory
  {

  public:
    memory(std::shared_ptr<xrt::core::hip::device> dev)
      : m_size(0), m_type(memory_type::hip_memory_type_invalid), m_hip_flags(0), m_host_mem(nullptr), m_device(dev), m_bo(nullptr), m_upload_from_host_mem_required(false), m_sync_host_mem_required(false)
    {
      assert(m_device);
      init_xrt_bo();
    }

    memory(size_t sz, unsigned int flags, std::shared_ptr<xrt::core::hip::device> dev);

    // construct from user host buffer
    memory(size_t sz, void *host_mem, unsigned int flags, std::shared_ptr<xrt::core::hip::device> dev)
      : m_size(sz), m_type(memory_type::hip_memory_type_registered), m_hip_flags(flags), m_host_mem(reinterpret_cast<unsigned char *>(host_mem)), m_device(dev), m_bo(nullptr), m_upload_from_host_mem_required(false), m_sync_host_mem_required(false)
    {
      assert(m_device);
      init_xrt_bo();
    }

    ~memory()
    { 
      free_mem(); 
    }

    memory(size_t sz, std::shared_ptr<xrt::core::hip::device>  dev);

    static void lock_pages(void* addr, size_t size);

    void validate();
    void pre_kernel_run_sync_host_mem();
    void post_kernel_run_sync_host_mem();

    void copy_from(const xrt::core::hip::memory *src, size_t size, size_t src_offset = 0, size_t offset = 0);
    void copy_from(const void *host_src, size_t size, size_t src_offset = 0, size_t offset = 0);

    void copy_to(void *host_dst, size_t size, size_t dst_offset = 0, size_t offset = 0) const;

    void set_device(std::shared_ptr<xrt::core::hip::device> device)
    { 
      m_device = device; 
    }

    void *get_host_addr();
    void *get_device_addr();

    const std::shared_ptr<xrt::bo> get_xrt_bo() const 
    { 
      return m_bo; 
    }
    
    std::shared_ptr<xrt::bo> get_xrt_bo() 
    { 
      return m_bo; 
    }

    memory_type get_type() const
    { 
      return m_type; 
    }

    memory_group get_group() const
    { 
      return m_group; 
    }
    
    void set_group(memory_group group)
    {
      m_group = group; 
    }
    
    std::shared_ptr<xrt::core::hip::device>
    get_device()
    {
      return m_device;
    }

  private:
    size_t m_size;
    memory_type m_type;
    unsigned int m_hip_flags;
    unsigned char *m_host_mem;
    std::shared_ptr<xrt::core::hip::device>  m_device;
    std::shared_ptr<xrt::bo> m_bo;
    xrt::memory_group m_group;
    bool m_upload_from_host_mem_required;
    bool m_sync_host_mem_required;

    void free_mem();

    void init_xrt_bo();
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

typedef std::map<address_range_key, std::shared_ptr<xrt::core::hip::memory>, addre_sz_key_compare> XRT_HIP_ADDR_MAP;
typedef XRT_HIP_ADDR_MAP::iterator XRT_HIP_ADDR_MAP_ITR;

class memory_database
{
private:
    XRT_HIP_ADDR_MAP m_hostAddrMap;
    XRT_HIP_ADDR_MAP m_devAddrMap;

protected:
    memory_database();

    static memory_database* m_memory_database;

public:
    ~memory_database();

    static memory_database* GetInstance();

    XRT_HIP_ADDR_MAP& get_hostaddr_map() 
    {
        return m_hostAddrMap;
    }

    void insert_host_addr(void* host_addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem);
    void delete_host_addr(void* host_addr);

    void insert_device_addr(uint64_t dev_addr, size_t size, std::shared_ptr<xrt::core::hip::memory> hip_mem);
    void delete_device_addr(uint64_t dev_addr);

    void delete_addr(uint64_t addr);

    std::shared_ptr<xrt::core::hip::memory> get_hip_mem_from_addr(void* addr);
    std::shared_ptr<const xrt::core::hip::memory> get_hip_mem_from_addr(const void* addr);

    std::shared_ptr<xrt::core::hip::memory> get_hip_mem_from_host_addr(void* host_addr);
    std::shared_ptr<const xrt::core::hip::memory> get_hip_mem_from_host_addr(const void* host_addr);

    std::shared_ptr<xrt::core::hip::memory> get_hip_mem_from_device_addr(void* dev_addr);
    std::shared_ptr<const xrt::core::hip::memory> get_hip_mem_from_device_addr(const void* dev_addr);
};


} // xrt::core::hip

#endif
