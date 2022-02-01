/**
 * Copyright (C) 2022 Xilinx, Inc
 * SPDX-License-Identifier: Apache-2.0
 */

/*
  Overlap Host Code

  There are many applications where all of the data cannot reside in an FPGA.
  For example, the data is too big to fit in an FPGA or the data is being
  streamed from a sensor or the network. In these situations data must be
  transferred to the host memory to the FPGA before the computation can be
  performed.

  Because PCIe is an full-duplex interconnect, you can transfer data to and from
  the FPGA simultaneously. Xilinx FPGAs can also perform computations during
  these data transfers. Performing all three of these operations at the same
  time allows you to keep the FPGA busy and take full advantage of all of the
  hardware on your system.

  In this example, we will demonstrate how to perform this using an out of order
  command queue.

  +---------+---------+---------+----------+---------+---------+---------
  | WriteA1 | WriteB1 | WriteA2 | Write B2 | WriteA1 | WriteB1 |   Wri...
  +---------+---------+---------+----------+---------+---------+---------
                      |       Compute1     |     Compute2      |  Compu...
                      +--------------------+-------------------+--------+
                                           | ReadC1 |          | ReadC2 |
                                           +--------+          +--------+

  Many OpenCL commands are asynchronous. This means that whenever you call an
  OpenCL function, the function will return before the operation has completed.
  Asynchronous nature of OpenCL allows you to simultaneously perform tasks on
  the host CPU as well as the FPGA.

  Memory transfer operations are asynchronous when the blocking_read,
  blocking_write parameters are set to CL_FALSE. These operations are behaving
  on host memory so it is important to make sure that the command has completed
  before that memory is used.

  You can make sure an operation has completed by querying events returned by
  these commands. Events are OpenCL objects that track the status of operations.
  Event objects are created by kernel execution commands, read, write, copy
  commands on memory objects or user events created using clCreateUserEvent.

  Events can be used to synchronize operations between the host thread and the
  device or between two operations in the same context. You can also use events
  to time a particular operation if the command queue was created using the
  CL_QUEUE_PROFILING_ENABLE flag.

  Most enqueuing commands return events by accepting a cl_event pointer as their
  last argument of the call. These events can be queried using the
  clGetEventInfo function to get the status of a particular operation.

  Many functions also accept event lists that can be used to enforce ordering in
  an OpenCL context. These events lists are especially important in the context
  of out of order command queues as they are the only way specify dependency.
  Normal in-order command queues do not need this because dependency is enforced
  in the order the operation was enqueued. See the concurrent execution example
  for additional details on how create an use these types of command queues.
 */

#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

using std::default_random_engine;
using std::generate;
using std::uniform_int_distribution;
using std::vector;

constexpr int ARRAY_SIZE = 1 << 14;

int
gen_random()
{
  static default_random_engine e;
  static uniform_int_distribution<int> dist(0, 100);

  return dist(e);
}

