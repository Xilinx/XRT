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


//#include "stdafx.h"
#include <string.h>
#include "CL/cl.h"
#include "utils.h"
//#include "utils.cpp"
#include<stdio.h>

cl_mem		g_inputBuffer = NULL;
cl_context	g_context = NULL;
cl_command_queue g_cmd_queue = NULL;
cl_program	g_program = NULL;
cl_kernel	g_kernel = NULL;

#if defined (FLOW_ZYNQ_HLS_BITSTREAM) || defined(FLOW_HLS_CSIM) || defined(FLOW_HLS_COSIM)
bool g_bRunOnPG = true;
#else
bool g_bRunOnPG = false;
#endif

//for perf. counters
//#include <Windows.h>
#define DWORD uint32_t
#define LONG uint32_t
#define LONGLONG uint64_t
typedef union _LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG  HighPart;
  };
  struct {
    DWORD LowPart;
    LONG  HighPart;
  } u;
  LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;
LARGE_INTEGER g_PerfFrequency;
LARGE_INTEGER g_PerformanceCountNDRangeStart;
LARGE_INTEGER g_PerformanceCountNDRangeStop;

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

void Cleanup_OpenCL()
{
    if( g_inputBuffer ) {clReleaseMemObject( g_inputBuffer ); g_inputBuffer = NULL;}
    if( g_kernel ) {clReleaseKernel( g_kernel ); g_kernel = NULL;}
    if( g_program ) {clReleaseProgram( g_program ); g_program = NULL;}
    if( g_cmd_queue ) {clReleaseCommandQueue( g_cmd_queue ); g_cmd_queue = NULL;}
    if( g_context ) {clReleaseContext( g_context ); g_context = NULL;}
}

int Setup_OpenCL( const char *program_source, cl_uint* alignment, const char *kernelfilename)
{
    cl_device_id devices[16];
    size_t cb;
    cl_uint size_ret = 0;
    cl_int err;

	if(g_bRunOnPG)
	{
		printf("Trying to run on a FPGA\n");
	}
	else
	{
		printf("Trying to run on a CPU \n");
	}

	cl_platform_id intel_platform_id = GetIntelOCLPlatform();
    if( intel_platform_id == NULL )
    {
        printf("ERROR: Failed to find Intel OpenCL platform.\n");
        return -1;
    }

    cl_context_properties context_properties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties)intel_platform_id, 0 };

    // create the OpenCL context on a CPU/PG 
	if(g_bRunOnPG)
	{
		g_context = clCreateContextFromType(context_properties, CL_DEVICE_TYPE_ACCELERATOR, NULL, NULL, NULL);
	}
	else
	{
		g_context = clCreateContextFromType(context_properties, CL_DEVICE_TYPE_CPU, NULL, NULL, NULL);
	}
    if (g_context == (cl_context)0)
        return -1;


    // get the list of CPU devices associated with context
    err = clGetContextInfo(g_context, CL_CONTEXT_DEVICES, 0, NULL, &cb);
    clGetContextInfo(g_context, CL_CONTEXT_DEVICES, cb, devices, NULL);

    if( alignment )
    {
        err = clGetDeviceInfo (devices[0],
            CL_DEVICE_MEM_BASE_ADDR_ALIGN ,
            sizeof(cl_uint),
            alignment,
            NULL);

        *alignment/=8; //in bytes
        printf("OpenCL data alignment is %d bytes.\n", *alignment);
    }

    g_cmd_queue = clCreateCommandQueue(g_context, devices[0], 0, NULL);
    if (g_cmd_queue == (cl_command_queue)0)
    {
        Cleanup_OpenCL();
        return -1;
    }



