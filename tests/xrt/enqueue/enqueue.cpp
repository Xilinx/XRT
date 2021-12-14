/**
 * Copyright (C) 2021 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************************
Enqueue example illustrating use of xrt::enqueue APIs

This example is totally meaningless, it is basically just an
illustration of how a complex event graph can be scheduled to run
without any explicit waits.

The example shows how to make xrt::bo::sync an asynchronous operation
by wrapping the synchronous sync operation in a callable lambda which
is then enqueue.

The event graph consist of input buffers a[0..5], kernel run objects
r[0..6], and output buffers o[0..6].  The grap is run in a loop with
event dependencies controlling the execution order.

    a[0]  a[1]  a[2]
      \   /  \  /
 a[3] r[0]   r[1] a[4]
   \   |       | \/ |
    \  |       | /\ |
     r[2]     r[3] r[4]  a[5]
        \    /        \  /
         r[5]         r[6]
          |            |
         o[5]         o[6]
 
a[0..5]: 
xrt::bo objects that are synced to device and used as input to
xrt::run objects.  Event dependencies ensure that no sync operation
takes place before the receiving kernel is done with prior execution.

a[0] : r[0]
a[1] : r[0], r[1]
a[2] : r[1]
a[3] : r[2]
a[4] : r[3], r[4]
a[5] : r[6]

r[0..6]: 
xrt::run objects from same xrt::kernel object.  Event dependencies
ensure that run objects wait for (1) input to be synced and (2)
receiving kernel is done with prior execution.

r[0] : a[0], a[1], r[2]
r[1] : a[1], a[2], r[3]
r[2] : a[3], r[0], r[5]
r[3] : a[4], r[1], r[5]
r[4] : a[4], r[1], r[6]
r[5] : r[2], r[3], o[5]
r[6] : r[4], r[5], o[6]

o[o..6]: 
xrt::bo objects for kernel run outputs.  The outputs o[0..4] are used
as input to following run objects. o[5] and o[6] are synced from
device.

o[5] : r[5]
o[6] : r[6]

****************************************************************/


#include "xrt.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "experimental/xrt_enqueue.h"
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

// Kernel specifics
// void addone (__global ulong8 *in1, __global ulong8* in2, __global ulong8* out, unsigned int elements)
// addone(in1, in2, out, ELEMENTS)
// The kernel is compiled with 8 CUs same connectivity.
static constexpr size_t ELEMENTS = 16;
static constexpr size_t ARRAY_SIZE = 8;
static constexpr size_t MAXCUS = 8;
static size_t compute_units = MAXCUS;

