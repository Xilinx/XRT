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
#include "util.h"

/**
 * Sanity test for DMA-BUF export/import. Uses object oriented modeling of BO
 * Performs simple alloc, read/write ,sync and free operations.
 * Compile command:
 * g++ -g -std=c++11 -I ../../include -I ../../drm/xocl xclgem6.cpp util.cpp
 */

static int runTest(int fd)
{
    std::cout << "CREATE" << std::endl;
    xoclutil::TestBO bo0("bo0", fd, 8192, nullptr);
    xoclutil::TestBO bo1("bo1", fd, 4200, nullptr);

    char *userptr = 0;
    posix_memalign((void **)&userptr, 4096, 8192);
    assert(userptr);
    xoclutil::TestBO bo2("bo2", fd, 8192, userptr);

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
    std::memset(buffer2, mean - 1, bo2.size());
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
        throw std::runtime_error("Inconsistent sync for B0 " + bo1.name());
    if (c2 != bo2.checksum())
        throw std::runtime_error("Inconsistent sync for B0 " + bo2.name());

    std::cout << "EXPORT" << std::endl;
    int fd0 = bo0.wexport();
    int fd1 = bo1.wexport();
    int fd2 = bo2.wexport();

    /**
     * TODO:
     * Call checksum on these objects to make sure that they match with original buffers
     * Move creation of these buffers to another thread
     */
    xoclutil::TestBO bo3("bo0-export", fd, fd0);
    xoclutil::TestBO bo4("bo1-export", fd, fd1);

    //Don't import below user ptr buffer on same device. As that will fail to map and give error
    //It is supposed to be used on different devices. And it will on different devices.
    //Check it in another testcase.
    //xoclutil::TestBO bo5("bo2-export", fd, fd2);

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

    int fd = xoclutil::openDevice(dev);

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