#if defined(FLOW_X86_64_ONLINE) || defined(FLOW_AMD_SDK_ONLINE) 
  unsigned char *kernelsrc;
  char *clsrc=(char *)kernelfilename;
  printf("loading %s\n", clsrc);
  int n_i = load_file_to_memory(clsrc, (char **) &kernelsrc);
  if (n_i < 0) {
    printf("failed to load kernel from source: %s\n", clsrc);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  // Create the compute program from the source buffer
  //
  g_program = clCreateProgramWithSource(g_context, 1, (const char **) &kernelsrc, NULL, &err);
  if (!g_program)
  {
    printf("ERROR: Failed to create compute program!\n");
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
#else
  // Load binary from disk
  unsigned char *kernelbinary;
  char *xclbin=(char *)kernelfilename;
  printf("loading %s\n", xclbin);
  int n_i = load_file_to_memory(xclbin, (char **) &kernelbinary);
  if (n_i < 0) {
    printf("failed to load kernel from xclbin: %s\n", xclbin);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
  size_t n = n_i;
  // Create the compute program from offline
  g_program = clCreateProgramWithBinary(g_context, 1, &(devices[0]), &n,
                                      (const unsigned char **) &kernelbinary, NULL , &err);
  if ((!g_program) || (err!=CL_SUCCESS)) {
    printf("ERROR: Failed to create compute program from binary %d!\n", err);
    printf("ERROR: Test failed\n");
    return EXIT_FAILURE;
  }
#endif



/*
#if 0
    g_program = clCreateProgramWithSource(g_context, 1, (const char**)&sources, NULL, NULL);
#else
    {
      // Load binary from disk
      unsigned char *kernelbinary;
      int n_i = load_file_to_memory("kernel.xclbin", (char **) &kernelbinary);
      if (n_i < 0) {
        printf("failed to load kernel\n");
        return EXIT_FAILURE;
      }
      size_t n = n_i;
      // Create the compute program from offline
      int status;
      g_program = clCreateProgramWithBinary(g_context, 1, devices, &n,
                                            (const unsigned char **) &kernelbinary, NULL, NULL);
    }
#endif
    if (g_program == (cl_program)0)
    {
        printf("ERROR: Failed to create Program with source...\n");
        Cleanup_OpenCL();
        free(sources);
        return -1;
    }
*/

    err = clBuildProgram(g_program, 0, NULL, NULL, NULL, NULL);
    if (err != CL_SUCCESS)
    {
        printf("ERROR: Failed to build program...\n");
        BuildFailLog(g_program, devices[0]);
        Cleanup_OpenCL();
        return -1;
    }

    g_kernel = clCreateKernel(g_program, "bitonicsort", NULL);
    if (g_kernel == (cl_kernel)0)
    {
        printf("ERROR: Failed to create kernel...\n");
        Cleanup_OpenCL();
        return -1;
    }

    return 0; // success...
}

void generateInput(cl_int* inputArray, size_t arraySize)
{
    const size_t minElement = 0;
    const size_t maxElement = arraySize + 1;

    srand(12345);

    // random initialization of input
    for (size_t i = 0; i < arraySize; ++i)
    {
        inputArray[i] = (cl_int)((float) (maxElement - minElement) * (rand() / (float) RAND_MAX));
    }
}

void ExecuteSortReference(cl_int* inputArray, cl_int arraySize, cl_bool sortAscending)
{
    cl_int numStages = 0;
    cl_uint temp;

    cl_int stage;
    cl_int passOfStage;

    for (temp = arraySize; temp > 1; temp >>= 1)
    {
        ++numStages;
    }

    for (stage = 0; stage < numStages; ++stage)
    {
        int dirMask = 1 << stage;

        for (passOfStage = 0; passOfStage < stage + 1; ++passOfStage)
        {
            const cl_uint shift = stage - passOfStage;
            const cl_uint distance = 1 << shift;
            const cl_uint lmask = distance - 1;

            for(int g_id=0; g_id < arraySize >> 1; g_id++)
            {
                cl_uint leftId = (( g_id >> shift ) << (shift + 1)) + (g_id & lmask);
                cl_uint rightId = leftId + distance;

                cl_uint left  = inputArray[leftId];
                cl_uint right = inputArray[rightId];

                cl_uint greater;
                cl_uint lesser;

                if(left > right)
                {
                    greater = left;
                    lesser  = right;
                }
                else
                {
                    greater = right;
                    lesser  = left;
                }

                cl_bool dir = sortAscending;
                if( ( g_id & dirMask) == dirMask )
                    dir = !dir;

                if(dir)
                {
                    inputArray[leftId]  = lesser;
                    inputArray[rightId] = greater;
                }
                else
                {
                    inputArray[leftId]  = greater;
                    inputArray[rightId] = lesser;
                }
            }
        }
    }
}

bool ExecuteSortKernel(cl_int* inputArray, cl_int arraySize, cl_uint sortAscending)
{
    cl_int err = CL_SUCCESS;
    cl_int numStages = 0;
    cl_uint temp;

    cl_int stage;
    cl_int passOfStage;

    //create OpenCL buffer using input array memory
    //g_inputBuffer = clCreateBuffer(g_context, CL_MEM_USE_HOST_PTR, sizeof(cl_int) * arraySize, inputArray, NULL);
    g_inputBuffer = clCreateBuffer(g_context, CL_MEM_READ_WRITE, sizeof(cl_int) * arraySize, NULL, NULL);
    if (g_inputBuffer == (cl_mem)0)
    {
        printf("ERROR: Failed to create input data Buffer\n");
        return false;
    }

    err = clEnqueueWriteBuffer(g_cmd_queue, g_inputBuffer, CL_TRUE, 0,
                               sizeof(cl_int) * arraySize, inputArray, 0, NULL, NULL);

    for (temp = arraySize; temp > 2; temp >>= 1)
    {
        numStages++;
    }

    err  = clSetKernelArg(g_kernel, 0, sizeof(cl_mem), (void *) &g_inputBuffer);
    err |= clSetKernelArg(g_kernel, 3, sizeof(cl_uint), (void *) &sortAscending);
    if (err != CL_SUCCESS)
    {
        printf("ERROR: Failed to set input kernel arguments\n");
        return false;
    }

    //QueryPerformanceCounter(&g_PerformanceCountNDRangeStart);
    for (stage = 0; stage < numStages; stage++)
    {
        if ( CL_SUCCESS != clSetKernelArg(g_kernel, 1, sizeof(cl_uint), (void *) &stage) )
            return false;

        for (passOfStage = stage; passOfStage >= 0; passOfStage--)
        {
            if ( CL_SUCCESS != clSetKernelArg(g_kernel, 2, sizeof(cl_uint), (void *) &passOfStage) )
                return false;

            // set work-item dimensions
            size_t gsz = arraySize / (2*4);
            size_t global_work_size[1] = { passOfStage ? gsz : gsz << 1 };	//number of quad items in input array
            size_t local_work_size[1]= { 4 };					//valid WG sizes are 1:1024

            // execute kernel
            if (CL_SUCCESS != clEnqueueNDRangeKernel(g_cmd_queue, g_kernel, 1, NULL, global_work_size, local_work_size, 0, NULL, NULL))
            {
                printf("ERROR: Failed to execute sorting kernel\n");
                return false;
            }
        }
    }

    //err = clFinish(g_cmd_queue);
    //QueryPerformanceCounter(&g_PerformanceCountNDRangeStop);

    // *** workaround - use clWaitForEvents instead of clFinish ***
    cl_event readevent;
    err = clEnqueueReadBuffer(g_cmd_queue, g_inputBuffer, CL_TRUE, 0, sizeof(cl_int) * arraySize, inputArray, 0, NULL, &readevent );  
  if (err != CL_SUCCESS)
  {
    printf("ERROR: Failed to read output array! %d\n", err);
    exit(1);
  }
#if 0
    void* tmp_ptr = NULL;
    tmp_ptr = clEnqueueMapBuffer(g_cmd_queue, g_inputBuffer, true, CL_MAP_READ, 0, sizeof(cl_int) * arraySize , 0, NULL, NULL, NULL);
	if(tmp_ptr!=inputArray)
	{
		printf("ERROR: clEnqueueMapBuffer failed to return original pointer\n");
		return false;
	}
#endif

    //err = clFinish(g_cmd_queue);
    clWaitForEvents(1, &readevent);

    //clEnqueueUnmapMemObject(g_cmd_queue, g_inputBuffer, tmp_ptr, 0, NULL, NULL);

    clReleaseMemObject(g_inputBuffer); g_inputBuffer = NULL;

    return true;
}

void Usage()
{
    printf("Usage: BitonicSort.exe [--h] [-s <arraySize>] [-d]\n");
    printf("  where, --h prints this message\n");
    printf("    <arraySize> is input/output array size\n");
    printf("    -d performs descending sort (default is ascending)\n");
    printf("    -g run on Processor Graphics\n");
    printf("    -k <kernelfile|xclbinfile>\n");
    exit(-1);
}

extern "C"
int main(int argc, char* argv[])
{
    cl_uint dev_alignment = 128;
    cl_bool sortAscending = true;
    cl_int arraySize = 1024;//4096;//(1 << 20);

    char *kernelfilename;

    int argn = 1;
    while (argn < argc)
    {

#define _T
        if (strcmp(argv[argn],"-k") == 0)
        {
            sortAscending = false;
            argn++;
            kernelfilename=argv[argn];
            argn++;
        }
        else if (strcmp(argv[argn], _T("--h")) == 0)
        {
            Usage();
        }
        else if (strcmp(argv[argn], _T("-s")) == 0)
        {
            if(++argn==argc)
                Usage();
            arraySize = atoi(argv[argn]);
            argn++;
        }
        else if (strcmp(argv[argn], _T("-d")) == 0)
        {
            sortAscending = false;
            argn++;
        }
        else if (strcmp(argv[argn], _T("-g")) == 0)
        {
            g_bRunOnPG = true;
            argn++;
        }
        else
        {
            argn++;
        }
    }
    if( argc < 2 )
    {
        printf("No command line arguments specified, using default values.\n");
    }

    //validate user input parameters, if any
    {
        cl_uint sz_log_b = 0;

        if( arraySize < 1024 )
        {
            printf("Input size should be no less than 1024!\n");
            return -1;
        }

        for (int temp = arraySize; temp>1; temp >>= 1)
            sz_log_b++;

        if( arraySize & ((1 << sz_log_b)-1)  )
        {
            printf("Input size should be (2^N)*4!\n");
            return -1;
        }
    }

    printf("Initializing OpenCL runtime...\n");

    //initialize Open CL objects (context, queue, etc.)
    if( 0 != Setup_OpenCL("kernel.cl", &dev_alignment, kernelfilename) )
        return -1;

    printf("Sort order is %s\n", sortAscending ? "ascending" : "descending" );
    printf("Input size is %d items\n", arraySize);
#define _aligned_malloc(A, B) malloc((A))
    cl_int* inputArray = (cl_int*)_aligned_malloc(sizeof(cl_int) * arraySize, dev_alignment);
    cl_int* refArray = (cl_int*)_aligned_malloc(sizeof(cl_int) * arraySize, dev_alignment);

    //random input
    generateInput(inputArray, arraySize);
    memcpy(refArray, inputArray, sizeof(cl_int) * arraySize);

    //sort input array in a given direction
    printf("Executing OpenCL kernel...\n");
    ExecuteSortKernel(inputArray, arraySize, sortAscending);

    printf("Executing reference...\n");
    ExecuteSortReference(refArray, arraySize, sortAscending);

    bool result = true;
    printf("Performing verification...\n");
    for (cl_int i = 0; i < arraySize; i++)
    {
        if (inputArray[i] != refArray[i])
        {
          printf("ERROR: [%d] %x !- %x\n", i, inputArray[i], refArray[i]);
            result = false;
            //break;
        }
    }

    if(!result)
    {
        printf("ERROR: Verification failed.\n");
    }
    else
    {
        printf("Verification succeeded.\n");
    }

    //retrieve perf. counter frequency
    //QueryPerformanceFrequency(&g_PerfFrequency);
    printf("NDRange perf. counter time %f ms.\n", 
        1000.0f*(float)(g_PerformanceCountNDRangeStop.QuadPart - g_PerformanceCountNDRangeStart.QuadPart)/(float)g_PerfFrequency.QuadPart);

    printf("Releasing resources...\n");
#define _aligned_free free
    _aligned_free( refArray );
    _aligned_free( inputArray );

    Cleanup_OpenCL();

    if(!result)
    {
      printf("ERROR: Test failed\n");
      return EXIT_FAILURE;
    }
    else
    {
      printf("Test passed!\n");
      return EXIT_SUCCESS;
    }

    return 0;
}

