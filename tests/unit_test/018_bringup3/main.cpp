// Copyright 2014, Xilinx Inc.
// All rights reserved.

#include <getopt.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include "oclHelper.h"

/*
 * Advanced loopback test. The OpenCL kernel returns the data sent to it. Tests the full
 * system. Each workitem copies a 64 byte block of data and 128 workitems run in parallel (local size).
 * Only one invocation of kernel (clEnqueueNDRangeKernel) is issued over the full global range.
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
        cl_int err = clReleaseMemObject(mSequence1);
        checkStatus(err);
        err = clReleaseMemObject(mSequence2);
        checkStatus(err);
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
    int iteration = 5;
    int length = 800;
    size_t workGroupSize = 128;
    int blockSize = 64;
    bool verbose = false;
    // Commandline
    int c;
    while ((c = getopt_long(argc, argv, "d:k:i:l:v:h", long_options, &option_index)) != -1)
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

    KernelHostData hostData(length * blockSize * workGroupSize);

    oclSoftware software;
    std::memset(&software, 0, sizeof(oclSoftware));
    std::strcpy(software.mKernelName, "loopback");
    std::strcpy(software.mFileName, kernelFile.c_str());
    std::sprintf(software.mCompileOptions, "");

    getOclSoftware(software, hardware);

    try {
        KernelDeviceData deviceData(hostData, hardware.mContext);
        cl_mem seq1 = deviceData.getSequence1();
        cl_mem seq2 = deviceData.getSequence2();

//        std::cout << "Sequence1: " << hostData.getSequence1() << "\n";
//        std::cout << "Sequence2: " << hostData.getSequence2() << "\n";

        cl_int err = clSetKernelArg(software.mKernel, 0, sizeof(cl_mem), &seq1);
        checkStatus(err);

        err = clSetKernelArg(software.mKernel, 1, sizeof(cl_mem), &seq2);
        checkStatus(err);

        const size_t globalSize[1] = {length * workGroupSize};
        size_t *localSize = 0;

        std::cout << "Global size = " << *globalSize << "\n";
        if (deviceType == CL_DEVICE_TYPE_ACCELERATOR) {
            localSize = &workGroupSize;
            std::cout << "Local size = " << *localSize << "\n";
        }

        std::cout << "Total buffer size to move = " << hostData.getLength() / 1024 << " KB\n";

        for(int i = 0; i < iteration; i++)
        {
            // Here we start measurings host time for kernel execution
            Timer timer;
            err = clEnqueueNDRangeKernel(hardware.mQueue, software.mKernel, 1, 0,
                                         globalSize, localSize, 0, 0, 0);
            checkStatus(err);

            err = clFinish(hardware.mQueue);
            checkStatus(err);

            clEnqueueMapBuffer(hardware.mQueue, deviceData.getSequence1(), CL_TRUE, CL_MAP_READ, 0,
                               hostData.getLength(), 0, 0, 0, &err);

            double delay = timer.stop();
            checkStatus(err);
//            std::cout << "Sequence1: " << hostData.getSequence1() << "\n";
//            std::cout << "Sequence2: " << hostData.getSequence2() << "\n";

            if (hostData.compare()) {
                throw std::runtime_error("Incorrect data from kernel");
            }
            std::cout << "OpenCL kernel time: " << delay << " sec\n";
        }
    }
    catch (std::exception const& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
        std::cout << "FAILED TEST\n";
        return 1;
    }
 
    try{
      release(software);
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

