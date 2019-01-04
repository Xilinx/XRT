/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

// Copyright 2017 Xilinx, Inc. All rights reserved.

#include <cstdio>
#include <stdint.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <CL/opencl.h>
#include <CL/cl_ext.h>

//pointer access pipeline loop
#define PIPELINELOOP                        2

#define RUNMODE                             2
#define TYPESIZE                            512
#define TYPE                                uint16
#define TYPEISVECTOR                        1
#define THROUGHPUT_CHK                      16000

const size_t typesize = TYPESIZE;
const int runmode = RUNMODE;

//expected throughput for test
//width,runmode
//width = 32,64,128,256,512
//runmode = 0..5
double expected[5][6]={    {300,240,450,250,250,250},        /* 32 bits*/
                           {600,500,1000,500,500,500},       /* 64 bits*/
                           {1100,900,1500,1100,1100,1100},   /*128 bits*/
                           {1500,1500,1900,2200,2200,2200},  /*256 bits*/
                           {1900,2000,2300,3800,3800,3800}  /*512 bits*/
};

////////////////////////////////////////////////////////////////////////////////
class Timer {
    std::chrono::high_resolution_clock::time_point mTimeStart;
public:
    Timer() {
        reset();
    }
    long long stop() {
        std::chrono::high_resolution_clock::time_point timeEnd = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - mTimeStart).count();
    }
    void reset() {
        mTimeStart = std::chrono::high_resolution_clock::now();
    }
};

