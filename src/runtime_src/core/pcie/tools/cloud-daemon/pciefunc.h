// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

/* Class defining device model needed by all daemons. */

#ifndef PCIEFUNC_H
#define PCIEFUNC_H

#include "core/pcie/linux/pcidev.h"
#include <mutex>
#include <string>

class pcieFunc {
public:
    pcieFunc(size_t index, bool user = true);
    ~pcieFunc();

    // Getters for various device attributes
    std::string getHost();
    uint16_t getPort();
    int getId();
    int getMailbox();
    uint64_t getSwitch();
    int getIndex() const;
    std::shared_ptr<xrt_core::pci::dev> getDev() const;

    // Load config from device's sysfs nodes
    bool loadConf();
    // Write config to device's sysfs nodes
    int updateConf(std::string host, uint16_t port, uint64_t swch);

    // prefix syslog msg with dev specific bdf
    void log(int priority, const char *format, ...) const;

private:
    std::string host;
    uint16_t port = 0;
    uint64_t chanSwitch = 0;
    int devId = 0;
    int mbxfd = -1;
    std::shared_ptr<xrt_core::pci::dev> dev;
    size_t index;
    std::mutex lock;
    bool validConf();
    void clearConf();
    int mailboxOpen();

    pcieFunc(const pcieFunc& dev) = delete;
    pcieFunc& operator=(const pcieFunc& dev) = delete;
};

#endif // PCIEFUNC_H
