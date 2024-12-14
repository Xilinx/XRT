// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. - All rights reserved

#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <syslog.h>

#include <cstdio>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <stdlib.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <uuid/uuid.h>
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/md5.h>
#undef OPENSSL_SUPPRESS_DEPRECATED
#include "xrt/detail/xclbin.h"
#include "container.h"

/*
 * Functions each plugin needs to provide
 */
extern "C" {
int init(mpd_plugin_callbacks *cbs);
void fini(void *mpc_cookie);
}

/*
 * Init function of the plugin that is used to hook the required functions.
 * The cookie is used by fini (see below). Can be NULL if not required.
 */ 
int init(mpd_plugin_callbacks *cbs)
{
    int ret = 1;
    auto total = xrt_core::pci::get_dev_total();
    if (total == 0) {
        syslog(LOG_INFO, "Container: no device found");
        return ret;
    }
    if (cbs) 
    {
        // hook functions
        cbs->mpc_cookie = NULL;
        cbs->get_remote_msd_fd = get_remote_msd_fd;
        cbs->mb_req.load_xclbin = xclLoadXclBin;
        ret = 0;
    }
    syslog(LOG_INFO, "container mpd plugin init called: %d\n", ret);
    return ret;
}

/*
 * Fini function of the plugin
 */
void fini(void *mpc_cookie)
{
     syslog(LOG_INFO, "container mpd plugin fini called\n");
}

/*
 * callback function that is used to setup communication channel
 * we are going to handle mailbox ourself, no comm channel is required.
 * so just return -1 to the fd
 * Input:
 *        index: index of the user PF
 * Output:    
 *        fd: socket handle of the communication channel
 * Return value:
 *        0: success
 *        1: failure
 */ 
int get_remote_msd_fd(size_t index, int* fd)
{
    *fd = -1;
    return 0;
}

/*
 * callback function that is used to handle MAILBOX_REQ_LOAD_XCLBIN msg
 *
 * Input:
 *        index: index of the FPGA device
 *        xclbin: the fake xclbin file
 * Output:
 *        resp: int as response msg
 * Return value:
 *        0: success
 *        others: error code
 */ 
int xclLoadXclBin(size_t index, const axlf *xclbin, int *resp)
{
    int ret = -1;
    Container d(index);
    if (d.isGood()) {
        *resp = d.xclLoadXclBin(xclbin);
        ret = 0;
    }
    return ret;
}

int Container::xclLoadXclBin(const xclBin *buffer)
{
    /*
     * This file is delivered by default not to provide xclbin protection.
     * That means, the input xclbin file is downloaded as-is. This is also
     * useful in xrt container deployment case, with which, only the user PF
     * is granted to the container, and mgmt is left at host, but within
     * container users can use xbutil and/or openCL without seeing any
     * difference compared to within host.
     *
     * If container platform vendors, say Nimbix, want to have xclbin proction,
     * their code can be added here.
     */
#if 1
    xclmgmt_ioc_bitstream_axlf obj = { const_cast<axlf *>(buffer) };
#else
    //add vendor specific code here
    std::vector<char> real_xclbin;
    if (retrieve_xclbin(buffer, real_xclbin) != 0)
        return -EINVAL;
    xclmgmt_ioc_bitstream_axlf obj = {reinterpret_cast<axlf *>(real_xclbin.data())};    
#endif
    int fd = mgmtDev->open("", O_RDWR);
    int ret = mgmtDev->ioctl(fd, XCLMGMT_IOCICAPDOWNLOAD_AXLF, &obj);
    mgmtDev->close(fd);
    return ret;
}

bool Container::isGood()
{
    return mgmtDev != nullptr;
}

Container::~Container()
{
}

Container::Container(size_t index)
{
    mgmtDev = xrt_core::pci::get_dev(index, false);
}

