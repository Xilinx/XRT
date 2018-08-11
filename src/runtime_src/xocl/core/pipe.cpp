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

#include "pipe.h"
#include "xocl/core/context.h"
#include "xocl/core/device.h"

namespace xocl { namespace pmd {

pipe::
pipe(context* ctx, device* dev, cl_mem_flags flags, cl_uint max_packets, cl_pipe_attributes attributes)
  : m_context(ctx), m_device(dev), m_strm(0)
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  XOCL_DEBUG(std::cout,"xocl::pmd::pipe::pipe(",m_uid,")\n");

  auto dir = (flags == CL_MEM_RTE_MBUF_WRITE_ONLY)
    ? xrt::device::direction::HOST2DEVICE 
    : xrt::device::direction::DEVICE2HOST;

  (void) dir;

  //m_strm = dev->get_xrt_device()->openStream(max_packets,attributes,dir);
}

pipe::
~pipe()
{
  XOCL_DEBUG(std::cout,"xocl::pmd::pipe::~pipe(",m_uid,")\n");
  //m_device->get_xrt_device()->closeStream(m_strm);
}

rte_mbuf*
pipe::
acquirePacket() const
{
    return nullptr;
  //return m_device->get_xrt_device()->acquirePacket();
}

size_t
pipe::
send(rte_mbuf** buf, size_t count) const
{
    return 0;
  //return m_device->get_xrt_device()->send(m_strm,buf,count);
}

size_t
pipe::
recv(rte_mbuf** buf, size_t count) const
{
    return 0;
  //return m_device->get_xrt_device()->recv(m_strm,buf,count);
}

}} // xocl




