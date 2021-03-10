/**
* Copyright (C) 2018-2020 Xilinx, Inc
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

#ifndef xocl_core_stream_h_
#define xocl_core_stream_h_

#include "xocl/core/object.h"
#include "xocl/core/refcount.h"
#include "xocl/core/property.h"

#include "xrt/device/device.h"

namespace xocl {
//class stream for qdma and other streaming purposes.
class stream : public _cl_stream // TODO: public refcount
{  
  using stream_opt_type = xrt_xocl::hal::StreamOptType;
  using stream_flags_type = property_object<cl_stream_flags>;
  using stream_attributes_type = property_object<cl_stream_attributes>;
protected:
  using stream_handle = xrt_xocl::hal::StreamHandle;
  using stream_xfer_flags = xrt_xocl::hal::StreamXferFlags;
  using stream_xfer_req = xrt_xocl::hal::StreamXferReq;

public:
  stream(stream_flags_type flags, stream_attributes_type attr, cl_mem_ext_ptr_t* ext);
private:
  unsigned int m_uid = 0;
  stream_flags_type m_flags {0};
  stream_attributes_type m_attrs {0};
  cl_mem_ext_ptr_t* m_ext {nullptr};
  stream_handle m_handle {0};
  device* m_device {nullptr};
  int m_connidx = -1;
public:
  int get_stream(device* device); 
  int poll_stream(xrt_xocl::device::stream_xfer_completions *comps, int min, int max, int *actual, int timeout); 
  int set_stream_opt(int type, uint32_t val);
  ssize_t read(void* ptr, size_t size, stream_xfer_req* req );
  ssize_t write(const void* ptr, size_t size, stream_xfer_req* req);
  int close();
};

//class stream_mem for streaming memory allocs.
class stream_mem : public _cl_stream_mem //TODO: public refcount
{   
  using stream_buf_handle = xrt_xocl::hal::StreamBufHandle;
  using stream_buf = xrt_xocl::hal::StreamBuf;
public:
  size_t m_size {0};
  stream_buf_handle m_handle {0};
  stream_buf m_buf {nullptr};
public:
  stream_mem(size_t size):m_size(size){};
public:
  int get(device* device);
  stream_buf map() {return m_buf;};
  void unmap() { /*do nothing*/ };
};

} //xocl

#endif
