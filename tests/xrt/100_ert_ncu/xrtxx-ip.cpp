/**
 * Copyright (C) 2021 Xilinx, Inc
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

////////////////////////////////////////////////////////////////
// This test uses xrt::ip for manual control of kernel compute
// units.  It does set up the CUs for execution first via regular
// xrt::kernel APIs, just to ensure valid register map before using
// manual xrt::ip control.
//
// Number of jobs created is number of cus specified.  Each job
// executes in its own thread.
////////////////////////////////////////////////////////////////

#include "xrt.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_device.h"
#include "experimental/xrt_ip.h"
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

constexpr uint32_t AP_START    = 0x1;
constexpr uint32_t AP_DONE     = 0x2;
constexpr uint32_t AP_IDLE     = 0x4;

const size_t ELEMENTS = 16;
const size_t ARRAY_SIZE = 8;
const size_t MAXCUS = 8;

static size_t compute_units = MAXCUS;
static bool use_interrupt = false;

static void usage()
{
  std::cout << "usage: %s [options] \n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -d <bdf | device_index>\n";
  std::cout << "";
  std::cout << "  [--cus <number>]: number of cus to use (default: 8) (max: 8)\n";
  std::cout << "  [--seconds <number>]: number of seconds to run\n";
  std::cout << "  [--intr]: use IP interrupt notification\n";
  std::cout << "";
  std::cout << "* Program schedules a job per CU specified. Each jobs is repeated\n";
  std::cout << "* unless specified seconds have elapsed\n";
  std::cout << "* Summary prints \"jsz sec jobs\" for use with awk, where jobs is total number \n";
  std::cout << "* of jobs executed in the specified run time\n";
}

static std::string
get_cu_name(size_t idx)
{
  std::string nm("addone:{");
  nm.append("addone_").append(std::to_string(idx+1)).append("}");
  return nm;
}

// Flag to stop job rescheduling.  Is set to true after
// specified number of seconds.
static std::atomic<bool> stop{true};

// Data for a single job
struct job_type
{
  size_t id = 0;
  size_t runs = 0;
  size_t reads = 0;
  bool running = false;

  // Custom IP controlled by this job
  xrt::ip ip;

  // Kernel arguments and run handle are managed by this job
  xrt::bo a;
  void* am               = nullptr;
  xrt::bo b;
  void* bm               = nullptr;

  job_type(const xrt::device& device, const xrt::uuid& xid, const std::string& cu)
  {
    static size_t count=0;
    id = count++;

    { 
      // Create kernel to run it once for preceeding of register map
      // Scoped to ensure automatic object are destructed before xrt::ip is created
      xrt::kernel kernel{device, xid, cu};

      auto grpid0 = kernel.group_id(0);
      auto grpid1 = kernel.group_id(1);

      const size_t data_size = ELEMENTS * ARRAY_SIZE;
      a = xrt::bo(device, data_size*sizeof(unsigned long), grpid0);
      am = a.map();
      auto adata = reinterpret_cast<unsigned long*>(am);
      for (unsigned int i=0;i<data_size;++i)
        adata[i] = i;

      b = xrt::bo(device, data_size*sizeof(unsigned long), grpid1);
      bm = b.map();
      auto bdata = reinterpret_cast<unsigned long*>(bm);
      for (unsigned int j=0;j<data_size;++j)
        bdata[j] = id;

      // Run the kernel once
      auto run = kernel(a, b, ELEMENTS);
      run.wait();
    }

    // Create the custom IP
    ip = xrt::ip{device, xid, cu};
  }

  job_type(job_type&& rhs)
    : id(rhs.id)
    , runs(rhs.runs)
    , reads(rhs.reads)
    , running(rhs.running)
    , ip(std::move(rhs.ip))
    , a(std::move(rhs.a))
    , am(rhs.am)
    , b(std::move(rhs.b))
    , bm(rhs.bm)
  {
    am=bm=nullptr;
  }

  ~job_type()
  {
    std::cout << "wait: " << reads << "\n";
  }

  void
  run_poll()
  {
    while (1) {
      ip.write_register(0, AP_START);
      ++runs;

      uint32_t val = 0;
      do {
        val = ip.read_register(0);
        ++reads;
      }
      while (!(val & (AP_IDLE | AP_DONE)));

      if (stop)
        break;
    }
  }

  void
  run_intr()
  {
    auto interrupt = ip.create_interrupt_notify();

    while (1) {
      ip.write_register(0, AP_START);
      ++runs;
      interrupt.wait();

      if (stop)
        break;
    }
  }
    

  void
  run()
  {
    if (use_interrupt)
      run_intr();
    else
      run_poll();
  }
};

static size_t
run_async(const xrt::device& device, const xrt::uuid& xid, const std::string& ipnm)
{
  job_type job {device, xid, ipnm};
  job.run();
  return job.runs;
}

static int
run(const xrt::device& device, const xrt::uuid& xid, size_t cus, size_t seconds)
{
  std::vector<std::future<size_t>> jobs;
  jobs.reserve(cus);

  stop = (seconds == 0) ? true : false;

  for (int i=0; i<cus; ++i)
    jobs.emplace_back(std::async(std::launch::async, run_async, device, xid, get_cu_name(i)));

  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  stop=true;

  size_t total = 0;
  for (auto& job : jobs) {
    auto val = job.get();
    total += val;
    std::cout << "job count: " << val << "\n";
  }

  std::cout << "xrtxx-ip: ";
  std::cout << "jobsize cus seconds total = "
            << compute_units << " "
            << compute_units << " "
            << seconds << " "
            << total << "\n";

  return 0;
}

int run(int argc, char** argv)
{
  std::vector<std::string> args(argv+1,argv+argc);

  std::string xclbin_fnm;
  std::string device_id = "0";
  size_t secs = 0;
  size_t jobs = 1;
  size_t cus  = 1;

  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
    }

    if (arg == "--intr") {
      use_interrupt = true;
      continue;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-d")
      device_id = arg;
    else if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "--seconds")
      secs = std::stoi(arg);
    else if (cur == "--cus")
      cus = std::stoi(arg);
    else
      throw std::runtime_error("bad argument '" + cur + " " + arg + "'");
  }

  compute_units = cus = std::min(cus, compute_units);

  xrt::xclbin xclbin{xclbin_fnm};
  xrt::device device{device_id};
  auto uuid = device.load_xclbin(xclbin);

  run(device, uuid, cus, secs);

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
