// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef xrthip_memory_POOL_h
#define xrthip_memory_POOL_h

#include <cstdint>
#include <list>
#include <map>
#include <mutex>

#include "core/common/device.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/experimental/xrt_ext.h"

#include "device.h"
#include "memory.h"

namespace xrt::core::hip
{
  // TODO: query the driver and select appropriate memory_pool sizes.
  const size_t MEMORY_POOL_BLOCK_SIZE_NPU = (static_cast<size_t>(1) << 30); // 1GB
  const size_t MAX_MEMORY_POOL_SIZE_NPU = 4*(static_cast<size_t>(1) << 30); // 4GB

  // opaque memory pool handle
  using mem_pool_handle = void*;

  class memory_pool_slot
  {
  public:
    memory_pool_slot(size_t start, size_t size);

    size_t start() { return m_start; }
    size_t end() { return m_start + m_size; }
    
    bool
    is_free()
    {
      return m_is_free;
    }

    std::shared_ptr<memory_pool_slot>
    prev()
    {
      return m_prev;
    }

    std::shared_ptr<memory_pool_slot>
    next()
    {
      return m_next;
    }

    size_t
    get_size() const
    {
      return m_size;
    }

    void
    set_size(size_t size)
    {
      m_size = size;
    }

    size_t m_start;
    size_t m_size;
    std::shared_ptr<memory_pool_slot> m_prev;
    std::shared_ptr<memory_pool_slot> m_next;
    bool m_is_free;
  };

  class memory_pool_node
  {
  public:
    memory_pool_node(device* device, size_t size, int id);

    size_t
    get_size() const
    {
      return m_memory->get_size();
    }

    size_t
    free(size_t start);

    void
    merge_free_slots(std::shared_ptr<memory_pool_slot> new_free_slot);

    int m_id;
    size_t m_used;    
    std::shared_ptr<memory> m_memory;
    std::shared_ptr<memory_pool_slot> m_free_list;
    std::shared_ptr<memory_pool_slot> m_alloc_list;
  };

  class memory_pool
  {
  public:

    memory_pool(device* device, size_t max_total_size, size_t pool_size);

    void
    init();

    // Release all unused memory blocks back to the system.
    void
    purge();

    void
    trim_to(size_t min_bytes_to_hold);

    void
    malloc(void* ptr, size_t size);
    
    void
    free(void* ptr);

    void
    get_attribute(hipMemPoolAttr attr, void* value);
    
    void
    set_attribute(hipMemPoolAttr attr, void* value);

    device*
    get_device()
    {
      return m_device;
    }

  protected:
    
    bool
    extend_memory_list(size_t size);

    // add one block to the memory pool
    bool
    extend_memory_pool(size_t aligned_size);

    std::shared_ptr<memory_pool_node>
    find_memory_pool_node(void* ptr, uint64_t &start);

    device* m_device;
    int m_last_id;
    bool m_auto_extend;
    size_t m_max_total_size;
    size_t m_pool_size;
    std::list<std::shared_ptr<memory_pool_node>> m_list;
    std::mutex m_mutex;

    int m_reuse_follow_event_dependencies;
    int m_reuse_allow_opportunistic;
    int m_reuse_allow_internal_dependencies;
    uint64_t m_release_threshold; // Amount of reserved memory in bytes to hold onto before trying to release memory back to the OS.
    uint64_t m_reserved_mem_current; // Amount of backing memory currently allocated for the mempool.
    uint64_t m_reserved_mem_high; // High watermark of backing memory allocated for the mempool since the last time it was reset.
    uint64_t m_used_mem_current; //  Amount of memory from the pool that is currently in use by the application.
    uint64_t m_used_mem_high; // High watermark of the amount of memory from the pool that was in use
  }; 

  // The pointer to a memory_pool object is shared between memory_pool_db and mem_pool_cache.
  // each hip device have exactly one default mem pool and zero or more user created mem pools. 
  // only user created mem pools need to be looked up via an opaque handle.
  // default mem pool and user mem pool both need to be looked up by device id.
  // Global map of memory_pool associated with device id.
  extern std::map<uint32_t, std::list<std::shared_ptr<memory_pool>>> memory_pool_db;
  extern std::map<uint32_t, std::shared_ptr<memory_pool>> current_memory_pool_db;
  extern xrt_core::handle_map<mem_pool_handle, std::shared_ptr<memory_pool>> mem_pool_cache;

  std::shared_ptr<memory_pool>
  get_mem_pool(hipMemPool_t mem_pool);

  hipMemPool_t
  get_mem_pool_handle(std::shared_ptr<memory_pool> mem_pool);

} // xrt::core::hip

#endif
