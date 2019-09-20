/*
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
#include <fstream>
#include <exception>
#include <uuid/uuid.h>
#include "xclbin.h"
#include "aws_dev.h"


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
    auto total = pcidev::get_dev_total();
    if (total == 0) {
        syslog(LOG_INFO, "aws: no device found");
        return ret;
    }
    if (cbs) 
    {
        // hook functions
        cbs->mpc_cookie = NULL;
        cbs->get_remote_msd_fd = get_remote_msd_fd;
        cbs->load_xclbin = awsLoadXclBin;
        cbs->get_icap_data = awsGetIcap;
        cbs->get_sensor_data = awsGetSensor;
        cbs->get_board_info = awsGetBdinfo;
        cbs->get_mig_data = awsGetMig;
        cbs->get_firewall_data = awsGetFirewall;
        cbs->get_dna_data = awsGetDna;
        cbs->get_subdev_data = awsGetSubdev;
        cbs->lock_bitstream = awsLockDevice;
        cbs->unlock_bitstream = awsUnlockDevice;
        cbs->hot_reset = awsResetDevice;
        cbs->reclock2 = awsReClock2;
        ret = 0;
    }
    syslog(LOG_INFO, "aws mpd plugin init called: %d\n", ret);
    return ret;
}

/*
 * Fini function of the plugin
 */ 
void fini(void *mpc_cookie)
{
     syslog(LOG_INFO, "aws mpd plugin fini called\n");
}

/*
 * callback function that is used to setup communication channel
 * aws doesn't need this, so just return -1 to the fd
 * Input:
 *        d: dbdf of the user PF
 * Output:
 *        fd: socket handle of the communication channel
 * Return value:
 *        0: success
 *        1: failure
 */
int get_remote_msd_fd(size_t index, int& fd)
{
    fd = -1;
    return 0;
}

/*
 * callback function that is used to handle MAILBOX_REQ_LOAD_XCLBIN msg
 * 
 * Input:
 *        index: index of the FPGA device
 *        awsbin: the aws xclbin file
 * Output:
 *        none    
 * Return value:
 *        0: success
 *        others: err code
 */
int awsLoadXclBin(size_t index, const axlf *&xclbin)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsLoadXclBin(xclbin);
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind ICAP
 * 
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetIcap(size_t index, std::unique_ptr<xcl_pr_region> &resp)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsGetIcap(resp);
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind SENSOR
 * 
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetSensor(size_t index, std::unique_ptr<xcl_sensor> &resp)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsGetSensor(resp);
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind BDINFO
 * 
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: response msg
 *        resp_len: length of response msg 
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetBdinfo(size_t index, std::unique_ptr<xcl_board_info> &resp)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsGetBdinfo(resp);
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind MIG_ECC
 * 
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetMig(size_t index, std::unique_ptr<std::vector<char>> &resp, size_t &resp_len)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsGetMig(resp, resp_len);
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind FIREWALL
 * 
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetFirewall(size_t index, std::unique_ptr<xcl_mig_ecc> &resp)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsGetFirewall(resp);
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind DNA
 *
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetDna(size_t index, std::unique_ptr<xcl_dna> &resp)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsGetDna(resp);
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind SUBDEV
 *
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetSubdev(size_t index, std::unique_ptr<std::vector<char>> &resp, size_t &resp_len)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsGetSubdev(resp, resp_len);
}

/*
 * callback function that is used to handle MAILBOX_REQ_LOCK_BITSTREAM msg 
 *
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        none
 * Return value:
 *        0: success
 *        others: err code
 */
int awsLockDevice(size_t index)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsLockDevice();
}

/*
 * callback function that is used to handle MAILBOX_REQ_UNLOCK_BITSTREAM msg
 * 
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        none
 * Return value:
 *        0: success
 *        others: err code
 */
int awsUnlockDevice(size_t index)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsUnlockDevice();
}

/*
 * callback function that is used to handle MAILBOX_REQ_HOT_RESET msg
 * 
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        none
 * Return value:
 *        0: success
 *        others: err code
 */
int awsResetDevice(size_t index)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsResetDevice();
}

/*
 * callback function that is used to handle MAILBOX_REQ_RECLOCK msg
 * 
 * Input:
 *        index: index of the FPGA device
 *        obj: the mailbox req msg of type 'struct xclmgmt_ioc_freqscaling'
 * Output:
 *        none
 * Return value:
 *        0: success
 *        others: err code
 */
