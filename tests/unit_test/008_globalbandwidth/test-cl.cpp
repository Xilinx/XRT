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
#include <time.h>
#include "time_profile.c"

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

#define MODE_READ       1
#define MODE_WRITE      2
#define BURSTBUFFERSIZE 16192*4                   //measured in bytes
#define MAX_REPS        32

int main(int argc, char** argv)
{
  int err;                            // error code returned from api calls
     
  cl_uint *a;           
  cl_uint *b;                
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
  
  cl_mem input_a;
  cl_mem output_b;                      
  
  struct timespec clndrangestart,clndrangeend;
  struct timespec clgetplatformidstart,clgetplatformidend;
  struct timespec clcreatecontextstart,clcreatecontextend;
  struct timespec clcreatecommandqueuestart,clcreatecommandqueueend;
  struct timespec clcreateprogramwithbinarystart,clcreateprogramwithbinaryend;
  double clndrangeelapsed[MAX_REPS];
  double clgetplatformidelapsed;
  double clcreatecontextelapsed;
  double clcreatecommandqueueelapsed;
  double clcreateprogramwithbinaryelapsed;

  
  
  //Parameters and parameter checking
  //
  if (argc != 6){
    printf("test-cl.exe <inputfile> <bursts> <burstlengthinbytes> <burstlengthreps> -r | -rw | -w\n");
    return EXIT_FAILURE;
  }
 
  cl_uint mode=0;
  if(!strcmp(argv[5],"-r")) mode=MODE_READ;
  if(!strcmp(argv[5],"-w")) mode=MODE_WRITE;
  if(!strcmp(argv[5],"-rw")) mode=MODE_READ|MODE_WRITE;

  cl_uint bursts = atoi(argv[2]);
  cl_uint burstlength = atoi(argv[3]);
  cl_uint burstlengthin32bitwords=burstlength/sizeof(cl_uint);
  cl_uint reps = atoi(argv[4]);
  if(burstlength>BURSTBUFFERSIZE){
    printf("ERROR <burstlength> > %i\n",BURSTBUFFERSIZE);
    return EXIT_FAILURE;
  }
  if((burstlength%4)!=0){
    printf("ERROR <burstlengthinbytes> must be a multiple of 4 (32-bits)\n");
    return EXIT_FAILURE;
  }
  if(reps>MAX_REPS){
    printf("ERROR <burstlengthreps> <= %i\n",MAX_REPS);
    return EXIT_FAILURE;
  }
  if(exp2(reps-1)*burstlength>BURSTBUFFERSIZE){
    printf("ERROR <bursts>=%i <burstlength>=%i at max of <burstlengthreps>=%i <= %i\n",burstlength,reps,BURSTBUFFERSIZE);
    return EXIT_FAILURE;
  }

  

  printf("Mode %i Bursts %i Burstlength %i\n",mode,bursts,burstlength); 

  //Malloced data areas
  //
  a=(cl_uint *)malloc(bursts*burstlength);
  b=(cl_uint *)malloc(bursts*burstlength);

  if(a==NULL || b==NULL){
    printf("ERROR : cannot malloc source and destination \n");
  }
  
  {
    //Fill input
    for(size_t i = 0; i < bursts*burstlengthin32bitwords; i++) {
      a[i] = (int)i;
    }
  }
 
  // Connect to first platform
  //
  clock_gettime(CLOCK_MONOTONIC,&clgetplatformidstart);
  err = clGetPlatformIDs(1,&platform_id,NULL);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to find an OpenCL platform!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  clock_gettime(CLOCK_MONOTONIC,&clgetplatformidend);
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
  printf("Get %d devices\n", num_devices);
  cl_device_id * devices = (cl_device_id *)malloc(num_devices*sizeof(cl_device_id));
  err = clGetDeviceIDs(platform_id, fpga ? CL_DEVICE_TYPE_ACCELERATOR : CL_DEVICE_TYPE_CPU,
                       num_devices, devices, NULL);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to create a device group!\n");
    return EXIT_FAILURE;
  }

  clock_gettime(CLOCK_MONOTONIC,&clcreatecontextstart);
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

  clock_gettime(CLOCK_MONOTONIC,&clcreatecontextend);

  // Create a command commands
  //
  clock_gettime(CLOCK_MONOTONIC,&clcreatecommandqueuestart);
  commands = clCreateCommandQueue(context, device_id, 0, &err);
  if (!commands)
  {
    printf("ERROR: Failed to create a command commands!\n");
    printf("ERROR: code %i\n",err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  clock_gettime(CLOCK_MONOTONIC,&clcreatecommandqueueend);

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
  clock_gettime(CLOCK_MONOTONIC,&clcreateprogramwithbinarystart);
  program = clCreateProgramWithBinary(context, 1, &device_id, &n,
                                      (const unsigned char **) &kernelbinary, &status, &err);
  clock_gettime(CLOCK_MONOTONIC,&clcreateprogramwithbinaryend);
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
  kernel = clCreateKernel(program, "globalbandwidth", &err);
  if (!kernel || err != CL_SUCCESS)
  {
    printf("ERROR: Failed to create compute kernel!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Create the input and output_b arrays in device memory for our calculation
  //
  input_a = clCreateBuffer(context,  CL_MEM_READ_ONLY,  bursts*exp2(reps-1)*burstlength, NULL, NULL);
  output_b = clCreateBuffer(context, CL_MEM_WRITE_ONLY, bursts*exp2(reps-1)*burstlength, NULL, NULL);
  if (!input_a || !output_b)
  {
    printf("ERROR: Failed to allocate device memory!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }    
    
  // Write our data set into the input array in device memory 
  //
  err = clEnqueueWriteBuffer(commands, input_a, CL_TRUE, 0, bursts*burstlength, a, 0, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to write to source array a!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

   
  // Set the arguments to our compute kernel
  //
  err = 0;
  err  = clSetKernelArg(kernel, 0, sizeof(cl_mem), &input_a);
  err |= clSetKernelArg(kernel, 1, sizeof(cl_mem), &output_b);
  err |= clSetKernelArg(kernel, 2, sizeof(cl_uint), &bursts);
  err |= clSetKernelArg(kernel, 4, sizeof(cl_uint), &mode);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to set kernel arguments! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Execute the kernel over the entire range of our 1d input data set
  // using the maximum number of work group items for this device
  //
  global[0] = 1;
  global[1] = 1;
  local[0] = 1;
  local[1] = 1;

  cl_uint burstlengthin32bitwordsloop=burstlengthin32bitwords;
  for(unsigned int i=0;i<reps;i++){
    clSetKernelArg(kernel, 3, sizeof(cl_uint), &burstlengthin32bitwordsloop);

    clock_gettime(CLOCK_MONOTONIC,&clndrangestart);
    err = clEnqueueNDRangeKernel(commands, kernel, 2, NULL, 
        (size_t*)&global, (size_t*)&local, 0, NULL, NULL);
    if (err)
    {
      printf("ERROR: Failed to execute kernel! %d\n", err);
      printf("ERROR: Test failed\n");
      return EXIT_FAILURE;
    }
    clock_gettime(CLOCK_MONOTONIC,&clndrangeend);
    clndrangeelapsed[i] = time_elapsed(clndrangestart,clndrangeend);

    burstlengthin32bitwordsloop=burstlengthin32bitwordsloop * 2;
  }

  // Wait for the command commands to get serviced before reading back results
  // *** workaround - use clWaitForEvents instead of clFinish ***
  //clFinish(commands);

  // Read back the results from the device to verify the output_b
  //
  cl_event readevent;
  err = clEnqueueReadBuffer( commands, output_b, CL_TRUE, 0, bursts*burstlength, b, 0, NULL, &readevent );  
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to read output_b array! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  clWaitForEvents(1, &readevent);
   
  // Validate our results
  //
  correct = 0;
  for(size_t i = 0; i < bursts*burstlengthin32bitwords; i++)
  {
    if(a[i]==b[i]) correct++;
  }

   
  // Print a brief summary detailing the results
  //
    
  // Shutdown and cleanup
  //
  clReleaseMemObject(input_a);
  clReleaseMemObject(output_b);
  clReleaseProgram(program);
  clReleaseKernel(kernel);
  clReleaseCommandQueue(commands);
  clReleaseContext(context);


#if 0
  {
    clgetplatformidelapsed = time_elapsed(clgetplatformidstart,clgetplatformidend);
    clcreatecontextelapsed = time_elapsed(clcreatecontextstart,clcreatecontextend);
    clcreatecommandqueueelapsed = time_elapsed(clcreatecommandqueuestart,clcreatecommandqueueend);
    clcreateprogramwithbinaryelapsed = time_elapsed(clcreateprogramwithbinarystart,clcreateprogramwithbinaryend);

    printf("clgetplatformid              %f (ns)\n",clgetplatformidelapsed);
    printf("clcreatecontext              %f (ns)\n",clcreatecontextelapsed);
    printf("clcreatecommandqueue         %f (ns)\n",clcreatecommandqueueelapsed);
    printf("clcreateprogramwithbinary    %f (ns)\n",clcreateprogramwithbinaryelapsed);
  }
#endif  

  cl_uint burstlengthloop=burstlength;
  printf("Burst (B)  Total (KB)  Time (ns)       MB/sec\n");
  for(unsigned int i=0;i<reps;i++){ 
    if(mode==MODE_READ){
      printf("%08ld    %08ld    %012ld    %08.08f\n",
          burstlengthloop,(bursts*burstlengthloop)/1024,(long)clndrangeelapsed[i],
          ((((double)(1000000000.0*bursts*burstlengthloop))/1048576.0)/((double)clndrangeelapsed[i])));
    }
    if(mode==MODE_WRITE){
      printf("%08ld    %08ld    %012ld    %08.08f\n", 
          burstlengthloop,(bursts*burstlengthloop)/1024,(long)clndrangeelapsed[i],
          ((((double)(1000000000.0*bursts*burstlengthloop))/1048576.0)/((double)clndrangeelapsed[i])));
    }
    if(mode==(MODE_READ|MODE_WRITE)){
      printf("%08ld    %08ld    %012ld    %08.08f\n",
          burstlengthloop,(2*bursts*burstlengthloop)/1024,(long)clndrangeelapsed[i],
          ((((double)(1000000000.0*2.0*bursts*burstlengthloop))/1048576.0)/((double)clndrangeelapsed[i])));
    }
    burstlengthloop=burstlengthloop*2;
  }

  if(mode==(MODE_READ|MODE_WRITE)){
    printf("Copied '%d/%d' correct values!\n", correct, bursts*burstlengthin32bitwords);
		if (correct != bursts*burstlengthin32bitwords){
			printf("Test failed\n");
			return EXIT_FAILURE;
  }
  if((mode==MODE_READ || mode==MODE_WRITE || reps!=1) || 
      ((reps==1) && (mode==MODE_READ|MODE_WRITE) && (correct == bursts*burstlengthin32bitwords))){
    printf("Test passed!\n");
    return EXIT_SUCCESS;
  }
  else{
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
	}
}
