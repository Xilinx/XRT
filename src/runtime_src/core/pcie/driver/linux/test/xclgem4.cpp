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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <cassert>
#include <vector>
#include <future>
#include "drm/drm.h"
#include "xocl_ioctl.h"
#include "util.h"

/**
 * Validate xocl's multi channel DMA support and measure bandwidth
 * Perform DMA operations from 2 threads in parallel
 * Compile command:
 * g++ -g -pthread -std=c++11 -I ../../include -I ../../drm/xocl xclgem4.cpp util.cpp
 */

class Timer {
	std::chrono::high_resolution_clock::time_point mTimeStart;
	public:
	Timer() {
		reset();
	}
	double stop() {
		std::chrono::high_resolution_clock::time_point timeEnd = std::chrono::high_resolution_clock::now();
		return std::chrono::duration_cast<std::chrono::microseconds>(timeEnd - mTimeStart).count();
	}
	void reset() {
		mTimeStart = std::chrono::high_resolution_clock::now();
	}
};


static int runSyncWorker(std::vector<unsigned>::const_iterator b,
                         std::vector<unsigned>::const_iterator e,
                         size_t size, drm_xocl_sync_bo_dir dir, int fd)
{
    int result = 0;
    for (std::vector<unsigned>::const_iterator i = b; i < e; ++i) {
        drm_xocl_sync_bo syncInfo = {*i, 0, size, 0, dir};
        result = ioctl(fd, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
        if (result) {
            std::cout << "Sync BO " << *i << " result = " << result << std::endl;
            break;
        }
    }
    return result;
}

/**
 * Runs POSIX sync (DMA) operation on buffer objects using C++11 thread pool
 * If mt is true two thread will be simultaneously perform DMA validating xocl's
 * multi threading/multi channel support
 */
static int runSync(const std::vector<unsigned> &boList,
                   size_t size, drm_xocl_sync_bo_dir dir,
                   int fd, bool mt)
{
    const std::vector<unsigned>::const_iterator b = boList.begin();
    const std::vector<unsigned>::const_iterator e = boList.end();
    if (mt) {
        auto len = e - b;
        auto mid = b + len/2;
        auto future0 = std::async(std::launch::async, runSyncWorker, b,
                                  mid, size, dir, fd);
        auto future1 = std::async(std::launch::async, runSyncWorker, mid,
                                  e, size, dir, fd);
        return (future0.get() + future1.get());
    }
    else {
        auto future0 = std::async(std::launch::async, runSyncWorker, b,
                                  e, size, dir, fd);
        return future0.get();
    }
}

int runTest(size_t size, int fd, bool mt)
{

    char *buf = new char[size];
    std::memset(buf, 'g', size);

    std::vector<unsigned> boList;
    std::cout << "\nCREATE" << std::endl;

    /* Try to fill 4 GB of space with buffers */
    long long count = 0x100000000/size;
    /* Limit to 256 K buffers */
    if (count > 0x40000)
        count = 0x40000;

    long long i = 0;
    int result = 0;
    for (i = 0; i < count; i++) {
        drm_xocl_create_bo info = {size, 0xffffffff, 0};
        result = ioctl(fd, DRM_IOCTL_XOCL_CREATE_BO, &info);
        if (result) {
            //std::cout << "result = " << result << std::endl;
            break;
        }
        boList.push_back(info.handle);
    }

    if (size < 1024)
        std::cout << i << " buffers of " << size << " bytes\n";
    else
        std::cout << i << " buffers of " << size/1024 << " KB\n";

    std::cout << "PWRITE" << std::endl;
    for (unsigned boh : boList) {
        drm_xocl_pwrite_bo pwriteInfo = {boh, 0, 0, size, reinterpret_cast<uint64_t>(buf)};
        result = ioctl(fd, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfo);
        if (result) {
            std::cout << "result = " << result << std::endl;
            break;
        }
    }

    std::cout << "SYNC" << std::endl;
    Timer timer;
    result = runSync(boList, size, DRM_XOCL_SYNC_BO_TO_DEVICE, fd, mt);
    double timer_stop = timer.stop();
    double rate = (i * size)/0x100000; // MB
    rate /= timer_stop;
    rate *= 1000000; // s
    std::cout << "Write Bandwidth = " << rate << " MB/s\n";

    timer.reset();
    result = runSync(boList, size, DRM_XOCL_SYNC_BO_FROM_DEVICE, fd, mt);
    timer_stop = timer.stop();
    rate = (i * size)/0x100000; // MB
    rate /= timer_stop;
    rate *= 1000000; //
    std::cout << "Read Bandwidth = " << rate << " MB/s\n";


    std::cout << "CLOSE" << std::endl;
    for (unsigned boh : boList) {
        drm_gem_close closeInfo = {boh, 0};
        result = ioctl(fd, DRM_IOCTL_GEM_CLOSE, &closeInfo);
        if (result) {
            std::cout << "result = " << result << std::endl;
            break;
        }
    }
    return result;
}


int main(int argc, char *argv[])
{
    const char *dev = "xocl";
    unsigned kind = 0;
    bool mt = true;
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

    int result = runTest(0x4000000, fd, mt);
    result += runTest(0x100000, fd, mt);
    result += runTest(0x40000, fd, mt);
    result += runTest(0x4000, fd, mt);
    result = close(fd);
    return result;
}


