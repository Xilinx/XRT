/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Author(s): Hem C. Neema, Ryan Radjabi
 * PCIe HAL Driver layered on top of XOCL GEM kernel driver
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

#include <stdexcept>
#include <cassert>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <mutex>
#include <regex>
#include <sys/stat.h>
#include <sys/file.h>
#include <poll.h>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "xclbin.h"
#include "scan.h"
#include "core/common/utils.h"

#define RENDER_NM       "renderD"
#define DEV_TIMEOUT	90 // seconds
#define MGMT_DRV_V1	"xclmgmt"
#define USER_DRV_V1	"xocl"
#define MGMT_DRV_V2	"xrt-mgmt"
#define USER_DRV_V2	"xrt-user"

namespace {

namespace bfs = boost::filesystem;


static std::string
get_name(const std::string& dir, const std::string& subdir)
{
  std::string line;
  std::ifstream ifs(dir + "/" + subdir + "/name");

  if (ifs.is_open())
    std::getline(ifs, line);

  return line;
}

// Helper to find subdevice directory name
// Assumption: all subdevice's sysfs directory name starts with subdevice name!!
static int
get_subdev_dir_name(const std::string& dir, const std::string& subDevName, std::string& subdir)
{
  DIR *dp;
  size_t sub_nm_sz = subDevName.size();

  subdir = "";
  if (subDevName.empty())
    return 0;

  int ret = -ENOENT;
  dp = opendir(dir.c_str());
  if (dp) {
    struct dirent *entry;
    while ((entry = readdir(dp))) {
      std::string nm = get_name(dir, entry->d_name);
      if (!nm.empty()) {
        if (nm != subDevName)
          continue;
      } else if(strncmp(entry->d_name, subDevName.c_str(), sub_nm_sz) ||
                entry->d_name[sub_nm_sz] != '.') {
        continue;
      }
      // found it
      subdir = entry->d_name;
      ret = 0;
      break;
    }
    closedir(dp);
  }

  return ret;
}

static bool
is_admin()
{
  return (getuid() == 0) || (geteuid() == 0);
}

static size_t
bar_size(const std::string &dir, unsigned bar)
{
  std::ifstream ifs(dir + "/resource");
  if (!ifs.good())
    return 0;
  std::string line;
  for (unsigned i = 0; i <= bar; i++) {
    line.clear();
    std::getline(ifs, line);
  }
  long long start, end, meta;
  if (sscanf(line.c_str(), "0x%llx 0x%llx 0x%llx", &start, &end, &meta) != 3)
    return 0;
  return end - start + 1;
}

static int
get_render_value(const std::string& dir)
{
  struct dirent *entry;
  DIR *dp;
  int instance_num = INVALID_ID;

  dp = opendir(dir.c_str());
  if (dp == NULL)
    return instance_num;

  while ((entry = readdir(dp))) {
    if(strncmp(entry->d_name, RENDER_NM, sizeof (RENDER_NM) - 1) == 0) {
      sscanf(entry->d_name, RENDER_NM "%d", &instance_num);
      break;
    }
  }

  closedir(dp);

  return instance_num;
}

/*
 * wordcopy()
 *
 * Copy bytes word (32bit) by word.
 * Neither memcpy, nor std::copy work as they become byte copying
 * on some platforms.
 */
inline void*
wordcopy(void *dst, const void* src, size_t bytes)
{
  // assert dest is 4 byte aligned
  assert((reinterpret_cast<intptr_t>(dst) % 4) == 0);

  using word = uint32_t;
  volatile auto d = reinterpret_cast<word*>(dst);
  auto s = reinterpret_cast<const word*>(src);
  auto w = bytes/sizeof(word);

  for (size_t i=0; i<w; ++i)
    d[i] = s[i];

  return dst;
}

} // namespace

