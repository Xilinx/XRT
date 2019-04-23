/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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
value_or_empty(const char* cstr)
{
  return cstr ? cstr : "";
}

static bool
is_true(const std::string& str)
{
  return str=="true";
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

/*
 * Look for xrt.ini and if not found look for legacy sdaccel.ini.
 */
static std::string
verify_ini_path(const boost::filesystem::path& dir)
{
  auto file_path = dir / "xrt.ini";
  if (boost::filesystem::exists(file_path))
    return file_path.string();

  file_path = dir / "sdaccel.ini";
  if (boost::filesystem::exists(file_path))
    return file_path.string();

  return "";
}

static std::string
get_ini_path()
{
  std::string full_path;
  try {
    //The env variable should be the full path which includes xrt.ini
    auto xrt_path = boost::filesystem::path(value_or_empty(std::getenv("XRT_INI_PATH")));
    if (boost::filesystem::exists(xrt_path))
      return xrt_path.string();

    //The env variable should be the full path which includes sdaccel.ini
    auto sda_path = boost::filesystem::path(value_or_empty(std::getenv("SDACCEL_INI_PATH")));
    if (boost::filesystem::exists(sda_path))
      return sda_path.string();

    auto exe_path = boost::filesystem::path(get_self_path()).parent_path();
    full_path = verify_ini_path(exe_path);
    if (!full_path.empty())
      return full_path;

    auto self_path = boost::filesystem::current_path();
    full_path = verify_ini_path(self_path);
    if (!full_path.empty())
      return full_path;

  }
  catch (const boost::filesystem::filesystem_error& e) {
  }
  return full_path;
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

      // inform which .ini was read
      xrt_core::message::send(xrt_core::message::severity_level::INFO, "XRT", std::string("Read ") + path);
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

const char*
get_env_value(const char* env)
{
  return std::getenv(env);
}

bool
get_bool_value(const char* key, bool default_value)
{
  if (auto env = get_env_value(key))
    return is_true(env);

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
