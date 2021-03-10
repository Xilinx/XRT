/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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

// g++ -g -std=c++14 -I$XILINX_XRT/include -L$XILINX_XRT/lib -I.. -o host.exe hello.cpp -lxilinxopencl

#include "hostsrc/utils.hpp"
#include "CL/cl_ext_xilinx.h"
#include "experimental/xrt++.hpp"

#define LENGTH (20)

// From HLS, must match verify.xclbin
#define XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA         0x40

namespace {

using utils::throw_if_error;

static void
help()
{
    std::cout << "usage: %s <bitstream>  [options] \n\n";
    std::cout << "  [-d <index>] : index of device to use (default: 0)\n";
    std::cout << "  [-x]         : use alternative experimental API (xrtcpp) (default: off)\n";
    std::cout << "  [-l <loops>] : loop  kernel execution loops number of times (default: 1)\n";
    std::cout << "  [-w]         : wait for each kernel execution to finish in loop iteration (default: off)\n";
    std::cout << "* Bitstream is required\n";
}

int
run(int argc, char* argv[])
{
  std::string xclbin;
  unsigned int device_index = 0;
  bool xrt = false;
  bool wait = false;
  size_t loops = 1;

  std::vector<std::string> args(argv+1,argv+argc);
  std::string cur;
  for (auto& arg : args) {
    if (arg == "-h") {
      help();
      return 1;
    }

    if (arg == "-x") {
      xrt = true;
      continue;
    }
    if (arg == "-w") {
      wait = true;
      continue;
    }

    if (arg[0] == '-') {
      cur = arg;
      continue;
    }

    if (cur == "-d")
      device_index = std::stoi(arg);
    else if (cur == "-l")
      loops = std::stoi(arg);
    else {
      xclbin = arg;
      continue;
    }
  }

  auto platform = utils::open_platform("Xilinx","Xilinx");
  auto device = utils::get_device(platform,device_index);

  cl_int err = CL_SUCCESS;
  auto context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
  throw_if_error(err,"clCreateContext failed");

  auto queue = clCreateCommandQueue(context, device, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
  throw_if_error(err || !queue,"clCreateCommandQueue failed");

  auto bitstream = utils::read_xclbin(xclbin);
  auto size = bitstream.size();
  auto data = reinterpret_cast<const unsigned char*>(bitstream.data());
  auto program = clCreateProgramWithBinary(context, 1, &device, &size, &data, nullptr, &err);
  throw_if_error(err || !program,"clCreateProgramWithBinary failed");

  auto kernel = clCreateKernel(program, "hello", &err);
  throw_if_error(err || !kernel,"clCreateKernel failed");

  auto d_buf = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(char) * LENGTH, nullptr, &err);
  throw_if_error(err || !d_buf,"clCreateKernel failed");

  if (xrt) {
    uint64_t d_buf_addr;
    throw_if_error(xclGetMemObjDeviceAddress(d_buf,device,sizeof(uint64_t),&d_buf_addr),"failed to get dbuf address");
    auto xdev = xclGetXrtDevice(device,&err);
    throw_if_error(err || !xdev,"failed to get xrt device");
    xrtcpp::acquire_cu_context(xdev,0/*cuidx*/);
    auto start = utils::time_ns();
    for (int i=0; i<loops; ++i) {
      xrtcpp::exec::exec_cu_command cmd(xdev);
      cmd.add_cu(0);
      cmd.add(XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA>>2,d_buf_addr); // low
      cmd.add((XHELLO_HELLO_CONTROL_ADDR_ACCESS1_DATA>>2)+1,(d_buf_addr >> 32) & 0xFFFFFFFF); // high part of a
      cmd.execute();
      cmd.wait();
    }
    auto end = utils::time_ns();
    std::cout << "total (ms): " << (end-start)*1e-6 << "\n";
    xrtcpp::release_cu_context(xdev,0/*cuidx*/);
  }
  else {
    auto start = utils::time_ns();
    for (int i=0; i<loops; ++i) {
      throw_if_error(clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_buf),"clSetKenelArg failed");
      throw_if_error(clEnqueueTask(queue, kernel, 0, nullptr, nullptr),"clEnqueueTask failed");
      if (wait)
        clFinish(queue);
    }
    clFinish(queue);
    auto end = utils::time_ns();
    std::cout << "total (ms): " << (end-start)*1e-6 << "\n";
  }


  char h_buf[LENGTH] = {0};
  throw_if_error(clEnqueueReadBuffer(queue, d_buf, CL_TRUE, 0, sizeof(char) * LENGTH, h_buf, 0, nullptr, nullptr),"clEnqueueReadBuffer failed");

  std::cout << "RESULT: " << h_buf << "\n";

  clReleaseMemObject(d_buf);
  clReleaseProgram(program);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(queue);
  clReleaseContext(context);
  clReleaseDevice(device);

  return 0;
}

}

int main(int argc, char* argv[])
{
  try {
    auto ret = run(argc,argv);
    std::cout << "SUCCESS\n";
    return ret;
  }
  catch (const std::exception& ex) {
    std::cout << "FAIL: " << ex.what() << "\n";
    return 1;
  }
  catch (...) {
    std::cout << "FAIL\n";
    return 1;
  }
}
