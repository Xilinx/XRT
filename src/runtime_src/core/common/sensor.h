/**
 * Copyright (C) 2018-2021 Xilinx, Inc
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
#include "config.h"
#include "device.h"

#include <boost/lexical_cast.hpp>
#include <iostream>

namespace xrt_core { namespace sensor {

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
read_electrical(const xrt_core::device * device);

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
read_thermals(const xrt_core::device * device);

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
read_power_consumption(const xrt_core::device * device);

XRT_CORE_COMMON_EXPORT
boost::property_tree::ptree
read_mechanical(const xrt_core::device * device);

}} // sensor, xrt_core


// The following namespace is only used by legacy xbutil dump
namespace sensor_tree {

boost::property_tree::ptree&
instance();

// Puts @val at the @path for instance()'s ptree.
template <typename T>
void
put(const std::string &path, T val)
{
  instance().put(path,val);
} 

// Gets value of type @T at the @path for instance()'s ptree.
// @defaultVal is returned if @path does not exist.
template <typename T>
inline T
get(const std::string &path, const T &defaultVal)
{
  return instance().get( path, defaultVal );
}

// Gets value of type @T at the @path for instance()'s ptree.
template <typename T>
inline T
get(const std::string &path)
{
  return instance().get<T>( path );
}

// Inserts child node @child at @path.
inline void
add_child(const std::string &path, const boost::property_tree::ptree& child)
{
  instance().add_child( path, child );
}

// Gets child node at @path.
inline boost::property_tree::ptree
get_child(const std::string &path)
{
  return instance().get_child( path );
}

// Dumps json format of ptree to @ostr.
void
json_dump(std::ostream &ostr);

// Wipe out whole ptree.
inline void clear()
{
  instance().clear();
}

} // namespace sensor_tree

#endif
