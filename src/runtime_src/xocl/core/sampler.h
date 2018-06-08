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

#ifndef xocl_core_sampler_h_
#define xocl_core_sampler_h_

#include "xocl/core/object.h"
#include "xocl/core/refcount.h"

namespace xocl {

class sampler : public refcount, public _cl_sampler
{
public:
  sampler(context* ctx, bool norm_mode, cl_addressing_mode addr_mode, cl_filter_mode filter_mode);
  virtual ~sampler();

  context*
  get_context() const 
  {
    return m_context.get();
  }

  bool
  get_norm_mode() const 
  {
    return m_norm_mode;
  }

  cl_addressing_mode
  get_addr_mode() const
  {
    return m_addr_mode;
  }

  cl_filter_mode
  get_filter_mode() const
  {
    return m_filter_mode;
  }

private:
  ptr<context> m_context;
  bool m_norm_mode;
  cl_addressing_mode m_addr_mode;
  cl_filter_mode m_filter_mode;
};

} // xocl

#endif


