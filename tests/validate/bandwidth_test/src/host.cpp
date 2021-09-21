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
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <math.h>
#include <sys/time.h>
#include <xcl2.hpp>

static void printHelp() {
    std::cout << "usage: %s <options>\n";
    std::cout << "  -p <path>\n";
    std::cout << "  -d <device> \n";
    std::cout << "  -l <loop_iter_cnt> \n";
    std::cout << "  -s <supported>\n";
    std::cout << "  -h <help>\n";
}

int main(int argc, char** argv) {
    std::string dev_id = "0";
    std::string test_path;
    std::string iter_cnt = "10000";
    std::string b_file = "/bandwidth.xclbin";
    bool flag_s = false;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-p") == 0) || (strcmp(argv[i], "--path") == 0)) {
            test_path = argv[i + 1];
        } else if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--device") == 0)) {
            dev_id = argv[i + 1];
        } else if ((strcmp(argv[i], "-l") == 0) || (strcmp(argv[i], "--loop_iter_cnt") == 0)) {
            iter_cnt = argv[i + 1];
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
    std::string binaryFile = test_path + b_file;
    std::ifstream infile(binaryFile);
    if (flag_s) {
        if (!infile.good()) {
            std::cout << "\nNOT SUPPORTED" << std::endl;
            return EOPNOTSUPP;
        } else {
            std::cout << "\nSUPPORTED" << std::endl;
            return EXIT_SUCCESS;
        }
    }

    int num_kernel = 0, num_kernel_ddr = 0;
    bool chk_hbm_mem = false;
    std::string filename = "/platform.json";
    std::string platform_json = test_path + filename;

    try {
        boost::property_tree::ptree load_ptree_root;
        boost::property_tree::read_json(platform_json, load_ptree_root);

        auto temp = load_ptree_root.get_child("total_ddr_banks");
        num_kernel = temp.get_value<int>();
        num_kernel_ddr = num_kernel;
        auto pt_mem_array = load_ptree_root.get_child("meminfo");
        for (const auto& mem_entry : pt_mem_array) {
            boost::property_tree::ptree pt_mem_entry = mem_entry.second;
            auto sValue = pt_mem_entry.get<std::string>("type");
            if (sValue == "HBM") {
                chk_hbm_mem = true;
            }
        }
        if (chk_hbm_mem) {
            num_kernel_ddr = num_kernel - 1;
        }
    } catch (const std::exception& e) {
        std::string msg("ERROR: Bad JSON format detected while marshaling build metadata (");
        msg += e.what();
        msg += ").";
        std::cout << msg << std::endl;
    }

    if (!infile.good()) {
        std::cout << "\nNOT SUPPORTED" << std::endl;
        return EOPNOTSUPP;
    }

    cl_int err;
    cl::Context context;
    std::string krnl_name = "bandwidth";
    std::vector<cl::Kernel> krnls(num_kernel);
    cl::CommandQueue q;

    // OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the xilinx
    // platforms and will return list of devices connected to Xilinx platform
    auto devices = xcl::get_xil_devices();
    // read_binary_file() is a utility API which will load the binaryFile
    // and will return the pointer to file buffer.
    auto file_buf = xcl::read_binary_file(binaryFile);
    cl::Program::Binaries bins{{file_buf.data(), file_buf.size()}};

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
        for (int i = 0; i < num_kernel; i++) {
            std::string cu_id = std::to_string(i + 1);
            std::string krnl_name_full = krnl_name + ":{" + "bandwidth_" + cu_id + "}";

            // Here Kernel object is created by specifying kernel name along with
            // compute unit.
            // For such case, this kernel object can only access the specific
            // Compute unit

            OCL_CHECK(err, krnls[i] = cl::Kernel(program, krnl_name_full.c_str(), &err));
        }
    }

    double max_throughput = 0;
    int reps = stoi(iter_cnt);
    if (num_kernel_ddr) {
        for (uint32_t i = 4 * 1024; i <= 16 * 1024 * 1024; i *= 2) {
            unsigned int data_size = i;

            if (xcl::is_emulation()) {
                reps = 2;
                if (data_size > 8 * 1024) {
                    break;
                }
            }

            unsigned int vector_size_bytes = data_size;
            std::vector<unsigned char, aligned_allocator<unsigned char> > input_host(data_size);
            std::vector<unsigned char, aligned_allocator<unsigned char> > output_host[num_kernel_ddr];

            for (int i = 0; i < num_kernel_ddr; i++) {
                output_host[i].resize(data_size);
            }
            for (uint32_t j = 0; j < data_size; j++) {
                input_host[j] = j % 256;
            }

            // Initializing output vectors to zero
            for (int i = 0; i < num_kernel_ddr; i++) {
                std::fill(output_host[i].begin(), output_host[i].end(), 0);
            }

            std::vector<cl::Buffer> input_buffer(num_kernel_ddr);
            std::vector<cl::Buffer> output_buffer(num_kernel_ddr);

            // These commands will allocate memory on the FPGA. The cl::Buffer objects
            // can
            // be used to reference the memory locations on the device.
            // Creating Buffers
            for (int i = 0; i < num_kernel_ddr; i++) {
                OCL_CHECK(err,
                          input_buffer[i] = cl::Buffer(context, CL_MEM_READ_WRITE, vector_size_bytes, nullptr, &err));
                OCL_CHECK(err,
                          output_buffer[i] = cl::Buffer(context, CL_MEM_READ_WRITE, vector_size_bytes, nullptr, &err));
            }

            for (int i = 0; i < num_kernel_ddr; i++) {
                OCL_CHECK(err, err = krnls[i].setArg(0, input_buffer[i]));
                OCL_CHECK(err, err = krnls[i].setArg(1, output_buffer[i]));
                OCL_CHECK(err, err = krnls[i].setArg(2, data_size));
                OCL_CHECK(err, err = krnls[i].setArg(3, reps));
            }

            for (int i = 0; i < num_kernel_ddr; i++) {
                OCL_CHECK(err, err = q.enqueueWriteBuffer(input_buffer[i], CL_TRUE, 0, vector_size_bytes,
                                                          input_host.data(), nullptr, nullptr));
                OCL_CHECK(err, err = q.finish());
            }

            auto time_start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < num_kernel_ddr; i++) {
                OCL_CHECK(err, err = q.enqueueTask(krnls[i]));
            }
            q.finish();
            auto time_end = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < num_kernel_ddr; i++) {
                OCL_CHECK(err, err = q.enqueueReadBuffer(output_buffer[i], CL_TRUE, 0, vector_size_bytes,
                                                         output_host[i].data(), nullptr, nullptr));
                OCL_CHECK(err, err = q.finish());
            }

            // check
            for (int i = 0; i < num_kernel_ddr; i++) {
                for (uint32_t j = 0; j < data_size; j++) {
                    if (output_host[i][j] != input_host[j]) {
                        std::cout << "ERROR : kernel failed to copy entry " << j << " input " << input_host[j]
                                  << " output " << output_host[i][j] << std::endl;
                        return EXIT_FAILURE;
                    }
                }
            }

            double usduration =
                (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count() / reps);

            double dnsduration = (double)usduration;
            double dsduration = dnsduration / ((double)1000000000);
            double bpersec = (data_size * num_kernel_ddr) / dsduration;
            double mbpersec = (2 * bpersec) / ((double)1024 * 1024); // For concurrent Read/Write

            if (mbpersec > max_throughput) max_throughput = mbpersec;
        }

        std::cout << "Throughput (Type: DDR) (Bank count: " << num_kernel_ddr << ") : " << max_throughput << "MB/s\n";
    }
    if (chk_hbm_mem) {
        for (uint32_t i = 4 * 1024; i <= 16 * 1024 * 1024; i *= 2) {
            unsigned int data_size = i;

            if (xcl::is_emulation()) {
                reps = 2;
                if (data_size > 8 * 1024) {
                    break;
                }
            }

            unsigned int vector_size_bytes = data_size;
            std::vector<unsigned char, aligned_allocator<unsigned char> > input_host(data_size);
            std::vector<unsigned char, aligned_allocator<unsigned char> > output_host(data_size);

            for (uint32_t j = 0; j < data_size; j++) {
                input_host[j] = j % 256;
            }

            // Initializing output vectors to zero
            std::fill(output_host.begin(), output_host.end(), 0);

            cl::Buffer input_buffer, output_buffer;

            // These commands will allocate memory on the FPGA. The cl::Buffer objects
            // can
            // be used to reference the memory locations on the device.
            // Creating Buffers
            OCL_CHECK(err, input_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, vector_size_bytes, nullptr, &err));
            OCL_CHECK(err, output_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, vector_size_bytes, nullptr, &err));

            OCL_CHECK(err, err = krnls[num_kernel - 1].setArg(0, input_buffer));
            OCL_CHECK(err, err = krnls[num_kernel - 1].setArg(1, output_buffer));
            OCL_CHECK(err, err = krnls[num_kernel - 1].setArg(2, data_size));
            OCL_CHECK(err, err = krnls[num_kernel - 1].setArg(3, reps));

            OCL_CHECK(err, err = q.enqueueWriteBuffer(input_buffer, CL_TRUE, 0, vector_size_bytes, input_host.data(),
                                                      nullptr, nullptr));
            OCL_CHECK(err, err = q.finish());

            auto time_start = std::chrono::high_resolution_clock::now();

            OCL_CHECK(err, err = q.enqueueTask(krnls[num_kernel - 1]));
            q.finish();
            auto time_end = std::chrono::high_resolution_clock::now();

            OCL_CHECK(err, err = q.enqueueReadBuffer(output_buffer, CL_TRUE, 0, vector_size_bytes, output_host.data(),
                                                     nullptr, nullptr));
            OCL_CHECK(err, err = q.finish());

            // check
            for (uint32_t j = 0; j < data_size; j++) {
                if (output_host[j] != input_host[j]) {
                    std::cout << "ERROR : kernel failed to copy entry " << j << " input " << input_host[j] << " output "
                              << output_host[j] << std::endl;
                    return EXIT_FAILURE;
                }
            }

            double usduration =
                (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count() / reps);

            double dnsduration = (double)usduration;
            double dsduration = dnsduration / ((double)1000000000);
            double bpersec = data_size / dsduration;
            double mbpersec = (2 * bpersec) / ((double)1024 * 1024); // For concurrent Read/Write

            if (mbpersec > max_throughput) max_throughput = mbpersec;
        }

        std::cout << "Throughput (Type: HBM) (Bank count: 1) : " << max_throughput << "MB/s\n";
    }

    std::cout << "TEST PASSED\n";

    return EXIT_SUCCESS;
}
