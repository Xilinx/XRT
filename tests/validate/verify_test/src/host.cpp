/**
* Copyright (C) 2020-2021 Xilinx, Inc
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
#define LENGTH 64

int main(int argc, char** argv) {
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
    parser.addSwitch("--kernel", "-k", "kernel name", "verify");
    parser.parse(argc, argv);

    // Read settings
    std::string dev_id = parser.value("device");
    std::string krnl_name = parser.value("kernel");

    std::string test_path = argv[1];

    std::string b_file = "/verify.xclbin";
    std::string binaryFile = test_path + b_file;
    std::ifstream infile(binaryFile);
    if (!infile.good()) {
        std::cout << "\nNOT SUPPORTED" << std::endl;
        return EOPNOTSUPP;
    }
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
    OCL_CHECK(err, q = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err));
    std::cout << "Trying to program device " << device.getInfo<CL_DEVICE_NAME>() << std::endl;
    cl::Program program(context, {device}, bins, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cout << "Failed to program device with xclbin file!\n";
    } else {
        std::cout << "Device program successful!\n";
        OCL_CHECK(err, krnl_verify = cl::Kernel(program, krnl_name.c_str(), &err));
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
    std::cout << "TEST PASSED\n";
    return 0;
}
