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
    std::string b_file = "/hostmemory.xclbin";
    std::string old_b_file = "/slavebridge.xclbin";
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
    std::string binary_file = test_path + b_file;
    std::ifstream infile(binary_file);
    // This is for backward compatibility support when older platforms still having slavebridge.xclbin.
    std::string old_binary_file = test_path + old_b_file;
    std::ifstream old_infile(old_binary_file);
    if (flag_s) {
        if (!infile.good()) {
            if (!old_infile.good()) {
                std::cout << "\nNOT SUPPORTED" << std::endl;
                return EOPNOTSUPP;
            } else {
                std::cout << "\nSUPPORTED" << std::endl;
                return EXIT_SUCCESS;
            }
        } else {
            std::cout << "\nSUPPORTED" << std::endl;
            return EXIT_SUCCESS;
        }
    }

    int num_kernel;
    std::string filename = "/platform.json";
    std::string platform_json = test_path + filename;

    try {
        boost::property_tree::ptree load_ptree_root;
        boost::property_tree::read_json(platform_json, load_ptree_root);
        auto temp = load_ptree_root.get_child("total_host_banks");
        num_kernel = temp.get_value<int>();
    } catch (const std::exception& e) {
        std::string msg("ERROR: Bad JSON format detected while marshaling build metadata (");
        msg += e.what();
        msg += ").";
        std::cout << msg;
    }

    if (!infile.good()) {
        if (!old_infile.good()) {
            std::cout << "\nNOT SUPPORTED" << std::endl;
            return EOPNOTSUPP;
        }
    }

    cl_int err;
    cl::Context context;
    std::string krnl_name = "hostmemory";
    if (!infile.good()) {
        krnl_name = "slavebridge";
    }
    std::vector<cl::Kernel> krnls(num_kernel);
    cl::CommandQueue q;

    // OPENCL HOST CODE AREA START
    // get_xil_devices() is a utility API which will find the xilinx
    // platforms and will return list of devices connected to Xilinx platform
    auto devices = xcl::get_xil_devices();
    // read_binary_file() is a utility API which will load the binary_file
    // and will return the pointer to file buffer.
    std::vector<unsigned char> file_buf;

    // Backward compatability support if platform had older xclbin
    if (infile.good()) {
        file_buf = xcl::read_binary_file(binary_file);
    } else {
        file_buf = xcl::read_binary_file(old_binary_file);
    }
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
            std::string krnl_name_full;
            if (infile.good()) {
                krnl_name_full = krnl_name + ":{" + "hostmemory_" + cu_id + "}";
            } else {
                krnl_name_full = krnl_name + ":{" + "slavebridge_" + cu_id + "}";
            }

            // Here Kernel object is created by specifying kernel name along with
            // compute unit.
            // For such case, this kernel object can only access the specific
            // Compute unit

            OCL_CHECK(err, krnls[i] = cl::Kernel(program, krnl_name_full.c_str(), &err));
        }
    }

    double max_throughput = 0;
    int reps = stoi(iter_cnt);

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

        for (uint32_t j = 0; j < data_size; j++) {
            input_host[j] = j % 256;
        }

        std::vector<cl::Buffer> input_buffer(num_kernel);
        std::vector<cl::Buffer> output_buffer(num_kernel);

        std::vector<cl_mem_ext_ptr_t> input_buffer_ext(num_kernel);
        std::vector<cl_mem_ext_ptr_t> output_buffer_ext(num_kernel);
        for (int i = 0; i < num_kernel; i++) {
            input_buffer_ext[i].flags = XCL_MEM_EXT_HOST_ONLY;
            input_buffer_ext[i].obj = nullptr;
            input_buffer_ext[i].param = 0;

            output_buffer_ext[i].flags = XCL_MEM_EXT_HOST_ONLY;
            output_buffer_ext[i].obj = nullptr;
            output_buffer_ext[i].param = 0;
        }

        for (int i = 0; i < num_kernel; i++) {
            OCL_CHECK(err, input_buffer[i] = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                        vector_size_bytes, &input_buffer_ext[i], &err));
            OCL_CHECK(err, output_buffer[i] = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                                         vector_size_bytes, &output_buffer_ext[i], &err));
        }

        unsigned char* map_input_buffer[num_kernel];
        unsigned char* map_output_buffer[num_kernel];

        for (int i = 0; i < num_kernel; i++) {
            OCL_CHECK(err, err = krnls[i].setArg(0, input_buffer[i]));
            OCL_CHECK(err, err = krnls[i].setArg(1, output_buffer[i]));
            OCL_CHECK(err, err = krnls[i].setArg(2, data_size));
            OCL_CHECK(err, err = krnls[i].setArg(3, reps));
        }

        for (int i = 0; i < num_kernel; i++) {
            OCL_CHECK(err,
                      map_input_buffer[i] = (unsigned char*)q.enqueueMapBuffer(
                          (input_buffer[i]), CL_FALSE, CL_MAP_WRITE, 0, vector_size_bytes, nullptr, nullptr, &err));
            OCL_CHECK(err, err = q.finish());
        }

        /* prepare data to be written to the device */
        for (int i = 0; i < num_kernel; i++) {
            for (size_t j = 0; j < vector_size_bytes; j++) {
                map_input_buffer[i][j] = input_host[j];
            }
        }

        auto time_start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_kernel; i++) {
            OCL_CHECK(err, err = q.enqueueTask(krnls[i]));
        }
        q.finish();
        auto time_end = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_kernel; i++) {
            OCL_CHECK(err,
                      map_output_buffer[i] = (unsigned char*)q.enqueueMapBuffer(
                          (output_buffer[i]), CL_FALSE, CL_MAP_READ, 0, vector_size_bytes, nullptr, nullptr, &err));
            OCL_CHECK(err, err = q.finish());
        }

        // check
        for (int i = 0; i < num_kernel; i++) {
            for (uint32_t j = 0; j < data_size; j++) {
                if (map_output_buffer[i][j] != map_input_buffer[i][j]) {
                    std::cout << "ERROR : kernel failed to copy entry " << j << " input " << map_input_buffer[i][j]
                              << " output " << map_output_buffer[i][j] << std::endl;
                    return EXIT_FAILURE;
                }
            }
        }

        double usduration =
            (double)(std::chrono::duration_cast<std::chrono::nanoseconds>(time_end - time_start).count() / reps);
        double dnsduration = (double)usduration;
        double dsduration = dnsduration / ((double)1000000000);
        double bpersec = (data_size * num_kernel) / dsduration;
        double mbpersec = (2 * bpersec) / ((double)1024 * 1024); // For concurrent Read/Write

        if (mbpersec > max_throughput) max_throughput = mbpersec;
    }
    std::cout << "Throughput (Type: HOST) (Bank count: " << num_kernel << "): " << max_throughput << "MB/s\n";

    std::cout << "TEST PASSED\n";

    return EXIT_SUCCESS;
}
