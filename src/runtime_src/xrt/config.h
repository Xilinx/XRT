/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#ifndef xrt_config_h_
#define xrt_config_h_

#include "xrt/util/config_reader.h"

#ifdef _WIN32
# ifdef XRT_SOURCE
#  define XRT_EXPORT __declspec(dllexport)
# else
#  define XRT_EXPORT __declspec(dllimport)
# endif
#endif
#ifdef __GNUC__
# ifdef XRT_SOURCE
#  define XRT_EXPORT __attribute__ ((visibility("default")))
# else
#  define XRT_EXPORT
# endif
#endif

#ifndef XRT_EXPORT
# define XRT_EXPORT
#endif

#ifdef __GNUC__
# define XRT_UNUSED __attribute__((unused))
#endif

#ifdef _WIN32
# define XRT_UNUSED
#endif

#endif
