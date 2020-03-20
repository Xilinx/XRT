/**
 * Copyright (C) 2015-2019 Xilinx, Inc
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
        std::vector<xclBufferHandle> mBOList;
        xclDeviceHandle mHandle;
        size_t mSize;
        unsigned mFlags;

        int runSyncWorker(std::vector<xclBufferHandle>::const_iterator b,
                          std::vector<xclBufferHandle>::const_iterator e,
                          xclBOSyncDirection dir) const {
            int result = 0;
            while (b < e) {
                result = xclSyncBO(mHandle, *b, dir, mSize, 0);
                if (result != 0) {
                    std::cout << "DMA failed with Error = " << result << "\n";
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
            std::vector<std::future<int>> threads;
            while (b < e) {
                threads.push_back(std::async(std::launch::async, &DMARunner::runSyncWorker, this, b, b + len, dir));
                b += len;
            }

            int result = 0;
            for_each(threads.begin(), threads.end(), [&](std::future<int> &v) {result += v.get();});
            return result;
        }

    public:
        DMARunner(xclDeviceHandle handle, size_t size, unsigned flags=0) : mHandle(handle), mSize(size), mFlags(flags) {
            long long count = 0x100000000/size;
            if (count > 0x40000)
                count = 0x40000;

            for (long long i = 0; i < count; i++) {
                auto bo = xclAllocBO(mHandle, mSize, 0, mFlags);
                if (bo == XRT_NULL_BO)
                    break;
                mBOList.push_back(bo);
            }
        }

        ~DMARunner() {
            for (auto bo : mBOList)
                xclFreeBO(mHandle, bo);
        }

        int validate(const char *buf) const {
            std::unique_ptr<char[]> bufCmp(new char[mSize]);
            size_t result = 0;
            for (auto i : mBOList) {
                //Clear out the host buffer
                std::memset(bufCmp.get(), 0, mSize);
                result = xclReadBO(mHandle, i, bufCmp.get(), mSize, 0);
                if (result) {
                    std::cout << "DMA Test data integrity read failed with Error = " << result << "\n";
                    break;
		}

                if (std::memcmp(buf, bufCmp.get(), mSize)) {
                    std::cout << "DMA Test data integrity check failed\n";
                    break;
                }
            }
            return static_cast<int>(result);
        }

        int run() const {
            std::unique_ptr<char[]> buf(new char[mSize]);
            std::memset(buf.get(), 'x', mSize);

            xclDeviceInfo2 info;
            int rc = xclGetDeviceInfo2(mHandle, &info);
            if (rc)
                return rc;

            if (info.mDMAThreads == 0)
                return -EINVAL;

            //std::cout << "Using " << info.mDMAThreads << " bi-directional PCIe DMA channels for DMA test\n";

            size_t result = 0;
            for (auto i : mBOList)
                result += xclWriteBO(mHandle, i, buf.get(), mSize, 0);

            if (result)
                return static_cast<int>(result);

            Timer timer;
            result = runSync(XCL_BO_SYNC_BO_TO_DEVICE, info.mDMAThreads);
            if (result)
                return static_cast<int>(result);

            auto timer_stop = timer.stop();
            double rate = static_cast<double>(mBOList.size() * mSize);
            rate /= 0x100000; // MB
            rate /= timer_stop;
            rate *= 1000000; // s
            std::cout << "Host -> PCIe -> FPGA write bandwidth = " << rate << " MB/s\n";

            timer.reset();
            result = runSync(XCL_BO_SYNC_BO_FROM_DEVICE, info.mDMAThreads);
            if (result)
                return static_cast<int>(result);

            timer_stop = timer.stop();
            rate = static_cast<double>(mBOList.size() * mSize);
            rate /= 0x100000; // MB
            rate /= timer_stop;
            rate *= 1000000; //
            std::cout << "Host <- PCIe <- FPGA read bandwidth = " << rate << " MB/s\n";

            // data integrity check: compare with initialized value 'x'
            result = validate(buf.get());
            return static_cast<int>(result);
        }
    };
}

#endif /* DMATEST_H */
