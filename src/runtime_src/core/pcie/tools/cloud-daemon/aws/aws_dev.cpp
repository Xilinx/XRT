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
#include <fstream>
#include <exception>
#include <future>
#include <uuid/uuid.h>
#include "xrt/detail/xclbin.h"
#include "aws_dev.h"

static std::map<std::string, size_t>index_map;
#ifndef INTERNAL_TESTING_FOR_AWS
static std::thread rescan_thread[16];
#endif
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
        syslog(LOG_INFO, "aws: no device found");
        return ret;
    }
    ret = AwsDev::init(index_map);
    if (ret)
        return ret;    
    if (cbs) 
    {
        // hook functions
        cbs->mpc_cookie = NULL;
        cbs->get_remote_msd_fd = get_remote_msd_fd;
        cbs->mb_notify = mb_notify;
        cbs->mb_req.load_xclbin = awsLoadXclBin;
        cbs->mb_req.peer_data.get_icap_data = awsGetIcap;
        cbs->mb_req.peer_data.get_sensor_data = awsGetSensor;
        cbs->mb_req.peer_data.get_board_info = awsGetBdinfo;
        cbs->mb_req.peer_data.get_mig_data = awsGetMig;
        cbs->mb_req.peer_data.get_firewall_data = awsGetFirewall;
        cbs->mb_req.peer_data.get_dna_data = awsGetDna;
        cbs->mb_req.peer_data.get_subdev_data = awsGetSubdev;
        cbs->mb_req.hot_reset = awsResetDevice;
        cbs->mb_req.reclock2 = awsReClock2;
        cbs->mb_req.user_probe = awsUserProbe;
        cbs->mb_req.program_shell = awsProgramShell;
        cbs->mb_req.read_p2p_bar_addr = awsReadP2pBarAddr;
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
 * callback function that is used to notify xocl the imagined xclmgmt
 * online/offline
 * Input:
 *        index: index of the user PF
 *        fd: mailbox file descriptor
 *        online: online or offline 
 * Output:
 *        None
 * Return value:
 *        0: success
 *        others: err code
 */
int mb_notify(size_t index, int fd, bool online)
{
    std::unique_ptr<sw_msg> swmsg;
    struct xcl_mailbox_req *mb_req = NULL;
    struct xcl_mailbox_peer_state mb_conn = { 0 };
    size_t data_len = sizeof(struct xcl_mailbox_peer_state) + sizeof(struct xcl_mailbox_req);
    pcieFunc dev(index);
   
    std::vector<char> buf(data_len, 0);
    mb_req = reinterpret_cast<struct xcl_mailbox_req *>(buf.data());

    mb_req->req = XCL_MAILBOX_REQ_MGMT_STATE;
    if (online)
        mb_conn.state_flags |= XCL_MB_STATE_ONLINE;
    else
        mb_conn.state_flags |= XCL_MB_STATE_OFFLINE;
    memcpy(mb_req->data, &mb_conn, sizeof(mb_conn));

    try {
        swmsg = std::make_unique<sw_msg>(mb_req, data_len, 0x1234, XCL_MB_REQ_FLAG_REQUEST);
    } catch (std::exception &e) {
        std::cout << "aws mb_notify: " << e.what() << std::endl;
        throw;
    }

    struct queue_msg msg;
    msg.localFd = fd;
    msg.type = REMOTE_MSG;
    msg.cb = nullptr;
    msg.data = std::move(swmsg);

    return handleMsg(dev, msg);    
}

/*
 * callback function that is used to handle MAILBOX_REQ_LOAD_XCLBIN msg
 * 
 * Input:
 *        index: index of the FPGA device
 *        awsbin: the aws xclbin file
 * Output:
 *        resp: int as response msg
 * Return value:          
 *        0: success
 *        others: err code
 */
int awsLoadXclBin(size_t index, const axlf *xclbin, int *resp)
{
	int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood()) {
        *resp = d.awsLoadXclBin(xclbin);
        ret = 0;
    }
    return ret;
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
int awsGetIcap(size_t index, xcl_pr_region *resp)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood())
        ret = d.awsGetIcap(resp);
    return ret;
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
int awsGetSensor(size_t index, xcl_sensor *resp)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood())
        ret = d.awsGetSensor(resp);
    return ret;
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind BDINFO
 * 
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetBdinfo(size_t index, xcl_board_info *resp)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood())
        ret = d.awsGetBdinfo(resp);
    return ret;
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind MIG_ECC
 * 
 * Input:
 *        index: index of the FPGA device
 *        resp_len: response msg length
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetMig(size_t index, char *resp, size_t resp_len)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood())
        ret = d.awsGetMig(resp, resp_len);
    return ret;
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
int awsGetFirewall(size_t index, xcl_mig_ecc *resp)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood())
        ret = d.awsGetFirewall(resp);
    return ret;
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
int awsGetDna(size_t index, xcl_dna *resp)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood())
        ret = d.awsGetDna(resp);
    return ret;
}

