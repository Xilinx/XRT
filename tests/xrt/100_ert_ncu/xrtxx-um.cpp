/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2020-2021 Xilinx, Inc. All rights reserved.
 */

// This test uses unmanaged execution of kernel, where each run object
// is reused after it completes.  The test is internal and uses a non
// exported private function to safely call execwait.
//
// The test illustrates how the host code can continuosly start any
// number of runs, wait on execwait, and then iterate the run obects
// one by one to check for runs that have completed.  The objective is
// for the iteration to not have to call execwait again, basically
// reusing any run objects that have completed by the time it is being
// checked.
//
// The internal exec_wait call will be made public at a later time.
//
// % g++ -g -std=c++17 -I${XILINX_XRT}/include -L${XILINX_XRT}/lib -o xrtxx-um.exe xrtxx-um.cpp -lxrt_coreutil -pthread -luuid

#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>
#include <tuple>

#include "ert.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"

// decl internal non public function
namespace xrt_core { namespace device_int {
std::cv_status
exec_wait(const xrt::device& device, const std::chrono::milliseconds& timeout_ms);
}}

#ifdef _WIN32
# pragma warning ( disable : 4267 )
#endif

using namespace std::chrono_literals;

static constexpr size_t ELEMENTS = 16;
static constexpr size_t ARRAY_SIZE = 8;
static constexpr size_t MAXCUS = 8;

static size_t compute_units = MAXCUS;

static void
usage()
{
  std::cout << "usage: %s [options] \n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -d <bdf | device_index>\n";
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

// Data for a single job
struct job_type
{
  size_t id = 0;
  size_t runs = 0;

  // Device and kernel are not managed by this job
  xrt::kernel k;

  // Kernel arguments and run handle are managed by this job
  xrt::bo a;
  void* am               = nullptr;
  xrt::bo b;
  void* bm               = nullptr;
  xrt::run r;

  job_type(const xrt::device& device, const xrt::kernel& kernel)
    : k(kernel)
  {
    static size_t count=0;
    id = count++;

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
  }

  job_type(job_type&& rhs)
    : id(rhs.id)
    , runs(rhs.runs)
    , k(std::move(rhs.k))
    , a(std::move(rhs.a))
    , am(rhs.am)
    , b(std::move(rhs.b))
    , bm(rhs.bm)
    , r(std::move(rhs.r))
  {
    am=bm=nullptr;
  }

  void
  run()
  {
    ++runs;
    if (!r) {
      r = xrt::run(k);
      r(a, b, ELEMENTS);
    }
    else if (!stop)
      r.start();
  }

  ert_cmd_state
  wait()
  {
    return r.wait();
  }

  bool
  is_done() const
  {
    auto state = r.state();
    return (state >= ERT_CMD_STATE_COMPLETED);
  }
};



static std::tuple<size_t, size_t, size_t, size_t>
run_async(const xrt::device& device, std::vector<job_type>& jobs)
{
  size_t completed = 0;
  size_t loops = 0;
  size_t skipped = 0;
  size_t timeout = 0;

  // repeat
  while (!stop) {

    if (xrt_core::device_int::exec_wait(device, 1ms) == std::cv_status::timeout)
      ++timeout;

    ++loops;

    for (auto& j : jobs) {

      if (!j.is_done()) {
        ++skipped;
        continue;
      }

      ++completed;
      j.run();
    }
  }

  // drain
  for (auto& j : jobs) {
    j.wait();
    ++completed;
  }

  return std::make_tuple(completed, loops, skipped, timeout);
}

static void
run(const xrt::device& device, const xrt::kernel& kernel, size_t num_jobs, size_t seconds)
{
  std::vector<job_type> jobs;
  jobs.reserve(num_jobs);
  for (int i=0; i<num_jobs; ++i)
    jobs.emplace_back(device, kernel);

  stop = (seconds == 0) ? true : false;

  // start all jobs
  for (auto& j : jobs)
    j.run();

  // babysit runs in single thread
  auto value = std::async(std::launch::async, run_async, device, std::ref(jobs));

  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  stop=true;

  auto cw = value.get();
  std::cout << "total completed = " << std::get<0>(cw) << "\n";
  std::cout << "total loops = " << std::get<1>(cw)<< "\n";
  std::cout << "total skipped = " << std::get<2>(cw)<< "\n";
  std::cout << "total timeout = " << std::get<3>(cw)<< "\n";

  size_t total = 0;
  for (auto& job : jobs) {
    auto val = job.runs;
    total += val;
    std::cout << "job count: " << val << "\n";
  }

  std::cout << "xrtxx-um: ";
  std::cout << "jobsize cus seconds total = "
            << num_jobs << " "
            << compute_units << " "
            << seconds << " "
            << total << "\n";

}


static int
run(int argc, char** argv)
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

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-d")
      device_id = arg;
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

  auto device = xrt::device(device_id);
  auto uuid = device.load_xclbin(xclbin_fnm);

  compute_units = cus = std::min(cus, compute_units);
  std::string kname = get_kernel_name(cus);
  auto kernel = xrt::kernel(device, uuid.get(), kname);

  run(device,kernel,jobs,secs);

  return 0;
}

int
main(int argc, char* argv[])
{
  try {
    return run(argc,argv);
  }
  catch (const std::exception& ex) {
    std::cout << "TEST FAILED: " << ex.what() << "\n";
  }
  catch (...) {
    std::cout << "TEST FAILED\n";
  }

  return 1;
}
