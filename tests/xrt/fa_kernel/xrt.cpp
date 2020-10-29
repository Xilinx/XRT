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

#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <cstring>
#include <ctime>
#include <chrono>
#include <numeric>

#include "experimental/xrt_device.h"
#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"

#ifdef _WIN32
# pragma warning ( disable : 4244 )
#endif

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -h\n\n";
    std::cout << "* Bitstream is required\n";
}

struct job_type
{
  static constexpr uint32_t aes_key[16] = {
    0xeb5aa3b8,
    0x17750c26,
    0x9d0db966,
    0xbcb9e3b6,
    0x510e08c6,
    0x83956e46,
    0x3bd10f72,
    0x769bf32e,
    0xfa374467,
    0x3386553a,
    0x46f91c6a,
    0x6b25d1b4,
    0x6116fa6f,
    0xd29b1a56,
    0x9c193635,
    0x10ed77d4
  };

  static constexpr uint32_t aes_iv[4] = {
    0x149f40ae,
    0x38f1817d,
    0x32ccb7db,
    0xa6ef0e05
  }; 

  static constexpr size_t len = 4096;

  xrt::run run;

  xrt::bo in;
  xrt::bo out;
  xrt::bo out_status;

  job_type(const xrt::device& device, const xrt::kernel& aes)
    : run(aes)
    , in(device, len, aes.group_id(0))
    , out(device, len, aes.group_id(2))
    , out_status(device, len, aes.group_id(4))
  {
    auto in_data = in.map<uint32_t*>();
    std::iota(in_data, in_data + len/sizeof(uint32_t), 0);
    in.sync(XCL_BO_SYNC_BO_TO_DEVICE , len, 0);
  }

  void
  start()
  {
    run(in, len, out, len, out_status, aes_key, aes_iv);
  }

  void
  wait()
  {
    run.wait();
  }

  void
  verify() const
  {
  }
};

constexpr uint32_t job_type::aes_key[16];
constexpr uint32_t job_type::aes_iv[4];
constexpr size_t job_type::len;

static double
run(std::vector<job_type>& cmds, size_t total)
{
  size_t i = 0;
  size_t issued = 0, completed = 0;
  auto start = std::chrono::high_resolution_clock::now();

  for (auto& cmd : cmds) {
    cmd.start();
    if (++issued == total)
      break;
  }

  while (completed < total) {
    cmds[i].wait();

    // cmds[i].verify()
        
    completed++;
    if (issued < total) {
      cmds[i].start();
      ++issued;
    }

    if (++i == cmds.size())
      i = 0;
  }

  auto end = std::chrono::high_resolution_clock::now();
  return (std::chrono::duration_cast<std::chrono::microseconds>(end - start)).count();
  
}

static void
run(const xrt::device& device, xrt::kernel& aes)
{
  std::vector<size_t> cmds_per_run = { 100, 1000, 10000, 100000, 1000000 };
  size_t expected_cmds = 1000;

  std::vector<job_type> jobs;
  jobs.reserve(expected_cmds);
  for (int i = 0; i < expected_cmds; ++i)
    jobs.emplace_back(device, aes);

  for (auto num_cmds : cmds_per_run) {
    auto duration = run(jobs, num_cmds);

    std::cout << "Commands: " << std::setw(7) << num_cmds
              << " iops: " << (num_cmds * 1000.0 * 1000.0 / duration)
              << std::endl;
  }
}

int
run(int argc, char** argv)
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  unsigned int device_index = 0;

  std::vector<std::string> args(argv+1,argv+argc);
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

    if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "-d")
      device_index = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  auto device = xrt::device{device_index};
  auto uuid = device.load_xclbin(xclbin_fnm);
  auto aes = xrt::kernel{device, uuid, "fa_aes_xts2_rtl_dec"};

  run(device, aes);

  return 0;
}

int main(int argc, char** argv)
{
  try {
    auto ret = run(argc, argv);
    std::cout << "PASSED TEST\n";
    return ret;
  }
  catch (std::exception const& e) {
    std::cout << "Exception: " << e.what() << "\n";
    std::cout << "FAILED TEST\n";
    return 1;
  }

  std::cout << "PASSED TEST\n";
  return 0;
}