namespace pcidev {

namespace sysfs {

static const std::string dev_root = "/sys/bus/pci/devices/";
static const std::string drv_root = "/sys/bus/pci/drivers/";

static std::string
get_path(const std::string& name, const std::string& subdev, const std::string& entry)
{
  std::string subdir;
  if (get_subdev_dir_name(dev_root + name, subdev, subdir) != 0)
    return "";

  auto path = dev_root;
  path += name;
  path += "/";
  path += subdir;
  path += "/";
  path += entry;
  return path;
}

static std::fstream
open_path(const std::string& path, std::string& err, bool write, bool binary)
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

static std::fstream
open(const std::string& name,
     const std::string& subdev, const std::string& entry,
     std::string& err, bool write, bool binary)
{
  std::fstream fs;
  auto path = get_path(name, subdev, entry);

  if (path.empty()) {
    std::stringstream ss;
    ss << "Failed to find subdirectory for " << subdev
       << " under " << dev_root + name << std::endl;
    err = ss.str();
  } else {
    fs = open_path(path, err, write, binary);
  }

  return fs;
}

static void
get(const std::string& name,
    const std::string& subdev, const std::string& entry,
    std::string& err, std::vector<std::string>& sv)
{
  std::fstream fs = open(name, subdev, entry, err, false, false);
  if (!err.empty())
    return;

  sv.clear();
  std::string line;
  while (std::getline(fs, line))
    sv.push_back(line);
}

static void
get(const std::string& name,
    const std::string& subdev, const std::string& entry,
    std::string& err, std::vector<uint64_t>& iv)
{
  iv.clear();

  std::vector<std::string> sv;
  get(name, subdev, entry, err, sv);
  if (!err.empty())
    return;

  for (auto& s : sv) {
    if (s.empty()) {
      std::stringstream ss;
      ss << "Reading " << get_path(name, subdev, entry) << ", ";
      ss << "can't convert empty string to integer" << std::endl;
      err = ss.str();
      break;
    }
    char* end = nullptr;
    auto n = std::strtoull(s.c_str(), &end, 0);
    if (*end != '\0') {
      std::stringstream ss;
      ss << "Reading " << get_path(name, subdev, entry) << ", ";
      ss << "failed to convert string to integer: " << s << std::endl;
      err = ss.str();
      break;
    }
    iv.push_back(n);
  }
}

static void
get(const std::string& name,
    const std::string& subdev, const std::string& entry,
    std::string& err, std::string& s)
{
  std::vector<std::string> sv;
  get(name, subdev, entry, err, sv);
  if (!sv.empty())
    s = sv[0];
  else
    s = ""; // default value
}

static void
get(const std::string& name,
    const std::string& subdev, const std::string& entry,
    std::string& err, std::vector<char>& buf)
{
  std::fstream fs = open(name, subdev, entry, err, false, true);
  if (!err.empty())
    return;

  buf.clear();
  buf.insert(std::end(buf),std::istreambuf_iterator<char>(fs),
             std::istreambuf_iterator<char>());
}

static void
put(const std::string& name,
    const std::string& subdev, const std::string& entry,
    std::string& err, const std::string& input)
{
  std::fstream fs = open(name, subdev, entry, err, true, false);
  if (!err.empty())
    return;
  fs << input;
  fs.close(); // flush and close, if either fails then stream failbit is set.
  if (!fs.good()) {
    std::stringstream ss;
    ss << "Failed to write " << get_path(name, subdev, entry) << ": "
       << strerror(errno) << std::endl;
    err = ss.str();
  }
}

static void
put(const std::string& name,
    const std::string& subdev, const std::string& entry,
    std::string& err, const std::vector<char>& buf)
{
  std::fstream fs = open(name, subdev, entry, err, true, true);
  if (!err.empty())
    return;

  fs.write(buf.data(), buf.size());
  fs.close(); // flush and close, if either fails then stream failbit is set.
  if (!fs.good()) {
    std::stringstream ss;
    ss << "Failed to write " << get_path(name, subdev, entry) << ": "
       << strerror(errno) << std::endl;
    err = ss.str();
  }
}

static void
put(const std::string& name,
    const std::string& subdev, const std::string& entry,
    std::string& err, const unsigned int& input)
{
  std::fstream fs = open(name, subdev, entry, err, true, false);
  if (!err.empty())
    return;
  fs << input;
  fs.close(); // flush and close, if either fails then stream failbit is set.
  if (!fs.good()) {
    std::stringstream ss;
    ss << "Failed to write " << get_path(name, subdev, entry) << ": "
       << strerror(errno) << std::endl;
    err = ss.str();
  }
}

} // sysfs

static bool is_in_use(std::vector<std::shared_ptr<pci_device>>& vec)
{
  for (auto& d : vec)
    if (d.use_count() > 1)
      return true;
  return false;
}

static bool is_drv_v2(const std::string& driver)
{
  return ((driver.compare(MGMT_DRV_V2) == 0) || (driver.compare(USER_DRV_V2) == 0));
}

static bool is_drv_mgmt(const std::string& driver)
{
  return ((driver.compare(MGMT_DRV_V1) == 0) || (driver.compare(MGMT_DRV_V2) == 0));
}

void
pci_device::
sysfs_get(const std::string& subdev, const std::string& entry,
          std::string& err, std::vector<std::string>& ret)
{
  sysfs::get(sysfs_name, subdev, entry, err, ret);
}

void
pci_device::
sysfs_get(const std::string& subdev, const std::string& entry,
          std::string& err, std::vector<uint64_t>& ret)
{
  sysfs::get(sysfs_name, subdev, entry, err, ret);
}

void
pci_device::
sysfs_get(const std::string& subdev, const std::string& entry,
          std::string& err, std::vector<char>& ret)
{
  sysfs::get(sysfs_name, subdev, entry, err, ret);
}

void
pci_device::
sysfs_get(const std::string& subdev, const std::string& entry,
          std::string& err, std::string& s)
{
  sysfs::get(sysfs_name, subdev, entry, err, s);
}


void
pci_device::
sysfs_put(const std::string& subdev, const std::string& entry,
          std::string& err, const std::string& input)
{
  sysfs::put(sysfs_name, subdev, entry, err, input);
}

void
pci_device::
sysfs_put(const std::string& subdev, const std::string& entry,
          std::string& err, const std::vector<char>& buf)
{
  sysfs::put(sysfs_name, subdev, entry, err, buf);
}

void
pci_device::
sysfs_put(const std::string& subdev, const std::string& entry,
          std::string& err, const unsigned int& buf)
{
  sysfs::put(sysfs_name, subdev, entry, err, buf);
}

std::string
pci_device::
get_sysfs_path(const std::string& subdev, const std::string& entry)
{
  return sysfs::get_path(sysfs_name, subdev, entry);
}

std::string
pci_device::
get_subdev_path(const std::string& subdev, uint idx)
{
  // Main devfs path
  if (subdev.empty()) {
    std::string instStr = std::to_string(instance);
    if (is_mgmt()) {
      std::string prefixStr = "/dev/xclmgmt";
      return prefixStr + instStr;
    }
    std::string prefixStr = "/dev/dri/" RENDER_NM;
    return prefixStr + instStr;
  }

  // Subdev devfs path
  std::string path("/dev/xfpga/");

  path += subdev;
  path += is_mgmt() ? ".m" : ".u";
  //if the domain number is big, the shift overflows, hence need to cast
  uint32_t dom = static_cast<uint32_t>(domain);
  path += std::to_string( (dom<<16)+ (bus<<8) + (dev<<3) + func);
  path += "." + std::to_string(idx);
  return path;
}

int
pci_device::
open(const std::string& subdev, uint32_t idx, int flag)
{
  if (is_mgmt() && !::is_admin())
    throw std::runtime_error("Root privileges required");

  std::string devfs = get_subdev_path(subdev, idx);
  return ::open(devfs.c_str(), flag);
}

int
pci_device::
open(const std::string& subdev, int flag)
{
  return open(subdev, 0, flag);
}

pci_device::
pci_device(const std::string& drv_name, const std::string& sysfs) : sysfs_name(sysfs)
{
  uint16_t dom, b, d, f;
  if(sscanf(sysfs.c_str(), "%hx:%hx:%hx.%hx", &dom, &b, &d, &f) < 4)
    return;
  domain = dom;
  bus = b;
  dev = d;
  func = f;

  // Determine if device is of supported vendor
  std::string err;
  sysfs_get<uint16_t>("", "vendor", err, vendor_id, INVALID_ID);
  if (!err.empty()) {
    std::cout << err << std::endl;
    return;
  }
  if ((vendor_id != XILINX_ID) && (vendor_id != ADVANTECH_ID) &&
    (vendor_id != AWS_ID) && (vendor_id != ARISTA_ID))
    return;
  sysfs_get<uint16_t>("", "device", err, device_id, INVALID_ID);
  mgmt = is_drv_mgmt(drv_name);

  if (is_mgmt())
    sysfs_get("", "instance", err, instance, static_cast<uint32_t>(INVALID_ID));
  else
    instance = get_render_value(sysfs::dev_root + sysfs + "/drm");

  sysfs_get<int>("", "userbar", err, user_bar, 0);
  user_bar_size = bar_size(sysfs::dev_root + sysfs, user_bar);
  sysfs_get<bool>("", "ready", err, is_ready, false);
}

pci_device::
~pci_device()
{
  if (user_bar_map != MAP_FAILED)
    ::munmap(user_bar_map, user_bar_size);
}

int
pci_device::
map_usr_bar()
{
  std::lock_guard<std::mutex> l(lock);

  if (user_bar_map != MAP_FAILED)
    return 0;

  int dev_handle = open("", O_RDWR);
  if (dev_handle < 0)
    return -errno;

  user_bar_map = (char *)::mmap(0, user_bar_size,
                                PROT_READ | PROT_WRITE, MAP_SHARED, dev_handle, 0);

  // Mapping should stay valid after handle is closed
  // (according to man page)
  (void)close(dev_handle);

  if (user_bar_map == MAP_FAILED)
    return -errno;

  return 0;
}

void
pci_device::
close(int dev_handle)
{
  if (dev_handle != -1)
    (void)::close(dev_handle);
}


int
pci_device::
pcieBarRead(uint64_t offset, void* buf, uint64_t len)
{
  if (user_bar_map == MAP_FAILED) {
    int ret = map_usr_bar();
    if (ret)
      return ret;
  }
  (void) wordcopy(buf, user_bar_map + offset, len);
  return 0;
}

int
pci_device::
pcieBarWrite(uint64_t offset, const void* buf, uint64_t len)
{
  if (user_bar_map == MAP_FAILED) {
    int ret = map_usr_bar();
    if (ret)
      return ret;
  }
  (void) wordcopy(user_bar_map + offset, buf, len);
  return 0;
}

int
pci_device::
ioctl(int dev_handle, unsigned long cmd, void *arg)
{
  if (dev_handle == -1) {
    errno = -EINVAL;
    return -1;
  }
  return ::ioctl(dev_handle, cmd, arg);
}

int
pci_device::
poll(int dev_handle, short events, int timeoutMilliSec)
{
  pollfd info = {dev_handle, events, 0};
  return ::poll(&info, 1, timeoutMilliSec);
}

void*
pci_device::
mmap(int dev_handle, size_t len, int prot, int flags, off_t offset)
{
  if (dev_handle == -1) {
    errno = -EINVAL;
    return MAP_FAILED;
  }
  return ::mmap(0, len, prot, flags, dev_handle, offset);
}

int
pci_device::
munmap(int dev_handle, void* addr, size_t len)
{
  if (dev_handle == -1) {
    errno = -EINVAL;
    return -1;
  }
  return ::munmap(addr, len);
}

int
pci_device::
get_partinfo(std::vector<std::string>& info, void *blob)
{
  std::vector<char> buf;
  if (!blob) {
    std::string err;
    sysfs_get("", "fdt_blob", err, buf);
    if (!buf.size())
      return -ENOENT;

    blob = buf.data();
  }

  struct fdt_header *bph = (struct fdt_header *)blob;
  uint32_t version = be32toh(bph->version);
  uint32_t off_dt = be32toh(bph->off_dt_struct);
  const char *p_struct = (const char *)blob + off_dt;
  uint32_t off_str = be32toh(bph->off_dt_strings);
  const char *p_strings = (const char *)blob + off_str;
  const char *p, *s;
  uint32_t tag;
  uint32_t level = 0;

  p = p_struct;
  while ((tag = be32toh(GET_CELL(p))) != FDT_END) {
    if (tag == FDT_BEGIN_NODE) {
      s = p;
      p = PALIGN(p + strlen(s) + 1, 4);
      std::regex e("partition_info_([0-9]+)");
      std::cmatch cm;
      std::regex_match(s, cm, e);
      if (cm.size())
        level = std::stoul(cm.str(1));
      continue;
    }

    if (tag != FDT_PROP)
      continue;

    int sz = be32toh(GET_CELL(p));
    s = p_strings + be32toh(GET_CELL(p));
    if (version < 16 && sz >= 8)
      p = PALIGN(p, 8);

    if (strcmp(s, "__INFO")) {
      p = PALIGN(p + sz, 4);
      continue;
    }

    if (info.size() <= level)
      info.resize(level + 1);

    info[level] = std::string(p);

    p = PALIGN(p + sz, 4);
  }
  return 0;
}

int
pci_device::
flock(int dev_handle, int op)
{
  if (dev_handle == -1) {
    errno = -EINVAL;
    return -1;
  }
  return ::flock(dev_handle, op);
}

std::shared_ptr<pci_device>
pci_device::
lookup_peer_dev()
{
  if (!is_mgmt())
    return nullptr;

  int i = 0;
  for (auto udev = get_dev(i, true); udev; udev = get_dev(i, true), ++i)
    if (udev->domain == domain && udev->bus == bus && udev->dev == dev)
      return udev;

  return nullptr;
}

class pci_device_v2 : public pci_device
{
public:
  pci_device_v2(const std::string& drv_name, const std::string& sysfs_name) :
    pci_device(drv_name, sysfs_name)
  {
    std::string err;
    sysfs_get<bool>("", "ready", err, is_ready, false);
  }
  template <typename T>
  void
  sysfs_get(const std::string& subdev, const std::string& entry,
            std::string& err, T& i, const T& default_val)
  {
    std::vector<uint64_t> iv;
    sysfs_get(subdev, entry, err, iv);
    if (!iv.empty())
      i = static_cast<T>(iv[0]);
    else
      i = static_cast<T>(default_val); // default value
  }
  void sysfs_get(const std::string& subdev, const std::string& entry,
    std::string& err, std::vector<std::string>& sv)
  {
    try {
      auto map = find_sysfs_map(subdev, entry);
      sysfs::get(sysfs_name, map.subdev_v2, map2entry(map, entry), err, sv);
    } catch (...) {
      throw std::runtime_error("sysfs_get_sv(" + subdev + "/" + entry + ") is not supported");
    }
  }
  void sysfs_get(const std::string& subdev, const std::string& entry,
    std::string& err, std::vector<uint64_t>& iv)
  {
    try {
      if (subdev.compare("") == 0 && entry.compare("mfg") == 0) {
          bool golden = !sysfs::get_path(sysfs_name, "xrt_vsec_golden", "").empty();
          iv.push_back(golden);
      } else {
        auto map = find_sysfs_map(subdev, entry);
        sysfs::get(sysfs_name, map.subdev_v2, map2entry(map, entry), err, iv);
      }
    } catch (...) {
      throw std::runtime_error("sysfs_get_iv(" + subdev + "/" + entry + ") is not supported");
    }
  }
  void sysfs_get(const std::string& subdev, const std::string& entry,
    std::string& err, std::string& s)
  {
    try {
      if (subdev.compare("rom") == 0 && entry.compare("VBNV") == 0) {
        sysfs::get(sysfs_name, "xmgmt_main", "VBNV", err, s);
        if (!err.empty())
          sysfs::get(sysfs_name, "xrt_vsec_golden", "VBNV", err, s);
      } else {
        auto map = find_sysfs_map(subdev, entry);
        sysfs::get(sysfs_name, map.subdev_v2, map2entry(map, entry), err, s);
      }
    } catch (...) {
      throw std::runtime_error("sysfs_get_s(" + subdev + "/" + entry + ") is not supported");
    }
  }
  void sysfs_get(const std::string& subdev, const std::string& entry,
    std::string& err, std::vector<char>& buf)
  {
    try {
      auto map = find_sysfs_map(subdev, entry);
      sysfs::get(sysfs_name, map.subdev_v2, map2entry(map, entry), err, buf);
    } catch (...) {
      throw std::runtime_error("sysfs_get_cv(" + subdev + "/" + entry + ") is not supported");
    }
  }
  void sysfs_put(const std::string& subdev, const std::string& entry,
    std::string& err, const std::string& input)
  {
    throw std::runtime_error("sysfs_put_s(" + subdev + "/" + entry + ") is not supported");
  }
  void sysfs_put(const std::string& subdev, const std::string& entry,
    std::string& err, const std::vector<char>& buf)
  {
    throw std::runtime_error("sysfs_put_cv(" + subdev + "/" + entry + ") is not supported");
  }
  void sysfs_put(const std::string& subdev, const std::string& entry,
    std::string& err, const unsigned int& buf)
  {
    throw std::runtime_error("sysfs_put_i(" + subdev + "/" + entry + ") is not supported");
  }
  std::string get_sysfs_path(const std::string& subdev, const std::string& entry)
  {
    try {
      auto map = find_sysfs_map(subdev, entry);
      return sysfs::get_path(sysfs_name, map.subdev_v2, map2entry(map, entry));
    } catch (...) {
      throw std::runtime_error("sysfs_get_cv(" + subdev + "/" + entry + ") is not supported");
    }
  }
  std::string get_subdev_path(const std::string& subdev, uint32_t idx)
  {
    std::string path("/dev/xrt/");
    path += sysfs_name;
    path += "/";
    try {
      auto map = find_devfs_map(subdev);
      path += map.subdev_v2;
    } catch (...) {
      path += subdev;
    }
    if (idx != (uint32_t)-1)
      path += "." + std::to_string(idx);
    return path;
  }
  int open(const std::string& subdev, int flag)
  {
    return open(subdev, -1, flag);
  }
  int open(const std::string& subdev, uint32_t idx, int flag)
  {
    if (is_mgmt() && !::is_admin())
      throw std::runtime_error("Root privileges required");

    std::string devfs = get_subdev_path(subdev, idx);
    return ::open(devfs.c_str(), flag);
  }
  void *mmap(int devhdl, size_t len, int prot, int flags, off_t offset)
  {
    throw std::runtime_error("mmap is not supported");
  }
  int munmap(int devhdl, void* addr, size_t len)
  {
    throw std::runtime_error("munmap is not supported");
  }
  int pcieBarRead(uint64_t offset, void *buf, uint64_t len)
  {
    throw std::runtime_error("pcieBarRead is not supported");
  }
  int pcieBarWrite(uint64_t offset, const void *buf, uint64_t len)
  {
    throw std::runtime_error("pcieBarWrite is not supported");
  }

private:
  // For sysfs node mapping
  struct sysfs_node_map {
    const std::string subdev;
    const std::string entry;
    const std::string subdev_v2;
    const std::string entry_v2;
  };
  const std::vector<sysfs_node_map> sysfs_map {
    // Map as-is
    { "",       "ready",        "",             "ready" },
    { "",       "vendor",       "",             "vendor" },
    { "",       "device",       "",             "device" },
    // rom/xxx
    { "rom",    "uuid",         "xmgmt_main",   "logic_uuids" },
    { "rom",    "*",            "xmgmt_main",   "*" },
    // root/xxx
    { "",       "*",            "xmgmt_main",   "*" },
    // xmc/xxx
    { "xmc",    "*",            "xrt_cmc",     "*" },
    // flash/xxx
    { "flash",  "*",            "xrt_qspi",    "*" },
  };
  const sysfs_node_map& find_sysfs_map(const std::string& subdev, const std::string& entry)
  {
    for (auto& m : sysfs_map) {
      if (subdev == m.subdev && (entry == m.entry || m.entry.compare("*") == 0)) {
#ifdef MAP_DEBUG
        std::cout << "map <" << subdev << "/" << entry << "> to <" <<
          m.subdev_v2 << "/" << map2entry(m, entry) << ">" << std::endl;
#endif
        return m;
      }
    }
    throw std::runtime_error("can't map <" + subdev + "/" + entry + ">");
  }
  const std::string& map2entry(const sysfs_node_map& map, const std::string& entry)
  {
    return map.entry_v2.compare("*") ? map.entry_v2 : entry;
  }

