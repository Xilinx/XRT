/**
 * Copyright (C) 2022 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */

/****************************************************************
Enqueue example illustrating use of xrt::queue APIs

This example is totally meaningless, it is basically just an
illustration of how a complex event graph can be scheduled to run
without any explicit waits.

The example shows how to make xrt::bo::sync an asynchronous operation
by wrapping the synchronous sync operation in a callable lambda which
is then enqueued.

The event graph consist of input buffers a[0..5], kernel run objects
r[0..6], and output buffers o[0..6].  The graph is run in a loop with
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
r[1] : a[1], a[2], r[3], r[4]
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

This example uses xrt::queue which is an in-order synchronous queue,
meaning enqueued operations are executed asynchronously from enqueuing
thread but synchronously and in order within the queue.

In order to implement concurrent execution of multiple jobs, several
queues are needed.  Each queue executes independently of one another,
but the result of enqueuing one job in a queue can be used to block
execution in one or more other queues.  This blocking entitity is
referred to as an event.  The event becomes ready when the associated
job completes, and this in turn allows the blocked queue to continue.

This example starts out with 5=6 queues corresponding to 5 concurrent
sync of input buffers a[0..5]:

q0: a[0],r[0],r[2],r[5],o[5]
q1: a[1],r[1],r[3]
q2: a[2]
q3: a[3]
q4: a[4],r[4],r[6],o[6]
q5: a[5]

Inserting dependencies (z[z]) into the queue based on event graph gives:

q0: (r[0]) a[0] (a[0]) (a[1]) (r[2]) r[0] (a[3]) (r[0]) (r[5]) r[2] (r[2]) (r[3]) (o[5]) r[5] (r[5]) o[5]
q1: (r[0]) (r[1]) a[1] (a[1]) (a[2]) (r[3]) (r[4]) r[1] (a[4]) (r[1]) (r[5]) r[3]
q2: (r[1]) a[2]
q3: (r[2]) a[3]
q4: (r[3]) (r[4]) a[4] (a[4]) (r[1]) (r[6]) r[4] (r[4]) (a[5]) (o[6]) r[6] (r[6]) o[6]
q5: (r[6]) a[5]

Since queue execution is in-order and synchronous all event
dependencies on events in same queue can be removed.  This gives:

q0:        a[0] (a[1]) r[0] (a[3]) r[2] (r[3]) r[5] o[5]
q1: (r[0]) a[1] (a[2]) (r[4]) r[1] (a[4]) (r[5]) r[3]
q2: (r[1]) a[2]
q3: (r[2]) a[3]
q4: (r[3]) a[4] (r[1]) r[4] (a[5]) r[6] o[6]
q5: (r[6]) a[5]

% g++ -g -std=c++17 -I$XILINX_XRT/include -L$XILINX_XRT/lib -o enqueue2.exe enqueue2.cpp -lxrt_coreutil -luuid -pthread

****************************************************************/

#include "xclbin.h"
#include "xrt.h"

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

#include "experimental/xrt_queue.h"

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <list>
#include <thread>
#include <vector>

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
  std::cout << "";
  std::cout << "  [--jobs <number>]: number of concurrently scheduled jobs\n";
  std::cout << "  [--cus <number>]: number of cus to use (default: 8) (max: 8)\n";
  std::cout << "  [--seconds <number>]: number of seconds to run\n";
  std::cout << "";
  std::cout << "* Program repeatedly enqueues an event graph for specified number of seconds\n";
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

