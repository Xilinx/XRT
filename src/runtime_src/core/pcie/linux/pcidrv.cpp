// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv.h"
#include "pcidev_linux.h"
#include <boost/filesystem.hpp>

namespace xrt_core { namespace pci {

void
drv::
scan_devices(std::vector<std::shared_ptr<xrt_core::device_factory>>& dev_list) const
{
  namespace bfs = boost::filesystem;
  const std::string drv_root = "/sys/bus/pci/drivers/";
  const std::string drvpath = drv_root + name();

  if (!bfs::exists(drvpath))
    return;

  // Gather all sysfs directory and sort
  std::vector<bfs::path> vec{ bfs::directory_iterator(drvpath), bfs::directory_iterator() };
  std::sort(vec.begin(), vec.end());

  for (auto& path : vec) {
    try {
      auto pf = std::dynamic_pointer_cast<xrt_core::pci::pcidev_linux>(create_pcidev(path.filename().string()));

      // In docker, all host sysfs nodes are available. So, we need to check
      // devnode to make sure the device is really assigned to docker.
      if (!bfs::exists(pf->get_subdev_path("", -1)))
        continue;

      // Insert detected device into proper list.
      dev_list.push_back(std::move(pf));   
    }
    catch (const std::invalid_argument& ex) {
      continue;
    }
  }
}
} } // namespace xrt_core :: pci