/////////////////////////////////////////////////////////////////////////////////
//load_file_to_memory
//Allocated memory for and load file from disk memory
//Return value
// 0   Success
//-1   Failure to open file
//-2   Failure to allocate memory
int load_file_to_memory(const char *filename, char **result,size_t *inputsize)
{
    int size = 0;
    FILE *f = fopen(filename, "rb");
    if (f == NULL) {
        *result = NULL;
        return -1; // -1 means file opening fail
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    *result = (char *)malloc(size+1);
    if (size != fread(*result, sizeof(char), size, f))
    {
        free(*result);
        return -2; // -2 means file reading fail
    }
    fclose(f);
    (*result)[size] = 0;
    if(inputsize!=NULL) (*inputsize)=size;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////////
//opencl_setup
//Create context for Xilinx platform, Accelerator device
//Create single command queue for accelerator device
//Create program object with clCreateProgramWithBinary using given xclbin file name
//Return value
// 0    Success
//-1    Error
//-2    Failed to load XCLBIN file from disk
//-3    Failed to clCreateProgramWithBinary
int opencl_setup(const char *xclbinfilename, cl_platform_id *platform_id,
                 cl_device_id *devices, cl_device_id *device_id, cl_context  *context,
                 cl_command_queue *command_queue, cl_program *program,
                 char *cl_platform_name, const char *target_device_name) {

    char cl_platform_vendor[1001];
    char cl_device_name[1001];
    cl_int err;
    cl_uint num_devices;
    unsigned int device_found = 0;

    // Get first platform
    err = clGetPlatformIDs(1,platform_id,NULL);
    if (err != CL_SUCCESS) {
        std::printf("ERROR: Failed to find an OpenCL platform!\n");
        std::printf("ERROR: Test failed\n");
        return -1;
    }
    err = clGetPlatformInfo(*platform_id,CL_PLATFORM_VENDOR,1000,(void *)cl_platform_vendor,NULL);
    if (err != CL_SUCCESS) {
        std::printf("ERROR: clGetPlatformInfo(CL_PLATFORM_VENDOR) failed!\n");
        std::printf("ERROR: Test failed\n");
        return -1;
    }
    std::printf("CL_PLATFORM_VENDOR %s\n",cl_platform_vendor);
    err = clGetPlatformInfo(*platform_id,CL_PLATFORM_NAME,1000,(void *)cl_platform_name,NULL);
    if (err != CL_SUCCESS) {
        std::printf("ERROR: clGetPlatformInfo(CL_PLATFORM_NAME) failed!\n");
        std::printf("ERROR: Test failed\n");
        return -1;
    }
    std::printf("CL_PLATFORM_NAME %s\n",cl_platform_name);

    // Get Accelerator compute device
    int accelerator = 1;
    err = clGetDeviceIDs(*platform_id, CL_DEVICE_TYPE_ACCELERATOR, 16, devices, &num_devices);
    if (err != CL_SUCCESS) {
        std::printf("ERROR: Failed to create a device group!\n");
        std::printf("ERROR: Test failed\n");
        return -1;
    }

    //iterate all devices to select the target device.
    for (int i=0; i<num_devices; i++) {
        err = clGetDeviceInfo(devices[i], CL_DEVICE_NAME, 1024, cl_device_name, 0);
        if (err != CL_SUCCESS) {
            std::printf("Error: Failed to get device name for device %d!\n", i);
            std::printf("Test failed\n");
            return EXIT_FAILURE;
        }
        //std::printf("CL_DEVICE_NAME %s\n", cl_device_name);
        if(std::strstr(cl_device_name, target_device_name)) {
            *device_id = devices[i];
            device_found = 1;
            std::printf("Selected %s as the target device\n", cl_device_name);
        }
    }

    if (!device_found) {
        std::printf("Target device %s not found. Exit.\n", target_device_name);
        return EXIT_FAILURE;
    }

    // Create a compute context containing accelerator device
    (*context)= clCreateContext(0, 1, device_id, NULL, NULL, &err);
    if (!(*context))
    {
        std::printf("ERROR: Failed to create a compute context!\n");
        std::printf("ERROR: Test failed\n");
        return -1;
    }

    // Create a command queue for accelerator device
    (*command_queue) = clCreateCommandQueue(*context, *device_id, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
    if (!(*command_queue))
    {
        std::printf("ERROR: Failed to create a command commands!\n");
        std::printf("ERROR: code %i\n",err);
        std::printf("ERROR: Test failed\n");
        return -1;
    }

    // Load XCLBIN file binary from disk
    int status;
    unsigned char *kernelbinary;
    std::printf("loading %s\n", xclbinfilename);
    size_t xclbinlength;
    err = load_file_to_memory(xclbinfilename, (char **) &kernelbinary,&xclbinlength);
    if (err != 0) {
        std::printf("ERROR: failed to load kernel from xclbin: %s\n", xclbinfilename);
        std::printf("ERROR: Test failed\n");
        return -2;
    }

    // Create the program from XCLBIN file, configuring accelerator device
    (*program) = clCreateProgramWithBinary(*context, 1, device_id, &xclbinlength, (const unsigned char **) &kernelbinary, &status, &err);
    if ((!(*program)) || (err!=CL_SUCCESS)) {
        std::printf("ERROR: Failed to create compute program from binary %d!\n", err);
        std::printf("ERROR: Test failed\n");
        return -3;
    }

    // Build the program executable (no-op)
    err = clBuildProgram(*program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t len;
        char buffer[2048];
        std::printf("ERROR: Failed to build program executable!\n");
        clGetProgramBuildInfo(*program, *device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        std::printf("%s\n", buffer);
        std::printf("ERROR: Test failed\n");
        return -1;
    }

    return 0;
}



/////////////////////////////////////////////////////////////////////////////////
//main

int main(int argc, char** argv)
{
    //change the line below to match the target device
    const char *target_device_name = "xilinx";

    int err;

    size_t globalbuffersize = 1024*1024*16;    //16 MB

    //opencl setup
    //create context for first platform ACCELERATOR device
    //load xclbinfile "test.xclbin"
    cl_platform_id platform_id;
    cl_device_id device_id;
    cl_device_id devices[16];  // compute device id
    cl_context context;
    cl_command_queue command_queue;
    cl_program program;
    char cl_platform_name[1001];
    cl_kernel kernel[2] = {NULL, NULL};

/*    char *cwd;
    cwd = (char *)malloc(100*sizeof(char));
    getcwd(cwd,100);
    std::printf("Current working directory is: %s\n", cwd);
*/
    err = opencl_setup("bandwidth.xclbin", &platform_id, devices, &device_id,
                       &context, &command_queue, &program, cl_platform_name,
                       target_device_name);
    if(err==-1){
        std::printf("Error : general failure setting up opencl context\n");
        return -1;
    }
    if(err==-2) {
        std::printf("Error : failed to load bandwidth.xclbin from disk\n");
        return -1;
    }
    if(err==-3) {
        std::printf("Error : failed to clCreateProgramWithBinary with contents of bandwidth.xclbin\n");
    }

    //access the ACCELERATOR kernel
    cl_int clstatus;
    kernel[0] = clCreateKernel(program, "bandwidth1", &clstatus);
    if (!kernel[0] || clstatus != CL_SUCCESS) {
        std::printf("Error: Failed to create compute kernel!\n");
        std::printf("Error: Test failed\n");
        return -1;
    }

    kernel[1] = clCreateKernel(program, "bandwidth2", &clstatus);
    if (!kernel[1] || clstatus != CL_SUCCESS) {
        std::printf("Error: Failed to create compute kernel!\n");
        std::printf("Error: Test failed\n");
        return -1;
    }

    //input buffer
    unsigned char *input_host1 = new unsigned char[globalbuffersize];
    unsigned char *input_host2 = new unsigned char[globalbuffersize];
    if(input_host1==NULL || input_host2==NULL) {
        std::printf("Error: Failed to allocate host side copy of OpenCL source buffer of size %zu\n", globalbuffersize);
        return -1;
    }
    unsigned int i;
    for(i=0; i<globalbuffersize; i++) {
        input_host1[i]=i%256;
        input_host2[i]=i%256;
    }

    cl_mem input_buffer1;
    cl_mem_ext_ptr_t input_buffer1_ext;
    input_buffer1_ext.flags = 1;
    input_buffer1_ext.obj = NULL;
    input_buffer1_ext.param = kernel[0];

    input_buffer1 = clCreateBuffer(context,
                                   CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                   globalbuffersize,
                                   &input_buffer1_ext,
                                   &err);

    cl_mem input_buffer2;
    cl_mem_ext_ptr_t input_buffer2_ext;
    input_buffer2_ext.flags = 1;
    input_buffer2_ext.obj = NULL;
    input_buffer2_ext.param = kernel[1];
    input_buffer2 = clCreateBuffer(context,
                                   CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                   globalbuffersize,
                                   &input_buffer2_ext,
                                   &err);

    if(err != CL_SUCCESS) {
        std::printf("Error: Failed to allocate OpenCL source buffer of size %zu\n", globalbuffersize);
        return -1;
    }

    //output host
    unsigned char *output_zerohost = ((unsigned char *)malloc(globalbuffersize));
    if(output_zerohost==NULL){
        std::printf("Error: Failed to allocate host side copy of OpenCL source buffer of size %zu\n", globalbuffersize);
        return -1;
    }
    for(i=0;i<globalbuffersize;i++) output_zerohost[i]=0;
    unsigned char *output_host1 = new unsigned char[globalbuffersize];
    unsigned char *output_host2 = new unsigned char[globalbuffersize];
    if(output_host1==NULL || output_host2==NULL){
        std::printf("Error: Failed to allocate host side copy of OpenCL source buffer of size %zu\n",globalbuffersize);
        return -1;
    }
    //output buffer
    cl_mem output_buffer1;
    cl_mem_ext_ptr_t output_buffer1_ext;
    output_buffer1_ext.flags = 0;
    output_buffer1_ext.obj = NULL;
    output_buffer1_ext.param = kernel[0];
    output_buffer1 = clCreateBuffer(context,
                                    CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                    globalbuffersize,
                                    &output_buffer1_ext,
                                    &err);

    cl_mem output_buffer2;
    cl_mem_ext_ptr_t output_buffer2_ext;
    output_buffer2_ext.flags = 0;
    output_buffer2_ext.obj = NULL;
    output_buffer2_ext.param = kernel[1];

    output_buffer2 = clCreateBuffer(context,
                                    CL_MEM_READ_WRITE | CL_MEM_EXT_PTR_XILINX,
                                    globalbuffersize,
                                    &output_buffer2_ext,
                                    &err);
    if (err != CL_SUCCESS) {
        std::printf("Error: Failed to allocate worst case OpenCL output buffer of size %zu\n",globalbuffersize);
        return -1;
    }

    //copy input dataset to OpenCL buffer
    uint32_t globalbuffersizeinbeats = globalbuffersize/(typesize/8);
    uint32_t tests=std::log2(globalbuffersizeinbeats)+1;

    double dnsduration[tests];
    double dsduration[tests];
    double dbytes[tests];
    double dmbytes[tests];
    double bpersec[tests];
    double mbpersec[tests];


    //run tests with burst length 1 beat to globalbuffersize
    //double burst length each test
    uint32_t beats;
    uint32_t test=0;
    FILE *metric1 = fopen("metric1.csv","w");
    if(metric1==NULL){
        std::printf("Error : cannot create metric1.csv\n");
        exit(1);
    }

    float throughput[7] = {0,0,0,0,0,0,0};
    for(beats=16;beats<=1024;beats=beats*4)
    {
        uint32_t memcpyblocks = 512;
        unsigned char mode = 0;

        //iterate until the test takes at least 10 second

        mode = runmode;


        if(mode==PIPELINELOOP) std::printf("LOOP PIPELINE %d beats\n", beats);

        long long usduration;
        long long tenseconds = 10 * 1000000;
        unsigned reps=64;
        do{

            //write input buffers
            err = clEnqueueWriteBuffer(command_queue, input_buffer1, CL_FALSE, 0, globalbuffersize, input_host1, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                std::printf("Error: Failed to copy input dataset to OpenCL buffer\n");
                std::printf("Error: Test failed\n");
                return -1;
            }

            err = clEnqueueWriteBuffer(command_queue, input_buffer2, CL_FALSE, 0, globalbuffersize, input_host2, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                std::printf("Error: Failed to copy input dataset to OpenCL buffer\n");
                std::printf("Error: Test failed\n");
                return -1;
            }

            //clear output buffers
            err = clEnqueueWriteBuffer(command_queue, output_buffer1, CL_FALSE, 0, globalbuffersize, output_zerohost, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                std::printf("Error: Failed to copy input dataset to OpenCL buffer\n");
                std::printf("Error: Test failed\n");
                return -1;
            }

            err = clEnqueueWriteBuffer(command_queue, output_buffer2, CL_FALSE, 0, globalbuffersize, output_zerohost, 0, NULL, NULL);
            if (err != CL_SUCCESS)
            {
                std::printf("Error: Failed to copy input dataset to OpenCL buffer\n");
                std::printf("Error: Test failed\n");
                return -1;
            }

            // Ensure the operations are done
            clFinish(command_queue);
            // Send the buffers to device
            cl_mem arr[] = {input_buffer1, input_buffer2, output_buffer1, output_buffer2};
            clEnqueueMigrateMemObjects(command_queue, 4, arr, 0, 0, NULL, NULL);
            clFinish(command_queue);
            //execute kernel
            err = 0;
            int i;

            err = clSetKernelArg(kernel[0], 0, sizeof(cl_mem), &output_buffer1);
            err |= clSetKernelArg(kernel[0], 1, sizeof(cl_mem), &input_buffer1);
            err |= clSetKernelArg(kernel[0], 2, sizeof(uint32_t), &beats);
            err |= clSetKernelArg(kernel[0], 3, sizeof(uint32_t), &reps);

            if (err != CL_SUCCESS) {
                std::printf("ERROR: Failed to set kernel[0] arguments! %d\n", err);
                std::printf("ERROR: Test failed\n");
                return EXIT_FAILURE;
            }

            err = clSetKernelArg(kernel[1], 0, sizeof(cl_mem), &output_buffer2);
            err |= clSetKernelArg(kernel[1], 1, sizeof(cl_mem), &input_buffer2);
            err |= clSetKernelArg(kernel[1], 2, sizeof(uint32_t), &beats);
            err |= clSetKernelArg(kernel[1], 3, sizeof(uint32_t), &reps);


            if (err != CL_SUCCESS) {
                std::printf("ERROR: Failed to set kernel[1] arguments! %d\n", err);
                std::printf("ERROR: Test failed\n");
                return EXIT_FAILURE;
            }


            size_t global[1];
            size_t local[1];
            global[0]=1;
            local[0]=1;

            Timer timer;
            err = clEnqueueNDRangeKernel(command_queue, kernel[0], 1, NULL, global, local, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                std::printf("ERROR: Failed to execute kernel %d\n", err);
                std::printf("ERROR: Test failed\n");
                return EXIT_FAILURE;
            }


            err = clEnqueueNDRangeKernel(command_queue, kernel[1], 1, NULL, global, local, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                std::printf("ERROR: Failed to execute kernel %d\n", err);
                std::printf("ERROR: Test failed\n");
                return EXIT_FAILURE;
            }

            clFinish(command_queue);
            const long long timer_stop = timer.stop();
            //copy results back from OpenCL buffer

            err = clEnqueueReadBuffer( command_queue, output_buffer1, CL_FALSE, 0, globalbuffersize, output_host1, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                std::printf("ERROR: Failed to read output size buffer %d\n", err);
                std::printf("ERROR: Test failed\n");
                return EXIT_FAILURE;
            }

            err = clEnqueueReadBuffer( command_queue, output_buffer2, CL_FALSE, 0, globalbuffersize, output_host2, 0, NULL, NULL);
            if (err != CL_SUCCESS) {
                std::printf("ERROR: Failed to read output size buffer %d\n", err);
                std::printf("ERROR: Test failed\n");
                return EXIT_FAILURE;
            }

            clFinish(command_queue);

            //check
            for (i=0; i<beats*(typesize/8); i++) {
                if(output_host1[i] != input_host1[i]){
                    std::printf("ERROR : kernel failed to copy entry %i input %i output %i\n",i,input_host1[i],output_host1[i]);
                    return EXIT_FAILURE;
                }
                if(output_host2[i] != input_host2[i]){
                    std::printf("ERROR : kernel failed to copy entry %i input %i output %i\n",i,input_host2[i],output_host2[i]);
                    return EXIT_FAILURE;
                }
            }

            usduration = timer_stop;
            dnsduration[test] = ((double)usduration);
            dsduration[test] = dnsduration[test] / ((double) 1000000);

            //std::printf("Reps = %d, Beats = %d, Duration = %lli us\n",reps, beats, usduration);
            std::fprintf(metric1, "Reps = %f Duration = %f \n",((float)reps),((float)dsduration[test]));

            if(usduration<tenseconds) reps=reps*2;

        } while(usduration < tenseconds);

        dnsduration[test] = ((double)usduration);
        dsduration[test] = dnsduration[test] / ((double) 1000000);
        dbytes[test] = reps*beats*(typesize/8);
        dmbytes[test] = dbytes[test] / (((double)1024) * ((double)1024));
        bpersec[test] = (2*dbytes[test])/dsduration[test]; // for 2 Kernels
        mbpersec[test] = 2*bpersec[test] / ((double) 1024*1024 ); // for concurrent READ and WRITE

        throughput[test] = mbpersec[test];
        std::printf ("Test : %d, Throughput: %f MB/s\n",test, throughput[test]);

        std::fprintf(metric1, "Buffer size = %f (MB) \n",dmbytes[test]);
        std::fprintf(metric1, "Reps = %f\n",((float)reps));
        std::fprintf(metric1, "Total Dataset size = %f (MB) \n",dmbytes[test]);
        std::fprintf(metric1, "Execution time = %f (sec) \n",dsduration[test]);
        std::fprintf(metric1, "Throughput  = %f (MB/sec) \n",mbpersec[test]);
        test++;
    }
    fclose(metric1);
    delete [] output_host1;
    delete [] output_host2;
    delete [] input_host1;
    delete [] input_host2;

    FILE *csvfile=fopen("output.csv","w");
    if(csvfile==NULL){
        std::printf("Error : cannot create output.csv\n");
        exit(1);
    }
    for(test=0;test<tests;test++){
        std::fprintf(csvfile,"%f",mbpersec[test]);
        if(test!=(tests-1)) std::fprintf(csvfile,",");
    }
    std::fprintf(csvfile,"\n");
    fclose(csvfile);

    float max_V;
    int ii;
    max_V = throughput[0];
    std::printf("TTTT : %f\n",throughput[0]);
    int count = 0;
    for (ii=1;ii<7;ii++) {
        if (max_V < throughput[ii]) {
            count++;
            max_V = throughput[ii];
        }
    }
    if (count == 0) {
        max_V = throughput[0];
    }
    std::printf ("Maximum throughput: %f MB/s\n",max_V);

    if (max_V < THROUGHPUT_CHK) {
        std::printf("ERROR: Throughput is less than expected value of %d GB/sec\n", THROUGHPUT_CHK/1000);
        exit(1);
    }
    else {
        std::printf("TEST PASSED\n");
    }

    return EXIT_SUCCESS;
}
