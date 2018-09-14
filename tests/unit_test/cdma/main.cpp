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

const size_t ELEMENTS = 16;
const size_t ARRAY_SIZE = 8;
const size_t data_size = ELEMENTS * ARRAY_SIZE;

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

// Job execution is defined as:
//   [in0]->add0->[io0]
//   [io1]->copy->[in1]
//   [in1]->add1->[io1]
//   [io1]->copy->[in2]
//   [in2]->add2->[io2]
//   [io2]->copy->[in3]
//   [in3]->add3->[io3]
//   [io3]->copy->[in0]
//
// Kernels are scheduled with dependencies, such that job execution
// is the following sequence of command executed by scheduler.
//   [add0][copy][add1][copy][add2][copy][add3][copy]
// A job is rescheduled immediately when it is done.
//
// If multiple jobs are specified, then all jobs are scheduled
// immediately.  Once a job completes it is immediately rescheduled.
// Since each commmand in a job is tied to a specific compute unit,
// multiple jobs fight for the same CUs.

// Size of buffers transferred to device and copied by CDMA/copy.
// Increase to make copying work with more bytes.  The minimum
// size is data_size in bytes.
const size_t buffer_size = data_size*sizeof(unsigned long); //*1024;
static_assert(buffer_size>=data_size*sizeof(unsigned long),"minimum buffer size");

// Configure number of jobs to run
static const size_t num_jobs = 10;

// Configure how long to iterate the jobs
static const size_t seconds = 5;

// Flag to stop job rescheduling.  Is set to true after
// specified number of seconds.
static bool stop = false;

// Forward declaration of event callback function for event of last
// copy stage of a job.
static void
copy_done(cl_event event, cl_int status, void* data);

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

  std::vector<cl_mem> in; // input       : i0, i1, i2, i3
  std::vector<cl_mem> io; // input/output: o0, o1, o2, o3

  std::array<unsigned long,buffer_size/sizeof(unsigned long)> input;

  job_type(cl_context c, cl_command_queue q, cl_program p)
    : context(c), queue(q), program(p)
  {
    static size_t count = 0;
    id = count++;

    for (size_t bank=0; bank<4; ++bank) {
      cl_int err=CL_SUCCESS;

      // Populate input buffer
      std::iota(input.begin(),input.end(),0);

      std::string kname("add" + std::to_string(bank));
      add.push_back(clCreateKernel(program,kname.c_str(),&err));
      throw_if_error(err,"failed to allocate buffer");

      // Create buffers, agnostic bank assignment deferred to clSetKernelArg
      auto data = input.data();
      in.push_back(clCreateBuffer(context,CL_MEM_COPY_HOST_PTR,buffer_size,data,&err));
      throw_if_error(err,"failed to allocate in buffer");
      io.push_back(clCreateBuffer(context,CL_MEM_COPY_HOST_PTR,buffer_size,data,&err));
      throw_if_error(err,"failed to allocate io buffer");

      // set kernel args
      // in[i] -> add[i] <-> io[i]
      clSetKernelArg(add.back(),0,sizeof(cl_mem),&in.back());
      clSetKernelArg(add.back(),1,sizeof(cl_mem),&io.back());
      clSetKernelArg(add.back(),2,sizeof(int),&ELEMENTS);
    }

    // Migrate all memory objects to device
    clEnqueueMigrateMemObjects(queue,in.size(),in.data(),0,0,nullptr,nullptr);
    clEnqueueMigrateMemObjects(queue,io.size(),io.data(),0,0,nullptr,nullptr);

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
    cl_event events[8]={nullptr};

    err = clEnqueueTask(queue,add[0],0,nullptr,&events[0]);
    throw_if_error(err,"failed to enqueue add[0]");
    err = clEnqueueCopyBuffer(queue,io[0],in[1],0,0,buffer_size,1,&events[0],&events[1]);
    throw_if_error(err,"failed to copy io[0]->in[1]");

    err = clEnqueueTask(queue,add[1],1,&events[1],&events[2]);
    throw_if_error(err,"failed to enqueue add[1]");
    err = clEnqueueCopyBuffer(queue,io[1],in[2],0,0,buffer_size,1,&events[2],&events[3]);
    throw_if_error(err,"failed to copy io[1]->in[2]");

    err = clEnqueueTask(queue,add[2],1,&events[3],&events[4]);
    throw_if_error(err,"failed to enqueue add[2]");
    err = clEnqueueCopyBuffer(queue,io[2],in[3],0,0,buffer_size,1,&events[4],&events[5]);
    throw_if_error(err,"failed to copy io[2]->in[3]");

    err = clEnqueueTask(queue,add[3],1,&events[5],&events[6]);
    throw_if_error(err,"failed to enqueue add[3]");
    err = clEnqueueCopyBuffer(queue,io[3],in[0],0,0,buffer_size,1,&events[6],&events[7]);
    throw_if_error(err,"failed to copy io[3]->in[0]");
    clSetEventCallback(events[7],CL_COMPLETE,&copy_done,this);

    // Release all but events[7]
    std::for_each(events,events+7,[](cl_event ev){clReleaseEvent(ev);});
  }

  // Verify data of last stage addone output.  Note that last output
  // has been copied back to in[0] up job completion
  void
  verify_results()
  {
    // The addone kernel adds 1 to the first element in input a, since
    // the job has 4 stages, the resulting first element of a[0] will
    // be incremented by 4.
    std::array<unsigned long,data_size> result;
    auto bytes = data_size*sizeof(unsigned long);
    cl_int err = clEnqueueReadBuffer(queue,in[0],CL_TRUE,0,bytes,result.data(),0,nullptr,nullptr);
    throw_if_error(err,"failed to read results");
    for (size_t idx=0; idx<data_size; ++idx) {
      unsigned long add = idx%ARRAY_SIZE ? 0 : 4;
      if (result[idx] != input[idx] + add) {
        std::cout << "got result[" << idx << "] = " << result[idx] << " expected " << input[idx]+4 << "\n";
        throw std::runtime_error("VERIFY FAILED");
      }
    }

    // The result in now the new input if job is iterated
    std::copy(result.begin(),result.end(),input.begin());
  }

};

static void
copy_done(cl_event event, cl_int status, void* data)
{
  reinterpret_cast<job_type*>(data)->done();
  clReleaseEvent(event);
}

int
run_test(cl_context context, cl_command_queue queue, cl_program program)
{
  // create the jobs
  std::vector<job_type> jobs;
  jobs.reserve(num_jobs);
  for (size_t j=0; j<num_jobs; ++j)
    jobs.emplace_back(context,queue,program);

  //  stop = true; // one iteration only
  stop = false; // one iteration only
  std::for_each(jobs.begin(),jobs.end(),[](job_type& j){j.run();});

  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  stop=true;

  clFinish(queue);

  for (size_t j=0; j<num_jobs; ++j) {
    std::cout << "job[" << j << "]:" << jobs[j].runs << "\n";
  }

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
