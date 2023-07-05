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

#ifndef SAMPLES_DOT_H
#define SAMPLES_DOT_H

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

#include "types.h"

namespace xdp {

  // Various portions of the designs will have sets of registers we can
  // read at regular intervals.  This class stores sets of these sampled
  // registers.  We instantiate a different instance of this object for
  // each class of register.
  class SampleContainer
  {
  private:
    std::vector<counters::Sample> samples;

    std::mutex containerLock; // Protects the "samples" vector

  public:
    SampleContainer() = default;
    ~SampleContainer() = default;

    inline void addSample(const counters::Sample& s)
    {
      std::lock_guard<std::mutex> lock(containerLock);
      samples.push_back(s);
    }
    inline std::vector<counters::Sample> getSamples()
    {
      std::lock_guard<std::mutex> lock(containerLock);
      return samples;
    }

  };

  class DoubleSampleContainer
  {
  private:
    std::vector<counters::DoubleSample> samples;

    std::mutex containerLock; // Protects the "samples" vector

  public:
    DoubleSampleContainer() = default;
    ~DoubleSampleContainer() = default;

    inline void addSample(const counters::DoubleSample& s)
    {
      std::lock_guard<std::mutex> lock(containerLock);
      samples.push_back(s);
    }
    inline std::vector<counters::DoubleSample> getSamples()
    {
      std::lock_guard<std::mutex> lock(containerLock);
      return samples;
    }

  };
} // end namespace xdp

#endif
