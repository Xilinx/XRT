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


#include "oclHelper.h"
#include "svm.h"
#include <vector>
#include <iostream>
#include <stdlib.h>
#include <string.h>

#define NUM_NODES 16

#define CLEAR(x) memset(&(x), 0, sizeof(x))

void checkErrorStatus(cl_int error, const char* message)
{
	if (error != CL_SUCCESS)
	{
		printf("%s\n", message);
		printf("%s\n", oclErrorCode(error));
		exit(0);
	}
}

int main(int argc, char** argv)
{
	//OPENCL HOST CODE AREA START
	oclHardware hardware = getOclHardware(CL_DEVICE_TYPE_ACCELERATOR, "zcu102_svm");
	oclSoftware software;

	const char* xclbinFilename = argv[1];

	CLEAR(software);
	strcpy(software.mKernelName, "link_sum");
	strcpy(software.mFileName, xclbinFilename);
	strcpy(software.mCompileOptions, "-g -Wall");

	getOclSoftware(software, hardware);

	//Allocate Memory in Host memory
	std::vector<long> sw_results(NUM_NODES);

	//Allocate SVM Buffer and initial linked list
	long* hw_results = (long *)clSVMAlloc(hardware.mContext, CL_MEM_WRITE_ONLY, sizeof(long)*NUM_NODES, 4096);

	Node *head = nullptr, *p;
	// Initial head node
	head = (Node*)clSVMAlloc(hardware.mContext, CL_MEM_READ_ONLY, sizeof(Node), 4096);
	head->val = 0;
	sw_results[0] = 0;
	hw_results[0] = 0;

	// Initial other nodes
	p = head;
	for (int i = 1; i < NUM_NODES; i++) {
		p->next = (Node*)clSVMAlloc(hardware.mContext, CL_MEM_READ_ONLY, sizeof(Node), 4096);
		p->next->val = i;
		sw_results[i] = sw_results[i-1] + i;
		hw_results[i] = 0;
		p = p->next;
	}
	// Set end of the list
	p->next = nullptr;

	// Print list
	p = head;
	while(p) {
		std::cout << "This " << p << ", val " << p->val << ", next " << p->next << std::endl;
		p = p->next;
	}

	// Set kernel argument
	int nargs=0;
	clSetKernelArgSVMPointer(software.mKernel, nargs++, head);
	clSetKernelArgSVMPointer(software.mKernel, nargs++, hw_results);

	//Launch the Kernel

	// Define iteration space 
	size_t globalSize[3] = {1, 1, 1};
	size_t localSize[3] = {1, 1, 1};
	cl_event seq_complete;

	// Actually start the kernels on the hardware
	int err = clEnqueueNDRangeKernel(hardware.mQueue,
			software.mKernel,
			1,
			NULL,
			globalSize,
			localSize,
			0,
			NULL,
			&seq_complete);

	checkErrorStatus(err, "Unable to enqueue NDRange");

	// Wait for kernel to finish
	clWaitForEvents(1, &seq_complete);

	//OPENCL HOST CODE AREA END

	// Compare the results of the Device to the simulation
	bool match = true;
	for (int i = 0; i < NUM_NODES; i++){
		if (hw_results[i] != sw_results[i]){
			std::cout << "Error: Result mismatch" << std::endl;
			std::cout << "i = " << i << " CPU result = " << sw_results[i]
				<< " Device result = " << hw_results[i] << std::endl;
			match = false;
			break;
		}
	}

	// Free SVM buffer
	clSVMFree(hardware.mContext, hw_results);
	while (head) {
		p = head->next;
		clSVMFree(hardware.mContext, head);
		head = p;
	}

	release(software);
	release(hardware);

	std::cout << "TEST " << (match ? "PASSED" : "FAILED") << std::endl; 
	return (match ? EXIT_SUCCESS :  EXIT_FAILURE);
}

// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
