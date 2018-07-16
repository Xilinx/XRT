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

#ifndef xocl_core_pmd_pipe_h_
#define xocl_core_pmd_pipe_h_

#include "xocl/core/object.h"
#include "xocl/core/refcount.h"

#include "xrt/device/device.h"

namespace xocl {

namespace pmd { class pipe; class notype;}

}

struct _cl_pipe : public xocl::object<xocl::pmd::pipe,xocl::pmd::notype,_cl_pipe> {};

namespace xocl { namespace pmd {

/**
 * The pmd_pipe class is for Huawei only.
 */

class pipe : public refcount, public _cl_pipe
{
  using stream_handle = xrt::device::stream_handle;

public:
  pipe(context* ctx, device* dev, cl_mem_flags flags, cl_uint max_packets, cl_pipe_attributes attributes);
  virtual ~pipe();

  unsigned int
  get_uid() const 
  {
    return m_uid;
  }

  const device*
  get_device() const
  {
    return m_device.get();
  }

  rte_mbuf*
  acquirePacket() const;

  size_t
  send(rte_mbuf** buf, size_t count) const;

  size_t
  recv(rte_mbuf** buf, size_t count) const;

private:
  unsigned int m_uid = 0;
  ptr<context> m_context;
  ptr<xocl::device> m_device;
  stream_handle m_strm;
};

}} // pmd,xocl

#endif


