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

#include "memory.h"
#include "xocl/core/memory.h"
#include "xocl/core/error.h"
#include "xocl/api/api.h"

#include <bitset>
#include <string>
#include <map>

namespace xocl { namespace detail {

namespace memory {

static std::string
memFlagsToString(cl_mem_flags flags)
{
  std::string retval;
  static std::map<unsigned long, std::string> f2s = {
    { CL_MEM_READ_WRITE,      "CL_MEM_READ_WRITE" }
   ,{ CL_MEM_WRITE_ONLY,      "CL_MEM_WRITE_ONLY" }
   ,{ CL_MEM_READ_ONLY,       "CL_MEM_READ_ONLY" }
   ,{ CL_MEM_USE_HOST_PTR,    "CL_MEM_USE_HOST_PTR" }
   ,{ CL_MEM_ALLOC_HOST_PTR,  "CL_MEM_ALLOC_HOST_PTR" }
   ,{ CL_MEM_COPY_HOST_PTR,   "CL_MEM_COPY_HOST_PTR" }
   ,{ CL_MEM_HOST_WRITE_ONLY, "CL_MEM_HOST_WRITE_ONLY" }
   ,{ CL_MEM_HOST_READ_ONLY,  "CL_MEM_HOST_READ_ONLY" }
   ,{ CL_MEM_HOST_NO_ACCESS,  "CL_MEM_HOST_NO_ACCESS" }
  };

  std::bitset<10> bset(flags);
  int separator = 0;
  size_t idx = 0;
  while (idx < bset.size() && bset.test(idx)) {
    retval.append(separator,',').append(f2s[1<<idx]);
    separator = 1;
  }
  return retval;
}

void
validOrError(const cl_mem_flags xflags)
{
  // don't check xilinx ext flags
  auto flags = get_ocl_flags(xflags);

  const cl_mem_flags dev_access_flags =
    CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_REGISTER_MAP;
  const cl_mem_flags host_ptr_flags1 =
    CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR;
  const cl_mem_flags host_ptr_flags2 =
    CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR;
  const cl_mem_flags host_ptr_flags =
    host_ptr_flags1 | host_ptr_flags2;
  const cl_mem_flags host_access_flags =
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

  const cl_mem_flags valid_flags =
    dev_access_flags | host_access_flags | host_ptr_flags;

  if (flags & ~valid_flags)
    throw error(CL_INVALID_VALUE,"unexpected cl_mem_flags");

  if (std::bitset<10>(flags & dev_access_flags).count() > 1)
    throw error(CL_INVALID_VALUE,memFlagsToString(dev_access_flags));

  if (std::bitset<10>(flags & host_access_flags).count() > 1)
    throw error(CL_INVALID_VALUE,memFlagsToString(host_access_flags));

  if (std::bitset<10>(flags & host_ptr_flags1).count() > 1)
    throw error(CL_INVALID_VALUE,memFlagsToString(host_ptr_flags1));

  if (std::bitset<10>(flags & host_ptr_flags2).count() > 1)
    throw error(CL_INVALID_VALUE,memFlagsToString(host_ptr_flags2));
}

void
validHostPtrOrError(cl_mem_flags flags, const void* host_ptr)
{
  // CL_INVALID_HOST_PTR if host_ptr is NULL and CL_MEM_EXT_PTR_XILINX is set
  // In this case host_ptr is actually a ptr to some struct
  if (!host_ptr && (flags & CL_MEM_EXT_PTR_XILINX))
    throw error(CL_INVALID_HOST_PTR,"host_ptr may not be nullptr when CL_ME_EXT_PTR_XILINX is specified");

  // CL_INVALID_HOST_PTR if host_ptr is NULL and CL_MEM_USE_HOST_PTR
  // or CL_MEM_COPY_HOST_PTR are set in flags or if host_ptr is not
  // NULL but CL_MEM_COPY_HOST_PTR or CL_MEM_USE_HOST_PTR are not set
  // in flags.
  auto ubuf = get_host_ptr(flags,host_ptr); // adjust host_ptr based on ext flags
  if ( bool(ubuf) != bool(flags & (CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR)) )
    throw error(CL_INVALID_HOST_PTR,"bad host_ptr of mem use flags");

  if (auto ext_flags = get_xlnx_ext_flags(flags,host_ptr)) {
    if (!get_xlnx_ext_kernel(flags,host_ptr) && !(ext_flags & XCL_MEM_TOPOLOGY)) {
      auto ddr_bank_mask = XCL_MEM_DDR_BANK0 | XCL_MEM_DDR_BANK1 | XCL_MEM_DDR_BANK2 | XCL_MEM_DDR_BANK3;
      // Test that only one bank flag is set
      if (std::bitset<12>(ext_flags & ddr_bank_mask).count() > 1)
       throw xocl::error(CL_INVALID_VALUE,"Multiple bank flags specified");
    }
  }
}

static void
validAccessOrError(const cl_mem mem, const cl_mem_flags flags)
{
  if (xocl(mem)->get_flags() & ~flags &
      (CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS))
    throw error(CL_INVALID_OPERATION,"Invalid mem access flags");
}

static void
validMapFlagsOrError(const cl_mem mem, cl_map_flags mapflags)
{
  if ((mapflags & (CL_MAP_WRITE | CL_MAP_READ)) &&
      (mapflags & CL_MAP_WRITE_INVALIDATE_REGION))
    throw error(CL_INVALID_VALUE,"Mutually exclusive flags specified");

  if (mapflags & CL_MAP_READ)
    validAccessOrError(mem, CL_MEM_HOST_READ_ONLY);

  if (mapflags & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION))
    validAccessOrError(mem, CL_MEM_HOST_WRITE_ONLY);
}

void
validOrError(const cl_mem mem, size_t offset, size_t size)
{
  if (size==0)
    throw error(CL_INVALID_VALUE,"size of 0");
  if (offset+size > xocl(mem)->get_size())
    throw error(CL_INVALID_VALUE,"offset+size > mem size");
}

void
validOrError(const cl_mem mem, cl_map_flags mapflags, size_t offset, size_t size)
{
  validOrError(mem);
  validMapFlagsOrError(mem,mapflags);
  validOrError(mem,offset,size);
}

void
validOrError(const cl_mem mem
             ,const size_t* buffer_origin, const size_t* host_origin, const size_t* region
             ,size_t buffer_row_pitch,size_t buffer_slice_pitch
             ,size_t host_row_pitch,size_t host_slice_pitch)
{
  // CL_INVALID_VALUE if any region array element is 0
  if (!region || region[0]==0 || region[1]==0 || region[2]==0)
    throw error(CL_INVALID_VALUE,"One or more region values are zero");

  // CL_INVALID_VALUE if buffer_row_pitch is not 0 and is less than region[0]
  if (buffer_row_pitch && buffer_row_pitch < region[0])
    throw error(CL_INVALID_VALUE,"buffer_row_pitch error");

  // CL_INVALID_VALUE if host_row_pitch is not 0 and is less than region[0]
  if (host_row_pitch && host_row_pitch < region[0])
    throw error(CL_INVALID_VALUE,"host_row_pitch error");

  // CL_INVALID_VALUE if buffer_slice_pitch is not 0 and is less than
  // region[1] * buffer_row_pitch and not a multiple of
  // buffer_row_pitch
  if (buffer_slice_pitch && (buffer_slice_pitch < region[1]*buffer_row_pitch) && (buffer_slice_pitch % buffer_row_pitch))
    throw error(CL_INVALID_VALUE,"buffer_slice_pitch error");

  // CL_INVALID_VALUE if host_slice_pitch is not 0 and is less than
  // region[1] * host_row_pitch and not a multiple of host_row_pitch
  if (host_slice_pitch && (host_slice_pitch < region[1]*host_row_pitch) && (host_slice_pitch % host_row_pitch))
    throw error(CL_INVALID_VALUE,"host_slice_pitch error");


  size_t buffer_origin_in_bytes =
    buffer_origin[2]*buffer_slice_pitch+
    buffer_origin[1]*buffer_row_pitch+
    buffer_origin[0];
  size_t buffer_extent_in_bytes =
    buffer_origin_in_bytes+
    region[2]*buffer_slice_pitch+
    region[1]*buffer_row_pitch+
    region[0];

  // CL_INVALID_VALUE if the region being written specified by
  // (buffer_origin, region, buffer_row_pitch, buffer_slice_pitch) is
  // out of bounds.
  if (buffer_extent_in_bytes > (xocl(mem)->get_size()))
    throw error(CL_INVALID_VALUE,"buffer_origin, region, buffer_row_pitch, buffer_slice_pitch out of bounds");

}

void
validSubBufferOffsetAlignmentOrError(const cl_mem mem, const cl_device_id device)
{
  // CL_MISALIGNED_SUB_BUFFER_OFFSET if buffer is a sub-buffer object
  // and offset specified when the sub-buffer object is created is not
  // aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device
  // associated with queue.
  if (xocl(mem)->is_sub_buffer()) {
    cl_uint align = 0;
    api::clGetDeviceInfo(device,CL_DEVICE_MEM_BASE_ADDR_ALIGN,sizeof(cl_uint),&align,nullptr);
    if (xocl(mem)->get_sub_buffer_offset() % align)
      throw error(CL_MISALIGNED_SUB_BUFFER_OFFSET,"sub buffer offset not aligned to device base addr alignment");
  }
}

void
validOrError(const cl_mem mem)
{
  if (!mem)
    throw error(CL_INVALID_MEM_OBJECT,"mem is nullptr");
}

void
validOrError(const std::vector<cl_mem>& mem_objects)
{
  std::for_each(mem_objects.begin(),mem_objects.end(),[](cl_mem mem){validOrError(mem); });
}


} // memory

}} // detail,xocl
