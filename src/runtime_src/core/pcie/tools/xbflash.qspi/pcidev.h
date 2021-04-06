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
#ifndef _PCIDEV_H_
#define _PCIDEV_H_

#include <string>
#include <sys/mman.h>
#include <mutex>

#define INVALID_ID ((uint16_t)-1)

namespace pcidev {

// One PCIE function on FPGA board
class pci_device
{
public:
  pci_device(const std::string& sysfs, int ubar, size_t flash_off, std::string flash_type = "");
  ~pci_device();
  int pcieBarRead(uint64_t offset, void *buf, uint64_t len);
  int pcieBarWrite(uint64_t offset, const void *buf, uint64_t len);
  int open(const std::string& subdev, int flag);
  void close(int devhdl);
  size_t get_flash_offset() { return flash_offset; }
  int get_flash_bar_index() { return user_bar_index; }
  std::string get_flash_type() { return flash_type_str; };

private:
  uint16_t domain =           INVALID_ID;
  uint16_t bus =              INVALID_ID;
  uint16_t dev =              INVALID_ID;
  uint16_t func =             INVALID_ID;

  int map_usr_bar(void);
  std::mutex lock;
  char *user_bar_map = reinterpret_cast<char *>(MAP_FAILED);
  int user_bar_index;
  size_t user_bar_size;
  size_t flash_offset;
  std::string flash_type_str;
};

} // namespace pcidev

#endif
