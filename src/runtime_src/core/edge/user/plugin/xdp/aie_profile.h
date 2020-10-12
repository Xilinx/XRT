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

#ifndef AIE_PROFILE_DOT_H
#define AIE_PROFILE_DOT_H

namespace xdpaieprofile {

  void load_xdp_aie_plugin();
  void register_aie_callbacks(void* handle);
  void warning_aie_callbacks();

} // end namespace xdpaieprofile

namespace xdpaiectr {
  void update_aie_device(void* handle);
  void end_aie_ctr_poll(void* handle);
}

#endif
