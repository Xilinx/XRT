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
 * Sanity test case which creates 3 regular and 1 userptr BOs
 * Performs simple read/write and sync operations.
 * Compile command:
 * g++ -g -std=c++11 -I ../../include -I ../../drm/xocl xclgem1.cpp util.cpp
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

    drm_xocl_create_bo info2 = {4200, 0xffffffff, 2};
    result = ioctl(fd, DRM_IOCTL_XOCL_CREATE_BO, &info2);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info2.handle << std::endl;

    drm_xocl_create_bo info3 = {4200, 0xffffffff, 0};
    result = ioctl(fd, DRM_IOCTL_XOCL_CREATE_BO, &info3);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info3.handle << std::endl;

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

    drm_xocl_info_bo infoInfo2 = {info2.handle, 0, 0};
    std::cout << "BO2" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_INFO_BO, &infoInfo2);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info2.handle << std::endl;
    std::cout << "Size " << infoInfo2.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo2.paddr << std::dec << std::endl;

    drm_xocl_info_bo infoInfo3 = {info3.handle, 0, 0};
    std::cout << "BO3" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_INFO_BO, &infoInfo3);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info3.handle << std::endl;
    std::cout << "Size " << infoInfo3.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo3.paddr << std::dec << std::endl;

    drm_xocl_info_bo infoInfo4 = {info4.handle, 0, 0};
    std::cout << "BO4" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_INFO_BO, &infoInfo4);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info4.handle << std::endl;
    std::cout << "Size " << infoInfo4.size << std::endl;
    std::cout << "Physical " << std::hex << infoInfo4.paddr << std::dec << std::endl;

    char *bufferA = new char[1024];
    char *bufferB = new char[4200];
    char *bufferC = new char[4200];
    char *bufferD = new char[8192];
    char *bufferCheck = new char[8192];

    std::cout << "PWRITE" << std::endl;
    std::cout << "BO1" << std::endl;
    std::memset(bufferA, 'a', 1024);
    drm_xocl_pwrite_bo pwriteInfo1 = {info1.handle, 0, 0, 1024, reinterpret_cast<uint64_t>(bufferA)};
    result = ioctl(fd, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfo1);
    std::cout << "result = " << result << std::endl;

    std::memset(bufferB, 'b', 2048);
    std::cout << "BO2" << std::endl;
    drm_xocl_pwrite_bo pwriteInfo2 = {info2.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferB)};
    result = ioctl(fd, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfo2);
    std::cout << "result = " << result << std::endl;

    std::memset(bufferC, 'c', 2048);
    std::cout << "BO3" << std::endl;
    drm_xocl_pwrite_bo pwriteInfo3 = {info3.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferC)};
    result = ioctl(fd, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfo3);
    std::cout << "result = " << result << std::endl;

    result = 0;
    std::memset(bufferD, 'd', 2048);
    std::cout << "BO4" << std::endl;
    std::memcpy(userptr, bufferD, 4200);
    std::cout << "result = " << result << std::endl;

    std::cout << "PREAD/COMPARE" << std::endl;
    std::cout << "BO1" << std::endl;
    drm_xocl_pread_bo preadInfo1 = {info1.handle, 0, 0, 1024, reinterpret_cast<uint64_t>(bufferCheck)};
    result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo1);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferA, bufferCheck, 1024);
    std::cout << "result = " << result << std::endl;

    drm_xocl_pread_bo preadInfo2 = {info2.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferCheck)};
    std::cout << "BO2" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo2);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferB, bufferCheck, 4200);
    std::cout << "result = " << result << std::endl;

    drm_xocl_pread_bo preadInfo3 = {info3.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferCheck)};
    std::cout << "BO3" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo3);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferC, bufferCheck, 4200);
    std::cout << "result = " << result << std::endl;

    std::cout << "MMAP" << std::endl;
    std::cout << "BO1" << std::endl;
    drm_xocl_map_bo mapInfo1 = {info1.handle, 0, 0};
    result = ioctl(fd, DRM_IOCTL_XOCL_MAP_BO, &mapInfo1);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info1.handle << std::endl;
    void *ptr1 = mmap(0, info1.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapInfo1.offset);
    std::cout << "Offset "  << std::hex << mapInfo1.offset << std::dec << std::endl;
    std::cout << "Pointer " << ptr1 << std::endl;

    drm_xocl_map_bo mapInfo2 = {info2.handle, 0, 0};
    std::cout << "BO2" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_MAP_BO, &mapInfo2);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info2.handle << std::endl;
    void *ptr2 = mmap(0, info2.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapInfo2.offset);
    std::cout << "Offset "  << std::hex << mapInfo2.offset << std::dec << std::endl;
    std::cout << "Pointer " << ptr2 << std::endl;

    drm_xocl_map_bo mapInfo3 = {info3.handle, 0, 0};
    std::cout << "BO3" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_MAP_BO, &mapInfo3);
    std::cout << "result = " << result << std::endl;
    std::cout << "Handle " << info3.handle << std::endl;
    void *ptr3 = mmap(0, info3.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapInfo3.offset);
    std::cout << "Offset "  << std::hex << mapInfo3.offset << std::dec << std::endl;
    std::cout << "Pointer " << ptr3 << std::endl;

    std::cout << "MMAP/COMPARE" << std::endl;
    std::cout << "BO1" << std::endl;
    result = std::memcmp(bufferA, ptr1, 1024);
    std::cout << "result = " << result << std::endl;

    result = std::memcmp(bufferB, ptr2, 4200);
    std::cout << "BO2" << std::endl;
    std::cout << "result = " << result << std::endl;

    result = std::memcmp(bufferC, ptr3, 4200);
    std::cout << "BO3" << std::endl;
    std::cout << "result = " << result << std::endl;

    result = std::memcmp(bufferD, userptr, 4200);
    std::cout << "BO4" << std::endl;
    std::cout << "result = " << result << std::endl;

    std::cout << "MMAP/UPDATE" << std::endl;

    std::memset(ptr1, 'p', 1024);
    std::memset(ptr2, 'q', 4200);
    std::memset(ptr3, 'r', 4200);
    std::memset(userptr, 's', 4200);

    std::memset(bufferA, 'p', 1024);
    std::memset(bufferB, 'q', 4200);
    std::memset(bufferC, 'r', 4200);
    std::memset(bufferD, 's', 4200);

    std::cout << "MUNMAP" << std::endl;
    std::cout << "BO1" << std::endl;
    result = munmap(ptr1, 1024);
    std::cout << "result = " << result << std::endl;

    result = munmap(ptr2, 4200);
    std::cout << "BO2" << std::endl;
    std::cout << "result = " << result << std::endl;

    result = munmap(ptr3, 4200);
    std::cout << "BO3" << std::endl;
    std::cout << "result = " << result << std::endl;

    std::cout << "PREAD/COMPARE" << std::endl;
    std::cout << "BO1" << std::endl;
    drm_xocl_pread_bo preadInfo11 = {info1.handle, 0, 0, 1024, reinterpret_cast<uint64_t>(bufferCheck)};
    result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo11);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferA, bufferCheck, 1024);
    std::cout << "result = " << result << std::endl;

    drm_xocl_pread_bo preadInfo22 = {info2.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferCheck)};
    std::cout << "BO2" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo22);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferB, bufferCheck, 4200);
    std::cout << "result = " << result << std::endl;

    drm_xocl_pread_bo preadInfo33 = {info3.handle, 0, 0, 4200, reinterpret_cast<uint64_t>(bufferCheck)};
    std::cout << "BO3" << std::endl;
    result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo33);
    std::cout << "result = " << result << std::endl;
    result = std::memcmp(bufferC, bufferCheck, 4200);
    std::cout << "result = " << result << std::endl;

    result = std::memcmp(bufferD, userptr, 4200);
    std::cout << "result = " << result << std::endl;

    delete [] bufferA;
    delete [] bufferB;
    delete [] bufferC;
    delete [] bufferD;
    delete [] bufferCheck;

    std::cout << "SYNC" << std::endl;
    std::cout << "BO1" << std::endl;
    drm_xocl_sync_bo syncInfo = {info1.handle, 0, info1.size, 0, DRM_XOCL_SYNC_BO_TO_DEVICE};
    result = ioctl(fd, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "BO2" << std::endl;
    syncInfo.handle = info2.handle;
    syncInfo.size = info2.size;
    result = ioctl(fd, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "BO3" << std::endl;
    syncInfo.handle = info3.handle;
    syncInfo.size = info3.size;
    result = ioctl(fd, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "BO4" << std::endl;
    syncInfo.handle = info4.handle;
    syncInfo.size = info4.size;
    result = ioctl(fd, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "CLOSE" << std::endl;
    std::cout << "BO1" << std::endl;
    drm_gem_close closeInfo = {info1.handle, 0};
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "BO2" << std::endl;
    closeInfo.handle = info2.handle;
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "BO3" << std::endl;
    closeInfo.handle = info3.handle;
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    std::cout << "BO4" << std::endl;
    closeInfo.handle = info4.handle;
    result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    std::cout << "result = " << result << std::endl;

    result = close(fd);
    std::cout << "result = " << result << std::endl;

    return result;
}


