/**
 * Copyright (C) 2021 Xilinx, Inc
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef PL_DEADLOCK_DOT_H
#define PL_DEADLOCK_DOT_H

namespace xdp::hw_emu::pl_deadlock {

  void load();
  void register_callbacks(void* handle);
  void warning_callbacks();

  void update_device(void* handle);

} // end namespace xdp::hw_emu::pl_deadlock

#endif
