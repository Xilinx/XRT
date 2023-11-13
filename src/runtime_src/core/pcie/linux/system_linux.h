// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef PCIE_SYSTEM_LINUX_H
#define PCIE_SYSTEM_LINUX_H

#include "pcidev.h"
#include "core/pcie/common/system_pcie.h"

namespace xrt_core {

class system_linux : public system_pcie
{
public:
  system_linux();

  void
  get_driver_info(boost::property_tree::ptree &pt);

  device::id_type
  get_device_id(const std::string& str) const;

  std::pair<device::id_type, device::id_type>
  get_total_devices(bool is_user) const;

  std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>
  get_bdf_info(device::id_type id, bool is_user) const;

  std::shared_ptr<device>
  get_userpf_device(device::id_type id) const;

  std::shared_ptr<device>
  get_userpf_device(device::handle_type device_handle, device::id_type id) const;

  std::shared_ptr<device>
  get_mgmtpf_device(device::id_type id) const;

  void
  program_plp(const device* dev, const std::vector<char> &buffer, bool force) const;

  monitor_access_type
  get_monitor_access_type() const
  {
    return monitor_access_type::ioctl;
  }

public:
  virtual
  std::shared_ptr<pci::dev>
  get_pcidev(unsigned index, bool is_user = true) const;

  virtual
  size_t
  get_num_dev_ready(bool is_user) const;

  virtual
  size_t
  get_num_dev_total(bool is_user) const;

private:
  std::vector<std::shared_ptr<pci::dev>> user_ready_list;
  std::vector<std::shared_ptr<pci::dev>> user_nonready_list;

  std::vector<std::shared_ptr<pci::dev>> mgmt_ready_list;
  std::vector<std::shared_ptr<pci::dev>> mgmt_nonready_list;
};

namespace pci {

/**
 * get_userpf_device
 * Force singleton initialization from static linking
 * with libxrt_core.
 */
std::shared_ptr<device>
get_userpf_device(device::handle_type device_handle, device::id_type id);

/**
 * get_device_id_from_bdf() - 
 * Force singleton initialization from static linking
 * with libxrt_core.
 */
device::id_type
get_device_id_from_bdf(const std::string& bdf);

/**
 * Adding driver instance to the global list. Should only be called during system_linux's
 * constructor, either explicitly for built-in drivers or through dlopen for plug-in ones.
 * For now, once added, it cannot be removed until the list itself is out of scope.
 */
void
register_driver(std::shared_ptr<drv> driver);

} // pci

} // xrt_core

#endif
