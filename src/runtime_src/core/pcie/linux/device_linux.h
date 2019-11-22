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

#ifndef DEVICE_LINUX_H
#define DEVICE_LINUX_H

// Please keep eternal include file dependencies to a minimum
#include "common/device_pcie.h"

namespace xrt_core {
class device_linux : public xrt_core::device_pcie {
  public:
    virtual void read_device_dma_stats(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;

    struct SysDevEntry {
      const std::string sSubDevice;
      const std::string sEntry;
    };

    const SysDevEntry & get_sysdev_entry( QueryRequest _eQueryRequest) const;

  protected:
    virtual uint64_t get_total_devices() const;
    virtual void query_device(uint64_t _deviceID, QueryRequest _eQueryRequest, const std::type_info & _typeInfo, boost::any &_returnValue) const;

  public:
    device_linux();
    virtual ~device_linux();

  private:
    device_linux(const device_linux&) = delete;
    device_linux& operator=(const device_linux&) = delete;
};
}

#endif /* CORE_SYSTEM_H */
