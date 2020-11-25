/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#include <boost/test/unit_test.hpp>
#include "setup.h"

#include "xocl/core/device.h"
#include "xocl/core/memory.h"
#include "xocl/core/time.h"
#include <vector>
#include <memory>

// Terminology
//   - ubuf is user's buffer in host code
//   - [hbuf,dbuf] is buffer object with host side and device side memory

// To run all tests in this suite use
//  % em -env opt txocl --run_test=test_clEnqueueWriteBuffer
// To run selective tests in this suite use
//  % em -env opt txocl --run_test=test_clEnqueueWriteBuffer/<test case name>

// test_clEnqueueWriteBuffer1
//   Test data consistency with write to resident memory object at offset.
//   Under the hood HAL should perform read/modify write.  

BOOST_AUTO_TEST_SUITE ( test_clEnqueueWriteBuffer )

// Test data consistency with write to resident memory object at offset.
// Under the hood HAL should perform read/modify write.  
// To run just this test use:
//   em -env opt --run-test=test_clEnqueueWriteBuffer/test_clEnqueueWriteBuffer1
BOOST_AUTO_TEST_CASE( test_clEnqueueWriteBuffer1 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  // Allocate unaligned (for xrt_xocl::device) buffer to force allocation
  // of separate host buffer in backing buffer object
  const size_t sz = 10;
  std::unique_ptr<char[]> storage(new char[sz]); 
  auto ubuf = storage.get();
  std::strncpy(ubuf,"helloworld",sz);

  auto cq = clCreateCommandQueue(ocl.context,ocl.device,0,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Create a mem object and request that host ptr (ubuf) be used.
  auto mem = clCreateBuffer(ocl.context,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,sz,ubuf,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Migrate the mem object to the device. The mem object becomes
  // resident on the device.  Under the hood a buffer object
  // [hbuf,dbuf] is created and ubuf is memcpy to hbuf before hbuf is
  // DMAed to the dbuf.
  cl_event migrate_event = nullptr;
  clEnqueueMigrateMemObjects(cq,1,&mem,0,0,nullptr,&migrate_event);
  clWaitForEvents(1,&migrate_event);
  clReleaseEvent(migrate_event);

  // Write to mem at offset 2, changing ll to LL.  Since mem is resident
  // this should update the device side buffer.
  clEnqueueWriteBuffer(cq,mem,CL_TRUE,2,2,"LL",0,nullptr,nullptr);

  // Verify that dbuf was updated by now mapping the mem object for
  // read, which will sync dbuf to hbuf and use memcpy to mem object's
  // buffer (ubuf).
  auto rptr = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_READ,0,sz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK_EQUAL(ubuf,rptr);
  BOOST_CHECK_EQUAL(std::strncmp(ubuf,"heLLoworld",sz),0);

  cl_event unmap_event = nullptr;
  clEnqueueUnmapMemObject(cq,mem,rptr,0,nullptr,&unmap_event);
  clWaitForEvents(1,&unmap_event);
  clReleaseEvent(unmap_event);

  clReleaseMemObject(mem);
}

BOOST_AUTO_TEST_SUITE_END()


