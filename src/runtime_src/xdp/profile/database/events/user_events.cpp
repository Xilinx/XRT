/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#define XDP_SOURCE

#include "xdp/profile/database/events/user_events.h"

namespace xdp {

  // **************************
  // User event definitions
  // **************************

  UserMarker::UserMarker(uint64_t s_id, double ts, uint64_t l) : 
    VTFEvent(s_id, ts, USER_MARKER), label(l)
  {
  }

  UserMarker::~UserMarker()
  {
  }

  void UserMarker::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    if (label != 0) fout << "," << label ;
    fout << std::endl ;
  }

  UserRange::UserRange(uint64_t s_id, double ts, bool s, 
		       uint64_t l, uint64_t tt) :
    VTFEvent(s_id, ts, USER_RANGE), isStart(s), label(l), tooltip(tt)
  {
  }

  UserRange::~UserRange()
  {
  }  

  void UserRange::dump(std::ofstream& fout, uint32_t bucket)
  {
    VTFEvent::dump(fout, bucket) ;
    if (isStart) 
    {
      fout << "," << label << "," << tooltip ;
    }

    fout << std::endl ;
  }

} // end namespace xdp
