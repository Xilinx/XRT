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

#define XRT_CORE_COMMON_SOURCE
#include "config_reader.h"
#include "message.h"
#include "error.h"

#include <set>
#include <iostream>
#include <mutex>
#include <cstdlib>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/format.hpp>

#ifdef __GNUC__
# include <linux/limits.h>
# include <sys/stat.h>
#endif

#ifdef _WIN32
# pragma warning( disable : 4996 )
#endif

namespace {

namespace key {

// Configuration values can be changed programmatically, but because
// values are statically cached, they can be changed only until they
// have been accessed the very first time.  This map tracks first key
// access.
static std::set<std::string> locked;
static std::mutex mutex;

static void
lock(const std::string& key)
{
  std::lock_guard<std::mutex> lk(mutex);
  locked.insert(key);
}

static bool
is_locked(const std::string& key)
{
  std::lock_guard<std::mutex> lk(mutex);
  return locked.find(key) != locked.end();
}

} // key

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
  catch (const boost::filesystem::filesystem_error&) {
  }
  return full_path;
}

struct tree
{
  boost::property_tree::ptree m_tree;
  const boost::property_tree::ptree null_tree;

  void
  read(const std::string& path)
  {
    try {
      read_ini(path,m_tree);

      // inform which .ini was read
      //xrt_core::message::send(xrt_core::message::severity_level::XRT_INFO, "XRT", std::string("Read ") + path);
    }
    catch (const std::exception& ex) {
      // Using the tree in this case is not safe, and since message
      // infra accesses xrt_core::config it can't be used safely.  Log
      // to stderr instead
      // xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", ex.what());
      std::cerr << "[XRT] Failed to read xrt.ini: " << ex.what() << std::endl;
    }
  }

  tree()
  {
    auto ini_path = get_ini_path();
    if (!ini_path.empty())
      read(ini_path);
  }

  void
  reread(const std::string& fnm)
  {
    read(fnm);
  }

  static tree*
  instance()
  {
    static tree s_tree;
    return &s_tree;
  }
};

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

  key::lock(key);
  return tree::instance()->m_tree.get<bool>(key,default_value);
}

std::string
get_string_value(const char* key, const std::string& default_value)
{
  std::string val = default_value;
  try {
    val = tree::instance()->m_tree.get<std::string>(key,default_value);
    // Although INI file entries are not supposed to have quotes around strings
    // but we want to be cautious
    if (!val.empty() && (val.front() == '"') && (val.back() == '"')) {
      val.erase(0, 1);
      val.erase(val.size()-1);
    } 
  }
  catch( std::exception const&) {
    // eat the exception, probably bad path
  }
  key::lock(key);
  return val;
}

unsigned int
get_uint_value(const char* key, unsigned int default_value)
{
  unsigned int val = default_value;
  try {
    val = tree::instance()->m_tree.get<unsigned int>(key,default_value);
  }
  catch( std::exception const&) {
    // eat the exception, probably bad path
  }
  key::lock(key);
  return val;
}


const boost::property_tree::ptree&
get_ptree_value(const char* key)
{
  auto s_tree  = tree::instance();
  auto i = s_tree->m_tree.find(key);
  key::lock(key);
  return (i != s_tree->m_tree.not_found()) ? i->second : s_tree->null_tree;
}

void
set(const std::string& key, const std::string& value)
{
  auto s_tree = tree::instance();

  if (key::is_locked(key)) {
    auto val = s_tree->m_tree.get<std::string>(key);
    auto fmt = boost::format("Cannot change value of configuration key '%s' because "
                             "its current value '%s' has already been used and has "
                             "been statically cached") % key % val;
    throw xrt_core::error(-EINVAL,fmt.str());
  }

  s_tree->m_tree.put(key, value);
}

std::ostream&
debug(std::ostream& ostr, const std::string& ini)
{
  auto s_tree  = tree::instance();
  if (!ini.empty())
    s_tree->reread(ini);

  for(auto& section : s_tree->m_tree) {
    ostr << "[" << section.first << "]\n";
    for (auto& key:section.second) {
      ostr << key.first << " = " << key.second.get_value<std::string>() << std::endl;
    }
  }
  return ostr;
}

} // detail

}}
