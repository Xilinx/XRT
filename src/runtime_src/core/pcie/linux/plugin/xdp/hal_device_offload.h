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

#ifndef HAL_DEVICE_OFFLOAD_DOT_H
#define HAL_DEVICE_OFFLOAD_DOT_H

namespace xdphaldeviceoffload {

  void load_xdp_hal_device_offload() ;
  void register_hal_device_offload_functions(void* handle) ;
  void hal_device_offload_warning_function() ;

} // end xdphaldeviceoffload

namespace xdphal {

  void flush_device(void* handle) ;
  void update_device(void* handle) ;

} // end namespace xdphal

#endif
