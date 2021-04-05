/**
 * Copyright (C) 2018-2020 Xilinx, Inc
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
#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <chrono>

#include "ert.h"

#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

namespace {

static const std::map<ert_cmd_state, std::string> ert_cmd_codes = {
    std::pair<ert_cmd_state, std::string>(ERT_CMD_STATE_NEW, "ERT_CMD_STATE_NEW"),
    std::pair<ert_cmd_state, std::string>(ERT_CMD_STATE_QUEUED, "ERT_CMD_STATE_QUEUED"),
    std::pair<ert_cmd_state, std::string>(ERT_CMD_STATE_RUNNING, "ERT_CMD_STATE_RUNNING"),
    std::pair<ert_cmd_state, std::string>(ERT_CMD_STATE_COMPLETED, "ERT_CMD_STATE_COMPLETED"),
    std::pair<ert_cmd_state, std::string>(ERT_CMD_STATE_ERROR, "ERT_CMD_STATE_ERROR"),
    std::pair<ert_cmd_state, std::string>(ERT_CMD_STATE_ABORT, "ERT_CMD_STATE_ABORT"),
};

/**
 * @return
 *   nanoseconds since first call
 */
static unsigned long
time_ns()
{
  static auto zero = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto integral_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(now-zero).count();
  return integral_duration;
}

/**
 * Simple time guard to accumulate scoped time
 */
class time_guard
{
  unsigned long zero = 0;
  unsigned long& tally;
public:
  time_guard(unsigned long& t)
    : zero(time_ns()), tally(t)
  {}

  ~time_guard()
  {
    tally += time_ns() - zero;
  }
};

static inline std::ostream&
stamp(std::ostream& os) {

  const auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::string st(std::ctime(&timenow));
  st.pop_back();
  os << '[' << getpid() << "] (" << st << "): ";
  return os;
}

int run_children(int argc, char *argv[], char *envp[], unsigned count)
{
    const char *path = argv[0];
    char buf[8];
    buf[0] = '\0';
    argv[0] = buf;
    pid_t pids[count];
    int result = 0;
    int wpid = 0;
    int wstatus = 0;
    for (unsigned i=0; i<count; i++)
        result += posix_spawn(&pids[i], path, 0, 0, argv, envp);

    while ((wpid = wait(&wstatus)) > 0);
    return result;
}

}


/**
 * Testcase to demostrate XRT's multiprocess support.
 * Runs multiple processes each exercising the same shared hello world kernel in loop
 */

static const unsigned LOOP = 16;
static const unsigned CHILDREN = 8;

////////////////////////////////////////////////////////////////////////////////

int runChildren(int argc, char *argv[], char *envp[], unsigned count);

static const char gold[] = "Hello World\n";

static void usage()
{
    std::cout << "usage: %s [options] -k <bitstream>\n\n";
    std::cout << "  -k <bitstream>\n";
    std::cout << "  -d <index>\n";
    std::cout << "  -r <num of runs, default is 16>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n\n";
    std::cout << "* Bitstream is required\n";
}

static void
run(const xrt::device& device, const xrt::uuid& uuid, size_t n_runs, bool verbose)
{
  const size_t size = 1024;
  auto kernel = xrt::kernel(device, uuid.get(), "hello");

  std::vector<xrt::bo> bos;
  std::vector<xrt::run> runs;

  for (size_t i=0; i<n_runs; ++i) {
    auto bo = xrt::bo(device, size, kernel.group_id(0));
    auto bo_data = bo.map<char*>();
    std::fill(bo_data, bo_data + size, 0);
    bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, size, 0);
    bos.push_back(std::move(bo));
  }

  for (size_t i=0; i<n_runs; ++i) {
    auto run = kernel(bos[i]);
    runs.push_back(std::move(run));
    stamp(std::cout) << "Submit execute(" << i << ")" << std::endl;
  }
  
  size_t count = runs.size();
  auto start = time_ns();
  
  while (count && (time_ns() - start) * 10e-9 < 30) {
    int run_idx = 0;
    for (auto& run : runs) {
      auto state = run.wait(1000);
      switch (state) {
      case ERT_CMD_STATE_COMPLETED:
      case ERT_CMD_STATE_ERROR:
      case ERT_CMD_STATE_ABORT:
        stamp(std::cout) << "Done execute(" << run_idx << ") "
                         << ert_cmd_codes.find(static_cast<ert_cmd_state>(state))->second << std::endl;
        --count;
        break;
      default:
        break;
      }
      ++run_idx;
    }
  }

  stamp(std::cout) << "wait time in (" << (time_ns() - start) * 10e-6 << "ms)\n";

  if (count)
    throw std::runtime_error("Could not finish all kernel runs in 30 secs");
}


int
run(int argc, char** argv, char *envp[])
{
  if (argc < 3) {
    usage();
    return 1;
  }

  std::string xclbin_fnm;
  bool verbose = false;
  unsigned int device_index = 0;
  unsigned int num_runs = LOOP;
  unsigned int child = CHILDREN;

  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
    }
    else if (arg == "-v") {
      verbose = true;
      continue;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "-d")
      device_index = std::stoi(arg);
    else if (cur == "-r")
      num_runs = std::stoi(arg);
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  if (xclbin_fnm.empty())
    throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin_fnm);

  if (std::strlen(argv[0]))
    return run_children(argc, argv, envp, child);

  run(device, uuid, num_runs, verbose);

  stamp(std::cout) << "PASSED TEST\n";
  return 0;
}

int
main(int argc, char** argv, char *envp[])
{
  try {
    auto ret = run(argc, argv, envp);
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
