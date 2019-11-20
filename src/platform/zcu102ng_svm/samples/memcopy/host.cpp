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




#include "oclHelper.h"
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <string.h>

//DATA_SIZE should be multiple of 4 as Kernel Code is using int4 vector datatype
//to read the operands from Global Memory. So every read/write to global memory
//will read 16 integers value.
#define DATA_SIZE 16

#define CLEAR(x) memset(&(x), 0, sizeof(x))

void checkErrorStatus(cl_int error, const char* message)
{
  if (error != CL_SUCCESS)
  {
    printf("%s\n", message) ;
    printf("%s\n", oclErrorCode(error)) ;
    exit(0) ;
  }
}

int main(int argc, char** argv)
{

  //OPENCL HOST CODE AREA START
  oclHardware hardware = getOclHardware(CL_DEVICE_TYPE_ACCELERATOR, "zcu102ng_svm");
  oclSoftware software;

  const char* xclbinFilename = argv[1] ;

  CLEAR(software);
  strcpy(software.mKernelName, "memcopy") ;
  strcpy(software.mFileName, xclbinFilename) ;
  strcpy(software.mCompileOptions, "-g -Wall") ;

  getOclSoftware(software, hardware) ;

  //Allocate Memory in Host memory
  std::vector<int> sw_results(DATA_SIZE);
  size_t vector_size_bytes = sizeof(int) * DATA_SIZE;

  //Allocate SVM Buffer
  int* source_in1        = (int*)clSVMAlloc(hardware.mContext, CL_MEM_WRITE_ONLY, vector_size_bytes, 4096);
  int* hw_results        = (int*)clSVMAlloc(hardware.mContext, CL_MEM_READ_ONLY, vector_size_bytes, 4096);

  clEnqueueSVMMap(hardware.mQueue, CL_FALSE, CL_MAP_READ, source_in1, vector_size_bytes, 0, NULL, NULL);
  clEnqueueSVMMap(hardware.mQueue, CL_FALSE, CL_MAP_READ, hw_results, vector_size_bytes, 0, NULL, NULL);


  // Create the test data and Software Result
  for(int i = 0 ; i < DATA_SIZE ; i++){
    //source_in1[i] = rand();
    source_in1[i] = i;
    sw_results[i] = source_in1[i];
    hw_results[i] = 0;
  }

  clEnqueueSVMUnmap(hardware.mQueue, source_in1, 0, NULL, NULL);
  clEnqueueSVMUnmap(hardware.mQueue, hw_results, 0, NULL, NULL);

  //Set the Kernel Arguments
  long size = DATA_SIZE/4;

  int nargs=0;
  clSetKernelArgSVMPointer(software.mKernel, nargs++, source_in1);
  clSetKernelArgSVMPointer(software.mKernel, nargs++, hw_results);
  clSetKernelArg(software.mKernel, nargs++, sizeof(long), &size);

  //Launch the Kernel

  // Define iteration space
  size_t globalSize[3] = { 1, 1, 1 } ;
  size_t localSize[3] = { 1, 1, 1} ;
  cl_event seq_complete ;

  // Actually start the kernels on the hardware
  int err = clEnqueueNDRangeKernel(hardware.mQueue,
			                         software.mKernel,
			                         1,
			                         NULL,
			                         globalSize,
			                         localSize,
			                         0,
			                         NULL,
			                         &seq_complete) ;

  checkErrorStatus(err, "Unable to enqueue NDRange") ;

  // Wait for kernel to finish
  clWaitForEvents(1, &seq_complete) ;

  //OPENCL HOST CODE AREA END

  clEnqueueSVMMap(hardware.mQueue, CL_FALSE, CL_MAP_READ, hw_results, vector_size_bytes, 0, NULL, NULL);

  // Compare the results of the Device to the simulation
  bool match = true;
  for (int i = 0 ; i < DATA_SIZE ; i++){
    if (hw_results[i] != sw_results[i]){
      std::cout << "Error: Result mismatch" << std::endl;
      std::cout << "i = " << i << " CPU result = " << sw_results[i]
        << " Device result = " << hw_results[i] << std::endl;
      match = false;
      break;
    }
  }

  clEnqueueSVMUnmap(hardware.mQueue, hw_results, 0, NULL, NULL);

  clSVMFree(hardware.mContext, source_in1);
  clSVMFree(hardware.mContext, hw_results);

  release(software);
  release(hardware);

  std::cout << "TEST " << (match ? "PASSED" : "FAILED") << std::endl;
  return (match ? EXIT_SUCCESS :  EXIT_FAILURE);
}
