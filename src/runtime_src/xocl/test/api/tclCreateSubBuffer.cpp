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

#include <boost/test/unit_test.hpp>
#include "setup.h"

#include "xocl/core/device.h"
#include "xocl/core/memory.h"
#include <vector>

BOOST_AUTO_TEST_SUITE ( test_clCreateSubBuffer )

BOOST_AUTO_TEST_CASE( test_clCreateSubBuffer1 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  size_t psz=1024*1024*1024; // 1G

  // Allocate parent buffer
  auto pbuf = clCreateBuffer(ocl.context,CL_MEM_READ_WRITE,psz,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Split parent into 4 chunks
  size_t ssz = psz/4;
  std::vector<cl_mem> sbuf_vec;
  sbuf_vec.reserve(psz/ssz);
  size_t sz=0;
  for (; sz<psz; sz+=ssz) {
    cl_buffer_region region {sz, ssz};
    auto sbuf = clCreateSubBuffer(pbuf,CL_MEM_READ_WRITE,CL_BUFFER_CREATE_TYPE_REGION,&region,&err);
    BOOST_CHECK_EQUAL(err,CL_SUCCESS);
    sbuf_vec.push_back(sbuf);
  }
  BOOST_CHECK_EQUAL(sz,psz);

  // Release parent buffer, it should be accounted for in sub buffers
  clReleaseMemObject(pbuf);

  // Release sub buffers, which will delete them one by one
  // When final sub buffer is deleted the parent buffer is too.
  for (auto sbuf : sbuf_vec)
    clReleaseMemObject(sbuf);
}

BOOST_AUTO_TEST_CASE( test_clCreateSubBuffer2 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  size_t psz=1024*1024*1024; // 1G

  // Allocate parent buffer
  auto pbuf = clCreateBuffer(ocl.context,CL_MEM_READ_WRITE,psz,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Split parent into 4 chunks
  size_t ssz = psz/4;
  std::vector<cl_mem> sbuf_vec;
  sbuf_vec.reserve(psz/ssz);
  size_t sz=0;
  for (; sz<psz; sz+=ssz) {
    cl_buffer_region region {sz, ssz};
    auto sbuf = clCreateSubBuffer(pbuf,CL_MEM_READ_WRITE,CL_BUFFER_CREATE_TYPE_REGION,&region,&err);
    BOOST_CHECK_EQUAL(err,CL_SUCCESS);
    sbuf_vec.push_back(sbuf);
  }
  BOOST_CHECK_EQUAL(sz,psz);

  // Create buffer objecs for sub buffers.  The first sub buffer
  // will force a bo on parent buffer, which is the offset into
  // each sub buffer
  auto device = xocl::xocl(ocl.device);;
  for (auto sbuf : sbuf_vec)
    xocl::xocl(sbuf)->get_buffer_object(device);

  // Release parent buffer, it should be accounted for in sub buffers
  clReleaseMemObject(pbuf);

  // Release sub buffers, which will delete them one by one
  // When final sub buffer is deleted the parent buffer is too.
  for (auto sbuf : sbuf_vec)
    clReleaseMemObject(sbuf);
}

BOOST_AUTO_TEST_CASE( test_clCreateSubBuffer3 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  size_t psz=1024*1024*1024; // 1G

  // Allocate parent buffer
  auto pbuf = clCreateBuffer(ocl.context,CL_MEM_READ_WRITE,psz,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Split parent into 4 chunks
  size_t ssz = psz/4;
  std::vector<cl_mem> sbuf_vec;
  sbuf_vec.reserve(psz/ssz);
  size_t sz=0;
  for (int i=0; i<2; ++i,sz+=ssz) {
    cl_buffer_region region {sz, ssz};
    auto sbuf = clCreateSubBuffer(pbuf,CL_MEM_READ_WRITE,CL_BUFFER_CREATE_TYPE_REGION,&region,&err);
    BOOST_CHECK_EQUAL(err,CL_SUCCESS);
    sbuf_vec.push_back(sbuf);
  }
  BOOST_CHECK_EQUAL(sz,psz/2);

  auto cq = clCreateCommandQueue(ocl.context,ocl.device,0,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Migrate parent buffer to device.  This implies all sub buffers are resident
  cl_event migrate_event = nullptr;
  clEnqueueMigrateMemObjects(cq,1,&pbuf,0,0,nullptr,&migrate_event);
  clWaitForEvents(1,&migrate_event);
  clReleaseEvent(migrate_event);

  // Create buffer objecs for sub buffers.  The first sub buffer
  // will force a bo on parent buffer, which is the offset into
  // each sub buffer
  auto device = xocl::xocl(ocl.device);
  for (auto sbuf : sbuf_vec)
    BOOST_CHECK_EQUAL(xocl::xocl(sbuf)->is_resident(device),true);

  // Create two more sub buffers, they too should be resident
  for (int i=0; i<2; ++i,sz+=ssz) {
    cl_buffer_region region {sz, ssz};
    auto sbuf = clCreateSubBuffer(pbuf,CL_MEM_READ_WRITE,CL_BUFFER_CREATE_TYPE_REGION,&region,&err);
    BOOST_CHECK_EQUAL(err,CL_SUCCESS);
    sbuf_vec.push_back(sbuf);
  }
  BOOST_CHECK_EQUAL(sz,psz);

  for (auto sbuf : sbuf_vec)
    BOOST_CHECK_EQUAL(xocl::xocl(sbuf)->is_resident(device),true);


  // Get second sub buffer, verify it is at expected offset
  auto mem = sbuf_vec[1];  
  BOOST_CHECK_EQUAL(xocl::xocl(mem)->get_sub_buffer_offset(),ssz);

  // Map it for write, and fill the sub buffer
  auto wptr = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_WRITE,0,ssz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK(wptr!=nullptr);
  auto cwptr = static_cast<char*>(wptr);
  std::fill(cwptr,cwptr+ssz,'5');

  // Unmap sub buffer to sync to device
  cl_event unmap_event = nullptr;
  clEnqueueUnmapMemObject(cq,mem,wptr,0,nullptr,&unmap_event);
  clWaitForEvents(1,&unmap_event);
  clReleaseEvent(unmap_event);

  // Map back parent buffer at second sub buffers offset (ssz)
  // to compare that it reflects written data
  auto rptr = clEnqueueMapBuffer(cq,pbuf,CL_TRUE,CL_MAP_READ,ssz,sz-ssz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK_EQUAL(rptr,wptr);
  
  // Release parent buffer, it should be accounted for in sub buffers
  clReleaseMemObject(pbuf);

  // Release sub buffers, which will delete them one by one
  // When final sub buffer is deleted the parent buffer is too.
  for (auto sbuf : sbuf_vec)
    clReleaseMemObject(sbuf);
}

BOOST_AUTO_TEST_CASE( test_clCreateSubBuffer4 )
{
  ocl_sw_emulation ocl;
  cl_int err = CL_SUCCESS;

  size_t psz=1024*2;

  // Allocate parent buffer
  auto pbuf = clCreateBuffer(ocl.context,CL_MEM_READ_WRITE,psz,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Split parent into 2 chunks
  // Since alignment requirement is 4096 only first buffer is aligned
  size_t ssz = psz/2;
  std::vector<cl_mem> sbuf_vec;
  sbuf_vec.reserve(psz/ssz);
  size_t sz=0;
  for (int i=0; i<2; ++i,sz+=ssz) {
    cl_buffer_region region {sz, ssz};
    auto sbuf = clCreateSubBuffer(pbuf,CL_MEM_READ_WRITE,CL_BUFFER_CREATE_TYPE_REGION,&region,&err);
    BOOST_CHECK_EQUAL(err,CL_SUCCESS);
    sbuf_vec.push_back(sbuf);
  }
  BOOST_CHECK_EQUAL(sz,psz);

  auto cq = clCreateCommandQueue(ocl.context,ocl.device,0,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);

  // Migrate parent buffer to device.  This implies all sub buffers are resident
  cl_event migrate_event = nullptr;
  clEnqueueMigrateMemObjects(cq,1,&pbuf,0,0,nullptr,&migrate_event);
  clWaitForEvents(1,&migrate_event);
  clReleaseEvent(migrate_event);

  // Create buffer objecs for sub buffers.  The first sub buffer
  // will force a bo on parent buffer, which is the offset into
  // each sub buffer
  auto device = xocl::xocl(ocl.device);
  for (auto sbuf : sbuf_vec)
    BOOST_CHECK_EQUAL(xocl::xocl(sbuf)->is_resident(device),true);

  // Get second sub buffer, verify it is at expected offset
  auto mem = sbuf_vec[1];  
  BOOST_CHECK_EQUAL(xocl::xocl(mem)->get_sub_buffer_offset(),ssz);

  // Map it for write, and fill the sub buffer
  auto wptr = clEnqueueMapBuffer(cq,mem,CL_TRUE,CL_MAP_WRITE,0,ssz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK(wptr!=nullptr);
  auto cwptr = static_cast<char*>(wptr);
  std::fill(cwptr,cwptr+ssz,'5');

  // Unmap sub buffer to sync to device
  cl_event unmap_event = nullptr;
  clEnqueueUnmapMemObject(cq,mem,wptr,0,nullptr,&unmap_event);
  clWaitForEvents(1,&unmap_event);
  clReleaseEvent(unmap_event);

  // Map back parent buffer at second sub buffers offset (ssz)
  // to compare that it reflects written data
  auto rptr = clEnqueueMapBuffer(cq,pbuf,CL_TRUE,CL_MAP_READ,ssz,sz-ssz,0,0,nullptr,&err);
  BOOST_CHECK_EQUAL(err,CL_SUCCESS);
  BOOST_CHECK_EQUAL(rptr,wptr);
  
  // Release parent buffer, it should be accounted for in sub buffers
  clReleaseMemObject(pbuf);

  // Release sub buffers, which will delete them one by one
  // When final sub buffer is deleted the parent buffer is too.
  for (auto sbuf : sbuf_vec)
    clReleaseMemObject(sbuf);
}

BOOST_AUTO_TEST_SUITE_END()


