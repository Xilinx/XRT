/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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

#include <getopt.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include "oclHelper.h"

/*
 * Basic test to verify host to device and device to host memory copy. Does not compile
 * or load the OpenCL kernel. Can be used to verify if the platform is functional.
 */


static void checkStatus(cl_int status)
{
    if (status != CL_SUCCESS) {
        throw std::runtime_error(oclErrorCode(status));
    }
}

class KernelHostData {
private:
    char *mSequence1;
    char *mSequence2;
    int mLength;

private:
    void fillData() {
        static const char repo[] = "ATCG";
        std::srand(std::time(0));
        int i = 0;
        for (; i < mLength - 1; i++) {
            const int index2 = std::rand() % (sizeof(repo) - 1);
            mSequence2[i] = repo[index2];
        }
        mSequence2[i] = '\0';
        std::memset(mSequence1, 0, mLength);
    }

public:
    KernelHostData(int length) : mLength(length) {
        mSequence1 = new char[mLength + 1]; // extra spaces for '\0' at end
        mSequence2 = new char[mLength + 1]; // extra spaces for '\0' at end
        fillData();
    }

    ~KernelHostData() {
        delete [] mSequence1;
        delete [] mSequence2;
    }

    int getLength() const {
        return mLength;
    }

    char *getSequence1() const {
        return mSequence1;
    }

    char *getSequence2() const {
        return mSequence2;
    }

    int compare() const {
        return std::memcmp(mSequence1, mSequence2, mLength);
    }
};

class KernelDeviceData {
private:
    cl_mem mSequence1;
    cl_mem mSequence2;

public:
    KernelDeviceData(const KernelHostData &host, cl_context context) {
        cl_int err = 0;
        mSequence1 = clCreateBuffer(context, CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR, host.getLength(), host.getSequence1(), &err);
        checkStatus(err);

        mSequence2 = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, host.getLength(), host.getSequence2(), &err);
        checkStatus(err);
    }

    ~KernelDeviceData() {
    }

    cl_mem getSequence1() const {
        return mSequence1;
    }

    cl_mem getSequence2() const {
        return mSequence2;
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

const static struct option long_options[] = {
    {"device",      required_argument, 0, 'd'},
    {"kernel",      required_argument, 0, 'k'},
    {"length",      optional_argument, 0, 'l'},
    {"iteration",   optional_argument, 0, 'i'},
    {"verbose",     no_argument,       0, 'v'},
    {"help",        no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

static void printHelp()
{
    std::cout << "usage: %s <options>\n";
    std::cout << "  -d <device>\n";
    std::cout << "  -k <kernel_file> \n";
    std::cout << "  -i <iteration_count>\n";
    std::cout << "  -l <sequence_length>\n";
    std::cout << "  -v\n";
    std::cout << "  -h\n";
}


int main(int argc, char** argv)
{
    cl_device_type deviceType = CL_DEVICE_TYPE_ACCELERATOR;;
    int option_index = 0;
    std::string kernelFile("kernel.cl");
    int iteration = 1;
    int length = 16;
    bool verbose = false;
    // Commandline
    int c;
    while ((c = getopt_long(argc, argv, "d:k:i:l:vh", long_options, &option_index)) != -1)
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
        case 'l':
            length = atoi(optarg);
            break;
        case 'h':
            printHelp();
            return 0;
        case 'v':
            verbose = true;
            break;
        default:
            printHelp();
            return 1;
        }
    }


    oclHardware hardware = getOclHardware(deviceType);
    if (!hardware.mQueue) {
        return -1;
    }

    KernelHostData hostData(length);

    oclSoftware software;
    std::memset(&software, 0, sizeof(oclSoftware));
    std::strcpy(software.mKernelName, "loopback");
    std::strcpy(software.mFileName, kernelFile.c_str());
    std::sprintf(software.mCompileOptions, "");
    
    getOclSoftware(software, hardware);

    try {
        std::cout << "Sequence1: " << hostData.getSequence1() << "\n";
        std::cout << "Sequence2: " << hostData.getSequence2() << "\n";
        cl_int err = CL_SUCCESS;
        cl_mem mSequence = clCreateBuffer(hardware.mContext, CL_MEM_READ_WRITE, hostData.getLength(), NULL, &err);

        err = clSetKernelArg(software.mKernel, 0, sizeof(cl_mem), &mSequence);
        checkStatus(err);

        //err = clSetKernelArg(software.mKernel, 1, sizeof(cl_mem), &seq2);
        //checkStatus(err);

        //err = clSetKernelArg(software.mKernel, 2, 4, &length);
        //checkStatus(err);

        err = clEnqueueWriteBuffer(hardware.mQueue, mSequence, CL_FALSE, 0, hostData.getLength(), hostData.getSequence2(), 0, NULL, NULL);
        checkStatus(err);
        if ((hardware.mMajorVersion >= 1) && (hardware.mMinorVersion > 1)) {
            // Use the OpenCL 1.2 API to force migration of buffer
            err = clEnqueueMigrateMemObjects(hardware.mQueue, 1, &mSequence, 0, 0, NULL, NULL);
            checkStatus(err);
        }
        err = clFinish(hardware.mQueue);
        checkStatus(err);

        err = clEnqueueReadBuffer(hardware.mQueue, mSequence, CL_FALSE, 0, hostData.getLength(), hostData.getSequence1(), 0, NULL, NULL);
        checkStatus(err);
        err = clFinish(hardware.mQueue);
        checkStatus(err);

        if (hostData.compare()) {
            throw std::runtime_error("Incorrect data from kernel");
        }
        release(hardware);
    }
    catch (std::exception const& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
        std::cout << "FAILED TEST\n";
        return 1;
    }
    std::cout << "PASSED TEST\n";
    return 0;
}