  // For devfs node mapping
  struct devfs_node_map {
    const std::string subdev;
    const std::string subdev_v2;
  };
  const std::vector<devfs_node_map> devfs_map {
    { "",        "xmgmt" },
    { "xmc",     "cmc" },
  };
  const devfs_node_map& find_devfs_map(const std::string& subdev)
  {
    for (auto& m : devfs_map) {
      if (subdev == m.subdev) {
#ifdef MAP_DEBUG
        std::cout << "map " << subdev " to " << m.subdev_v2 << std::endl;
#endif
        return m;
      }
    }
    throw std::runtime_error("can't map " + subdev);
  }
};

class pci_device_scanner
{
public:
  static pci_device_scanner*
  get_scanner()
  {
    static pci_device_scanner scanner;
    return &scanner;
  }

  void
  rescan()
  {
    std::lock_guard<std::mutex> l(lock);

    if (is_in_use(user_list) || is_in_use(mgmt_list)) {
      std::cout << "Device list is in use, can't rescan" << std::endl;
      return;
    }

    user_list.clear();
    mgmt_list.clear();

    rescan_nolock(MGMT_DRV_V1);
    rescan_nolock(USER_DRV_V1);
    rescan_nolock(MGMT_DRV_V2);
    rescan_nolock(USER_DRV_V2);
  }