//private methods, vendor dependant
/*
 * This file also gives examples how users add customized xclbin protection
 * leveraging the mpd/msd framework.
 *
 * The example here maintains a database in memory. The primary of the database
 * is the md5sum of the fake xclbin, and the path column saves the path to the
 * real xclbin file.
 * This is only for the sample code usage. Cloud vendor has freedom to define
 * its own in terms of their own implementations
 */ 
/* The fake xclbin file transmitted through mailbox is achieved by
 * #xclbinutil --input path_to_xclbin --remove-section BITSTREAM --output path_to_fake_xclbin
 * --skip-uuid-insertion
 * this new fake xclbin has same uuid to the real xclbin
 *
 * md5 of the fake xclbin can be achieved by
 * #md5sum path_to_fake_xclbin
 *
 * This md5 is the primary key of the repo database to get the real xclbin
 */
struct xclbin_repo {
    const char *md5; //md5 of the xclbin metadata. should be the primary key of DB of the repo
    const char *path; //path to xclbin file
};
#ifdef XRT_INSTALL_PREFIX
    #define VERIFY_XCLBIN_PATH XRT_INSTALL_PREFIX "/dsa/xilinx_u280_xdma_201910_1/test/verify.xclbin"
    #define BANDWIDTH_XCLBIN_PATH XRT_INSTALL_PREFIX "/dsa/xilinx_u280_xdma_201910_1/test/bandwidth.xclbin"
#else
    #define VERIFY_XCLBIN_PATH "/opt/xilinx/dsa/xilinx_u280_xdma_201910_1/test/verify.xclbin"
    #define BANDWIDTH_XCLBIN_PATH "/opt/xilinx/dsa/xilinx_u280_xdma_201910_1/test/bandwidth.xclbin"
#endif
static struct xclbin_repo repo[2] = {
    {
        .md5 = "d9662fc2a45422d5f7c80f57dae4c8db",
        .path = VERIFY_XCLBIN_PATH,
    },
    {
        .md5 = "97aefd0cd3dd9a96cc5d24c9afcd3818",
        .path = BANDWIDTH_XCLBIN_PATH,
    },
}; // there are only 2 xclbins in the sample code

/*
 * Sample code for user reference to get the real xclbin file from fake xclbin
 * container cloud vendor (Nimbix) need to implement this function according to
 * their mechanism to save the real xclbin files
 * This code is just for reference and for internal test purpose
 */ 
int Container::retrieve_xclbin(const xclBin *&orig, std::vector<char> &real_xclbin)
{
    std::string md5 = calculate_md5(reinterpret_cast<char *>(const_cast<xclBin *>(orig)),
        orig->m_header.m_length);
    for (const auto entry : repo) {
        if (strcmp(md5.c_str(), entry.md5) == 0) {
            real_xclbin = read_file(entry.path);
            return 0;
        }
    }
    return 1;
}

/*
 * Sample code to calculate the md5sum of the fake xclbin
 * the md5sum is the primary key for the retrieve_xclbin() to
 * get the real xclbin.
 * This code is just for reference and for internal test purpose
 */ 
std::string Container::calculate_md5(char *buf, size_t len)
{
    unsigned char s[16];
    MD5_CTX context;
    MD5_Init(&context);
    MD5_Update(&context, buf, len);
    MD5_Final(s, &context);

    std::stringstream md5;
    md5 << std::hex << std::setfill('0');
    for (auto &byte: s)
    {
        md5 << std::setw(2) << (int)byte;
    }

    return md5.str();
}

/*
 * Sample code to get the real xclbin file.
 * This code is just for reference and for internal test purpose
 */ 
std::vector<char> Container::read_file(const char *filename)
{
    std::ifstream t(filename, std::ios::binary | std::ios::in);
    t.seekg(0, std::ios::end);
    int len = t.tellg();
    t.seekg(0, std::ios::beg);    
    std::vector<char> buf;
    buf.resize(len);
    t.read(buf.data(), len);
    return buf;
}

