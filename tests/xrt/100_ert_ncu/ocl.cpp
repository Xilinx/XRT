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
#include <CL/cl_ext_xilinx.h>
#include <CL/cl.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <stdexcept>
#include <numeric>
#include <string>
#include <fstream>
#include <vector>
#include <limits>
#include <iostream>
#include <cstdarg>

const size_t ELEMENTS = 16;
const size_t ARRAY_SIZE = 8;
const size_t MAXCUS = 8;

size_t cus = MAXCUS;
size_t rsize = std::numeric_limits<size_t>::max();

#define MAYBE_UNUSED __attribute__((unused))

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

static std::string
get_kernel_name(int cus)
{
  std::string k("addone:{");
  for (int i=1; i<cus; ++i)
    k.append("addone_").append(std::to_string(i)).append(",");
  k.append("addone_").append(std::to_string(cus)).append("}");
  return k;
}

static void usage()
{
  std::cout << "usage: %s [options] \n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -d <device_index>\n";
  std::cout << "";
  std::cout << "  [--jobs <number>]: number of concurrently scheduled jobs\n";
  std::cout << "  [--cus <number>]: number of cus to use (default: 8) (max: 8)\n";
  std::cout << "  [--seconds <number>]: number of seconds to run\n";
  std::cout << "";
  std::cout << "* Program schedules specified number of jobs as commands to scheduler.\n";
  std::cout << "* Scheduler starts commands based on CU availability and state.\n";
  std::cout << "* Summary prints \"jsz sec jobs\" for use with awk, where jobs is total number \n";
  std::cout << "* of jobs executed in the specified run time\n";
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
  size_t id = 0;
  size_t runs = 0;

  cl_context context = nullptr;
  cl_command_queue queue = nullptr;
  cl_kernel kernel = nullptr;

  cl_mem a;
  cl_mem b;

  job_type(cl_context c, cl_command_queue q, cl_kernel k)
    : context(c), queue(q), kernel(k)
  {
    static size_t count=0;
    id = count++;

    // create buffers
    cl_int err = CL_SUCCESS;;
    constexpr size_t data_size = ELEMENTS * ARRAY_SIZE;
    unsigned long ubuf[data_size];
    std::iota(std::begin(ubuf), std::end(ubuf), 0);

    a = clCreateBuffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,data_size*sizeof(unsigned long),ubuf,&err);
    throw_if_error(err,"failed to allocate a buffer");

    b = clCreateBuffer(context,CL_MEM_WRITE_ONLY,data_size*sizeof(unsigned long),nullptr,&err);
    throw_if_error(err,"failed to allocate b buffer");

    // set kernel arguments
    throw_if_error(clSetKernelArg(kernel,0,sizeof(cl_mem),&a), "failed to set kernel arg a");
    throw_if_error(clSetKernelArg(kernel,1,sizeof(cl_mem),&b), "failed to set kernel arg b");
    uint elements = ELEMENTS;
    throw_if_error(clSetKernelArg(kernel,2,sizeof(uint),&elements), "failed to set kernel arg b");

    // Migrate all memory objects to device
    cl_mem args[2] = {a, b};
    throw_if_error(clEnqueueMigrateMemObjects(queue,2,args,0,0,nullptr,nullptr),"failed to migrate");
  }

  job_type(job_type&& rhs)
    : id(rhs.id), runs(rhs.runs),
      context(rhs.context), queue(rhs.queue), kernel(rhs.kernel)
  {}

  ~job_type()
  {
    clReleaseMemObject(a);
    clReleaseMemObject(b);
  }

  void
  run()
  {
    ++runs;

    cl_int err = CL_SUCCESS;
    cl_event kevent = nullptr;

    static size_t global[3] = {1,0,0};
    static size_t local[3] = {1,0,0};
    
    err = clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, global, local, 0, nullptr, &kevent);
    if (err) throw_if_error(err,"failed to execute job " + std::to_string(id));
    clSetEventCallback(kevent,CL_COMPLETE,&kernel_done,this);
  }

  void
  done()
  {
    // Reschedule
    if (!stop)
      run();
  }
};

static void
kernel_done(cl_event event, cl_int status, void* data)
{
  reinterpret_cast<job_type*>(data)->done();
  clReleaseEvent(event);
}

static int
run(cl_context context, cl_command_queue queue, cl_kernel kernel, size_t num_jobs, size_t seconds)
{
  std::vector<job_type> jobs;
  jobs.reserve(num_jobs);
  for (int i=0; i<num_jobs; ++i)
    jobs.emplace_back(context,queue,kernel);

  stop = (seconds==0) ? true : false;
  std::for_each(jobs.begin(),jobs.end(),[](job_type& j){j.run();});

  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  stop=true;

  clFinish(queue);

  size_t total = 0;
  for (auto& job : jobs) {
    total += job.runs;
  }

  std::cout << "ocl: ";
  std::cout << "jobsize cus seconds total = "
            << num_jobs << " "
            << cus << " "
            << seconds << " "
            << total << "\n";

}

static int
run(const std::string& fnm, size_t jobs, size_t seconds)
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
  auto kname = get_kernel_name(cus);
  auto kernel = clCreateKernel(program,kname.c_str(),&err);
  throw_if_error(err,"failed to allocate kernel object");

  run(context,queue,kernel,jobs,seconds);

  clReleaseKernel(kernel);
  clReleaseProgram(program);
  clReleaseCommandQueue(queue);
  clReleaseContext(context);
  clReleaseDevice(device);
  std::for_each(devices.begin(),devices.end(),[](cl_device_id d){clReleaseDevice(d);});
  return 0;
}

int run(int argc, char** argv)
{
  std::vector<std::string> args(argv+1,argv+argc);

  std::string xclbin_fnm;
  size_t device_index = 0;
  size_t secs = 0;
  size_t jobs = 1;
  size_t cus  = 1;
  
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-d")
      device_index = std::stoi(arg);
    else if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "--jobs")
      jobs = std::stoi(arg);
    else if (cur == "--seconds")
      secs = std::stoi(arg);
    else if (cur == "--cus")
      cus = std::stoi(arg);
    else
      throw std::runtime_error("bad argument '" + cur + " " + arg + "'");
  }

  run(xclbin_fnm,jobs,secs);

  return 0;
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
