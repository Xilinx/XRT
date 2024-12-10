// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc
// Copyright (C) 2022-2024 Advanced Micro Devices, Inc. All rights reserved.

#ifndef core_common_bo_cache_h_
#define core_common_bo_cache_h_

#include "core/common/system.h"
#include "core/common/device.h"
#include "core/common/shim/buffer_handle.h"
#include "core/include/xrt/detail/ert.h"

#include <vector>
#include <utility>
#include <mutex>

#ifdef _WIN32
# pragma warning( push )
# pragma warning( disable : 4245 )
#endif

namespace xrt_core {

// Create a cache of CMD BO objects -- for now only used for M2M -- to reduce
// the overhead of BO life cycle management.
template <size_t BoSize>
class bo_cache_t {
public:
  // Helper typedef for std::pair. Note the elements are const so that the
  // pair is immutable. The clients should not change the contents of cmd_bo.
  template <typename CommandType>
  using cmd_bo = std::pair<std::unique_ptr<buffer_handle>, CommandType *const>;
private:

  // We are really allocating a page size as that is what xocl/zocl do. Note on
  // POWER9 pagesize maybe more than 4K, xocl would upsize the allocation to the
  // correct pagesize. unmap always unmaps the full page.
  static constexpr size_t m_bo_size = BoSize;
  std::shared_ptr<device> m_device;
  // Maximum number of BOs that can be cached in the pool. Value of 0 indicates
  // caching should be disabled.
  const unsigned int m_cache_max_size;
  std::vector<cmd_bo<void>> m_cmd_bo_cache;
  std::mutex m_mutex;

public:
  bo_cache_t(std::shared_ptr<xrt_core::device> device, unsigned int max_size)
    : m_device(std::move(device)), m_cache_max_size(max_size)
  {}

  bo_cache_t(xclDeviceHandle handle, unsigned int max_size)
    : m_device(get_userpf_device(handle)), m_cache_max_size(max_size)
  {}

  ~bo_cache_t()
  {
    try {
      std::lock_guard<std::mutex> lock(m_mutex);
      for (auto& bo : m_cmd_bo_cache)
        destroy(bo);
    }
    catch (...) {
    }
  }

  template<typename T>
  cmd_bo<T>
  alloc()
  {
    auto bo = alloc_impl();
    return std::make_pair(std::move(bo.first), static_cast<T *>(bo.second));
  }

  template<typename T>
  void
  release(cmd_bo<T>&& bo)
  {
    release_impl(std::make_pair(std::move(bo.first), static_cast<void *>(bo.second)));
  }

private:
  cmd_bo<void>
  alloc_impl()
  {
    if (m_cache_max_size) {
      // If caching is enabled first look up in the BO cache
      std::lock_guard lock(m_mutex);
      if (!m_cmd_bo_cache.empty()) {
        auto bo = std::move(m_cmd_bo_cache.back());
        m_cmd_bo_cache.pop_back();
        return bo;
      }
    }

    auto execHandle = m_device->alloc_bo(m_bo_size, XCL_BO_FLAGS_EXECBUF);
    auto map = execHandle->map(buffer_handle::map_type::write);
    return std::make_pair(std::move(execHandle), map);
  }

  void
  release_impl(cmd_bo<void>&& bo)
  {
    if (m_cache_max_size) {
      // If caching is enabled and BO cache is not fully populated add this the cache
      std::lock_guard lock(m_mutex);
      if (m_cmd_bo_cache.size() < m_cache_max_size) {
        m_cmd_bo_cache.push_back(std::move(bo));
        return;
      }
    }
    destroy(bo);
  }

  void
  destroy(const cmd_bo<void>& bo)
  {
    bo.first->unmap(bo.second);
  }
};

using bo_cache = bo_cache_t<4096>;

} // xrt_core

#ifdef _WIN32
# pragma warning( pop )
#endif

#endif
