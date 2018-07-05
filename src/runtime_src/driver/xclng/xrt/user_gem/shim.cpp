/**
 * Copyright (C) 2016-2018 Xilinx, Inc
 * Author(s): Umang Parekh
 *          : Sonal Santan
 *          : Ryan Radjabi
 * PCIe HAL Driver layered on top of XOCL GEM kernel driver
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

#include "shim.h"
#include <errno.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/file.h>
#include <poll.h>
#include <dirent.h>
#include "driver/include/xclbin.h"
#include "scan.h"
#include "xbsak.h"

#ifdef NDEBUG
# undef NDEBUG
# include<cassert>
#endif

#if defined(__GNUC__)
#define SHIM_UNUSED __attribute__((unused))
#endif

#define GB(x)           ((size_t) (x) << 30)
#define	USER_PCIID(x)   (((x).bus << 8) | ((x).device << 3) | (x).user_func)
#define ARRAY_SIZE(x)   (sizeof (x) / sizeof (x[0]))

inline bool
is_multiprocess_mode()
{
  static bool val = std::getenv("XCL_MULTIPROCESS_MODE") != nullptr;
  return val;
}

/*
 * wordcopy()
 *
 * Copy bytes word (32bit) by word.
 * Neither memcpy, nor std::copy work as they become byte copying on some platforms.
 */
inline void* wordcopy(void *dst, const void* src, size_t bytes)
{
    // assert dest is 4 byte aligned
    assert((reinterpret_cast<intptr_t>(dst) % 4) == 0);

    using word = uint32_t;
    auto d = reinterpret_cast<word*>(dst);
    auto s = reinterpret_cast<const word*>(src);
    auto w = bytes/sizeof(word);

    for (size_t i=0; i<w; ++i)
    {
        d[i] = s[i];
    }

    return dst;
}

/*
 * newDeviceName()
 */
const std::string xocl::newDeviceName(const std::string& name)
{
    auto i = deviceOld2NewNameMap.find(name);
    return (i == deviceOld2NewNameMap.end()) ? name : i->second;
}

/*
 * numClocks()
 */
unsigned xocl::numClocks(const std::string& name)
{
    return name.compare(0, 15, "xilinx_adm-pcie", 15) ? 2 : 1;
}

/*
 * operator <<
 */
std::ostream& xocl::operator<< (std::ostream &strm, const AddressRange &rng)
{
    strm << "[" << rng.first << ", " << rng.second << "]";
    return strm;
}

/*
 * XOCLShim()
 */
xocl::XOCLShim::XOCLShim(unsigned index,
                         const char *logfileName,
                         xclVerbosityLevel verbosity) : mVerbosity(verbosity),
                                                        mBoardNumber(index),
                                                        mMgtMap(0),
                                                        mLocked(false),
                                                        mOffsets{0x0, 0x0, OCL_CTLR_BASE, 0x0, 0x0},
                                                        mMemoryProfilingNumberSlots(0),
                                                        mAccelProfilingNumberSlots(0),
                                                        mStallProfilingNumberSlots(0)
{
    mLogfileName = nullptr;
    init(index, logfileName, verbosity);
}

/*
 * init()
 */
void xocl::XOCLShim::init(unsigned index, const char *logfileName, xclVerbosityLevel verbosity)
{
    const std::string devName = "/dev/dri/renderD" + std::to_string(xcldev::pci_device_scanner::device_list[index].user_instance);
    mUserHandle = open(devName.c_str(), O_RDWR);
    if(mUserHandle > 0) {
        // Lets map 4M
        mUserMap = (char *)mmap(0, xcldev::pci_device_scanner::device_list[index].user_bar_size, PROT_READ | PROT_WRITE, MAP_SHARED, mUserHandle, 0);
        if (mUserMap == MAP_FAILED) {
            std::cout << "Map failed: " << devName << std::endl;
            close(mUserHandle);
            mUserHandle = -1;
        }
    } else {
        std::cout << "Cannot open: " << devName << std::endl;
    }
    if( logfileName != nullptr ) {
        mLogStream.open(logfileName);
        mLogStream << "FUNCTION, THREAD ID, ARG..." << std::endl;
        mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
    }

    std::string mgmtFile = "/dev/xclmgmt"+ std::to_string(xcldev::pci_device_scanner::device_list[index].mgmt_instance);
    mMgtHandle = open(mgmtFile.c_str(), O_RDWR | O_SYNC);
    if(mMgtHandle < 0) {
        std::cout << "Could not open " << mgmtFile << std::endl;
        return;
    }
    mMgtMap = (char *)mmap(0, xcldev::pci_device_scanner::device_list[index].user_bar_size, PROT_READ | PROT_WRITE, MAP_SHARED, mMgtHandle, 0);
    if (mMgtMap == MAP_FAILED) // Not an error if user is not privileged
        mMgtMap = nullptr;

    if (xclGetDeviceInfo2(&mDeviceInfo)) {
        if(mMgtHandle > 0) {
            close(mMgtHandle);
            mMgtHandle = -1;
        }
    }

    std::string streamFile = "/dev/str_dma.u"+ std::to_string(USER_PCIID(xcldev::pci_device_scanner::device_list[index]));
    mStreamHandle = open(streamFile.c_str(), O_RDWR | O_SYNC);

// dr_base_addr has moved. Uncomment when that location has been resolved.
//    try {
//        std::string dev_name = "/sys/bus/pci/devices/" + xcldev::pci_device_scanner::device_list[index].user_name;
//        uint64_t dr_base_addr = xcldev::get_val_long(dev_name, "dr_base_addr");
//        //std::cout << "dr_base_offset: " << dr_base_addr << std::endl;
//        mOffsets[XCL_ADDR_KERNEL_CTRL] += dr_base_addr;
//    }
//    catch (std::exception &ex) {
//        std::cout << ex.what() <<  std::endl;
//    }

    //
    // Profiling - defaults
    // Class-level defaults: mIsDebugIpLayoutRead = mIsDeviceProfiling = false
    mDevUserName = xcldev::pci_device_scanner::device_list[index].user_name;
    mMemoryProfilingNumberSlots = 0;
    mPerfMonFifoCtrlBaseAddress = 0x00;
    mPerfMonFifoReadBaseAddress = 0x00;
}

/*
 * ~XOCLShim()
 */
xocl::XOCLShim::~XOCLShim()
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
        mLogStream.close();
    }

    if (mUserMap != MAP_FAILED)
        munmap(mUserMap, xcldev::pci_device_scanner::device_list[mBoardNumber].user_bar_size);

    if (mMgtMap)
        munmap(mMgtMap, xcldev::pci_device_scanner::device_list[mBoardNumber].user_bar_size);

    if (mUserHandle > 0)
        close(mUserHandle);

    if (mMgtHandle > 0)
        close(mMgtHandle);

    if (mStreamHandle > 0)
        close(mStreamHandle);
}

/*
 * pcieBarRead()
 */
