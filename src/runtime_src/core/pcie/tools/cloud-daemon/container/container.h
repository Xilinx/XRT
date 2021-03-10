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

/*
 * This class is used for container running on top of baremetal machines(Nimbix)
 * In this case, if the container cloud vendor wants to have xclbin protection,
 * it can implement its own retrieve_xclbin() here in this plugin. This function
 * gets the real xclbin from the input fake xclbin, and it is called in xclLoadXclBin().
 * Without vendor specific code, this plugin function returns the input xclbin.
 *
 * This plugin can be used for
 * 1. internal test(no xclbin protection by default)
 * 2. internal test(xclbin protection. 2 entry database is maintained in memory)
 * 3. reference by cloud vendor 
 */ 
class Container
{
public:
    explicit Container(size_t index);
    ~Container();

    // Bitstreams
    int xclLoadXclBin(const xclBin *buffer);
    bool isGood();
private:
/*
 * Internal data structures this sample plugin uses
 */ 
    std::shared_ptr<pcidev::pci_device> mgmtDev;
    int retrieve_xclbin(const xclBin *&orig_xclbin, std::vector<char> &real_xclbin);
    std::string calculate_md5(char *buf, size_t len);
    std::vector<char> read_file(const char *filename);
};

int get_remote_msd_fd(size_t index, int* fd);
int xclLoadXclBin(size_t index, const axlf *xclbin, int *resp);
#endif
