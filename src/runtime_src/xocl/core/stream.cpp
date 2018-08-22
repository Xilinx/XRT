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
stream(stream::stream_flags_type flags, stream::stream_attributes_type attrs)
  : m_flags(flags), m_attrs(attrs) 
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
  return device->get_stream(m_flags, m_attrs, &m_handle);
}

ssize_t 
stream
::read(device* device, void* ptr, size_t offset, size_t size, stream_xfer_flags flags)
{
  if(device != m_device)
    throw xocl::error(CL_INVALID_OPERATION,"Stream read on a bad device");
  return m_device->read_stream(m_handle, ptr, offset, size, flags);
}

ssize_t 
stream
::write(device* device, const void* ptr, size_t offset, size_t size, stream_xfer_flags flags)
{
  if(device != m_device)
    throw xocl::error(CL_INVALID_OPERATION,"Stream write on a bad device");
  return m_device->write_stream(m_handle, ptr, offset, size, flags);
}

int
stream::
stream::close()
{
  return m_device->close_stream(m_handle);
}


int 
stream_mem::
stream_mem::get(device* device)
{
  m_buf = device->alloc_stream_buf(m_size,&m_handle);
  return 0;
}

} //xocl
