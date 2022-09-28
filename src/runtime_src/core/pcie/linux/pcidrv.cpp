// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include <boost/filesystem.hpp>
#include "pcidrv.h"

namespace pcidrv {

void
pci_driver::
scan_devices(std::vector<std::shared_ptr<pcidev::pci_device>>& ready_list,
             std::vector<std::shared_ptr<pcidev::pci_device>>& nonready_list) const
{
  namespace bfs = boost::filesystem;
  const std::string drv_root = "/sys/bus/pci/drivers/";
  const std::string drvpath = drv_root + name();

  if(!bfs::exists(drvpath))
    return;

  // Gather all sysfs directory and sort
  std::vector<bfs::path> vec{bfs::directory_iterator(drvpath), bfs::directory_iterator()};
  std::sort(vec.begin(), vec.end());

  for (auto& path : vec) {
    try {
      auto pf = create_pcidev(path.filename().string());

      // In docker, all host sysfs nodes are available. So, we need to check
      // devnode to make sure the device is really assigned to docker.
      if (!bfs::exists(pf->get_subdev_path("", -1)))
        continue;

      // Insert detected device into proper list.
      if (pf->is_ready)
        ready_list.push_back(pf);
      else
        nonready_list.push_back(pf);
    } catch (std::invalid_argument const& ex) {
      continue;
    }
  }
}

std::shared_ptr<pcidev::pci_device>
pci_driver::
create_pcidev(const std::string& sysfs) const
{
  return std::make_shared<pcidev::pci_device>(this, sysfs);
}

} // pcidrv
