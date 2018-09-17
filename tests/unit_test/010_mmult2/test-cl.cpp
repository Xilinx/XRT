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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <CL/opencl.h>

////////////////////////////////////////////////////////////////////////////////

// Use a static matrix for simplicity
//
#define MATRIX_RANK 16
#define DATA_SIZE MATRIX_RANK*MATRIX_RANK

////////////////////////////////////////////////////////////////////////////////

int
load_file_to_memory(const char *filename, char **result)
{ 
  int size = 0;
  FILE *f = fopen(filename, "rb");
  if (f == NULL) 
  { 
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
  return size;
}

int main(int argc, char** argv)
{
  int err;                            // error code returned from api calls
     
  int a[DATA_SIZE];              // original data set given to device
  int b[DATA_SIZE];              // original data set given to device
  int results[DATA_SIZE];           // results returned from device
  int sw_results[DATA_SIZE];           // results returned from device
  unsigned int correct;               // number of correct results returned

  size_t global[2];                      // global domain size for our calculation
  size_t local[2];                       // local domain size for our calculation

  cl_platform_id platform_id;         // platform id
  cl_device_id device_id;             // compute device id 
  cl_context context;                 // compute context
  cl_command_queue commands;          // compute command queue
  cl_program program;                 // compute program
  cl_kernel kernel;                   // compute kernel

  char cl_platform_vendor[1001];
  char cl_platform_name[1001];
   
  cl_mem input_a;                       // device memory used for the input array
  cl_mem input_b;                       // device memory used for the input array
  cl_mem output;                      // device memory used for the output array
   
  // Fill our data sets with pattern
  //
  int i = 0;
  unsigned int count = MATRIX_RANK;
  for(i = 0; i < DATA_SIZE; i++) {
//    a[i] = (rand() / (int)RAND_MAX);
//    b[i] = (rand() / (int)RAND_MAX);
    a[i] = (int)i;
    b[i] = (int)i;
    results[i] = 0;
  }
  
  int test_num = 1;
  bool bitstream = false;
  if (argc != 2){
    printf("test-cl.exe <inputfile>\n");
    return EXIT_FAILURE;
  }

  // Connect to first platform
  //
  err = clGetPlatformIDs(1,&platform_id,NULL);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to find an OpenCL platform!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  err = clGetPlatformInfo(platform_id,CL_PLATFORM_VENDOR,1000,(void *)cl_platform_vendor,NULL);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: clGetPlatformInfo(CL_PLATFORM_VENDOR) failed!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  printf("CL_PLATFORM_VENDOR %s\n",cl_platform_vendor);
  err = clGetPlatformInfo(platform_id,CL_PLATFORM_NAME,1000,(void *)cl_platform_name,NULL);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: clGetPlatformInfo(CL_PLATFORM_NAME) failed!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  printf("CL_PLATFORM_NAME %s\n",cl_platform_name);
  
  // Connect to a compute device
  //
  int fpga = 0;
#if defined (FLOW_ZYNQ_HLS_BITSTREAM) || defined(FLOW_HLS_CSIM) || defined(FLOW_HLS_COSIM)
  fpga = 1;
#endif
  cl_uint num_devices = 0;
  err = clGetDeviceIDs(platform_id, fpga ? CL_DEVICE_TYPE_ACCELERATOR : CL_DEVICE_TYPE_CPU,
                       0, NULL, &num_devices);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to create a device group!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  
  // Create a compute context 
  //
  printf("Get %d devices\n", num_devices);
  cl_device_id * devices = (cl_device_id *)malloc(num_devices*sizeof(cl_device_id));
  err = clGetDeviceIDs(platform_id, fpga ? CL_DEVICE_TYPE_ACCELERATOR : CL_DEVICE_TYPE_CPU,
                       num_devices, devices, NULL);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to create a device group!\n");
    return EXIT_FAILURE;
  }

  for(int i = 0; i < num_devices; i++) {
    context = clCreateContext(0, 1, &devices[i], NULL, NULL, &err);
    if(err != CL_SUCCESS || context == NULL)
      continue;
    else {
        device_id = devices[i];
        printf("Using %dth device\n", i+1);
        break;
    }
  }
  if  (device_id == NULL) {
    printf("ERROR: Can not find any available device\n");
    printf("ERROR: Failed to create a compute context!\n");
    return EXIT_FAILURE;
  }

  // Create a command commands
  //
  commands = clCreateCommandQueue(context, device_id, 0, &err);
  if (!commands)
  {
    printf("ERROR: Failed to create a command commands!\n");
    printf("ERROR: code %i\n",err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  int status;

#if defined(FLOW_X86_64_ONLINE) || defined(FLOW_AMD_SDK_ONLINE) 
  unsigned char *kernelsrc;
  char *clsrc=argv[1];
  printf("loading %s\n", clsrc);
  int n_i = load_file_to_memory(clsrc, (char **) &kernelsrc);
  if (n_i < 0) {
    printf("failed to load kernel from source: %s\n", clsrc);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  // Create the compute program from the source buffer
  //
  program = clCreateProgramWithSource(context, 1, (const char **) &kernelsrc, NULL, &err);
  if (!program)
  {
    printf("ERROR: Failed to create compute program!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
#else
  // Load binary from disk
  unsigned char *kernelbinary;
  char *xclbin=argv[1];
  printf("loading %s\n", xclbin);
  int n_i = load_file_to_memory(xclbin, (char **) &kernelbinary);
  if (n_i < 0) {
    printf("failed to load kernel from xclbin: %s\n", xclbin);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  size_t n = n_i;
  // Create the compute program from offline
  program = clCreateProgramWithBinary(context, 1, &device_id, &n,
                                      (const unsigned char **) &kernelbinary, &status, &err);
  if ((!program) || (err!=CL_SUCCESS)) {
    printf("ERROR: Failed to create compute program from binary %d!\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
#endif

  // Build the program executable
  //
  err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    size_t len;
    char buffer[2048];

    printf("ERROR: Failed to build program executable!\n");
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    printf("%s\n", buffer);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Create the compute kernel in the program we wish to run
  //
  kernel = clCreateKernel(program, "mmult", &err);
  if (!kernel || err != CL_SUCCESS)
  {
    printf("ERROR: Failed to create compute kernel!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Create the input and output arrays in device memory for our calculation
  //
  input_a = clCreateBuffer(context,  CL_MEM_READ_ONLY,  sizeof(int) * DATA_SIZE, NULL, NULL);
  input_b = clCreateBuffer(context,  CL_MEM_READ_ONLY,  sizeof(int) * DATA_SIZE, NULL, NULL);
  output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * DATA_SIZE, NULL, NULL);
  if (!input_a || !input_b || !output)
  {
    printf("ERROR: Failed to allocate device memory!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }    
    
  // Write our data set into the input array in device memory 
  //
  err = clEnqueueWriteBuffer(commands, input_a, CL_TRUE, 0, sizeof(int) * DATA_SIZE, a, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to write to source array a!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Write our data set into the input array in device memory 
  //
  err = clEnqueueWriteBuffer(commands, input_b, CL_TRUE, 0, sizeof(int) * DATA_SIZE, b, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to write to source array b!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
    
  // Set the arguments to our compute kernel
  //
  err = 0;
  err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_a);
  err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &input_b);
  err |= clSetKernelArg(kernel, 2, sizeof(cl_mem), &output);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to set kernel arguments! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Execute the kernel over the entire range of our 1d input data set
  // using the maximum number of work group items for this device
  //
  global[0] = MATRIX_RANK;
  global[1] = MATRIX_RANK;
  local[0] = MATRIX_RANK;
  local[1] = MATRIX_RANK;

  err = clEnqueueNDRangeKernel(commands, kernel, 2, NULL, 
                               (size_t*)&global, (size_t*)&local, 0, NULL, NULL);
  if (err)
  {
    printf("ERROR: Failed to execute kernel! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Wait for the command commands to get serviced before reading back results
  // *** workaround - use clWaitForEvents instead of clFinish ***
  //clFinish(commands);

  // Read back the results from the device to verify the output
  //
  cl_event readevent;
  err = clEnqueueReadBuffer( commands, output, CL_TRUE, 0, sizeof(int) * DATA_SIZE, results, 0, NULL, &readevent );  
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to read output array! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  clWaitForEvents(1, &readevent);
    
  printf("A\n");
  for (i=0;i<DATA_SIZE;i++) {
//    printf("%0.2f ",a[i]);
    printf("%x ",a[i]);
    if (((i+1) % 16) == 0)
      printf("\n");
  }
  printf("B\n");
  for (i=0;i<DATA_SIZE;i++) {
//    printf("%0.2f ",b[i]);
    printf("%x ",b[i]);
    if (((i+1) % 16) == 0)
      printf("\n");
  }
  printf("res\n");
  for (i=0;i<DATA_SIZE;i++) {
    //printf("%0.2f ",results[i]);
    printf("%x ",results[i]);
    if (((i+1) % 16) == 0)
      printf("\n");
  }
    
  // Validate our results
  //
  correct = 0;
  for(i = 0; i < DATA_SIZE; i++)
  {
    int row = i/MATRIX_RANK;
    int col = i%MATRIX_RANK;
    int running = 0;
    int index;
    for (index=0;index<MATRIX_RANK;index++) {
      int aIndex = row*MATRIX_RANK + index;
      int bIndex = col + index*MATRIX_RANK;
      running += a[aIndex] * b[bIndex];
    }
    sw_results[i] = running;
  }
    
  for (i = 0;i < DATA_SIZE; i++) 
    if(results[i] == sw_results[i])
      correct++;
  printf("Software\n");
  for (i=0;i<DATA_SIZE;i++) {
    //printf("%0.2f ",sw_results[i]);
    printf("%d ",sw_results[i]);
    if (((i+1) % 16) == 0)
      printf("\n");
  }
    
    
  // Print a brief summary detailing the results
  //
  printf("Computed '%d/%d' correct values!\n", correct, DATA_SIZE);
    
  // Shutdown and cleanup
  //
  clReleaseMemObject(input_a);
  clReleaseMemObject(input_b);
  clReleaseMemObject(output);
  clReleaseProgram(program);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(commands);
  clReleaseContext(context);

  if(correct == DATA_SIZE){
    printf("Test passed!\n");
    return EXIT_SUCCESS;
  }
  else{
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
}