int xocl::XOCLShim::pcieBarRead(unsigned int pf_bar, unsigned long long offset, void* buffer, unsigned long long length)
{
    const char *mem = 0;
    switch (pf_bar) {
        case 0:
        {
            // BAR0 on PF0
            mem = mUserMap;
            break;
        }
        case 0x10000:
        {
            // BAR0 on PF1
            mem = mMgtMap;
            break;
        }
        default:
        {
            return -1;
        }
    }
    wordcopy(buffer, mem + offset, length);
    return 0;
}

/*
 * pcieBarWrite()
 */
int xocl::XOCLShim::pcieBarWrite(unsigned int pf_bar, unsigned long long offset, const void* buffer, unsigned long long length)
{
    char *mem = 0;
    switch (pf_bar) {
        case 0:
        {
            // BAR0 on PF0
            mem = mUserMap;
            break;
        }
        case 0x10000:
        {
            // BAR0 on PF1
            mem = mMgtMap;
            break;
        }
        default:
        {
            return -1;
        }
    }

    wordcopy(mem + offset, buffer, length);
    return 0;
}

/*
 * xclWrite()
 */
size_t xocl::XOCLShim::xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
    switch (space) {
        case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
            //offset += mOffsets[XCL_ADDR_SPACE_DEVICE_PERFMON];
            if (pcieBarWrite(SHIM_USER_BAR, offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        case XCL_ADDR_KERNEL_CTRL:
        {
            offset += mOffsets[XCL_ADDR_KERNEL_CTRL];
            if (mLogStream.is_open()) {
                const unsigned *reg = static_cast<const unsigned *>(hostBuf);
                size_t regSize = size / 4;
                if (regSize > 32)
                regSize = 32;
                for (unsigned i = 0; i < regSize; i++) {
                    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << space << ", 0x"
                               << std::hex << offset + i << ", 0x" << std::hex << std::setw(8)
                               << std::setfill('0') << reg[i] << std::dec << std::endl;
                }
            }
            if (pcieBarWrite(SHIM_USER_BAR, offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        case XCL_ADDR_SPACE_DEVICE_CHECKER:
        default:
        {
            return -EPERM;
        }
    }
}

/*
 * xclRead()
 */
size_t xocl::XOCLShim::xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << space << ", "
                   << offset << ", " << hostBuf << ", " << size << std::endl;
    }

    switch (space) {
        case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
            //offset += mOffsets[XCL_ADDR_SPACE_DEVICE_PERFMON];
            if (pcieBarRead(SHIM_USER_BAR, offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        case XCL_ADDR_KERNEL_CTRL:
        {
            offset += mOffsets[XCL_ADDR_KERNEL_CTRL];
            int result = pcieBarRead(SHIM_USER_BAR, offset, hostBuf, size);
            if (mLogStream.is_open()) {
                const unsigned *reg = static_cast<const unsigned *>(hostBuf);
                size_t regSize = size / 4;
                if (regSize > 4)
                regSize = 4;
                for (unsigned i = 0; i < regSize; i++) {
                    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << space << ", 0x"
                               << std::hex << offset + i << std::dec << ", 0x" << std::hex << reg[i] << std::dec << std::endl;
                }
            }
            return !result ? size : 0;
        }
        case XCL_ADDR_SPACE_DEVICE_CHECKER:
        {
            if (pcieBarRead(SHIM_USER_BAR, offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        default:
        {
            return -EPERM;
        }
    }
}

/*
 * xclAllocBO()
 *
 * Assume that the memory is always created for the device ddr for now. Ignoring the flags as well.
 */
unsigned int xocl::XOCLShim::xclAllocBO(size_t size, xclBOKind domain, uint64_t flags)
{
    std::cout << "alloc bo with combined flags " << std::hex << flags ;
    unsigned flag = flags & 0xFFFFFFFFLL;
    unsigned type =  (unsigned)(flags >> 32);
    std::cout << " split flags "  << std::hex << flag << " " << type << std::dec << std::endl;
    drm_xocl_create_bo info = {size, mNullBO, flag, type};
    int result = ioctl(mUserHandle, DRM_IOCTL_XOCL_CREATE_BO, &info);
    return result ? mNullBO : info.handle;
}

/*
 * xclAllocUserPtrBO()
 */
unsigned int xocl::XOCLShim::xclAllocUserPtrBO(void *userptr, size_t size, uint64_t flags)
{
    std::cout << "User alloc bo with combined flags " << flags ;
    unsigned flag = flags & 0xFFFFFFFFLL;
    unsigned type =  (unsigned)(flags >> 32);
    std::cout << " split flags "  << std::hex << flag << " " << type << std::dec << std::endl;
    drm_xocl_userptr_bo user = {reinterpret_cast<uint64_t>(userptr), size, mNullBO, flag, type};
    int result = ioctl(mUserHandle, DRM_IOCTL_XOCL_USERPTR_BO, &user);
    return result ? mNullBO : user.handle;
}

/*
 * xclFreeBO()
 */
void xocl::XOCLShim::xclFreeBO(unsigned int boHandle)
{
    drm_gem_close closeInfo = {boHandle, 0};
    ioctl(mUserHandle, DRM_IOCTL_GEM_CLOSE, &closeInfo);
}

/*
 * xclWriteBO()
 */
int xocl::XOCLShim::xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek)
{
    drm_xocl_pwrite_bo pwriteInfo = { boHandle, 0, seek, size, reinterpret_cast<uint64_t>(src) };
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfo);
}

/*
 * xclReadBO()
 */
int xocl::XOCLShim::xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip)
{
    drm_xocl_pread_bo preadInfo = { boHandle, 0, skip, size, reinterpret_cast<uint64_t>(dst) };
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo);
}

/*
 * xclMapBO()
 */
void *xocl::XOCLShim::xclMapBO(unsigned int boHandle, bool write)
{
    drm_xocl_info_bo info = { boHandle, 0, 0 };
    int result = ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &info);
    if (result) {
        return nullptr;
    }

    drm_xocl_map_bo mapInfo = { boHandle, 0, 0 };
    result = ioctl(mUserHandle, DRM_IOCTL_XOCL_MAP_BO, &mapInfo);
    if (result) {
        return nullptr;
    }

    return mmap(0, info.size, (write ? (PROT_READ|PROT_WRITE) : PROT_READ),
              MAP_SHARED, mUserHandle, mapInfo.offset);
}

/*
 * xclSyncBO()
 */
int xocl::XOCLShim::xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
    drm_xocl_sync_bo_dir drm_dir = (dir == XCL_BO_SYNC_BO_TO_DEVICE) ?
            DRM_XOCL_SYNC_BO_TO_DEVICE :
            DRM_XOCL_SYNC_BO_FROM_DEVICE;
    drm_xocl_sync_bo syncInfo = {boHandle, 0, size, offset, drm_dir};
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
}

/*
 * xclCopyBO()
 */
int xocl::XOCLShim::xclCopyBO(unsigned int dst_boHandle, unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
    drm_xocl_copy_bo copyInfo = {dst_boHandle, src_boHandle, 0, size, dst_offset, src_offset};
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_COPY_BO, &copyInfo);
}

/*
 * xclSysfsGetErrorStatus()
 */
