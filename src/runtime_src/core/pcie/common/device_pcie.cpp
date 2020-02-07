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

// For now device_pcie.cpp is delivered with core library (libxrt_core), see
// for example core/pcie/windows/CMakeLists.txt.  To prevent compilation of
// this file from importing symbols from libxrt_core we define this source
// file to instead export with same macro as used in libxrt_core.
#define XCL_DRIVER_DLL_EXPORT

#include "device_pcie.h"
#include "core/common/query_requests.h"
#include <boost/property_tree/ptree.hpp>

namespace xrt_core {

device_pcie::
device_pcie(id_type device_id, bool user)
    : device(device_id), m_userpf(user)
{
  if (m_userpf)
    m_handle = xclOpen(device_id, nullptr, XCL_QUIET);
}

device_pcie::
~device_pcie()
{
  if (m_userpf && m_handle)
    xclClose(m_handle);
}

xclDeviceHandle
device_pcie::
get_device_handle() const
{
  return m_handle;
}

void
device_pcie::
get_info(boost::property_tree::ptree& pt) const
{
  ptree_updater<query::pcie_vendor>::query_and_put(this, pt);
  ptree_updater<query::pcie_device>::query_and_put(this, pt);
  ptree_updater<query::pcie_subsystem_vendor>::query_and_put(this, pt);
  ptree_updater<query::pcie_subsystem_id>::query_and_put(this, pt);
  ptree_updater<query::pcie_link_speed>::query_and_put(this, pt);
  ptree_updater<query::pcie_express_lane_width>::query_and_put(this, pt);
  query_and_put(QR_DMA_THREADS_RAW, pt);
}

} // xrt_core
