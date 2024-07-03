// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.

#ifdef _WIN32
#include <Windows.h>
#include <Memoryapi.h>
#else
#include <sys/mman.h>
#endif

#include "core/common/unistd.h"
#include "hip/config.h"
#include "hip/hip_runtime_api.h"

#include "common.h"
#include "memory_pool.h"

namespace xrt::core::hip
{
  // Global map of memory_pool associated with device id.
  std::map<int, std::list<std::shared_ptr<memory_pool>>> memory_pool_db;
  
  // Global map of memory_pool associated with its handle.
  xrt_core::handle_map<mem_pool_handle, std::shared_ptr<memory_pool>> mem_pool_cache;

  static void
  dlinkedlist_delete(std::shared_ptr<memory_pool_slot>& head, std::shared_ptr<memory_pool_slot>& node)
  {
    if (node->m_prev == nullptr) {
      head = node->m_next;
      if (node->m_next != nullptr) node->m_next->m_prev = nullptr;
    } else {
      node->m_prev->m_next = node->m_next;
      if (node->m_next!= nullptr) {
        node->m_next->m_prev = node->m_prev;
      }
    }
  }

  static void
  dlinkedlist_insert(std::shared_ptr<memory_pool_slot>& head, std::shared_ptr<memory_pool_slot>& node)
  {    
    node->m_prev = nullptr;
    node->m_next = head;
    if (head != nullptr) {
      head->m_prev = node;
    }
    head = node;
  }

  memory_pool_slot::memory_pool_slot(size_t start, size_t size)
      : m_start(start), m_size(size), m_prev(nullptr), m_next(nullptr), m_is_free(true)
  {
  }

  memory_pool_node::memory_pool_node(std::shared_ptr<device> device, size_t size, int id)
      : m_id(id), m_used(0), m_free_list(nullptr), m_alloc_list(nullptr)
  {
    m_memory = std::make_shared<memory>(device, size);
    m_free_list = std::make_shared<memory_pool_slot>(0, size);
  }

  void
  memory_pool_node::merge_free_slots(std::shared_ptr<memory_pool_slot> new_free_slot)
  {
    std::shared_ptr<memory_pool_slot> p0 = new_free_slot;
    std::shared_ptr<memory_pool_slot> p1 = new_free_slot;

    while (p0->is_free()) {
      p1 = p0;
      if (p0->start() == 0) break;
      p0 = p0->prev();
    }

    p0 = p1->next();
    while (p0 != nullptr && p0->start() < m_memory->get_size() && p0->is_free()) {
      dlinkedlist_delete(m_free_list, p0);
      p1->set_size(p1->get_size() + p0->get_size());
      p0 = p0->next();
    }
  }

  size_t
  memory_pool_node::free(size_t start)
  {
    size_t size_freed = 0;
    auto used_slot  = m_alloc_list;
    while (used_slot != nullptr)
    {
      if (used_slot->m_start == start )
      {
        size_freed = used_slot->get_size();
        m_used -= size_freed;
        dlinkedlist_delete(m_alloc_list, used_slot);
        dlinkedlist_insert(m_free_list, used_slot);
        used_slot->m_is_free = true;
        merge_free_slots(used_slot);
        break;
      }
      used_slot = used_slot->m_next;
    };
    return size_freed;
  }

  memory_pool::memory_pool(std::shared_ptr<device> device, size_t max_total_size, size_t pool_size)
      : m_device(std::move(device)), m_last_id(0), m_auto_extend(true), m_max_total_size(max_total_size), m_pool_size(pool_size), m_list(), m_mutex(),
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
    {
      throw std::runtime_error("mem poolsize is too big.");
    }
    else if (m_pool_size == m_max_total_size)
    {
      m_auto_extend = false;
    }

