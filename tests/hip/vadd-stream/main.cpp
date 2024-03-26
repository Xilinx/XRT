// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// Based on https://github.com/sonals/ROCmExp/blob/master/VectorAdd/main-stream.cpp

#include <cstring>
#include <algorithm>
#include <iostream>
#include <memory>
#include <thread>
#include <array>
#include <vector>

#include "hip/hip_runtime_api.h"

#include "common.h"

namespace {

static constexpr char const *kernel_filename = "kernel.co";
static constexpr char const *kernel_name = "vectoradd";

static constexpr char const *nop_kernel_filename = "nop.co";
static constexpr char const *nop_kernel_name = "mynop";

static constexpr int vector_length = 0x100000;
static constexpr int vector_size = vector_length * sizeof(float);
static constexpr int threads_per_block_x = 32;
static constexpr int repeat_loop = 5000;

void
runkernel(hipFunction_t function, hipStream_t stream, std::array<void *, 3> &args)
{
  const char *name = hipKernelNameRef(function);
  std::cout << "Running " << name << ' ' << repeat_loop << " times...\n";
  xrt_hip_test_common::hip_test_timer timer;
  const auto msmulti = static_cast<double>(xrt_hip_test_common::hip_test_timer::unit());

  const int globalr = std::strcmp(name, nop_kernel_name) ? vector_length/threads_per_block_x : 1;
  const int localr = std::strcmp(name, nop_kernel_name) ? threads_per_block_x : 1;

  for (int i = 0; i < repeat_loop; i++) {
    xrt_hip_test_common::test_hip_check(hipModuleLaunchKernel(function,
                                         globalr, 1, 1,
                                         localr, 1, 1,
                                         0, stream, args.data(), nullptr), name);
  }
  xrt_hip_test_common::test_hip_check(hipStreamSynchronize(stream));
  auto delayd = timer.stop();

  std::cout << "Throughput metrics" << std::endl;
  std::cout << '(' << repeat_loop << " loops, " << delayd << " us, "
            << (repeat_loop * msmulti)/static_cast<double>(delayd)
            << " ops/s, " << delayd/repeat_loop << " us average pipelined latency)" << std::endl;


  timer.reset();
  for (int i = 0; i < repeat_loop; i++) {
    xrt_hip_test_common::test_hip_check(hipModuleLaunchKernel(function,
                                         globalr, 1, 1,
                                         localr, 1, 1,
                                         0, stream, args.data(), nullptr), name);
    xrt_hip_test_common::test_hip_check(hipStreamSynchronize(stream));
  }

  delayd = timer.stop();

  std::cout << "Latency metrics" << std::endl;
  std::cout << '(' << repeat_loop << " loops, " << delayd << " us, "
            << (repeat_loop * msmulti)/static_cast<double>(delayd)
            << " ops/s, " << delayd/repeat_loop << " us average start-to-finish latency)" << std::endl;

}

int
mainworkerthread(hipFunction_t function, hipStream_t stream, bool validate = true) {

  std::cout << "*********************************************************************************\n";

  std::vector<float> host_a(vector_length);
  std::vector<float> host_b(vector_length);
  std::vector<float> host_c(vector_length);

  // Initialize input/output vectors
  for (int i = 0; i < vector_length; i++) {
    host_b[i] = (float)i;
    host_c[i] = (float)i * 2;
    host_a[i] = 0;
  }

  xrt_hip_test_common::hip_test_device_bo<float> device_a(vector_length);
  xrt_hip_test_common::hip_test_device_bo<float> device_b(vector_length);
  xrt_hip_test_common::hip_test_device_bo<float> device_c(vector_length);

  // Sync host buffers to device
  xrt_hip_test_common::test_hip_check(hipMemcpyWithStream(device_b.get(), host_b.data(), vector_size, hipMemcpyHostToDevice, stream));
  xrt_hip_test_common::test_hip_check(hipMemcpyWithStream(device_c.get(), host_c.data(), vector_size, hipMemcpyHostToDevice, stream));

  std::array<void *, 3> args_d = {&device_a.get(), &device_b.get(), &device_c.get()};

  std::cout << "---------------------------------------------------------------------------------\n";
  std::cout << "Run " << hipKernelNameRef(function) << ' ' << repeat_loop << " times using device resident memory" << std::endl;
  std::cout << "Host buffers: " << host_a.data() << ", "
            << host_b.data() << ", " << host_c.data() << std::endl;
  std::cout << "Device buffers: " << device_a.get() << ", "
            << device_b.get() << ", " << device_c.get() << std::endl;

  runkernel(function, stream, args_d);
  // Sync device output buffer to host
  xrt_hip_test_common::test_hip_check(hipMemcpyWithStream(host_a.data(), device_a.get(), vector_size, hipMemcpyDeviceToHost, stream));

  // Verify output and then reset it for the subsequent test
  int errors = 0;
  if (validate) {
    for (int i = 0; i < vector_length; i++) {
      if (host_a[i] != (host_b[i] + host_c[i])) {
        errors++;
        break;
      }
      host_a[i] = 0.0;
    }
  }

  if (errors)
    std::cout << "FAILED TEST" << std::endl;
  else
    std::cout << "PASSED TEST" << std::endl;

  // Register our buffer with ROCm so it is pinned and prepare for access by device
  xrt_hip_test_common::test_hip_check(hipHostRegister(host_a.data(), vector_size, hipHostRegisterDefault));
  xrt_hip_test_common::test_hip_check(hipHostRegister(host_b.data(), vector_size, hipHostRegisterDefault));
  xrt_hip_test_common::test_hip_check(hipHostRegister(host_c.data(), vector_size, hipHostRegisterDefault));

  void *tmp_a1 = nullptr;
  void *tmp_b1 = nullptr;
  void *tmp_c1 = nullptr;

  // Map the host buffer to device address space so device can access the buffers
  xrt_hip_test_common::test_hip_check(hipHostGetDevicePointer(&tmp_a1, host_a.data(), 0));
  xrt_hip_test_common::test_hip_check(hipHostGetDevicePointer(&tmp_b1, host_b.data(), 0));
  xrt_hip_test_common::test_hip_check(hipHostGetDevicePointer(&tmp_c1, host_c.data(), 0));

  std::cout << "---------------------------------------------------------------------------------\n";
  std::cout << "Run " << hipKernelNameRef(function) << ' ' << repeat_loop << " times using host resident memory" << std::endl;
  std::cout << "Device mapped host buffers: " << tmp_a1 << ", "
            << tmp_b1 << ", " << tmp_c1 << std::endl;

  std::array<void *, 3> args_h = {&tmp_a1, &tmp_b1, &tmp_c1};

  errors = 0;
  runkernel(function, stream, args_h);
  // Verify the output
  if (validate) {
    for (int i = 0; i < vector_length; i++) {
      if (host_a[i] == (host_b[i] + host_c[i]))
        continue;
      errors++;
      break;
    }
  }

  // Unmap the host buffers from device address space
  xrt_hip_test_common::test_hip_check(hipHostUnregister(host_c.data()));
  xrt_hip_test_common::test_hip_check(hipHostUnregister(host_b.data()));
  xrt_hip_test_common::test_hip_check(hipHostUnregister(host_a.data()));

  if (errors)
    std::cout << "FAILED TEST" << std::endl;
  else
    std::cout << "PASSED TEST" << std::endl;

  return errors;
}

int mainworker() {
  xrt_hip_test_common::hip_test_device hdevice;
    hdevice.show_info(std::cout);

    hipFunction_t vaddfunction = hdevice.get_function(kernel_filename, kernel_name);
    hipFunction_t nopfunction = hdevice.get_function(nop_kernel_filename, nop_kernel_name);

    hipStream_t vaddstream = nullptr;
    xrt_hip_test_common::test_hip_check(hipStreamCreateWithFlags(&vaddstream, hipStreamNonBlocking));

    hipStream_t nopstream = nullptr;
    xrt_hip_test_common::test_hip_check(hipStreamCreateWithFlags(&nopstream, hipStreamNonBlocking));

    std::thread vaddthread = std::thread(mainworkerthread, vaddfunction, vaddstream, true);
    std::thread nopthread = std::thread(mainworkerthread, nopfunction, nopstream, false);

    vaddthread.join();
    nopthread.join();

    xrt_hip_test_common::test_hip_check(hipStreamDestroy(nopstream));
    xrt_hip_test_common::test_hip_check(hipStreamDestroy(vaddstream));

//    mainworkerthread(vaddfunction, vaddstream, true);
//    mainworkerthread(nopfunction, nopstream, false);
    return 0;
}
}


int
main()
{
  try {
    mainworker();

  } catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
