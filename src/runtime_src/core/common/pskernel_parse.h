/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef pskernel_parse_h_
#define pskernel_parse_h_

#include <elfutils/libdw.h>
#include <elfutils/libdwfl.h>
#include <ffi.h>
#include <dwarf.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <unordered_map>
#include <map>
#include <vector>
#include <limits>

namespace xrt_core { namespace pskernel {
/**
 * struct kernel_argument -
 */
struct kernel_argument
{
  static constexpr size_t no_index { std::numeric_limits<size_t>::max() };
  // numbering must match that of meta data addressQualifier
  enum class argtype { scalar = 0, global = 1 };
  enum class direction { input = 0, output = 1};

  std::string name;
  std::string hosttype;
  std::string port;
  size_t index = no_index;
  size_t offset = 0;
  size_t size = 0;
  size_t hostsize = 0;
  argtype type;
  direction dir;
  ffi_type ffitype;
};

std::vector<kernel_argument> extract_args (Dwarf_Die *die);
std::vector<kernel_argument> pskernel_parse(char *so_file, size_t size, const char *func_name);
std::vector<kernel_argument> pskernel_parse(const char *so_file, const char *func_name);

}}

#endif