void xocl::XOCLShim::xclSysfsGetErrorStatus(xclErrorStatus& stat)
{
    unsigned int status = xclSysfsGetInt(true, "firewall", "detected_status");
    unsigned int level = xclSysfsGetInt(true, "firewall", "detected_level");
    unsigned long time = xclSysfsGetInt(true, "firewall", "detected_time");

    stat.mNumFirewalls = XCL_FW_MAX_LEVEL;
    for (unsigned i = 0; i < stat.mNumFirewalls; i++) {
        stat.mAXIErrorStatus[i].mErrFirewallID = static_cast<xclFirewallID>(i);
    }

    if (status && (level < ARRAY_SIZE(stat.mAXIErrorStatus))) {
        stat.mAXIErrorStatus[level].mErrFirewallStatus = status;
        stat.mAXIErrorStatus[level].mErrFirewallTime = time;
    }
}

/*
 * xclGetErrorStatus()
 */
int xocl::XOCLShim::xclGetErrorStatus(xclErrorStatus *info)
{
#ifdef AXI_FIREWALL
    std::memset(info, 0, sizeof(xclErrorStatus));

    // Obtain error status from sysfs. Fall back to IOCTL, if not supported.
    xclErrorStatus err_obj = { 0 };
    if (xclSysfsGetInt(true, "", "version") > 0) {
        xclSysfsGetErrorStatus(err_obj);
    } else {
        int ret = ioctl(mMgtHandle, XCLMGMT_IOCERRINFO, &err_obj);
        if (ret) {
            return ret;
        }
    }

    info->mNumFirewalls = err_obj.mNumFirewalls;
    std::memcpy(&info->mAXIErrorStatus[0], &err_obj.mAXIErrorStatus[0],
        sizeof(struct xclAXIErrorStatus) * info->mNumFirewalls);
#endif  // AXI Firewall
    return 0;
}

/*
 * xclSysfsGetDeviceInfo()
 */
void xocl::XOCLShim::xclSysfsGetDeviceInfo(xclmgmt_ioc_info& info)
{
    info.vendor =             xclSysfsGetInt(true, "", "vendor");
    info.device =             xclSysfsGetInt(true, "", "device");
    info.subsystem_vendor =   xclSysfsGetInt(true, "", "subsystem_vendor");
    info.subsystem_device =   xclSysfsGetInt(true, "", "subsystem_device");
    info.driver_version =     xclSysfsGetInt(true, "", "version");
    info.pci_slot =           xclSysfsGetInt(true, "", "slot");
    info.pcie_link_speed =    xclSysfsGetInt(true, "", "link_speed");
    info.pcie_link_width =    xclSysfsGetInt(true, "", "link_width");
    info.isXPR =              xclSysfsGetInt(true, "", "xpr");
    info.mig_calibration[0] = xclSysfsGetInt(true, "", "mig_calibration");
    info.mig_calibration[1] = info.mig_calibration[0];
    info.mig_calibration[2] = info.mig_calibration[0];
    info.mig_calibration[3] = info.mig_calibration[0];

    info.ddr_channel_num =  xclSysfsGetInt(true, "rom", "ddr_bank_count_max");
    info.ddr_channel_size = xclSysfsGetInt(true, "rom", "ddr_bank_size");
    info.time_stamp =       xclSysfsGetInt(true, "rom", "timestamp");
    snprintf(info.vbnv, sizeof (info.vbnv), "%s",
                            xclSysfsGetString(true, "rom", "VBNV").c_str());
    snprintf(info.fpga, sizeof (info.fpga), "%s",
                            xclSysfsGetString(true, "rom", "FPGA").c_str());

    info.onchip_temp = xclSysfsGetInt(true, "sysmon", "temp") / 1000;
    info.vcc_int =     xclSysfsGetInt(true, "sysmon", "vcc_int");
    info.vcc_aux =     xclSysfsGetInt(true, "sysmon", "vcc_aux");
    info.vcc_bram =    xclSysfsGetInt(true, "sysmon", "vcc_bram");

    auto freqs = xclSysfsGetInts(true, "icap", "clock_freqs");
    for (unsigned i = 0;
        i < std::min(freqs.size(), ARRAY_SIZE(info.ocl_frequency));
        i++) {
        info.ocl_frequency[i] = freqs[i];
    }
}

/*
 * xclGetDeviceInfo2()
 */
int xocl::XOCLShim::xclGetDeviceInfo2(xclDeviceInfo2 *info)
{
    std::memset(info, 0, sizeof(xclDeviceInfo2));
    info->mMagic = 0X586C0C6C;
    info->mHALMajorVersion = XCLHAL_MAJOR_VER;
    info->mHALMajorVersion = XCLHAL_MINOR_VER;
    info->mMinTransferSize = DDR_BUFFER_ALIGNMENT;
    info->mDMAThreads = 2;

    // Obtain device info from sysfs. Will fall back to IOCTL, if not supported.
    xclmgmt_ioc_info obj = { 0 };
    if (xclSysfsGetInt(true, "", "version") > 0) {
        xclSysfsGetDeviceInfo(obj);
    } else {
        int ret = ioctl(mMgtHandle, XCLMGMT_IOCINFO, &obj);
        if (ret) {
            return ret;
        }
    }

    info->mVendorId = obj.vendor;
    info->mDeviceId = obj.device;
    info->mSubsystemId = obj.subsystem_device;
    info->mSubsystemVendorId = obj.subsystem_vendor;
    info->mDeviceVersion = obj.subsystem_device & 0x00ff;
    info->mDataAlignment = KB(4);
    info->mDDRSize = GB(obj.ddr_channel_size);
    info->mDDRBankCount = obj.ddr_channel_num;
    info->mDDRSize *= info->mDDRBankCount;

    const std::string name = newDeviceName(obj.vbnv);
    std::memcpy(info->mName, name.c_str(), name.size() + 1);

    info->mNumClocks = numClocks(info->mName);

    for (int i = 0; i < info->mNumClocks; ++i) {
        info->mOCLFrequency[i] = obj.ocl_frequency[i];
    }

    info->mOnChipTemp  = obj.onchip_temp;
    info->mFanTemp     = obj.fan_temp;
    info->mVInt        = obj.vcc_int;
    info->mVAux        = obj.vcc_aux;
    info->mVBram       = obj.vcc_bram;
    info->mMigCalib    = obj.mig_calibration;
    info->mPCIeLinkWidth = obj.pcie_link_width;
    info->mPCIeLinkSpeed = obj.pcie_link_speed;

    return 0;
}

/*
 * resetDevice()
 */
int xocl::XOCLShim::resetDevice(xclResetKind kind)
{
    // Call a new IOCTL to just reset the OCL region
    if (kind == XCL_RESET_FULL) {
        int ret =  ioctl(mMgtHandle, XCLMGMT_IOCHOTRESET);
        return ret;
    }
    else if (kind == XCL_RESET_KERNEL) {
        return ioctl(mMgtHandle, XCLMGMT_IOCOCLRESET);
    }
    return -EINVAL;
}

/*
 * xclLockDevice()
 */
