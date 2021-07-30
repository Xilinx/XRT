/**
* Copyright (C) 2019-2021 Xilinx, Inc
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
#include "xcl2.hpp"
#include <algorithm>
#include <vector>
#include <math.h>
#include <sys/time.h>
#define LENGTH 64

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: -x xclbin_file"
                  << "<optional> -d device_id"
                  << "<optional> -l iter_cnt" << std::endl;
        return EXIT_FAILURE;
    }

    // Command Line Parser
    sda::utils::CmdLineParser parser;

    // Switches
    //**************//"<Full Arg>",  "<Short Arg>", "<Description>", "<Default>"
    parser.addSwitch("--xclbin_file", "-x", "input binary file string", "");
    parser.addSwitch("--device", "-d", "device id", "0");
    parser.addSwitch("--iter_cnt", "-l", "loop iteration count", "10000");
    parser.parse(argc, argv);

    // Read settings
    std::string dev_id = parser.value("device");
    std::string iter_cnt = parser.value("iter_cnt");
    std::string binaryFile = parser.value("xclbin_file");

    std::cout << "\nStarting the Verify test....\n";
    cl_int err;
    cl::Context context;
    cl::Kernel krnl_verify;
    cl::CommandQueue q;

    std::vector<char, aligned_allocator<char> > h_buf(LENGTH);

    // Create the test data
    for (int i = 0; i < LENGTH; i++) {
        h_buf[i] = 0;
    }

    // OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the xilinx
    // platforms and will return list of devices connected to Xilinx platform
    auto devices = xcl::get_xil_devices();
    std::vector<cl::Platform> platforms;
    OCL_CHECK(err, err = cl::Platform::get(&platforms));
    std::string platform_vers(1024, '\0'), platform_prof(1024, '\0'), platform_exts(1024, '\0');
    OCL_CHECK(err, err = platforms[0].getInfo(CL_PLATFORM_VERSION, &platform_vers));
    OCL_CHECK(err, err = platforms[0].getInfo(CL_PLATFORM_PROFILE, &platform_prof));
    OCL_CHECK(err, err = platforms[0].getInfo(CL_PLATFORM_EXTENSIONS, &platform_exts));
    std::cout << "Platform Version: " << platform_vers << std::endl;
    std::cout << "Platform Profile: " << platform_prof << std::endl;
    std::cout << "Platform Extensions: " << platform_exts << std::endl;

    // read_binary_file() is a utility API which will load the binaryFile
    // and will return the pointer to file buffer.
    auto fileBuf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{fileBuf.data(), fileBuf.size()}};

    auto pos = dev_id.find(":");
    cl::Device device;
    if (pos == std::string::npos) {
        uint32_t device_index = stoi(dev_id);
        if (device_index >= devices.size()) {
            std::cout << "The device_index provided using -d flag is outside the range of "
                         "available devices\n";
            return EXIT_FAILURE;
        }
        device = devices[device_index];
    } else {
        if (xcl::is_emulation()) {
            std::cout << "Device bdf is not supported for the emulation flow\n";
            return EXIT_FAILURE;
        }
        device = xcl::find_device_bdf(devices, dev_id);
    }

    // Creating Context and Command Queue for selected Device
    OCL_CHECK(err, context = cl::Context(device, nullptr, nullptr, nullptr, &err));
    OCL_CHECK(err, q = cl::CommandQueue(context, device,
                                        CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err));
    std::cout << "Trying to program device " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cout << "Failed to program device with xclbin file!\n";
    } else {
        std::cout << "Device program successful!\n";
        OCL_CHECK(err, krnl_verify = cl::Kernel(program, "verify", &err));
    }

    OCL_CHECK(err, cl::Buffer d_buf(context, CL_MEM_WRITE_ONLY, sizeof(char) * LENGTH, nullptr, &err));

    OCL_CHECK(err, err = krnl_verify.setArg(0, d_buf));

    // Launch the Kernel
    OCL_CHECK(err, err = q.enqueueTask(krnl_verify));
    q.finish();

    OCL_CHECK(err, err = q.enqueueReadBuffer(d_buf, CL_TRUE, 0, sizeof(char) * LENGTH, h_buf.data(), nullptr, nullptr));
    q.finish();
    for (int i = 0; i < 12; i++) {
        std::cout << h_buf[i];
    }

    std::cout << "\nStarting the Bandwidth test....\n";
    int NUM_KERNEL = 4;

    std::string krnl_name = "bandwidth";
    std::vector<cl::Kernel> krnls(NUM_KERNEL);
    for (int i = 0; i < NUM_KERNEL; i++) {
        std::string cu_id = std::to_string(i + 1);
        std::string krnl_name_full = krnl_name + ":{" + "bandwidth_" + cu_id + "}";

        printf("Creating a kernel [%s] for CU(%d)\n", krnl_name_full.c_str(), i + 1);

        // Here Kernel object is created by specifying kernel name along with
        // compute unit.
        // For such case, this kernel object can only access the specific
        // Compute unit

        OCL_CHECK(err, krnls[i] = cl::Kernel(program, krnl_name_full.c_str(), &err));
    }

    double max_throughput = 0;
    int reps = stoi(iter_cnt);

    for (uint32_t i = 4 * 1024; i <= 16 * 1024 * 1024; i *= 2) {
        unsigned int DATA_SIZE = i;

        if (xcl::is_emulation()) {
            reps = 2;
            if (DATA_SIZE > 8 * 1024) break;
        }

        unsigned int vector_size_bytes = DATA_SIZE;
        std::vector<unsigned char, aligned_allocator<unsigned char> > input_host(DATA_SIZE);
        std::vector<unsigned char, aligned_allocator<unsigned char> > output_host[NUM_KERNEL];

        for (int i = 0; i < NUM_KERNEL; i++) {
            output_host[i].resize(DATA_SIZE);
        }
        for (uint32_t j = 0; j < DATA_SIZE; j++) {
            input_host[j] = j % 256;
        }

        // Initializing output vectors to zero
        for (int i = 0; i < NUM_KERNEL; i++) {
            std::fill(output_host[i].begin(), output_host[i].end(), 0);
        }

        std::vector<cl::Buffer> input_buffer(NUM_KERNEL);
        std::vector<cl::Buffer> output_buffer(NUM_KERNEL);

        // These commands will allocate memory on the FPGA. The cl::Buffer objects
        // can
        // be used to reference the memory locations on the device.
        // Creating Buffers
        for (int i = 0; i < NUM_KERNEL; i++) {
            OCL_CHECK(err, input_buffer[i] = cl::Buffer(context, CL_MEM_READ_WRITE, vector_size_bytes, nullptr, &err));
            OCL_CHECK(err, output_buffer[i] = cl::Buffer(context, CL_MEM_READ_WRITE, vector_size_bytes, nullptr, &err));
        }

        for (int i = 0; i < NUM_KERNEL; i++) {
            OCL_CHECK(err, err = krnls[i].setArg(0, input_buffer[i]));
            OCL_CHECK(err, err = krnls[i].setArg(1, output_buffer[i]));
            OCL_CHECK(err, err = krnls[i].setArg(2, DATA_SIZE));
            OCL_CHECK(err, err = krnls[i].setArg(3, reps));
        }

        for (int i = 0; i < NUM_KERNEL; i++) {
            OCL_CHECK(err, err = q.enqueueWriteBuffer(input_buffer[i], CL_TRUE, 0, vector_size_bytes, input_host.data(),
                                                      nullptr, nullptr));
            OCL_CHECK(err, err = q.finish());
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
            OCL_CHECK(err, err = q.enqueueReadBuffer(output_buffer[i], CL_TRUE, 0, vector_size_bytes,
                                                     output_host[i].data(), nullptr, nullptr));
            OCL_CHECK(err, err = q.finish());
        }

        // check
        for (int i = 0; i < NUM_KERNEL; i++) {
            for (uint32_t j = 0; j < DATA_SIZE; j++) {
                if (output_host[i][j] != input_host[j]) {
                    printf("ERROR : kernel failed to copy entry %i input %i output %i\n", j, input_host[j],
                           output_host[i][j]);
                    return EXIT_FAILURE;
                }
            }
        }

        double usduration;
        double dnsduration;
        double dsduration;
        double bpersec;
        double mbpersec;

        usduration = (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(timeEnd - timeStart).count() / reps);

        dnsduration = (double)usduration;
        dsduration = dnsduration / ((double)1000000000);
        bpersec = (DATA_SIZE * NUM_KERNEL) / dsduration;
        mbpersec = (2 * bpersec) / ((double)1024 * 1024); // For concurrent Read/Write

        if (mbpersec > max_throughput) max_throughput = mbpersec;
    }

    std::cout << "Concurrent read and write throughput: " << max_throughput << "MB/s\n";

    std::cout << "TEST PASSED\n";
    return 0;
}
