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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#include <numeric>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <cassert>
#include <random>

#include "drm/drm.h"
#include "xocl_ioctl.h"

/**
 * Sanity test case similar to xclgem1.cpp, but uses object oriented modeling of BO
 * Performs simple alloc, read/write ,sync and free operations.
 * Compile command: g++ -g -std=c++11 -I ../../include -I ../../drm/xocl xclgem5.cpp
 */

class TestBO {
    const std::string mName;
    const int mDev;
    char *mMapped;
    unsigned mBO;
    size_t mSize;
    bool mUserPtr;

public:
    TestBO(const char *name, int fd, size_t size, void *userPtr = 0) : mName(name),
                                                                       mDev(fd),
                                                                       mMapped(nullptr),
                                                                       mBO(0xffffffff),
                                                                       mSize(size),
                                                                       mUserPtr(userPtr != nullptr) {
        int result;
        if (userPtr) {
            drm_xocl_userptr_bo info = {reinterpret_cast<uint64_t>(userPtr), size, 0xffffffff, 0};
            result = ioctl(mDev, DRM_IOCTL_XOCL_USERPTR_BO, &info);
            mBO = info.handle;
            mMapped = (char *)userPtr;
        }
        else {
            drm_xocl_create_bo info = {size, 0xffffffff, 0};
            result = ioctl(mDev, DRM_IOCTL_XOCL_CREATE_BO, &info);
            mBO = info.handle;
        }
        if (result)
            throw std::runtime_error("Could not create BO " + mName);

        if (mMapped)
            return;

        drm_xocl_map_bo mapInfo = {mBO, 0, 0};
        result = ioctl(mDev, DRM_IOCTL_XOCL_MAP_BO, &mapInfo);
        mMapped = (char *)mmap(0, mSize, PROT_READ | PROT_WRITE, MAP_SHARED, mDev, mapInfo.offset);
        if (mMapped == MAP_FAILED)
            throw std::runtime_error("Could not map BO " + mName);
    }

    ~TestBO() {
        if (mUserPtr)
            munmap(mMapped, mSize);
        drm_gem_close closeInfo = {mBO, 0};
        ioctl(mDev, DRM_IOCTL_GEM_CLOSE, &closeInfo);
    }

    void pwrite(void *data, size_t size, size_t seek = 0) {
        std::memcpy(mMapped + seek, data, size);
    }

    void pread(void *data, size_t size, size_t skip = 0) {
        std::memcpy(data, mMapped + skip, size);
    }