bool xocl::XOCLShim::xclLockDevice()
{
    if (!is_multiprocess_mode() && flock(mUserHandle, LOCK_EX | LOCK_NB) == -1)
        return false;

    mLocked = true;
    return true;
}

/*
 * xclUnlockDevice()
 */
bool xocl::XOCLShim::xclUnlockDevice()
{
    if (!is_multiprocess_mode())
      flock(mUserHandle, LOCK_UN);

    mLocked = false;
    return true;
}

/*
 * xclReClock2()
 */
int xocl::XOCLShim::xclReClock2(unsigned short region, const unsigned short *targetFreqMHz)
{
    xclmgmt_ioc_freqscaling obj;
    std::memset(&obj, 0, sizeof(xclmgmt_ioc_freqscaling));
    obj.ocl_region = region;
    obj.ocl_target_freq[0] = targetFreqMHz[0];
    obj.ocl_target_freq[1] = targetFreqMHz[1];
    return ioctl(mMgtHandle, XCLMGMT_IOCFREQSCALE, &obj);
}

/*
 * zeroOutDDR()
 */
bool xocl::XOCLShim::zeroOutDDR()
{
    // Zero out the DDR so MIG ECC believes we have touched all the bits
    // and it does not complain when we try to read back without explicit
    // write. The latter usually happens as a result of read-modify-write
    // TODO: Try and speed this up.
    // [1] Possibly move to kernel mode driver.
    // [2] Zero out specific buffers when they are allocated

    // TODO: Implement this
    //  static const unsigned long long BLOCK_SIZE = 0x4000000;
    //  void *buf = 0;
    //  if (posix_memalign(&buf, DDR_BUFFER_ALIGNMENT, BLOCK_SIZE))
    //      return false;
    //  memset(buf, 0, BLOCK_SIZE);
    //  mDataMover->pset64(buf, BLOCK_SIZE, 0, mDeviceInfo.mDDRSize/BLOCK_SIZE);
    //  free(buf);
    return true;
}

/*
 * xclLoadXclBin()
 */
int xocl::XOCLShim::xclLoadXclBin(const xclBin *buffer)
{
    int ret = 0;
    const char *xclbininmemory = reinterpret_cast<char*> (const_cast<xclBin*> (buffer));

    if (!memcmp(xclbininmemory, "xclbin2", 8)) {
        ret = xclLoadAxlf(reinterpret_cast<const axlf*>(xclbininmemory));
    } else {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", Legacy xclbin no longer supported" << std::endl;
        }
        return -EINVAL;
    }

    if( ret != 0 ) {
        std::string path = "/sys/bus/pci/devices/" + xcldev::pci_device_scanner::device_list[mBoardNumber].mgmt_name + "/error";
        std::ifstream errorLog( path );
        if( !errorLog.is_open() ) {
            std::cout << "Error opening: " << path << std::endl;
            return errno;
        } else {
            std::string line;
            std::getline( errorLog, line );
            std::cout << line << std::endl;
        }
        errorLog.close();
    }

    mIsDebugIpLayoutRead = false;

    return ret;
}

/*
 * xclLoadAxlf()
 */
int xocl::XOCLShim::xclLoadAxlf(const axlf *buffer)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << buffer << std::endl;
    }

    if (!mLocked) {
        return -EPERM;
    }

    const unsigned cmd = XCLMGMT_IOCICAPDOWNLOAD_AXLF;
    xclmgmt_ioc_bitstream_axlf obj = {const_cast<axlf *>(buffer)};
    int ret = ioctl(mMgtHandle, cmd, &obj);
    if(0 != ret) {
        return ret;
    }

    // If it is an XPR DSA, zero out the DDR again as downloading the XCLBIN
    // reinitializes the DDR and results in ECC error.
    if(isXPR())
    {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << "XPR Device found, zeroing out DDR again.." << std::endl;
        }

        if (zeroOutDDR() == false)
        {
            if (mLogStream.is_open()) {
                mLogStream <<  __func__ << "zeroing out DDR failed" << std::endl;
            }
            return -EIO;
        }
    }

    // Note: We have frequently seen that downloading the bitstream causes the CU status
    // to go bad. This indicates an HLS issue (most probably). It is better to fail here
    // rather than crashing/erroring out later. This should save a lot of debugging time.
    //if(!checkCUStatus())
    //return -EPERM;

    drm_xocl_axlf axlf_obj = {const_cast<axlf *>(buffer)};
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_READ_AXLF, &axlf_obj);
    return ret;
}

/*
 * xclExportBO()
 */
int xocl::XOCLShim::xclExportBO(unsigned int boHandle)
{
    drm_prime_handle info = {boHandle, 0, -1};
    int result = ioctl(mUserHandle, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
    return !result ? info.fd : result;
}

/*
 * xclImportBO()
 */
unsigned int xocl::XOCLShim::xclImportBO(int fd, unsigned flags)
{
    drm_prime_handle info = {mNullBO, flags, fd};
    int result = ioctl(mUserHandle, DRM_IOCTL_PRIME_FD_TO_HANDLE, &info);
    if (result) {
        std::cout << __func__ << " ERROR: FD to handle IOCTL failed" << std::endl;
    }
    return !result ? info.handle : mNullBO;
}

/*
 * xclGetBOProperties()
 */
int xocl::XOCLShim::xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties)
{
    drm_xocl_info_bo info = {boHandle, 0, mNullBO, mNullAddr};
    int result = ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &info);
    properties->handle = info.handle;
    properties->flags  = info.flags;
    properties->size   = info.size;
    properties->paddr  = info.paddr;
    properties->domain = XCL_BO_DEVICE_RAM; // currently all BO domains are XCL_BO_DEVICE_RAM
    return result;
}

int xocl::XOCLShim::xclGetSectionInfo(void* section_info, size_t * section_size,
	enum axlf_section_kind kind, int index)
{
    if(section_info == nullptr)
	return EFAULT;
    if(section_size == nullptr)
	return EFAULT;

    std::string path = "/sys/bus/pci/devices/" + xcldev::pci_device_scanner::device_list[mBoardNumber].user_name;
    std::ifstream file;
    std::streampos size;
    char* memblock;

    if(kind == MEM_TOPOLOGY)
	path += "/mem_topology";
    else if (kind == CONNECTIVITY)
	path += "/connectivity";
    else if (kind == IP_LAYOUT)
	path += "/ip_layout";
    else {
	std::cout << "Unhandled section found" << std::endl;
	return EINVAL;
    }

    file.open(path.c_str(),std::ifstream::binary);
    if(!file.good())
	return EBADFD;

    size = file.tellg();
    memblock = new char[size];
    file.seekg(0, std::ios::beg);
    file.read(memblock, size);
    file.close();

