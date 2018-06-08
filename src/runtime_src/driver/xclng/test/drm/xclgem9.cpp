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
 * Perform unmanaged read/write operations.
 * Compile command:
 * g++ -g -std=c++11 -I ../../include -I ../../drm/xocl xclgem9.cpp util.cpp
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

    const size_t size = 8192;

    int fd = xoclutil::openDevice(dev);

    if (fd < 0) {
        return -1;
    }

    char *ptrw = 0;
    posix_memalign((void **)&ptrw, 4096, size);
    assert(ptrw);
    std::memset(ptrw, 'q', size);

    char *ptrr = 0;
    posix_memalign((void **)&ptrr, 4096, size);
    assert(ptrr);
    std::memset(ptrr, 0, size);

    std::cout << "UNMGD PWRITE" << std::endl;
    drm_xocl_pwrite_unmgd infow = {0, 0, 4096, size, reinterpret_cast<uint64_t>(ptrw)};
    int result = ioctl(fd, DRM_IOCTL_XOCL_PWRITE_UNMGD, &infow);
    std::cout << "result = " << result << std::endl;

    std::cout << "UNMGD PREAD" << std::endl;
    drm_xocl_pread_unmgd infor = {0, 0, 4096, size, reinterpret_cast<uint64_t>(ptrr)};
    result = ioctl(fd, DRM_IOCTL_XOCL_PREAD_UNMGD, &infor);
    std::cout << "result = " << result << std::endl;

    std::cout << "COMPARE" << std::endl;
    result = std::memcmp(ptrw, ptrr, size);
    std::cout << "result = " << result << std::endl;

    free(ptrw);
    free(ptrr);

    result = close(fd);
    std::cout << "result = " << result << std::endl;
    return result;
}


