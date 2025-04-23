/**
 * Copyright (C) 2020 Xilinx, Inc
 * Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
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

namespace xdp::aie {
  void update_device(void* handle, bool hw_context_flow);
  void flush_device(void* handle);
  void finish_flush_device(void* handle);

namespace trace {
  void load();
  void register_callbacks(void* handle);
  void warning_function();
  int  error_function();

} // end namespace trace
} // end namespace xdp::aie

#endif

