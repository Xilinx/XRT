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

#ifndef _XOCL_GEM_TEST_UTIL_H_
#define _XOCL_GEM_TEST_UTIL_H_

#include <string>
#include <numeric>

#include "xocl_ioctl.h"

namespace xoclutil {
    int openDevice(const char *device);

    class TestBO {
        const std::string mName;
        const int mDev;
        char *mMapped;
        unsigned mBO;
        size_t mSize;
        uint64_t mDevAddr;
        bool mUserPtr;

    public:
        TestBO(const char *name, int dev, size_t size, void *userPtr);
        TestBO(const char *name, int dev, int bo);
        TestBO(TestBO && rhs);
        virtual ~TestBO();

        void pwrite(void *data, size_t size, size_t seek = 0);
        void pread(void *data, size_t size, size_t skip = 0);
        void sync(drm_xocl_sync_bo_dir dir, size_t size, size_t offset = 0) const;

        size_t size() const {
            return mSize;
        }

        unsigned long long checksum() const {
            return std::accumulate(&mMapped[0], &mMapped[mSize], 0x0ull);
        }

        std::string name() const {
            return mName;
        }

        int wexport() const;

        void dump(std::ostream &os);
    };
}

#endif


