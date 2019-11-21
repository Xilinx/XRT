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

#ifndef DEVICE_PCIE_H
#define DEVICE_PCIE_H

// Please keep eternal include file dependencies to a minimum
#include "common/device_core.h"

namespace xrt_core {
class device_pcie : public xrt_core::device_core {

  protected:
    device_pcie();
    virtual ~device_pcie();

  protected:
    virtual uint64_t get_total_devices() const = 0;
    virtual void get_devices(boost::property_tree::ptree &_pt) const;
    virtual void get_device_info(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;

  private:
    device_pcie(const device_pcie&);
    device_pcie& operator=(const device_pcie&);
};
}

#endif /* CORE_SYSTEM_H */
