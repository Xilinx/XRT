// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "core/common/unistd.h"
#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "common.h"
#include "memory_pool.h"

namespace xrt::core::hip
{
  // Global map of memory_pool associated with device id.
  std::map<uint32_t, std::list<std::shared_ptr<memory_pool>>> memory_pool_db;
  std::map<uint32_t, std::shared_ptr<memory_pool>> current_memory_pool_db;
  
  // Global map of memory_pool associated with its handle.
  xrt_core::handle_map<mem_pool_handle, std::shared_ptr<memory_pool>> mem_pool_cache;

  static void
  dlinkedlist_delete(std::shared_ptr<memory_pool_slot>& head, std::shared_ptr<memory_pool_slot>& node)
  {
    if (node->m_prev == nullptr) {
      head = node->m_next;
      if (node->m_next != nullptr)
        node->m_next->m_prev = nullptr;
    } 
    else {
      node->m_prev->m_next = node->m_next;
      if (node->m_next!= nullptr)
        node->m_next->m_prev = node->m_prev;
    }
  }

  static void
  dlinkedlist_insert(std::shared_ptr<memory_pool_slot>& head, std::shared_ptr<memory_pool_slot>& node)
  {    
    node->m_prev = nullptr;
    node->m_next = head;
    if (head != nullptr)
      head->m_prev = node;
    head = node;
  }

  memory_pool_slot::memory_pool_slot(size_t start, size_t size)
      : m_start(start), m_size(size), m_prev(nullptr), m_next(nullptr), m_is_free(true)
  {
  }

  memory_pool_node::memory_pool_node(device* device, size_t size, int id)
      : m_id(id), m_used(0), m_free_list(nullptr), m_alloc_list(nullptr)
  {
    m_memory = std::make_shared<memory>(device, size);
    m_free_list = std::make_shared<memory_pool_slot>(0, size);
  }

  // merge adjacent free slots as one slot
  void
  memory_pool_node::merge_free_slots(std::shared_ptr<memory_pool_slot> new_free_slot)
  {
    std::shared_ptr<memory_pool_slot> p0 = new_free_slot;
    std::shared_ptr<memory_pool_slot> p1 = new_free_slot;

    while (p0 && p0->is_free()) {
      p1 = p0;
      if (p0->start() == 0)
        break;
      p0 = p0->prev();
    }

    p0 = p1->next();
    while (p0 && p0->start() < m_memory->get_size() && p0->is_free()) {
      dlinkedlist_delete(m_free_list, p0);
      p1->set_size(p1->get_size() + p0->get_size());
      p0 = p0->next();
    }
  }

  size_t
  memory_pool_node::free(size_t start)
  {
    size_t size_freed = 0;
    auto used_slot = m_alloc_list;
    while (used_slot) {
      if (used_slot->m_start == start ) {
        size_freed = used_slot->get_size();
        m_used -= size_freed;
        dlinkedlist_delete(m_alloc_list, used_slot);
        dlinkedlist_insert(m_free_list, used_slot);
        used_slot->m_is_free = true;
        merge_free_slots(used_slot);
        break;
      }
      used_slot = used_slot->m_next;
    }
    return size_freed;
  }

  memory_pool::memory_pool(device* device, size_t max_total_size, size_t pool_size)
      : m_device(device), m_last_id(0), m_auto_extend(true), m_max_total_size(max_total_size), m_pool_size(pool_size), m_list(), m_mutex(),
        m_reuse_follow_event_dependencies(1), m_reuse_allow_opportunistic(1), m_reuse_allow_internal_dependencies(1),
        m_release_threshold(0), m_reserved_mem_current(0), m_reserved_mem_high(0), m_used_mem_current(0), m_used_mem_high(0)
  {
    init();
  }

  void
  memory_pool::init()
  {
    std::lock_guard lock(m_mutex);

    m_reserved_mem_current = m_pool_size;

    if (m_pool_size > m_max_total_size)
      throw std::runtime_error("mem poolsize is too big.");
    else if (m_pool_size == m_max_total_size)
      m_auto_extend = false;

    m_list.emplace(m_list.end(), std::make_shared<memory_pool_node>(m_device, m_pool_size, m_last_id++));
  }

