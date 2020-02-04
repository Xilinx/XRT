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

/**
 * @brief XOSPIVER_Flasher::XOSPIVER_Flasher
 */
XOSPIVER_Flasher::XOSPIVER_Flasher(std::shared_ptr<xrt_core::device> dev)
    : m_device(dev)
{
}

int XOSPIVER_Flasher::xclUpgradeFirmware(std::istream& binStream)
{
    int total_size = 0;

    binStream.seekg(0, binStream.end);
    total_size = static_cast<int>(binStream.tellg());
    binStream.seekg(0, binStream.beg);

    std::cout << "INFO: ***PDI has " << total_size << " bytes" << std::endl;

    int fd = m_device->open("ospi_versal", O_RDWR); 
    if (fd == -1) {
        std::cout << "ERROR Cannot open ospi_versal for writing " << std::endl;
        return -ENODEV;
    }

    std::vector<char> buffer(total_size);
    binStream.read(buffer.data(), total_size);
	ssize_t ret = write(fd, buffer.data(), total_size);

    m_device->close(fd);

    return ret == total_size ? 0 : -EIO;
}
