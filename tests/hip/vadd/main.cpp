// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// Based on https://github.com/ROCm-Developer-Tools/HIP-Examples/tree/master/vectorAdd

#include <cstring>
#include <algorithm>
#include <iostream>
#include <memory>

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
runkernel(hipFunction_t function, void *args[])
{
  const char *name = hipKernelNameRef(function);
  std::cout << "Running " << name << ' ' << repeat_loop << " times...\n";
  hip_test_timer timer;

  const int globalr = std::strcmp(name, nop_kernel_name) ? vector_length/threads_per_block_x : 1;
  const int localr = std::strcmp(name, nop_kernel_name) ? threads_per_block_x : 1;

  for (int i = 0; i < repeat_loop; i++) {
    test_hip_check(hipModuleLaunchKernel(function,
                                         globalr, 1, 1,
                                         localr, 1, 1,
                                         0, 0, args, nullptr), name);
  }
  test_hip_check(hipDeviceSynchronize());
  auto delayd = timer.stop();

  std::cout << "Throughput metrics" << std::endl;
  std::cout << '(' << repeat_loop << " loops, " << delayd << " us, " << (repeat_loop * 1000000.0)/delayd
            << " ops/s, " << delayd/repeat_loop << " us average pipelined latency)" << std::endl;


  timer.reset();
  for (int i = 0; i < repeat_loop; i++) {
    test_hip_check(hipModuleLaunchKernel(function,
                                         globalr, 1, 1,
                                         localr, 1, 1,
                                         0, 0, args, nullptr), name);
    test_hip_check(hipDeviceSynchronize());
  }

  delayd = timer.stop();

  std::cout << "Latency metrics" << std::endl;
  std::cout << '(' << repeat_loop << " loops, " << delayd << " us, " << (repeat_loop * 1000000.0)/delayd
            << " ops/s, " << delayd/repeat_loop << " us average start-to-finish latency)" << std::endl;

}

int
mainworker() {

  std::cout << "---------------------------------------------------------------------------------\n";
  hip_test_device hdevice;
  hdevice.show_info(std::cout);

  hipFunction_t function = hdevice.get_function(kernel_filename, kernel_name);
  hipFunction_t nopfunction = hdevice.get_function(nop_kernel_filename, nop_kernel_name);

  std::unique_ptr<float[]> host_a(new float[vector_length]);
  std::unique_ptr<float[]> host_b(new float[vector_length]);
  std::unique_ptr<float[]> host_c(new float[vector_length]);

  // Initialize input/output vectors
  for (int i = 0; i < vector_length; i++) {
    host_b[i] = i;
    host_c[i] = i * 2;
    host_a[i] = 0;
  }

  hip_test_device_bo<float> device_a(vector_length);
  hip_test_device_bo<float> device_b(vector_length);
  hip_test_device_bo<float> device_c(vector_length);

  // Sync host buffers to device
  test_hip_check(hipMemcpy(device_b.get(), host_b.get(), vector_size, hipMemcpyHostToDevice));
  test_hip_check(hipMemcpy(device_c.get(), host_c.get(), vector_size, hipMemcpyHostToDevice));

  void *args_d[] = {&device_a.get(), &device_b.get(), &device_c.get()};

  std::cout << "---------------------------------------------------------------------------------\n";
  std::cout << "Run " << hipKernelNameRef(function) << ' ' << repeat_loop << " times using device resident memory" << std::endl;
  std::cout << "Host buffers: " << host_a.get() << ", "
            << host_b.get() << ", " << host_c.get() << std::endl;
  std::cout << "Device buffers: " << device_a.get() << ", "
            << device_b.get() << ", " << device_c.get() << std::endl;

  runkernel(function, args_d);
  // Sync device output buffer to host
  test_hip_check(hipMemcpy(host_a.get(), device_a.get(), vector_size, hipMemcpyDeviceToHost));

  // Verify output and then reset it for the subsequent test
  int errors = 0;
  for (int i = 0; i < vector_length; i++) {
    if (host_a[i] != (host_b[i] + host_c[i])) {
      errors++;
      break;
    }
    host_a[i] = 0.0;
  }

  if (errors)
    std::cout << "FAILED" << std::endl;
  else
    std::cout << "PASSED" << std::endl;

  std::cout << "---------------------------------------------------------------------------------\n";

  std::cout << "Run " << hipKernelNameRef(nopfunction) << ' ' << repeat_loop << " times using device resident memory" << std::endl;
  std::cout << "Host buffers: " << host_a.get() << ", "
            << host_b.get() << ", " << host_c.get() << std::endl;
  std::cout << "Device buffers: " << device_a.get() << ", "
            << device_b.get() << ", " << device_c.get() << std::endl;

  runkernel(nopfunction, args_d);

  // Register our buffer with ROCm so it is pinned and prepare for access by device
  test_hip_check(hipHostRegister(host_a.get(), vector_size, hipHostRegisterDefault));
  test_hip_check(hipHostRegister(host_b.get(), vector_size, hipHostRegisterDefault));
  test_hip_check(hipHostRegister(host_c.get(), vector_size, hipHostRegisterDefault));

  void *tmp_a1 = nullptr;
  void *tmp_b1 = nullptr;
  void *tmp_c1 = nullptr;

  // Map the host buffer to device address space so device can access the buffers
  test_hip_check(hipHostGetDevicePointer(&tmp_a1, host_a.get(), 0));
  test_hip_check(hipHostGetDevicePointer(&tmp_b1, host_b.get(), 0));
  test_hip_check(hipHostGetDevicePointer(&tmp_c1, host_c.get(), 0));

  std::cout << "---------------------------------------------------------------------------------\n";
  std::cout << "Run " << hipKernelNameRef(function) << ' ' << repeat_loop << " times using host resident memory" << std::endl;
  std::cout << "Device mapped host buffers: " << tmp_a1 << ", "
            << tmp_b1 << ", " << tmp_c1 << std::endl;

  void *args_h[] = {&tmp_a1, &tmp_b1, &tmp_c1};

  runkernel(function, args_h);
  // Verify the output
  for (int i = 0; i < vector_length; i++) {
    if (host_a[i] == (host_b[i] + host_c[i]))
      continue;
    errors++;
    break;
  }

  std::cout << "---------------------------------------------------------------------------------\n";
  std::cout << "Run " << hipKernelNameRef(nopfunction) << ' ' << repeat_loop << " times using host resident memory" << std::endl;
  std::cout << "Device mapped host buffers: " << tmp_a1 << ", "
            << tmp_b1 << ", " << tmp_c1 << std::endl;

  runkernel(nopfunction, args_h);

  // Unmap the host buffers from device address space
  test_hip_check(hipHostUnregister(host_c.get()));
  test_hip_check(hipHostUnregister(host_b.get()));
  test_hip_check(hipHostUnregister(host_a.get()));

  if (errors)
    std::cout << "FAILED" << std::endl;
  else
    std::cout << "PASSED" << std::endl;


  return errors;
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
