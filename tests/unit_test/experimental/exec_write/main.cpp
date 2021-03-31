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


// This example is meant as an illustration of a user space XRT++ API
// for writing to specific AXI-lite exposed addresses with KDS exec
// write command.
//
// - Program creates an exec_write command through exposed API.
// - Command is populated with {addr,value} pairs.
// - Command is submitted to the scheduler.
// - The {addr,value} are processed in the order they were added to
//   the command (e.g. FIFO) regardless of addr written to.
//
// The example can be used with the verify kernel, but in reality the
// write command is not to be used with HLS kernels as the scheduler
// (KDS and ERT) will be oblivious to the fact that a kernel is
// started, running, and completing.
//
// ERT with CU interrupts must not be configured when this example is
// running because the firmware will be confused when the CU
// interrupts since ERT has not itself started the CU.
//
// ****************************************************************
// ******** Make sure to with sdaccel.ini disabling ERT. **********
// ****************************************************************
//
// The only code of interest in this example is
// - run_kernel(), which uses the XRT++ native interface to the write command.
// - xclGetXrtDevice(), which is an OpenCL extension to access the underlying
//   XRT device required for the XRT++ native interface.
//
// The example does illustrate how the OpenCL APIs can be used to
// gather number of compute units and compute unit base addresses.

#include "CL/cl_ext_xilinx.h"
#include "experimental/xrt++.hpp"
#include "xhello_hw.h"

#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cassert>

#define LENGTH (20)

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

namespace debug {

static std::mutex s_debug_mutex;

struct lock
{
  std::lock_guard<std::mutex> m_lk;
  lock() : m_lk(s_debug_mutex)
  {}
};

static void
printf(const char* format,...)
{
  lock lk;
  va_list args;
  va_start(args,format);
  vprintf(format,args);
  va_end(args);
}

}

namespace error {

static std::mutex mutex;
static std::exception_ptr exception_ptr;

void
handle_thread_exception(const std::exception& ex)
{
  std::lock_guard<std::mutex> lk(mutex);
  std::cout << "Thread failed with : " << ex.what() << "\n";
  if (!exception_ptr)
    exception_ptr = std::current_exception();
}

void
rethrow_if_error()
{
  if (exception_ptr)
    std::rethrow_exception(exception_ptr);
}
  
}
  

// Configure number of jobs to run
static const size_t num_jobs = 10;

// Configure how long to iterate the jobs
static const size_t mseconds = 1000;

// Flag to stop job rescheduling.  Is set to true after
// specified number of seconds.
static bool stop = false;

// Print sync mutex
static std::mutex print_mutex;

// A job schedules and runs a kernel using the exec_write command
// All jobs share same CU, but has seperate ddr location for result
// All jobs run as fast as they can, scheduler handles CU scheduling 
struct job_type
{
  size_t id = 0;       // unique id for this job
  size_t runs = 0;     // how many runs this job completed
  xrt_device* m_xdev;  // handle to lower level xrt device
  uint32_t m_cuidx;    // index of cu to use
  size_t m_cuaddr;     // cu base address added to regmap offset

  cl_mem m_mem;           // memory object for kernel write
  uint64_t m_bo_dev_addr; // physical device ddr address of mem 

  cl_command_queue m_queue; // for enqueue operations

  xrtcpp::exec::exec_write_command m_cmd;  // exec_write command object

  job_type(cl_context context, cl_device_id device, cl_command_queue queue, xrt_device* xdev, uint32_t cuidx, size_t cuaddr)
    : m_xdev(xdev), m_cuidx(cuidx), m_cuaddr(cuaddr)
    , m_mem(nullptr), m_bo_dev_addr(0)
    , m_queue(queue)
    , m_cmd(xrtcpp::exec::exec_write_command(m_xdev))
  {
    static size_t count = 0;
    id = count++;

    // Create a buffer for the verify kernel and get dbuf address
    cl_int err = CL_SUCCESS;
    m_mem = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(char) * LENGTH, nullptr,&err);
    throw_if_error(err,"failed to create kernel output buffer");
    throw_if_error(xclGetMemObjDeviceAddress(m_mem,device,sizeof(uint64_t),&m_bo_dev_addr),"failed to get dbuf address");

