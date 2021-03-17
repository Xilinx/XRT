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

#include "aie_sys_parser.h"
#include <boost/property_tree/json_parser.hpp>

std::fstream aie_sys_parser::sysfs_open_path(const std::string& path,
    bool write, bool binary)
{   
    std::fstream fs;
    std::ios::openmode mode = write ? std::ios::out : std::ios::in;
    
    if (binary) 
        mode |= std::ios::binary;
    
    fs.open(path, mode);
    if (!fs.is_open()) {
        throw std::runtime_error(boost::str(boost::format("Failed to open %s for %s %s: (%d) %s") 
            % path 
            % (binary ? "binary " : "") 
            % (write ? "writing" : "reading") 
            % errno
            % strerror(errno)));
    }
    return fs;
}

std::fstream aie_sys_parser::sysfs_open(const std::string& entry,
    bool write, bool binary)
{
    return sysfs_open_path(entry, write, binary);
}

void aie_sys_parser::sysfs_get(const std::string& entry, std::vector<std::string>& sv)
{
    std::fstream fs = sysfs_open(entry, false, false);
    sv.clear();
    std::string line;
    while (std::getline(fs, line))
        sv.push_back(line);
}

void
aie_sys_parser::addrecursive(const int col, const int row, const std::string& tag, const std::string& line,
    boost::property_tree::ptree &pt)
{
    std::string n(tag); 
    boost::property_tree::ptree value;
    int start_index = 0;
    int end_index = 0;
    pt.put("col",col);
    pt.put("row",row);
    std::size_t found = line.find_first_of(":|, ");
    while (found!=std::string::npos) {
        switch(line[found]) {
            case ':':	{ // ':' 58
                end_index = found;
                if(!n.empty())
                    n += ".";
                n += line.substr(start_index, (end_index-start_index));
                break;
                }
            case '|':	{ //'|' 124
                end_index = found;
                boost::property_tree::ptree v;
                v.put("", line.substr(start_index, (end_index-start_index)));
                value.push_back(std::make_pair("",v));
                break;
                }
            case ',':	{ // ',' 44
                end_index = found;
                boost::property_tree::ptree v;
                v.put("", line.substr(start_index, (end_index-start_index)));
                value.push_back(std::make_pair("",v));
                pt.add_child(n.c_str(), value);
                value.erase("");
                n = n.substr(0, n.rfind("."));
                break;
                }
            case ' ':	{ //space 32
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
aie_sys_parser::aie_sys_read(const int col,const int row)
{
    std::vector<std::string> tags{"core","dma","lock","errors","event"};
    std::vector<std::string> data;
    boost::property_tree::ptree pt;
    for(auto& tag:tags) {
        std::ifstream ifile(sysfs_root+std::to_string(col)+"_"+std::to_string(row)+"/"+tag);
        if(ifile.is_open()) {
            sysfs_get(sysfs_root+std::to_string(col)+"_"+std::to_string(row)+"/"+tag,data);
            for(auto line:data)
                addrecursive(col,row,tag,line,pt);
        }
    }
    return pt;	
}

aie_sys_parser *aie_sys_parser::get_parser()
{
    //TODO: get partitin id from xclbin but its not supported currently.
    static aie_sys_parser dev("/sys/class/aie/aiepart_0_50/");
    return &dev;
}

aie_sys_parser::aie_sys_parser(const std::string& root) : sysfs_root(root)
{
}
