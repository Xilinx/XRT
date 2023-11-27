/**
 * Copyright (C) 2022-2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef STRING_TABLE_DOT_H
#define STRING_TABLE_DOT_H

#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <string>

#include "xdp/config.h"

namespace xdp {

  class StringTable
  {
  private:
    std::map<std::string, uint64_t> table;
    uint64_t currentId = 1; // Start at 1 so we can use 0 as a special value

    std::mutex dataLock; // Protects "table" map and currentId
  public:
    StringTable() = default;
    ~StringTable() = default;

    XDP_CORE_EXPORT uint64_t addString(const std::string& value);
    XDP_CORE_EXPORT void dumpTable(std::ofstream& fout);
  };

} // end namespace xdp

#endif
