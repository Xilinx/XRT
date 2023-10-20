// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef _XCL_PCIDRV_H_
#define _XCL_PCIDRV_H_

#include "pcidev.h"

namespace xrt_core { namespace pci {

class drv : public std::enable_shared_from_this<drv>
{
public:
  // Name of the driver as shown under /sys/bus/pci/drivers/
  // The same name should also be driver module name as shown /sys/module
  virtual std::string
  name() const = 0;

  // If device runs user work load, it is a user pf
  virtual bool
  is_user() const = 0;

  // Prefix of the name of the device node.
  // E.g., "xclmgmt" as in /dev/xclmgmtxxxxx for Alveo mgmt pcie functions
  virtual std::string
  dev_node_prefix() const = 0;

  // Directory name of the device node.
  // E.g., "dri" as in /dev/dri/renderDxxx for Alveo user pcie functions
  virtual std::string
  dev_node_dir() const = 0;

  // Sysfs directory name for the dev node.
  // E.g., "drm" as in /sys/bus/pci/devices/0000\:61\:00.1/drm
  virtual std::string
  sysfs_dev_node_dir() const = 0;

  // Scan system, find all supported devices and add them to the list
  void
  scan_devices(std::vector<std::shared_ptr<dev>>& ready_list,
               std::vector<std::shared_ptr<dev>>& nonready_list) const;
private:
  // Create the type of pci::dev driven by this driver which can be added to the list
  virtual std::shared_ptr<xrt_core::pci::dev>
  create_pcidev(const std::string& sysfs) const;
};

} } // namespace xrt_core :: pci

#endif
