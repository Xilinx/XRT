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

#ifndef PCIE_DEVICE_LINUX_H
#define PCIE_DEVICE_LINUX_H

#include "common/device_pcie.h"

namespace xrt_core {

class device_linux : public device_pcie
{
public:
  struct SysDevEntry {
    const std::string sSubDevice;
    const std::string sEntry;
  };
  const SysDevEntry & get_sysdev_entry(QueryRequest qr) const;

  device_linux(id_type device_id, bool user);

  // query functions
  virtual void read_dma_stats(boost::property_tree::ptree& pt) const;
  virtual void query(QueryRequest qr, const std::type_info& tinfo, boost::any& value) const;

  //flash functions
  virtual void auto_flash(const std::string& shell, const std::string& id, bool force) const;
  virtual void reset_shell() const;
  virtual void update_shell(const std::string& flashType, const std::string& primary, const std::string& secondary) const;
  virtual void update_SC(const std::string& file) const;

};
}

#endif /* CORE_SYSTEM_H */
