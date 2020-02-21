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


#include "device_linux.h"
#include "xrt.h"
#include <string>
#include <iostream>
#include <map>
#include <boost/format.hpp>

namespace xrt_core {

device_linux::
device_linux(id_type device_id, bool user)
  : shim<device_edge>(device_id, user)
{
}

void
device_linux::
read_dma_stats(boost::property_tree::ptree& pt) const
{
}

void
device_linux::
read(uint64_t offset, void* buf, uint64_t len) const
{

  throw error(-ENODEV, "read failed");
}

void
device_linux::
write(uint64_t offset, const void* buf, uint64_t len) const
{
  throw error(-ENODEV, "write failed");
}

} // xrt_core