    if(kind == MEM_TOPOLOGY) {
	mem_topology* mem = (mem_topology*)memblock;
	if(index > (mem->m_count -1)) {
	    delete[] memblock;
	    return EINVAL;
	}
	memcpy(section_info, &mem->m_mem_data[index], sizeof(mem_data));
	*section_size = sizeof(mem_data);
    } else if (kind == CONNECTIVITY) {
	connectivity* con = (connectivity*)memblock;
	if(index > (con->m_count -1)) {
	    delete[] memblock;
	    return EINVAL;
	}
	memcpy(section_info, &con->m_connection[index], sizeof(connection));
	*section_size = sizeof(connection);

    } else if(kind == IP_LAYOUT) {
	ip_layout* ip = (ip_layout*)memblock;
	if(index > (ip->m_count -1)) {
	    delete[] memblock;
	    return EINVAL;
	}
	memcpy(section_info, &ip->m_ip_data[index], sizeof(ip_data));
	*section_size = sizeof(ip_data);
    }

    delete[] memblock;
    return 0;
}

/*
 * xclSysfsGetUsageInfo()
 */
void xocl::XOCLShim::xclSysfsGetUsageInfo(drm_xocl_usage_stat& stat)
{
    auto dmaStatStrs = xclSysfsGetStrings(false, "mm_dma", "channel_stat_raw");
    auto mmStatStrs = xclSysfsGetStrings(false, "", "memstat_raw");

    if (!dmaStatStrs.empty()) {
        stat.dma_channel_count = dmaStatStrs.size();
        for (unsigned i = 0;
            i < std::min(dmaStatStrs.size(), ARRAY_SIZE(stat.c2h));
            i++) {
            std::stringstream ss(dmaStatStrs[i]);
            ss >> stat.c2h[i] >> stat.h2c[i];
        }
    }

    if (!mmStatStrs.empty()) {
        stat.mm_channel_count = mmStatStrs.size();
        for (unsigned i = 0;
            i < std::min(mmStatStrs.size(), ARRAY_SIZE(stat.mm));
            i++) {
            std::stringstream ss(mmStatStrs[i]);
            ss >> stat.mm[i].memory_usage >> stat.mm[i].bo_count;
        }
    }
}

/*
 * xclGetUsageInfo()
 */
int xocl::XOCLShim::xclGetUsageInfo(xclDeviceUsage *info)
{
    drm_xocl_usage_stat stat = { 0 };

    // Obtain usage info from sysfs. Will fall back to IOCTL, if not supported.
    if (xclSysfsGetInt(true, "", "version") > 0) {
        xclSysfsGetUsageInfo(stat);
    } else {
        int result = ioctl(mUserHandle, DRM_IOCTL_XOCL_USAGE_STAT, &stat);
        if (result) {
            return result;
        }
    }
    std::memset(info, 0, sizeof(xclDeviceUsage));
    std::memcpy(info->h2c, stat.h2c, sizeof(size_t) * 8);
    std::memcpy(info->c2h, stat.c2h, sizeof(size_t) * 8);
    for (int i = 0; i < 8; i++) {
        info->ddrMemUsed[i] = stat.mm[i].memory_usage;
        info->ddrBOAllocated[i] = stat.mm[i].bo_count;
    }
    info->dma_channel_cnt = stat.dma_channel_count;
    info->mm_channel_cnt = stat.mm_channel_count;
    return 0;
}

/*
 * isGood()
 */
bool xocl::XOCLShim::isGood() const {
    return (mUserHandle >= 0) && (mMgtHandle >= 0);
}

/*
 * handleCheck()
 *
 * Returns pointer to valid handle on success, 0 on failure.
 */
xocl::XOCLShim *xocl::XOCLShim::handleCheck(void *handle)
{
    if (!handle) {
        return 0;
    }
    if (!((XOCLShim *) handle)->isGood()) {
        return 0;
    }
    return (XOCLShim *) handle;
}

/*
 * xclAllocDeviceBuffer()
 */
uint64_t xocl::XOCLShim::xclAllocDeviceBuffer(size_t size)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << size << std::endl;
    }

    uint64_t result = mNullAddr;
    unsigned boHandle = xclAllocBO(size, XCL_BO_DEVICE_RAM, 0x0);
    if (boHandle == mNullBO) {
        return result;
    }

    drm_xocl_info_bo boInfo = {boHandle, 0, 0, 0};
    if (ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &boInfo)) {
        return result;
    }

    void *hbuf = xclMapBO(boHandle, true);
    if (hbuf == MAP_FAILED) {
        xclFreeBO(boHandle);
        return mNullAddr;
    }
    mLegacyAddressTable.insert(boInfo.paddr, size, std::make_pair(boHandle, (char *)hbuf));
    return boInfo.paddr;
}

/*
 * xclAllocDeviceBuffer2()
 */
uint64_t xocl::XOCLShim::xclAllocDeviceBuffer2(size_t size, xclMemoryDomains domain, unsigned flags)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << size << ", "
                   << domain << ", " << flags << std::endl;
    }

    uint64_t result = mNullAddr;
    if (domain != XCL_MEM_DEVICE_RAM) {
        return result;
    }

    uint64_t ddr = 1;
    ddr <<= flags;
    unsigned boHandle = xclAllocBO(size, XCL_BO_DEVICE_RAM, ddr);
    if (boHandle == mNullBO) {
        return result;
    }

    drm_xocl_info_bo boInfo = {boHandle, 0, 0, 0};
    if (ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &boInfo)) {
        return result;
    }

    void *hbuf = xclMapBO(boHandle, true);
    if (hbuf == MAP_FAILED) {
        xclFreeBO(boHandle);
        return mNullAddr;
    }
    mLegacyAddressTable.insert(boInfo.paddr, size, std::make_pair(boHandle, (char *)hbuf));
    return boInfo.paddr;
}

/*
 * xclFreeDeviceBuffer()
 */
void xocl::XOCLShim::xclFreeDeviceBuffer(uint64_t buf)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << buf << std::endl;
    }

    std::pair<unsigned, char *> bo = mLegacyAddressTable.erase(buf);
    drm_xocl_info_bo boInfo = {bo.first, 0, 0, 0};
    if (!ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &boInfo)) {
        munmap(bo.second, boInfo.size);
    }
    xclFreeBO(bo.first);
}

/*
 * xclCopyBufferHost2Device()
 */
size_t xocl::XOCLShim::xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
                   << src << ", " << size << ", " << seek << std::endl;
    }

    std::pair<unsigned, char *> bo = mLegacyAddressTable.find(dest);
    std::memcpy(bo.second + seek, src, size);
    int result = xclSyncBO(bo.first, XCL_BO_SYNC_BO_TO_DEVICE, size, seek);
    if (result) {
        return result;
    }
    return size;
}

/*
 * xclCopyBufferDevice2Host()
 */
size_t xocl::XOCLShim::xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
                << src << ", " << size << ", " << skip << std::endl;
    }

    std::pair<unsigned, char *> bo = mLegacyAddressTable.find(src);
    int result = xclSyncBO(bo.first, XCL_BO_SYNC_BO_FROM_DEVICE, size, skip);
    if (result) {
        return result;
    }
    std::memcpy(dest, bo.second + skip, size);
    return size;
}

/*
 * xclUnmgdPwrite()
 */
ssize_t xocl::XOCLShim::xclUnmgdPwrite(unsigned flags, const void *buf, size_t count, uint64_t offset)
{
    if (flags) {
        return -EINVAL;
    }
    drm_xocl_pwrite_unmgd unmgd = {0, 0, offset, count, reinterpret_cast<uint64_t>(buf)};
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_PWRITE_UNMGD, &unmgd);
}

