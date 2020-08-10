/**
 * Copyright (C) 2020 Xilinx, Inc
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

#include <cassert>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <mutex>
#include <sys/stat.h>
#include <sys/file.h>
#include "pcidev.h"

namespace pcidev {

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

int
pci_device::
open(const std::string& subdev, int flag)
{
  int fd = -1;

  // Open legacy subdevice node
  std::string file("/dev/xfpga/");
  file += subdev;
  file += ".m";
  file += std::to_string((uint32_t)(domain<<16) + (bus<<8) + (dev<<3) + func);
  file += "." + std::to_string(0);
  fd = ::open(file.c_str(), flag);
  if (fd >= 0) {
    std::cout << "Successfully opened " << file << std::endl;
    return fd;
  }

  // Open xoclv2 subdevice node
  char bdf[20];
  std::snprintf(bdf, sizeof(bdf), "%04x:%02x:%02x.%x", domain, bus, dev, func);
  file = "/dev/xfpga/";
  file += subdev;
  file += ".";
  file += bdf;
  fd = ::open(file.c_str(), flag);
  if (fd >= 0) {
    std::cout << "Successfully opened " << file << std::endl;
    return fd;
  }

  return fd;
}

pci_device::
pci_device(const std::string& sysfs, int ubar, size_t flash_off)
  : user_bar_index(ubar), flash_offset(flash_off)
{
  uint16_t dom = 0, b, d, f;
  if((sscanf(sysfs.c_str(), "%hx:%hx:%hx.%hx", &dom, &b, &d, &f) < 4) &&
    (sscanf(sysfs.c_str(), "%hx:%hx.%hx", &b, &d, &f) < 3))
    throw std::runtime_error("Couldn't parse entry name " + sysfs);

  domain = dom;
  bus = b;
  dev = d;
  func = f;
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

  char sysfsname[20];
  std::snprintf(sysfsname, sizeof(sysfsname), "%04x:%02x:%02x.%x",
	  domain, bus, dev, func);
  std::string resfile("/sys/bus/pci/devices/");
  resfile += sysfsname;
  resfile += "/resource";
  resfile += std::to_string(user_bar_index);
  int dev_handle = ::open(resfile.c_str(), O_RDWR | O_SYNC);
  if (dev_handle < 0) {
    int err = errno;
    std::cout << "Failed to open " << resfile << " : "
      << strerror(err) << std::endl;
    return -err;
  }

  struct stat sb;
  if (fstat(dev_handle, &sb) == -1) {          // To obtain file size
    int err = errno;
    std::cout << "Failed to stat " << resfile << ": "
      << strerror(err) << std::endl;
    (void) close(dev_handle);
    return -err;
  }
  user_bar_size = sb.st_size;

  user_bar_map = (char *)::mmap(0, user_bar_size, PROT_READ | PROT_WRITE,
    MAP_SHARED, dev_handle, 0);

  // Mapping should stay valid after handle is closed
  // (according to man page)
  (void)close(dev_handle);

  if (user_bar_map == MAP_FAILED) {
    int err = errno;
    std::cout << "Failed to map " << resfile << ": "
      << strerror(err) << std::endl;
    return -err;
  }

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
    if (ret) {
      std::cout << "Failed to map in PCIE BAR."
                << " Secure boot may have been enabled on this system and can't be supported by this utiltity." << std::endl;
      return ret;
    }
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

} // namespace pcidev
