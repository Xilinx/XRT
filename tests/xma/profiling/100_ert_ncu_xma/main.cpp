// Copyright (C) 2018 Xilinx Inc.
// All rights reserved.

#include "utils.hpp"
#include "task.hpp"
#include "xaddone_hw_64.h"

// driver includes
#include "ert.h"
#include "xclhal2.h"
#include "xclbin.h"
#include "xma_profile.h"

#include <fstream>
#include <list>
#include <getopt.h>

const size_t ELEMENTS = 16;
const size_t ARRAY_SIZE = 8;
const size_t MAXCUS = 8;

size_t cus = MAXCUS;
size_t slotsize = 4096;

const static struct option long_options[] = {
  {"bitstream",       required_argument, 0, 'k'},
  {"hal_logfile",     required_argument, 0, 'l'},
  {"device",          required_argument, 0, 'd'},
  {"jobs",            required_argument, 0, 'j'},
  {"cus",             required_argument, 0, 'c'},
  {"seconds",         required_argument, 0, 's'},
  {"verbose",         no_argument,       0, 'v'},
  {"help",            no_argument,       0, 'h'},
  // enable embedded runtime
  {"ert",             no_argument,       0, '1'},
  {"slotsize",        required_argument, 0, '2'},
  {0, 0, 0, 0}
};

static void printHelp()
{
  std::cout << "usage: %s [options] -k <bitstream>\n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -l <hal_logfile>\n";
  std::cout << "  -d <device_index>\n";
  std::cout << "  -v\n";
  std::cout << "  -h\n\n";
  std::cout << "";
  std::cout << "  [--ert]: enable embedded runtime (default: false)\n";
  std::cout << "  [--slotsize]: command queue slotsize in kB (default: 4096)\n";
  std::cout << "  [--jobs <number>]: number of concurrently scheduled jobs\n";
  std::cout << "  [--cus <number>]: number of cus to use (default: 8) (max: 8)\n";
  std::cout << "  [--seconds <number>]: number of seconds to run\n";
  std::cout << "";
  std::cout << "* Program schedules specified number of jobs as commands to scheduler.\n";
  std::cout << "* Scheduler starts commands based on CU availability and state.\n";
  std::cout << "* Summary prints \"jsz sec jobs\" for use with awk, where jobs is total number \n";
  std::cout << "* of jobs executed in the specified run time\n";
}

// Data for a single job
struct job_type
{
  size_t id = 0;
  size_t runs = 0;
  std::atomic<bool> running{false};

  // execution buffer and arguments buffers to transfer to ddr
  utils::buffer ebo;
  utils::buffer a;
  utils::buffer b;

  job_type(utils::buffer e, utils::buffer aarg, utils::buffer barg)
    : ebo(std::move(e)), a(std::move(aarg)), b(std::move(barg))
  {
    static size_t count=0;
    id = count++;
  }
  
  job_type(job_type&& rhs)
    : id(rhs.id), runs(rhs.runs), ebo(std::move(rhs.ebo)), a(std::move(rhs.a)), b(std::move(rhs.b))
  {}
};

// Collection of all jobs, size is equal to number of jobs specified
// in command line option
using job_vec = std::vector<job_type>;
static job_vec g_jobs;

// Task launch queue is serviced by a worker thread, it is filled with
// jobs as they are ready to be scheduled.
static task::queue g_launch_queue;
static std::thread g_worker;

// Launcher is a separate thread that adds jobs to launch queue when
// they are ready to be scheduled.
static std::thread g_launcher;

// Stop all threads gracefully by setting g_stop to true
static std::atomic<bool> g_stop{false};

// Check if a job is running and has completed, in which case it can
// be relaunched.
inline bool
ready(job_type& job)
{
  if (job.running) {
    auto epacket = reinterpret_cast<ert_packet*>(job.ebo->data);
    if (epacket->state == ERT_CMD_STATE_COMPLETED) {
      DEBUGF("job %lu completed\n",job.id);
      job.running = false;
      return true;
    }
  }

  return false;
}

// Run a job
static void
run_kernel(const utils::device& d, job_type& job)
{
  xclBOProperties p;
  uint64_t a_addr = !xclGetBOProperties(job.a->dev,job.a->bo,&p) ? p.paddr : -1;
  if (a_addr==static_cast<uint64_t>(-1))
    throw std::runtime_error("bad 'a' buffer object address");

  uint64_t b_addr = !xclGetBOProperties(job.b->dev,job.b->bo,&p) ? p.paddr : -1;
  if (b_addr==static_cast<uint64_t>(-1))
    throw std::runtime_error("bad 'b' buffer object address");

  size_t regmap_size = (XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4+1) + 1;

  auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(job.ebo->data);

  // Program the command packet header
  ecmd->state = ERT_CMD_STATE_NEW;
  ecmd->opcode = ERT_START_CU;
  ecmd->count = 1 + regmap_size;  // cu_mask + regmap
  
  // Program the CU mask. One CU at index 0
  ecmd->cu_mask = (1<<cus)-1; // 0xFF for 8 CUs

  ecmd->data[XADDONE_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
  ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4] = a_addr;
  ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4] = b_addr;
  ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4 + 1] = (a_addr >> 32) & 0xFFFFFFFF;
  ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4 + 1] = (b_addr >> 32) & 0xFFFFFFFF;
  ecmd->data[XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4] = ELEMENTS;

  // Synchronize with launcher so that it knows this job has been scheduled
  job.running = true;
  ++job.runs;

  if (xclExecBuf(d->handle,job.ebo->bo))
    throw std::runtime_error("unable to issue xclExecBuf");

  DEBUGF("started job (%lu,%lu)\n",job.id,job.runs);
}