    m_list.emplace(m_list.end(), std::make_shared<memory_pool_node>(m_device, m_pool_size, m_last_id++));
  }

  void
  memory_pool::get_attribute(hipMemPoolAttr attr, void* value)
  {
    if (m_list.size() == 0) {
      init();
    }

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

  void memory_pool::malloc(void**ptr, size_t size)
  {
    *ptr = alloc(size);
  }

  void*
  memory_pool::alloc(size_t size)
  {
    if (m_list.size() == 0) {
      init();
    }

    size_t page_size = xrt_core::getpagesize();
    size_t aligned_size = ((size + page_size) / page_size) * page_size;

    std::lock_guard lock(m_mutex);

    if (aligned_size > m_pool_size)
    {
      throw std::runtime_error("requested size is greater than memory pool block size.");
    }

    std::shared_ptr<memory_pool_node> mm;

    uint32_t num_tries = 0;
    // find first free slot that fits, if none is found, enlarge the pool and try one more time
    while (num_tries <= 2) {
      ++ num_tries;
      auto mp_itr = m_list.begin();
      while (mp_itr != m_list.end())
      {
        mm = *mp_itr;
        if (m_pool_size - mm->m_used < aligned_size)
        {
          mp_itr++;
          continue;
        }

        auto _free = mm->m_free_list;
        auto _not_free = _free;

        while (_free != nullptr)
        {
          if (_free->m_size >= aligned_size)
          {
            if (_free->m_size > aligned_size)
            {
              _not_free = _free;

              _free = std::make_shared<memory_pool_slot>(_not_free->start() + aligned_size, _not_free->m_size - aligned_size);

              // update free_list
              if (_free->m_prev == nullptr) {
                mm->m_free_list = _free;
              }
              else {
                _free->m_prev->m_next = _free;
              }
              if (_free->m_next) {
                _free->m_next->m_prev = _free;
              }

              _not_free->m_is_free = false;
              _not_free->m_size = aligned_size;
            }
            else
            {
              _not_free = _free;
              dlinkedlist_delete(mm->m_free_list, _not_free);
              _not_free->m_is_free = false;
            }
            dlinkedlist_insert(mm->m_alloc_list, _not_free);

            mm->m_used += _not_free->m_size;
            m_used_mem_current += _not_free->m_size;
            m_used_mem_high = m_used_mem_current;

            size_t new_mem_start = reinterpret_cast<uint64_t>(mm->m_memory->get_address()) + _not_free->m_start;
            return reinterpret_cast<void*>(new_mem_start);
          }
          _free = _free->m_next;
        }
        mp_itr++;
        mm = *mp_itr;
      }
      
      if (num_tries == 2) break;
      
      if (m_auto_extend)
      {
        if (m_pool_size + aligned_size > m_max_total_size) {
          //throw std::runtime_error("No enough memory.");
          break;
        }
        size_t add_mem_sz = m_max_total_size - m_pool_size;
        add_mem_sz = add_mem_sz >= m_pool_size ? m_pool_size : add_mem_sz;
        // add additional block
        if (!extend_memory_list(add_mem_sz)) {
          //throw std::runtime_error("No enough memory.");
          break;
        }
        m_reserved_mem_current += add_mem_sz;
        m_reserved_mem_high = m_reserved_mem_current;
      }
    };

    throw std::runtime_error("No enough memory.");
    return nullptr;
  }

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

  void
  memory_pool::free(void* ptr)
  {
    if (ptr == nullptr || m_list.size() == 0) return;

    std::lock_guard lock(m_mutex);

    uint64_t start = 0;
    auto mm = find_memory_pool_node(ptr, start);
    if (mm != nullptr)
    {
      // return the slot to free_list and merge it with ajacent free slot
      auto size_freed = mm->free(start);
      m_used_mem_current -= size_freed;
    }
  }

  void
  memory_pool::trim_to(size_t min_bytes_to_hold)
  {
    if (m_reserved_mem_current < min_bytes_to_hold) return;

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

  void
  memory_pool::purge()
  {
    trim_to(m_release_threshold);
  }

  std::shared_ptr <memory_pool>
  get_mem_pool(hipMemPool_t mem_pool)
  {
    throw_invalid_handle_if(!mem_pool, "Invalid mem_pool handle.");

    return mem_pool_cache.get(mem_pool);
  }
} // namespace xrt::core::hip