/*
 * xclUnmgdPread()
 */
ssize_t xocl::XOCLShim::xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset)
{
    if (flags) {
        return -EINVAL;
    }
    drm_xocl_pread_unmgd unmgd = {0, 0, offset, count, reinterpret_cast<uint64_t>(buf)};
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_PREAD_UNMGD, &unmgd);
}

/*
 * xclExecBuf()
 */
int xocl::XOCLShim::xclExecBuf(unsigned int cmdBO)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << cmdBO << std::endl;
    }
    drm_xocl_execbuf exec = {0, cmdBO, 0,0,0,0,0,0,0,0};
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_EXECBUF, &exec);
}

/*
 * xclExecBuf()
 */
int xocl::XOCLShim::xclExecBuf(unsigned int cmdBO, size_t num_bo_in_wait_list, unsigned int *bo_wait_list)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", "
                   << cmdBO << ", " << num_bo_in_wait_list << ", " << bo_wait_list << std::endl;
    }
    unsigned int bwl[8] = {0};
    std::memcpy(bwl,bo_wait_list,num_bo_in_wait_list*sizeof(unsigned int));
    drm_xocl_execbuf exec = {0, cmdBO, bwl[0],bwl[1],bwl[2],bwl[3],bwl[4],bwl[5],bwl[6],bwl[7]};
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_EXECBUF, &exec);
}

/*
 * xclRegisterEventNotify()
 */
int xocl::XOCLShim::xclRegisterEventNotify(unsigned int userInterrupt, int fd)
{
    drm_xocl_user_intr userIntr = {0, fd, (int)userInterrupt};
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_USER_INTR, &userIntr);
}

/*
 * xclExecWait()
 */
int xocl::XOCLShim::xclExecWait(int timeoutMilliSec)
{
    std::vector<pollfd> uifdVector;
    pollfd info = {mUserHandle, POLLIN, 0};
    uifdVector.push_back(info);
    return poll(&uifdVector[0], uifdVector.size(), timeoutMilliSec);
}

/*
 * xclOpenContext
 */
int xocl::XOCLShim::xclOpenContext(xclContextProperties *context) const
{
    return ioctl(mUserHandle, DRM_IOCTL_XOCL_CTX, &context);
}

/*
 * xclBootFPGA()
 */
int xocl::XOCLShim::xclBootFPGA()
{
    return ioctl( mMgtHandle, XCLMGMT_IOCREBOOT );
}

/*
 * xclCreateWriteQueue()
 */
int xocl::XOCLShim::xclCreateWriteQueue(xclQueueContext *q_ctx, uint64_t *q_hdl)
{
    struct xocl_qdma_ioc_create_queue q_info;
    int	rc;

    memset(&q_info, 0, sizeof (q_info));
    q_info.write = 1;

    rc = ioctl(mStreamHandle, XOCL_QDMA_IOC_CREATE_QUEUE, &q_info);
    if (rc) {
        std::cout << __func__ << " ERROR: Create Write Queue IOCTL failed" << std::endl;
    } else
        *q_hdl = q_info.handle;

    return rc;
}

/*
 * xclCreateReadQueue()
 */
int xocl::XOCLShim::xclCreateReadQueue(xclQueueContext *q_ctx, uint64_t *q_hdl)
{
    struct xocl_qdma_ioc_create_queue q_info;
    int	rc;

    memset(&q_info, 0, sizeof (q_info));

    rc = ioctl(mStreamHandle, XOCL_QDMA_IOC_CREATE_QUEUE, &q_info);
    if (rc) {
        std::cout << __func__ << " ERROR: Create Read Queue IOCTL failed" << std::endl;
    } else
        *q_hdl = q_info.handle;

    return rc;
}

/*
 * xclDestroyQueue()
 */
int xocl::XOCLShim::xclDestroyQueue(uint64_t q_hdl)
{
    int rc;

    rc = close((int)q_hdl);
    if (rc)
        std::cout << __func__ << " ERROR: Destroy Queue failed" << std::endl;

    return rc;
}

/*
 * xclAllocQDMABuf()
 */
void *xocl::XOCLShim::xclAllocQDMABuf(size_t size, uint64_t *buf_hdl)
{
    struct xocl_qdma_ioc_alloc_buf req;
    void *buf;
    int rc;

    memset(&req, 0, sizeof (req));
    req.size = size;

    rc = ioctl(mStreamHandle, XOCL_QDMA_IOC_ALLOC_BUFFER, &req);
    if (rc) {
        std::cout << __func__ << " ERROR: Alloc buffer IOCTL failed" << std::endl;
	return NULL;
    }

    buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, req.buf_fd, 0);
    if (!buf) {
        std::cout << __func__ << " ERROR: Map buffer failed" << std::endl;
        close(req.buf_fd);
    } else {
        *buf_hdl = req.buf_fd;
    }

    return buf;
}

/*
 * xclFreeQDMABuf()
 */
int xocl::XOCLShim::xclFreeQDMABuf(uint64_t buf_hdl)
{
    int rc;

    rc = close((int)buf_hdl);
    if (rc)
        std::cout << __func__ << " ERROR: Destroy Queue failed" << std::endl;

    return rc;
}

// Helper to find subdevice directory name
// Assumption: all subdevice's sysfs directory name starts with subdevice name!!
static std::string getSubdevDirName(const std::string& dir,
    const std::string& subDevName)
{
    struct dirent *entry;
    DIR *dp;
    std::string nm;

    dp = opendir(dir.c_str());
    if (dp) {
        while ((entry = readdir(dp))) {
            if(strncmp(entry->d_name,
                subDevName.c_str(), subDevName.size()) == 0) {
                nm += entry->d_name;
                break;
            }
        }
        closedir(dp);
    }

    return nm;
}

/*
 * xclSysfsOpen()
 * Obtain ifstream of a device sysfs entry
 */
std::ifstream xocl::XOCLShim::xclSysfsOpen(bool mgmt,
    const std::string subDevName, const std::string entry)
{
    xcldev::pci_device_scanner::device_info& dev =
        xcldev::pci_device_scanner::device_list[mBoardNumber];
    std::string path = "/sys/bus/pci/devices/" +
        (mgmt ? dev.mgmt_name : dev.user_name) + "/";

    if (!subDevName.empty()) {
        path += getSubdevDirName(path, subDevName);
        path += "/";
    }

    return std::ifstream(path + entry);
}

/*
 * xclSysfsGetInts()
 * Obtain an array of integers from a device sysfs entry
 * All integers in sysfs entry are separated by '\n'.
 */
std::vector<unsigned long long> xocl::XOCLShim::xclSysfsGetInts(bool mgmt,
    const std::string subDevName, const std::string entry)
{
    std::vector<unsigned long long> iv;
    std::ifstream ifs = xclSysfsOpen(mgmt, subDevName, entry);
    std::string line;

    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            iv.push_back(std::stoull(line, nullptr, 0));
        }
    }

    return iv;
}

