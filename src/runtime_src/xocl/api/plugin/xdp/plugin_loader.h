/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

// This file contains the function that will load all of the OpenCL level
//  profiling and application debug plugins (except kernel debug)

#ifndef OPENCL_PLUGIN_LOADER_DOT_H
#define OPENCL_PLUGIN_LOADER_DOT_H

namespace xdp {
namespace plugins {

  bool load();

} // namespace loader
} // namespace xdp

#endif
