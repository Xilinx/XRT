// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2020 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
#include "pcidev.h"
#include "pcidrv.h"
#include "xrt/detail/xclbin.h"

#include "core/common/utils.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEV_TIMEOUT	90 // seconds

namespace {

namespace sfs = std::filesystem;

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
get_render_value(const std::string& dir, const std::string& devnode_prefix)
{
  struct dirent *entry;
  DIR *dp;
  int instance_num = INVALID_ID;

  dp = opendir(dir.c_str());
  if (dp == NULL)
    return instance_num;

  while ((entry = readdir(dp))) {
    std::string dirname{entry->d_name};
    if(dirname.compare(0, devnode_prefix.size(), devnode_prefix) == 0) {
      instance_num = std::stoi(dirname.substr(devnode_prefix.size()));
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

namespace xrt_core { namespace pci {

namespace sysfs {

static constexpr const char* dev_root = "/sys/bus/pci/devices/";

static std::string
get_path(const std::string& name, const std::string& subdev, const std::string& entry)
{
  std::string subdir;
  if (get_subdev_dir_name(dev_root + name, subdev, subdir) != 0)
    return "";

  std::string path = dev_root;
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

void
dev::
sysfs_get(const std::string& subdev, const std::string& entry,
          std::string& err, std::vector<std::string>& ret)
{
  sysfs::get(m_sysfs_name, subdev, entry, err, ret);
}

void
dev::
sysfs_get(const std::string& subdev, const std::string& entry,
          std::string& err, std::vector<uint64_t>& ret)
{
  sysfs::get(m_sysfs_name, subdev, entry, err, ret);
}

void
dev::
sysfs_get(const std::string& subdev, const std::string& entry,
          std::string& err, std::vector<char>& ret)
{
  sysfs::get(m_sysfs_name, subdev, entry, err, ret);
}

void
dev::
sysfs_get(const std::string& subdev, const std::string& entry,
          std::string& err, std::string& s)
{
  sysfs::get(m_sysfs_name, subdev, entry, err, s);
}

void
dev::
sysfs_put(const std::string& subdev, const std::string& entry,
          std::string& err, const std::string& input)
{
  sysfs::put(m_sysfs_name, subdev, entry, err, input);
}

void
dev::
sysfs_put(const std::string& subdev, const std::string& entry,
          std::string& err, const std::vector<char>& buf)
{
  sysfs::put(m_sysfs_name, subdev, entry, err, buf);
}

void
dev::
sysfs_put(const std::string& subdev, const std::string& entry,
          std::string& err, const unsigned int& buf)
{
  sysfs::put(m_sysfs_name, subdev, entry, err, buf);
}

std::string
dev::
get_sysfs_path(const std::string& subdev, const std::string& entry)
{
  return sysfs::get_path(m_sysfs_name, subdev, entry);
}

std::string
dev::
get_subdev_path(const std::string& subdev, uint idx) const
{
  // Main devfs path
  if (subdev.empty()) {
    std::string instStr = std::to_string(m_instance);
    std::string prefixStr = "/dev/";
    prefixStr += m_driver->dev_node_dir() + "/" + m_driver->dev_node_prefix();
    return prefixStr + instStr;
  }

  // Subdev devfs path
  std::string path("/dev/xfpga/");

  path += subdev;
  path += m_is_mgmt ? ".m" : ".u";
  //if the domain number is big, the shift overflows, hence need to cast
  uint32_t dom = static_cast<uint32_t>(m_domain);
  path += std::to_string( (dom<<16)+ (m_bus<<8) + (m_dev<<3) + m_func);
  path += "." + std::to_string(idx);
  return path;
}

int
dev::
open(const std::string& subdev, uint32_t idx, int flag) const
{
  if (m_is_mgmt && !::is_admin())
    throw std::runtime_error("Root privileges required");

  std::string devfs = get_subdev_path(subdev, idx);
  return ::open(devfs.c_str(), flag);
}

int
dev::
open(const std::string& subdev, int flag) const
{
  return open(subdev, 0, flag);
}

dev::
dev(std::shared_ptr<const drv> driver, std::string sysfs)
  : m_sysfs_name(std::move(sysfs))
  , m_driver(std::move(driver))
{
  std::string err;

  if(sscanf(m_sysfs_name.c_str(), "%hx:%hx:%hx.%hx", &m_domain, &m_bus, &m_dev, &m_func) < 4)
    throw std::invalid_argument(m_sysfs_name + " is not valid BDF");

  m_is_mgmt = !m_driver->is_user();

  if (m_is_mgmt) {
    sysfs_get("", "instance", err, m_instance, static_cast<uint32_t>(INVALID_ID));
  }
  else {
    m_instance = get_render_value(
      sysfs::dev_root + m_sysfs_name + "/" + m_driver->sysfs_dev_node_dir(),
      m_driver->dev_node_prefix());
  }

  sysfs_get<int>("", "userbar", err, m_user_bar, 0);
  m_user_bar_size = bar_size(sysfs::dev_root + m_sysfs_name, m_user_bar);
  sysfs_get<bool>("", "ready", err, m_is_ready, false);
  m_user_bar_map = reinterpret_cast<char *>(MAP_FAILED);
}

dev::
~dev()
{
  if (m_user_bar_map != MAP_FAILED)
    ::munmap(m_user_bar_map, m_user_bar_size);
}

int
dev::
map_usr_bar() const
{
  std::lock_guard<std::mutex> l(m_lock);

  if (m_user_bar_map != MAP_FAILED)
    return 0;

  int dev_handle = open("", O_RDWR);
  if (dev_handle < 0)
    return -errno;

  m_user_bar_map = (char *)::mmap(0, m_user_bar_size,
                                PROT_READ | PROT_WRITE, MAP_SHARED, dev_handle, 0);

  // Mapping should stay valid after handle is closed
  // (according to man page)
  (void)close(dev_handle);

  if (m_user_bar_map == MAP_FAILED)
    return -errno;

  return 0;
}

void
dev::
close(int dev_handle) const
{
  if (dev_handle != -1)
    (void)::close(dev_handle);
}


int
dev::
pcieBarRead(uint64_t offset, void* buf, uint64_t len) const
{
  if (m_user_bar_map == MAP_FAILED) {
    int ret = map_usr_bar();
    if (ret)
      return ret;
  }
  (void) wordcopy(buf, m_user_bar_map + offset, len);
  return 0;
}

int
dev::
pcieBarWrite(uint64_t offset, const void* buf, uint64_t len) const
{
  if (m_user_bar_map == MAP_FAILED) {
    int ret = map_usr_bar();
    if (ret)
      return ret;
  }
  (void) wordcopy(m_user_bar_map + offset, buf, len);
  return 0;
}

int
dev::
ioctl(int dev_handle, unsigned long cmd, void *arg) const
{
  if (dev_handle == -1) {
    errno = -EINVAL;
    return -1;
  }
  return ::ioctl(dev_handle, cmd, arg);
}

int
dev::
poll(int dev_handle, short events, int timeout_ms)
{
  pollfd info = {dev_handle, events, 0};
  return ::poll(&info, 1, timeout_ms);
}

void*
dev::
mmap(int dev_handle, size_t len, int prot, int flags, off_t offset)
{
  if (dev_handle == -1) {
    errno = -EINVAL;
    return MAP_FAILED;
  }
  return ::mmap(0, len, prot, flags, dev_handle, offset);
}

int
dev::
munmap(int dev_handle, void* addr, size_t len)
{
  if (dev_handle == -1) {
    errno = -EINVAL;
    return -1;
  }
  return ::munmap(addr, len);
}

int
dev::
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
dev::
flock(int dev_handle, int op)
{
  if (dev_handle == -1) {
    errno = -EINVAL;
    return -1;
  }
  return ::flock(dev_handle, op);
}

std::shared_ptr<dev>
dev::
lookup_peer_dev()
{
  if (!m_is_mgmt)
    return nullptr;

  int i = 0;
  for (auto udev = get_dev(i, true); udev; udev = get_dev(i, true), ++i)
    if (udev->m_domain == m_domain && udev->m_bus == m_bus && udev->m_dev == m_dev)
      return udev;

  return nullptr;
}
  
device::handle_type
dev::
create_shim(device::id_type id) const
{
  return xclOpen(id, nullptr, XCL_QUIET);
}

std::shared_ptr<device>
dev::
create_device(device::handle_type handle, device::id_type id) const
{
  return std::shared_ptr<device_linux>(new device_linux(handle, id, !m_is_mgmt));
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
  std::vector<sfs::path> vec{sfs::directory_iterator(pci_bridge_path), sfs::directory_iterator()};

  // Check number of Xilinx devices under this bridge.
  for (auto& path : vec) {
    if (!sfs::is_directory(path))
      continue;

    path += "/vendor";
    if(!sfs::exists(path))
	    continue;

    unsigned int vendor_id;
    std::ifstream file(path);
    file >> std::hex >> vendor_id;
    if (vendor_id != XILINX_ID)
	    continue;

    curr_act_dev++;
  }

  return curr_act_dev;
}

int
shutdown(dev *mgmt_dev, bool remove_user, bool remove_mgmt)
{
  if (!mgmt_dev->m_is_mgmt)
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
  parent_path = (sfs::canonical(parent_path)).c_str();

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

  for (int wait = 0; wait < DEV_TIMEOUT; wait++) {
    int curr_act_dev;
    std::string active_kids_path = parent_path + "/power/runtime_active_kids";
    if (!sfs::exists(active_kids_path)) {
      // RHEL 8.x specific 
      curr_act_dev = get_runtime_active_kids(parent_path);
    }
    else {
      std::ifstream file(active_kids_path);
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
check_p2p_config(const std::shared_ptr<dev>& dev, std::string &err)
{
  std::string errmsg;
  int ret = P2P_CONFIG_DISABLED;

  if (dev->m_is_mgmt) {
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

} } // namespace xrt_core :: pci