/*
 * xclSysfsGetStrings()
 * Obtain an array of strings from a device sysfs entry
 * All strings in sysfs entry are separated by '\n'.
 */
std::vector<std::string> xocl::XOCLShim::xclSysfsGetStrings(bool mgmt,
    const std::string subDevName, const std::string entry)
{
    std::vector<std::string> sv;
    std::ifstream ifs = xclSysfsOpen(mgmt, subDevName, entry);
    std::string line;

    if (ifs.is_open()) {
        while (std::getline(ifs, line)) {
            sv.push_back(line);
        }
    }

    return sv;
}

/*
 * xclSysfsGetString()
 * Obtain content string from a device sysfs entry
 */
std::string xocl::XOCLShim::xclSysfsGetString(bool mgmt,
    const std::string subDevName, const std::string entry)
{
    std::string s;
    auto v = xclSysfsGetStrings(mgmt, subDevName, entry);

    if (!v.empty()) {
        s = v[0];
    } else {
        std::cerr << __func__ << " ERROR: Failed to read string from ";
        std::cerr << (mgmt ? "mgmt" : "user") << " sysfs ";
        std::cerr << (!subDevName.empty() ? subDevName + "/" : "") + entry;
        std::cerr << " entry" << std::endl;
    }

    return s;
}

/*
 * xclSysfsGetInt()
 * Obtain a single integer from a device sysfs entry
 */
unsigned long long xocl::XOCLShim::xclSysfsGetInt(bool mgmt,
    const std::string subDevName, const std::string entry)
{
    unsigned long long l = 0;
    auto v = xclSysfsGetInts(mgmt, subDevName, entry);

    if (!v.empty()) {
        l = v[0];
    } else {
        std::cerr << __func__ << " ERROR: Failed to read integer from ";
        std::cerr << (mgmt ? "mgmt" : "user") << " sysfs ";
        std::cerr << (!subDevName.empty() ? subDevName + "/" : "") + entry;
        std::cerr << " entry" << std::endl;
    }

    return l;
}

/*
 * xclWriteQueue()
 */
ssize_t xocl::XOCLShim::xclWriteQueue(uint64_t q_hdl, xclQueueRequest *wr)
{
    ssize_t rc = 0;
    const void *buf;

    for (unsigned i = 0; i < wr->buf_num; i++) {
        buf = (const void *)wr->bufs[i].va;
        rc = write((int)q_hdl, buf, wr->bufs[i].len);
    }
    return rc;
}

/*
 * xclReadQueue()
 */
ssize_t xocl::XOCLShim::xclReadQueue(uint64_t q_hdl, xclQueueRequest *wr)
{
    ssize_t rc = 0;
    void *buf;

    for (unsigned i = 0; i < wr->buf_num; i++) {
        buf = (void *)wr->bufs[i].va;
        rc = read((int)q_hdl, buf, wr->bufs[i].len);
    }
    return rc;

}

/*******************************/
/* GLOBAL DECLARATIONS *********/
/*******************************/
SHIM_UNUSED
static int getUserSlotNo(int fd)
{
    drm_xocl_info obj;
    std::memset(&obj, 0, sizeof(drm_xocl_info));
    int ret = ioctl(fd, DRM_IOCTL_XOCL_INFO, &obj);
    if (ret) {
        return ret;
    }
    return obj.pci_slot;
}

static int getMgmtSlotNo(int handle)
{
    xclmgmt_ioc_info obj;
    std::memset(&obj, 0, sizeof(xclmgmt_ioc_info));
    int ret = ioctl(handle, XCLMGMT_IOCINFO, &obj);
    if (ret) {
        return ret;
    }
    return obj.pci_slot;
}

SHIM_UNUSED
static int findMgmtDeviceID(int user_slot)
{
    int mgmt_fd = -1;
    int mgmt_slot = -1;

    for(int i = 0; i < 16; ++i) {
        std::string mgmtFile = "/dev/xclmgmt"+ std::to_string(i);
        mgmt_fd = open(mgmtFile.c_str(), O_RDWR | O_SYNC);
        if(mgmt_fd < 0) {
            std::cout << "Could not open " << mgmtFile << std::endl;
            continue;
        }

        mgmt_slot = getMgmtSlotNo(mgmt_fd);
        if(mgmt_slot == user_slot) {
            return i;
        }
    }

    return -1;
}


unsigned xclProbe()
{
    std::lock_guard<std::mutex> lock(xocl::deviceListMutex);

    if(xcldev::pci_device_scanner::device_list.size()) {
        return xcldev::pci_device_scanner::device_list.size();
    }

    xcldev::pci_device_scanner devScanner;
    devScanner.scan(false);
    return xcldev::pci_device_scanner::device_list.size();
}


xclDeviceHandle xclOpen(unsigned deviceIndex, const char *logFileName, xclVerbosityLevel level)
{
    if(xcldev::pci_device_scanner::device_list.size() <= deviceIndex) {
        printf("Cannot find index %d \n", deviceIndex);
        return nullptr;
    }

    xocl::XOCLShim *handle = new xocl::XOCLShim(deviceIndex, logFileName, level);
    if (!xocl::XOCLShim::handleCheck(handle)) {
        delete handle;
        handle = nullptr;
    }
    return static_cast<xclDeviceHandle>(handle);
}

void xclClose(xclDeviceHandle handle)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    if (drv) {
        delete drv;
    }
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclLoadXclBin(buffer) : -ENODEV;
}

size_t xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclWrite(space, offset, hostBuf, size) : -ENODEV;
}

size_t xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
    //  std::cout << "xclRead called" << std::endl;
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclRead(space, offset, hostBuf, size) : -ENODEV;
}

int xclGetErrorStatus(xclDeviceHandle handle, xclErrorStatus *info)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    if (!drv) {
        return -1;
    }
    return drv->xclGetErrorStatus(info);
}

int xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclGetDeviceInfo2(info) : -ENODEV;
}

unsigned int xclVersion ()
{
    return 2;
}

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, xclBOKind domain, uint64_t flags)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclAllocBO(size, domain, flags) : -ENODEV;
}

unsigned int xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, uint64_t flags)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclAllocUserPtrBO(userptr, size, flags) : -ENODEV;
}

void xclFreeBO(xclDeviceHandle handle, unsigned int boHandle) {
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    if (!drv) {
        return;
    }
    drv->xclFreeBO(boHandle);
}

size_t xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src, size_t size, size_t seek)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclWriteBO(boHandle, src, size, seek) : -ENODEV;
}

size_t xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst, size_t size, size_t skip)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclReadBO(boHandle, dst, size, skip) : -ENODEV;
}

void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclMapBO(boHandle, write) : nullptr;
}

int xclSyncBO(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclSyncBO(boHandle, dir, size, offset) : -ENODEV;
}

int xclCopyBO(xclDeviceHandle handle, unsigned int dst_boHandle,
            unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ?
      drv->xclCopyBO(dst_boHandle, src_boHandle, size, dst_offset, src_offset) : -ENODEV;
}

int xclReClock2(xclDeviceHandle handle, unsigned short region, const unsigned short *targetFreqMHz)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclReClock2(region, targetFreqMHz) : -ENODEV;
}

