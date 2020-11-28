/**
 * Copyright (C) 2020 Xilinx, Inc
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
#include <CL/cl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -h\n\n";
    std::cout << "* Bitstream is required\n";
}

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


// Flag to stop job rescheduling.  Is set to true after
// specified number of seconds.
static bool stop = true;

// Forward declaration of event callback function for event of last
// copy stage of a job.
static void
kernel_done(cl_event event, cl_int status, void* data);

// Data for a single job
struct job_type
{
  static constexpr uint32_t aes_key[16] = {
    0xeb5aa3b8,
    0x17750c26,
    0x9d0db966,
    0xbcb9e3b6,
    0x510e08c6,
    0x83956e46,
    0x3bd10f72,
    0x769bf32e,
    0xfa374467,
    0x3386553a,
    0x46f91c6a,
    0x6b25d1b4,
    0x6116fa6f,
    0xd29b1a56,
    0x9c193635,
    0x10ed77d4
  };

  static constexpr uint32_t aes_iv[4] = {
    0x149f40ae,
    0x38f1817d,
    0x32ccb7db,
    0xa6ef0e05
  };

  static constexpr size_t len = 4096;

  size_t id = 0;
  size_t runs = 0;

  cl_context context = nullptr;
  cl_command_queue queue = nullptr;
  cl_kernel kernel = nullptr;

  cl_mem in;
  cl_mem out;
  cl_mem out_status;

  std::atomic<bool> busy{false};

  job_type(cl_context c, cl_command_queue q, cl_kernel k)
    : context(c), queue(q), kernel(k)
  {
    static size_t count=0;
    id = count++;

    // create buffers
    cl_int err = CL_SUCCESS;;
    uint32_t in_data[len/sizeof(uint32_t)];
    std::iota(std::begin(in_data), std::end(in_data), 0);

    in = clCreateBuffer(context,CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR,len,in_data,&err);
    throw_if_error(err,"failed to allocate 'in' buffer");

    out = clCreateBuffer(context,CL_MEM_WRITE_ONLY,len,nullptr,&err);
    throw_if_error(err,"failed to allocate 'out' buffer");

    out_status = clCreateBuffer(context,CL_MEM_WRITE_ONLY,len,nullptr,&err);
    throw_if_error(err,"failed to allocate 'out_status' buffer");

    // set kernel arguments
    throw_if_error(clSetKernelArg(kernel,0,sizeof(cl_mem),&in), "failed to set kernel arg(0) 'in'");
    throw_if_error(clSetKernelArg(kernel,1,sizeof(int),&len), "failed to set kernel arg(1) 'len'");
    throw_if_error(clSetKernelArg(kernel,2,sizeof(cl_mem),&out), "failed to set kernel arg(2) 'out'");
    throw_if_error(clSetKernelArg(kernel,3,sizeof(int),&len), "failed to set kernel arg(3) 'len'");
    throw_if_error(clSetKernelArg(kernel,4,sizeof(cl_mem),&out_status), "failed to set kernel arg(4) 'out_status'");
    throw_if_error(clSetKernelArg(kernel,5,sizeof(aes_key),&aes_key), "failed to set kernel arg(5) 'aes_key'");
    throw_if_error(clSetKernelArg(kernel,6,sizeof(aes_iv),&aes_iv), "failed to set kernel arg(6) 'aes_iv'");

    // Migrate 'in memory objects to device
    throw_if_error(clEnqueueMigrateMemObjects(queue,1,&in,0,0,nullptr,nullptr),"failed to migrate");
    throw_if_error(clFinish(queue),"failed clFinish");
  }

  job_type(job_type&& rhs)
    : id(rhs.id), runs(rhs.runs),
      in(rhs.in), out(rhs.out), out_status(rhs.out_status),
      context(rhs.context), queue(rhs.queue), kernel(rhs.kernel),
      busy(false)
  {}

  ~job_type()
  {
    clReleaseMemObject(in);
    clReleaseMemObject(out);
    clReleaseMemObject(out_status);
  }

  void
  start()
  {
    if (busy)
      throw std::runtime_error("job is already running");

    ++runs;
    busy = true;

    cl_int err = CL_SUCCESS;
    cl_event kevent = nullptr;

    static size_t global[3] = {1,0,0};
    static size_t local[3] = {1,0,0};

    err = clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, global, local, 0, nullptr, &kevent);
    if (err) throw_if_error(err,"failed to execute job " + std::to_string(id));
    clSetEventCallback(kevent,CL_COMPLETE,&kernel_done,this);
  }

  void
  mark_done()
  {
    busy = false;
  }

  bool
  is_done() const
  {
    return !busy;
  }
};

constexpr uint32_t job_type::aes_key[16];
constexpr uint32_t job_type::aes_iv[4];
constexpr size_t job_type::len;

static void
kernel_done(cl_event event, cl_int status, void* data)
{
  reinterpret_cast<job_type*>(data)->mark_done();
  clReleaseEvent(event);
}

static double
run(std::vector<job_type>& cmds, size_t total)
{
  size_t i = 0;
  size_t issued = 0, completed = 0;
  auto start = std::chrono::high_resolution_clock::now();

  for (auto& cmd : cmds) {
    cmd.start();
    if (++issued == total)
      break;
  }

  while (completed < total) {
    auto& cmd = cmds[i];

    if (cmd.is_done()) {
      ++completed;
      // cmd.verify()
      if (issued < total) {
        cmd.start();
        ++issued;
      }
    }

    if (++i == cmds.size())
      i = 0;
  }

  auto end = std::chrono::high_resolution_clock::now();
  return static_cast<double>((std::chrono::duration_cast<std::chrono::microseconds>(end - start)).count());

}

static void
run(cl_context context, cl_command_queue queue, cl_kernel kernel)
{
  std::vector<size_t> cmds_per_run = { 16, 100, 1000, 10000, 100000, 1000000 };
  size_t expected_cmds = 10000;

  std::vector<job_type> jobs;
  jobs.reserve(expected_cmds);
  for (int i = 0; i < expected_cmds; ++i)
    jobs.emplace_back(context, queue, kernel);

  for (auto num_cmds : cmds_per_run) {
    auto duration = run(jobs, num_cmds);

    std::cout << "Commands: " << std::setw(7) << num_cmds
              << " iops: " << (num_cmds * 1000.0 * 1000.0 / duration)
              << std::endl;
  }
}

static void
run(const std::string& fnm)
{
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
  std::ifstream stream(fnm);
  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);
  std::vector<char> xclbin(size);
  stream.read(xclbin.data(),size);
  const unsigned char* data = reinterpret_cast<unsigned char*>(xclbin.data());
  cl_int status = CL_SUCCESS;
  auto program = clCreateProgramWithBinary(context,1,&device,&size,&data,&status,&err);
  throw_if_error(err,"failed to create program");
  auto kernel = clCreateKernel(program,"fa_aes_xts2_rtl_enc",&err);
  throw_if_error(err,"failed to allocate kernel object");

  run(context,queue,kernel);

  clReleaseKernel(kernel);
  clReleaseProgram(program);
  clReleaseCommandQueue(queue);
  clReleaseContext(context);
  clReleaseDevice(device);
  std::for_each(devices.begin(),devices.end(),[](cl_device_id d){clReleaseDevice(d);});
}

void
run(int argc, char** argv)
{
  std::vector<std::string> args(argv+1,argv+argc);

  std::string xclbin_fnm;

  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-k")
      xclbin_fnm = arg;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  run(xclbin_fnm);
}

int
main(int argc, char* argv[])
{
  try {
    run(argc,argv);
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
