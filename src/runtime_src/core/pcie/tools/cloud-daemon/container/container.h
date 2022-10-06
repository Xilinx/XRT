// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
#ifndef _CONTAINER_H_
#define _CONTAINER_H_

#include "xclhal2.h"
#include "../mpd_plugin.h"
#include "core/pcie/driver/linux/include/mailbox_proto.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"
#include "core/pcie/driver/linux/include/xocl_ioctl.h"
#include "core/pcie/linux/pcidev.h"
#include <fstream>
#include <string>
#include <vector>

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
    std::shared_ptr<xrt_core::pci::dev> mgmtDev;
    int retrieve_xclbin(const xclBin *&orig_xclbin, std::vector<char> &real_xclbin);
    std::string calculate_md5(char *buf, size_t len);
    std::vector<char> read_file(const char *filename);
};

int get_remote_msd_fd(size_t index, int* fd);
int xclLoadXclBin(size_t index, const axlf *xclbin, int *resp);
#endif