int awsReClock2(size_t index, struct xclmgmt_ioc_freqscaling *&obj)
{
    AwsDev d(index, nullptr);
    if (!d.isGood())
        return -1;
    return d.awsReClock2(obj);
}

int AwsDev::awsLoadXclBin(const xclBin *&buffer)
{
#ifdef INTERNAL_TESTING_FOR_AWS
    if ( mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << buffer << std::endl;
    }
    
    std::cout << "Downloading xclbin ...\n" << std::endl;
    const unsigned cmd = XCLMGMT_IOCICAPDOWNLOAD_AXLF;
    xclmgmt_ioc_bitstream_axlf obj = { const_cast<axlf *>(buffer) };
    return ioctl(mMgtHandle, cmd, &obj);
#else
    int retVal = 0;
    axlf *axlfbuffer = reinterpret_cast<axlf*>(const_cast<xclBin*> (buffer));
    fpga_mgmt_image_info orig_info;
    char* afi_id = get_afi_from_axlf(axlfbuffer);

    if (!afi_id)
        return -EINVAL;

    std::memset(&orig_info, 0, sizeof(struct fpga_mgmt_image_info));
    fpga_mgmt_describe_local_image(mBoardNumber, &orig_info, 0);
    
    if (checkAndSkipReload(afi_id, &orig_info)) {
        // force data retention option
        union fpga_mgmt_load_local_image_options opt;
        fpga_mgmt_init_load_local_image_options(&opt);
        opt.flags = FPGA_CMD_DRAM_DATA_RETENTION;
        opt.afi_id = afi_id;
        opt.slot_id = mBoardNumber;
        retVal = fpga_mgmt_load_local_image_with_options(&opt);
        if (retVal == FPGA_ERR_DRAM_DATA_RETENTION_NOT_POSSIBLE ||
            retVal == FPGA_ERR_DRAM_DATA_RETENTION_FAILED ||
            retVal == FPGA_ERR_DRAM_DATA_RETENTION_SETUP_FAILED) {
            std::cout << "INFO: Could not load AFI for data retention, code: " << retVal 
                      << " - Loading in classic mode." << std::endl;
            retVal = fpga_mgmt_load_local_image(mBoardNumber, afi_id);
        }
        // check retVal from image load
        if (retVal) {
            std::cout << "Failed to load AFI, error: " << retVal << std::endl;
            return -retVal;
        }
        retVal = sleepUntilLoaded( std::string(afi_id) );
    }
    return retVal;
#endif
}

int AwsDev::awsGetIcap(std::unique_ptr<xcl_pr_region> &hwicap)
{
    struct xcl_pr_region data = {0};
#define FIELD(var, field, index) (var.field##_##index)
#ifdef INTERNAL_TESTING_FOR_AWS
    xclmgmt_ioc_info mgmt_info_obj;
    int ret = ioctl(mMgtHandle, XCLMGMT_IOCINFO, &mgmt_info_obj);
    if (ret)
           return -EFAULT;
    FIELD(data, freq, 0) = mgmt_info_obj.ocl_frequency[0];
    FIELD(data, freq, 1) = mgmt_info_obj.ocl_frequency[1];
    FIELD(data, freq, 2) = mgmt_info_obj.ocl_frequency[2];
    FIELD(data, freq, 3) = mgmt_info_obj.ocl_frequency[3];
    FIELD(data, freq_cntr, 0) = mgmt_info_obj.ocl_frequency[0] * 1000;
    FIELD(data, freq_cntr, 1) = mgmt_info_obj.ocl_frequency[1] * 1000;
    FIELD(data, freq_cntr, 2) = mgmt_info_obj.ocl_frequency[2] * 1000;
    FIELD(data, freq_cntr, 3) = mgmt_info_obj.ocl_frequency[3] * 1000;
#else
    fpga_mgmt_image_info imageInfo;
    fpga_mgmt_describe_local_image( mBoardNumber, &imageInfo, 0 );
    FIELD(data, freq, 0) = imageInfo.metrics.clocks[0].frequency[0] / 1000000;
    FIELD(data, freq, 1) = imageInfo.metrics.clocks[1].frequency[0] / 1000000;
    FIELD(data, freq, 2) = imageInfo.metrics.clocks[2].frequency[0] / 1000000;
    FIELD(data, freq_cntr, 0) = imageInfo.metrics.clocks[0].frequency[0] / 1000;
    FIELD(data, freq_cntr, 1) = imageInfo.metrics.clocks[1].frequency[0] / 1000;
    FIELD(data, freq_cntr, 2) = imageInfo.metrics.clocks[2].frequency[0] / 1000;
#endif
    //do we need to save uuid of xclbin loaded so that we can return xclbin uuid here?
    //seems not. we check afi before load new xclbin.

    hwicap = std::make_unique<xcl_pr_region>(data);
    return 0;
}

