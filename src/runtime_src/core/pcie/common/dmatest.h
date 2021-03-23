/**
 * Copyright (C) 2015-2020 Xilinx, Inc
 *
 * PCIe DMA Test implementation
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

#ifndef DMATEST_H
#define DMATEST_H

#include <chrono>
#include <future>
#include <vector>
#include <cstring>
#include <iostream>

#include "xrt.h"
#include "core/common/memalign.h"
#include "core/common/unistd.h"
#include "core/common/error.h"

#include <boost/format.hpp>

namespace xcldev {
    class Timer {
        std::chrono::high_resolution_clock::time_point mTimeStart;
    public:
        Timer() {
            reset();
        }
        long long stop() {
            std::chrono::high_resolution_clock::time_point timeEnd = std::chrono::high_resolution_clock::now();
            return std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - mTimeStart).count();
        }
        void reset() {
            mTimeStart = std::chrono::high_resolution_clock::now();
        }
    };

    class DMARunner {
        // Ideally I would use a C++ BO object which hides this detail completely inside
        // but that feature is coming in a future release. For now use a poor man implementation.
        // DMARunner now uses xclAllocUserPtrBO() to allocate buffers. This reduces memory pressure on
        // Linux kernel which other wise tries very hard inside xocl to allocate and pin pages when
        // xlcAllocBO() is used may oops.
        using buffer_and_deleter = std::pair<xclBufferHandle, xrt_core::aligned_ptr_type>;
        std::vector<buffer_and_deleter> mBOList;
        xclDeviceHandle mHandle;
        size_t mSize;
        size_t mTotalSize;
        unsigned mFlags;
        char mPattern;

        int runSyncWorker(std::vector<buffer_and_deleter>::const_iterator b,
                          std::vector<buffer_and_deleter>::const_iterator e,
                          xclBOSyncDirection dir) const {
            int result = 0;
            while (b < e) {
                result = xclSyncBO(mHandle, b->first, dir, mSize, 0);
                if (result != 0) {
                    throw xrt_core::error(result, "DMA failed");
                    break;
                }
                ++b;
            }
            return result;
        }

        int runSync(xclBOSyncDirection dir, unsigned count) const {
            auto b = mBOList.begin();
            const auto e = mBOList.end();
            if (count == 1) {
                auto future0 = std::async(std::launch::async, &DMARunner::runSyncWorker, this, b, e, dir);
                return future0.get();
            }

            auto len = ((e - b) < count) ? 1 : (e - b)/count;
            const auto ajust_e = b + len * count;
            std::vector<std::future<int>> threads;
            while (b < ajust_e) {
                threads.push_back(std::async(std::launch::async, &DMARunner::runSyncWorker, this, b, b + len, dir));
                b += len;
            }

            int result = 0;
            std::for_each(threads.begin(), threads.end(), [&](std::future<int> &v) {result += v.get();});
            return result;
        }

        void clear() {
            //Clear out the host shadow buffer
            std::for_each(mBOList.begin(), mBOList.end(), [&](auto &v) {std::memset(v.second.get(), 0, mSize);});
        }

        int validate() const {
            std::vector<char> bufCmp(mSize, mPattern);
            for (const buffer_and_deleter &bo : mBOList) {
                if (!std::memcmp(bo.second.get(), bufCmp.data(), mSize))
                    continue;
                throw xrt_core::error(-EIO, "DMA test data integrity check failed.");
            }
            return 0;
        }

    public:
        DMARunner(xclDeviceHandle handle, size_t size, unsigned flags = 0, size_t totalSize = 0) :
                mHandle(handle),
                mSize(size),
                mTotalSize(totalSize ? totalSize : 0x100000000),
                mFlags(flags),
                mPattern('x') {
            long long count = mTotalSize / mSize;

            if (count == 0)
                throw xrt_core::error(-EINVAL,
                    boost::str(boost::format("DMA buffer size cannot be larger than %#x.") % mTotalSize));

            if (count > 0x40000)
                count = 0x40000;

            for (long long i = 0; i < count; i++) {
                // This can throw and callers of DMARunner are supposed to catch this.
                xrt_core::aligned_ptr_type buf = xrt_core::aligned_alloc(xrt_core::getpagesize(), mSize);
                xclBufferHandle bo = xclAllocUserPtrBO(mHandle, buf.get(), mSize, mFlags);
                if (bo == XRT_NULL_BO)
                    break;
                std::memset(buf.get(), mPattern, mSize);
                mBOList.emplace_back(bo, std::move(buf));
            }
            if (mBOList.size() == 0)
                throw xrt_core::error(-ENOMEM, "No DMA buffers could be allocated.");
        }

        ~DMARunner() {
            std::for_each(mBOList.begin(), mBOList.end(), [&](auto &bo) {xclFreeBO(mHandle, bo.first); });
        }

        int run(std::ostream& ostr = std::cout) const {
            xclDeviceInfo2 info;
            int rc = xclGetDeviceInfo2(mHandle, &info);
            if (rc)
                throw xrt_core::error(rc, "Unable to get device information.");

            if (info.mDMAThreads == 0)
                throw xrt_core::error(-EINVAL, "Unable to determine number of DMA channels.");

            size_t result = 0;
            Timer timer;
            result = runSync(XCL_BO_SYNC_BO_TO_DEVICE, info.mDMAThreads);
            if (result)
                throw xrt_core::error(static_cast<int>(result), "DMA from host to device failed.");

            auto timer_stop = timer.stop();
            double rate = static_cast<double>(mBOList.size() * mSize);
            rate /= 0x100000; // MB
            rate /= timer_stop;
            rate *= 1000000; // s
            ostr << boost::str(boost::format("Host -> PCIe -> FPGA write bandwidth = %f MB/s\n") % rate);

            timer.reset();
            result = runSync(XCL_BO_SYNC_BO_FROM_DEVICE, info.mDMAThreads);
            if (result)
                throw xrt_core::error(static_cast<int>(result), "DMA from device to host failed.");

            timer_stop = timer.stop();
            rate = static_cast<double>(mBOList.size() * mSize);
            rate /= 0x100000; // MB
            rate /= timer_stop;
            rate *= 1000000; //
            ostr << boost::str(boost::format("Host <- PCIe <- FPGA read bandwidth = %f MB/s\n") % rate);

            // data integrity check: compare with initialized pattern
            return validate();
        }
    };
}

#endif /* DMATEST_H */