    void sync(drm_xocl_sync_bo_dir dir, size_t size, size_t offset = 0) const {
        drm_xocl_sync_bo syncInfo = {mBO, 0, size, offset, dir};
        int result = ioctl(mDev, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
        if (result)
            throw std::runtime_error("Could not sync BO " + mName);
    }

    size_t size() const {
        return mSize;
    }

    unsigned long long checksum() const {
        return std::accumulate(&mMapped[0], &mMapped[mSize], 0x0ull);
    }

    std::string name() const {
        return mName;
    }
};

static int openDevice(const char *device)
{
    std::string dev("/dev/dri/renderD");
    char name[128];
    char desc[512];
    char date[128];
    for (int i = 129; ;i++) {
        std::string devName = dev;
        devName += std::to_string(i);
        int fd = open(devName.c_str(),  O_RDWR);
        if (fd < 0) {
            return fd;
        }
        drm_version version;
        std::memset(&version, 0, sizeof(version));
        std::memset(name, 0, 128);
        std::memset(desc, 0, 512);
        std::memset(date, 0, 128);
        version.name = name;
        version.name_len = 128;
        version.desc = desc;
        version.desc_len = 512;
        version.date = date;
        version.date_len = 128;
        int result = ioctl(fd, DRM_IOCTL_VERSION, &version);
        if (result < 0)
            continue;
        if (!std::strstr(version.name, device))
            continue;
        std::cout << version.name << "\n";
        std::cout << version.version_major << '.' << version.version_minor << '.' << version.version_patchlevel << "\n";
        std::cout << version.desc << "\n";
        return fd;
    }
}

static int runTest(int fd)
{
    std::cout << "CREATE" << std::endl;
    TestBO bo0("bo0", fd, 8192);
    TestBO bo1("bo1", fd, 4200);

    char *userptr = 0;
    posix_memalign((void **)&userptr, 4096, 8192);
    assert(userptr);
    TestBO bo2("bo2", fd, 8192, userptr);

    std::random_device r;
    // Choose a random mean between 1 and 254
    std::default_random_engine e1(r());
    std::uniform_int_distribution<int> uniform_dist(0, 255);
    int mean = uniform_dist(e1);

    char *buffer0 = new char[bo0.size()];
    std::memset(buffer0, mean, bo0.size());
    char *buffer1 = new char[bo1.size()];
    std::memset(buffer1, mean + 1, bo1.size());
    char *buffer2 = new char[bo2.size()];
    std::memset(buffer1, mean - 1, bo1.size());
    char *bufferCheck = new char[8192];
    std::memset(bufferCheck, 0, 8192);

    std::cout << "PWRITE (update hbuf)" << std::endl;
    bo0.pwrite(buffer0, bo0.size());
    unsigned long long c0 = bo0.checksum();
    bo1.pwrite(buffer1, bo1.size());
    unsigned long long c1 = bo1.checksum();
    bo2.pwrite(buffer2, bo2.size());
    unsigned long long c2 = bo2.checksum();

    std::cout << "SYNC TO DEVICE (update dbuf with hbuf)" << std::endl;
    bo0.sync(DRM_XOCL_SYNC_BO_TO_DEVICE, bo0.size());
    bo1.sync(DRM_XOCL_SYNC_BO_TO_DEVICE, bo1.size());
    bo2.sync(DRM_XOCL_SYNC_BO_TO_DEVICE, bo2.size());

    std::cout << "PWRITE (clear hbuf)" << std::endl;
    bo0.pwrite(bufferCheck, bo0.size());
    bo1.pwrite(bufferCheck, bo1.size());
    bo2.pwrite(bufferCheck, bo2.size());

    if (bo0.checksum() != 0)
        throw std::runtime_error("Could not clear BO " + bo0.name());
    if (bo1.checksum() != 0)
        throw std::runtime_error("Could not clear BO " + bo1.name());
    if (bo2.checksum() != 0)
        throw std::runtime_error("Could not clear BO " + bo2.name());

    std::cout << "SYNC FROM DEVICE (refresh hbuf from dbuf)" << std::endl;
    bo0.sync(DRM_XOCL_SYNC_BO_FROM_DEVICE, bo0.size());
    bo1.sync(DRM_XOCL_SYNC_BO_FROM_DEVICE, bo1.size());
    bo2.sync(DRM_XOCL_SYNC_BO_FROM_DEVICE, bo2.size());

    std::cout << "VALIDATE SYNC DATA" << std::endl;
    if (c0 != bo0.checksum())
        throw std::runtime_error("Inconsistent sync for BO " + bo0.name());
    if (c1 != bo1.checksum())
        throw std::runtime_error("Inconsistent sync for B1 " + bo1.name());
    if (c2 != bo2.checksum())
        throw std::runtime_error("Inconsistent sync for B2 " + bo2.name());

    return 0;
}

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

    int fd = openDevice(dev);

    if (fd < 0) {
        return -1;
    }

    int result = 0;
    try {
        result = runTest(fd);
        if (result == 0)
            std::cout << "PASSED TEST\n";
        else
            std::cout << "FAILED TEST\n";
    }
    catch (const std::exception& ex) {
        std::cout << ex.what() << std::endl;
        std::cout << "FAILED TEST\n";
    }

    close(fd);
    return result;
}


