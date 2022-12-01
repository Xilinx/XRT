/**
 * Copyright (C) 2022 Xilinx, Inc
 * Author(s): Jeff Lin	<jefflin@amd.com>
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

#ifndef __PSCONTEXT_H_
#define __PSCONTEXT_H_

#include "xrt.h"
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

typedef pscontext* (* kernel_init_t)(xclDeviceHandle device, const uuid_t &uuid);
typedef int (* kernel_fini_t)(pscontext *xrtHandles);

#endif