int
run(const xrt::device& device, const xrt::kernel& kernel, size_t iterartions)
{
  xrt::queue q0;
  xrt::queue q1;
  xrt::queue q2;

  // We will break down our problem into multiple iterations. Each iteration
  // will perform computation on a subset of the entire data-set.
  constexpr size_t elements_per_iteration = 2048;
  constexpr size_t bytes_per_iteration = elements_per_iteration * sizeof(int);
  constexpr size_t num_iterations = ARRAY_SIZE / elements_per_iteration;

  std::vector<int> A(ARRAY_SIZE);
  std::vector<int> B(ARRAY_SIZE);
  std::vector<int> C(ARRAY_SIZE);
  std::generate(begin(A), end(A), gen_random);
  std::generate(begin(B), end(B), gen_random);

  // ping/pong buffer set
  std::array<xrt::bo, 2> a{device, bytes_per_iteration, kernel::group_id(0)};
  std::array<xrt::bo, 2> b{device, bytes_per_iteration, kernel::group_id(1)};;
  std::array<xrt::bo, 2> c{device, bytes_per_iteration, kernel::group_id(2)};;



  for (int
}

int
run(int argc, char* argv[])
{
  if (argc != 2) {
    std::cout << "Usage: " << argv[0] << " <XCLBIN File>\n";
    return EXIT_FAILURE;
  }

  xrt::device device(0);
  xrt::xclbin xclbin(argv[0]);
  auto uuid = device.load_xclbin(xclbin);
  xrt::kernel kernel(device, uuid, "...");


}

int
main(int argc, char* argv[])
{
  try {
    return run(argc, argv);
  }
  catch (const std::exception& ex) {
    std::cout << "ERROR: " << ex.what() << "\n";
    return EXIT_FAILURE;
  }



    auto binaryFile = argv[1];
    cl_int err;
    cl::CommandQueue q;
    cl::Context context;
    cl::Kernel krnl_vadd;

    // OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the xilinx
    // platforms and will return list of devices connected to Xilinx platform
    std::cout << "Creating Context..." << std::endl;
    auto devices = xcl::get_xil_devices();

    // read_binary_file() is a utility API which will load the binaryFile
    // and will return the pointer to file buffer.
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
    bool valid_device = false;
    for (unsigned int i = 0; i < devices.size(); i++) {
        auto device = devices[i];
        // Creating Context and Command Queue for selected Device
        OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
        // This example will use an out of order command queue. The default command
        // queue created by cl::CommandQueue is an inorder command queue.
        OCL_CHECK(err, q = cl::CommandQueue(context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err));

        std::cout << "Trying to program device[" << i << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
        cl::Program program(context, {device}, bins, nullptr, &err);
        if (err != CL_SUCCESS) {
            std::cout << "Failed to program device[" << i << "] with xclbin file!\n";
        } else {
            std::cout << "Device[" << i << "]: program successful!\n";
            OCL_CHECK(err, krnl_vadd = cl::Kernel(program, "vadd", &err));
            valid_device = true;
            break; // we break because we found a valid device
        }
    }
    if (!valid_device) {
        std::cout << "Failed to program any device found, exit!\n";
        exit(EXIT_FAILURE);
    }

    // We will break down our problem into multiple iterations. Each iteration
    // will perform computation on a subset of the entire data-set.
    size_t elements_per_iteration = 2048;
    size_t bytes_per_iteration = elements_per_iteration * sizeof(int);
    size_t num_iterations = ARRAY_SIZE / elements_per_iteration;

    // Allocate memory on the host and fill with random data.
    vector<int, aligned_allocator<int> > A(ARRAY_SIZE);
    vector<int, aligned_allocator<int> > B(ARRAY_SIZE);
    generate(begin(A), end(A), gen_random);
    generate(begin(B), end(B), gen_random);
    vector<int, aligned_allocator<int> > device_result(ARRAY_SIZE);

    // THIS PAIR OF EVENTS WILL BE USED TO TRACK WHEN A KERNEL IS FINISHED WITH
    // THE INPUT BUFFERS. ONCE THE KERNEL IS FINISHED PROCESSING THE DATA, A NEW
    // SET OF ELEMENTS WILL BE WRITTEN INTO THE BUFFER.
    vector<cl::Event> kernel_events(2);
    vector<cl::Event> read_events(2);
    cl::Buffer buffer_a[2], buffer_b[2], buffer_c[2];

    for (size_t iteration_idx = 0; iteration_idx < num_iterations; iteration_idx++) {
        int flag = iteration_idx % 2;

        if (iteration_idx >= 2) {
            OCL_CHECK(err, err = read_events[flag].wait());
        }

        // Allocate Buffer in Global Memory
        // Buffers are allocated using CL_MEM_USE_HOST_PTR for efficient memory and
        // Device-to-host communication
        std::cout << "Creating Buffers..." << std::endl;
        OCL_CHECK(err, buffer_a[flag] = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, bytes_per_iteration,
                                                   &A[iteration_idx * elements_per_iteration], &err));
        OCL_CHECK(err, buffer_b[flag] = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, bytes_per_iteration,
                                                   &B[iteration_idx * elements_per_iteration], &err));
        OCL_CHECK(err,
                  buffer_c[flag] = cl::Buffer(context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, bytes_per_iteration,
                                              &device_result[iteration_idx * elements_per_iteration], &err));

        vector<cl::Event> write_event(1);

        OCL_CHECK(err, err = krnl_vadd.setArg(0, buffer_c[flag]));
        OCL_CHECK(err, err = krnl_vadd.setArg(1, buffer_a[flag]));
        OCL_CHECK(err, err = krnl_vadd.setArg(2, buffer_b[flag]));
        OCL_CHECK(err, err = krnl_vadd.setArg(3, int(elements_per_iteration)));

        // Copy input data to device global memory
        std::cout << "Copying data (Host to Device)..." << std::endl;
        // Because we are passing the write_event, it returns an event object
        // that identifies this particular command and can be used to query
        // or queue a wait for this particular command to complete.
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_a[flag], buffer_b[flag]}, 0 /*0 means from host*/,
                                                        nullptr, &write_event[0]));
        set_callback(write_event[0], "ooo_queue");

        printf("Enqueueing NDRange kernel.\n");
        // This event needs to wait for the write buffer operations to complete
        // before executing. We are sending the write_events into its wait list to
        // ensure that the order of operations is correct.
        // Launch the Kernel
        std::vector<cl::Event> waitList;
        waitList.push_back(write_event[0]);
        OCL_CHECK(err, err = q.enqueueNDRangeKernel(krnl_vadd, 0, 1, 1, &waitList, &kernel_events[flag]));
        set_callback(kernel_events[flag], "ooo_queue");

        // Copy Result from Device Global Memory to Host Local Memory
        std::cout << "Getting Results (Device to Host)..." << std::endl;
        std::vector<cl::Event> eventList;
        eventList.push_back(kernel_events[flag]);
        // This operation only needs to wait for the kernel call. This call will
        // potentially overlap the next kernel call as well as the next read
        // operations
        OCL_CHECK(err, err = q.enqueueMigrateMemObjects({buffer_c[flag]}, CL_MIGRATE_MEM_OBJECT_HOST, &eventList,
                                                        &read_events[flag]));
        set_callback(read_events[flag], "ooo_queue");
    }

    // Wait for all of the OpenCL operations to complete
    printf("Waiting...\n");
    OCL_CHECK(err, err = q.flush());
    OCL_CHECK(err, err = q.finish());
    // OPENCL HOST CODE AREA ENDS
    bool match = true;
    // Verify the results
    for (int i = 0; i < ARRAY_SIZE; i++) {
        int host_result = A[i] + B[i];
        if (device_result[i] != host_result) {
            printf("Error: Result mismatch:\n");
            printf("i = %d CPU result = %d Device result = %d\n", i, host_result, device_result[i]);
            match = false;
            break;
        }
    }

    printf("TEST %s\n", (match ? "PASSED" : "FAILED"));
    return (match ? EXIT_SUCCESS : EXIT_FAILURE);
}

