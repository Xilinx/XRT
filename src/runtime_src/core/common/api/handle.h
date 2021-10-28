/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2021 Xilinx, Inc. All rights reserved.
 */

#include "core/common/error.h"

#include <map>
#include <mutex>

namespace xrt_core {

// Custom mutex protected handle map for managing C-API handles that
// must be explicitly opened and closed.  For some of the C-APIs,
// the implmentation is a managed shared object so when the handle
// is removed from the map, then underlying implementation might
// still be in use if it was shared.  The sharing of implmentation
// requires that the handles are stored as opposed to be raw opaque
// pointers that are reinterpreted.
template <typename HandleType, typename ImplType>
class handle_map
{
  std::mutex mutex;
  std::map<HandleType, ImplType> handles;

public:
  // get() - Get implementation for handle
  ImplType
  get_impl(HandleType handle)
  {
    std::lock_guard<std::mutex> lk(mutex);
    auto itr = handles.find(handle);
    if (itr == handles.end())
      throw xrt_core::error(-EINVAL, "No such handle");
    return itr->second;
  }

  // add() - Record handle impl pair
  void
  add(HandleType handle, ImplType&& impl)
  {
    std::lock_guard<std::mutex> lk(mutex);
    handles.emplace(handle, std::move(impl));
  }

  // remove() - Remove handle from map
  void
  remove(HandleType handle)
  {
    std::lock_guard<std::mutex> lk(mutex);
    if (handles.erase(handle) == 0)
    throw xrt_core::error(-EINVAL, "No such handle");
  }
};

} // xrt_core
