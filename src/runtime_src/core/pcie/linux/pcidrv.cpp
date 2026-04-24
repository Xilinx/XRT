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

  static const std::string bus_roots[] = {
    "/sys/bus/pci/drivers/",
    "/sys/bus/rpmsg/drivers/",
    "/sys/bus/platform/drivers/",
  };

  const std::string drv_name = name();

  for (const auto& bus_root : bus_roots) {
    const std::string drvpath = bus_root + drv_name;

    if (!sfs::exists(drvpath))
      continue;

    std::vector<sfs::path> vec;
    try {
      vec.assign(sfs::directory_iterator(drvpath), sfs::directory_iterator());
    }
    catch (const sfs::filesystem_error&) {
      continue;
    }
    std::sort(vec.begin(), vec.end());

    for (auto& path : vec) {
      try {
        if (!sfs::is_symlink(path))
          continue;

        // Device entries are symlinks into /sys/devices/; skip
        // standard driver attributes (module, bind, unbind, etc.)
        auto real = sfs::canonical(path);
        if (real.string().rfind("/sys/devices/", 0) != 0)
          continue;

        // PCI: pass BDF filename (e.g. "0000:01:00.0")
        // rpmsg/platform: pass canonical device sysfs path
        auto fname = path.filename().string();
        bool is_bdf = (fname.find(':') != std::string::npos);
        std::string dev_sysfs = is_bdf ? fname : real.string();

        auto pf = create_pcidev(dev_sysfs);

        if (!pf)
          continue;

        // In docker, all host sysfs nodes are available. So, we need to check
        // devnode to make sure the device is really assigned to docker.
        if (!sfs::exists(pf->get_subdev_path("", -1)))
          continue;

        if (pf->m_is_ready)
          ready_list.push_back(std::move(pf));
        else
          nonready_list.push_back(std::move(pf));
      }
      catch (const std::exception& ex) {
        continue;
      }
    }

    if (!ready_list.empty() || !nonready_list.empty())
      return;
  }
}

std::shared_ptr<dev>
drv::
create_pcidev(const std::string& sysfs) const
{
  return std::make_shared<dev>(shared_from_this(), sysfs);
}

} } // namespace xrt_core :: pci
