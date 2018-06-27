// Copyright (C) 2018 Xilinx Inc.
// All rights reserved.

// Testing of
//   - cdma kernel (yet to be supported in DSA)
//   - scheduler command dependencies via xclExecBufWithWaitList
// This test is not concerned with data integrety, its sole purpose is to 
// test scheduling.

#include "utils.hpp"
#include "task.hpp"
#include "xaddone_hw_64.h"

// driver includes
#include "ert.h"
#include "xclhal2.h"
#include "xclbin.h"

#include <fstream>
#include <list>
#include <getopt.h>

const size_t ELEMENTS = 16;
const size_t ARRAY_SIZE = 8;
const size_t MAXCUS = 8;

size_t cus = MAXCUS;

const static struct option long_options[] = {
  {"bitstream",       required_argument, 0, 'k'},
  {"hal_logfile",     required_argument, 0, 'l'},
  {"device",          required_argument, 0, 'd'},
  {"jobs",            required_argument, 0, 'j'},
  {"seconds",         required_argument, 0, 's'},
  {"verbose",         no_argument,       0, 'v'},
  {"help",            no_argument,       0, 'h'},
  // enable embedded runtime
  {"ert",             no_argument,       0, '1'},
  {"cdma",            no_argument,       0, '2'},
  {"wl",              no_argument,       0, '3'},
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
  std::cout << "  [--cdma]: enable embedded copy kernel (default: false)\n";
  std::cout << "  [--ert]:  enable embedded runtime (default: false)\n";
  std::cout << "  [--wl]:   use command waitlist (xclExecWithWaitList) (default: false)\n";
  std::cout << "  [--jobs <number>]: number of concurrently scheduled jobs\n";
  std::cout << "  [--seconds <number>]: number of seconds to run\n";
  std::cout << "";
  std::cout << "* Program schedules specified number of jobs as commands to scheduler.\n";
  std::cout << "* Scheduler starts commands based on CU availability and state.\n";
  std::cout << "* Summary prints \"jsz sec jobs\" for use with awk, where jobs is total number \n";
  std::cout << "* of jobs executed in the specified run time\n";
}

// Data for a single job
// 
// Job execution is defined as:
//   [a0,b0]->add0->[b0] 
//   [b0]->copy0->[b1]
//   [a1,b1]->add1->[b1] 
//   [b1]->copy1->[b2]
//   [a2,b1]->add2->[b2] 
//   [b2]->copy2->[b3]
//   [a3,b2]->add3->[b3]
//   [b3]->copy3->[b0]
//
// Kernels are scheduled with dependencies, such that a job execution
// is the following sequence of command executed by scheduler.
//   [a0][c0][a1][c1][a2][c2][a3][c3]
// A job is rescheduled immediately when it is done.
//
// Each command is tied to its own compute unit.
// 
// If --wl is specified, then all commands in a job are scheduled in
// parallel with embedded dependencies to preserve the required
// sequence in kds.
//
// If multiple jobs are specified, then all jobs are scheduled
// immediately and if --wl is specified, then all commands in a job
// are scheduled at once.  Since each commmand in a job is tied to a
// specific compute unit, multiple jobs fight for the same CUs, but
// pipeline is to happen at the scheduler level such that execution
// will be like
//  job1: [a0][c0][a1][c1][a2][c2][a3][c3][a0][c0][a1][c1][a2][c2][a3][c3]
//  job2:     [a0][c0][a1][c1][a2][c2][a3][c3][a0][c0][a1][c1][a2][c2][a3][c3]
//  job3:         [a0][c0][a1][c1][a2][c2][a3][c3][a0][c0][a1][c1][a2][c2][a3][c3]
//  job4:             [a0][c0][a1][c1][a2][c2][a3][c3][a0][c0][a1][c1][a2][c2][a3][c3]
//  job5:                 [a0][c0][a1][c1][a2][c2][a3][c3][a0][c0][a1][c1][a2][c2][a3][c3]
//  job6:                     [a0][c0][a1][c1][a2][c2][a3][c3][a0][c0][a1][c1][a2][c2][a3][c3]
//  job7:                         [a0][c0][a1][c1][a2][c2][a3][c3][a0][c0][a1][c1][a2][c2][a3][c3]
//  job8:                             [a0][c0][a1][c1][a2][c2][a3][c3][a0][c0][a1][c1][a2][c2][a3][c3]
//  job9:                                 [a0][c0][a1][c1][a2][c2][a3][c3][a0][c0][a1][c1][a2][c2][a3][c3]
// When job9+ are scheduled multiple commands in the scheduler will
// begin fighting for same CUs.
//
// If --ert is specified then the embedded scheduler is used.  The
// embedded scheduler is expected to improve performance when the HW
// command queue begin to saturate with pending commands.  Since the
// add and copy kernel are extremely fast (finish almost immediately)
// it will like be a handful of jobs before any visible benefit.
struct job_type
{
  size_t id;
  size_t runs;

