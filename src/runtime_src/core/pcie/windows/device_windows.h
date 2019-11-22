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

#ifndef DEVICE_WINDOWS_H
#define DEVICE_WINDOWS_H

// Please keep eternal include file dependencies to a minimum
#include <boost/functional/factory.hpp>
#include "common/device_pcie.h"

namespace xrt_core {
  
class device_windows : public device_pcie {
  public:
    struct IOCTLEntry {
      uint64_t IOCTLValue;
    };

    const IOCTLEntry & get_IOCTL_entry( QueryRequest _eQueryRequest) const;

  protected:
    virtual void read_device_dma_stats(uint64_t _deviceID, boost::property_tree::ptree &_pt) const;
    virtual uint64_t get_total_devices() const;
    virtual void query_device(uint64_t _deviceID, QueryRequest _eQueryRequest, const std::type_info & _typeInfo, boost::any &_returnValue) const;

  public:
    device_windows();
    virtual ~device_windows();

  private:
    device_windows(const device_windows&) = delete;
    device_windows& operator=(const device_windows&) = delete;
};

} // xrt_core

#endif 
