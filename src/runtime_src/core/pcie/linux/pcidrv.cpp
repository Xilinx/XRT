// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

#include "pcidrv.h"
#include <filesystem>

namespace xrt_core { namespace pci {

void
drv::
scan_devices(std::vector<std::shared_ptr<dev>>& ready_list,
             std::vector<std::shared_ptr<dev>>& nonready_list) const
{
  namespace sfs = std::filesystem;
  const std::string drv_root = "/sys/bus/pci/drivers/";
  const std::string drvpath = drv_root + name();

  if (!sfs::exists(drvpath))
    return;

  // Gather all sysfs directory and sort
  std::vector<sfs::path> vec{ sfs::directory_iterator(drvpath), sfs::directory_iterator() };
  std::sort(vec.begin(), vec.end());

  for (auto& path : vec) {
    try {
      auto pf = create_pcidev(path.filename().string());

      // In docker, all host sysfs nodes are available. So, we need to check
      // devnode to make sure the device is really assigned to docker.
      if (!sfs::exists(pf->get_subdev_path("", -1)))
        continue;

      // Insert detected device into proper list.
      if (pf->m_is_ready)
        ready_list.push_back(std::move(pf));
      else
        nonready_list.push_back(std::move(pf));
    }
    catch (const std::invalid_argument& ex) {
      continue;
    }
  }
}

std::shared_ptr<dev>
drv::
create_pcidev(const std::string& sysfs) const
{
  return std::make_shared<dev>(shared_from_this(), sysfs);
}

} } // namespace xrt_core :: pci
