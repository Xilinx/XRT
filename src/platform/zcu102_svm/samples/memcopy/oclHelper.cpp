/*******************************************************************************
Vendor: Xilinx
Associated Filename: oclHelper.cpp
Purpose: SDAccel median filter example
Revision History: December 6, 2015

*******************************************************************************
Copyright (C) 2015 XILINX, Inc.

This file contains confidential and proprietary information of Xilinx, Inc. and
is protected under U.S. and international copyright and other intellectual
property laws.

DISCLAIMER
This disclaimer is not a license and does not grant any rights to the materials
distributed herewith. Except as otherwise provided in a valid license issued to
you by Xilinx, and to the maximum extent permitted by applicable law:
(1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL FAULTS, AND XILINX
HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY,
INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT, OR
FITNESS FOR ANY PARTICULAR PURPOSE; and (2) Xilinx shall not be liable (whether
in contract or tort, including negligence, or under any other theory of
liability) for any loss or damage of any kind or nature related to, arising under
or in connection with these materials, including for any direct, or any indirect,
special, incidental, or consequential loss or damage (including loss of data,
profits, goodwill, or any type of loss or damage suffered as a result of any
action brought by a third party) even if such damage or loss was reasonably
foreseeable or Xilinx had been advised of the possibility of the same.

CRITICAL APPLICATIONS
Xilinx products are not designed or intended to be fail-safe, or for use in any
application requiring fail-safe performance, such as life-support or safety
devices or systems, Class III medical devices, nuclear facilities, applications
related to the deployment of airbags, or any other applications that could lead
to death, personal injury, or severe property or environmental damage
(individually and collectively, "Critical Applications"). Customer assumes the
sole risk and liability of any use of Xilinx products in Critical Applications,
subject only to applicable laws and regulations governing limitations on product
liability.

THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE AT
ALL TIMES.

*******************************************************************************/

#include "oclHelper.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

//
// Load file to memory
//
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

//
// Get device version
//
static void getDeviceVersion(oclHardware& hardware)
{
    char versionString[512];
    size_t size = 0;
    cl_int err = clGetDeviceInfo (hardware.mDevice, CL_DEVICE_VERSION, 511, versionString, &size);
    if ( err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return;
    }

    unsigned major = 0;
    unsigned minor = 0;
    unsigned state = 0;
    for (size_t i = 0; i < size; i++) {
        if (versionString[i] == ' ') {
            state++;
            continue;
        }
        if (versionString[i] == '.') {
            state++;
            continue;
        }
        if (state == 0) {
            continue;
        }
        if (state == 1) {
            major *= 10;
            major += (versionString[i] - '0');
            continue;
        }
        if (state == 2 && versionString[i]) {
            minor *= 10;
            minor += (versionString[i] - '0');
            continue;
        }
        break;
    }
    hardware.mMajorVersion = major;
    hardware.mMinorVersion = minor;
}

