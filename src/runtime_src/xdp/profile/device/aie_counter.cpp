/**
 * Copyright (C) 2019 Xilinx, Inc
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

#include "aie_counter.h"
#include "tracedefs.h"
#include <vector>
#include <iomanip>

#include <boost/algorithm/string.hpp>

namespace xdp {

AIECounter::AIECounter(Device* handle, uint64_t index, debug_ip_data* data)
    : ProfileIP(handle, index, data),
      mMajorVersion(0),
      mMinorVersion(0),
      mID(0),
      mColumn(0),
      mRow(0),
      mCounterNumber(0),
      mStartEvent(0),
      mEndEvent(0),
      mResetEvent(0),
      mClockFreqMhz(1000.0)
{
  if (data) {
    mMajorVersion = data->m_major;
    mMinorVersion = data->m_minor;
  }
}

void AIECounter::init()
{
  // TBD
}

void AIECounter::showProperties()
{
  std::ostream *outputStream = (out_stream) ? out_stream : (&(std::cout));
  (*outputStream) << " AIE Counter " << std::endl;
  ProfileIP::showProperties();
}

void AIECounter::showStatus()
{
  // TBD
}

}   // namespace xdp
