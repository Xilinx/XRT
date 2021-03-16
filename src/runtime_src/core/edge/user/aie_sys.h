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

namespace aie_sys_parser {

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

std::fstream sysfs_open(const std::string& entry,
    std::string& err, bool write, bool binary)
{
    return sysfs_open_path(entry, err, write, binary);
}

void sysfs_get(const std::string& entry, std::string& err_msg,
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

void addrecursive(int col, int row, std::string tag, std::string line, boost::property_tree::ptree &pt) {
    std::string n(tag); 
    boost::property_tree::ptree value;
    int start_index = 0;
    int end_index = 0;
    pt.put("col",col);
    pt.put("row",row);
    std::size_t found = line.find_first_of(":|, ");
    while (found!=std::string::npos) {
        switch(line[found]) {
            case 58:	{ // ':'
                end_index = found;
                if(!n.empty())
                    n += ".";
                n += line.substr(start_index, (end_index-start_index));
                break;
                }
            case 124:	{ //'|'
                end_index = found;
                boost::property_tree::ptree v;
                v.put("", line.substr(start_index, (end_index-start_index)));
                value.push_back(std::make_pair("",v));
                break;
                }
            case 44:	{ // ','
                end_index = found;
                boost::property_tree::ptree v;
                v.put("", line.substr(start_index, (end_index-start_index)));
                value.push_back(std::make_pair("",v));
                pt.add_child(n.c_str(), value);
                value.erase("");
                n = n.substr(0, n.rfind("."));
                break;
                }
            case 32:	{ //space
                start_index++;
                break;
                }
	}
        start_index = found+1;
        found = line.find_first_of(":|, ",found+1);;
    }
    end_index = found;
    boost::property_tree::ptree v;
    v.put("", line.substr(start_index, (end_index-start_index)));
    value.push_back(std::make_pair("",v));
    pt.add_child(n.c_str(), value);
}

boost::property_tree::ptree
aie_sys_read(int col,int row, const std::string& path) {
    std::vector<std::string> tags{"core","dma","lock","errors","event"};
    std::vector<std::string> data;
    std::string err_msg;
    boost::property_tree::ptree pt;
    path = path+"/"+std::to_string(col)+"_"+std::to_string(row);
    for(auto tag:tags) {
        std::ifstream ifile(path+"/"+tag);
        if(ifile.is_open()) {
            sysfs_get(path+"/"+tag, err_msg,data);
            if (!err_msg.empty())
                throw std::runtime_error(err_msg);
            for(auto line:data)
                addrecursive(col,row,tag,line,pt);
        }
    }
    return pt;	
}

} // aie_sys_parser

#endif