/*
 * callback function that is used to handle MAILBOX_REQ_PEER_DATA msg
 * kind SUBDEV
 *
 * Input:
 *        index: index of the FPGA device
 *        resp_len: response msg length
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsGetSubdev(size_t index, char *resp, size_t resp_len)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood())
        ret = d.awsGetSubdev(resp, resp_len);
    return ret;
}

/*
 * Reset requires the mailbox msg return before the real reset
 * happens. So we run user special reset in async thread.
 */
std::future<void> nouse; //so far we don't care the return value of reset
static void awsResetDeviceAsync(size_t index)
{
    AwsDev d(index, nullptr);
    if (d.isGood()) {
        d.awsResetDevice();
    }
}
/*
 * callback function that is used to handle MAILBOX_REQ_HOT_RESET msg
 * 
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: int as response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsResetDevice(size_t index, int *resp)
{
    *resp = -ENOTSUP;
    nouse = std::async(std::launch::async, &awsResetDeviceAsync, index);
    return 0;
}

/*
 * callback function that is used to handle MAILBOX_REQ_RECLOCK msg
 * 
 * Input:
 *        index: index of the FPGA device
 *        obj: the mailbox req msg of type 'struct xclmgmt_ioc_freqscaling'
 * Output:
 *        resp: int as response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsReClock2(size_t index, const xclmgmt_ioc_freqscaling *obj, int *resp)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood()) {
        *resp = d.awsReClock2(obj);
        ret = 0;
    }
    return ret;
}

/*
 * callback function that is used to handle MAILBOX_REQ_USER_PROBE msg 
 *
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsUserProbe(size_t index, xcl_mailbox_conn_resp *resp)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood()) {
        ret = d.awsUserProbe(resp);
    }
    return ret;
}

/*
 * callback function that is used to handle MAILBOX_REQ_PROGRAM_SHELL msg 
 *
 * Input:
 *        index: index of the FPGA device
 * Output:
 *        resp: int as response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsProgramShell(size_t index, int *resp)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood()) {
        *resp = d.awsProgramShell();
        ret = 0;
    }
    return ret;
}

/*
 * callback function that is used to handle MAILBOX_REQ_READ_P2P_BAR_ADDR msg 
 *
 * Input:
 *        index: index of the FPGA device
 *        addr: p2p bar addr
 * Output:
 *        resp: int as response msg
 * Return value:
 *        0: success
 *        others: err code
 */
int awsReadP2pBarAddr(size_t index, const xcl_mailbox_p2p_bar_addr *addr, int *resp)
{
    int ret = -1;
    AwsDev d(index, nullptr);
    if (d.isGood()) {
        *resp = d.awsReadP2pBarAddr(addr);
        ret = 0;
    }
    return ret;
}

#ifndef INTERNAL_TESTING_FOR_AWS
static void awsPciRescan(int index)
{
    std::string sysfs_name = xrt_core::pci::get_dev(index, true)->m_sysfs_name;
    int mBoardNumber = index_map[sysfs_name];
    std::this_thread::sleep_for(std::chrono::seconds(1));
    int dev_offline = -1;
    std::string err;
    xclDeviceHandle handle;
    uint64_t hostmem_size;
    int ret;
    
    // removal & rescan will make the host mem configed lost. so if there is host mem config
    // save the info and reconfig it after the removal & rescan
    xrt_core::pci::get_dev(index)->sysfs_get("", "host_mem_size", err, hostmem_size, static_cast<uint64_t>(0));
    if (hostmem_size)
        std::cout << "host mem config information saved: " << hostmem_size << std::endl;
    
    fpga_pci_rescan_slot_app_pfs(mBoardNumber);
    while (dev_offline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
	xrt_core::pci::get_dev(index)->sysfs_get<int>("", "dev_offline", err, dev_offline, -1);
    }
    if (!hostmem_size) {
        //no need to reconfig host mem, just tell the user xclbin load ioctl can be re-issued now
	    xrt_core::pci::get_dev(index)->sysfs_put("", "dev_hotplug_done", err, 1);
        return;
    }
    
    handle = xclOpen(index, nullptr, XCL_QUIET);
    // there is no way to return this failure, if any, to user
    // so just save log in case there is failure here
    // still need to notify user to avoid hang
    if (!handle) {
        std::cerr << "host mem config not recovered" << std::endl;
	xrt_core::pci::get_dev(index)->sysfs_put("", "dev_hotplug_done", err, 1);
        return;
    }
    std::cout << "host mem reconfig (size: " << hostmem_size << ")..." << std::endl;
    ret = xclCmaEnable(handle, true, hostmem_size);
    std::cout << "host mem reconfig: " << ret << std::endl;
    xclClose(handle);
    
    //tell the user xclbin load ioctl can be re-issued now
    xrt_core::pci::get_dev(index)->sysfs_put("", "dev_hotplug_done", err, 1);
}
#endif

