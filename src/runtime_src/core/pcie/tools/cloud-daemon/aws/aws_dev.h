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

    int awsGetIcap(std::unique_ptr<xcl_pr_region> &resp);
    int awsGetSensor(std::unique_ptr<xcl_sensor> &resp);
    int awsGetBdinfo(std::unique_ptr<xcl_board_info> &resp);
    int awsGetMig(std::unique_ptr<std::vector<char>> &resp, size_t &resp_len);
    int awsGetFirewall(std::unique_ptr<xcl_mig_ecc> &resp);
    int awsGetDna(std::unique_ptr<xcl_dna> &resp);
    int awsGetSubdev(std::unique_ptr<std::vector<char>> &resp, size_t &resp_len);
    // Bitstreams
    int awsLoadXclBin(const xclBin *&buffer);
    //int xclBootFPGA();
    int awsResetDevice();
    int awsReClock2(xclmgmt_ioc_freqscaling *&obj);
    int awsLockDevice();
    int awsUnlockDevice();
    bool isGood();
private:
    const int mBoardNumber;
    bool mLocked;
    std::ofstream mLogStream;
#ifdef INTERNAL_TESTING_FOR_AWS
    int mMgtHandle;
#else
    int sleepUntilLoaded( const std::string &afi );
    int checkAndSkipReload( char *afi_id, fpga_mgmt_image_info *info );
    int loadDefaultAfiIfCleared( void );
    char* get_afi_from_axlf(const axlf * buffer);
#endif
};

int get_remote_msd_fd(size_t index, int& fd);
int awsLoadXclBin(size_t index, const axlf *&xclbin);
int awsGetIcap(size_t index, std::unique_ptr<xcl_pr_region> &resp);
int awsGetSensor(size_t index, std::unique_ptr<xcl_sensor> &resp);
int awsGetBdinfo(size_t index, std::unique_ptr<xcl_board_info> &resp);
int awsGetMig(size_t index, std::unique_ptr<std::vector<char>> &resp, size_t &resp_len);
int awsGetFirewall(size_t index, std::unique_ptr<xcl_mig_ecc> &resp);
int awsGetDna(size_t index, std::unique_ptr<xcl_dna> &resp);
int awsGetSubdev(size_t index, std::unique_ptr<std::vector<char>> &resp, size_t &resp_len);
int awsLockDevice(size_t index);
int awsUnlockDevice(size_t index);
int awsResetDevice(size_t index);
int awsReClock2(size_t index, struct xclmgmt_ioc_freqscaling *&obj);
#endif
