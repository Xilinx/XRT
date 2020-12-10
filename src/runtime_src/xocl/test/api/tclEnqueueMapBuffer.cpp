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
//  % em -env opt txocl --run_test=test_clEnqueueMapBuffer
// To run selective tests in this suite use
//  % em -env opt txocl --run_test=test_clEnqueueMapBuffer/<test case name>

// test_clEnqueueMapBuffer1
//   Test data consistency with map and unmap of resident memory object and
//   unaligned ubuf.  This creates [hbuf,dbuf] where hbuf is separate
//   from ubuf.
// test_clEnqueueMapBuffer2
//   Test data consistency with map and unmap of resident memory object and
//   aligned ubuf.  This creates [hbuf,dbuf] where hbuf is the same as ubuf.
// test_clEnqueueMapBuffer3
//   Test data consistency with map and unmap of resident memory object and
//   no ubuf.  This creates [hbuf,dbuf], where hbuf is directly used by user.


BOOST_AUTO_TEST_SUITE ( test_clEnqueueMapBuffer )

// Test data consistency with map and unmap of resident memory object and
// likely unaligned user buffer.
// To run just this test use:
//   em -env opt --run-test=test_clEnqueueMapBuffer/test_clEnqueueMapBuffer1
BOOST_AUTO_TEST_CASE( test_clEnqueueMapBuffer1 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  // Allocate unaligned (for xrt_xocl::device) buffer to force allocation
  // of separate host buffer in backing buffer object
  const size_t sz = 5;
  std::unique_ptr<char[]> storage(new char[sz]); 
  auto ubuf = storage.get();
  std::strncpy(ubuf,"hello",sz);

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

  // Since ubuf is unaligned, the underlying [hbuf,dbuf] has allocated
  // its own host backing buffer (hbuf).  Verify that map buffer still
  // returns user's ptr (ubuf) not the backing ptr (hbuf).
  auto wptr = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_WRITE,0,sz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK_EQUAL(wptr,ubuf);

  // Since the mem object is mapped for writing, writing to the mapped 
  // ptr (wptr) and unmapping should ensure that hbuf is updated when
  // the mem object is unmapped.  Further, since the mem object is
  // resident, unmapping should also update the device buffer (dbuf).
  std::strncpy(ubuf,"01234",sz);  // remember ubuf is same as wptr
  cl_event unmap_event = nullptr;
  clEnqueueUnmapMemObject(cq,mem,wptr,0,nullptr,&unmap_event);
  clWaitForEvents(1,&unmap_event);
  clReleaseEvent(unmap_event);

  // Verify that dbuf was updated by now mapping the mem object for
  // read, which will sync dbuf to hbuf and use memcpy to mem object's
  // buffer (ubuf).
  auto rptr = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_READ,0,sz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK_EQUAL(rptr,ubuf);
  BOOST_CHECK_EQUAL(std::strncmp(ubuf,"01234",sz),0);

  clReleaseMemObject(mem);
}


