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

#define XDP_CORE_SOURCE

#include "xdp/profile/database/dynamic_info/string_table.h"

namespace xdp {

  uint64_t StringTable::addString(const std::string& value)
  {
    std::lock_guard<std::mutex> lock(dataLock);

    if (table.find(value) == table.end())
      table[value] = currentId++;

    return table[value];
  }

  void StringTable::dumpTable(std::ofstream& fout)
  {
    std::lock_guard<std::mutex> lock(dataLock);

    for (auto& s : table)
      fout << s.second << "," << s.first.c_str() << "\n";
  }

} // end namespace xdp
