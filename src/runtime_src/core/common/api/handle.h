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
};

template <typename HandleType, typename ImplType>
class handle_map<HandleType, std::shared_ptr<ImplType>>
{
  mutable std::mutex mutex;
  std::map<HandleType, std::shared_ptr<ImplType>> handles;

  std::shared_ptr<ImplType>
  get_no_lock(HandleType handle) const
  {
    auto itr = handles.find(handle);
    return (itr == handles.end())
      ? nullptr
      : itr->second;
  }

public:
  const std::shared_ptr<ImplType>&
  get_or_error(HandleType handle) const
  {
    std::lock_guard<std::mutex> lk(mutex);
    auto itr = handles.find(handle);
    if (itr == handles.end())
      throw xrt_core::error(-EINVAL, "No such handle");

    return (*itr).second;
  }

  std::shared_ptr<ImplType>
  get(HandleType handle) const
  {
    std::lock_guard<std::mutex> lk(mutex);
    auto itr = handles.find(handle);
    return (itr == handles.end())
      ? nullptr
      : (*itr).second;
  }

  void
  add(HandleType handle, std::shared_ptr<ImplType>&& impl)
  {
    std::lock_guard<std::mutex> lk(mutex);
    handles.emplace(handle, std::move(impl));
  }

  void
  remove_or_error(HandleType handle)
  {
    std::lock_guard<std::mutex> lk(mutex);
    if (handles.erase(handle) == 0)
      throw xrt_core::error(-EINVAL, "No such handle");
  }

  void
  remove(HandleType handle)
  {
    std::lock_guard<std::mutex> lk(mutex);
    handles.erase(handle);
  }

  size_t
  count(HandleType handle) const
  {
    std::lock_guard<std::mutex> lk(mutex);
    return handles.count(handle);
  }
};

template <typename HandleType, typename ImplType>
class handle_map<HandleType, std::unique_ptr<ImplType>>
{
  mutable std::mutex mutex;
  std::map<HandleType, std::unique_ptr<ImplType>> handles;

public:
  ImplType*
  get_or_error(HandleType handle) const
  {
    std::lock_guard<std::mutex> lk(mutex);
    auto itr = handles.find(handle);
    if (itr == handles.end())
      throw xrt_core::error(-EINVAL, "No such handle");
    return (*itr).second.get();
  }

  ImplType*
  get(HandleType handle) const
  {
    std::lock_guard<std::mutex> lk(mutex);
    auto itr = handles.find(handle);
    return (itr == handles.end())
      ? nullptr
      : (*itr).second.get();
  }

  void
  add(HandleType handle, std::unique_ptr<ImplType>&& impl)
  {
    std::lock_guard<std::mutex> lk(mutex);
    handles.emplace(handle, std::move(impl));
  }

  void
  remove_or_error(HandleType handle)
  {
    std::lock_guard<std::mutex> lk(mutex);
    if (handles.erase(handle) == 0)
      throw xrt_core::error(-EINVAL, "No such handle");
  }

  void
  remove(HandleType handle)
  {
    std::lock_guard<std::mutex> lk(mutex);
    handles.erase(handle);
  }

  size_t
  count(HandleType handle) const
  {
    std::lock_guard<std::mutex> lk(mutex);
    return handles.count(handle);
  }
};

} // xrt_core
