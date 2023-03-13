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

#ifndef DEPENDENCY_MANAGER_DOT_H
#define DEPENDENCY_MANAGER_DOT_H

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace xdp {

  class DependencyManager
  {
  private:
    // For each OpenCL event ID (generated on the XRT side), map
    // to the start and end XDP event IDs (used by the XDP database)
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> openclEventMap;

    // For each XDP event Id, what are the list of OpenCL Id events
    // that it is dependent on
    std::map<uint64_t, std::vector<uint64_t>> dependencies;

    std::mutex openclLock; // Protects the "openclEventMap" map
    std::mutex depLock;    // Protects the "dependencies" map

  public:
    DependencyManager() = default;
    ~DependencyManager() = default;

    void addOpenCLMapping(uint64_t openclId,
                          uint64_t endXDPEventId, uint64_t startXDPEventId);
    std::pair<uint64_t, uint64_t> lookupOpenCLMapping(uint64_t openclId);

    void addDependency(uint64_t id, uint64_t dependency);
    std::map<uint64_t, std::vector<uint64_t>> copyDependencyMap();
  };

} // end namespace xdp

#endif
