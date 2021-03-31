#include <iostream>
#include <iomanip>
#include <vector>
#include <chrono>

#include "xrt/xrt_device.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"

void usage()
{
  std::cout  << "Usage: test -k <xclbin>\n";
}

double runTest(std::vector<xrt::run>& cmds, unsigned int total)
{
  int i = 0;
  unsigned int issued = 0, completed = 0;
  auto start = std::chrono::high_resolution_clock::now();

  for (auto& cmd : cmds) {
    cmd.start();
    if (++issued == total)
      break;
  }

  while (completed < total) {
    cmds[i].wait();
        
    completed++;
    if (issued < total) {
      cmds[i].start();
      issued++;
    }

    if (++i == cmds.size())
      i = 0;
  }

  auto end = std::chrono::high_resolution_clock::now();
  return (std::chrono::duration_cast<std::chrono::microseconds>(end - start)).count();
}

int testSingleThread(const xrt::device& device, const xrt::uuid& uuid)
{
  /* The command would incease */
  std::vector<unsigned int> cmds_per_run = { 10,50,100,200,500,1000,1500,2000,3000,5000,10000,50000,100000,500000,1000000 };
  int expected_cmds = 10000;

  auto hello = xrt::kernel(device, uuid.get(), "hello");

  /* Create 'expected_cmds' commands if possible */
  std::vector<xrt::run> cmds;
  for (int i = 0; i < expected_cmds; i++) {
    auto run = xrt::run(hello);
    run.set_arg(0, xrt::bo(device, 20, hello.group_id(0)));
    cmds.push_back(std::move(run));
  }
  std::cout << "Allocated commands, expect " << expected_cmds << ", created " << cmds.size() << std::endl;

  for (auto num_cmds : cmds_per_run) {
    double duration = runTest(cmds, num_cmds);
    std::cout << "Commands: " << std::setw(7) << num_cmds
              << " iops: " << (num_cmds * 1000.0 * 1000.0 / duration)
              << std::endl;
  }

  return 0;
}

int _main(int argc, char* argv[])
{
  if (argc < 3 || argv[1] != std::string("-k")) {
    usage();
    return 1;
  }

  std::string xclbin_fn = argv[2];

  auto device = xrt::device(0);
  auto uuid = device.load_xclbin(xclbin_fn);

  testSingleThread(device, uuid);

  return 0;
}

int main(int argc, char *argv[])
{
  try {
    _main(argc, argv);
    return 0;
  }
  catch (const std::exception& ex) {
    std::cout << "TEST FAILED: " << ex.what() << std::endl;
  }
  catch (...) {
    std::cout << "TEST FAILED" << std::endl;
  }

  return 1;
};