  size_t data_size;
  std::vector<utils::buffer> add;  // add0,add1,add2,add3
  std::vector<utils::buffer> copy; // copy30,copy01,copy12,copy23
  std::vector<utils::buffer> a;    // a0,a1,a2,a3
  std::vector<utils::buffer> b;    // b0,b1,b2,b3

  std::atomic<bool> running{false};
  std::atomic<bool> completed{false};

  job_type()
  {}

  job_type(job_type&& rhs)
    : id(rhs.id), runs(rhs.runs)
    , add(std::move(rhs.add))
    , copy(std::move(rhs.copy))
    , a(std::move(rhs.a))
    , b(std::move(rhs.b))
  {}

  void
  reset_cmds()
  {
    for (int idx=0; idx<4; ++idx) {
      auto addcmd = reinterpret_cast<ert_packet*>(add[idx]->data);
      addcmd->state = ERT_CMD_STATE_NEW;
      auto cpcmd = reinterpret_cast<ert_start_kernel_cmd*>(copy[idx]->data);
      cpcmd->state = ERT_CMD_STATE_NEW;
    }
  }

  void 
  configure_add(int idx)
  {
    xclBOProperties p;
    uint64_t a_addr = !xclGetBOProperties(a[idx]->dev,a[idx]->bo,&p) ? p.paddr : -1;
    if (a_addr==static_cast<uint64_t>(-1))
      throw std::runtime_error("bad 'a' buffer object address");

    uint64_t b_addr = !xclGetBOProperties(b[idx]->dev,b[idx]->bo,&p) ? p.paddr : -1;
    if (b_addr==static_cast<uint64_t>(-1))
      throw std::runtime_error("bad 'b' buffer object address");

    size_t regmap_size = (XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4+1) + 1;

    auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(add[idx]->data);

    // Program the command packet header
    ecmd->state = ERT_CMD_STATE_NEW;
    ecmd->opcode = ERT_START_CU;
    ecmd->count = 1 + regmap_size;  // cu_mask + regmap
  
    // Program the CU mask. One CU at index idx
    ecmd->cu_mask = 1<<idx;

    ecmd->data[XADDONE_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
    ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4] = a_addr;
    ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4] = b_addr;
    ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4 + 1] = (a_addr >> 32) & 0xFFFFFFFF;
    ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4 + 1] = (b_addr >> 32) & 0xFFFFFFFF;
    ecmd->data[XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4] = ELEMENTS;
  }

  void 
  configure_copy(int in,int out)
  {
    xclBOProperties p;
    uint64_t in_addr = !xclGetBOProperties(b[in]->dev,b[in]->bo,&p) ? p.paddr : -1;
    if (in_addr==static_cast<uint64_t>(-1))
      throw std::runtime_error("bad 'in' buffer object address");

    uint64_t out_addr = !xclGetBOProperties(b[out]->dev,b[out]->bo,&p) ? p.paddr : -1;
    if (out_addr==static_cast<uint64_t>(-1))
      throw std::runtime_error("bad 'out' buffer object address");

    size_t regmap_size = (XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4+1) + 1;

    auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(copy[out]->data);

    // Program the command packet header
    ecmd->state = ERT_CMD_STATE_NEW;
    ecmd->opcode = ERT_START_CU;
    ecmd->count = 1 + regmap_size;  // cu_mask + regmap
  
    // Program the CU mask. One CU at index 0
    ecmd->cu_mask = 1<<(out+4);  // copy30 is CU 0+4=4, copy01 is CU 1+4=5, etc

    // Copy kernel has same signature as addone kernel
    ecmd->data[XADDONE_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
    ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4] = in_addr;
    ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4] = out_addr;
    ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4 + 1] = (in_addr >> 32) & 0xFFFFFFFF;
    ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4 + 1] = (out_addr >> 32) & 0xFFFFFFFF;
    ecmd->data[XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4] = ELEMENTS;
  }

  void 
  configure_cdma(int in, int out)
  {
#if 0
    xclBOProperties p;
    uint64_t in_addr = !xclGetBOProperties(b[in]->dev,b[in]->bo,&p) ? p.paddr : -1;
    if (in_addr==static_cast<uint64_t>(-1))
      throw std::runtime_error("bad 'in' buffer object address");

    uint64_t out_addr = !xclGetBOProperties(b[out]->dev,b[out]->bo,&p) ? p.paddr : -1;
    if (out_addr==static_cast<uint64_t>(-1))
      throw std::runtime_error("bad 'out' buffer object address");

    auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(job.ebo->data);

    // Program the command packet header
    ecmd->state = ERT_CMD_STATE_NEW;
    ecmd->opcode = ERT_START_CU;
    ecmd->count = 1 + ((0x28/4) + 1);  // cu_mask + regmap
  
    // Program the CU mask. One CU at index 0
    ecmd->cu_mask = 1<<9; // cdma is after regular cus (0b10000)

    ecmd->data[0] = 0x0; // ap_start
    ecmd->data[0x10/4] = out_addr;
    ecmd->data[0x10/4 + 1] = (out_addr >> 32) & 0xFFFFFFFF;
    ecmd->data[0x1c/4] = in_addr;
    ecmd->data[0x1c/4 + 1] = (in_addr >> 32) & 0xFFFFFFFF;
    ecmd->data[0x28/4] = (data_size*8)/512; // units of 512 bits
#endif
  }

  void 
  configure()
  {
    static size_t count = 0;
    id = count++;

    runs=0;
    for (int i=0; i<4; ++i)
      configure_add(i);

    for (int in=0,out=1; in<4; ++in,++out)
      configure_copy(in,out%4);
  }

  // Schedule this job in parallel with implicit waits in kds
  void 
  run()
  {
    running = true;
    ++runs;

    DEBUGF("starting job(%d,%d)\n",id,runs);

    xclExecBuf(add[0]->dev,add[0]->bo);
    xclExecBufWithWaitList(copy[0]->dev,copy[0]->bo,1,&add[0]->bo);

    xclExecBufWithWaitList(add[1]->dev,add[1]->bo,1,&copy[0]->bo);
    xclExecBufWithWaitList(copy[1]->dev,copy[1]->bo,1,&add[1]->bo);

    xclExecBufWithWaitList(add[2]->dev,add[2]->bo,1,&copy[1]->bo);
    xclExecBufWithWaitList(copy[2]->dev,copy[2]->bo,1,&add[2]->bo);

    xclExecBufWithWaitList(add[3]->dev,add[3]->bo,1,&copy[2]->bo);
    xclExecBufWithWaitList(copy[3]->dev,copy[3]->bo,1,&add[3]->bo);
  }

  // Wait for a specific command finish
  void 
  wait(ert_packet* epacket)
  {
    while (epacket->state != ERT_CMD_STATE_COMPLETED)
      xclExecWait(add[0]->dev,1000);
  }

  // Schedule this job sequentially with explicit waits
  // for each command to finish
  void
  run_wait()
  {
    running = true;
    ++runs;

    DEBUGF("starting job(%d,%d)\n",id,runs);

    xclExecBuf(add[0]->dev,add[0]->bo);
    wait(reinterpret_cast<ert_packet*>(add[0]->data));
    xclExecBuf(copy[0]->dev,copy[0]->bo);
    wait(reinterpret_cast<ert_packet*>(copy[0]->data));

    xclExecBuf(add[1]->dev,add[1]->bo);
    wait(reinterpret_cast<ert_packet*>(add[1]->data));
    xclExecBuf(copy[1]->dev,copy[1]->bo);
    wait(reinterpret_cast<ert_packet*>(copy[1]->data));

    xclExecBuf(add[2]->dev,add[2]->bo);
    wait(reinterpret_cast<ert_packet*>(add[2]->data));
    xclExecBuf(copy[2]->dev,copy[2]->bo);
    wait(reinterpret_cast<ert_packet*>(copy[2]->data));

    xclExecBuf(add[3]->dev,add[3]->bo);
    wait(reinterpret_cast<ert_packet*>(add[3]->data));
    xclExecBuf(copy[3]->dev,copy[3]->bo);
    wait(reinterpret_cast<ert_packet*>(copy[3]->data));

    done();
  }

  // Check if a job is done
  bool
  done()
  {
    // a job cannot be done unless it is currently in running state
    if (!running)
      return false;

    // check add3 and add0 are both complete
    auto epacket3 = reinterpret_cast<ert_packet*>(copy[3]->data);
    if (epacket3->state != ERT_CMD_STATE_COMPLETED)
      return false;

    // reset
    reset_cmds();

    DEBUGF("job(%lu) run(%lu) completed\n",id,runs);
    running = false;
    return true;
  }

  // Same as done
  bool 
  ready()
  {
    return done();
  }
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

