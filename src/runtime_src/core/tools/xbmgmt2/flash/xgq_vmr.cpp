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
  auto total_size = static_cast<int>(binStream.tellg());
  binStream.seekg(0, binStream.beg);

  std::cout << "INFO: ***PDI has " << total_size << " bytes" << std::endl;

  try {
    auto fd = m_device->file_open("xgq_vmr", O_RDWR);
    std::vector<char> buffer(total_size);
    binStream.read(buffer.data(), total_size);
    ssize_t ret = total_size;
#ifdef __GNUC__
    ret = write(fd.get(), buffer.data(), total_size);
#endif
    return ret == total_size ? 0 : -EIO;
  }
  catch (const std::exception& e) {
    xrt_core::send_exception_message(e.what(), "XBMGMT");
    return -EIO;
  }
}
