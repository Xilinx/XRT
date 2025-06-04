// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights

#include "dev.h"

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

namespace xrt_core::edge {

std::string
dev::
get_sysfs_path(const std::string& entry)
{
  return sysfs_root + entry;
}

std::fstream
dev::
sysfs_open(const std::string& entry,
    std::string& err, bool write, bool binary)
{
  return sysfs_open_path(get_sysfs_path(entry), err, write, binary);
}

void
dev::
sysfs_put(const std::string& entry, std::string& err_msg,
    const std::string& input)
{
  std::fstream fs = sysfs_open(entry, err_msg, true, false);
  if (!err_msg.empty())
    return;
  fs << input;
}

void
dev::
sysfs_put(const std::string& entry, std::string& err_msg,
    const std::vector<char>& buf)
{
  std::fstream fs = sysfs_open(entry, err_msg, true, true);
  if (!err_msg.empty())
    return;
  fs.write(buf.data(), buf.size());
}

void
dev::
sysfs_get(const std::string& entry, std::string& err_msg,
    std::vector<char>& buf)
{
  std::fstream fs = sysfs_open(entry, err_msg, false, true);
  if (!err_msg.empty())
      return;
  buf.insert(std::end(buf),std::istreambuf_iterator<char>(fs),
      std::istreambuf_iterator<char>());
}

void
dev::
sysfs_get(const std::string& entry, std::string& err_msg,
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

void
dev::
sysfs_get(const std::string& entry, std::string& err_msg,
    std::vector<uint64_t>& iv)
{
  uint64_t n;
  std::vector<std::string> sv;

  iv.clear();

  sysfs_get(entry, err_msg, sv);
  if (!err_msg.empty())
    return;

  char *end;
  for (auto& s : sv) {
    std::stringstream ss;

    if (s.empty()) {
      ss << "Reading " << get_sysfs_path(entry) << ", ";
      ss << "can't convert empty string to integer" << std::endl;
          err_msg = ss.str();
          break;
    }
    n = std::strtoull(s.c_str(), &end, 0);
    if (*end != '\0') {
      ss << "Reading " << get_sysfs_path(entry) << ", ";
      ss << "failed to convert string to integer: " << s << std::endl;
      err_msg = ss.str();
      break;
    }
    iv.push_back(n);
  }
}

void
dev::
sysfs_get(const std::string& entry, std::string& err_msg,
    std::string& s)
{
  std::vector<std::string> sv;

  sysfs_get(entry, err_msg, sv);
  if (!sv.empty())
    s = sv[0];
  else
    s = ""; // default value
}
}//namespace xrt_core::edge

std::string
get_render_devname()
{
  static const std::string render_dir{"/dev/dri/"};
  static const std::string render_dev_sym_dir = render_dir + "by-path/";
  std::string render_devname;

  // On Edge platforms 'zyxclmm_drm' is the name of zocl node in device tree
  // A symlink to render device is created based on this node name
  try {
    static const std::regex filter{"platform.*zyxclmm_drm-render"};
    std::filesystem::directory_iterator end_itr;
    for (std::filesystem::directory_iterator itr( render_dev_sym_dir ); itr != end_itr; ++itr) {
      if (!std::regex_match(itr->path().filename().string(), filter))
        continue;

      if (std::filesystem::is_symlink(itr->path()))
        render_devname = std::filesystem::read_symlink(itr->path()).filename().string();

       break;
     }
  }
  catch (std::exception &e) {
    render_devname = "renderD128";
 }

  if (render_devname.empty())
    render_devname = "renderD128";

  return render_devname;
}
