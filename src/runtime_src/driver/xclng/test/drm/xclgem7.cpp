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
#include <future>
#include <random>

#include "drm/drm.h"
#include "xocl_ioctl.h"
#include "util.h"

/**
 * Run buffer allocation and migration from multiple threads simultaneously
 * Compile command:
 * g++ -pthread -g -std=c++11 -I ../../include -I ../../drm/xocl xclgem7.cpp util.cpp
 */


static int runTest(int fd, size_t size, unsigned count, bool verbose)
{
    std::vector<xoclutil::TestBO> boList;
    if (verbose)
        std::cout << '[' << std::this_thread::get_id() << ']' << "CREATE" << std::endl;

    try {
        for (unsigned i = 0; i < count; i++) {
            std::string name("bo");
            name += std::to_string(boList.size());
            boList.push_back(xoclutil::TestBO(name.c_str(), fd, size, nullptr));
        }
    }
    catch (const std::exception& ex) {
        std::cout << ex.what() << std::endl;
    }

    std::random_device r;
    // Choose a random mean between 1 and 254
    std::default_random_engine e1(r());
    std::uniform_int_distribution<int> uniform_dist(0, 255);
    int mean = uniform_dist(e1);

    char *bufferCheck = new char[size];
    std::memset(bufferCheck, 0, size);
    char *buffer0 = new char[size];

    unsigned i = 0;
    for (xoclutil::TestBO & bo : boList) {
        std::memset(buffer0, mean + i, size);
        if (verbose)
            std::cout << '[' << std::this_thread::get_id() << ']'
                      << "PWRITE (update hbuf)" << std::endl;
        bo.pwrite(buffer0, bo.size());
        unsigned long long c0 = bo.checksum();

        if (verbose)
            std::cout << '[' << std::this_thread::get_id() << ']'
                      << "SYNC TO DEVICE (update dbuf with hbuf)" << std::endl;
        bo.sync(DRM_XOCL_SYNC_BO_TO_DEVICE, bo.size());

        if (verbose)
            std::cout << '[' << std::this_thread::get_id() << ']'
                      << "PWRITE (clear hbuf)" << std::endl;
        bo.pwrite(bufferCheck, bo.size());
        if (bo.checksum() != 0)
            throw std::runtime_error("Could not clear BO " + bo.name());

        if (verbose)
            std::cout << '[' << std::this_thread::get_id() << ']'
                      << "SYNC FROM DEVICE (refresh hbuf from dbuf)" << std::endl;
        bo.sync(DRM_XOCL_SYNC_BO_FROM_DEVICE, bo.size());
        if (verbose)
            std::cout << '[' << std::this_thread::get_id() << ']'
                      << "VALIDATE SYNC DATA" << std::endl;
        if (c0 != bo.checksum())
            throw std::runtime_error("Inconsistent sync for BO " + bo.name());
        i++;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    const char *dev = "xocl";
    const size_t total = 0x100000000;
    bool verbose = true;

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

    size_t size = 0x100000; // (1 MB)
    unsigned count = total / size;
    count /= 4; // divide equally among 4 threads
    count /= 2; // conservative number

    std::cout << "Launching 4 threads each with " << count << " objects of size " << size/1024 << " KB\n";
    auto future0 = std::async(std::launch::async, runTest, fd, size, count, verbose);
    auto future1 = std::async(std::launch::async, runTest, fd, size, count, verbose);
    auto future2 = std::async(std::launch::async, runTest, fd, size, count, verbose);
    auto future3 = std::async(std::launch::async, runTest, fd, size, count, verbose);

    int result = 0;
    try {
        result = future0.get() + future1.get() + future2.get() + future3.get();
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