  void
  memory_pool::get_attribute(hipMemPoolAttr attr, void* value)
  {
    if (m_list.size() == 0)
      init();

    switch (attr)
    {
    case hipMemPoolReuseFollowEventDependencies:
      *reinterpret_cast<int*>(value) = m_reuse_follow_event_dependencies;
      break;

    case hipMemPoolReuseAllowOpportunistic:
      *reinterpret_cast<int*>(value) = m_reuse_allow_opportunistic;
      break;

    case hipMemPoolReuseAllowInternalDependencies:
      *reinterpret_cast<int*>(value) = m_reuse_allow_internal_dependencies;
      break;

    case hipMemPoolAttrReleaseThreshold:
      *reinterpret_cast<uint64_t*>(value) = m_release_threshold;
      break;

    case hipMemPoolAttrReservedMemCurrent:
      *reinterpret_cast<uint64_t*>(value) = m_reserved_mem_current;
      break;

    case hipMemPoolAttrReservedMemHigh:
      *reinterpret_cast<uint64_t*>(value) = m_reserved_mem_high;
      break;
    
    case hipMemPoolAttrUsedMemCurrent:
      *reinterpret_cast<uint64_t*>(value) = m_used_mem_current;
      break;

    case hipMemPoolAttrUsedMemHigh:
      *reinterpret_cast<uint64_t*>(value) = m_used_mem_high;
      break;
    };
  }

  void
  memory_pool::set_attribute(hipMemPoolAttr attr, void* value)
  {
    if (m_list.size() == 0) {
      init();
    }

    switch (attr)
    {
    case hipMemPoolReuseFollowEventDependencies:
      m_reuse_follow_event_dependencies = *reinterpret_cast<int*>(value);
      break;

    case hipMemPoolReuseAllowOpportunistic:
      m_reuse_allow_opportunistic = *reinterpret_cast<int*>(value);
      break;

    case hipMemPoolReuseAllowInternalDependencies:
      m_reuse_allow_internal_dependencies = *reinterpret_cast<int*>(value);
      break;

    case hipMemPoolAttrReleaseThreshold:
      m_release_threshold = *reinterpret_cast<uint64_t*>(value);
      break;

    case hipMemPoolAttrReservedMemCurrent:
      m_reserved_mem_current = *reinterpret_cast<uint64_t*>(value);
      break;

    case hipMemPoolAttrReservedMemHigh:
      m_reserved_mem_high = *reinterpret_cast<uint64_t*>(value);
      break;

    case hipMemPoolAttrUsedMemCurrent:
      m_used_mem_current = *reinterpret_cast<uint64_t*>(value);
      break;

    case hipMemPoolAttrUsedMemHigh:
      m_used_mem_high = *reinterpret_cast<uint64_t*>(value);
      break;
    };
  }

  bool
  memory_pool::extend_memory_list(size_t size)
  {
    m_list.insert(m_list.begin(), std::make_shared<memory_pool_node>(m_device, size, m_last_id++));
    return true;
  }

  // add one block to the memory pool
  bool
  memory_pool::extend_memory_pool(size_t aligned_size)
  {
    if (m_pool_size + aligned_size > m_max_total_size)
      return false;

    size_t add_mem_sz = m_max_total_size - m_pool_size;
    add_mem_sz = add_mem_sz >= m_pool_size ? m_pool_size : add_mem_sz;

    // add additional block
    if (!extend_memory_list(add_mem_sz))
      return false;

    m_reserved_mem_current += add_mem_sz;
    m_reserved_mem_high = m_reserved_mem_current;
    return true;
  }