// Global flag to indicate implicit waits with waitlist
static bool g_use_waitlist = false;

// Thread to launch ready jobs
static void
launcher_thread(const utils::device& d)
{
  auto launch = [](job_type& j) {
    j.run();
  };

  if (!g_use_waitlist) {
    PRINTF("executing each command sequentially\n");
    while (!g_stop) {
      for (auto& job: g_jobs)
        job.run_wait();
    }
    return;
  }

  // first launch all jobs
  PRINTF("executing with command waitlist\n");
  for (auto& job : g_jobs) {
    DEBUGF("scheduling job(%d,%d)\n",job.id,job.runs);
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
      DEBUGF("checking job(%d,%d)\n",job.id,job.runs);
      if (job.ready()) {
        DEBUGF("re-scheduling job(%d,%d)\n",job.id,job.runs);
        task::createF(g_launch_queue,launch,std::ref(job));
      }
    }

  }

  // wait for all launched jobs to finish
  for (auto& job : g_jobs) {
    while (!job.done())
      while (xclExecWait(d->handle,1000)==0);
  }

}

static void
init_scheduler(const utils::device& d, bool ert, bool cdma)
{
  auto execbo = utils::get_exec_buffer(d,1024);
  auto ecmd = reinterpret_cast<ert_configure_cmd*>(execbo->data);
  ecmd->state = ERT_CMD_STATE_NEW;
  ecmd->opcode = ERT_CONFIGURE;

  ecmd->slot_size = 4096;
  ecmd->num_cus = cus + (cdma ? 1 : 0);
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

  // CDMA kernel
  if (cdma) {
    ecmd->data[cus] = 0x00240000;
    ecmd->count += 1;  // +1 for copy kernel
  }
  

  if (xclExecBuf(d->handle,execbo->bo))
    throw std::runtime_error("unable to issue xclExecBuf");

  while (xclExecWait(d->handle,1000)==0);
}