  size_t
  get_num_ready(bool is_user)
  {
    std::lock_guard<std::mutex> l(lock);
    return is_user ? num_user_ready : num_mgmt_ready;
  }

  size_t
  get_num_total(bool is_user)
  {
    std::lock_guard<std::mutex> l(lock);
    return is_user ? user_list.size() : mgmt_list.size();
  }

  const std::shared_ptr<pci_device>
  get_dev(unsigned index,bool user)
  {
    std::lock_guard<std::mutex> l(lock);
    auto list = user ? &user_list : &mgmt_list;
    if (index >= list->size())
      return nullptr;
    return (*list)[index];
  }

private:
  void rescan_nolock(const std::string driver)
  {
    const std::string drvpath = sysfs::drv_root + driver;
    if(!bfs::exists(drvpath))
      return;

    // Gather all sysfs directory and sort
    std::vector<bfs::path> vec{bfs::directory_iterator(drvpath), bfs::directory_iterator()};
    std::sort(vec.begin(), vec.end());

    for (auto& path : vec) {
      std::shared_ptr<pci_device> pf;
      if (!is_drv_v2(driver))
        pf = std::make_shared<pci_device>(driver, path.filename().string());
      else
        pf = std::make_shared<pci_device_v2>(driver, path.filename().string());
      if(!pf || pf->domain == INVALID_ID)
        continue;

      // In docker, all host sysfs nodes are available. So, we need to check
      // devnode to make sure the device is really assigned to docker. For
      // xoclv2 driver, we only have flash devnode when running golden image.
      if (!bfs::exists(pf->get_subdev_path("", -1)) &&
        !bfs::exists(pf->get_subdev_path("flash", -1)))
        continue;

      auto& list = pf->is_mgmt() ? mgmt_list : user_list;
      auto& num_ready = pf->is_mgmt() ? num_mgmt_ready : num_user_ready;
      if (pf->is_ready) {
        list.insert(list.begin(), pf);
        ++num_ready;
      } else {
        list.push_back(pf);
      }
    }
  }

