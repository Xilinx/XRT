/**
 * Copyright (C) 2021-2022 Xilinx, Inc
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

#include <fcntl.h>
#include "core/common/query_requests.h"
#include "core/tools/common/BusyBar.h"
#include "tools/common/XBUtilitiesCore.h"
#include "flasher.h"
#include "xgq_vmr.h"

/**
 * @brief XGQ_VMR_Flasher::XGQ_VMR_Flasher
 */
XGQ_VMR_Flasher::XGQ_VMR_Flasher(std::shared_ptr<xrt_core::device> dev)
    : m_device(std::move(dev))
{
}

int XGQ_VMR_Flasher::xclUpgradeFirmware(std::istream& binStream)
{
  binStream.seekg(0, binStream.end);
  ssize_t total_size = static_cast<int>(binStream.tellg());
  binStream.seekg(0, binStream.beg);

  std::cout << "INFO: ***xsabin has " << total_size << " bytes" << std::endl;

  XBUtilities::BusyBar busy_bar("Working...", std::cout);
  busy_bar.start(XBUtilities::is_escape_codes_disabled());

  try {
    std::vector<char> buffer(total_size);
    binStream.read(buffer.data(), total_size);
    ssize_t ret = total_size;

#ifdef __linux__
    auto fd = m_device->file_open("xgq_vmr", O_RDWR);
    ret = write(fd.get(), buffer.data(), total_size);
#endif
    busy_bar.finish();
    std::cout << "INFO: ***Write " << ret << " bytes" << std::endl;
    return ret == total_size ? 0 : -EIO;
  }
  catch (const std::exception& e) {
    busy_bar.finish();
    xrt_core::send_exception_message(e.what(), "xgq_vmr operation failed");
    return -EIO;
  }
}

int XGQ_VMR_Flasher::xclGetBoardInfo(std::map<char, std::string>& info)
{
  int ret = 0;

  try {
    info[BDINFO_SN] = xrt_core::device_query<xrt_core::query::hwmon_sdm_serial_num>(m_device);
    info[BDINFO_MAC0] = xrt_core::device_query<xrt_core::query::hwmon_sdm_mac_addr0>(m_device);
    info[BDINFO_MAC1] = xrt_core::device_query<xrt_core::query::hwmon_sdm_mac_addr1>(m_device);
    info[BDINFO_REV] = xrt_core::device_query<xrt_core::query::hwmon_sdm_revision>(m_device);
    info[BDINFO_NAME] = xrt_core::device_query<xrt_core::query::hwmon_sdm_board_name>(m_device);
    info[BDINFO_BMC_VER] = xrt_core::device_query<xrt_core::query::hwmon_sdm_active_msp_ver>(m_device);
    info[BDINFO_FAN_PRESENCE] = xrt_core::device_query<xrt_core::query::hwmon_sdm_fan_presence>(m_device);
  }
  catch (const xrt_core::query::exception&) {
    ret = -EOPNOTSUPP;
  }

  return ret;
}
