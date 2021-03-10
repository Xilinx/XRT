/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include <sstream>
#include <fstream>
#include <cstring>
#include "zynq_dev.h"

static std::fstream sysfs_open_path(const std::string& path, std::string& err,
    bool write, bool binary)
{
    std::fstream fs;
    std::ios::openmode mode = write ? std::ios::out : std::ios::in;

    if (binary)
        mode |= std::ios::binary;

    err.clear();
    fs.open(path, mode);
    if (!fs.is_open()) {
        std::stringstream ss;
        ss << "Failed to open " << path << " for "
            << (binary ? "binary " : "")
            << (write ? "writing" : "reading") << ": "
            << strerror(errno) << std::endl;
        err = ss.str();
    }
    return fs;
}

std::string zynq_device::get_sysfs_path(const std::string& entry)
{
    return sysfs_root + entry;
}

std::fstream zynq_device::sysfs_open(const std::string& entry,
    std::string& err, bool write, bool binary)
{
    return sysfs_open_path(get_sysfs_path(entry), err, write, binary);
}

void zynq_device::sysfs_put(const std::string& entry, std::string& err_msg,
    const std::string& input)
{
    std::fstream fs = sysfs_open(entry, err_msg, true, false);
    if (!err_msg.empty())
        return;
    fs << input;
}

void zynq_device::sysfs_put(const std::string& entry, std::string& err_msg,
    const std::vector<char>& buf)
{
    std::fstream fs = sysfs_open(entry, err_msg, true, true);
    if (!err_msg.empty())
        return;
    fs.write(buf.data(), buf.size());
}

void zynq_device::sysfs_get(const std::string& entry, std::string& err_msg,
    std::vector<char>& buf)
{
    std::fstream fs = sysfs_open(entry, err_msg, false, true);
    if (!err_msg.empty())
        return;
    buf.insert(std::end(buf),std::istreambuf_iterator<char>(fs),
        std::istreambuf_iterator<char>());
}

void zynq_device::sysfs_get(const std::string& entry, std::string& err_msg,
    std::vector<std::string>& sv)
{
    std::fstream fs = sysfs_open(entry, err_msg, false, false);
    if (!err_msg.empty())
        return;

    sv.clear();
    std::string line;
    while (std::getline(fs, line))
        sv.push_back(line);
}

void zynq_device::sysfs_get(const std::string& entry, std::string& err_msg,
    std::vector<uint64_t>& iv)
{
    uint64_t n;
    std::vector<std::string> sv;

    iv.clear();

    sysfs_get(entry, err_msg, sv);
    if (!err_msg.empty())
        return;

    char *end;
    for (auto& s : sv) {
        std::stringstream ss;

        if (s.empty()) {
            ss << "Reading " << get_sysfs_path(entry) << ", ";
            ss << "can't convert empty string to integer" << std::endl;
            err_msg = ss.str();
            break;
        }
        n = std::strtoull(s.c_str(), &end, 0);
        if (*end != '\0') {
            ss << "Reading " << get_sysfs_path(entry) << ", ";
            ss << "failed to convert string to integer: " << s << std::endl;
            err_msg = ss.str();
            break;
        }
        iv.push_back(n);
    }
}

void zynq_device::sysfs_get(const std::string& entry, std::string& err_msg,
    std::string& s)
{
    std::vector<std::string> sv;

    sysfs_get(entry, err_msg, sv);
    if (!sv.empty())
        s = sv[0];
    else
        s = ""; // default value
}

zynq_device *zynq_device::get_dev()
{
    // This is based on the fact that on edge devices, we only have one DRM
    // device, which is named renderD128.
    // This path is reliable. It is the same for ARM32 and ARM64.
    static zynq_device dev("/sys/class/drm/renderD128/device/");
    return &dev;
}

zynq_device::zynq_device(const std::string& root) : sysfs_root(root)
{
}
