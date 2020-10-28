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
#include <iostream>
#include <cstring>
#include <fstream>
#include <xma.h>
#include <xmaplugin.h>

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

    int32_t rc = 0;
    const char *hello_file = argv[3];
    const char *log_file = argv[4];
    int32_t size_hello = 4;
    uint32_t size_log = 4096;
    uint32_t dev_id = 0;

    if (argv[2])
        dev_id = atoi(argv[2]);

    XmaXclbinParameter tmp_xclbin_param;
    tmp_xclbin_param.device_id = dev_id;
    //tmp_xclbin_param.xclbin_name = "soft_hello_world.xclbin";
    tmp_xclbin_param.xclbin_name = argv[1];

    rc = xma_initialize(&tmp_xclbin_param, 1);
    if (rc != 0) {
        std::cout << "ERROR: Failed to load xclbin & xma_init failed" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }


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

    // Setup copy encoder properties
    //Using dummy session; so values do not matter
    XmaEncoderProperties enc_props;
    std::memset(&enc_props, 0, sizeof(enc_props));
    enc_props.hwencoder_type = XMA_COPY_ENCODER_TYPE;
    //strcpy(enc_props.hwvendor_string, "Xilinx");
    std::string x_vendor = "Xilinx";
    x_vendor.copy(enc_props.hwvendor_string, x_vendor.size());
    enc_props.format = XMA_YUV420_FMT_TYPE;
    enc_props.bits_per_pixel = 8;
    enc_props.width = 1920;
    enc_props.height = 1080;

    enc_props.plugin_lib = "./libdummy_plugin_enc.so";
    enc_props.dev_index = dev_id;
    enc_props.ddr_bank_index = -1;//XMA to select the ddr bank based on xclbin meta data
    //enc_props.ddr_bank_index = 2;//XMA to use user provided ddr bank; error if invalid ddr bank

    //hardware PL hello CUs are: 0 - 7
    enc_props.cu_index = 0;//cu_index of hw kernel in xclbin; 

    //Create dummy xma session
    XmaEncoderSession *xma_enc_session1 = xma_enc_session_create(&enc_props);
    if (!xma_enc_session1) {
        std::cout << "ERROR: Failed to create dummy xma encoder session for PL hello CU" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }

    XmaBufferObj buf_hello = xma_plg_buffer_alloc(xma_enc_session1->base, size_hello, false, &rc);
    if (rc != 0) {
        std::cout << "ERROR: Failed to allocate device buffer for hello_world" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }

    regfile_hw_t regmap1;
    std::memset(&regmap1, 0, sizeof(regmap1));

    regmap1.out_hello = buf_hello.paddr;

    //Start hardware PL kernel
    XmaCUCmdObj cu_cmd1 = xma_plg_schedule_work_item(xma_enc_session1->base, (void*)&regmap1, sizeof(regmap1), &rc);
    if (rc != 0) {
        std::cout << "ERROR: Failed to start hello PL kernel" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }

    //Wait for the hello PL kernel to finish
    xma_plg_is_work_item_done(xma_enc_session1->base, 10000);

    //DMA kernel output to host
    rc = xma_plg_buffer_read(xma_enc_session1->base, buf_hello, buf_hello.size, 0);
    if (rc != 0) {
        std::cout << "ERROR: DMA failed" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }
    std::string hello_str1((char *)buf_hello.data);
    if (hello_str1.find("Hello World") != std::string::npos) {
        std::cout << "Hello world check on hardware PL CU completed: Correct" << std::endl;
    } else {
        std::cout << "ERROR: Hello world check failed" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
    }

    //Clear out the buffer on device for next soft kernel check
    std::memset(buf_hello.data, 0, buf_hello.size);
    rc = xma_plg_buffer_write(xma_enc_session1->base, buf_hello, buf_hello.size, 0);
    if (rc != 0) {
        std::cout << "ERROR: DMA failed" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }

    //soft hello CUs are: 8 - 15
    enc_props.cu_index = 8;//cu_index of soft kernel in xclbin; 

    //Create dummy xma session
    XmaEncoderSession *xma_enc_session2 = xma_enc_session_create(&enc_props);
    if (!xma_enc_session2) {
        std::cout << "ERROR: Failed to create dummy xma encoder session for soft hello_world CU" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }


    regfile_t regmap;
    std::memset(&regmap, 0, sizeof(regmap));
    XmaBufferObj buf_log = xma_plg_buffer_alloc(xma_enc_session2->base, size_log, false, &rc);
    if (rc != 0) {
        std::cout << "ERROR: Failed to allocate device buffer for log files" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }
    /*
    uint32_t *regptr1 = (uint32_t *) &regmap;
    uint64_t *regptr2 = (uint64_t *) &regptr1[1];//After skipping first reserved 32 bits
    regptr1[0] = 0x1;
    regptr2[0] = buf_hello.paddr;
    regptr2[1] = buf_log.paddr;

    regptr1[5] = size_hello;
    regptr1[6] = size_log;

    //dummy values below
    regptr1[7] = 0x8;
    regptr1[8] = 0x9;
    regptr1[9] = 0xA;
    regptr1[10] = 0xB;
    regptr1[11] = 0xC;
    regptr1[12] = 0xD;
    */
     
    regmap.out_hello = buf_hello.paddr;
    regmap.out_log = buf_log.paddr;
    regmap.size_hello = size_hello;
    regmap.size_log = size_log;

    //Start soft kernel
    XmaCUCmdObj cu_cmd2 = xma_plg_schedule_work_item(xma_enc_session2->base, (void*)&regmap, sizeof(regmap), &rc);
    if (rc != 0) {
        std::cout << "ERROR: Failed to start soft kernel" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }

    //Wait for the soft kernel to finish
    xma_plg_is_work_item_done(xma_enc_session2->base, 10000);

    //DMA kernel output to host
    rc = xma_plg_buffer_read(xma_enc_session2->base, buf_hello, buf_hello.size, 0);
    if (rc != 0) {
        std::cout << "ERROR: DMA failed" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }
    rc = xma_plg_buffer_read(xma_enc_session2->base, buf_log, buf_log.size, 0);
    if (rc != 0) {
        std::cout << "ERROR: DMA failed" << std::endl;
        std::cout << ">>>>>>>> TEST FAILED >>>>>>>" << std::endl;
        return -1;
    }
    //memcpy(&out_size, ctx->encoder.output_len[nb].data, ctx->encoder.output_len[nb].size);
    std::string hello_str2((char *)buf_hello.data);
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
        log_stream.write((const char *)buf_log.data, buf_log.size);
        log_stream.close();
    }

    return 0;
    
}
