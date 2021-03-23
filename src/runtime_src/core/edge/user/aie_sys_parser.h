/**
 * Copyright (C) 2021 Xilinx, Inc
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
#ifndef _XCL_AIE_SYS_H_
#define _XCL_AIE_SYS_H_

#include <string>
#include <vector>
#include <fstream>
#include <boost/property_tree/ptree.hpp>

class aie_sys_parser {

private:
    std::fstream sysfs_open_path(const std::string& path, bool write, bool binary);
    std::fstream sysfs_open(const std::string& entry, bool write, bool binary);
    void sysfs_get(const std::string& entry, std::vector<std::string>& sv);
    void addrecursive(const int col, const int row, const std::string& tag, const std::string& line,
                      boost::property_tree::ptree &pt);

    std::string sysfs_root;
    aie_sys_parser(const std::string& sysfs_base);
    aie_sys_parser(const aie_sys_parser& s) = delete;
    aie_sys_parser& operator=(const aie_sys_parser& s) = delete;

public:
    static aie_sys_parser *get_parser();
    boost::property_tree::ptree aie_sys_read(const int col, const int row);

};

#endif
