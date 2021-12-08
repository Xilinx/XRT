#include <unistd.h>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

// Test export / import of XRT buffer object between processes.
//
// Test requires pidfd Linux kernel support and is supposed only on
// x86.  The example has been tested on Ubuntu 21.10
//
// % uname -r
// 5.13.0-22-generic
//
// Also, note that the importing process must have permission to
// duplicate the exporting process' file descriptor.  This permission
// is controlled by ptrace access mode PTRACE_MODE_ATTACH_REALCREDS
// check (see ptrace(2)).  Alternatively, run the example as root.
//
// In the test, the parent process writes "parent" to the BO. The
// child child process waits for the host buffer to contain the parent
// string and writes "child" and program terminates after parent sees
// the child string.
//
// The program allocates the buffer in memory bank 0, so make sure a
// corresponding xclbin is loaded, e.g. bandwidth.xclbin
//
// % g++ -g -std=c++14 -I${XILINX_XRT}/include -L${XILINX_XRT}/lib -o import.exe import.cpp -lxrt_coreutil -pthread -luuid
//
// # kernel allocates host buffer
// % import.exe -k verify.xclbin
//
// # userspace allocates host buffer (fails in xrt-2.12.x)
// % import.exe -k verify.xclbin --ubuf

// Golden pattern from kernel
static constexpr char gold[] = "Hello World\n";

// A pipe to communicate exported bo handle to child
static int talk[2];

static void
usage()
{
  std::cout << "usage: %s [options] \n\n"
            << "  -k <bitstream>\n"
            << "  -d <bdf | device_index>\n"
            << ""
            << "  [--ubuf]: BO host backing should be created in user space (default kernel space)\n";
}


static void
child(const std::string& device_id, pid_t pid)
{
  close(talk[1]);
  int fd = 0;
  read(talk[0], &fd, sizeof(fd));
  close(talk[0]);

  std::cout << "child pid: " << pid << '\n';
  std::cout << "child fd: " << fd << '\n';
  
  xrt::device device{device_id};
  xrt::bo bo{device, pid, fd};

  try {
    auto bo_data = bo.map<char*>();
    int count = 5;
    while (!std::equal(std::begin(gold), std::end(gold), bo_data) && --count)
      bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

    std::cout << "child reads: " << bo_data << '\n';
    if (!count)
      std::cout << "child times out\n";
    else
      bo.write("child");
    bo.write("child");
  }
  catch (const std::exception& ex) {
    std::cout << "child fails with: " << ex.what() << "\n";
  }
}

static void
parent(const std::string& device_id, const std::string& xclbin_fnm, xrt::bo::flags flags)
{
  close(talk[0]);
  
  xrt::device device{device_id};
  auto uuid = device.load_xclbin(xclbin_fnm);
  xrt::kernel hello(device, uuid, "hello");
  xrt::bo bo(device, 1024, flags, hello.group_id(0));
  auto bo_data = bo.map<char*>();

  // clear device data
  std::fill(bo_data, bo_data + 1024, 0);
  bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  auto export_handle = bo.export_buffer();
  write(talk[1], &export_handle, sizeof(export_handle));
  close(talk[1]);

  // run kernel, child will wait for golden string
  // the write "child" to buffer
  auto run = hello(bo);
  run.wait();

  // wait for child to write to buffer
  int count = 5;
  while (strncmp(bo_data, "child", bo.size()) && --count)
    std::this_thread::sleep_for(std::chrono::seconds(1));

  if (!count)
    std::cout << "parent times out\n";
  else
    std::cout << "parent reads: " << bo_data << '\n';
}

static int
run(int argc, char* argv[])
{
  std::vector<std::string> args(argv+1,argv+argc);
  std::string xclbin_fnm;
  std::string device_id = "0";
  xrt::bo::flags flags = xrt::bo::flags::cacheable;
  
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return 1;
    }

    if (arg[0] == '-') {
      cur = arg;

      if (cur == "--ubuf")
        flags = xrt::bo::flags::normal;

      continue;
    }

    if (cur == "-d")
      device_id = arg;
    else if (cur == "-k")
      xclbin_fnm = arg;
    else
      throw std::runtime_error("bad argument '" + cur + " " + arg + "'");
  }

  pipe(talk);
  
  switch (fork()) {
  case 0:
    child(device_id, getppid());
    break;
  case -1:
    throw std::runtime_error("error forking process");
    break;
  default:
    parent(device_id, xclbin_fnm, flags);
    break;
  }
  return 0;
}

int
main(int argc, char* argv[])
{
  try {
    return run(argc, argv);
  }
  catch (const std::exception& ex) {
    std::cout << "TEST FAILED: " << ex.what() << '\n';
  }
  catch (...) {
    std::cout << "TEST FAILED\n";
  }

  return 1;
}
