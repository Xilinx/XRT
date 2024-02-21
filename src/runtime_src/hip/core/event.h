// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_event_h
#define xrthip_event_h

#include "stream.h"
#include "common.h"
#include "xrt/xrt_kernel.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace xrt::core::hip {
class command
{
public:
  enum class state : uint8_t
  {
    init,
    recorded,
    running,
    completed,
    error,
    abort
  };

  enum class type : uint8_t
  {
    event,
    buffer_copy,
    kernel_start
  };

private:
  std::shared_ptr<stream> cstream;
  uint64_t ctime;
  type ctype;
  state cstate;

public:
  virtual bool submit() = 0;
  virtual bool wait() = 0;
};

class event : public command
{
private:
  std::vector<command*> dep_commands;
  std::vector<command*> chain_commands;

public:
  bool submit();
  bool wait();
};

class kernel_start : public command
{
private:
  //std::shared_ptr<function> func;
  xrt::run r;

public:
  kernel_start(xrt::kernel &, void* args); //creates run object
  bool submit();
  bool wait();

};

class copy_buffer : public command
{
private:
  //std::shared_ptr<buffer> buff;
  //direction cdirection;

public:
  bool submit();
  bool wait();

};

} // xrt::core::hip

#endif
