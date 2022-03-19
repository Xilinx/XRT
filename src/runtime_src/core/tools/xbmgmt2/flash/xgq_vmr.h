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

#ifndef _XGQ_VMR_FLASHER_H_
#define _XGQ_VMR_FLASHER_H_

#include <iostream>
#include "core/common/system.h"
#include "core/common/device.h"

class XGQ_VMR_Flasher
{
public:
    /* Constructor of ospi_xgq flash */
    XGQ_VMR_Flasher(std::shared_ptr<xrt_core::device> dev);
    /* API of flashing binStream via ospi_xgq driver */
    int xclUpgradeFirmware(std::istream& binStream);
    int xclGetBoardInfo(std::map<char, std::string>& info);

private:
    std::shared_ptr<xrt_core::device> m_device;
};

#endif
