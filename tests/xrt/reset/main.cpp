#include "xrt/xrt_device.h"

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <chrono>
#include <thread>

#include <signal.h>

static std::mutex mutex;
static std::condition_variable cond;
static bool reset{false};
static xclDeviceHandle handle = XRT_NULL_HANDLE;

static void
usage()
{
  std::cout << "zzz [-d <device>]\n";
}

static void
SigBusHandler(int sig)
{
  std::lock_guard<std::mutex> lk(mutex);
  std::cout  << "-> sig bus handler\n";
  reset = true;
  std::this_thread::sleep_for(std::chrono::seconds(10));
  std::cout  << "notify()\n";
  cond.notify_all();
  std::cout  << "<- sig bus handler\n";
}

static void
SigIntHandler(int sig)
{
  std::lock_guard<std::mutex> lk(mutex);
  std::cout  << "sig int handler\n";
  reset = true;
  cond.notify_all();
}

static void
install()
{
  signal(SIGBUS, SigBusHandler);
  signal(SIGINT, SigIntHandler);
}

void
run(xrt::device& device)
{
  install();
  std::unique_lock<std::mutex> lk(mutex);
  while (!reset)
    cond.wait(lk);
}

int
run(int argc, char* argv[])
{
  std::string device_index = "0";

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

    if (cur == "-d")
      device_index = arg;
    else
      throw std::runtime_error("Unknown option value " + cur + " " + arg);
  }

  auto device = xrt::device(device_index);
  handle = (xclDeviceHandle) device;

  std::unique_lock<std::mutex> lk(mutex);
  while (!reset)
    cond.wait(lk);
}

int main(int argc, char* argv[])
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
