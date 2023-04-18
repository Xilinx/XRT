// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2016-2020 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#include "dev_factory.h"
#include "xclbin.h"

#include "utils.h"

#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem.hpp>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <dirent.h>
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

#define RENDER_NM       "renderD"
#define DEV_TIMEOUT	90 // seconds



namespace device_list {

  static std::vector<std::shared_ptr<xrt_core::pci::dev>> user_ready_list;
  static std::vector<std::shared_ptr<xrt_core::pci::dev>> user_nonready_list;
  static std::vector<std::shared_ptr<xrt_core::pci::dev>> mgmt_ready_list;
  static std::vector<std::shared_ptr<xrt_core::pci::dev>> mgmt_nonready_list;

  void
  append(std::vector<std::shared_ptr<xrt_core::pci::dev>> devlist, bool isuser, bool isready)
  {
      for (auto pcidev : devlist) {
          if (isuser) {
              if (isready)
                  user_ready_list.push_back(std::move(pcidev));
              else
                  user_nonready_list.push_back(std::move(pcidev));
          }
          else {
              if (isready)
                  mgmt_ready_list.push_back(std::move(pcidev));
              else
                  mgmt_nonready_list.push_back(std::move(pcidev));
          }
      }
  }
  const std::vector<std::shared_ptr<xrt_core::pci::dev>>&
  get(bool isuser, bool isready)
  {
    if (isuser) {
        if (isready)
            return user_ready_list ;
        else
            return user_nonready_list;
    }
    else {
        if (isready)
            return mgmt_ready_list;
        else
            return mgmt_nonready_list;
    }
  }
} // namespcace device_list

namespace xrt_core { namespace pci {

void
add_device_list(std::vector<std::shared_ptr<xrt_core::pci::dev>> devlist, bool isuser, bool isready)
{
    device_list::append(devlist, isuser, isready);
}

const std::vector<std::shared_ptr<xrt_core::pci::dev>>&
get_device_list(bool isuser, bool isready)
{
    return device_list::get(isuser, isready);
}

} } // namespace xrt_core :: pci