/*
 * On AWS F1, fpga user PF without xclbin being loaded (cleared) has different
 * device id (0x1042) than that of the user PF with xclbin being loaded (0xf010)
 * Changing the device id needs pci node remove and rescan.
 * fpga_pci_rescan_slot_app_pfs( mBoardNumber ) is the function used to do that.
 * Doing so in the context of xclbin download ioctl is impossible.
 * What we are doing here is, if pcie removal & rescan is required, we will return
 * EAGAIN to userload, at the same time, we do removal & rescan in a separate thread,
 * Userload code (shim), when seeing EAGAIN, will retry the xclbin download after
 * the device id has been changed.
 * This is all transparent to end user.
 */
int AwsDev::awsLoadXclBin(const xclBin *buffer)
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
    axlf *axlfbuffer = reinterpret_cast<axlf*>(const_cast<xclBin*> (buffer));
    char* afi_id = get_afi_from_axlf(axlfbuffer);

    if (!afi_id)
        return -EINVAL;

    // get old image info before loading new
    // new image info can only be achieved after being loaded
    fpga_mgmt_image_info imageInfoOld;
    fpga_mgmt_describe_local_image(mBoardNumber, &imageInfoOld, 0);

    int retVal = 0;
    union fpga_mgmt_load_local_image_options opt;
    // force data retention option
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
        std::cerr << "Failed to load AFI, error: " << retVal << std::endl;
        return -retVal;
    }

    fpga_mgmt_image_info imageInfoNew;
    retVal = sleepUntilLoaded(std::string(afi_id), &imageInfoNew);
    if (retVal) {
        std::cerr << "Failed to load AFI, error: " << retVal << std::endl;
        return -retVal;
    }

    // if there is device id change, or shell version change (2rp case), we do a rescan
    // at the same time, we ask the user (shim) to reload the 2nd time later
    std::cout << "device id before load: 0x" <<  std::hex << imageInfoOld.spec.map[FPGA_APP_PF].device_id << std::endl;
    std::cout << "device id after load: 0x" <<  imageInfoNew.ids.afi_device_ids.device_id << std::endl;
    std::cout << "shell version before load: 0x" <<  imageInfoOld.sh_version << std::endl;
    std::cout << "shell version after load: 0x" <<  imageInfoNew.sh_version << std::dec << std::endl;
    if (imageInfoOld.spec.map[FPGA_APP_PF].device_id != imageInfoNew.ids.afi_device_ids.device_id ||
        imageInfoOld.sh_version != imageInfoNew.sh_version) {
        std::cout << "pci removal & rescan..." << std::endl;
        std::string err;
	xrt_core::pci::get_dev(index)->sysfs_put("", "dev_hotplug_done", err, 0);
	    
        if (rescan_thread[mBoardNumber].joinable())
                rescan_thread[mBoardNumber].join();
        rescan_thread[mBoardNumber]  = std::thread(awsPciRescan, index);
        return -EAGAIN;
    }
    
    return 0;
#endif
}

int AwsDev::awsGetIcap(xcl_pr_region *data)
{
#define FIELD(var, field, index) (var->field##_##index)
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
    data->data_retention = 1;
#else
    fpga_mgmt_image_info imageInfo;
    fpga_mgmt_describe_local_image( mBoardNumber, &imageInfo, 0 );
    FIELD(data, freq, 0) = imageInfo.metrics.clocks[0].frequency[0] / 1000000;
    FIELD(data, freq, 1) = imageInfo.metrics.clocks[1].frequency[0] / 1000000;
    FIELD(data, freq, 2) = imageInfo.metrics.clocks[2].frequency[0] / 1000000;
    FIELD(data, freq_cntr, 0) = imageInfo.metrics.clocks[0].frequency[0] / 1000;
    FIELD(data, freq_cntr, 1) = imageInfo.metrics.clocks[1].frequency[0] / 1000;
    FIELD(data, freq_cntr, 2) = imageInfo.metrics.clocks[2].frequency[0] / 1000;
    data->data_retention = 1;
#endif
    //do we need to save uuid of xclbin loaded so that we can return xclbin uuid here?
    //seems not. we check afi before load new xclbin.
    return 0;
}

