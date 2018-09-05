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
  int results[DATA_SIZE];           // results returned from device
  int sw_results[DATA_SIZE];           // results returned from device
  unsigned int correct;               // number of correct results returned

  size_t global[2];                      // global domain size for our calculation
  size_t local[2];                       // local domain size for our calculation

  cl_device_id device_id;             // compute device id
  cl_context context;                 // compute context
  cl_command_queue commands;          // compute command queue
  cl_program program0;                // compute program

  cl_kernel input_stage;
  cl_kernel adder_stage;
  cl_kernel output_stage;

  cl_mem input;                       // device memory used for the input array
  cl_mem output;                      // device memory used for the output array
  cl_mem buf0,buf1;

  // Fill our data sets with pattern
  //
  int i = 0;
  unsigned int count = MATRIX_RANK;
  for(i = 0; i < DATA_SIZE; i++) {
    a[i] = (int)i;
    results[i] = 0;
  }

  // Connect to a compute device
  //
  int fpga = 0;
#if defined (FLOW_ZYNQ_HLS_BITSTREAM) || defined(FLOW_HLS_CSIM) || defined(FLOW_HLS_COSIM)
  fpga = 1;
#endif
  cl_uint num_devices = 0;
  err = clGetDeviceIDs(NULL, fpga ? CL_DEVICE_TYPE_ACCELERATOR : CL_DEVICE_TYPE_CPU,
                       0, NULL, &num_devices);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to create a device group!\n");
    return EXIT_FAILURE;
  }
  
  // Create a compute context 
  //
  printf("Get %d devices\n", num_devices);
  cl_device_id * devices = (cl_device_id *)malloc(num_devices*sizeof(cl_device_id));
  err = clGetDeviceIDs(NULL, fpga ? CL_DEVICE_TYPE_ACCELERATOR : CL_DEVICE_TYPE_CPU,
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

#if defined(FLOW_X86_64_ONLINE)
  unsigned char *kernelsrc;
  char *clsrc = argv[1]; ;
  printf("loading %s\n", clsrc);
  int n_i = load_file_to_memory(clsrc, (char **) &kernelsrc);
  if (n_i < 0) {
    printf("failed to load kernel from source: %s\n", clsrc);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Create the compute program from the source buffer
  //
  program0 = clCreateProgramWithSource(context, 1, (const char **) &kernelsrc, NULL, &err);
  if (!program0)
  {
    printf("ERROR: Failed to create compute program0!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
#else
  // Load binary from disk
  unsigned char *kernelbinary;

  //
  // stages
  //
  char *xclbin = argv[1];
  printf("loading %s\n", xclbin);
  int n_i = load_file_to_memory(xclbin, (char **) &kernelbinary);
  if (n_i < 0) {
    printf("failed to load kernel from xclbin: %s\n", xclbin);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  size_t n = n_i;
  // Create the compute program from offline
  program0 = clCreateProgramWithBinary(context, 1, &device_id, &n,
                                      (const unsigned char **) &kernelbinary, &status, &err);
  if ((!program0) || (err!=CL_SUCCESS)) {
    printf("ERROR: Failed to create compute program0 from binary %d!\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
#endif
  // Build the program executable
  //
  err = clBuildProgram(program0, 0, NULL, NULL, NULL, NULL);
  if (err != CL_SUCCESS)
  {
    size_t len;
    char buffer[2048];

    printf("ERROR: Failed to build program0 executable!\n");
    clGetProgramBuildInfo(program0, device_id, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    printf("%s\n", buffer);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Create the compute kernels in the program we wish to run
  //
  input_stage = clCreateKernel(program0, "input_stage", &err);
  if (!input_stage || err != CL_SUCCESS)
  {
    printf("ERROR: Failed to create compute kernel input_stage!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  adder_stage = clCreateKernel(program0, "adder_stage", &err);
  if (!adder_stage || err != CL_SUCCESS)
  {
    printf("ERROR: Failed to create compute kernel adder_stage!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  output_stage = clCreateKernel(program0, "output_stage", &err);
  if (!output_stage || err != CL_SUCCESS)
  {
    printf("ERROR: Failed to create compute kernel output_stage!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Create the input and output arrays in device memory for our calculation
  //
  input = clCreateBuffer(context,  CL_MEM_READ_ONLY,  sizeof(int) * DATA_SIZE, NULL, NULL);
  buf0 = clCreateBuffer(context,  CL_MEM_READ_WRITE,  sizeof(int) * DATA_SIZE, NULL, NULL);
  buf1 = clCreateBuffer(context,  CL_MEM_READ_WRITE,  sizeof(int) * DATA_SIZE, NULL, NULL);
  output = clCreateBuffer(context, CL_MEM_WRITE_ONLY, sizeof(int) * DATA_SIZE, NULL, NULL);
  if (!input || !output || !buf0 || !buf1)
  {
    printf("ERROR: Failed to allocate device memory!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Write our data set into the input array in device memory 
  //
  cl_event wr_evt;
  err = clEnqueueWriteBuffer(commands, input, CL_TRUE, 0,
                             sizeof(int) * DATA_SIZE, a, 0, NULL, &wr_evt);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to write to source array a!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Set the arguments to our compute kernels
  //
  err = 0;
  err  = clSetKernelArg(input_stage, 0, sizeof(cl_mem), &input);
  err |= clSetKernelArg(input_stage, 1, sizeof(cl_mem), &buf0);
  err |= clSetKernelArg(adder_stage, 0, sizeof(cl_mem), &buf0);
  err |= clSetKernelArg(adder_stage, 1, sizeof(cl_mem), &buf1);
  err |= clSetKernelArg(output_stage, 0, sizeof(cl_mem), &buf1);
  err |= clSetKernelArg(output_stage, 1, sizeof(cl_mem), &output);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to set kernel arguments! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Execute the kernel over the entire range of our 1d input data set
  // using the maximum number of work group items for this device
  //
  global[0] = DATA_SIZE;
  local[0] = DATA_SIZE;

  cl_event input_evt;
  err = clEnqueueNDRangeKernel(commands, input_stage, 1, NULL,
                               (size_t*)&global, (size_t*)&local, 1, &wr_evt, &input_evt);
  if (err)
  {
    printf("ERROR: Failed to execute kernel! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  cl_event adder_evt;
  err = clEnqueueNDRangeKernel(commands, adder_stage, 1, NULL,
                               (size_t*)&global, (size_t*)&local, 1, &input_evt, &adder_evt);
  if (err)
  {
    printf("ERROR: Failed to execute kernel! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  cl_event output_evt;
  err = clEnqueueNDRangeKernel(commands, output_stage, 1, NULL,
                               (size_t*)&global, (size_t*)&local, 1, &adder_evt, &output_evt);
  if (err)
  {
    printf("ERROR: Failed to execute kernel! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  // Read back the results from the device to verify the output
  //
  cl_event rd_evt;
  err = clEnqueueReadBuffer(commands, output, CL_TRUE, 0,
                            sizeof(int) * DATA_SIZE, results, 1, &output_evt, &rd_evt);
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to read output array! %d\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }

  clWaitForEvents(1, &rd_evt);

  printf("A\n");
  for (i=0; i<DATA_SIZE; i++) {
    printf("%d ",a[i]);
    if (((i+1) % 16) == 0)
      printf("\n");
  }
  printf("res\n");
  for (i=0; i<DATA_SIZE; i++) {
    printf("%d ", results[i]);
    if (((i+1) % 16) == 0)
      printf("\n");
  }

  // Validate our results
  //
  correct = 0;
  for(i = 0; i < DATA_SIZE; i++)
  {
    sw_results[i] = (a[i]&0x0f0f0f0f) + 1000;
    if (!(i % 2))
      sw_results[i]++;
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
//  clReleaseMemObject(input);
//  clReleaseMemObject(output);
//  clReleaseMemObject(output);
//  clReleaseProgram(program);
//  clReleaseKernel(kernel);
//  clReleaseCommandQueue(commands);
//  clReleaseContext(context);

  if(correct == DATA_SIZE){
    printf("Test passed!\n");
    return EXIT_SUCCESS;
  }
  else{
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
}
