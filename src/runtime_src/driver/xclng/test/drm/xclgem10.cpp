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
#include <sys/eventfd.h>
#include <sys/time.h>
#include <poll.h>

#include <cassert>
#include <vector>
#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <cerrno>

#include "drm/drm.h"
#include "xocl_ioctl.h"
#include "util.h"

static const int pollTimeout = 2000;
static const int maxTimeout = 60000;
/**
 * User interrupt test.
 * Compile command:
 * g++ -g -std=c++11 -I ../../include -I ../../drm/xocl xclgem10.cpp util.cpp
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

    int result = 0;

    std::vector<pollfd> uifdVector;
    for (int i = 4; i < 8; i++) {
        int uifd = eventfd(0, EFD_CLOEXEC);
        if (uifd < 0) {
            result = errno;
            std::perror("eventfd");
            break;
        }
        drm_xocl_user_intr intr = {0, uifd, i};
        result = ioctl(fd, DRM_IOCTL_XOCL_USER_INTR, &intr);
        if (result < 0) {
            result = errno;
            std::perror("eventfd");
            break;
        }
        pollfd info = {uifd, POLLIN, 0};
        uifdVector.push_back(info);
    }

    int delay = 0;
    while (!result && (delay < maxTimeout)) {
        result = poll(&uifdVector[0], uifdVector.size(), pollTimeout);
        delay += pollTimeout;
        if (result < 0) {
            result = errno;
            std::perror("poll");
            break;
        }
        if (!result) {
            std::cout << "poll timeout after " << pollTimeout << " ms\n";
            continue;
        }

        timeval now;
        gettimeofday(&now, 0);
        std::cout << "tv_sec[" << now.tv_sec << "]tv_usec[" << now.tv_usec << "]\n";
        for (auto i : uifdVector) {
            if (i.revents == POLLIN) {
                long long val;
                read(i.fd, &val, 8);
                std::cout << "User interrupt " << i.fd << " event " << val << "\n";
            }
        }
        result = 0;
    }
    for (auto i : uifdVector)
        close(i.fd);

    close(fd);
    std::cout << "result = " << result << std::endl;
    return result;
}


