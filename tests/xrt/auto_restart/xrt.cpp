#include "experimental/xrt_kernel.h"
#include "experimental/xrt_bo.h"
#include "experimental/xrt_device.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

static const char gold[] = "Hello World\n";

static void usage()
{
  std::cout << "usage: %s [options] \n\n";
  std::cout << "  -k <bitstream>\n";
  std::cout << "  -d <device_index>\n";
  std::cout << "  -i <iterations>\n";
  std::cout << "";
  std::cout << "  [--iterations <number>]: number of time to iterate kernel\n";
}
  

static std::string
get_kernel_name()
{
  return "hello";
}

static void
verify(const char* data)
{
  std::cout << "RESULT: " << std::endl;
  for (unsigned i = 0; i < 20; ++i)
    std::cout << data[i];
  std::cout << std::endl;
  if (!std::equal(std::begin(gold), std::end(gold), data))
    throw std::runtime_error("Incorrect value obtained");
}

static void
verify(xrt::bo& bo)
{
  std::cout << "Get the output data from the device" << std::endl;
  bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE, 1024, 0);
  verify(bo.map<char*>());
}

xrt::bo
create_bo_at_index(const xrt::device& device, const xrt::kernel& hello, int argidx)
{
  auto bo = xrt::bo(device, 1024, hello.group_id(argidx));
  auto bo_data = bo.map<char*>();
  std::fill(bo_data, bo_data + 1024, 0);
  bo.sync(XCL_BO_SYNC_BO_TO_DEVICE, 1024,0);
  return bo;
}


static void
run(const xrt::device& device, xrt::kernel hello, unsigned int iterations)
{
  // Create argument BO
  auto bo0 = create_bo_at_index(device, hello, 0);

  // Start kernel iterating for specified iterations
  auto run = hello(xrt::autostart{iterations}, bo0);

  // Create another BO to replace currently set arg at index 0
  auto bo1 = create_bo_at_index(device, hello, 0);

  // Create a run update object
  xrt::mailbox mailbox{run};

  // Update the software copy the argument at index 0
  mailbox.set_arg(0, bo1);

  // Safely sync the update argument to the next kernel invocation
  // This API pauses the running kernel, sets the argument, and
  // restarts the kernel
  mailbox.write();

  // Run for a while, then stop the kernel
  std::this_thread::sleep_for(std::chrono::seconds(5));
  run.stop();  // synchronous 
  run.wait();  // redundant

  // Verify both BOs have the proper pattern
  verify(bo0);
  verify(bo1);
}

static void
run(int argc, char* argv[])
{
  std::vector<std::string> args(argv+1,argv+argc);

  std::string xclbin_fnm;
  unsigned int device_index = 0;
  unsigned int iterations = 1;

  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      usage();
      return;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-d")
      device_index = std::stoi(arg);
    else if (cur == "-k")
      xclbin_fnm = arg;
    else if (cur == "-i")
      iterations = std::stoi(arg);
    else if (cur == "--iterations")
      iterations = std::stoi(arg);
    else
      throw std::runtime_error("bad argument '" + cur + " " + arg + "'");
  }

  auto device = xrt::device(device_index);
  auto uuid = device.load_xclbin(xclbin_fnm);

  std::string kname = get_kernel_name();
  auto kernel = xrt::kernel(device, uuid, kname);

  run(device,kernel,iterations);

  return;
}

} // namespace

int main(int argc, char* argv[])
{
  try {
    run(argc, argv);
    return 0;
  }
  catch (const std::exception& ex) {
    std::cout << "TEST FAILED: " << ex.what() << "\n";
  }
  return 1;
}
