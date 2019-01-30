/**
 * Copyright (C) 2018 Xilinx, Inc
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

#include <CL/cl_ext_xilinx.h>
#include <CL/cl.h>
#include <fstream>
#include <stdexcept>
#include <numeric>
#include <iostream>
#include <array>
#include <vector>
#include <algorithm>
#include <string>
#include <chrono>
#include <thread>

const size_t NUM_WORKGROUPS = 1;
const size_t WORKGROUP_SIZE = 16;
const size_t LENGTH = NUM_WORKGROUPS * WORKGROUP_SIZE;
const size_t data_size = LENGTH;


static void
throw_if_error(cl_int errcode, const char* msg=nullptr)
{
  if (!errcode)
    return;
  std::string err = "errcode '";
  err.append(std::to_string(errcode)).append("'");
  if (msg)
    err.append(" ").append(msg);
  throw std::runtime_error(err);
}

static void
throw_if_error(cl_int errcode, const std::string& msg)
{
  throw_if_error(errcode,msg.c_str());
}

// vadd has 8 CUs each with 4 arguments connected as follows
//  vadd_1 (0,1,2,3)
//  vadd_2 (0,1,2,3)
//  vadd_3 (1,2,3,0)
//  vadd_4 (1,2,3,0)
//  vadd_5 (2,3,0,1)
//  vadd_6 (2,3,0,1)
//  vadd_7 (3,0,1,2)
//  vadd_8 (3,0,1,2)
// Purpose of this test is to execute 4 kernel jobs with auto select
// of matching CUs based on the connectivity of the buffer arguments.

using data_type = int;
const size_t buffer_size = data_size*sizeof(data_type);

// Flag to stop job rescheduling.  Is set to true after
// specified number of seconds.
static bool stop = true;

// Forward declaration of event callback function for event of last
// copy stage of a job.
static void
kernel_done(cl_event event, cl_int status, void* data);

struct job_type
{
  size_t id = 0;
  size_t runs = 0;
  bool running = false;

  cl_context context = nullptr;
  cl_command_queue queue = nullptr;
  cl_program program = nullptr;
  cl_kernel kernel = nullptr;

  std::array<cl_mem,4> args;

  std::array<int,buffer_size/sizeof(data_type)> dataA;
  std::array<int,buffer_size/sizeof(data_type)> dataB;
  std::array<int,buffer_size/sizeof(data_type)> dataC;
  std::array<int,buffer_size/sizeof(data_type)> dataO;

  std::array<void*,3> input_data;
  std::array<void*,1> output_data;

  job_type(cl_context c, cl_command_queue q, cl_program p, const std::array<int,4>& banks)
    : context(c), queue(q), program(p)
  {
    static size_t count = 0;
    id = count++;

    std::iota(dataA.begin(),dataA.end(),0);
    std::iota(dataB.begin(),dataB.end(),1);
    std::iota(dataC.begin(),dataC.end(),2);
    input_data[0] = dataA.data();
    input_data[1] = dataB.data();
    input_data[2] = dataC.data();

    std::fill(dataO.begin(),dataO.end(),0);
    output_data[0] = dataO.data();

    cl_int err=CL_SUCCESS;

    kernel = clCreateKernel(program,"vadd",&err);
    throw_if_error(err,"failed to allocate kernel object");

    // Set kernel inputs
    for (size_t arg=0; arg<3; ++arg) {
      unsigned int bank = (XCL_MEM_DDR_BANK0 << banks[arg]);
      cl_mem_ext_ptr_t ext = {bank,input_data[arg],nullptr};
      args[arg] = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_EXT_PTR_XILINX|CL_MEM_COPY_HOST_PTR,buffer_size,&ext,&err);
      throw_if_error(err,"failed to allocate input buffer");
      throw_if_error(clSetKernelArg(kernel,arg,sizeof(cl_mem),&args[arg]),"failed to set kernel input arg");
    }

    // Set kernel output
    unsigned int bank = (XCL_MEM_DDR_BANK0 << banks[3]);
    cl_mem_ext_ptr_t ext = {bank,output_data[0],nullptr};
    args[3] = clCreateBuffer(context,CL_MEM_READ_WRITE|CL_MEM_EXT_PTR_XILINX|CL_MEM_COPY_HOST_PTR,buffer_size,&ext,&err);
    throw_if_error(clSetKernelArg(kernel,3,sizeof(cl_mem),&args[3]),"failed to set kernel output arg");

      // Migrate all memory objects to device
    clEnqueueMigrateMemObjects(queue,args.size(),args.data(),0,0,nullptr,nullptr);
  }

  ~job_type()
  {
    clReleaseKernel(kernel);
    std::for_each(args.begin(),args.end(),[](cl_mem m)      { clReleaseMemObject(m); });
  }

  // Event callback for last job event
  void
  done()
  {
    verify_results();
    running = false;

    // Reschedule job unless stop is asserted.
    // This ties up the XRT thread that notifies host that event is done
    // Probably not too bad given that enqueue (run()) time should be very fast.
    if (!stop)
      run();
  }

  void
  run()
  {
    running = true;
    ++runs;

    cl_int err = CL_SUCCESS;
    cl_event kevent = nullptr;

    size_t global[1] = {NUM_WORKGROUPS * WORKGROUP_SIZE};
    size_t local[1] = {WORKGROUP_SIZE};
    err = clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, global, local, 0, nullptr, &kevent);
    throw_if_error(err,"failed to execute job " + std::to_string(id));
    clSetEventCallback(kevent,CL_COMPLETE,&kernel_done,this);
  }

  // Verify data of last stage addone output.  Note that last output
  // has been copied back to in[0] up job completion
  void
  verify_results()
  {
    // The addone kernel adds 1 to the first element in input a, since
    // the job has 4 stages, the resulting first element of a[0] will
    // be incremented by 4.
    std::array<int,data_size> result;
    auto bytes = data_size*sizeof(data_type);
    cl_int err = clEnqueueReadBuffer(queue,args[3],CL_TRUE,0,bytes,result.data(),0,nullptr,nullptr);
    throw_if_error(err,"failed to read results");
    for (size_t idx=0; idx<data_size; ++idx) {
      unsigned long add = dataA[idx] + dataB[idx] + dataC[idx];
      if (result[idx] != add) {
        std::cout << "got result[" << idx << "] = " << result[idx] << " expected " << add << "\n";
        throw std::runtime_error("VERIFY FAILED");
      }
    }
  }
};

static void
kernel_done(cl_event event, cl_int status, void* data)
{
  reinterpret_cast<job_type*>(data)->done();
  clReleaseEvent(event);
}

int
run_test(cl_context context, cl_command_queue queue, cl_program program)
{
  // create the jobs
  std::vector<job_type> jobs;
  jobs.reserve(5);

  // success jobs
  jobs.emplace_back(context,queue,program,std::array<int,4>{0,1,2,3});
  jobs.emplace_back(context,queue,program,std::array<int,4>{1,2,3,0});
  jobs.emplace_back(context,queue,program,std::array<int,4>{2,3,0,1});
  jobs.emplace_back(context,queue,program,std::array<int,4>{3,0,1,2});

  // failed jobs
  try {
    jobs.emplace_back(context,queue,program,std::array<int,4>{3,0,1,0});
    throw 10;
  }
  catch (const std::exception& ex) {
    std::cout << "job creation failed as expected: " << ex.what() << "\n";
  }
  catch (int) {
    throw std::runtime_error("job creation succeeded unexpectedly");
  }

  std::for_each(jobs.begin(),jobs.end(),[](job_type& j){j.run();});
  clFinish(queue);

  return 0;
}

int
run(int argc, char** argv)
{
  if (argc < 2)
    throw std::runtime_error("usage: host.exe <xclbin>");

  // Init OCL
  cl_int err = CL_SUCCESS;
  cl_platform_id platform = nullptr;
  throw_if_error(clGetPlatformIDs(1,&platform,nullptr));

  cl_uint num_devices = 0;
  throw_if_error(clGetDeviceIDs(platform,CL_DEVICE_TYPE_ACCELERATOR,0,nullptr,&num_devices));
  throw_if_error(num_devices==0,"no devices");
  std::vector<cl_device_id> devices(num_devices);
  throw_if_error(clGetDeviceIDs(platform,CL_DEVICE_TYPE_ACCELERATOR,num_devices,devices.data(),nullptr));
  cl_device_id device = devices.front();

  cl_context context = clCreateContext(0,1,&device,nullptr,nullptr,&err);
  throw_if_error(err);

  cl_command_queue queue = clCreateCommandQueue(context,device,CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,&err);
  throw_if_error(err,"failed to create command queue");

  // Read xclbin and create program
  std::string fnm = argv[1];
  std::ifstream stream(fnm);
  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);
  std::vector<char> xclbin(size);
  stream.read(xclbin.data(),size);
  const unsigned char* data = reinterpret_cast<unsigned char*>(xclbin.data());
  cl_int status = CL_SUCCESS;
  cl_program program = clCreateProgramWithBinary(context,1,&device,&size,&data,&status,&err);
  throw_if_error(err,"failed to create program");

  run_test(context,queue,program);

  clReleaseProgram(program);
  clReleaseCommandQueue(queue);
  clReleaseContext(context);
  clReleaseDevice(device);
  std::for_each(devices.begin(),devices.end(),[](cl_device_id d){clReleaseDevice(d);});

  return 0;
}

int
main(int argc, char* argv[])
{
  try {
    run(argc,argv);
    std::cout << "TEST SUCCESS\n";
    return 0;
  }
  catch (const std::exception& ex) {
    std::cout << "TEST FAILED: " << ex.what() << "\n";
  }
  catch (...) {
    std::cout << "TEST FAILED\n";
  }

  return 1;
}