//
// Get OCL hardware
//
oclHardware getOclHardware(cl_device_type type, const char *target_device)
{
    oclHardware hardware = {0, 0, 0, 0, 0, 0};
    cl_platform_id platforms[16] = { 0 };
    cl_device_id devices[16];
    cl_device_id device_id;
    char platformName[256];
    char deviceName[256];
    cl_uint platformCount = 0;
    cl_int err = clGetPlatformIDs(0, 0, &platformCount);
    err = clGetPlatformIDs(16, platforms, &platformCount);
    if (err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return hardware;
    }

    for (cl_uint i = 0; i < platformCount; i++) {
        err = clGetPlatformInfo(platforms[i], CL_PLATFORM_NAME, 256, platformName, 0);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return hardware;
        }

        if (strcmp(platformName, "Xilinx") != 0) {
            //skip non-Xilinx platform
            continue;
        }

        //std::cout << "INFO: Selected platform " << platformName << std::endl;
        
        cl_uint deviceCount = 0;
        err = clGetDeviceIDs(platforms[i], type, 16, devices, &deviceCount);
        if ((err != CL_SUCCESS) || (deviceCount == 0)) {
            continue;
        }

        //iterate all devices to find the target device
        cl_uint idev;
        for (idev=0; idev < deviceCount; idev++) {
            err = clGetDeviceInfo(devices[idev], CL_DEVICE_NAME, 256, deviceName, 0);
            if (err != CL_SUCCESS) {
                std::cout << oclErrorCode(err) << "\n";
                return hardware;
            }
            if (strcmp(deviceName, target_device) == 0) {
                device_id = devices[i];
                break;
            }            
        }
        if (idev == deviceCount) {
            std::cout << "ERROR: target device " << target_device << " not found " << std::endl;
            return hardware;
        }

        cl_context_properties contextData[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platforms[i], 0};
        cl_context context = clCreateContextFromType(contextData, type, 0, 0, &err);
        if (err != CL_SUCCESS) {
            continue;
        }
        cl_command_queue queue = clCreateCommandQueue(context, device_id, 0, &err);
        if (err != CL_SUCCESS) {
            std::cout << oclErrorCode(err) << "\n";
            return hardware;
        }
        
        hardware.mPlatform = platforms[i];
        hardware.mContext = context;
        hardware.mDevice = device_id;
        hardware.mQueue = queue;
        getDeviceVersion(hardware);
        std::cout << "Platform = " << platformName << "\n";
        std::cout << "Device = " << deviceName << "\n";
        std::cout << "OpenCL Version = " << hardware.mMajorVersion << '.' << hardware.mMinorVersion << "\n";
        return hardware;
    }
    return hardware;
}

//
// Get OCL software
//
int getOclSoftware(oclSoftware &software, const oclHardware &hardware)
{
    cl_device_type deviceType = CL_DEVICE_TYPE_DEFAULT;
    cl_int err = clGetDeviceInfo(hardware.mDevice, CL_DEVICE_TYPE, sizeof(deviceType), &deviceType, 0);
    if ( err != CL_SUCCESS) {
        std::cout << oclErrorCode(err) << "\n";
        return -1;
    }

    unsigned char *kernelCode = 0;
    std::cout << "Loading " << software.mFileName << "\n";

    int size = loadFile2Memory(software.mFileName, (char **) &kernelCode);
    if (size < 0) {
        std::cout << "Failed to load kernel\n";
        return -2;
    }

    if (deviceType == CL_DEVICE_TYPE_ACCELERATOR) {
        size_t n = size;
        software.mProgram = clCreateProgramWithBinary(hardware.mContext, 1, &hardware.mDevice, &n,
                                                      (const unsigned char **) &kernelCode, 0, &err);
    }
    else {
        software.mProgram = clCreateProgramWithSource(hardware.mContext, 1, (const char **)&kernelCode, 0, &err);
    }
    if (!software.mProgram || (err != CL_SUCCESS)) {
        std::cout << oclErrorCode(err) << "\n";
        return -3;
    }

    software.mKernel = clCreateKernel(software.mProgram, software.mKernelName, NULL);
    if (software.mKernel == 0) {
        std::cout << oclErrorCode(err) << "\n";
        return -4;
    }
    
    delete [] kernelCode;
    return 0;
}

//
// Release software and hardware
//
void release(oclSoftware& software)
{
    clReleaseKernel(software.mKernel);
    clReleaseProgram(software.mProgram);
}

void release(oclHardware& hardware)
{
    clReleaseCommandQueue(hardware.mQueue);
    clReleaseContext(hardware.mContext);
    if ((hardware.mMajorVersion >= 1) && (hardware.mMinorVersion > 1)) {
        // Only available in OpenCL >= 1.2   
        clReleaseDevice(hardware.mDevice);
    }
}

// 67d7842dbbe25473c3c32b93c0da8047785f30d78e8a024de1b57352245f9689
