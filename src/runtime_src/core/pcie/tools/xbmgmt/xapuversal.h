/**
 * Copyright (C) 2021 Xilinx, Inc
 * Author: Yidong Zhang
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

#ifndef _XAPUVERSAL_H_
#define _XAPUVERSAL_H_

#include <iostream>
#include "core/pcie/linux/scan.h"

class XAPUVER_Flasher
{
public:
    XAPUVER_Flasher(std::shared_ptr<pcidev::pci_device> dev);
    ~XAPUVER_Flasher();
    int xclUpgradeFirmware(std::istream& binStream);

private:
    std::shared_ptr<pcidev::pci_device> mDev;
};

#endif
