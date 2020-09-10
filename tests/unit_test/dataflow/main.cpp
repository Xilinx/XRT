/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <array>
#include <vector>
#include <algorithm>
#include <string>
#include <chrono>
#include <thread>

// This is a test harness for data flow scheduler
//  % g++ -std=c++14 -g -I/opt/xilinx/xrt/include src/sts.cpp -o sts -L/opt/xilinx/xrt/lib -lxilinxopencl
//  % env Runtime.sws=1 ./host.exe N_stage_Adders.hw.xilinx_u200_xdma_201830_1.xclbin
// The kernel is copied from
//   https://github.com/Xilinx/SDAccel_Examples/tree/master/getting_started/dataflow/dataflow_stream_array_c
// and modified for HLS dataflow support by adding
//  #pragma HLS INTERFACE ap_ctrl_chain port=return bundle=control

// Kernel constants
const int data_size = 4096;
const int incr = 4;
const int stages = 4;

// Options
static bool opt_verify=false;  // verify results
static int  opt_jobs = 10;     // number of jobs to run concurrently
static int  opt_seconds = 5;   // number of seconds to run

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

// Flag to stop job rescheduling.  Is set to true after
// specified number of seconds.
static bool stop = false;

// Forward declaration of event callback function for event of last
// copy stage of a job.
static void
kernel_done(cl_event event, cl_int status, void* data);

// Data for a single job
struct job_type
{
  size_t id = 0;
  size_t runs = 0;
  bool running = false;

  cl_context context = nullptr;
  cl_command_queue queue = nullptr;
  cl_program program = nullptr;

  std::vector<cl_kernel> add;

  std::vector<cl_mem> in; // input
  std::vector<cl_mem> io; // output

  const size_t bytes = sizeof(int)*data_size;

  std::array<int,data_size> input;

  job_type(cl_context c, cl_command_queue q, cl_program p)
    : context(c), queue(q), program(p)
  {
    static size_t count = 0;
    id = count++;

    // Populate input buffer
    std::iota(input.begin(),input.end(),0);
    auto data = input.data();

    cl_int err=CL_SUCCESS;
    add.push_back(clCreateKernel(program,"N_stage_Adders",&err));
    throw_if_error(err,"failed to allocate kernel");

    in.push_back(clCreateBuffer(context,CL_MEM_COPY_HOST_PTR|CL_MEM_READ_ONLY,bytes,data,&err));
    throw_if_error(err,"failed to allocate in buffer");
    io.push_back(clCreateBuffer(context,CL_MEM_COPY_HOST_PTR|CL_MEM_WRITE_ONLY,bytes,data,&err));
    throw_if_error(err,"failed to allocate io buffer");

    throw_if_error(clSetKernelArg(add[0],0,sizeof(cl_mem),&in.back()),"failed to set in");
    throw_if_error(clSetKernelArg(add[0],1,sizeof(cl_mem),&io.back()),"failed to set out");
    throw_if_error(clSetKernelArg(add[0],2,sizeof(decltype(incr)),&incr),"failed to set incr");
    throw_if_error(clSetKernelArg(add[0],3,sizeof(decltype(data_size)),&data_size),"failed to set size");

    throw_if_error(clEnqueueMigrateMemObjects(queue,in.size(),in.data(),0,0,nullptr,nullptr),"migrate failed");
    throw_if_error(clEnqueueMigrateMemObjects(queue,io.size(),io.data(),0,0,nullptr,nullptr),"migrate failed");
    clFinish(queue);
  }

  ~job_type()
  {
    std::for_each(add.begin(),add.end(),[](cl_kernel k) { clReleaseKernel(k); });
    std::for_each(in.begin(),in.end(),[](cl_mem m)      { clReleaseMemObject(m); });
    std::for_each(io.begin(),io.end(),[](cl_mem m)      { clReleaseMemObject(m); });
  }

  // Event callback for last job event
  void
  done()
  {
    if (opt_verify)
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
    cl_event events[2]={nullptr};

    throw_if_error(clEnqueueWriteBuffer(queue,in[0],false,0,bytes,input.data(),0,nullptr,&events[0]),"write failed");
    throw_if_error(clEnqueueTask(queue,add[0],1,&events[0],&events[1]),"failed to enqueue kernel");

    clSetEventCallback(events[1],CL_COMPLETE,&kernel_done,this);

    // Release all but events[1]
    std::for_each(events,events+1,[](cl_event ev){clReleaseEvent(ev);});
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
    cl_int err = clEnqueueReadBuffer(queue,io[0],CL_TRUE,0,bytes,result.data(),0,nullptr,nullptr);
    throw_if_error(err,"failed to read results");
    for (size_t idx=0; idx<data_size; ++idx) {
      int add = incr * stages;
      if (result[idx] != input[idx] + add) {
        std::cout << "got result[" << idx << "] = " << result[idx] << " expected " << input[idx]+add << "\n";
        throw std::runtime_error("VERIFY FAILED");
      }
    }

    // The result in now the new input if job is iterated
    std::copy(result.begin(),result.end(),input.begin());
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
  jobs.reserve(opt_jobs);
  for (size_t j=0; j<opt_jobs; ++j)
    jobs.emplace_back(context,queue,program);

  //  stop = true; // one iteration only
  stop = false; // one iteration only
  std::for_each(jobs.begin(),jobs.end(),[](job_type& j){j.run();});

  std::this_thread::sleep_for(std::chrono::seconds(opt_seconds));
  stop=true;

  clFinish(queue);

  for (size_t j=0; j<opt_jobs; ++j) {
    std::cout << "job[" << j << "]:" << jobs[j].runs << "\n";
  }

  return 0;
}

int
run(int argc, char** argv)
{
  if (argc < 2)
    throw std::runtime_error("usage: host.exe <xclbin>");

  if (argc==3)
    opt_verify=true;

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
