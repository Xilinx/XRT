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

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <cassert>
#include "drm/drm.h"
#include "xocl_ioctl.h"
#include "util.h"

/**
 * Sanity test case to validate UNMGD (unmanaged) DMA operations
 * Performs simple read/write and sync operations.
 * Compile command:
 * g++ -g -std=c++11 -I ../../include -I ../../drm/xocl xclgem8.cpp util.cpp
 */


int main(int argc, char *argv[])
{
    const char *dev = "xocl";
    unsigned kind = 0;
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [xocl]\n";
        return 1;
    }

    if (argc == 2) {
        dev = argv[1];
        if (std::strcmp(dev, "xocl")) {
            std::cerr << "Usage: " << argv[0] << " [zocl]\n";
            return 1;
        }
    }

    int fd = xoclutil::openDevice(dev);

    if (fd < 0) {
        return -1;
    }

    std::cout << "CREATE" << std::endl;
    drm_xocl_create_bo info1 = {1024, 0xffffffff, 0};
    int result = ioctl(fd, DRM_IOCTL_XOCL_CREATE_BO, &info1);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info1.handle << std::endl;

    char *userptr = 0;
    posix_memalign((void **)&userptr, 4096, 8192);
    assert(userptr);
    std::cout << "User Pointer " << (void *)userptr << std::endl;

    drm_xocl_userptr_bo info4 = {reinterpret_cast<uint64_t>(userptr), 8192, 0xffffffff, 2};
    result = ioctl(fd, DRM_IOCTL_XOCL_USERPTR_BO, &info4);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info4.handle << std::endl;

    std::cout << "INFO" << std::endl;
    std::cout << "BO1" << std::endl;
    drm_xocl_info_bo infoInfo1 = {info1.handle, 0, 0, 0};
    result = ioctl(fd, DRM_IOCTL_XOCL_INFO_BO, &infoInfo1);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info1.handle << std::endl;
    std::cout << "Size " << infoInfo1.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo1.paddr << std::dec << std::endl;

    drm_xocl_info_bo infoInfo4 = {info4.handle, 0, 0};
    std::cout << "BO4" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_INFO_BO, &infoInfo4);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info4.handle << std::endl;
    std::cout << "Size " << infoInfo4.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo4.paddr << std::dec << std::endl;

    char *bufferA = new char[1024];
    char *bufferD = new char[8192];
    char *bufferCheckA = new char[8192];
    char *bufferCheckD = new char[8192];

    std::cout << "PWRITE" << std::endl;
    std::cout << "BO1" << std::endl;
    std::memset(bufferA, 'a', 1024);
    drm_xocl_pwrite_bo pwriteInfo1 = {info1.handle, 0, 0, 1024, reinterpret_cast<uint64_t>(bufferA)};
    result = ioctl(fd, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfo1);
    std::cout << "result = " << result << std::endl;

    result = 0;
    std::memset(bufferD, 'd', 2048);
    std::cout << "BO4" << std::endl;
    std::memcpy(userptr, bufferD, 4200);
    std::cout << "result = " << result << std::endl;

    std::cout << "SYNC" << std::endl;
    std::cout << "BO1" << std::endl;
    drm_xocl_sync_bo syncInfo = {info1.handle, 0, info1.size, 0, DRM_XOCL_SYNC_BO_TO_DEVICE};
    result = ioctl(fd, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "BO4" << std::endl;
    syncInfo.handle = info4.handle;
    syncInfo.size = info4.size;
    result = ioctl(fd, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "UNMGD/COMPARE" << std::endl;
    drm_xocl_pread_unmgd unmgd1 = {0, 0, infoInfo1.paddr, info1.size, reinterpret_cast<uint64_t>(bufferCheckA)};
    result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_UNMGD, &unmgd1);
    std::cout << "result = " << result << std::endl;

    drm_xocl_pread_unmgd unmgd4 = {0, 0, infoInfo4.paddr, info4.size, reinterpret_cast<uint64_t>(bufferCheckD)};
    result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_UNMGD, &unmgd4);
    std::cout << "result = " << result << std::endl;

    std::cout << "COMPARE" << std::endl;
    std::cout << "BO1" << std::endl;
    result = std::memcmp(bufferA, bufferCheckA, 1024);
    std::cout << "result = " << result << std::endl;

    std::cout << "BO4" << std::endl;
    result = std::memcmp(bufferD, bufferCheckD, 8192);
    std::cout << "result = " << result << std::endl;

    std::cout << "CLOSE" << std::endl;
    std::cout << "BO1" << std::endl;
    drm_gem_close closeInfo = {info1.handle, 0};
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "BO4" << std::endl;
    closeInfo.handle = info4.handle;
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    delete [] bufferCheckA;
    delete [] bufferCheckD;
    delete [] bufferA;
    delete [] bufferD;

    result = close(fd);
    std::cout << "result = " << result << std::endl;

    return result;
}


