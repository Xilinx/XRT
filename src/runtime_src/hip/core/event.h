// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_event_h
#define xrthip_event_h

#include "command.h"

#include <condition_variable>
#include <mutex>
#include <vector>

namespace xrt::core::hip {

class event : public command
{
private:
  std::shared_ptr<std::condition_variable> cv;
  std::shared_ptr<std::mutex> mtx;
  bool etype; //Notify wait
  std::vector<command*> chain_commands;

public:
  bool submit();
  bool wait();
};

} // xrt::core::hip

#endif