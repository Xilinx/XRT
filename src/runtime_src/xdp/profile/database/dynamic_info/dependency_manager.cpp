/**
 * Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved
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

#include "xdp/profile/database/dynamic_info/dependency_manager.h"

namespace xdp {

  void DependencyManager::addOpenCLMapping(uint64_t openclId,
                                           uint64_t endXDPEventId,
                                           uint64_t startXDPEventId)
  {
    std::lock_guard<std::mutex> lock(openclLock);

    openclEventMap[openclId] = std::make_pair(endXDPEventId, startXDPEventId);
  }

  std::pair<uint64_t, uint64_t>
  DependencyManager::lookupOpenCLMapping(uint64_t openclId)
  {
    std::lock_guard<std::mutex> lock(openclLock);

    auto iter = openclEventMap.find(openclId);

    if (iter == openclEventMap.end())
      return std::make_pair(0, 0);
    return (*iter).second;
  }

  void DependencyManager::addDependency(uint64_t id, uint64_t dependency)
  {
    std::lock_guard<std::mutex> lock(depLock);

    if (dependencies.find(id) == dependencies.end()) {
      std::vector<uint64_t> blank;
      dependencies[id] = blank;
    }
    dependencies[id].push_back(dependency);
  }

  std::map<uint64_t, std::vector<uint64_t>>
  DependencyManager::copyDependencyMap()
  {
    std::lock_guard<std::mutex> lock(depLock);

    return dependencies; // Explicitly make a copy
  }

} // end namespace xdp
