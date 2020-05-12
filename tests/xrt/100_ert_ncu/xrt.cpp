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

#include "xaddone_hw_64.h"

// driver includes
#include "ert.h"
#include "xrt.h"
#include "xclbin.h"

#include <fstream>
#include <list>
#include <thread>
#include <atomic>
#include <iostream>
#include <vector>

namespace {

static std::vector<char>
load_xclbin(xclDeviceHandle device, const std::string& fnm)
{
  if (fnm.empty())
    throw std::runtime_error("No xclbin speified");

  // load bit stream
  std::ifstream stream(fnm);
  stream.seekg(0,stream.end);
  size_t size = stream.tellg();
  stream.seekg(0,stream.beg);

  std::vector<char> header(size);
  stream.read(header.data(),size);

  auto top = reinterpret_cast<const axlf*>(header.data());
  if (xclLoadXclBin(device, top))
    throw std::runtime_error("Bitstream download failed");

  return header;
}
  
}

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

// Data for a single job
struct job_type
{
  size_t id = 0;
  size_t runs = 0;

  // execution buffer and arguments buffers to transfer to ddr
  xclDeviceHandle d;
  xclBufferHandle a;
  void* a_data;
  xclBufferHandle b;
  void* b_data;

  uint64_t a_addr = -1;
  uint64_t b_addr = -1;

  xclBufferHandle ebo;
  void* ebo_data;

  job_type(xclDeviceHandle device, unsigned int first_used_mem)
    : d(device)
  {
    static size_t count=0;
    id = count++;

    const size_t data_size = ELEMENTS * ARRAY_SIZE;
    a = xclAllocBO(d, data_size*sizeof(unsigned long), 0, first_used_mem);
    a_data = xclMapBO(d, a, true);
    auto adata = reinterpret_cast<unsigned long*>(a_data);
    for (size_t i=0;i<data_size;++i)
      adata[i] = i;

    b = xclAllocBO(d, data_size*sizeof(unsigned long), 0, first_used_mem);
    b_data = xclMapBO(d, b, true);
    auto bdata = reinterpret_cast<unsigned long*>(b_data);
     for (size_t j=0;j<data_size;++j)
       bdata[j] = id;

    xclBOProperties p;
    a_addr = !xclGetBOProperties(d,a,&p) ? p.paddr : -1;
    b_addr = !xclGetBOProperties(d,b,&p) ? p.paddr : -1;

    // Exec buffer object
    ebo = xclAllocBO(d, 1024, 0, XCL_BO_FLAGS_EXECBUF);
    ebo_data = xclMapBO(d, ebo, true);
  }

  job_type(job_type&& rhs)
    : id(rhs.id)
    , runs(rhs.runs)
    , d(rhs.d)
    , a(rhs.a)
    , a_data(rhs.a_data)
    , b(rhs.b)
    , b_data(rhs.b_data)
    , a_addr(rhs.a_addr)
    , b_addr(rhs.b_addr)
    , ebo(rhs.ebo)
    , ebo_data(rhs.ebo_data)
  {
    d=XRT_NULL_HANDLE;
    a=b=ebo=XRT_NULL_BO;
    a_data=b_data=ebo_data=nullptr;
  }

  ~job_type()
  {
    if (a_data) {
      xclUnmapBO(d, a, a_data);
      xclFreeBO(d, a);
    }
    if (b_data) {
      xclUnmapBO(d, b, b_data);
      xclFreeBO(d, b);
    }
    if (ebo_data) {
      xclUnmapBO(d, ebo, ebo_data);
      xclFreeBO(d, ebo);
    }
  }

