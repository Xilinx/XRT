#include "oclHelper.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

static int loadFile2Memory(const char *filename, char **result)
{
    int size = 0;

    std::ifstream stream(filename, std::ifstream::binary);
    if (!stream) {
        return -1;
    }

    stream.seekg(0, stream.end);
    size = stream.tellg();
    stream.seekg(0, stream.beg);

    *result = new char[size + 1];
    stream.read(*result, size);
    if (!stream) {
        return -2;
    }
    stream.close();
    (*result)[size] = 0;
    return size;
}


static int compileProgram(const oclHardware &hardware, oclSoftware &software)
{
    cl_int err = clBuildProgram(software.mProgram, 1, &hardware.mDevice, software.mCompileOptions, 0, 0);
    if (err != CL_SUCCESS)
    {
        std::cout << oclErrorCode(err) << "\n";
        size_t size = 0;
        err = clGetProgramBuildInfo(software.mProgram, hardware.mDevice, CL_PROGRAM_BUILD_LOG, 0, 0, &size);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }

        std::vector<char> log(size + 1);
        err = clGetProgramBuildInfo(software.mProgram, hardware.mDevice, CL_PROGRAM_BUILD_LOG, size, &log[0], 0);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return -1;
        }

        std::cout << &log[0] << "\n";
        return -1;
    }

    software.mKernel = clCreateKernel(software.mProgram, software.mKernelName, NULL);
    if (software.mKernel == 0)
    {
        std::cout << oclErrorCode(err) << "\n";
        return -2;
    }
    return 0;
}


oclHardware getOclHardware(cl_device_type type)
{
    oclHardware hardware = {0, 0, 0, 0};
    cl_platform_id platforms[16] = { 0 };
    cl_device_id devices[16];
    char platformName[256];
    char deviceName[256];
    cl_uint platformCount = 0;
    cl_int err = clGetPlatformIDs(0, 0, &platformCount);
    err = clGetPlatformIDs(16, platforms, &platformCount);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return hardware;
    }

    for (int i = 0; i < platformCount; i++) {
        err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 256, platformName, 0);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return hardware;
        }
        cl_uint deviceCount = 0;
        err = clGetDeviceIDs(platforms[i], type, 16, devices, &deviceCount);
        if ((err != CL_SUCCESS) || (deviceCount == 0)) {
            continue;
        }

        err = clGetDeviceInfo(devices[0], CL_DEVICE_NAME, 256, deviceName, 0);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return hardware;
        }

        cl_context_properties contextData[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[i], 0};
        cl_context context = clCreateContextFromType(contextData, type, 0, 0, &err);
        if (err != CL_SUCCESS) {
            continue;
        }
        cl_command_queue queue = clCreateCommandQueue(context, devices[0], 0, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return hardware;
        }
        hardware.mPlatform = platforms[i];
        hardware.mContext = context;
        hardware.mDevice = devices[0];
        hardware.mQueue = queue;
        std::cout << "Platform = " << platformName << "\n";
        std::cout << "Device = " << deviceName << "\n";
        return hardware;
    }
    return hardware;
}

int getOclSoftware(oclSoftware &soft, const oclHardware &hardware)
{
    cl_device_type deviceType = CL_DEVICE_TYPE_DEFAULT;
    cl_int err = clGetDeviceInfo(hardware.mDevice, CL_DEVICE_TYPE, sizeof(deviceType), &deviceType, 0);
    if ( err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    }

    unsigned char *kernelCode = 0;
    std::cout << "Loading " << soft.mFileName << "\n";

    int size = loadFile2Memory(soft.mFileName, (char **) &kernelCode);
    if (size < 0) {
        std::cout << "Failed to load kernel\n";
        return -2;
    }

    if (deviceType == CL_DEVICE_TYPE_ACCELERATOR) {
        size_t n = size;
        soft.mProgram = clCreateProgramWithBinary(hardware.mContext, 1, &hardware.mDevice, &n,
                                                  (const unsigned char **) &kernelCode, 0, &err);
    }
    else {
        soft.mProgram = clCreateProgramWithSource(hardware.mContext, 1, (const char **)&kernelCode, 0, &err);
    }
    if (!soft.mProgram || (err != CL_SUCCESS)) {
        std::cout << oclErrorCode(err) << "\n";
        return -3;
    }

    int status = compileProgram(hardware, soft);
    delete [] kernelCode;
    return status;
}
