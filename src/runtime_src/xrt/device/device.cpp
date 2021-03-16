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
#include "device.h"

#include "xrt/util/task.h"
#include "xrt/util/event.h"

#include <future>
#include <cstring> // for std::memset

namespace xrt_xocl {

std::ostream&
device::
printDeviceInfo(std::ostream& ostr) const
{
  return m_hal->printDeviceInfo(ostr);
}

void
device::
close()
{
  if (!m_close_callbacks.empty()) {
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto& cb : m_close_callbacks)
      cb();
  }

  m_hal->close();
}

} // xrt
