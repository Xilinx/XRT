// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#ifndef xrthip_buffer_copy_h
#define xrthip_buffer_copy_h

#include "command.h"

namespace xrt::core::hip {

class copy_buffer : public command
{
private:
  std::shared_ptr<function> func;
  xrt::run r;

public:
  kernel_start(xrt::kernel &, void* args); //creates run object
  bool submit();
  bool wait();

};

} // xrt::core::hip

#endif  