/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef AIE_STATUS_DOT_H
#define AIE_STATUS_DOT_H

namespace xdp {
namespace aie {

namespace status {
  void load();
  void register_callbacks(void* handle);
  void warning_callbacks();
} // end namespace status

namespace sts {
  void update_device(void* handle, bool hw_context_flow);
  void end_poll(void* handle);
} // end namespace sts

} // end namespace aie
} // end namespace xdp

#endif