int AwsDev::awsGetSensor(std::unique_ptr<xcl_sensor> &sensor)
{
    struct xcl_sensor data = {0};
    sensor = std::make_unique<xcl_sensor>(data);
    return -ENOTSUP;
}

int AwsDev::awsGetBdinfo(std::unique_ptr<xcl_board_info> &bdinfo)
{
    struct xcl_board_info data = {0};
    bdinfo = std::make_unique<xcl_board_info>(data);
    return -ENOTSUP;
}

int AwsDev::awsGetMig(std::unique_ptr<std::vector<char>> &mig, size_t &resp_len)
{
    resp_len = sizeof (struct xcl_mig_ecc) * 64; //MAX_M_COUNT defined in xocl_drv.h
    mig = std::make_unique<std::vector<char>>(resp_len, 0); 
    return -ENOTSUP;
}

int AwsDev::awsGetFirewall(std::unique_ptr<xcl_mig_ecc> &firewall)
{
    struct xcl_mig_ecc data = {0};
    firewall = std::make_unique<xcl_mig_ecc>(data);
    return -ENOTSUP;
}

int AwsDev::awsGetDna(std::unique_ptr<xcl_dna> &dna)
{
    struct xcl_dna data = {0};
    dna = std::make_unique<xcl_dna>(data);
    return -ENOTSUP;
}

int AwsDev::awsGetSubdev(std::unique_ptr<std::vector<char>> &subdev, size_t &resp_len)
{
    resp_len = sizeof (struct xcl_subdev) + 0; //stub code. 0 may be any number in the future
    subdev = std::make_unique<std::vector<char>>(resp_len, 0);
    return -ENOTSUP;
}

bool AwsDev::isGood() {
#ifdef INTERNAL_TESTING_FOR_AWS
    if (mMgtHandle < 0) {
        std::cout << "AwsDev: Bad handle. No mgmtPF Handle" << std::endl;
        return false;
    }
#endif    
    return true;
}

int AwsDev::awsLockDevice()
{
    // AWS FIXME - add lockDevice
    mLocked = true;
    return 0;
}

int AwsDev::awsUnlockDevice()
{
    // AWS FIXME - add unlockDevice
    mLocked = false;
    return 0;
}

int AwsDev::awsResetDevice() {
    // AWS FIXME - add reset
    return 0;
}

int AwsDev::awsReClock2(xclmgmt_ioc_freqscaling *&obj) {
#ifdef INTERNAL_TESTING_FOR_AWS
    return ioctl(mMgtHandle, XCLMGMT_IOCFREQSCALE, obj);
#else
//    # error "INTERNAL_TESTING macro disabled. AMZN code goes here. "
//    # This API is not supported in AWS, the frequencies are set per AFI
    return 0;
#endif
}

AwsDev::~AwsDev()
{
#ifdef INTERNAL_TESTING_FOR_AWS
    if (mMgtHandle > 0)
        close(mMgtHandle);
#else
    fpga_mgmt_close(); // aws-fpga version newer than 09/2019 need this
#endif
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
        mLogStream.close();
    }
}