  pci_device_scanner()
  {
    rescan();
  }

  std::mutex lock;

  // Full list of discovered user devices. Index 0 ~ (num_user_ready - 1) are
  // boards ready for use. The rest, if any, are not ready, according to what
  // is indicated by driver's "ready" sysfs entry. The applications only see
  // ready-for-use boards since xclProbe returns num_user_ready, not the size
  // of the full list.
  std::vector<std::shared_ptr<pci_device>> user_list;
  size_t num_user_ready;

  // Full list of discovered mgmt devices. Index 0 ~ (num_mgmt_ready - 1) are
  // boards ready for use. The rest, if any, are not ready, according to what
  // is indicated by driver's "ready" sysfs entry. Application does not see
  // mgmt devices.
  std::vector<std::shared_ptr<pci_device>> mgmt_list;
  size_t num_mgmt_ready;

};

void
rescan(void)
{
  pci_device_scanner::get_scanner()->rescan();
}

size_t
get_dev_ready(bool user)
{
  return pci_device_scanner::get_scanner()->get_num_ready(user);
}

size_t
get_dev_total(bool user)
{
  return pci_device_scanner::get_scanner()->get_num_total(user);
}

std::shared_ptr<pci_device>
get_dev(unsigned index, bool user)
{
  return pci_device_scanner::get_scanner()->get_dev(index, user);
}

int
get_axlf_section(const std::string& filename, int kind, std::shared_ptr<char>& buf)
{
  std::ifstream in(filename);
  if (!in.is_open()) {
    std::cout << "Can't open " << filename << std::endl;
    return -ENOENT;
  }

  // Read axlf from dsabin file to find out number of sections in total.
  axlf a;
  size_t sz = sizeof (axlf);
  in.read(reinterpret_cast<char *>(&a), sz);
  if (!in.good()) {
      std::cout << "Can't read axlf from "<< filename << std::endl;
      return -EINVAL;
  }
  // Reread axlf from dsabin file, including all sections headers.
  // Sanity check for number of sections coming from user input file
  if (a.m_header.m_numSections > XCLBIN_MAX_NUM_SECTION)
    return -EINVAL;

  sz = sizeof (axlf) + sizeof (axlf_section_header) * (a.m_header.m_numSections - 1);

  std::vector<char> top(sz);
  in.seekg(0);
  in.read(top.data(), sz);
  if (!in.good()) {
    std::cout << "Can't read axlf and section headers from "<< filename << std::endl;
    return -EINVAL;
  }
  const axlf *ap = reinterpret_cast<const axlf *>(top.data());

  const axlf_section_header* section = xclbin::get_axlf_section(ap, (enum axlf_section_kind)kind);
  if (!section)
    return -EINVAL;

  buf = std::shared_ptr<char>(new char[section->m_sectionSize]);
  in.seekg(section->m_sectionOffset);
  in.read(buf.get(), section->m_sectionSize);

  return 0;
}

int
get_uuids(std::shared_ptr<char>& dtbbuf, std::vector<std::string>& uuids)
{
  struct fdt_header *bph = (struct fdt_header *)dtbbuf.get();
  uint32_t version = be32toh(bph->version);
  uint32_t off_dt = be32toh(bph->off_dt_struct);
  const char *p_struct = (const char *)bph + off_dt;
  uint32_t off_str = be32toh(bph->off_dt_strings);
  const char *p_strings = (const char *)bph + off_str;
  const char *p, *s;
  uint32_t tag;
  int sz;

  p = p_struct;
  uuids.clear();
  while ((tag = be32toh(GET_CELL(p))) != FDT_END) {
    if (tag == FDT_BEGIN_NODE) {
      s = p;
      p = PALIGN(p + strlen(s) + 1, 4);
      continue;
    }
    if (tag != FDT_PROP)
      continue;

    sz = be32toh(GET_CELL(p));
    s = p_strings + be32toh(GET_CELL(p));
    if (version < 16 && sz >= 8)
      p = PALIGN(p, 8);

    if (!strcmp(s, "logic_uuid"))
      uuids.insert(uuids.begin(), std::string(p));
    else if (!strcmp(s, "interface_uuid"))
      uuids.push_back(std::string(p));

    p = PALIGN(p + sz, 4);
  }

  return uuids.size() ? 0 : -EINVAL;
}

/*
 * This is for the RHEL 8.x kernel. From the RHEL 8.x kernel removed the runtime_active_kids sysfs
 * node for the Linux power driver. Hence, to get the active kids under a bridge we need this
 * alternative solution.
 */
int
get_runtime_active_kids(std::string &pci_bridge_path)
{
  int curr_act_dev = 0;
  std::vector<bfs::path> vec{bfs::directory_iterator(pci_bridge_path), bfs::directory_iterator()};

  // Check number of Xilinx devices under this bridge.
  for (auto& path : vec) {
    if (!bfs::is_directory(path))
      continue;

    path += "/vendor";
    if(!bfs::exists(path))
	    continue;

    unsigned int vendor_id;
    bfs::ifstream file(path);
    file >> std::hex >> vendor_id;
    if (vendor_id != XILINX_ID)
	    continue;

    curr_act_dev++;
  }

  return curr_act_dev;
}

int
shutdown(std::shared_ptr<pci_device> mgmt_dev, bool remove_user, bool remove_mgmt)
{
  if (!mgmt_dev->is_mgmt())
    return -EINVAL;

  auto udev = mgmt_dev->lookup_peer_dev();
  if (!udev) {
    std::cout << "ERROR: User function is not found. " <<
      "This is probably due to user function is running in virtual machine or user driver is not loaded. " << std::endl;
    return -ECANCELED;
  }

  std::cout << "Stopping user function..." << std::endl;
  // This will trigger hot reset on device.
  std::string errmsg;
  udev->sysfs_put("", "shutdown", errmsg, "1\n");
  if (!errmsg.empty()) {
    std::cout << "ERROR: Shutdown user function failed." << std::endl;
    return -EINVAL;
  }

  // Poll till shutdown is done.
  int userShutdownStatus = 0;
  int mgmtOfflineStatus = 1;
  for (int wait = 0; wait < DEV_TIMEOUT; wait++) {
    sleep(1);

    udev->sysfs_get<int>("", "shutdown", errmsg, userShutdownStatus, EINVAL);
    if (!errmsg.empty())
      // Ignore the error since sysfs nodes will be removed during hot reset.
      continue;

    if (userShutdownStatus != 1)
      continue;

    // User shutdown is done successfully. Now needs to wait for mgmt
    // to finish reset. By the time we got here mgmt pf should be offline.
    // We just need to wait for it to be online again.
    mgmt_dev->sysfs_get<int>("", "dev_offline", errmsg, mgmtOfflineStatus, EINVAL);
    if (!errmsg.empty()) {
      std::cout << "ERROR: Can't read mgmt dev_offline: " << errmsg << std::endl;
      break;
    }
    if (mgmtOfflineStatus == 0)
      break; // Shutdown is completed
  }

  if (userShutdownStatus != 1 || mgmtOfflineStatus != 0) {
    std::cout << "ERROR: Shutdown user function timeout." << std::endl;
    return -ETIMEDOUT;
  }

  if (!remove_user && !remove_mgmt)
    return 0;

  /* Cache the parent sysfs path before remove the PF */
  std::string parent_path = mgmt_dev->get_sysfs_path("", "dparent");
  /* Get the absolute path from the symbolic link */
  parent_path = (bfs::canonical(parent_path)).c_str();

  int active_dev_num;
  mgmt_dev->sysfs_get<int>("", "dparent/power/runtime_active_kids", errmsg, active_dev_num, EINVAL);
  if (!errmsg.empty()) {
    // RHEL 8.x onwards this sysfs node is deprecated
    active_dev_num = get_runtime_active_kids(parent_path);
    if (!active_dev_num) {
      std::cout << "ERROR: can not read active device number" << std::endl;
      return -ENOENT;
    }
  }

  int rem_dev_cnt = 0;
  if (remove_user) {
    udev->sysfs_put("", "remove", errmsg, "1\n");
    if (!errmsg.empty()) {
      std::cout << "ERROR: removing user function failed" << std::endl;
      return -EINVAL;
    }
    rem_dev_cnt++;
  }

  if (remove_mgmt) {
    mgmt_dev->sysfs_put("", "remove", errmsg, "1\n");
    if (!errmsg.empty()) {
      std::cout << "ERROR: removing mgmt function failed" << std::endl;
      return -EINVAL;
    }
    rem_dev_cnt++;
  }

  if (!rem_dev_cnt)
    return 0;

  for (int wait = 0; wait < DEV_TIMEOUT; wait++) {
    int curr_act_dev;
    std::string active_kids_path = parent_path + "/power/runtime_active_kids";
    if (!bfs::exists(active_kids_path)) {
      // RHEL 8.x specific 
      curr_act_dev = get_runtime_active_kids(parent_path);
    }
    else {
      bfs::ifstream file(active_kids_path);
      file >> curr_act_dev;
    }

    if (curr_act_dev + rem_dev_cnt == active_dev_num)
      return 0;

    sleep(1);
  }

  std::cout << "ERROR: removing device node timed out" << std::endl;

  return -ETIMEDOUT;
}

int
check_p2p_config(const std::shared_ptr<pci_device>& dev, std::string &err)
{
  std::string errmsg;
  int ret = P2P_CONFIG_DISABLED;

  if (dev->is_mgmt()) {
    return -EINVAL;
  }
  err.clear();

  std::vector<std::string> p2p_cfg;
  dev->sysfs_get("p2p", "config", errmsg, p2p_cfg);
  if (errmsg.empty()) {
    long long bar = -1;
    long long rbar = -1;
    long long remap = -1;
    long long exp_bar = -1;

    for (unsigned int i = 0; i < p2p_cfg.size(); i++) {
        const char *str = p2p_cfg[i].c_str();
        std::sscanf(str, "bar:%lld", &bar);
        std::sscanf(str, "exp_bar:%lld", &exp_bar);
        std::sscanf(str, "rbar:%lld", &rbar);
        std::sscanf(str, "remap:%lld", &remap);
    }
    if (bar == -1) {
      ret = P2P_CONFIG_NOT_SUPP;
      err = "ERROR: P2P is not supported. Cann't find P2P BAR.";
    }
    else if (rbar != -1 && rbar > bar) {
      ret = P2P_CONFIG_REBOOT;
    }
    else if (remap > 0 && remap != bar) {
      ret = P2P_CONFIG_ERROR;
      err = "ERROR: P2P remapper is not set correctly";
    }
    else if (bar == exp_bar) {
      ret = P2P_CONFIG_ENABLED;
    }

    return ret;
  }

  return P2P_CONFIG_NOT_SUPP;
}

} // namespace pcidev