/****************************************************************
Enqueue example illustrating use of xrt::queue APIs

This example uses xrt::queue objects to create two concurrent
write-execute-read pipelines with two sets of buffer objects.

The example shows how to make xrt::bo::sync an asynchronous operation
by wrapping the synchronous sync operation in a callable lambda which
is then enqueued.

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
thead but synchronously and in order within the queue.

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


#include "xrt.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "experimental/xrt_queue.h"
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

// Create queues to execute write-execute-read for 2 input, 1 output kernel
static xrt::queue qwrite0;
static xrt::queue qwrite1;
static xrt::queue qexe;
static xrt::queue qread;

// Data for a single job
struct job_type
{
  size_t id = 0;
  size_t runs = 0;

  // Kernel object
  // void addone (__global ulong8 *in1, __global ulong8* in2, __global ulong8* out, unsigned int elements)
  xrt::kernel k;

  // Run for the job
  xrt::run r;

  // Input buffers for the runs
  std::array<xrt::bo, 2> a;

  // Output buffers
  std::array<xrt::bo, 1> o;

  job_type(const xrt::device& device, xrt::kernel krnl)
    : k(std::move(krnl))
  {
    static size_t count=0;
    id = count++;

    auto grpid0 = k.group_id(0);
    auto grpid1 = k.group_id(1);
    const size_t data_size = ELEMENTS * ARRAY_SIZE;

    run = xrt::run(k);

    for (int i = 0; i < 2; ++i)
      a[i] = xrt::bo(device, data_size * sizeof(unsigned long), grpid0);

    for (int i = 0; i < 1; ++i)
      o[i] = xrt::bo(device, data_size * sizeof(unsigned long), grpid1);

    run.set_arg(0, a[0]);
    run.set_arg(1, a[1]);
    run.set_arg(2, o[0]);
  }

  job_type(job_type&& rhs)
    : id(rhs.id)
    , run(rhs.run)
    , k(std::move(rhs.k))
    , r(std::move(rhs.r))
    , a(std::move(rhs.a))
    , o(std::move(rhs.o))
  {}

  void
  enqueue()
  {
    // Create a lambda for the synchronous sync operation
    // The lambda is enqueued and executed asynchrnously
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

    // write a[0]
    auto ea0 = qwrite0.enqueue([this]() { sync(a[0], XCL_BO_SYNC_BO_TO_DEVICE); });

    // write a[1]
    auto ea1 = qwrite1.enqueue([this]() { sync(a[1], XCL_BO_SYNC_BO_TO_DEVICE); });

    // execute q1:r[0] when q0:a0 and q1:a1 are done
    qexe.enqueue(ea0);
    qexe.enqueue(ea1);
    auto e_run = qexe.enqueue([this]() { er[0].start(); er[0].wait(); });

    // read o[0] and consume
    qread.enqueue(e_run);
    auto e_o = qread.enqueue([this]() { sync(o[0], XCL_BO_SYNC_BO_FROM_DEVICE); });

    // prepare new input
    e_run.wait();
    //..,



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
