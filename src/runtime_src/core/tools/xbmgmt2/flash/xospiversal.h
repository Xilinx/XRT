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

#ifndef _XOSPIVERSAL_H_
#define _XOSPIVERSAL_H_

#include <iostream>
#include "core/common/system.h"
#include "core/common/device.h"

class XOSPIVER_Flasher
{
public:
    XOSPIVER_Flasher(unsigned int device_index);
    int xclUpgradeFirmware(std::istream& binStream);

private:
    std::shared_ptr<xrt_core::device> m_device;
};

#endif