// Create queues to execute the event graph
static xrt::queue q0;
static xrt::queue q1;
static xrt::queue q2;
static xrt::queue q3;
static xrt::queue q4;
static xrt::queue q5;

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
  std::array<xrt::queue::event, 6> ea;

  // run events
  std::array<xrt::queue::event, 7> er;

  // output events
  std::array<xrt::queue::event, 7> eo;

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
    // Create a lambda for the synchronous sync operation.
    // The lambda is enqueued and executed asynchronously
    static auto sync = [] (xrt::bo& bo, xclBOSyncDirection dir) {
                         return bo.sync(dir);
                       };

    /**
     * enqueue() - Enqueue a callable
     *
     * @param c
     *   Callable function, typically a lambda
     * @return
     *   Future result of the function (std::future)
     *
     * A callable is an argument less lambda function.  The function is
     * executed asynchronously by the queue consumer (worker thread).
     * Upon completion the returned future becomes valid and will
     * contain the return value of executing the lambda.
     */

    /**
     * enqueue() - Enqueue an event (type erased future)
     *
     * @oaram ev
     *   Event to enqueue
     * @param
     *   Future of event (std::shared_future<void>)
     *
     * Subsequent enqueued task blocks until the enqueued event is
     * valid.
     *
     * This type of enqueued event is used for synchronization between
     * multiple queues.
     */

    // sync q0:a0 when q0:r0 is done
    ea[0] = q0.enqueue([this] { sync(a[0], XCL_BO_SYNC_BO_TO_DEVICE); });

    // sync q1:a1 when q0:r0 and q1:r1 are done
    q1.enqueue(er[0]);
    ea[1] = q1.enqueue([this] { sync(a[1], XCL_BO_SYNC_BO_TO_DEVICE); });

    // sync q2:a2 when q1:r1 is done
    q2.enqueue(er[1]);
    ea[2] = q2.enqueue([this] { sync(a[2], XCL_BO_SYNC_BO_TO_DEVICE); });

    // async q3:a3 when q0:r2 is done
    q3.enqueue(er[2]);
    ea[3] = q3.enqueue([this] { sync(a[3], XCL_BO_SYNC_BO_TO_DEVICE); });

    // sync q4:a4 when q1:r3 and q4:r4 are done
    q4.enqueue(er[3]);
    ea[4] = q4.enqueue([this] { sync(a[4], XCL_BO_SYNC_BO_TO_DEVICE); });

    // sync q5:a5 when q4:r6 is done
    q5.enqueue(er[6]);
    ea[5] = q5.enqueue([this] { sync(a[5], XCL_BO_SYNC_BO_TO_DEVICE); });

    // run q0:r0 when q0:a0, q1:a1, q0:r2 are done
    q0.enqueue(ea[1]);
    er[0] = q0.enqueue([this] { r[0](a[0],a[1],o[0],ELEMENTS); r[0].wait(); });

    // run q1:r1 when q1:a1, q2:a2, q1:r3 are done
    q1.enqueue(ea[2]);
    er[1] = q1.enqueue([this] { r[1](a[1],a[2],o[1],ELEMENTS); r[1].wait(); });

    // run q0:r2 when q3:a3, q0:r0, q0:r5 are done
    q0.enqueue(ea[3]);
    er[2] = q0.enqueue([this] {r[2](a[1],o[0],o[2],ELEMENTS); r[2].wait(); });

    // run q1:r3 when q4:a4, q1:r1, q0:r5 are done
    q1.enqueue(ea[4]);
    q1.enqueue(er[5]);
    er[3] = q1.enqueue([this] {r[3](a[4],o[1],o[3],ELEMENTS); r[3].wait(); });

    // run q4:r4 when q4:a4, q1:r1, q4:r6 are done
    q4.enqueue(er[1]);
    er[4] = q4.enqueue([this] {r[4](a[4],o[1],o[4],ELEMENTS); r[4].wait(); });

    // run q0:r5 when q0:r2, q1:r3, q0:o5 are done
    q0.enqueue(er[3]);
    er[5] = q0.enqueue([this] {r[5](o[2],o[3],o[5],ELEMENTS); r[5].wait(); });

    // run q4:r6 when q5:a5, q4:r4, q4:o6 are done
    q4.enqueue(ea[5]);
    er[6] = q4.enqueue([this] {r[6](a[5],o[4],o[6],ELEMENTS); r[6].wait(); });

    // sync q0:o5 when q0:r5 is done
    eo[5] = q0.enqueue([this] { sync(o[5],XCL_BO_SYNC_BO_FROM_DEVICE); });

    // sync q4:o6 when q4:r6 is done
    q4.enqueue([this] { sync(o[6],XCL_BO_SYNC_BO_FROM_DEVICE); });
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

  stop = seconds == 0;

  for (int i=0; i<num_jobs; ++i)
    jobs.emplace_back(std::async(std::launch::async, [=] { run_async(device, kernel); });

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
