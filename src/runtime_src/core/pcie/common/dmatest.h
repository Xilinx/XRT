// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2015-2020 Xilinx, Inc
// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef DMATEST_H
#define DMATEST_H

#include <chrono>
#include <future>
#include <vector>
#include <cstring>
#include <iostream>


#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/memalign.h"
#include "core/common/unistd.h"
#include "core/common/shim/buffer_handle.h"

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
        using buffer_and_deleter = std::pair<std::unique_ptr<xrt_core::buffer_handle>, xrt_core::aligned_ptr_type>;
        std::vector<buffer_and_deleter> mBOList;
        std::shared_ptr<xrt_core::device> mHandle;
	std::unique_ptr<xrt_core::hwctx_handle> mhwCtxHandle;
        size_t mSize;
        size_t mTotalSize;
        unsigned mFlags;
        char mPattern;

        int runSyncWorker(std::vector<buffer_and_deleter>::const_iterator b,
                          std::vector<buffer_and_deleter>::const_iterator e,
                          xclBOSyncDirection dir) const {
            int result = 0;
            while (b < e) {
                try {
                  b->first->sync(static_cast<xrt_core::buffer_handle::direction>(dir), mSize, 0);
                }
                catch (const std::exception&) {
                    throw xrt_core::error(result, "DMA failed");
                    break;
                }
                ++b;
            }
            return result;
        }

        int runSync(xclBOSyncDirection dir, size_t count) const {
            auto b = mBOList.begin();
            const auto e = mBOList.end();
            if (count == 1) {
                auto future0 = std::async(std::launch::async, &DMARunner::runSyncWorker, this, b, e, dir);
                return future0.get();
            }
            /* Calculate the DMA workers number, the DMA may have one or more channels
             * which means it's capable to run multi DMA transaction at the same time
             *   mBOList.size()  |  len  |  ajust_e
             * --------------------------------------
             *          1        |   1   |     b+1
             * --------------------------------------
             *          2        |   1   |     b+2
             * --------------------------------------
             *          4        |   2   |     b+4
             */
            size_t len = (((size_t)(e - b)) < count) ? 1 : ((size_t)(e - b))/count;
            auto bo_cnt = static_cast<size_t>(e - b);
            const auto adjust_e = b + len * ((len == 1) ? bo_cnt : std::min<size_t>(count, len));

            std::vector<std::future<int>> threads;
            while (b < adjust_e) {
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
        DMARunner(const std::shared_ptr<xrt_core::device>& handle, size_t size, unsigned flags = 0, size_t totalSize = 0) :
                mHandle(handle),
                mSize(size),
                mTotalSize(totalSize),
                mFlags(flags),
                mPattern('x') {
            long long count = mTotalSize / mSize;

            if (count == 0)
                throw xrt_core::error(-EINVAL,
                    boost::str(boost::format("DMA buffer size cannot be larger than %#x.") % mTotalSize));

            if (count > 0x40000)
                count = 0x40000;

	    mhwCtxHandle = mHandle->create_hw_context(mHandle->get_xclbin_uuid().get(), {}, xrt::hw_context::access_mode::shared);
	    xcl_bo_flags xflags{mFlags};
	    xflags.slot = static_cast<uint8_t>(mhwCtxHandle->get_slotidx());
	    mFlags = xflags.flags;

            for (long long i = 0; i < count; i++) {
                // This can throw and callers of DMARunner are supposed to catch this.
                xrt_core::aligned_ptr_type buf = xrt_core::aligned_alloc(xrt_core::getpagesize(), mSize);
                auto bo = mhwCtxHandle->alloc_bo(buf.get(), mSize, mFlags);
                if (!bo)
                    break;
                std::memset(buf.get(), mPattern, mSize);
                mBOList.emplace_back(std::move(bo), std::move(buf));
            }
            if (mBOList.size() == 0)
                throw xrt_core::error(-ENOMEM, "No DMA buffers could be allocated.");
        }

        ~DMARunner()
        {
	    // This explicit call is to make sure BOs get free before hw context
	    // handler (mhwCtxHandle) destructed.
	    mBOList.clear();
	}

        int run(std::ostream& ostr = std::cout) const {
            auto dma_threads = xrt_core::device_query<xrt_core::query::dma_threads_raw>(mHandle);
            if (dma_threads.empty())
                throw xrt_core::error(-EINVAL, "Unable to determine number of DMA channels.");

            int result = 0;
            Timer timer;
            result = runSync(XCL_BO_SYNC_BO_TO_DEVICE, dma_threads.size());
            if (result)
                throw xrt_core::error(result, "DMA from host to device failed.");

            auto timer_stop = timer.stop();
            double rate = static_cast<double>(mBOList.size() * mSize);
            rate /= 0x100000; // MB
            rate /= timer_stop;
            rate *= 1000000; // s
            ostr << boost::str(boost::format("Host -> PCIe -> FPGA write bandwidth = %.1f MB/s\n") % rate);

            timer.reset();
            result = runSync(XCL_BO_SYNC_BO_FROM_DEVICE, dma_threads.size());
            if (result)
                throw xrt_core::error(result, "DMA from device to host failed.");

            timer_stop = timer.stop();
            rate = static_cast<double>(mBOList.size() * mSize);
            rate /= 0x100000; // MB
            rate /= timer_stop;
            rate *= 1000000; //
            ostr << boost::str(boost::format("Host <- PCIe <- FPGA read bandwidth = %.1f MB/s\n") % rate);

            // data integrity check: compare with initialized pattern
            return validate();
        }
    };
}

#endif /* DMATEST_H */