int xclLockDevice(xclDeviceHandle handle)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    if (!drv)
        return -ENODEV;
    return drv->xclLockDevice() ? 0 : 1;
}

int xclUnlockDevice(xclDeviceHandle handle)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    if (!drv)
        return -ENODEV;
    return drv->xclUnlockDevice() ? 0 : 1;
}

int xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->resetDevice(kind) : -ENODEV;
}

/*
 * xclBootFPGA
 *
 * Sequence:
 *   1) call boot ioctl
 *   2) close the device, unload the driver
 *   3) remove and scan
 *   4) rescan pci devices
 *   5) reload the driver (done by the calling function xcldev::boot())
 *
 * Return 0 on success, negative value on failure.
 */
int xclBootFPGA(xclDeviceHandle handle)
{
    int retVal = -1;

    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    if( !drv )
        return -ENODEV;

    retVal = drv->xclBootFPGA(); // boot ioctl

    if( retVal == 0 )
    {
        xclClose(handle); // close the device, unload the driver
        retVal = xclRemoveAndScanFPGA(); // remove and scan
    }

    if( retVal == 0 )
    {
        xcldev::pci_device_scanner devScanner;
        devScanner.scan( true ); // rescan pci devices
    }

    return retVal;
}

int xclRemoveAndScanFPGA( void )
{
    const std::string devPath =    "/devices/";
    const std::string removePath = "/remove";
    const std::string pciPath =    "/sys/bus/pci";
    const std::string rescanPath = "/rescan";
    const char *input = "1\n";

    // remove devices "echo 1 > /sys/bus/pci/devices/<deviceHandle>/remove"
    for (unsigned int i = 0; i < xcldev::pci_device_scanner::device_list.size(); i++)
    {
        std::string dev_name_pf_user = pciPath + devPath + xcldev::pci_device_scanner::device_list[i].user_name + removePath;
        std::string dev_name_pf_mgmt = pciPath + devPath + xcldev::pci_device_scanner::device_list[i].mgmt_name + removePath;

        std::ofstream userFile( dev_name_pf_user );
        if( !userFile.is_open() ) {
            perror( dev_name_pf_user.c_str() );
            return errno;
        }
        userFile << input;

        std::ofstream mgmtFile( dev_name_pf_mgmt );
        if( !mgmtFile.is_open() ) {
            perror( dev_name_pf_mgmt.c_str() );
            return errno;
        }
        mgmtFile << input;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    // initiate rescan "echo 1 > /sys/bus/pci/rescan"
    std::ofstream rescanFile( pciPath + rescanPath );
    if( !rescanFile.is_open() ) {
        perror( std::string( pciPath + rescanPath ).c_str() );
        return errno;
    }
    rescanFile << input;

    return 0;
}

// Support for XCLHAL1 legacy API's

uint64_t xclAllocDeviceBuffer(xclDeviceHandle handle, size_t size)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclAllocDeviceBuffer(size) : xocl::mNullAddr;
}

uint64_t xclAllocDeviceBuffer2(xclDeviceHandle handle, size_t size, xclMemoryDomains domain, unsigned flags)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclAllocDeviceBuffer2(size, domain, flags) : xocl::mNullAddr;
}

void xclFreeDeviceBuffer(xclDeviceHandle handle, uint64_t buf)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    if (!drv) {
        return;
    }
    drv->xclFreeDeviceBuffer(buf);
}

size_t xclCopyBufferHost2Device(xclDeviceHandle handle, uint64_t dest, const void *src, size_t size, size_t seek)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclCopyBufferHost2Device(dest, src, size, seek) : -ENODEV;
}


size_t xclCopyBufferDevice2Host(xclDeviceHandle handle, void *dest, uint64_t src, size_t size, size_t skip)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclCopyBufferDevice2Host(dest, src, size, skip) : -ENODEV;
}

int xclExportBO(xclDeviceHandle handle, unsigned int boHandle)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclExportBO(boHandle) : -ENODEV;
}

unsigned int xclImportBO(xclDeviceHandle handle, int fd, unsigned flags)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    if (!drv) {
        std::cout << __func__ << ", " << std::this_thread::get_id() << ", handle & XOCL Device are bad" << std::endl;
    }
    return drv ? drv->xclImportBO(fd, flags) : -ENODEV;
}

ssize_t xclUnmgdPwrite(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclUnmgdPwrite(flags, buf, count, offset) : -ENODEV;
}

ssize_t xclUnmgdPread(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclUnmgdPread(flags, buf, count, offset) : -ENODEV;
}

int xclGetBOProperties(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties *properties)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclGetBOProperties(boHandle, properties) : -ENODEV;
}

int xclGetUsageInfo(xclDeviceHandle handle, xclDeviceUsage *info)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclGetUsageInfo(info) : -ENODEV;
}

int xclGetSectionInfo(xclDeviceHandle handle, void* section_info, size_t * section_size,
	enum axlf_section_kind kind, int index)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclGetSectionInfo(section_info, section_size, kind, index) : -ENODEV;
}

int xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclExecBuf(cmdBO) : -ENODEV;
}

int xclExecBufWithWaitList(xclDeviceHandle handle, unsigned int cmdBO, size_t num_bo_in_wait_list, unsigned int *bo_wait_list)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclExecBuf(cmdBO,num_bo_in_wait_list,bo_wait_list) : -ENODEV;
}

int xclRegisterEventNotify(xclDeviceHandle handle, unsigned int userInterrupt, int fd)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclRegisterEventNotify(userInterrupt, fd) : -ENODEV;
}

int xclXbsak(int argc, char *argv[])
{
    return xcldev::xclXbsak(argc, argv);
}

int xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclExecWait(timeoutMilliSec) : -ENODEV;
}

int xclOpenContext(xclDeviceHandle handle, xclContextProperties *context)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclOpenContext(context) : -ENODEV;
}

// QDMA streaming APIs
int xclCreateWriteQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclCreateWriteQueue(q_ctx, q_hdl) : -ENODEV;
}

int xclCreateReadQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclCreateReadQueue(q_ctx, q_hdl) : -ENODEV;
}

int xclDestroyQueue(xclDeviceHandle handle, uint64_t q_hdl)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclDestroyQueue(q_hdl) : -ENODEV;
}

void *xclAllocQDMABuf(xclDeviceHandle handle, size_t size, uint64_t *buf_hdl)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclAllocQDMABuf(size, buf_hdl) : NULL;
}

int xclFreeQDMABuf(xclDeviceHandle handle, uint64_t buf_hdl)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclFreeQDMABuf(buf_hdl) : -ENODEV;
}

ssize_t xclWriteQueue(xclDeviceHandle handle, uint64_t q_hdl, xclQueueRequest *wr)
{
	xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
	return drv ? drv->xclWriteQueue(q_hdl, wr) : -ENODEV;
}

ssize_t xclReadQueue(xclDeviceHandle handle, uint64_t q_hdl, xclQueueRequest *wr)
{
	xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
	return drv ? drv->xclReadQueue(q_hdl, wr) : -ENODEV;
}
