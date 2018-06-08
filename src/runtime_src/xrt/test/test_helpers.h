/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "xrt/device/hal.h"
#include "xrt/device/device.h"
#include <memory>
#include <vector>
#include <iosfwd>
#include <chrono>

namespace xrt { 

class device;

namespace test {

template <typename T> 
class AlignedAllocator {
  void *mBuffer;
  size_t mCount;
public:
  T*
  getBuffer() 
  {
    return (T *)mBuffer;
  }

  size_t 
  size() const 
  {
    return mCount * sizeof(T);
  }

  AlignedAllocator(size_t alignment, size_t count) 
    : mBuffer(0), mCount(count) 
  {
    if (posix_memalign(&mBuffer, alignment, count * sizeof(T))) {
      mBuffer = 0;
    }
  }

  ~AlignedAllocator() 
  {
    if (mBuffer)
      free(mBuffer);
  }
};

class Timer {
    std::chrono::high_resolution_clock::time_point mTimeStart;
    std::chrono::high_resolution_clock::time_point mTimeEnd;
public:
    Timer() {
        reset();
    }
    double stop() {
        mTimeEnd = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::duration<double>>(mTimeEnd- mTimeStart).count();
    }
    void reset() {
        mTimeStart = std::chrono::high_resolution_clock::now();
        mTimeEnd = mTimeStart;
    }
};

inline const char* 
emptyOrValue(const char* cstr)
{
  return cstr ? cstr : "";
}

// Construct xrt::device objects from loaded hal::device drivers
// if predicate is satisfied
// Predicate: [](const xrt::hal::device&) { ... }
template <typename UnaryPredicate>
std::vector<xrt::device>
loadDevices(UnaryPredicate pred)
{
  std::vector<xrt::device> devices;
  auto haldevices = hal::loadDevices();
  for (auto& hal : haldevices) { // unique_ptr<hal::device>
    if (pred(*hal.get()))
      devices.emplace_back(std::move(hal));
  }
  return std::move(devices);
}

// construct xrt::device objects from loaded hal::device drivers
inline std::vector<xrt::device>
loadDevices()
{
  return loadDevices([](const xrt::hal::device& hal){return true;});
}

}}


