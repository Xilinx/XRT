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
#include "xcl2.hpp"
#include <boost/filesystem.hpp>
#include <algorithm>
#include <vector>
#define LENGTH 64

static void printHelp() {
    std::cout << "usage: %s <options>\n";
    std::cout << "  -p <path>\n";
    std::cout << "  -d <device> \n";
    std::cout << "  -s <supported>\n";
    std::cout << "  -h <help>\n";
}

int main(int argc, char** argv) {
    std::string dev_id = "0";
    std::string test_path;
    std::string b_file = "/verify.xclbin";
    bool flag_s = false;
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--path") == 0)) {
            test_path = argv[i + 1];
        } else if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--device") == 0)) {
            dev_id = argv[i + 1];
        } else if ((strcmp(argv[i], "-s") == 0) || (strcmp(argv[i], "--supported") == 0)) {
            flag_s = true;
        } else if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
            printHelp();
            return 1;
        }
    }

    if (test_path.empty()) {
        std::cout << "ERROR : please provide the platform test path to -p option\n";
        return EXIT_FAILURE;
    }
    auto binaryFile = boost::filesystem::path(test_path) / b_file;
    std::ifstream infile(binaryFile.string());
    if (flag_s) {
        if (!infile.good()) {
            std::cout << "\nNOT SUPPORTED" << std::endl;
            return EOPNOTSUPP;
        } else {
            std::cout << "\nSUPPORTED" << std::endl;
            return EXIT_SUCCESS;
        }
    }

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
    auto fileBuf = xcl::read_binary_file(binaryFile.string());
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
    std::cout << "TEST PASSED\n";
    return 0;
}
