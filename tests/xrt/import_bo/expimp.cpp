#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "xrt/xrt_bo.h"
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"

// Test export / import of XRT buffer object between device objects
// within same process.
//
// In the test, the parent device writes "parent" to the BO. The
// child device waits for the host buffer to contain the parent
// string and writes "child" and program terminates after parent sees
// the child string.
//
// % g++ -g -std=c++17 -I${XILINX_XRT}/include -L${XILINX_XRT}/lib -o expimp.exe expimp.cpp -lxrt_coreutil -pthread -luuid
//
// # kernel allocates host buffer
// % expimp.exe -k verify.xclbin
//
// # userspace allocates host buffer (fails in xrt-2.12.x)
// % expimpl.exe -k verify.xclbin --ubuf

// Golden pattern from kernel
static constexpr char gold[] = "Hello World\n";

class fd
{
  xclBufferExportHandle m_handle;
  std::mutex m_mutex;
  std::condition_variable m_cv;
  bool m_ready = false;

public:
  void
  set(xclBufferExportHandle handle)
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handle = handle;
    m_ready = true;
    m_cv.notify_one();
  }

  xclBufferExportHandle
  get()
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_ready; });
    return m_handle;
  }
};

// Exchange file descriptor between parent and child
static fd talk;

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
child(const std::string& device_id)
{
  auto fd = talk.get();
  xrt::device device{device_id};
  xrt::bo bo{device, fd};

  try {
    auto bo_data = bo.map<char*>();
    // wait at most 5 seconds for expected buffer content
    // parent has started kernel
    int count = 5;
    while (!std::equal(std::begin(gold), std::end(gold), bo_data) && --count) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    }

    std::cout << "child reads: " << bo_data << '\n';
    if (!count)
      std::cout << "child times out\n";
    else
      bo.write("child");
  }
  catch (const std::exception& ex) {
    std::cout << "child fails with: " << ex.what() << "\n";
  }
}

static void
parent(const std::string& device_id, const std::string& xclbin_fnm, xrt::bo::flags flags)
{
  xrt::device device{device_id};
  auto uuid = device.load_xclbin(xclbin_fnm);
  xrt::kernel hello(device, uuid, "hello");
  xrt::bo bo(device, 1024*4, flags, hello.group_id(0)); // 1K buffer is somewhat arbitrary
  auto bo_data = bo.map<char*>();

  // clear device data
  std::fill(bo_data, bo_data + bo.size(), 0);
  bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);

  auto export_handle = bo.export_buffer();
  talk.set(export_handle);

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

  std::thread tc([=] { child(device_id); });
  std::thread tp([=] { parent(device_id, xclbin_fnm, flags); });

  tc.join();
  tp.join();
  
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