std::ostream&
operator<<(std::ostream& stream, const std::shared_ptr<pcidev::pci_device>& dev)
{
  std::ios_base::fmtflags f(stream.flags());

  stream << std::hex << std::right << std::setfill('0');

  // [dddd:bb:dd.f]
  stream << std::setw(4) << dev->domain << ":"
         << std::setw(2) << dev->bus << ":"
         << std::setw(2) << dev->dev << "."
         << std::setw(1) << dev->func;

  // board/shell name
  std::string shell_name;
  std::string err;
  bool is_mfg = false;
  uint64_t ts = 0;
  dev->sysfs_get<bool>("", "mfg", err, is_mfg, false);
  if (is_mfg) {
    unsigned ver = 0;
    std::string nm;

    dev->sysfs_get("", "board_name", err, nm);
    dev->sysfs_get<unsigned>("", "mfg_ver", err, ver, 0);
    shell_name += "xilinx_";
    shell_name += nm;
    shell_name += "_GOLDEN_";
    shell_name += std::to_string(ver);
  } else {
    dev->sysfs_get("rom", "VBNV", err, shell_name);
    dev->sysfs_get<uint64_t>("rom", "timestamp", err, ts, 0);
  }
  stream << " " << shell_name;
  if (ts != 0)
    stream << "(ID=0x" << std::hex << ts << ")";

  if (dev->is_mgmt())
    stream << " mgmt";
  else
    stream << " user";

  // instance number
  if (dev->instance != INVALID_ID)
      stream << "(inst=" << std::dec << dev->instance << ")";

  stream.flags(f);
  return stream;
}

