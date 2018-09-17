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

#include <getopt.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include "oclHelper.h"


const static struct option long_options[] = {
    {"device",      required_argument, 0, 'd'},
    {"kernel",      required_argument, 0, 'k'},
    {"platform",    required_argument, 0, 'p'},
    {"iteration",   required_argument, 0, 'i'},
    {"help",        no_argument      , 0, 'h'},
    {0, 0, 0, 0}
};

struct KernelHostData {
    float *mStock;
    float *mStrike;
    float *mTime;
    float *mCall;
    float *mPut;
    int mCount;

    void fillRandom(float *buffer, float rangeStart, float rangeEnd) {
        for (int i = 0; i < mCount; i++) {
            mStock[i] = rangeStart + (std::rand()/RAND_MAX) * (rangeEnd - rangeStart);
        }
    }

    void fillZeros(float *buffer) {
        std::memset(buffer, 0, mCount * 4);
    }

    void init(int count) {
        mCount = count;
        mStock = new float[count];
        mStrike = new float[count];
        mTime = new float[count];
        mCall = new float[count];
        mPut = new float[count];
        fillRandom(mStock, 10.0, 50.0);
        fillRandom(mStrike, 10.0, 50.0);
        fillRandom(mTime, 0.2, 2.0);
        fillZeros(mCall);
        fillZeros(mPut);
    }
};

struct KernelDeviceData {
    cl_mem mStock;
    cl_mem mStrike;
    cl_mem mTime;
    cl_mem mCall;
    cl_mem mPut;
    int init(KernelHostData &host, cl_context context) {
        cl_int err = 0;
        mStock = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, host.mCount * 4, host.mStock, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }
        mStrike = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, host.mCount * 4, host.mStrike, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }

        mTime = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, host.mCount * 4, host.mTime, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }

        mCall = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, host.mCount * 4, host.mCall, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }

        mPut = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, host.mCount * 4, host.mPut, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }
        return 0;
    }
};

class Timer {
    time_t mTimeStart;
    time_t mTimeEnd;
public:
    Timer() {
        mTimeStart = std::time(0);
        mTimeEnd = mTimeStart;
    }
    double stop() {
        mTimeEnd = std::time(0);
        return std::difftime(mTimeEnd, mTimeStart);
    }
    void reset() {
        mTimeStart = time(0);
        mTimeEnd = mTimeStart;
    }
};


static void printHelp()
{
    std::cout << "usage: %s <options>\n";
    std::cout << "  -p <platform>\n";
    std::cout << "  -d <device>\n";
    std::cout << "  -k <kernel_file> \n";
    std::cout << "  -i <iteration_count>\n";
    std::cout << "  -h\n";
}

int main(int argc, char** argv)
{
    cl_device_type deviceType = CL_DEVICE_TYPE_ACCELERATOR;;
    int option_index = 0;
    std::string kernelFile("kernel.cl");
    int iteration = 5;
    int count = 65536/8;
    size_t workGroupSize = 256;

    // Commandline
    int c;
    while ((c = getopt_long(argc, argv, "d:k:i:h", long_options, &option_index)) != -1)
    {
        switch (c)
        {
        case 0:
            if (long_options[option_index].flag != 0)
                break;
        case 'd':
            if (strcmp(optarg, "gpu") == 0)
                deviceType = CL_DEVICE_TYPE_GPU;
            else if (strcmp(optarg, "cpu") == 0)
                deviceType = CL_DEVICE_TYPE_CPU;
            else if (strcmp(optarg, "acc") != 0) {
                std::cout << "Incorrect platform specified\n";
                printHelp();
                return -1;
            }
            break;
        case 'k':
            kernelFile = optarg;
            break;
        case 'i':
            iteration = atoi(optarg);
            break;
        case 'h':
            printHelp();
            return 0;
        default:
            printHelp();
            return 1;
        }
    }

    oclHardware hardware = getOclHardware(deviceType);
    if (!hardware.mQueue) {
        return -1;
    }

    oclSoftware software;
    std::memset(&software, 0, sizeof(oclSoftware));
    std::strcpy(software.mKernelName, "montecarlo");
    std::strcpy(software.mFileName, kernelFile.c_str());
    getOclSoftware(software, hardware);

    KernelHostData hostData;
    hostData.init(count);

    KernelDeviceData deviceData;
    deviceData.init(hostData, hardware.mContext);

    float riskFree = 0.05f;
    float sigma = 0.2f; // volatility
    std::cout << "Risk free rate = " << riskFree << "\n";
    std::cout << "Vvolatility = " << sigma << "\n";
    std::cout << "Number of options = " << count << "\n";

    cl_int err = clSetKernelArg(software.mKernel, 0, sizeof(cl_mem), &deviceData.mCall);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };
    err = clSetKernelArg(software.mKernel, 1, sizeof(cl_mem), &deviceData.mPut);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };
    err = clSetKernelArg(software.mKernel, 2, 4, &riskFree);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };
    err = clSetKernelArg(software.mKernel, 3, 4, &sigma);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };
    err = clSetKernelArg(software.mKernel, 4, sizeof(cl_mem), &deviceData.mStock);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };
    err = clSetKernelArg(software.mKernel, 5, sizeof(cl_mem), &deviceData.mStrike);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };
    err = clSetKernelArg(software.mKernel, 6, sizeof(cl_mem), &deviceData.mTime);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    };

    // Define ndrange iteration space: global and local sizes based on
    // parameters given by user

    size_t globalSize[1] = {count};
    size_t *localSize = 0;

    std::cout << "Global size = " << count << "\n";

    if(deviceType == CL_DEVICE_TYPE_ACCELERATOR) {
        localSize = &workGroupSize;
        std::cout << "Local size = " << *localSize << "\n";
    }

    for(int i = 0; i < iteration; i++)
    {
        // Here we start measurings host time for kernel execution
        Timer timer;
        err = clEnqueueNDRangeKernel(hardware.mQueue, software.mKernel, 1, 0,
                                     globalSize, localSize, 0, 0, 0);

        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        };

        err = clFinish(hardware.mQueue);

        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        };

        double delay = timer.stop();

        std::cout << "OpenCL kernel time: " << delay << " sec\n";
        std::cout << "OpenCL kernel performance: " << count/delay << " options per second\n";
    }
    return 0;
}
