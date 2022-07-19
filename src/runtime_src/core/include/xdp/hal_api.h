/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc - All rights reserved
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

#ifndef XDP_HAL_API_DOT_H
#define XDP_HAL_API_DOT_H

#include <cstdint>

// The declarations used in this file are shared between the different shims
// and the XDP library.

namespace xdp {

// This enum is used in the HAL API Interface to access hardware counters from
// host code.  We are passing this enum through a callback function that is
// dynamically linked via dlsym.  The enum is treated as an unsigned int,
// so it is explicitly called out in the declaration.

enum HalInterfaceCallbackType : unsigned int {
  start_device_profiling  = 0,
  create_profile_results  = 1,
  get_profile_results     = 2,
  destroy_profile_results = 3
};

// These structs were originally intended to provide support for other
// types of C callbacks dynamically linked that would be accessed by
// user functions.

struct CBPayload {
  uint64_t idcode;
  void* deviceHandle;
};

struct ProfileResultsCBPayload
{
  struct CBPayload basePayload;
  void* results;
};

} // end namespace xdp

#endif
