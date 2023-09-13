// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __PSCONTEXT_H_
#define __PSCONTEXT_H_

#include <memory> 

/*
 * PS Context Data Structure included by user PS kernel code
 */

class pscontext {
public:
 pscontext()
   : pimpl{std::make_shared<pscontext::impl>()} {}
  virtual ~pscontext() {}
 
protected:
  struct impl;
  std::shared_ptr<impl> pimpl;
};

struct pscontext::impl {
private:
  bool aie_profile_en;
};

#endif
