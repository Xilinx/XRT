/**
 * Copyright (C) 2018 Xilinx, Inc
 * Author: Ryan Radjabi
 * Wrapper around boost::property_tree::ptree for storing data that can
 * be accessed easily and exported to formats like JSON and XML.
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

#ifndef COMMON_SENSOR_H
#define COMMON_SENSOR_H

#include <boost/property_tree/ptree.hpp>
#include <string>

namespace sensor_tree {

boost::property_tree::ptree&
instance();

template <typename T>
void put(const std::string &path, T val)
{
  instance().put(path,val);
}

template <typename T>
inline T get(const std::string &path, const T &defaultVal)
{
    return instance().get( path, defaultVal );
}

inline void
add_child(const std::string &path, boost::property_tree::ptree& child)
{
  instance().add_child( path, child );
}

inline boost::property_tree::ptree
get_child(const std::string &path)
{
  return instance().get_child( path );
}

void
json_dump(std::ostream &ostr);

} // namespace sensor_tree

#endif
