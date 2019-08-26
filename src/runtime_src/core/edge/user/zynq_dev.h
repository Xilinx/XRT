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
#ifndef _XCL_ZYNQ_DEV_H_
#define _XCL_ZYNQ_DEV_H_

#include <string>
#include <vector>
#include <fstream>

class zynq_device {
public:

    void sysfs_get(const std::string& entry, std::string& err_msg,
        std::vector<std::string>& sv);
    void sysfs_get(const std::string& entry, std::string& err_msg,
        std::vector<uint64_t>& iv);
    void sysfs_get(const std::string& entry, std::string& err_msg,
        std::string& s);
    void sysfs_get(const std::string& entry, std::string& err_msg,
        std::vector<char>& buf);
    template <typename T>
    void sysfs_get(const std::string& entry, std::string& err_msg,
        T& i, T def) {
        std::vector<uint64_t> iv;

        sysfs_get(entry, err_msg, iv);
        if (!iv.empty())
            i = static_cast<T>(iv[0]);
        else
            i = def; // user defined default value
    }

    void sysfs_put(const std::string& entry, std::string& err_msg,
        const std::string& input);
    void sysfs_put(const std::string& entry, std::string& err_msg,
        const std::vector<char>& buf);
    std::string get_sysfs_path(const std::string& entry);

    static zynq_device *get_dev();
private:
    std::fstream sysfs_open(const std::string& entry, std::string& err,
        bool write = false, bool binary = false);

    std::string sysfs_root;
    zynq_device(const std::string& sysfs_base);
    zynq_device(const zynq_device& s) = delete;
    zynq_device& operator=(const zynq_device& s) = delete;
};

#endif
