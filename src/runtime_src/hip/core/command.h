// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_command_h
#define xrthip_command_h

#include "stream.h"

#include <memory>


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

} // xrt::core::hip

#endif  