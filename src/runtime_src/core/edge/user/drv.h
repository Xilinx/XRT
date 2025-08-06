// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights

#ifndef DRV_H_
#define DRV_H_

#include <vector>
#include <memory>
#include <string>

namespace xrt_core::edge {

class dev;
class drv  //Base class for edge type of drivers
{
public:
   virtual std::string
   name() const = 0;

   virtual void
   scan_devices(std::vector<std::shared_ptr<dev>>& dev_list) = 0;

   virtual std::shared_ptr<dev>
   create_edev(const std::string& sysfs="") const = 0;

   virtual ~drv() = default;
   drv() = default;
   drv(const drv&) = default;
   drv(drv&&) = default;
   drv& operator=(const drv&) = default;
   drv& operator=(drv&&) = default;
};

} //namespace xrt_core::edge
#endif