static void
usage()
{
  std::cout << "usage: %s [options] \n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -d <device_index>\n";
  std::cout << "  -t <threads>\n";
  std::cout << "";
  std::cout << "  [--jobs <number>]: number of concurrently scheduled jobs\n";
  std::cout << "  [--cus <number>]: number of cus to use (default: 8) (max: 8)\n";
  std::cout << "  [--seconds <number>]: number of seconds to run\n";
  std::cout << "";
  std::cout << "* Program repeatedly enqueues an event graph for specified number of seconds\n";
  std::cout << "* Since event graph is asynchronous, the number of enqueues is dependent on host\n";
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

// Create an event queue with two event handlers
static xrt::event_queue queue;
static xrt::event_handler h1(queue);
static xrt::event_handler h2(queue);

// Data for a single job
struct job_type
{
  size_t id = 0;
  size_t runs = 0;

  // Kernel object
  // void addone (__global ulong8 *in1, __global ulong8* in2, __global ulong8* out, unsigned int elements)
  xrt::kernel k;

  // Runs for the job
  std::array<xrt::run, 7> r;

  // Input buffers for the runs
  std::array<xrt::bo, 6> a;

  // Output buffers
  std::array<xrt::bo, 7> o;

  // a events
  std::array<xrt::event, 6> ea;

  // run events
  std::array<xrt::event, 7> er;

  // output events
  std::array<xrt::event, 7> eo;

  job_type(const xrt::device& device, xrt::kernel krnl)
    : k(std::move(krnl))
  {
    static size_t count=0;
    id = count++;

    auto grpid0 = k.group_id(0);
    auto grpid1 = k.group_id(1);
    const size_t data_size = ELEMENTS * ARRAY_SIZE;

    for (int i = 0; i < 7; ++i)
      r[i] = xrt::run(k);

    for (int i = 0; i < 6; ++i)
      a[i] = xrt::bo(device, data_size * sizeof(unsigned long), grpid0);
    
    for (int i = 0; i < 7; ++i)
      o[i] = xrt::bo(device, data_size * sizeof(unsigned long), grpid1);
  }

  job_type(job_type&& rhs)
    : id(rhs.id)
    , runs(rhs.runs)
    , k(std::move(rhs.k))
    , r(std::move(rhs.r))
    , a(std::move(rhs.a))
    , o(std::move(rhs.o))
  {
    // ...
  }

  void
  enqueue()
  {
    // Create a lambda for the synchronous sync operation
    // The lambda is enqueued and executed asynchrnously
    static auto sync = [] (xrt::bo& bo, xclBOSyncDirection dir) {
                         return bo.sync(dir);
                       };

    /**
     * enqueue_with_waitlist() - Enqueue a callable with dependencies
     *
     * @c     : Callable function
     * @deps  : Event dependencies to complete before executing @c
     * @args  : Arguments to callable functions
     * Return : Event that can be waited on or chained with
     *
     * template <typename Callable, typename ...Args>
     * auto
     * enqueue_with_waitlist(Callable&& c, const std::vector<event>& deps, Args&&... args)
     */

    // sync a0 when r0 is done
    ea[0] = queue.enqueue_with_waitlist(sync, {er[0]}, a[0], XCL_BO_SYNC_BO_TO_DEVICE);

    // sync a1 when r0 and r1 are done
    ea[1] = queue.enqueue_with_waitlist(sync, {er[0], er[1]}, a[1], XCL_BO_SYNC_BO_TO_DEVICE);

    // sync a2 when r1 is done
    ea[2] = queue.enqueue_with_waitlist(sync, {er[1]}, a[2], XCL_BO_SYNC_BO_TO_DEVICE);

    // async a3 when r2 is done
    ea[3] = queue.enqueue_with_waitlist(sync, {er[2]}, a[3], XCL_BO_SYNC_BO_TO_DEVICE);

    // sync a4 when r3 and r4 are done
    ea[4] = queue.enqueue_with_waitlist(sync, {er[3], er[4]}, a[4], XCL_BO_SYNC_BO_TO_DEVICE);

    // sync a5 when r6 is done
    ea[5] = queue.enqueue_with_waitlist(sync, {er[6]}, a[5], XCL_BO_SYNC_BO_TO_DEVICE);
    
    // run r0 when a0, a1, r2  are done
    er[0] = queue.enqueue_with_waitlist(r[0], {ea[0], ea[1], er[2]}, a[0], a[1], o[0], ELEMENTS);

    // run r1 when a1, a2, r3 are done
    er[1] = queue.enqueue_with_waitlist(r[1], {ea[1], ea[2], er[3]}, a[1], a[2], o[1], ELEMENTS);

    // run r2 when a3, r0, r5 are done
    er[2] = queue.enqueue_with_waitlist(r[2], {ea[3], er[0], er[5]}, a[1], o[0], o[2], ELEMENTS);

    // run r3 when a4, r1, r5 are done
    er[3] = queue.enqueue_with_waitlist(r[3], {ea[4], er[1], er[5]}, a[4], o[1], o[3], ELEMENTS);

    // run r4 when a4, r1, r6 are done
    er[4] = queue.enqueue_with_waitlist(r[4], {ea[4], er[1], er[6]}, a[4], o[1], o[4], ELEMENTS);

    // run r5 when r2, r3, o5 are done
    er[5] = queue.enqueue_with_waitlist(r[5], {er[2], er[3], eo[5]}, o[2], o[3], o[5], ELEMENTS);

    // run r6 when a5, r4, o6 are done
    er[6] = queue.enqueue_with_waitlist(r[6], {ea[5], er[4], eo[6]}, a[5], o[4], o[6], ELEMENTS);

    // sync o5 when r5 is done
    eo[5] = queue.enqueue_with_waitlist(sync, {er[5]}, o[5], XCL_BO_SYNC_BO_FROM_DEVICE);
    
    // sync o6 when r6 is done
    eo[6] = queue.enqueue_with_waitlist(sync, {er[6]}, o[6], XCL_BO_SYNC_BO_FROM_DEVICE);
  }

  void
  run()
  {
    while (1) {

      enqueue();

      ++runs;

      if (stop)
        break;
    }

    // wait for eo5 an eo6 which terminates the graph
    eo[5].wait();
    eo[6].wait();
  }
};

// Run a job on its own thread
static size_t
run_async(const xrt::device& device, const xrt::kernel& kernel)
{
  job_type job {device, kernel};
  job.run();
  return job.runs;
}

static void
run(const xrt::device& device, const xrt::kernel& kernel, size_t num_jobs, size_t seconds)
{
  std::vector<std::future<size_t>> jobs;
  jobs.reserve(num_jobs);

  stop = (seconds == 0) ? true : false;

  for (int i=0; i<num_jobs; ++i)
    jobs.emplace_back(std::async(std::launch::async, run_async, device, kernel));

  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  stop=true;

  auto jobidx = 0;
  size_t total = 0;
  for (auto& job : jobs) {
    auto val = job.get();
    total += val;
    std::cout << "job[" << jobidx++ << "] runs: " << val << "\n";
  }

  std::cout << "enqueue: ";
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

  auto device = xrt::device(device_index);
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
