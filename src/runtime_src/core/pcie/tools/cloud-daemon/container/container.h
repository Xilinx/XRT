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
#ifndef _CONTAINER_H_
#define _CONTAINER_H_

#include <fstream>
#include <vector>
#include <string>
#include "xclhal2.h"
#include "core/pcie/driver/linux/include/mailbox_proto.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"
#include "core/pcie/linux/scan.h"
#include "core/pcie/driver/linux/include/xocl_ioctl.h"
#include "../mpd_plugin.h"

class Container
{
public:
    Container(size_t index);
    ~Container();

    // Bitstreams
    int xclLoadXclBin(const xclBin *&buffer);
    bool isGood();
private:
/*
 * Internal data structures this sample plugin uses
 */ 
    std::shared_ptr<pcidev::pci_device> mgmtDev;
    int retrieve_xclbin(const xclBin *&orig_xclbin,
           std::shared_ptr<std::vector<char>> &real_xclbin);
    void calculate_md5(char *md5, char *buf, size_t len);
    int read_file(const char *filename, std::shared_ptr<std::vector<char>> &sp);
};

int get_remote_msd_fd(size_t index, int& fd);
int xclLoadXclBin(size_t index, const axlf *&xclbin);
#endif
