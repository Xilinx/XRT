/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef XRT_CORE_SYSTEM_H
#define XRT_CORE_SYSTEM_H

#include "core/common/config.h"
#include "core/common/device.h"

#include <boost/property_tree/ptree.hpp>

namespace xrt_core {

/**
 * class isystem - Representation of host system
 *
 * The system class is a singleton base class specialized by different
 * types of systems we support, e.g. linux, windows, pcie, edge.
 */
class system
{
public:
  virtual void get_xrt_info(boost::property_tree::ptree &pt) = 0;
  virtual void get_os_info(boost::property_tree::ptree &pt) = 0;
  virtual void get_devices(boost::property_tree::ptree &pt) const = 0;
  virtual std::pair<device::id_type, device::id_type> get_total_devices() const = 0;
  virtual void scan_devices(bool verbose, bool json) const = 0;
  virtual uint16_t bdf2index(const std::string& bdfStr) const = 0;


  virtual std::shared_ptr<device> get_userpf_device(device::id_type id) const = 0;
  virtual std::shared_ptr<device> get_mgmtpf_device(device::id_type id) const = 0;
}; // system

/**
 */
XRT_CORE_COMMON_EXPORT
void
get_xrt_info(boost::property_tree::ptree& pt);

/**
 */
XRT_CORE_COMMON_EXPORT
void
get_os_info(boost::property_tree::ptree& pt);

/**
 */
XRT_CORE_COMMON_EXPORT
void
get_devices(boost::property_tree::ptree& pt);

/**
 * get_total_devices() - Get total devices and total usable devices
 *
 * Return: Pair of total devices and usable devices
 */
XRT_CORE_COMMON_EXPORT
std::pair<uint64_t, uint64_t>
get_total_devices();

XRT_CORE_COMMON_EXPORT
void
scan_devices(bool verbose, bool json);

XRT_CORE_COMMON_EXPORT
std::shared_ptr<device>
get_userpf_device(device::id_type id);

XRT_CORE_COMMON_EXPORT
std::shared_ptr<device>
get_mgmtpf_device(device::id_type id);

XRT_CORE_COMMON_EXPORT
uint16_t
bdf2index(const std::string& bdfStr);

} //xrt_core

#endif /* CORE_SYSTEM_H */