static int 
run(const utils::device& dev, size_t jobs, size_t seconds, bool ert, bool cdma)
{
  init_scheduler(dev, ert, cdma);

  // Create jobs.  4 add commands, and 4 copy commands. 
  // Allocate arguments in 4 banks, such that copy kernel can copy
  // from one bank to another (multiple banks not yet supported)
  const size_t data_size = ELEMENTS * ARRAY_SIZE;

  for (size_t j=0; j<jobs; ++j) {
    job_type job;

    for (size_t i=0; i<4; ++i) {
      auto bank = i;
      // for now
      bank = 0;
      auto a = utils::create_bo(dev,data_size*sizeof(unsigned long),bank);
      auto adata = reinterpret_cast<unsigned long*>(a->data);
      auto b = utils::create_bo(dev,data_size*sizeof(unsigned long),bank);
      auto bdata = reinterpret_cast<unsigned long*>(b->data);
      for (size_t j=0;j<data_size;++j) {
        adata[j] = i;
        bdata[j] = j+i;
      }

      // Exec buffer object
      job.a.push_back(a);
      job.b.push_back(b);
      job.add.emplace_back(utils::create_exec_bo(dev,1024));
      job.copy.emplace_back(utils::create_exec_bo(dev,1024));
    }

    job.configure();
    g_jobs.emplace_back(std::move(job));
  }

  // start launcher thread
  g_launcher = std::move(std::thread(launcher_thread,dev));

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

  if (ert)
    std::cout << "ert";
  else 
    std::cout << "kds";
  if (g_use_waitlist)
    std::cout << " (wl): ";
  else 
    std::cout << ": ";
  std::cout << "jobsize seconds total = "
            << jobs << " "
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
  bool cdma = false;
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
      cdma = true;
      break;
    case '3':
      g_use_waitlist = true;
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

  if (cdma) {
    std::cout << "Ignorning --cdma because CDMA kernel not yet supported\n";
    cdma = false;
  }

  run(device,jobs,seconds,ert,cdma);

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