  void
  run()
  {
    ++runs;

    size_t regmap_size = (XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4+1) + 1;

    auto ecmd = reinterpret_cast<ert_start_kernel_cmd*>(ebo_data);

    // Program the command packet header
    ecmd->state = ERT_CMD_STATE_NEW;
    ecmd->opcode = ERT_START_CU;
    ecmd->count = 1 + regmap_size;  // cu_mask + regmap

    // Program the CU mask. One CU at index 0
    ecmd->cu_mask = (1<<compute_units)-1; // 0xFF for 8 CUs

    ecmd->data[XADDONE_CONTROL_ADDR_AP_CTRL] = 0x0; // ap_start
    ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4] = a_addr;
    ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4] = b_addr;
    ecmd->data[XADDONE_CONTROL_ADDR_A_DATA/4 + 1] = (a_addr >> 32) & 0xFFFFFFFF;
    ecmd->data[XADDONE_CONTROL_ADDR_B_DATA/4 + 1] = (b_addr >> 32) & 0xFFFFFFFF;
    ecmd->data[XADDONE_CONTROL_ADDR_ELEMENTS_DATA/4] = ELEMENTS;

    if (xclExecBuf(d,ebo))
      throw std::runtime_error("unable to issue xclExecBuf");
  }

  bool
  done()
  {
    auto epacket = reinterpret_cast<ert_packet*>(ebo_data);
    if (epacket->state == ERT_CMD_STATE_COMPLETED) {
      return true;
    }
    return false;
  }
    
};

// Launcher is a separate thread that adds jobs to launch queue when
// they are ready to be scheduled.
static std::thread g_launcher;

// Stop all threads gracefully by setting g_stop to true
static std::atomic<bool> g_stop{false};

static std::vector<job_type> g_jobs;

// Thread to launch ready jobs
static void
launcher_thread(xclDeviceHandle d)
{
  // start all jobs
  std::for_each(g_jobs.begin(),g_jobs.end(),[](job_type& j){j.run();});

  // now iterate until stopped
  while (!g_stop) {
    // wait for at least one job to complete
    while (xclExecWait(d,1000)==0) {
    }

    for (auto& job : g_jobs) {
      if (job.done() && !g_stop) {
        job.run();
      }
    }
  }

  // wait for all running commands to finish
  for (auto& job : g_jobs) {
    while (!job.done())
      while (xclExecWait(d,1000)==0);
  }
}


static int
run(xclDeviceHandle d,size_t num_jobs, size_t seconds, int first_used_mem)
{
  g_jobs.reserve(num_jobs);
  for (int i=0; i<num_jobs; ++i)
    g_jobs.emplace_back(d, first_used_mem);

  if (seconds == 0)
    g_stop = true;

  // start launcher thread
  g_launcher = std::move(std::thread(launcher_thread,d));

  // Now run for specified period of time
  std::this_thread::sleep_for(std::chrono::seconds(seconds));

  // Stop everything gracefully
  g_stop = true;
  g_launcher.join();

  size_t total = 0;
  for (auto& job : g_jobs) {
    total += job.runs;
  }

  std::cout << "xrt: ";
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

  auto probe = xclProbe();
  if (probe < device_index)
    throw std::runtime_error("Bad device index '" + std::to_string(device_index) + "'");

  auto device = xclOpen(device_index, nullptr, XCL_QUIET);

  auto header = load_xclbin(device, xclbin_fnm);
  auto top = reinterpret_cast<const axlf*>(header.data());
  auto ip = xclbin::get_axlf_section(top, IP_LAYOUT);
  auto layout = reinterpret_cast<ip_layout*>(header.data() + ip->m_sectionOffset);
  auto topo = xclbin::get_axlf_section(top, MEM_TOPOLOGY);
  auto topology = reinterpret_cast<mem_topology*>(header.data() + topo->m_sectionOffset);

  uuid_t xclbin_id;
  uuid_copy(xclbin_id, top->m_header.uuid);
  
  int first_used_mem = 0;
  size_t maxcus = 0;
  std::for_each(layout->m_ip_data,layout->m_ip_data+layout->m_count,
                [device,xclbin_id,&maxcus](auto ip_data) mutable{
                  if (ip_data.m_type != IP_KERNEL)
                    return;
                  xclOpenContext(device,xclbin_id,maxcus++,true);
                });

  for (int i=0; i<topology->m_count; ++i) {
    if (topology->m_mem_data[i].m_used) {
      first_used_mem = i;
      break;
    }
  }

  compute_units = cus = std::min(cus, maxcus);

  run(device,jobs,secs,first_used_mem);

  for (size_t cuidx=0; cuidx<cus; ++cuidx)
    xclCloseContext(device,xclbin_id,cuidx);
  xclClose(device);

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
