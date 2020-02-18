/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef core_common_scope_guard_h
#define core_common_scope_guard_h

#include <utility>
#include <type_traits>

namespace xrt_core { 

/**
 * class scope_guard - RAII for fundamental types that need to be 
 * terminated / cleaned-up at scope exit
 */
template <typename ValueType, typename ExitFunction>
class scope_guard
{
  static_assert(std::is_fundamental<ValueType>::value,"Invalid ValueType");
  ValueType value;
  ExitFunction exfcn;
public:
  scope_guard(ValueType v, ExitFunction&& exf)
    : value(v), exfcn(std::move(exf))
  {}

  ~scope_guard()
  {
    try {
      exfcn(value);
    }
    catch (...) {
    }
  }

  ValueType
  get() const
  {
    return value;
  }
};

} // xrt_core


#endif