// Thread to launch ready jobs
static void
launcher_thread(const utils::device& d)
{
  auto launch = [&d](job_type& j) {
    run_kernel(d,j);
  };

  // first launch all jobs
  for (auto& job : g_jobs) {
    DEBUGF("scheduling job %lu\n",job.id);
    task::createF(g_launch_queue,launch,std::ref(job));
  }

  // now iterate until stopped
  while (!g_stop) {

    // wait for at least one job to complete
    DEBUGF("waiting for one job to complete\n");
    while (xclExecWait(d->handle,1000)==0) {
      DEBUGF("reentering wait\n");
    }

    
    for (auto& job : g_jobs) {
      DEBUGF("checking job %lu\n",job.id);
      if (ready(job)) {
        DEBUGF("scheduling job %lu\n",job.id);
        task::createF(g_launch_queue,launch,std::ref(job));
      }
    }

  }

  // wait for all running commands to finish
  for (auto& job : g_jobs) {
    while (!ready(job))
      while (xclExecWait(d->handle,1000)==0);
  }

}

static void
init_scheduler(const utils::device& d, bool ert)
{
  auto execbo = utils::get_exec_buffer(d,1024);
  auto ecmd = reinterpret_cast<ert_configure_cmd*>(execbo->data);
  ecmd->state = ERT_CMD_STATE_NEW;
  ecmd->opcode = ERT_CONFIGURE;

  ecmd->slot_size = slotsize;
  ecmd->num_cus = cus;
  ecmd->cu_shift = 16;
  ecmd->cu_base_addr = d->cu_base_addr; 
  
  ecmd->ert = ert;
  if (ert) {
    ecmd->cu_dma = 1;
    ecmd->cu_isr = 1;
  }

  // TODO: read from xclbin
  for (size_t i=0; i<cus; ++i)
    ecmd->data[i] = (i<<16) + d->cu_base_addr;
  
  ecmd->count = 5 + cus;

  if (xclExecBuf(d->handle,execbo->bo))
    throw std::runtime_error("unable to issue xclExecBuf");

  while (xclExecWait(d->handle,1000)==0);
}


static int 
run(const utils::device& d,size_t jobs, size_t seconds, bool ert)
{
  init_scheduler(d, ert);

  profile_initialize(d->handle, 1, 1, "coarse", "all");
  profile_start(d->handle);	

  // Create specified number of jobs.  All jobs shared input vector 'a'
  const size_t data_size = ELEMENTS * ARRAY_SIZE;
  auto a = utils::create_bo(d,data_size*sizeof(unsigned long));
  auto adata = reinterpret_cast<unsigned long*>(a->data);
  for (size_t i=0;i<data_size;++i)
    adata[i] = i;

  // Each job has its own input/output vector 'b'
  for (size_t i=0; i<jobs; ++i) {
    auto b = utils::create_bo(d,data_size*sizeof(unsigned long));
    auto bdata = reinterpret_cast<unsigned long*>(b->data);
    for (size_t j=0;j<data_size;++j) {
      bdata[j] = i;
    }

    // Exec buffer object
    auto execbo = utils::create_exec_bo(d,1024);

    // Construct and store the job
    g_jobs.emplace_back(execbo,a,b);
  }

  // start launcher thread
  g_launcher = std::move(std::thread(launcher_thread,d));

  // start worker thread that services the task queue
  g_worker = std::move(std::thread(task::worker,std::ref(g_launch_queue)));

  // Now run for specified period of time
  std::this_thread::sleep_for(std::chrono::seconds(seconds));

  // Stop everything gracefully
  g_stop = 1;
  g_launcher.join();

  g_launch_queue.stop();
  g_worker.join();

  size_t total = 0;
  for (auto& job : g_jobs) {
    total += job.runs;
    DEBUGF("job (%lu,%lu)\n",job.id,job.runs);
  }

  profile_stop(d->handle);
  profile_finalize(d->handle);	

  if (ert)
    std::cout << "ert: ";
  else 
    std::cout << "kds: ";
  std::cout << "jobsize cus seconds total = "
            << jobs << " "
            << cus << " "
            << seconds << " "
            << total << "\n";
                       
  return 0;
}

int run(int argc, char** argv)
{
  std::string bitstream;
  std::string hallog;
  int option_index = 0;
  unsigned device_index = 0;
  size_t jobs=10;
  size_t seconds=10;
  bool verbose = false;
  bool ert = false;
  int c;
  while ((c = getopt_long(argc, argv, "k:l:d:j:vh", long_options, &option_index)) != -1) {
    switch (c) {
    case 0:
      if (long_options[option_index].flag != 0)
        break;
    case '1':
      ert = true;
      break;
    case '2':
      slotsize = std::atoi(optarg);
      break;
    case 'k':
      bitstream = optarg;
      break;
    case 'l':
      hallog = optarg;
      break;
    case 'd':
      device_index = std::atoi(optarg);
      break;
    case 'j':
      jobs = std::atoi(optarg);
      break;
    case 'c':
      cus = std::min(8,std::atoi(optarg));
      break;
    case 's':
      seconds = std::atoi(optarg);
      break;
    case 'h':
      printHelp();
      return 0;
    case 'v':
      verbose = true;
      break;
    default:
      printHelp();
      return -1;
    }
  }

  // bogus compiler warnings
  (void)verbose;

  if (bitstream.empty())
    throw std::runtime_error("No bitstream specified");

  if (!hallog.empty())
    std::cout << "Using " << hallog << " as XRT driver logfile\n";

  std::cout << "Compiled kernel = " << bitstream << std::endl;

  auto device = utils::init(bitstream,device_index,hallog);
  run(device,jobs,seconds,ert);

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
