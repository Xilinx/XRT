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
#include <boost/lexical_cast.hpp>
#include <typeinfo>
#include <limits>
#include <string>
#include <sstream>

namespace sensor_tree {

boost::property_tree::ptree&
instance();

/* 
 * Puts @val at the @path for instance()'s ptree.
 */
template <typename T>
void put(const std::string &path, T val)
{
    instance().put(path,val);
} 

/* 
 * Gets value of type @T at the @path for instance()'s ptree.
 * @defaultVal is returned if @path does not exist.
 */
template <typename T>
inline T get(const std::string &path, const T &defaultVal)
{
    return instance().get( path, defaultVal );
}

/* 
 * Gets value of type @T at the @path for instance()'s ptree.
 */
template <typename T>
inline T get(const std::string &path)
{
    return instance().get<T>( path );
}

/* 
 * Inserts child node @child at @path.
 */
inline void
add_child(const std::string &path, const boost::property_tree::ptree& child)
{
    instance().add_child( path, child );
}

/* 
 * Gets child node at @path.
 */
inline boost::property_tree::ptree
get_child(const std::string &path)
{
    return instance().get_child( path );
}

/* 
 * Dumps json format of ptree to @ostr.
 */
void json_dump(std::ostream &ostr);

/* 
 * Wipe out whole ptree.
 */
inline void clear()
{
    instance().clear();
}

/*
 * Pretty formats @val. If @val is non-string, and is equal to max_value of type @T, 
 * returns @default_val.
 * Returns a string.
 */
template <typename T>
inline std::string pretty( const T &val, const std::string &default_val = "N/A", bool isHex = false )
{   
    if( typeid(val).name() != typeid(std::string).name() ) {
        if( val >= std::numeric_limits<T>::max() || val == 0) {
            return default_val;
        }

        if( isHex ) {
            std::stringstream ss;
            ss << "0x" << std::hex << val;
            return ss.str();
        }
    }
    return boost::lexical_cast<std::string>(val);
}

/*
 * Wrapper around get() that formats as pretty.
 */
template <typename T>
inline std::string
get_pretty( const std::string &path, const std::string &default_val = "N/A", bool isHex = false )
{
    T val = instance().get<T>( path );
    return pretty( val, default_val, isHex );
}

} // namespace sensor_tree

#endif
