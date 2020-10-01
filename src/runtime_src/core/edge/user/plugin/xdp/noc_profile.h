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

#ifndef NOC_PROFILE_DOT_H
#define NOC_PROFILE_DOT_H

namespace xdpnocprofile {

  void load_xdp_noc_plugin();
  void register_noc_callbacks(void* handle);
  void warning_noc_callbacks();

} // end namespace xdpnocprofile

#endif