AwsDev::AwsDev(size_t index, const char *logfileName) : mBoardNumber(index), mLocked(false)
{
    if (logfileName != nullptr) {
        mLogStream.open(logfileName);
        mLogStream << "FUNCTION, THREAD ID, ARG..." << std::endl;
        mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

#ifdef INTERNAL_TESTING_FOR_AWS
    char file_name_buf[128];
    std::fill(&file_name_buf[0], &file_name_buf[0] + 128, 0);
    std::sprintf((char *)&file_name_buf, "/dev/awsmgmt%d", mBoardNumber);
    mMgtHandle = open(file_name_buf, O_RDWR | O_SYNC);
    if(mMgtHandle > 0)
        std::cout << "opened /dev/awsmgmt" << mBoardNumber << std::endl;
    else
        throw std::runtime_error("Can't open /dev/awsmgmt");

#else
    fpga_mgmt_init(); // aws-fpga version newer than 09/2019 need this
    loadDefaultAfiIfCleared();
    //bar0 is mapped already. seems other 2 bars are not required.
#endif
}

//private functions
#ifndef INTERNAL_TESTING_FOR_AWS
int AwsDev::loadDefaultAfiIfCleared( void )
{
    int array_len = 16;
    fpga_slot_spec spec_array[ array_len ];
    std::memset( spec_array, mBoardNumber, sizeof(fpga_slot_spec) * array_len );
    fpga_pci_get_all_slot_specs( spec_array, array_len );
    if( spec_array[mBoardNumber].map[FPGA_APP_PF].device_id == AWS_UserPF_DEVICE_ID ) {
        std::string agfi = DEFAULT_GLOBAL_AFI;
        fpga_mgmt_load_local_image( mBoardNumber, const_cast<char*>(agfi.c_str()) );
        if( sleepUntilLoaded( agfi ) ) {
            std::cout << "ERROR: Sleep until load failed." << std::endl;
            return -1;
        }
        fpga_pci_rescan_slot_app_pfs( mBoardNumber );
    }
    return 0;
}

int AwsDev::sleepUntilLoaded( const std::string &afi )
{
    for( int i = 0; i < 20; i++ ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        fpga_mgmt_image_info info;
        std::memset( &info, 0, sizeof(struct fpga_mgmt_image_info) );
        int result = fpga_mgmt_describe_local_image( mBoardNumber, &info, 0 );
        if( result ) {
            std::cout << "ERROR: Load image failed." << std::endl;
            return 1;
        }
        if( (info.status == FPGA_STATUS_LOADED) && !std::strcmp(info.ids.afi_id, const_cast<char*>(afi.c_str())) ) {
            break;
        }
    }
    return 0;
}

int AwsDev::checkAndSkipReload( char *afi_id, fpga_mgmt_image_info *orig_info )
{
    if( (orig_info->status == FPGA_STATUS_LOADED) && !std::strcmp(orig_info->ids.afi_id, afi_id) ) {
        std::cout << "This AFI already loaded. Skip reload!" << std::endl;
        int result = 0;
        //existing afi matched.
        uint16_t status = 0;
        result = fpga_mgmt_get_vDIP_status(mBoardNumber, &status);
        if(result) {
            std::cout << "Error: can not get virtual DIP Switch state" << std::endl;
            return result;
        }
        //Set bit 0 to 1
        status |=  (1 << 0);
        result = fpga_mgmt_set_vDIP(mBoardNumber, status);
        if(result) {
            std::cout << "Error trying to set virtual DIP Switch" << std::endl;
            return result;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(250));
        //pulse the changes in.
        result = fpga_mgmt_get_vDIP_status(mBoardNumber, &status);
        if(result) {
            std::cout << "Error: can not get virtual DIP Switch state" << std::endl;
            return result;
        }
        //Set bit 0 to 0
        status &=  ~(1 << 0);
        result = fpga_mgmt_set_vDIP(mBoardNumber, status);
        if(result) {
            std::cout << "Error trying to set virtual DIP Switch" << std::endl;
            return result;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(250));
        
        std::cout << "Successfully skipped reloading of local image." << std::endl;
        return result;
    } else {
        std::cout << "AFI not yet loaded, proceed to download." << std::endl;
        return 1;
    }
}

char *AwsDev::get_afi_from_axlf(const axlf *buffer)
{
    const axlf_section_header *bit_header = xclbin::get_axlf_section(buffer, BITSTREAM);
    char *afid = const_cast<char *>(reinterpret_cast<const char *>(buffer));
    afid += bit_header->m_sectionOffset;
    if (bit_header->m_sectionSize > AFI_ID_STR_MAX)
        return nullptr;
    if (std::memcmp(afid, "afi-", 4) && std::memcmp(afid, "agfi-", 5))
        return nullptr;
    return afid;
}
#endif
