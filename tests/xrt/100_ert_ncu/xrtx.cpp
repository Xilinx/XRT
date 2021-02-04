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

// driver includes
#include "xrt.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_xclbin.h"
#include "experimental/xrt_device.h"
#include "xclbin.h"

#include <fstream>
#include <list>
#include <thread>
#include <atomic>
#include <iostream>
#include <vector>
#include <cstdlib>

#ifdef _WIN32
# pragma warning ( disable : 4267 )
#endif

const size_t ELEMENTS = 16;
const size_t ARRAY_SIZE = 8;
const size_t MAXCUS = 8;

size_t compute_units = MAXCUS;

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

static std::string
get_kernel_name(size_t cus)
{
  std::string k("addone:{");
  for (int i=1; i<cus; ++i)
    k.append("addone_").append(std::to_string(i)).append(",");
  k.append("addone_").append(std::to_string(cus)).append("}");
  return k;
}

// Flag to stop job rescheduling.  Is set to true after
// specified number of seconds.
static std::atomic<bool> stop{true};

// Forward declaration of event callback function for event of last
// copy stage of a job.
static void
kernel_done(xrtRunHandle, ert_cmd_state, void*);

// Data for a single job
struct job_type
{
  size_t id = 0;
  size_t runs = 0;
  bool running = false;

  // Device and kernel are not managed by this job
  xrtDeviceHandle d      = XRT_NULL_HANDLE;
  xrtKernelHandle k      = XRT_NULL_HANDLE;

  // Kernel arguments and run handle are managed by this job
  xrtBufferHandle a      = XRT_NULL_HANDLE;
  void* am               = nullptr;
  xrtBufferHandle b      = XRT_NULL_HANDLE;
  void* bm               = nullptr;
  xrtRunHandle r         = XRT_NULL_HANDLE;

  job_type(xrtDeviceHandle device, xrtKernelHandle kernel)
    : d(device), k(kernel)
  {
    static size_t count=0;
    id = count++;

    const size_t data_size = ELEMENTS * ARRAY_SIZE;
    a = xrtBOAlloc(d, data_size*sizeof(unsigned long), 0, xrtKernelArgGroupId(kernel, 0));
    am = xrtBOMap(a);
    auto adata = reinterpret_cast<unsigned long*>(am);
    for (unsigned int i=0;i<data_size;++i)
      adata[i] = i;

    b = xrtBOAlloc(d, data_size*sizeof(unsigned long), 0, xrtKernelArgGroupId(kernel, 0));
    bm = xrtBOMap(b);
    auto bdata = reinterpret_cast<unsigned long*>(bm);
     for (unsigned int j=0;j<data_size;++j)
       bdata[j] = id;
  }

  job_type(job_type&& rhs)
    : id(rhs.id)
    , runs(rhs.runs)
    , running(rhs.running)
    , d(rhs.d)
    , k(rhs.k)
    , a(rhs.a)
    , am(rhs.am)
    , b(rhs.b)
    , bm(rhs.bm)
    , r(rhs.r)
  {
    a=b=XRT_NULL_HANDLE;
    am=bm=nullptr;
    d=k=r=XRT_NULL_HANDLE;
  }

  ~job_type()
  {
    if (am) {
      xrtBOFree(a);
    }

    if (bm) {
      xrtBOFree(b);
    }

    if (r != XRT_NULL_HANDLE)
      xrtRunClose(r);
  }

  void
  run()
  {
    ++runs;
    if (r == XRT_NULL_HANDLE) {
      running = true;
      r = xrtKernelRun(k, a, b, ELEMENTS);
      xrtRunSetCallback(r, ERT_CMD_STATE_COMPLETED, kernel_done, this);
    }
    else if (!stop)
      xrtRunStart(r);
  }

  bool
  done()
  {
    if (!stop) {
      run();
      return false;
    }

    running = false;
    return true;
  }

  void
  wait()
  {
    // Must wait for callback to complete
    while (running)
      xrtRunWait(r);
  }
};

static void
kernel_done(xrtRunHandle rhdl, ert_cmd_state state, void* data)
{
  reinterpret_cast<job_type*>(data)->done();
}

static int
run(xrtDeviceHandle device, xrtKernelHandle kernel, size_t num_jobs, size_t seconds)
{
  std::vector<job_type> jobs;
  jobs.reserve(num_jobs);
  for (int i=0; i<num_jobs; ++i)
    jobs.emplace_back(device, kernel);

  stop = (seconds == 0) ? true : false;
  std::for_each(jobs.begin(),jobs.end(),[](job_type& j){j.run();});

  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  stop=true;

  // Drain jobs
  for (auto& job : jobs)
    job.wait();

  size_t total = 0;
  for (auto& job : jobs) {
    total += job.runs;
  }

  std::cout << "xrtx: ";
  std::cout << "jobsize cus seconds total = "
            << num_jobs << " "
            << compute_units << " "
            << seconds << " "
            << total << "\n";

  return 0;
}

int run(int argc, char** argv)
{
  std::vector<std::string> args(argv+1,argv+argc);

  std::string xclbin_fnm;
  unsigned int device_index = 0;
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

  auto device = xrtDeviceOpen(device_index);

  if (xrtDeviceLoadXclbinFile(device, xclbin_fnm.c_str()))
    throw std::runtime_error("failed to load xclbin");

  xuid_t uuid;
  xrtDeviceGetXclbinUUID(device, uuid);

  compute_units = cus = std::min<size_t>(cus, compute_units);
  std::string kname = get_kernel_name(cus);
  auto kernel = xrtPLKernelOpen(device, uuid, kname.c_str());

  run(device,kernel,jobs,secs);

  xrtKernelClose(kernel);
  xrtDeviceClose(device);

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
