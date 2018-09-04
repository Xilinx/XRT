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

/// This file was modified from the Xilinx provided test-cl.c file ///

// This is the host code for the hello example
// If it runs correctly, "Hellow World" will be printed at the end of the run


#include <getopt.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fstream>
#include <CL/cl.h>
#include<iostream>
#include <ctype.h>

using namespace std;

#define LENGTH (20)


static void printHelp()
{
    std::cout << "usage: %s <bitstream>  [options] \n\n";
    std::cout << "  -d <index>\n";
    std::cout << "  using XILINX_OPENCL and XCL_PLATFORM environment variables\n";
    std::cout << "* Bitstream is required\n";
}

bool fexists(const char *filename) {
  std::ifstream ifile(filename);
  return (bool)ifile;
}


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
  cl_int err;                         // error code returned from api calls
  unsigned index = 0;
  cl_device_id device_id;             // compute device id
  char h_buf[LENGTH];                 // host memory for buffer
  std::string bitstreamFile;
  cl_platform_id platform_id;         // platform id
  cl_context context;                 // compute context
  cl_command_queue commands;          // compute command queue
  cl_program program;                 // compute program
  cl_kernel kernel;                   // compute kernel

  cl_mem d_buf;                       // device memory for buffer
  size_t global[1];
  size_t local[1];


  // Trying to identify one platform:

  cl_uint num_platforms;


    if (argc < 2) {
        std::cout << "Error: No bitstream specified\n";
        printHelp();
        return -1;
    }
    if (argc == 4) {
      if (strcmp(argv[2], "-d") != 0) {
        cout << "Invalid option \n";
        printHelp();
        return -1;
     } else {
       if (!isdigit(*argv[3])) {
         cout << "Error: Device index should be an integer\n";
         return EXIT_FAILURE;
         }
      }
        index = std::atoi(argv[3]) ;
   } else { 
      if (argc == 3 || argc > 4) {
        cout << "Invalid option \n";
        printHelp();
        return -1;
    }
}

unsigned char *kernelbinary;
char *xclbin=argv[1];

bool T;
T = fexists(xclbin);
  if (T != 1) {
    printf("Error: argv[1] must be xclbin file!\n");
    return EXIT_FAILURE;
  }


// Fill our data sets with pattern
 int i = 0;
  for(i = 0; i < LENGTH; i++) {
    h_buf[i] = 0;
  }

  // Connect to first platform
  //

  err = clGetPlatformIDs(1, &platform_id, &num_platforms);

  if (err != CL_SUCCESS) {
    printf("Error: Failed to get a platform id!\n");
    return EXIT_FAILURE;
  }


  // Trying to query platform specific information...

  size_t returned_size = 0;
  cl_char platform_name[1024] = {0}, platform_prof[1024] = {0}, platform_vers[1024] = {0}, platform_exts[1024] = {0};

  err  = clGetPlatformInfo(platform_id, CL_PLATFORM_NAME,       sizeof(platform_name), platform_name, &returned_size);
  err |= clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION,    sizeof(platform_vers), platform_vers, &returned_size);
  err |= clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE,    sizeof(platform_prof), platform_prof, &returned_size);
  err |= clGetPlatformInfo(platform_id, CL_PLATFORM_EXTENSIONS, sizeof(platform_exts), platform_exts, &returned_size);

  if (err != CL_SUCCESS) {
    printf("Error: Failed to get platform infor!\n");
    return EXIT_FAILURE;
  }

  printf("\nPlatform information\n");
  printf("Platform name:       %s\n", (char *)platform_name);
  printf("Platform version:    %s\n", (char *)platform_vers);
  printf("Platform profile:    %s\n", (char *)platform_prof);
  printf("Platform extensions: %s\n", ((char)platform_exts[0] != '\0') ? (char *)platform_exts : "NONE");

  // Get all available devices

  cl_uint num_devices;

   clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ACCELERATOR, 0, NULL, &num_devices);
   cl_device_id*  devices = (cl_device_id*) malloc(sizeof(cl_device_id) * num_devices);

  err = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ACCELERATOR, num_devices, devices, NULL);

  if (err != CL_SUCCESS) {
    printf("Failed to collect device list on this platform!\n");
    return EXIT_FAILURE;
  }

  printf("\nFound %d compute devices!:\n", num_devices);

   if (index >= num_devices) {
 cout << "Out of range index: " << index << " >= num_devices: " << num_devices << "\n";
        return EXIT_FAILURE;
  }

// Checking for availability of the required device
      device_id = devices[index];

  // We have a compute device of required type! Next, create a compute context on it.


  context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &err);
  if (!context) {
    printf("Error: Failed to create a compute context!\n");
    return EXIT_FAILURE;
  }

  // Creating a command queue for the selected device within context

  commands = clCreateCommandQueue(context, device_id, 0, &err);

  if (!commands) {
    printf("Error: Failed to create a command queue!\n");
    return EXIT_FAILURE;
  }


  printf("loading %s\n", xclbin);
  int n_i = load_file_to_memory(xclbin, (char **) &kernelbinary);
  if (n_i < 0) {
    printf("failed to load kernel from xclbin: %s\n", xclbin);
    printf("Test failed\n");
  }
  size_t n = n_i;

// Create the compute program from offline

program = clCreateProgramWithBinary(context, 1, &device_id, &n,
                                      (const unsigned char **) &kernelbinary, NULL, &err);

  if (!program) {
    printf("Error: Failed to create compute program!\n");
    return EXIT_FAILURE;
  }

  // Build the program executable


  err = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);

  if (err != CL_SUCCESS) {
    size_t len;
    char buffer[2048];
    printf("Error: Failed to build program executable!\n");

    // See page 98...
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
 printf("%s\n", buffer);
    exit(1);
  }

  // Create the compute kernel object in the program we wish to run

 kernel = clCreateKernel(program, "hello", &err);
  if (!kernel || err != CL_SUCCESS)
  {
    printf("Error: Failed to create compute kernel!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Create the input and output arrays in device memory for our calculation
  //
  d_buf = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(char) * LENGTH, NULL, NULL);
  if (!d_buf)
  {
    printf("Error: Failed to allocate device memory!\n");
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Set the arguments to our compute kernel
  //
  err = 0;
  err = clSetKernelArg(kernel, 0, sizeof(cl_mem), &d_buf);
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to set kernel arguments! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  // Execute the kernel over the entire range of our 1d input data set
  // using the maximum number of work group items for this device
  //

#ifdef C_KERNEL
  err = clEnqueueTask(commands, kernel, 0, NULL, NULL);
#else
  global[0] = 1;
  local[0] = 1;
  err = clEnqueueNDRangeKernel(commands, kernel, 1, NULL,
                               (size_t*)&global, (size_t*)&local, 0, NULL, NULL);
#endif
  if (err)
  {
    printf("Error: Failed to execute kernel! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }
// Read back the results from the device to verify the output
  //
  cl_event readevent;
  err = clEnqueueReadBuffer( commands, d_buf, CL_TRUE, 0, sizeof(char) * LENGTH, h_buf, 0, NULL, &readevent );
  if (err != CL_SUCCESS)
  {
    printf("Error: Failed to read output array! %d\n", err);
    printf("Test failed\n");
    return EXIT_FAILURE;
  }

  clWaitForEvents(1, &readevent);

  printf("\nRESULT:\n%s", &h_buf[0]);


  // Shutdown and cleanup
  //
  clReleaseMemObject(d_buf);
  clReleaseProgram(program);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(commands);
  clReleaseContext(context);

  return 0;
}

