/**
 * Copyright (C) 2018-2019 Xilinx, Inc
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

#include "stream.h"
#include "device.h"

namespace xocl { 

stream::
stream(stream::stream_flags_type flags, stream::stream_attributes_type attrs, cl_mem_ext_ptr_t* ext)
  : m_flags(flags), m_attrs(attrs), m_ext(ext) 
{
  static unsigned int uid_count = 0;
  m_uid = uid_count++;

  XOCL_DEBUG(std::cout,"xocl::stream::stream(): ",m_uid,"\n");
}

int
stream::
stream::get_stream(device* device)
{
  m_device = device;
  return device->get_stream(m_flags, m_attrs, m_ext, &m_handle, m_connidx);
}

ssize_t 
stream
::read(void* ptr, size_t size, stream_xfer_req* req)
{
  return m_device->read_stream(m_handle, ptr, size, req);
}

ssize_t 
stream
::write(const void* ptr, size_t size, stream_xfer_req* req)
{
  return m_device->write_stream(m_handle, ptr, size, req);
}

int
stream::
stream::close()
{
  assert(m_connidx!=-1);
  return m_device->close_stream(m_handle,m_connidx);
}


int 
stream_mem::
stream_mem::get(device* device)
{
  m_buf = device->alloc_stream_buf(m_size,&m_handle);
  return 0;
}

} //xocl
