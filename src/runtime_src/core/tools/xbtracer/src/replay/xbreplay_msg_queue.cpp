// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <cstring>
#include <iostream>
#include <replay/xbreplay_common.h>

namespace xrt::tools::xbtracer
{

xbreplay_msg_queue::
xbreplay_msg_queue(): ended(0) {}

void
xbreplay_msg_queue::
push(const std::shared_ptr<xbtracer_proto::Func>& value)
{
  {
    std::lock_guard<std::mutex> lock(mlock);
    queue.push(value);
  }
  cond.notify_one();
}

bool
xbreplay_msg_queue::
try_pop(std::shared_ptr<xbtracer_proto::Func>& result)
{
  std::lock_guard<std::mutex> lock(mlock);
  if (queue.empty())
    return false;
  result = queue.front();
  queue.pop();
  return true;
}

void
xbreplay_msg_queue::
wait_and_pop(std::shared_ptr<xbtracer_proto::Func>& result)
{
  std::unique_lock<std::mutex> lock(mlock);
  cond.wait(lock, [this]{ return !queue.empty() || ended; });
  if (!queue.empty()) {
    result = queue.front();
    queue.pop();
  }
}

bool
xbreplay_msg_queue::
empty()
{
  std::lock_guard<std::mutex> lock(mlock);
  return queue.empty();
}

void
xbreplay_msg_queue::
end_queue()
{
  std::lock_guard<std::mutex> lock(mlock);
  ended = 1;
}

} // namespace xrt::tools::xbtracer