    // No indirect migration so force it
    throw_if_error(clEnqueueMigrateMemObjects(m_queue,1,&m_mem,0,0,nullptr,nullptr),"failed to migrate");
    clFinish(m_queue);
  }

  ~job_type()
  {
    clReleaseMemObject(m_mem);
  }

  void
  run()
  {
    try {
      while (!stop) {
        m_cmd.clear();
        for (uint32_t offset = 0x10; offset < XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA; offset += 4)
          m_cmd.add(offset,0);
        m_cmd.add(XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA,m_bo_dev_addr); // low
        m_cmd.add(XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA+4,(m_bo_dev_addr >> 32) & 0xFFFFFFFF); // high part of a
        m_cmd.add_cu(m_cuidx);
        //m_cmd.add_ctx(0);
        m_cmd.execute();
        m_cmd.wait();
        assert(m_cmd.state() == ERT_CMD_STATE_COMPLETED);

        // execute same command again demo completed() API busy wait
        int count = 0;
        m_cmd.execute();
        while (!m_cmd.completed()) ++count;

        runs += 2;
      }

      // Verify result
      char hbuf[LENGTH] = {0};
      throw_if_error(clEnqueueReadBuffer(m_queue,m_mem,CL_TRUE,0,sizeof(char)*LENGTH,hbuf,0,nullptr,nullptr),"failed to read");
      debug::printf("job[%d] daddr(%p) result = %s\n",id,m_bo_dev_addr,hbuf);
    }
    catch (const std::exception& ex) {
      error::handle_thread_exception(ex);
      return;
    }
  }
};

static int
run_kernel(cl_context context, cl_device_id device, cl_command_queue queue, xrt_device* xdev, uint32_t cuidx, size_t cuaddr)
{
  // create jobs
  std::vector<job_type> jobs;
  jobs.reserve(num_jobs);
  for (size_t j=0; j<num_jobs; ++j)
    jobs.emplace_back(context,device,queue,xdev,cuidx,cuaddr);

  // each job runs on its own thread
  auto launch = [](job_type& j) {
    j.run();
  };

  stop = false;
  std::vector<std::thread> workers;
  for (auto& j : jobs)
    workers.emplace_back(std::thread(launch,std::ref(j)));

  std::this_thread::sleep_for(std::chrono::milliseconds(mseconds));
  stop=true;

  for (auto& t : workers)
    t.join();

  for (auto& j : jobs)
    std::cout << "job[" << j.id << "] runs(" << j.runs << ")\n";

  error::rethrow_if_error();

  return 0;
}

