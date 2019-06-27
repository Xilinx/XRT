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
#include <unistd.h>
#include <sys/mman.h>

#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>

#include "drm/drm.h"
#include "util.h"

namespace xoclutil {
    int openDevice(const char *device)
    {
        std::string dev("/dev/dri/renderD");
        char name[128];
        char desc[512];
        char date[128];
        // Let's stop after 32 devices for now.
        for (int i = 128; i < 160 ;i++) {
            std::string devName = dev;
            devName += std::to_string(i);
            int fd = open(devName.c_str(),  O_RDWR);
            if (fd < 0)
                continue;

            drm_version version;
            std::memset(&version, 0, sizeof(version));
            std::memset(name, 0, sizeof(name));
            std::memset(desc, 0, sizeof(desc));
            std::memset(date, 0, sizeof(date));
            version.name = name;
            version.name_len = sizeof(name);
            version.desc = desc;
            version.desc_len = sizeof(desc);
            version.date = date;
            version.date_len = sizeof(date);
            int result = ioctl(fd, DRM_IOCTL_VERSION, &version);
            close(fd);
            if (result < 0)
                continue;
            if (!std::strstr(version.name, device))
                continue;

            std::cout << version.name << '.';
            std::cout << version.version_major << '.' << version.version_minor << '.' << version.version_patchlevel << "\n";
            std::cout << version.desc << "\n";
            return open(devName.c_str(),  O_RDWR);
        }
    }

    TestBO::TestBO(const char *name, int dev, size_t size, void *userPtr) : mName(name),
                                                                            mDev(dev),
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

        drm_xocl_info_bo info = {mBO, 0, 0, 0};
        result = ioctl(mDev, DRM_IOCTL_XOCL_INFO_BO, &info);
        if (result)
            throw std::runtime_error("Could not query BO " + mName);
        mDevAddr = info.paddr;

        if (mMapped)
            return;

        try {
            drm_xocl_map_bo mapInfo = {mBO, 0, 0};
            result = ioctl(mDev, DRM_IOCTL_XOCL_MAP_BO, &mapInfo);
            if (result)
                throw std::runtime_error("Could not prepare map for BO " + mName);
            mMapped = (char *)mmap(0, mSize, PROT_READ | PROT_WRITE, MAP_SHARED, mDev, mapInfo.offset);
            if (mMapped == MAP_FAILED)
                throw std::runtime_error("Could not map BO " + mName);
        }
        catch (std::exception &ex) {
            drm_gem_close closeInfo = {mBO, 0};
            ioctl(mDev, DRM_IOCTL_GEM_CLOSE, &closeInfo);
            throw ex;
        }
    }

    TestBO::TestBO(const char *name, int dev, int bo) : mName(name),
                                                        mDev(dev),
                                                        mMapped(nullptr),
                                                        mBO(0xffffffff),
                                                        mSize(0),
                                                        mUserPtr(false) {
        drm_prime_handle primeInfo = {0xffffffff, 0, bo};
        int result = ioctl(mDev, DRM_IOCTL_PRIME_FD_TO_HANDLE, &primeInfo);
        if (result)
            throw std::runtime_error("Could not import BO " + mName);
        mBO = primeInfo.handle;

        try {
            drm_xocl_info_bo info = {mBO, 0, 0, 0};
            result = ioctl(mDev, DRM_IOCTL_XOCL_INFO_BO, &info);
            if (result)
                throw std::runtime_error("Could not query BO " + mName);

            mDevAddr = info.paddr;
            mSize = info.size;
            drm_xocl_map_bo mapInfo = {mBO, 0, 0};
            result = ioctl(mDev, DRM_IOCTL_XOCL_MAP_BO, &mapInfo);
            if (result)
                throw std::runtime_error("Could not prepare map for BO " + mName);
            mMapped = (char *)mmap(0, mSize, PROT_READ | PROT_WRITE, MAP_SHARED, mDev, mapInfo.offset);
            if (mMapped == MAP_FAILED)
                throw std::runtime_error("Could not map BO " + mName);
        }
        catch (std::exception &ex) {
            drm_gem_close closeInfo = {mBO, 0};
            ioctl(mDev, DRM_IOCTL_GEM_CLOSE, &closeInfo);
            throw ex;
        }
    }

    TestBO::TestBO(TestBO && rhs) : mName(std::move(rhs.mName)),
                                    mDev(rhs.mDev),
                                    mMapped(rhs.mMapped),
                                    mBO(rhs.mBO),
                                    mSize(rhs.mSize),
                                    mDevAddr(rhs.mDevAddr),
                                    mUserPtr(rhs.mUserPtr) {
        rhs.mMapped = nullptr;
        rhs.mBO = 0xffffffff;
        rhs.mSize = 0;
        rhs.mDevAddr = 0xffffffffffffffff;
    }

    TestBO::~TestBO() {
        if (mUserPtr)
            munmap(mMapped, mSize);
        if (mBO != 0xffffffff) {
            drm_gem_close closeInfo = {mBO, 0};
            ioctl(mDev, DRM_IOCTL_GEM_CLOSE, &closeInfo);
        }
    }

    void TestBO::pwrite(void *data, size_t size, size_t seek) {
        std::memcpy(mMapped + seek, data, size);
    }

    void TestBO::pread(void *data, size_t size, size_t skip) {
        std::memcpy(data, mMapped + skip, size);
    }

    void TestBO::sync(drm_xocl_sync_bo_dir dir, size_t size, size_t offset) const {
        drm_xocl_sync_bo syncInfo = {mBO, 0, size, offset, dir};
        int result = ioctl(mDev, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
        if (result)
            throw std::runtime_error("Could not sync BO " + mName);
    }

    int TestBO::wexport() const {
        drm_prime_handle primeInfo = {mBO, 0, -1};
        int result = ioctl(mDev, DRM_IOCTL_PRIME_HANDLE_TO_FD, &primeInfo);
        if (result)
            throw std::runtime_error("Could not export BO " + mName);
        return primeInfo.fd;
    }

    void TestBO::dump(std::ostream &os) {


    }
}


