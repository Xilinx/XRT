/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include "xospiversal.h"
#include "core/common/xclbin_parser.h"

/**
 * @brief XOSPIVER_Flasher::XOSPIVER_Flasher
 */
XOSPIVER_Flasher::XOSPIVER_Flasher(std::shared_ptr<xrt_core::device> dev)
    : m_device(std::move(dev))
{
}

int XOSPIVER_Flasher::xclUpgradeFirmware(std::istream& binStream)
{
  binStream.seekg(0, binStream.end);
  auto total_size = static_cast<int>(binStream.tellg());
  binStream.seekg(0, binStream.beg);

  std::cout << "INFO: ***xsabin has " << total_size << " bytes" << std::endl;

  try {
    std::vector<char> buffer(total_size);
    binStream.read(buffer.data(), total_size);
    ssize_t ret = total_size;

    auto top = reinterpret_cast<const axlf*>(buffer.data());
    auto hdr = xrt_core::xclbin::get_axlf_section(top, PDI);
    if (!hdr)
        throw std::runtime_error("No PDI section in xclbin");
    ssize_t size = hdr->m_sectionSize;
    auto data = reinterpret_cast<const char*>(reinterpret_cast<const char*>(top) +
        hdr->m_sectionOffset);

    std::cout << "INFO: ***PDI has " << size << " bytes" << std::endl;

#ifdef __GNUC__
    auto fd = m_device->file_open("xfer_versal", O_RDWR);
    ret = write(fd.get(), data, size);
#endif
    std::cout << "INFO: ***Write " << ret << " bytes" << std::endl;

    return ret == size ? 0 : -EIO;
  }
  catch (const std::exception& e) {
    xrt_core::send_exception_message(e.what(), "xfer_versal operation failed");
    return -EIO;
  }
}