static int
run_test(cl_device_id device, cl_program program, cl_context context, cl_command_queue queue)
{
  cl_int err = 0;

  // Create kernel to get cu index to use with exec_write
  auto kernel = clCreateKernel(program, "hello", &err);
  throw_if_error(err,"failed to create hello kernel");
  cl_uint numcus = 0;
  throw_if_error(clGetKernelInfo(kernel,CL_KERNEL_COMPUTE_UNIT_COUNT,sizeof(cl_uint),&numcus,nullptr),"info numcus failed");
  throw_if_error(numcus==0,"no cus in program");

  cl_uint cuidx;  // retrieve index of first cu in kernel
  throw_if_error(xclGetComputeUnitInfo(kernel,0,XCL_COMPUTE_UNIT_INDEX,sizeof(cuidx),&cuidx,nullptr),"info index failed");

  size_t cuaddr;
  throw_if_error(xclGetComputeUnitInfo(kernel,0,XCL_COMPUTE_UNIT_BASE_ADDRESS,sizeof(cuaddr),&cuaddr,nullptr),"info addr failed");

  // Get handle to underlying xrt_device
  auto xdev = xclGetXrtDevice(device,&err);
  throw_if_error(err,"failed to get xrt_device");

  // Now run the kernel using the low level exec write command interface
  auto ret = run_kernel(context,device,queue,xdev,cuidx,cuaddr);

  ////////////////////////////////////////////////////////////////
  // Unrelated code demoing xclGetComputeUnitInfo 
  ////////////////////////////////////////////////////////////////
  cl_uint numargs = 0;
  throw_if_error(clGetKernelInfo(kernel,CL_KERNEL_NUM_ARGS,sizeof(numargs),&numargs,nullptr),"info numargs failed");
  std::cout << "kernel nm = hello\n";
  std::cout << "kernel number of arguments = " << numargs << "\n";

  for (int cuid=0; cuid<numcus; ++cuid) {
    char cunm[512] = {0};
    throw_if_error
      (xclGetComputeUnitInfo(kernel,cuid,XCL_COMPUTE_UNIT_NAME,sizeof(cunm),cunm,nullptr),"info name failed");
    cl_uint cuidx;
    throw_if_error
      (xclGetComputeUnitInfo(kernel,cuid,XCL_COMPUTE_UNIT_INDEX,sizeof(cuidx),&cuidx,nullptr),"info index failed");
    size_t  cuaddr;
    throw_if_error
      (xclGetComputeUnitInfo(kernel,cuid,XCL_COMPUTE_UNIT_BASE_ADDRESS,sizeof(cuaddr),&cuaddr,nullptr),"info addr failed");
    std::vector<cl_ulong> cumem(numargs);
    throw_if_error
      (xclGetComputeUnitInfo(kernel,cuid,XCL_COMPUTE_UNIT_CONNECTIONS,sizeof(cl_ulong)*numargs,cumem.data(),nullptr),"info conn failed");
    std::cout << " cu[" << cuid << "].name = " << cunm << "\n";
    std::cout << " cu[" << cuid << "].idx  = "  << cuidx << "\n";
    std::cout << " cu[" << cuid << "].addr = 0x" << std::hex << cuaddr << std::dec << "\n";
    for (auto memidx : cumem) 
      std::cout << " cu[" << cuid << "].mem  = 0x" << std::hex << memidx << std::dec << "\n";
  }

  clReleaseKernel(kernel);

  return ret;
}

int
run(int argc, char** argv)
{
  if (argc < 2)
    throw std::runtime_error("usage: host.exe <path to verify.xclbin>");

  // Init OCL
  cl_int err = CL_SUCCESS;
  cl_platform_id platform = nullptr;
  throw_if_error(clGetPlatformIDs(1,&platform,nullptr));

  cl_uint num_devices = 0;
  throw_if_error(clGetDeviceIDs(platform,CL_DEVICE_TYPE_ACCELERATOR,0,nullptr,&num_devices));
  throw_if_error(num_devices==0,"no devices");
  std::vector<cl_device_id> devices(num_devices);
  throw_if_error(clGetDeviceIDs(platform,CL_DEVICE_TYPE_ACCELERATOR,num_devices,devices.data(),nullptr));
  auto device = devices.front();

  auto context = clCreateContext(0,1,&device,nullptr,nullptr,&err);
  throw_if_error(err,"failed to create context");

  auto queue = clCreateCommandQueue(context,device,CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,&err);
  throw_if_error(err,"failed to create command queue");

  // Read xclbin and create program
  std::string fnm = argv[1];
  std::ifstream stream(fnm);
  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);
  std::vector<char> xclbin(size);
  stream.read(xclbin.data(),size);
  auto data = reinterpret_cast<const unsigned char*>(xclbin.data());
  cl_int status = CL_SUCCESS;
  auto program = clCreateProgramWithBinary(context,1,&device,&size,&data,&status,&err);
  throw_if_error(err,"failed to create program");

  run_test(device,program,context,queue);

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
