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

#ifndef XDP_AIE_TRACE_H
#define XDP_AIE_TRACE_H

namespace xdp {
namespace aie {

  void update_device(void* handle);
  void flush_device(void* handle);

namespace trace {

  void load();
  void register_callbacks(void* handle);
  void warning_function();
  int  error_function();
} // end namespace trace
} // end namespace aie
} // end namespace xdp

#endif