int AwsDev::awsGetSensor(xcl_sensor *sensor)
{
//TODO
    return -ENOTSUP;
}

int AwsDev::awsGetBdinfo(xcl_board_info *bdinfo)
{
//TODO
    return -ENOTSUP;
}

int AwsDev::awsGetMig(char *mig, size_t resp_len)
{
//TODO
    return -ENOTSUP;
}

int AwsDev::awsGetFirewall(xcl_mig_ecc *firewall)
{
//TODO
    return -ENOTSUP;
}

int AwsDev::awsGetDna(xcl_dna *dna)
{
//TODO
    return -ENOTSUP;
}

int AwsDev::awsGetSubdev(char *subdev, size_t resp_len)
{
//TODO
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

int AwsDev::awsUserProbe(xcl_mailbox_conn_resp *resp)
{
    resp->conn_flags |= XCL_MB_PEER_READY;
    return 0;
}

int AwsDev::awsResetDevice() {
    // AWS FIXME - add reset
    return 0;
}

int AwsDev::awsReClock2(const xclmgmt_ioc_freqscaling *obj) {
#ifdef INTERNAL_TESTING_FOR_AWS
    return ioctl(mMgtHandle, XCLMGMT_IOCFREQSCALE, obj);
#else
    int retVal = 0;
    fpga_mgmt_image_info orig_info;
    std::memset(&orig_info, 0, sizeof(struct fpga_mgmt_image_info));
    fpga_mgmt_describe_local_image(mBoardNumber, &orig_info, 0);
    if(orig_info.status == FPGA_STATUS_LOADED) {
        std::cout << "Reclock AFI(" << orig_info.ids.afi_id << ")" << std::endl;
        union fpga_mgmt_load_local_image_options opt;
        fpga_mgmt_init_load_local_image_options(&opt);
        opt.afi_id = orig_info.ids.afi_id;
        opt.slot_id = mBoardNumber;
        opt.clock_mains[0] = obj->ocl_target_freq[0];
        opt.clock_mains[1] = obj->ocl_target_freq[1];
        opt.clock_mains[2] = obj->ocl_target_freq[2];
        retVal = fpga_mgmt_load_local_image_with_options(&opt);
        if (retVal) {
            std::cout << "Failed to load AFI with freq , error: " << retVal << std::endl;
            return -retVal;
        }
        return retVal;
    }
    return 1;
#endif
}

int AwsDev::awsProgramShell() {
    // AWS FIXME - add 2rp support
    return 0;
}

int AwsDev::awsReadP2pBarAddr(const xcl_mailbox_p2p_bar_addr *addr) {
    // AWS FIXME - add p2p support
    return 0;
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

AwsDev::AwsDev(size_t index, const char *logfileName)
{
    if (logfileName != nullptr) {
        mLogStream.open(logfileName);
        mLogStream << "FUNCTION, THREAD ID, ARG..." << std::endl;
        mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

    std::string sysfs_name = xrt_core::pci::get_dev(index, true)->m_sysfs_name;
    std::cout << "AwsDev: " << sysfs_name << "(index: " << index << ")" << std::endl;
#ifdef INTERNAL_TESTING_FOR_AWS
    mBoardNumber = index;
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
    mBoardNumber = index_map[sysfs_name];
    this->index = index;
    //bar0 is mapped already. seems other 2 bars are not required.
#endif
}

//private functions
#ifndef INTERNAL_TESTING_FOR_AWS
int AwsDev::sleepUntilLoaded( const std::string &afi, fpga_mgmt_image_info *image_info )
{
    for( int i = 0; i < 20; i++ ) {
        std::this_thread::sleep_for( std::chrono::milliseconds( 500 ) );
        fpga_mgmt_image_info info;
        std::memset( &info, 0, sizeof(struct fpga_mgmt_image_info) );
        int result = fpga_mgmt_describe_local_image( mBoardNumber, &info, 0 );
        if( result ) {
            std::cout << "ERROR: Load image failed." << std::endl;
            return 1;
        }
        if( (info.status == FPGA_STATUS_LOADED) && !std::strcmp(info.ids.afi_id, const_cast<char*>(afi.c_str())) ) {
            *image_info = info;
            break;
        }
    }
    return 0;
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
