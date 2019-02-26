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

#include "config_reader.h"
#include "message.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <iostream>
#include <cstdlib>

#ifdef __GNUC__
# include <linux/limits.h>
# include <sys/stat.h>
#endif

namespace {

static const char*
valueOrEmpty(const char* cstr)
{
  return cstr ? cstr : "";
}

static std::string
get_self_path()
{
#ifdef __GNUC__
  char buf[PATH_MAX] = {0};
  auto len = ::readlink("/proc/self/exe", buf, PATH_MAX);
  return std::string(buf, (len>0) ? len : 0);
#else
  return "";
#endif
}

static std::string
get_ini_path()
{
  auto ini_path = boost::filesystem::path(valueOrEmpty(std::getenv("SDACCEL_INI_PATH")));
  // Support SDACCEL_INI_PATH with/without actual filename
  if (ini_path.filename() != "sdaccel.ini")
    ini_path /= "sdaccel.ini";
  if (boost::filesystem::exists(ini_path))
    return ini_path.string();
  auto exe_path = boost::filesystem::path(get_self_path()).parent_path()/"sdaccel.ini";
  if (boost::filesystem::exists(exe_path))
    return exe_path.string();
  auto self_path = boost::filesystem::current_path()/"sdaccel.ini";
  try {
    if (boost::filesystem::exists(self_path))
      return self_path.string();
  }  catch (const boost::filesystem::filesystem_error& e){ }

  return "";
}

struct tree
{
  boost::property_tree::ptree m_tree;

  void
  setenv()
  {
    if (xrt_core::config::get_multiprocess())
      ::setenv("XCL_MULTIPROCESS_MODE","1",1);
  }

  void
  read(const std::string& path)
  {
    try {
      read_ini(path,m_tree);
      
      // set env vars to expose sdaccel.ini to hal layer
      setenv();
    }
    catch (const std::exception& ex) {
      xrt_core::message::send(xrt_core::message::severity_level::WARNING, "XRT", ex.what());
    }
  }

  tree()
  {
    auto ini_path = get_ini_path();
    if (ini_path.empty())
      return;

    //XRT_PRINT(std::cout,"Reading configuration from '",ini_path,"'\n");
    read(ini_path);
  }

  void
  reread(const std::string& fnm)
  {
    read(fnm);
  }
};

static tree s_tree;

}

namespace xrt_core { namespace config {

namespace detail {

bool
get_bool_value(const char* key, bool default_value)
{
  return s_tree.m_tree.get<bool>(key,default_value);
}

std::string
get_string_value(const char* key, const std::string& default_value)
{
  std::string val = s_tree.m_tree.get<std::string>(key,default_value);
  // Although INI file entries are not supposed to have quotes around strings
  // but we want to be cautious
  if ((val.front() == '"') && (val.back() == '"')) {
    val.erase(0, 1);
    val.erase(val.size()-1);
  }
  return val;
}

unsigned int
get_uint_value(const char* key, unsigned int default_value)
{
  return s_tree.m_tree.get<unsigned int>(key,default_value);
}

std::ostream&
debug(std::ostream& ostr, const std::string& ini)
{
  if (!ini.empty())
    s_tree.reread(ini);

  for(auto& section : s_tree.m_tree) {
    ostr << "[" << section.first << "]\n";
    for (auto& key:section.second) {
      ostr << key.first << " = " << key.second.get_value<std::string>() << std::endl;
    }
  }
  return ostr;
}

} // detail

}}


