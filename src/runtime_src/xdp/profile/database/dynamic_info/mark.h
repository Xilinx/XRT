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

#ifndef MARK_DOT_H
#define MARK_DOT_H

#include <map>
#include <mutex>

namespace xdp {

  // For API tracking, we will encounter the start of an API event and the 
  // end of the API event through different callbacks which may not be
  // sequential.  When we are processing the end of the API, we must lookup
  // the corresponding start of the API event that was previously stored
  // so we can collect the information we need when we dump events.

  template <typename id_type, typename start_type>
  class APIMatch
  {
  private:
    // Each start event is identified by a unique uint64_t, given by the
    // database when stored away.
    std::map<id_type, start_type> map;

    std::mutex mapLock;
  public:
    void registerStart(id_type ID, start_type eventNum)
    {
      std::lock_guard<std::mutex> lock(mapLock);
      map[ID] = eventNum;
    }

    start_type lookupStart(id_type endID)
    {
      std::lock_guard<std::mutex> lock(mapLock);
      auto iter = map.find(endID);
      if (iter == map.end())
        return {0};

      start_type value = (*iter).second;
      map.erase(endID);
      return value;
    }
  };

} // end namespace xdp

#endif