// Test data consistency with map and unmap of resident memory object and
// aligned user buffer.
BOOST_AUTO_TEST_CASE( test_clEnqueueMapBuffer2 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  // Allocate aligned (for xrt_xocl::device) buffer to force allocation
  // of separate host buffer in backing buffer object
  const size_t sz = 5;
  auto deleter = [](void* v) { free(v); };
  std::unique_ptr<void,decltype(deleter)> storage(nullptr,deleter);
  void* vbuf = nullptr;
  BOOST_CHECK_EQUAL(posix_memalign(&vbuf,128,sz),0);
  storage.reset(vbuf);
  auto ubuf = static_cast<char*>(vbuf);
  std::strncpy(ubuf,"hello",5);

  auto cq = clCreateCommandQueue(ocl.context,ocl.device,0,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Create a mem object and request that host ptr (ubuf) be used.
  auto mem = clCreateBuffer(ocl.context,CL_MEM_READ_WRITE|CL_MEM_USE_HOST_PTR,sz,ubuf,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Migrate the mem object to the device. The mem object becomes
  // resident on the device.  Under the hood a buffer object
  // [hbuf,dbuf] is created, where hbuf is the same as ubuf provided
  // alignment needs are met.  Finally hbuf is DMA'ed to dbuf.
  cl_event migrate_event = nullptr;
  clEnqueueMigrateMemObjects(cq,1,&mem,0,0,nullptr,&migrate_event);
  clWaitForEvents(1,&migrate_event);
  clReleaseEvent(migrate_event);

  // Since ubuf is aligned, the underlying [hbuf,dbuf] uses ubuf as hbuf.
  // This is invisible to user, there is no way to check. Verify that map 
  // buffer returns ubuf.
  auto wptr = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_WRITE,0,sz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK_EQUAL(wptr,ubuf);

  // Since the mem object is mapped for writing, writing to the mapped 
  // ptr (wptr) and unmapping should ensure that hbuf is updated when
  // the mem object is unmapped.  Further, since the mem object is
  // resident, unmapping should also update the device buffer (dbuf).
  std::strncpy(ubuf,"01234",5);  // remember ubuf is same as wptr
  cl_event unmap_event = nullptr;
  clEnqueueUnmapMemObject(cq,mem,wptr,0,nullptr,&unmap_event);
  clWaitForEvents(1,&unmap_event);
  clReleaseEvent(unmap_event);

  // Verify that dbuf was updated by now mapping the mem object for
  // read, which will sync dbuf to hbuf but since hbuf and ubuf are
  // the same memcpy is skipped, which again is invisble to user.
  auto rptr = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_READ,0,sz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK_EQUAL(rptr,ubuf);
  BOOST_CHECK_EQUAL(std::strncmp(ubuf,"01234",5),0);

  clReleaseMemObject(mem);
}

// Test data consistency with map and unmap of resident memory object
// with no user buffer
BOOST_AUTO_TEST_CASE( test_clEnqueueMapBuffer3 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  size_t sz = 5;

  auto cq = clCreateCommandQueue(ocl.context,ocl.device,0,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Create a mem object and request that host ptr (ubuf) be used.
  auto mem = clCreateBuffer(ocl.context,CL_MEM_READ_WRITE,sz,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Migrate the mem object to the device. The mem object becomes
  // resident on the device.  Under the hood a buffer object
  // [hbuf,dbuf] is created, where hbuf is the same as ubuf provided
  // alignment needs are met.  Finally hbuf is DMA'ed to dbuf.
  cl_event migrate_event = nullptr;
  clEnqueueMigrateMemObjects(cq,1,&mem,0,0,nullptr,&migrate_event);
  clWaitForEvents(1,&migrate_event);
  clReleaseEvent(migrate_event);

  // Since ubuf is aligned, the underlying [hbuf,dbuf] uses ubuf as hbuf.
  // This is invisible to user, there is no way to check. Verify that map 
  // buffer returns ubuf.
  auto wptr = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_WRITE,0,sz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK(wptr!=nullptr);

  // Since the mem object is mapped for writing, writing to the mapped 
  // ptr (wptr) and unmapping should ensure that hbuf is updated when
  // the mem object is unmapped.  Further, since the mem object is
  // resident, unmapping should also update the device buffer (dbuf).
  std::strncpy(static_cast<char*>(wptr),"01234",5);  // remember ubuf is same as wptr
  cl_event unmap_event = nullptr;
  clEnqueueUnmapMemObject(cq,mem,wptr,0,nullptr,&unmap_event);
  clWaitForEvents(1,&unmap_event);
  clReleaseEvent(unmap_event);

  // Verify that dbuf was updated by now mapping the mem object for
  // read, which will sync dbuf to hbuf but since hbuf and ubuf are
  // the same memcpy is skipped, which again is invisble to user.
  auto rptr = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_READ,0,sz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK_EQUAL(rptr,wptr);
  BOOST_CHECK_EQUAL(std::strncmp(static_cast<char*>(rptr),"01234",5),0);

  clReleaseMemObject(mem);
}

BOOST_AUTO_TEST_SUITE_END()


