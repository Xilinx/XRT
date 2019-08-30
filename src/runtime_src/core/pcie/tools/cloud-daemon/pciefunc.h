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

/* Class defining device model needed by all daemons. */

#ifndef PCIEFUNC_H
#define PCIEFUNC_H

#include <string>
#include <mutex>
#include "core/pcie/linux/scan.h"

class pcieFunc {
public:
    pcieFunc(std::shared_ptr<pcidev::pci_device> d);
    ~pcieFunc();

    // Getters for various device attributes
    std::string getHost();
    uint16_t getPort();
    int getId();
    int getMailbox();
    uint64_t getSwitch();

    // Load config from device's sysfs nodes
    bool loadConf();
    // Write config to device's sysfs nodes
    int updateConf(std::string host, uint16_t port, uint64_t swch);

    // Perform IOCTL on this device
    int ioctl(unsigned long cmd, void *arg = nullptr);

    // prefix syslog msg with dev specific bdf
    void log(int priority, const char *format, ...);

private:
    std::string host;
    uint16_t port = 0;
    uint64_t chanSwitch = 0;
    int devId = 0;
    int mbxfd = -1;
    std::shared_ptr<pcidev::pci_device> dev;
    std::mutex lock;
    bool validConf();
    void clearConf();
    int mailboxOpen();

    pcieFunc(const pcieFunc& dev) = delete;
    pcieFunc& operator=(const pcieFunc& dev) = delete;
};

#endif // PCIEFUNC_H
