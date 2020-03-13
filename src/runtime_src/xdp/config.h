/*
 * Copyright (C) 2015-2019, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) APIs
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

#ifndef xdp_config_h_
#define xdp_config_h_

//------------------Enable dynamic linking on windows-------------------------// 

#ifdef _WIN32
  #ifdef XDP_SOURCE
    #define XDP_EXPORT __declspec(dllexport)
  #else
    #define XDP_EXPORT __declspec(dllimport)
  #endif  
#endif
#ifdef __GNUC__
  #ifdef XDP_SOURCE
    #define XDP_EXPORT __attribute__ ((visibility("default")))
  #else
    #define XDP_EXPORT
  #endif
#endif

#ifndef XDP_EXPORT
  #define XDP_EXPORT
#endif

#endif
