// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef __PSCONTEXT_H_
#define __PSCONTEXT_H_

#include "xrt.h"
#include <memory>

/*
 * PS Context Data Structure included by user PS kernel code
 */

class pscontext_impl;
class pscontext {
public:
  pscontext();
  virtual ~pscontext();

  pscontext(pscontext&& rhs);
  pscontext& operator=(pscontext&& rhs);
 
protected:
  std::unique_ptr<pscontext_impl> pimpl;
};

typedef pscontext* (* kernel_init_t)(xclDeviceHandle device, const uuid_t &uuid);
typedef int (* kernel_fini_t)(pscontext *xrtHandles);

#endif
