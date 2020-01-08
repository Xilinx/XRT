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
#ifndef _AWS_DEV_H_
#define _AWS_DEV_H_

#include <fstream>
#include <vector>
#include <string>
#include "xclhal2.h"
#include "core/pcie/driver/linux/include/mailbox_proto.h"
#include "core/pcie/driver/linux/include/mgmt-ioctl.h"
#include "core/pcie/linux/scan.h"
#include "../mpd_plugin.h"
#include "../common.h"
#include "../sw_msg.h"
#include "../pciefunc.h"

#ifdef INTERNAL_TESTING_FOR_AWS
#include "core/pcie/driver/linux/include/xocl_ioctl.h"
#endif

#ifndef INTERNAL_TESTING_FOR_AWS
#include "fpga_pci.h"
#include "fpga_mgmt.h"
#include "hal/fpga_common.h"
#endif

#define DEFAULT_GLOBAL_AFI "agfi-069ddd533a748059b" // 1.4 shell
#define XILINX_ID 0x1d0f
#define AWS_UserPF_DEVICE_ID 0x1042     //userPF device on AWS F1 & Pegasus
#define AWS_MgmtPF_DEVICE_ID 0x1040     //mgmtPF device on Pegasus (mgmtPF not visible on AWS)
#define AWS_UserPF_DEVICE_ID_SDx 0xf010 //userPF device on AWS F1 after downloading xclbin into FPGA (SHELL 1.4)

/*
 * This class is used to handle ioctl access to mgmt PF of AWS specific FPGA.
 * 
 * Since AWS has its own FPGA mgmt hardware/driver, any mgmt HW access request from xocl driver
 * will be forwarded by sw mailbox and be intercepted and interpreted by this mpd plugin. And
 * with the plugin, those special requests will be translated to API calls into the libmgmt.so
 * (or .a) provided by AWS
 *
 * This class will *only* handle AWS specific parts. Those not implemented in AWS hardware
 * so far will be returned as NOTSUPPORTED
 */
class AwsDev
{
public:
    AwsDev(size_t index, const char *logfileName);
    ~AwsDev();

    int awsGetIcap(xcl_pr_region *resp);
    int awsGetSensor(xcl_sensor *resp);
    int awsGetBdinfo(xcl_board_info *resp);
    int awsGetMig(char *resp, size_t resp_len);
    int awsGetFirewall(xcl_mig_ecc *resp);
    int awsGetDna(xcl_dna *resp);
    int awsGetSubdev(char *resp, size_t resp_len);
    // Bitstreams
    int awsLoadXclBin(const xclBin *buffer);
    //int xclBootFPGA();
    int awsResetDevice();
    int awsReClock2(const xclmgmt_ioc_freqscaling *obj);
    int awsProgramShell();
    int awsReadP2pBarAddr(const xcl_mailbox_p2p_bar_addr *addr);
    int awsUserProbe(xcl_mailbox_conn_resp *resp);
    bool isGood();
private:
    const int mBoardNumber;
    std::ofstream mLogStream;
#ifdef INTERNAL_TESTING_FOR_AWS
    int mMgtHandle;
#else
    int sleepUntilLoaded( const std::string &afi );
    int checkAndSkipReload( char *afi_id, fpga_mgmt_image_info *info );
    char* get_afi_from_axlf(const axlf * buffer);
#endif
};

int get_remote_msd_fd(size_t index, int* fd);
int mb_notify(size_t index, int fd, bool online);
int awsLoadXclBin(size_t index, const axlf *xclbin, int *resp);
int awsGetIcap(size_t index, xcl_pr_region *resp);
int awsGetSensor(size_t index, xcl_sensor *resp);
int awsGetBdinfo(size_t index, xcl_board_info *resp);
int awsGetMig(size_t index, char *resp, size_t resp_len);
int awsGetFirewall(size_t index, xcl_mig_ecc *resp);
int awsGetDna(size_t index, xcl_dna *resp);
int awsGetSubdev(size_t index, char *resp, size_t resp_len);
int awsResetDevice(size_t index, int *resp);
int awsReClock2(size_t index, const xclmgmt_ioc_freqscaling *obj, int *resp);
int awsUserProbe(size_t index, xcl_mailbox_conn_resp *resp);
int awsProgramShell(size_t index, int *resp);
int awsReadP2pBarAddr(size_t index, const xcl_mailbox_p2p_bar_addr *addr, int *resp);
#endif
