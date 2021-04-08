/*
 * Copyright (C) 2018, Xilinx Inc - All rights reserved
 * Xilinx SDAccel Media Accelerator API
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
//#include "xrt/xrt_bo.h"
#include "xrt/xrt_kernel.h"
#include "experimental/xrt_xclbin.h"
#include "xrt/xrt_device.h"
#include "xrt.h"
#include <iostream>
#include <cstring>
#include <fstream>

/*
typedef struct regfile {
    unsigned long long out_hello;
    unsigned long long out_log;
    int size_hello;
    unsigned int size_log;
} regfile_t;
*/

//MUST use packed struct; Else 64 bit argument may have offset
//Use packed struct in both host application and soft kernel
typedef struct __attribute__ ((packed)) regfile {
    uint32_t reserved;//Reserved 32 bit; This is not passed to soft kernel
    //Start of soft kernel arguments below
    uint64_t out_hello;
    uint64_t out_log;
    uint32_t  size_hello;
    uint32_t  size_log;
    uint32_t  dummy[8];//Extra margin
} regfile_t;

typedef struct __attribute__ ((packed)) regfile_hw {
    uint8_t reserved[16];//Reserved till 0x10 addr; 16 bytes
    //Start of hardware PL kernel arguments below
    uint64_t out_hello;
    uint32_t  dummy[8];//Extra margin
} regfile_hw_t;




int main(int argc, char *argv[])
{
    if (argc < 5 || argc > 7)
    {
        printf("Usage:\n");
        printf("   hello_world.exe <xclbin_file> <device_id> <outfile1.txt> <outfile2.txt> [max size of file1] [max size of file2]\n");
        printf("   device_id: device_id to use\n");
        printf("   outfile1: Has hello world string from soft kernel\n");
        printf("   outfile2: Has log files from U30 device PS\n");
        printf("   max file sizes: Size to be given in units of KB\n");
        printf("   ./hello_world.exe xclbin_file 0 out_hello.txt out_logs.txt 4 128\n");
        return -1;
    }

    const char *hello_file = argv[3];
    const char *log_file = argv[4];
    int32_t size_hello = 4;
    uint32_t size_log = 4096;
    uint32_t dev_id = 0;

    if (argv[2])
        dev_id = atoi(argv[2]);

    std::string xclbin_file(argv[1]);
    std::string cu1_name = "hello";
    std::string cu_soft_name = "hello_world";

    if (xclbin_file.empty())
        throw std::runtime_error("FAILED_TEST\nNo xclbin specified");

    auto device = xrt::device(dev_id);
    auto uuid = device.load_xclbin(xclbin_file);
    auto kernel1 = xrt::kernel(device, uuid, cu1_name);
    auto grpidx1 = kernel1.group_id(0);//Memory bank index
    //auto kernel_soft = xrt::kernel(device, uuid, cu_soft_name);//Soft kenrl not working
    auto kernel_soft = xrt::kernel(device, uuid, cu1_name);
    auto grpidx_soft = kernel_soft.group_id(0);//Memory bank index

    if (argv[5])
        size_hello = atoi(argv[5]);
    if (argv[6])
        size_log = atoi(argv[6]);
    if (size_hello <= 0 ) {
        size_hello = 4096;//in bytes
    } else {
        size_hello = size_hello * 1024;//in bytes
    }
    if (size_log > 512 * 1024) {//Check user input in KB
        size_log = 512 * 1024 * 1024;//in bytes
    } else if (size_log == 0) {
        size_log = 4096;//in bytes
    } else {
        size_log = size_log * 1024;//in bytes
    }

    auto buf_hello = xrt::bo(device, size_hello, grpidx1);
    const char* buf_hello_ptr = (const char*)buf_hello.map();
    if (!buf_hello_ptr) {
        std::cout << "ERROR: Failed to allocate device buffer for hello_world" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }
    std::memset((void*)buf_hello_ptr, 0, size_hello);
    //Clear out the buffer before test
    buf_hello.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    auto run1 = kernel1(buf_hello);//Set arguments and start kernel
    run1.wait();
    //run1(buf_new);//Set new arguments and start kernel
    //run1.wait();

    //DMA kernel output to host
    buf_hello.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    std::string hello_str1((char *)buf_hello_ptr);
    if (hello_str1.find("Hello World") != std::string::npos) {
        std::cout << "Hello world check on hardware PL CU completed: Correct" << std::endl;
    } else {
        std::cout << "ERROR: Hello world check failed" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
    }

    //Clear out the buffer on device for next soft kernel check
    std::memset((void*)buf_hello_ptr, 0, size_hello);
    //Clear out the buffer before test
    buf_hello.sync(XCL_BO_SYNC_BO_TO_DEVICE);

    auto buf_log = xrt::bo(device, size_log, grpidx_soft);
    const char* buf_log_ptr = (const char*)buf_log.map();
    if (!buf_log_ptr) {
        std::cout << "ERROR: Failed to allocate device buffer for soft CU hello_world" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }
    auto run2 = kernel_soft(buf_hello, buf_log);
    run2.wait();

    //DMA kernel output to host
    buf_hello.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    buf_log.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
    std::string hello_str2((char *)buf_hello_ptr);
    if (hello_str2.find("Hello World - ") != std::string::npos) {
        std::cout << "TEST PASSED: Hello world check completed" << std::endl;
    } else {
        std::cout << "ERROR: Hello world check failed" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
    }
    std::ofstream hello_stream(hello_file, std::ios::binary | std::ios::out);
    if (hello_stream.is_open()) {
        hello_stream.write(hello_str2.c_str(), hello_str2.size() + 1);
        hello_stream.close();
    }
    std::ofstream log_stream(log_file, std::ios::binary | std::ios::out);
    if (log_stream.is_open()) {
        log_stream.write(buf_log_ptr, size_log);
        log_stream.close();
    }

    return 0;
    
}
