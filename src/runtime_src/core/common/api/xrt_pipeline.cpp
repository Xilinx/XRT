/*
 * Copyright (C) 2020, Xilinx Inc - All rights reserved
 * Xilinx Runtime (XRT) Experimental APIs
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
// This file implements XRT xclbin APIs as declared in
// core/include/experimental/xrt_enqueue.h
#define XCL_DRIVER_DLL_EXPORT  // exporting xrt_pipeline.h
#define XRT_CORE_COMMON_SOURCE // in same dll as core_common
#include "core/include/experimental/xrt_pipeline.h"

#include "core/common/debug.h"

#include <memory>
#include <vector>
#include <functional>
#include <algorithm>

#ifdef _WIN32
# pragma warning( disable : 4244 )
#endif

namespace {

}

namespace xrt {

class pipeline_impl : public std::enable_shared_from_this<pipeline_impl>
{
  event_queue m_queue;
  unsigned long m_uid;
  std::vector<pipeline::stage> m_stages;

public:
  // Construct the pipeline implementation
  pipeline_impl(const xrt::event_queue& q)
    : m_queue(q)
  {
    static unsigned int count = 0;
    m_uid = count++;
    XRT_DEBUGF("pipeline_impl::pipeline_impl(%d)\n", m_uid);
  }

  // Event destructor added for debuggability.
  ~pipeline_impl()
  {
    XRT_DEBUGF("pipeline_impl::~pipeline_impl(%d)\n", m_uid);
  }

  xrt::event
  execute(xrt::event event)
  {
    for (auto& s : m_stages)
      event = s.enqueue(m_queue, {event});
    return event;
  }

  const pipeline::stage&
  add_stage(pipeline::stage&& s)
  {
    m_stages.push_back(std::move(s));
    return m_stages.back();
  }
};

} // xrt


////////////////////////////////////////////////////////////////
// xrt_enqueue C++ API implmentations (xrt_pipeline.h)
////////////////////////////////////////////////////////////////
namespace xrt {

pipeline::
pipeline(const xrt::event_queue& q)
  : m_impl(std::make_shared<pipeline_impl>(q))
{}

xrt::event
pipeline::
execute(xrt::event event)
{
  return m_impl->execute(event);
}

const pipeline::stage&
pipeline::
add_stage(pipeline::stage&& s)
{
  return m_impl->add_stage(std::move(s));
}


  
  
} // xrt