  // create allocation from a free slot in the memory pool
  void
  memory_pool::malloc(void* ptr, size_t size)
  {
    if (m_list.size() == 0)
      init();

    assert(ptr);
    auto sub_mem = memory_database::instance().get_sub_mem_from_handle(reinterpret_cast<memory_handle>(ptr));
    if (!sub_mem) {
      throw std::runtime_error("Invlid sub_memory handle");
      return;
    }

    // every allocation from pool has page size alignment
    size_t aligned_size = get_page_aligned_size(size);

    std::lock_guard lock(m_mutex);

    if (aligned_size > m_pool_size)
      throw std::runtime_error("requested size is greater than memory pool block size.");

    std::shared_ptr<memory_pool_node> mm;

    uint32_t num_tries = 0;
    // find first free slot that fits, if none is found, enlarge the pool and try one more time
    while (num_tries <= 2) {
      ++ num_tries;
      auto mp_itr = m_list.begin();
      while (mp_itr != m_list.end()){
        mm = *mp_itr;
        // skip blocks with too liitle memory left to fit the required aligned_size
        if (m_pool_size - mm->m_used < aligned_size) {
          mp_itr++;
          continue;
        }

        auto free_slot = mm->m_free_list;
        auto alloc_slot = free_slot;

        // find the free slot large enough from the free list (m_free_list), mark it as "not free"
        // and move it to m_alloc_list
        while (free_slot != nullptr) {
          if (free_slot->m_size >= aligned_size) {
            // found a slot that is large enough
            if (free_slot->m_size > aligned_size) {
              // if the slot found is large than require aligned_size, divide it into two slots:
              // use the first one (alloc_slot) for the current requested allocation
              // save the other one (free_slot) for future use
              alloc_slot = free_slot;
              free_slot = std::make_shared<memory_pool_slot>(alloc_slot->start() + aligned_size, alloc_slot->m_size - aligned_size);

              // update m_free_list,remove alloc_slot from double linked m_free_list
              if (free_slot->m_prev == nullptr) {
                mm->m_free_list = free_slot;
              }
              else {
                free_slot->m_prev->m_next = free_slot;
              }
              if (free_slot->m_next) {
                free_slot->m_next->m_prev = free_slot;
              }

              alloc_slot->m_is_free = false;
              alloc_slot->m_size = aligned_size;
            }
            else {
              // if the free slot found has exactly the same size as required aligned_size
              // there is no need to divide the slot, just remove the slot from m_free_list
              alloc_slot = free_slot;
              dlinkedlist_delete(mm->m_free_list, alloc_slot);
              alloc_slot->m_is_free = false;
            }

            // move the newly found slot to m_alloc_list
            dlinkedlist_insert(mm->m_alloc_list, alloc_slot);

            // keep track of the total allocated size
            mm->m_used += alloc_slot->m_size;
            m_used_mem_current += alloc_slot->m_size;
            m_used_mem_high = m_used_mem_current;

            // init the sub_mem with bo/offset fro the newly found slot
            sub_mem->init(mm->m_memory, size, alloc_slot->m_start);
            memory_database::instance().insert(reinterpret_cast<uint64_t>(ptr),
                                                            sub_mem->get_size(), sub_mem);
            return;
          }
          free_slot = free_slot->m_next;
        }

        mp_itr++;
        mm = *mp_itr;
      }
      
      // we have already tried enlarging the pool and still no free slot is found
      if (num_tries == 2)
        break;
      
      if (m_auto_extend) {
        // no free slot has been found, add one additional block to the pool and try one more time
        if (extend_memory_pool(aligned_size) == false) {
          // enlarging pool failed
          break;
        }
      }
    };

    // allocation failed
    return;
  }

  // lookup the memory pool node from address (ptr)
  std::shared_ptr<memory_pool_node>
  memory_pool::find_memory_pool_node(void* ptr, uint64_t &start)
  {
    auto mem_info = memory_database::instance().get_hip_mem_from_addr(ptr);
    auto hip_mem = mem_info.first;
    size_t offset = mem_info.second;

    for (auto & node : m_list) {
      if (hip_mem == node->m_memory) {
        start = offset;
        return node;
      }
    }

    return nullptr;
  }

  // free a previous allocation
  void
  memory_pool::free(void* ptr)
  {
    if (!ptr || m_list.size() == 0)
      return;

    std::lock_guard lock(m_mutex);

    uint64_t start = 0;
    auto mm = find_memory_pool_node(reinterpret_cast<void*>(ptr), start);
    if (mm != nullptr)
    {
      // return the slot to free_list and merge it with ajacent free slot
      auto size_freed = mm->free(start);
      m_used_mem_current -= size_freed;
    }

    memory_database::instance().remove(reinterpret_cast<uint64_t>(ptr));
  }

  // trim memory pool by releasing unused blocks back to system until
  // either total size < min_bytes_to_hold or there is no more blocks to free
  void
  memory_pool::trim_to(size_t min_bytes_to_hold)
  {
    if (m_reserved_mem_current < min_bytes_to_hold)
      return;

    std::lock_guard lock(m_mutex);

    bool node_deleted = false;
    do {
      node_deleted = false;
      auto itr = m_list.begin();
      while (itr != m_list.end()) {
        // delete pool block if it is free
        auto node = *itr;
        if (node->m_alloc_list == nullptr) {
          m_reserved_mem_current -= node->get_size();
          m_list.remove(node);
          node_deleted = true;
          break;
        }
        ++ itr;
      };
    } while (node_deleted && m_reserved_mem_current >= min_bytes_to_hold);
  }

  // trim memory pool by releasing unused blocks back to system until
  // either total size < m_release_threshold (set by user) or there is no more blocks to free
  void
  memory_pool::purge()
  {
    trim_to(m_release_threshold);
  }

  // lookup memory pool by opaque handle
  std::shared_ptr<memory_pool>
  get_mem_pool(hipMemPool_t mem_pool)
  {
    throw_invalid_handle_if(!mem_pool, "Invalid mem_pool handle.");

    return mem_pool_cache.get(mem_pool);
  }

  // lookup memory pool handle by memory_pool pointer
  hipMemPool_t
  get_mem_pool_handle(std::shared_ptr<memory_pool> mem_pool)
  {
    throw_invalid_handle_if(!mem_pool, "Invalid mem_pool handle.");

    for (auto& item : mem_pool_cache.get_map())
    {
      if (item.second == mem_pool)
        return reinterpret_cast<hipMemPool_t>(item.first);
    }
    return 0;
  }
} // namespace xrt::core::hip
