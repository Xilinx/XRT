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
#include "cmdlineparser.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <math.h>
#include <sys/time.h>
#include <xcl2.hpp>

int main(int argc, char **argv) {
  if (argc < 2) {
    std::cout << "Usage: " << argv[0] << " <Platform Test Area Path>"
              << "<optional> -d device_id" << std::endl;
    return EXIT_FAILURE;
  }

  // Command Line Parser
  sda::utils::CmdLineParser parser;

  // Switches
  //**************//"<Full Arg>",  "<Short Arg>", "<Description>", "<Default>"
  parser.addSwitch("--device", "-d", "device id", "0");
  parser.parse(argc, argv);

  // Read settings
  unsigned int dev_id = stoi(parser.value("device"));

  int NUM_KERNEL;
  std::string test_path = argv[1];
  std::string filename = "/platform.json";
  std::string platform_json = test_path + filename;

  try {
    boost::property_tree::ptree loadPtreeRoot;
    boost::property_tree::read_json(platform_json, loadPtreeRoot);
    boost::property_tree::ptree temp;

    temp = loadPtreeRoot.get_child("total_host_banks");
    NUM_KERNEL = temp.get_value<int>();

  } catch (const std::exception &e) {
    std::string msg(
        "ERROR: Bad JSON format detected while marshaling build metadata (");
    msg += e.what();
    msg += ").";
    std::cout << msg;
  }

  std::string b_file = "/slavebridge.xclbin";
  std::string binaryFile = test_path + b_file;
  std::ifstream infile(binaryFile);
  if (!infile.good()) {
    std::cout << "\nNOT SUPPORTED" << std::endl;
    return EOPNOTSUPP;
  }

  cl_int err;
  cl::Context context;
  std::string krnl_name = "slavebridge";
  std::vector<cl::Kernel> krnls(NUM_KERNEL);
  cl::CommandQueue q;

  // OPENCL HOST CODE AREA START
  // get_xil_devices() is a utility API which will find the xilinx
  // platforms and will return list of devices connected to Xilinx platform
  auto devices = xcl::get_xil_devices();
  // read_binary_file() is a utility API which will load the binaryFile
  // and will return the pointer to file buffer.
  auto fileBuf = xcl::read_binary_file(binaryFile);
  cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};
  if (dev_id >= devices.size()) {
    std::cout << "The device_id provided using -d flag is outside the range of "
                 "available devices\n";
    return EXIT_FAILURE;
  }
  auto device = devices[dev_id];
  // Creating Context and Command Queue for selected Device
  OCL_CHECK(err,
            context = cl::Context(device, nullptr, nullptr, nullptr, &err));
  OCL_CHECK(err,
            q = cl::CommandQueue(context, device,
                                 CL_QUEUE_PROFILING_ENABLE |
                                     CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                 &err));
  std::cout << "Trying to program device[" << dev_id
            << "]: " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
  cl::Program program(context, {device}, bins, nullptr, &err);
  if (err != CL_SUCCESS) {
    std::cout << "Failed to program device[" << dev_id
              << "] with xclbin file!\n";
  } else {
    std::cout << "Device[" << dev_id << "]: program successful!\n";
    for (int i = 0; i < NUM_KERNEL; i++) {
      std::string cu_id = std::to_string(i + 1);
      std::string krnl_name_full =
          krnl_name + ":{" + "slavebridge_" + cu_id + "}";

      printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(),
             i + 1);

      // Here Kernel object is created by specifying kernel name along with
      // compute unit.
      // For such case, this kernel object can only access the specific
      // Compute unit

      OCL_CHECK(err,
                krnls[i] = cl::Kernel(program, krnl_name_full.c_str(), &err));
    }
  }

  double max_throughput = 0;
  for (uint32_t i = 4 * 1024; i <= 16 * 1024 * 1024; i *= 2) {
    unsigned int reps = 1000;
    unsigned int DATA_SIZE = i;

    if (xcl::is_emulation()) {
      reps = 2;
      if (DATA_SIZE > 8 * 1024)
        break;
    }

    unsigned int vector_size_bytes = DATA_SIZE;
    std::vector<unsigned char, aligned_allocator<unsigned char>> input_host(
        DATA_SIZE);

    for (uint32_t j = 0; j < DATA_SIZE; j++) {
      input_host[j] = j % 256;
    }

    std::vector<cl::Buffer> input_buffer(NUM_KERNEL);
    std::vector<cl::Buffer> output_buffer(NUM_KERNEL);

    std::vector<cl_mem_ext_ptr_t> input_buffer_ext(NUM_KERNEL);
    std::vector<cl_mem_ext_ptr_t> output_buffer_ext(NUM_KERNEL);
    for (int i = 0; i < NUM_KERNEL; i++) {
      input_buffer_ext[i].flags = XCL_MEM_EXT_HOST_ONLY;
      input_buffer_ext[i].obj = NULL;
      input_buffer_ext[i].param = 0;

      output_buffer_ext[i].flags = XCL_MEM_EXT_HOST_ONLY;
      output_buffer_ext[i].obj = NULL;
      output_buffer_ext[i].param = 0;
    }

    for (int i = 0; i < NUM_KERNEL; i++) {
      OCL_CHECK(err, input_buffer[i] = cl::Buffer(
                         context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                         vector_size_bytes, &input_buffer_ext[i], &err));
      OCL_CHECK(err, output_buffer[i] = cl::Buffer(
                         context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                         vector_size_bytes, &output_buffer_ext[i], &err));
    }

    unsigned char *map_input_buffer[NUM_KERNEL];
    unsigned char *map_output_buffer[NUM_KERNEL];

    for (int i = 0; i < NUM_KERNEL; i++) {
      OCL_CHECK(err, err = krnls[i].setArg(0, input_buffer[i]));
      OCL_CHECK(err, err = krnls[i].setArg(1, output_buffer[i]));
      OCL_CHECK(err, err = krnls[i].setArg(2, DATA_SIZE));
      OCL_CHECK(err, err = krnls[i].setArg(3, reps));
    }

    for (int i = 0; i < NUM_KERNEL; i++) {
      OCL_CHECK(err, map_input_buffer[i] = (unsigned char *)q.enqueueMapBuffer(
                         (input_buffer[i]), CL_FALSE, CL_MAP_WRITE, 0,
                         vector_size_bytes, NULL, NULL, &err));
      OCL_CHECK(err, err = q.finish());
    }

    /* prepare data to be written to the device */
    for (int i = 0; i < NUM_KERNEL; i++) {
      for (size_t j = 0; j < vector_size_bytes; j++) {
        map_input_buffer[i][j] = input_host[j];
      }
    }
    std::chrono::high_resolution_clock::time_point timeStart;
    std::chrono::high_resolution_clock::time_point timeEnd;

    timeStart = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_KERNEL; i++) {
      OCL_CHECK(err, err = q.enqueueTask(krnls[i]));
    }
    q.finish();
    timeEnd = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_KERNEL; i++) {
      OCL_CHECK(err, map_output_buffer[i] = (unsigned char *)q.enqueueMapBuffer(
                         (output_buffer[i]), CL_FALSE, CL_MAP_READ, 0,
                         vector_size_bytes, NULL, NULL, &err));
      OCL_CHECK(err, err = q.finish());
    }

    // check
    for (int i = 0; i < NUM_KERNEL; i++) {
      for (uint32_t j = 0; j < DATA_SIZE; j++) {
        if (map_output_buffer[i][j] != map_input_buffer[i][j]) {
          printf("ERROR : kernel failed to copy entry %i input %i output %i\n",
                 j, map_input_buffer[i][j], map_output_buffer[i][j]);
          return EXIT_FAILURE;
        }
      }
    }

    double usduration;
    double dnsduration;
    double dsduration;
    double bpersec;
    double mbpersec;

    usduration = (double)(std::chrono::duration_cast<std::chrono::microseconds>(
                              timeEnd - timeStart)
                              .count() /
                          reps);

    dnsduration = (double)usduration;
    dsduration = dnsduration / ((double)1000000);
    bpersec = (DATA_SIZE * NUM_KERNEL) / dsduration;
    mbpersec =
        (2 * bpersec) / ((double)1024 * 1024); // For concurrent Read/Write

    if (mbpersec > max_throughput)
      max_throughput = mbpersec;
  }
  std::cout << "Maximum throughput: " << max_throughput << "MB/s\n";

  std::cout << "TEST PASSED\n";

  return EXIT_SUCCESS;
}

