/**
 * Copyright (C) 2019 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef core_common_bo_cache_h_
#define core_common_bo_cache_h_

#include "system.h"
#include "device.h"
#include "ert.h"

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
class bo_cache {
public:
  // Helper typedef for std::pair. Note the elements are const so that the
  // pair is immutable. The clients should not change the contents of cmd_bo.
  template <typename CommandType>
  using cmd_bo = std::pair<const xclBufferHandle, CommandType *const>;
private:

  // We are really allocating a page size as that is what xocl/zocl do. Note on
  // POWER9 pagesize maybe more than 4K, xocl would upsize the allocation to the
  // correct pagesize. unmap always unmaps the full page.
  static const size_t mBOSize = 4096;
  std::shared_ptr<device> mDevice;
  // Maximum number of BOs that can be cached in the pool. Value of 0 indicates
  // caching should be disabled.
  const unsigned int mCacheMaxSize;
  std::vector<cmd_bo<void>> mCmdBOCache;
  std::mutex mCacheMutex;

public:
  bo_cache(xclDeviceHandle handle, unsigned int max_size)
    : mDevice(get_userpf_device(handle)), mCacheMaxSize(max_size)
  {}

  ~bo_cache()
  {
    std::lock_guard<std::mutex> lock(mCacheMutex);
    for (auto& bo : mCmdBOCache)
      destroy(bo);
  }

  template<typename T>
  cmd_bo<T>
  alloc()
  {
    auto bo = alloc_impl();
    return std::make_pair(bo.first, static_cast<T *>(bo.second));
  }

  template<typename T>
  void
  release(cmd_bo<T>& bo)
  {
    release_impl(std::make_pair(bo.first, static_cast<void *>(bo.second)));
  }

private:
  cmd_bo<void>
  alloc_impl()
  {
    if (mCacheMaxSize) {
      // If caching is enabled first look up in the BO cache
      std::lock_guard<std::mutex> lock(mCacheMutex);
      if (!mCmdBOCache.empty()) {
        auto bo = mCmdBOCache.back();
        mCmdBOCache.pop_back();
        return bo;
      }
    }

    auto execHandle = mDevice->alloc_bo(mBOSize, XCL_BO_FLAGS_EXECBUF);
    return std::make_pair(execHandle, mDevice->map_bo(execHandle, true));
  }

  void
  release_impl(const cmd_bo<void> &bo)
  {
    if (mCacheMaxSize) {
      // If caching is enabled and BO cache is not fully populated add this the cache
      std::lock_guard<std::mutex> lock(mCacheMutex);
      if (mCmdBOCache.size() < mCacheMaxSize) {
        mCmdBOCache.push_back(bo);
        return;
      }
    }
    destroy(bo);
  }

  void
  destroy(const cmd_bo<void> &bo)
  {
    mDevice->unmap_bo(bo.first, bo.second);
    mDevice->free_bo(bo.first);
  }
};

} // xrt_core

#ifdef _WIN32
# pragma warning( pop )
#endif

#endif
