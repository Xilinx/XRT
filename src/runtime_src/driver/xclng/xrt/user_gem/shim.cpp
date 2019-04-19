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
#include <sstream>
#include <vector>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/file.h>
#include <poll.h>
#include <sys/syscall.h>
#include <linux/aio_abi.h>
#include "driver/include/xclbin.h"
#include "scan.h"
#include <ert.h>
#include "driver/common/message.h"
#include "driver/common/scheduler.h"
#include <cstdio>
#include <stdarg.h>

#ifdef NDEBUG
# undef NDEBUG
# include<cassert>
#endif

#if defined(__GNUC__)
#define SHIM_UNUSED __attribute__((unused))
#endif

#define GB(x)           ((size_t) (x) << 30)
#define USER_PCIID(x)   (((x)->bus << 8) | ((x)->dev << 3) | (x)->func)
#define ARRAY_SIZE(x)   (sizeof (x) / sizeof (x[0]))

#define SHIM_QDMA_AIO_EVT_MAX   1024 * 64

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

inline int io_setup(unsigned nr, aio_context_t *ctxp)
{
        return syscall(__NR_io_setup, nr, ctxp);
}

inline int io_destroy(aio_context_t ctx)
{
        return syscall(__NR_io_destroy, ctx);
}

inline int io_submit(aio_context_t ctx, long nr,  struct iocb **iocbpp)
{
        return syscall(__NR_io_submit, ctx, nr, iocbpp);
}

inline int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
                struct io_event *events, struct timespec *timeout)
{
        return syscall(__NR_io_getevents, ctx, min_nr, max_nr, events, timeout);
}

/*
 * XOCLShim()
 */
xocl::XOCLShim::XOCLShim(unsigned index,
                         const char *logfileName,
                         xclVerbosityLevel verbosity) : mVerbosity(verbosity),
                                                        mUserHandle(-1),
                                                        mMgtHandle(-1),
                                                        mUserMap(nullptr),
                                                        mBoardNumber(index),
                                                        mMgtMap(nullptr),
                                                        mLocked(false),
                                                        mOffsets{0x0, 0x0, OCL_CTLR_BASE, 0x0, 0x0},
                                                        mMemoryProfilingNumberSlots(0),
                                                        mAccelProfilingNumberSlots(0),
                                                        mStallProfilingNumberSlots(0),
                                                        mStreamProfilingNumberSlots(0)
{
    mLogfileName = nullptr;
    init(index, logfileName, verbosity);
}

int xocl::XOCLShim::dev_init()
{
    auto dev = pcidev::get_dev(mBoardNumber);

   // pcidev::dump_dev_list();
    if(dev->user) {
        // Should only touch user pf when device is ready.
        const std::string devName = "/dev/dri/renderD" +
            std::to_string(dev->user->instance);
        mUserHandle = open(devName.c_str(), O_RDWR);
        if(mUserHandle > 0) {
            drm_version version;
            const std::unique_ptr<char[]> name(new char[128]);
            const std::unique_ptr<char[]> desc(new char[512]);
            const std::unique_ptr<char[]> date(new char[128]);
            std::memset(&version, 0, sizeof(version));
            version.name = name.get();
            version.name_len = 128;
            version.desc = desc.get();
            version.desc_len = 512;
            version.date = date.get();
            version.date_len = 128;

            int result = ioctl(mUserHandle, DRM_IOCTL_VERSION, &version);
            if (result)
                return -errno;

            // Lets map 4M
            mUserMap = (char *)mmap(0, dev->user->user_bar_size,
                PROT_READ | PROT_WRITE, MAP_SHARED, mUserHandle, 0);
            if (mUserMap == MAP_FAILED) {
                std::cout << "Map failed: " << devName << std::endl;
                close(mUserHandle);
                mUserHandle = -1;
                mUserMap = nullptr;
            }
        } else {
            std::cout << "Cannot open: " << devName << std::endl;
        }

        std::string streamFile = "/dev/str_dma.u"+
            std::to_string(USER_PCIID(dev->user));
        mStreamHandle = open(streamFile.c_str(), O_RDWR | O_SYNC);
    }

    if (dev->mgmt) {
        std::string mgmtFile = "/dev/xclmgmt"+ std::to_string(dev->mgmt->instance);
        mMgtHandle = open(mgmtFile.c_str(), O_RDWR | O_SYNC);
        if(mMgtHandle < 0) {
            std::cout << "Could not open " << mgmtFile << std::endl;
            return -errno;
        }
        mMgtMap = (char *)mmap(0, dev->mgmt->user_bar_size, PROT_READ | PROT_WRITE,
            MAP_SHARED, mMgtHandle, 0);
        if (mMgtMap == MAP_FAILED) // Not an error if user is not privileged
            mMgtMap = nullptr;
    }

    if (xclGetDeviceInfo2(&mDeviceInfo)) {
        if(mMgtHandle > 0) {
            close(mMgtHandle);
            mMgtHandle = -1;
        }
    }

    memset(&mAioContext, 0, sizeof(mAioContext));
    if (io_setup(SHIM_QDMA_AIO_EVT_MAX, &mAioContext) != 0) {
        mAioEnabled = false;
    } else {
        mAioEnabled = true;
    }

    return 0;
}

void xocl::XOCLShim::dev_fini()
{
    auto dev = pcidev::get_dev(mBoardNumber);

    if (mUserMap != nullptr) {
        munmap(mUserMap, dev->user->user_bar_size);
        mUserMap = nullptr;
    }

    if (mMgtMap != nullptr) {
        munmap(mMgtMap, dev->mgmt->user_bar_size);
        mMgtMap = nullptr;
    }

    if (mUserHandle > 0) {
        close(mUserHandle);
        mUserHandle = 0;
    }

    if (mMgtHandle > 0) {
        close(mMgtHandle);
        mMgtHandle = 0;
    }

    if (mStreamHandle > 0) {
        close(mStreamHandle);
        mStreamHandle = 0;
    }

    if (mAioEnabled) {
        io_destroy(mAioContext);
            mAioEnabled = false;
    }
}

/*
 * init()
 */
void xocl::XOCLShim::init(unsigned index, const char *logfileName,
    xclVerbosityLevel verbosity)
{
    if( logfileName != nullptr ) {
        mLogStream.open(logfileName);
        mLogStream << "FUNCTION, THREAD ID, ARG..." << std::endl;
        mLogStream << __func__ << ", " << std::this_thread::get_id()
            << std::endl;
    }

    dev_init();

    auto dev = pcidev::get_dev(index);

   // pcidev::dump_dev_list();
    if(dev->user) {
        // Profiling - defaults
        // Class-level defaults: mIsDebugIpLayoutRead = mIsDeviceProfiling = false
        mDevUserName = dev->user->sysfs_name;
        mMemoryProfilingNumberSlots = 0;
        mPerfMonFifoCtrlBaseAddress = 0x00;
        mPerfMonFifoReadBaseAddress = 0x00;
    }
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

    dev_fini();
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
 * xclLogMsg()
 */
int xocl::XOCLShim::xclLogMsg(xclDeviceHandle handle, xclLogMsgLevel level, const char* tag, const char* format, va_list args)
{
    va_list args_bak;
    // vsnprintf will mutate va_list so back it up
    va_copy(args_bak, args);
    int len = std::vsnprintf(nullptr, 0, format, args_bak);
    va_end(args_bak);

    if (len < 0) {
        //illegal arguments
        std::string err_str = "ERROR: Illegal arguments in log format string. ";
        err_str.append(std::string(format));
        xrt_core::message::send((xrt_core::message::severity_level)level, tag, err_str.c_str());
        return len;
    }
    ++len; //To include null terminator

    std::vector<char> buf(len);
    len = std::vsnprintf(buf.data(), len, format, args);

    if (len < 0) {
        //error processing arguments
        std::string err_str = "ERROR: When processing arguments in log format string. ";
        err_str.append(std::string(format));
        xrt_core::message::send((xrt_core::message::severity_level)level, tag, err_str.c_str());
        return len;
    }
    xrt_core::message::send((xrt_core::message::severity_level)level, tag, buf.data());

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
unsigned int xocl::XOCLShim::xclAllocBO(size_t size, xclBOKind domain, unsigned flags)
{
    //std::cout << "alloc bo with combined flags " << std::hex << flags ;
    unsigned flag = flags & 0xFFFFFFLL;
    unsigned type = flags & 0xFF000000LL ;
    //std::cout << "git: alloc bo: with combined flags " << std::hex << flags << " split, flag: " << flag << "type: " << type << std::endl;

    drm_xocl_create_bo info = {size, mNullBO, flag, type};
    int result = ioctl(mUserHandle, DRM_IOCTL_XOCL_CREATE_BO, &info);
    return result ? mNullBO : info.handle;
}

/*
 * xclAllocUserPtrBO()
 */
unsigned int xocl::XOCLShim::xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags)
{
    //std::cout << "User alloc bo with combined flags " << flags ;
    unsigned flag = flags & 0xFFFFFFLL;
    unsigned type = flags & 0xFF000000LL ;
    //std::cout << "git: user alloc bo: with combined flags " << std::hex << flags << " split, flag: " << flag << "type: " << type << std::endl;

    //std::cout << " split flags "  << std::hex << flag << " " << type << std::dec << std::endl;
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
    int ret;
    drm_xocl_pwrite_bo pwriteInfo = { boHandle, 0, seek, size, reinterpret_cast<uint64_t>(src) };
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfo);
    return ret ? -errno : ret;
}

/*
 * xclReadBO()
 */
int xocl::XOCLShim::xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip)
{
    int ret;
    drm_xocl_pread_bo preadInfo = { boHandle, 0, skip, size, reinterpret_cast<uint64_t>(dst) };
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo);
    return ret ? -errno : ret;
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
    int ret;
    drm_xocl_sync_bo_dir drm_dir = (dir == XCL_BO_SYNC_BO_TO_DEVICE) ?
            DRM_XOCL_SYNC_BO_TO_DEVICE :
            DRM_XOCL_SYNC_BO_FROM_DEVICE;
    drm_xocl_sync_bo syncInfo = {boHandle, 0, size, offset, drm_dir};
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
    return ret ? -errno : ret;
}

/*
 * xclCopyBO() - TO BE REMOVED
 */
int xocl::XOCLShim::xclCopyBO(unsigned int dst_boHandle,
    unsigned int src_boHandle, size_t size, size_t dst_offset,
    size_t src_offset)
{
    int ret;
    unsigned execHandle = xclAllocBO(sizeof (ert_start_copybo_cmd),
        xclBOKind(0), (1<<31));
    struct ert_start_copybo_cmd *execData =
        reinterpret_cast<struct ert_start_copybo_cmd *>(
        xclMapBO(execHandle, true));

    ert_fill_copybo_cmd(execData, src_boHandle, dst_boHandle,
        src_offset, dst_offset, size);

    ret = xclExecBuf(execHandle);
    if (ret == 0)
        while (xclExecWait(1000) == 0);

    (void) munmap(execData, sizeof (ert_start_copybo_cmd));
    xclFreeBO(execHandle);

    return ret;
}

/*
 * xclSysfsGetErrorStatus()
 */
void xocl::XOCLShim::xclSysfsGetErrorStatus(xclErrorStatus& stat)
{
    std::string errmsg;
    unsigned int status;
    unsigned int level;
    unsigned long time;
    auto dev = pcidev::get_dev(mBoardNumber);
    if(dev->mgmt == NULL)
        return;
    dev->mgmt->sysfs_get("firewall", "detected_status", errmsg, status);
    dev->mgmt->sysfs_get("firewall", "detected_level", errmsg, level);
    dev->mgmt->sysfs_get("firewall", "detected_time", errmsg, time);

    stat.mNumFirewalls = XCL_FW_MAX_LEVEL;
    stat.mFirewallLevel = level;
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
    xclSysfsGetErrorStatus(*info);
#endif  // AXI Firewall
    return 0;
}

/*
 * xclSysfsGetDeviceInfo()
 */
void xocl::XOCLShim::xclSysfsGetDeviceInfo(xclDeviceInfo2 *info)
{
    std::string s;
    std::string errmsg;
    auto dev = pcidev::get_dev(mBoardNumber);

    if(dev->mgmt){
        dev->mgmt->sysfs_get("", "vendor", errmsg, info->mVendorId);
        dev->mgmt->sysfs_get("", "device", errmsg, info->mDeviceId);
        dev->mgmt->sysfs_get("", "subsystem_device", errmsg, info->mSubsystemId);
        info->mDeviceVersion = info->mSubsystemId & 0xff;
        dev->mgmt->sysfs_get("", "subsystem_vendor", errmsg, info->mSubsystemVendorId);
        info->mDataAlignment = getpagesize();
        dev->mgmt->sysfs_get("rom", "ddr_bank_size", errmsg, info->mDDRSize);
        info->mDDRSize = GB(info->mDDRSize);

        dev->mgmt->sysfs_get("rom", "VBNV", errmsg, s);
        snprintf(info->mName, sizeof (info->mName), "%s", s.c_str());
        dev->mgmt->sysfs_get("rom", "FPGA", errmsg, s);
        snprintf(info->mFpga, sizeof (info->mFpga), "%s", s.c_str());
        dev->mgmt->sysfs_get("rom", "timestamp", errmsg, info->mTimeStamp);
        dev->mgmt->sysfs_get("rom", "ddr_bank_count_max", errmsg, info->mDDRBankCount);
        info->mDDRSize *= info->mDDRBankCount;

        info->mNumClocks = numClocks(info->mName);

        dev->mgmt->sysfs_get("", "link_width", errmsg, info->mPCIeLinkWidth);
        dev->mgmt->sysfs_get("", "link_speed", errmsg, info->mPCIeLinkSpeed);
        dev->mgmt->sysfs_get("", "link_speed_max", errmsg, info->mPCIeLinkSpeedMax);
        dev->mgmt->sysfs_get("", "link_width_max", errmsg, info->mPCIeLinkWidthMax);

        dev->mgmt->sysfs_get("", "version", errmsg, info->mDriverVersion);
        dev->mgmt->sysfs_get("", "slot", errmsg, info->mPciSlot);
        dev->mgmt->sysfs_get("", "xpr", errmsg, info->mIsXPR);
        dev->mgmt->sysfs_get("", "mig_calibration", errmsg, info->mMigCalib);

        dev->mgmt->sysfs_get("sysmon", "vcc_int", errmsg, info->mVInt);
        dev->mgmt->sysfs_get("sysmon", "vcc_aux", errmsg, info->mVAux);
        dev->mgmt->sysfs_get("sysmon", "vcc_bram", errmsg, info->mVBram);

        dev->mgmt->sysfs_get("microblaze", "version", errmsg, info->mMBVersion);

        dev->mgmt->sysfs_get("xmc", "version", errmsg, info->mXMCVersion);
        dev->mgmt->sysfs_get("xmc", "xmc_12v_pex_vol", errmsg, info->m12VPex);
        dev->mgmt->sysfs_get("xmc", "xmc_12v_aux_vol", errmsg, info->m12VAux);
        dev->mgmt->sysfs_get("xmc", "xmc_12v_pex_curr", errmsg, info->mPexCurr);
        dev->mgmt->sysfs_get("xmc", "xmc_12v_aux_curr", errmsg, info->mAuxCurr);
        dev->mgmt->sysfs_get("xmc", "xmc_dimm_temp0", errmsg, info->mDimmTemp[0]);
        dev->mgmt->sysfs_get("xmc", "xmc_dimm_temp1", errmsg, info->mDimmTemp[1]);
        dev->mgmt->sysfs_get("xmc", "xmc_dimm_temp2", errmsg, info->mDimmTemp[2]);
        dev->mgmt->sysfs_get("xmc", "xmc_dimm_temp3", errmsg, info->mDimmTemp[3]);
        dev->mgmt->sysfs_get("xmc", "xmc_se98_temp0", errmsg, info->mSE98Temp[0]);
        dev->mgmt->sysfs_get("xmc", "xmc_se98_temp1", errmsg, info->mSE98Temp[1]);
        dev->mgmt->sysfs_get("xmc", "xmc_se98_temp2", errmsg, info->mSE98Temp[2]);
        dev->mgmt->sysfs_get("xmc", "xmc_fan_temp", errmsg, info->mFanTemp);
        dev->mgmt->sysfs_get("xmc", "xmc_fan_rpm", errmsg, info->mFanRpm);
        dev->mgmt->sysfs_get("xmc", "xmc_3v3_pex_vol", errmsg, info->m3v3Pex);
        dev->mgmt->sysfs_get("xmc", "xmc_3v3_aux_vol", errmsg, info->m3v3Aux);
        dev->mgmt->sysfs_get("xmc", "xmc_ddr_vpp_btm", errmsg, info->mDDRVppBottom);
        dev->mgmt->sysfs_get("xmc", "xmc_ddr_vpp_top", errmsg, info->mDDRVppTop);
        dev->mgmt->sysfs_get("xmc", "xmc_sys_5v5", errmsg, info->mSys5v5);
        dev->mgmt->sysfs_get("xmc", "xmc_1v2_top", errmsg, info->m1v2Top);
        dev->mgmt->sysfs_get("xmc", "xmc_1v8", errmsg, info->m1v8Top);
        dev->mgmt->sysfs_get("xmc", "xmc_0v85", errmsg, info->m0v85);
        dev->mgmt->sysfs_get("xmc", "xmc_mgt0v9avcc", errmsg, info->mMgt0v9);
        dev->mgmt->sysfs_get("xmc", "xmc_12v_sw", errmsg, info->m12vSW);
        dev->mgmt->sysfs_get("xmc", "xmc_mgtavtt", errmsg, info->mMgtVtt);
        dev->mgmt->sysfs_get("xmc", "xmc_vcc1v2_btm", errmsg, info->m1v2Bottom);
        dev->mgmt->sysfs_get("xmc", "xmc_vccint_vol", errmsg, info->mVccIntVol);
        dev->mgmt->sysfs_get("xmc", "xmc_fpga_temp", errmsg, info->mOnChipTemp);

        std::vector<uint64_t> freqs;
        dev->mgmt->sysfs_get("icap", "clock_freqs", errmsg, freqs);
        for (unsigned i = 0;
            i < std::min(freqs.size(), ARRAY_SIZE(info->mOCLFrequency));
            i++) {
            info->mOCLFrequency[i] = freqs[i];
        }
    }
    // Below info from user pf.
    if(dev->user){
        dev->user->sysfs_get("", "vendor", errmsg, info->mVendorId);
        dev->user->sysfs_get("", "device", errmsg, info->mDeviceId);
        dev->user->sysfs_get("", "subsystem_device", errmsg, info->mSubsystemId);
        info->mDeviceVersion = info->mSubsystemId & 0xff;
        dev->user->sysfs_get("", "subsystem_vendor", errmsg, info->mSubsystemVendorId);
        info->mDataAlignment = getpagesize();
        dev->user->sysfs_get("rom", "ddr_bank_size", errmsg, info->mDDRSize);
        info->mDDRSize = GB(info->mDDRSize);

        dev->user->sysfs_get("rom", "VBNV", errmsg, s);
        snprintf(info->mName, sizeof (info->mName), "%s", s.c_str());
        dev->user->sysfs_get("rom", "FPGA", errmsg, s);
        snprintf(info->mFpga, sizeof (info->mFpga), "%s", s.c_str());
        dev->user->sysfs_get("rom", "timestamp", errmsg, info->mTimeStamp);
        dev->user->sysfs_get("rom", "ddr_bank_count_max", errmsg, info->mDDRBankCount);
        info->mDDRSize *= info->mDDRBankCount;

        info->mNumClocks = numClocks(info->mName);

        dev->user->sysfs_get("mb_scheduler", "kds_numcdmas", errmsg, info->mNumCDMA);
        dev->user->sysfs_get("xmc", "xmc_12v_pex_vol", errmsg, info->m12VPex);
        dev->user->sysfs_get("xmc", "xmc_12v_aux_vol", errmsg, info->m12VAux);
        dev->user->sysfs_get("xmc", "xmc_12v_pex_curr", errmsg, info->mPexCurr);
        dev->user->sysfs_get("xmc", "xmc_12v_aux_curr", errmsg, info->mAuxCurr);
        dev->user->sysfs_get("xmc", "xmc_dimm_temp0", errmsg, info->mDimmTemp[0]);
        dev->user->sysfs_get("xmc", "xmc_dimm_temp1", errmsg, info->mDimmTemp[1]);
        dev->user->sysfs_get("xmc", "xmc_dimm_temp2", errmsg, info->mDimmTemp[2]);
        dev->user->sysfs_get("xmc", "xmc_dimm_temp3", errmsg, info->mDimmTemp[3]);
        dev->user->sysfs_get("xmc", "xmc_se98_temp0", errmsg, info->mSE98Temp[0]);
        dev->user->sysfs_get("xmc", "xmc_se98_temp1", errmsg, info->mSE98Temp[1]);
        dev->user->sysfs_get("xmc", "xmc_se98_temp2", errmsg, info->mSE98Temp[2]);
        dev->user->sysfs_get("xmc", "xmc_fan_temp", errmsg, info->mFanTemp);
        dev->user->sysfs_get("xmc", "xmc_fan_rpm", errmsg, info->mFanRpm);
        dev->user->sysfs_get("xmc", "xmc_3v3_pex_vol", errmsg, info->m3v3Pex);
        dev->user->sysfs_get("xmc", "xmc_3v3_aux_vol", errmsg, info->m3v3Aux);
        dev->user->sysfs_get("xmc", "xmc_ddr_vpp_btm", errmsg, info->mDDRVppBottom);
        dev->user->sysfs_get("xmc", "xmc_ddr_vpp_top", errmsg, info->mDDRVppTop);
        dev->user->sysfs_get("xmc", "xmc_sys_5v5", errmsg, info->mSys5v5);
        dev->user->sysfs_get("xmc", "xmc_1v2_top", errmsg, info->m1v2Top);
        dev->user->sysfs_get("xmc", "xmc_1v8", errmsg, info->m1v8Top);
        dev->user->sysfs_get("xmc", "xmc_0v85", errmsg, info->m0v85);
        dev->user->sysfs_get("xmc", "xmc_mgt0v9avcc", errmsg, info->mMgt0v9);
        dev->user->sysfs_get("xmc", "xmc_12v_sw", errmsg, info->m12vSW);
        dev->user->sysfs_get("xmc", "xmc_mgtavtt", errmsg, info->mMgtVtt);
        dev->user->sysfs_get("xmc", "xmc_vcc1v2_btm", errmsg, info->m1v2Bottom);
        dev->user->sysfs_get("xmc", "xmc_vccint_vol", errmsg, info->mVccIntVol);
        dev->user->sysfs_get("xmc", "xmc_fpga_temp", errmsg, info->mOnChipTemp);

        dev->user->sysfs_get("", "link_width", errmsg, info->mPCIeLinkWidth);
        dev->user->sysfs_get("", "link_speed", errmsg, info->mPCIeLinkSpeed);
        dev->user->sysfs_get("", "link_speed_max", errmsg, info->mPCIeLinkSpeedMax);
        dev->user->sysfs_get("", "link_width_max", errmsg, info->mPCIeLinkWidthMax);
        std::vector<uint64_t> freqs;
        dev->user->sysfs_get("icap", "clock_freqs", errmsg, freqs);
        for (unsigned i = 0;
            i < std::min(freqs.size(), ARRAY_SIZE(info->mOCLFrequency));
            i++) {
            info->mOCLFrequency[i] = freqs[i];
        }
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
    info->mHALMinorVersion = XCLHAL_MINOR_VER;
    info->mMinTransferSize = DDR_BUFFER_ALIGNMENT;
    info->mDMAThreads = 2;
    xclSysfsGetDeviceInfo(info);
    return 0;
}

/*
 * resetDevice()
 */
int xocl::XOCLShim::resetDevice(xclResetKind kind)
{
    int ret;
    std::string err;

    if (kind == XCL_RESET_FULL)
        ret = ioctl(mMgtHandle, XCLMGMT_IOCHOTRESET);
    else if (kind == XCL_RESET_KERNEL)
        ret = ioctl(mMgtHandle, XCLMGMT_IOCOCLRESET);
    else if (kind == XCL_USER_RESET) {
        int dev_offline = 1;
        ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_HOT_RESET);
        if (ret)
        return errno;

        dev_fini();
    while (dev_offline) {
            pcidev::get_dev(mBoardNumber)->user->sysfs_get("", "dev_offline", err, dev_offline);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    dev_init();
    } else
        return -EINVAL;

    return ret ? errno : 0;
}

int xocl::XOCLShim::p2pEnable(bool enable, bool force)
{
    const std::string input = "1\n";
    std::string err;

    if (enable)
        pcidev::get_dev(mBoardNumber)->user->sysfs_put("", "p2p_enable", err, "1");
    else
        pcidev::get_dev(mBoardNumber)->user->sysfs_put("", "p2p_enable", err, "0");

    if (errno == ENOSPC)
        return errno;
    else if (errno == EALREADY && !force)
        return 0;

    if (force) {
        dev_fini();
        /* remove root bus and rescan */
        pcidev::get_dev(mBoardNumber)->user->sysfs_put("", "root_dev/remove", err, input);


        // initiate rescan "echo 1 > /sys/bus/pci/rescan"
        const std::string rescan_path = "/sys/bus/pci/rescan";
        std::ofstream rescanFile(rescan_path);
        if(!rescanFile.is_open()) {
            perror(rescan_path.c_str());
        } else {
            rescanFile << input;
    }

    dev_init();
    }

    int p2p_enable = -1;
    pcidev::get_dev(mBoardNumber)->user->sysfs_get("", "p2p_enable", err, p2p_enable);
    if (p2p_enable == 2)
        return EBUSY;

    return 0;
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
    int ret;
    xclmgmt_ioc_freqscaling obj;
    std::memset(&obj, 0, sizeof(xclmgmt_ioc_freqscaling));
    obj.ocl_region = region;
    obj.ocl_target_freq[0] = targetFreqMHz[0];
    obj.ocl_target_freq[1] = targetFreqMHz[1];
    obj.ocl_target_freq[2] = targetFreqMHz[2];
    ret = ioctl(mMgtHandle, XCLMGMT_IOCFREQSCALE, &obj);
    return ret ? -errno : ret;
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

int xocl::XOCLShim::xclLoadXclBinMgmt(const xclBin *buffer)
{
    int ret = 0;
    const char *xclbininmemory = reinterpret_cast<char*> (const_cast<xclBin*> (buffer));

    if (!memcmp(xclbininmemory, "xclbin2", 8)) {
        ret = xclLoadAxlfMgmt(reinterpret_cast<const axlf*>(xclbininmemory));
        if (ret != 0) {
            if (ret == -EINVAL) {
                std::stringstream output;
                output << "Xclbin does not match DSA on card.\n"
                    << "Please run xbutil flash -a all to flash card."
                    << std::endl;
                if (mLogStream.is_open()) {
                    mLogStream << output.str();
                } else {
                    std::cout << output.str();
                }
            }
        }
    } else {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", Legacy xclbin no longer supported" << std::endl;
        }
        return -EINVAL;
    }

    if( ret != 0 ) {
        std::string errmsg;
        std::string line;
        auto dev = pcidev::get_dev(mBoardNumber);
        if(dev->mgmt){
            dev->mgmt->sysfs_get(
                "", "error", errmsg, line);
            std::cout << line << std::endl;
        }
    }

    mIsDebugIpLayoutRead = false;

    return ret;
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
        if (ret != 0) {
            if (ret == -EINVAL) {
                std::stringstream output;
                output << "Xclbin does not match DSA on card or xrt version.\n"
                    << "Please install compatible xrt or run xbutil flash -a all to flash card."
                    << std::endl;
                if (mLogStream.is_open()) {
                    mLogStream << output.str();
                } else {
                    std::cout << output.str();
                }
            }
        }
    } else {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", Legacy xclbin no longer supported" << std::endl;
        }
        return -EINVAL;
    }

    if( ret != 0 ) {
        std::string errmsg;
        std::string line;
        auto dev = pcidev::get_dev(mBoardNumber);
        if(dev->mgmt){
            dev->mgmt->sysfs_get(
                "", "error", errmsg, line);
            std::cout << line << std::endl;
        }
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
         std::cout << __func__ << " ERROR: Device is not locked" << std::endl;
        return -EPERM;
    }
    int ret;

    drm_xocl_axlf axlf_obj = {const_cast<axlf *>(buffer)};
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_READ_AXLF, &axlf_obj);
    if(ret) {
        return ret ? -errno : ret;
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
    return ret;
}

int xocl::XOCLShim::xclLoadAxlfMgmt(const axlf *buffer)
{
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << buffer << std::endl;
    }

    const unsigned cmd = XCLMGMT_IOCICAPDOWNLOAD_AXLF;
    xclmgmt_ioc_bitstream_axlf obj = {const_cast<axlf *>(buffer)};
    int ret = ioctl(mMgtHandle, cmd, &obj);
    if(ret) {
        return ret ? -errno : ret;
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
    return result ? -errno : result;
}

int xocl::XOCLShim::xclGetSectionInfo(void* section_info, size_t * section_size,
    enum axlf_section_kind kind, int index)
{
    if(section_info == nullptr || section_size == nullptr)
        return -EINVAL;

    std::string entry;
    if(kind == MEM_TOPOLOGY)
        entry = "mem_topology";
    else if (kind == CONNECTIVITY)
        entry = "connectivity";
    else if (kind == IP_LAYOUT)
        entry = "ip_layout";
    else {
        std::cout << "Unhandled section found" << std::endl;
        return -EINVAL;
    }

    std::string err;
    std::vector<char> buf;
    pcidev::get_dev(mBoardNumber)->user->sysfs_get("icap", entry, err, buf);
    if (!err.empty()) {
        std::cout << err << std::endl;
        return -EINVAL;
    }

    char* memblock = buf.data();

    if(kind == MEM_TOPOLOGY) {
        mem_topology* mem = reinterpret_cast<mem_topology *>(memblock);
        if (index > (mem->m_count -1))
            return -EINVAL;
        memcpy(section_info, &mem->m_mem_data[index], sizeof(mem_data));
        *section_size = sizeof (mem_data);
    } else if (kind == CONNECTIVITY) {
        connectivity* con = reinterpret_cast<connectivity *>(memblock);
        if (index > (con->m_count -1))
            return -EINVAL;
        memcpy(section_info, &con->m_connection[index], sizeof(connection));
        *section_size = sizeof (connection);
    } else if(kind == IP_LAYOUT) {
        ip_layout* ip = reinterpret_cast<ip_layout *>(memblock);
        if (index > (ip->m_count -1))
            return -EINVAL;
        memcpy(section_info, &ip->m_ip_data[index], sizeof(ip_data));
        *section_size = sizeof (ip_data);
    }

    return 0;
}

/*
 * xclSysfsGetUsageInfo()
 */
void xocl::XOCLShim::xclSysfsGetUsageInfo(drm_xocl_usage_stat& stat)
{
    std::string errmsg;
    std::vector<std::string> dmaStatStrs;
    std::vector<std::string> mmStatStrs;
    std::vector<std::string> xmcStatStrs;
    auto dev = pcidev::get_dev(mBoardNumber);
    if(dev->user){
        dev->user->sysfs_get("mm_dma", "channel_stat_raw", errmsg, dmaStatStrs);
        dev->user->sysfs_get("", "memstat_raw", errmsg, mmStatStrs);
        dev->user->sysfs_get("microblaze", "version", errmsg, xmcStatStrs);
    }
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

    xclSysfsGetUsageInfo(stat);
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
    return (mUserHandle >= 0);
}

bool xocl::XOCLShim::isGoodMgmt() const {
    return (mMgtHandle >= 0);
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

xocl::XOCLShim *xocl::XOCLShim::handleCheckMgmt(void *handle)
{
    if (!handle) {
        return NULL;
    }
    if (!((XOCLShim *) handle)->isGoodMgmt()) {
        return NULL;
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
    int ret;
    if (mLogStream.is_open()) {
        mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << cmdBO << std::endl;
    }
    drm_xocl_execbuf exec = {0, cmdBO, 0,0,0,0,0,0,0,0};
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_EXECBUF, &exec);
    return ret ? -errno : ret;
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
    int ret;
    unsigned int bwl[8] = {0};
    std::memcpy(bwl,bo_wait_list,num_bo_in_wait_list*sizeof(unsigned int));
    drm_xocl_execbuf exec = {0, cmdBO, bwl[0],bwl[1],bwl[2],bwl[3],bwl[4],bwl[5],bwl[6],bwl[7]};
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_EXECBUF, &exec);
    return ret ? -errno : ret;
}

/*
 * xclRegisterEventNotify()
 */
int xocl::XOCLShim::xclRegisterEventNotify(unsigned int userInterrupt, int fd)
{
    int ret ;
    drm_xocl_user_intr userIntr = {0, fd, (int)userInterrupt};
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_USER_INTR, &userIntr);
    return ret ? -errno : ret;
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
int xocl::XOCLShim::xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared) const
{
    unsigned int flags = shared ? XOCL_CTX_SHARED : XOCL_CTX_EXCLUSIVE;
    int ret;
    drm_xocl_ctx ctx = {XOCL_CTX_OP_ALLOC_CTX};
    std::memcpy(ctx.xclbin_id, xclbinId, sizeof(uuid_t));
    ctx.cu_index = ipIndex;
    ctx.flags = flags;
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_CTX, &ctx);
    return ret ? -errno : ret;
}

/*
 * xclCloseContext
 */
int xocl::XOCLShim::xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex) const
{
    int ret;
    drm_xocl_ctx ctx = {XOCL_CTX_OP_FREE_CTX};
    std::memcpy(ctx.xclbin_id, xclbinId, sizeof(uuid_t));
    ctx.cu_index = ipIndex;
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_CTX, &ctx);
    return ret ? -errno : ret;
}

/*
 * xclBootFPGA()
 */
int xocl::XOCLShim::xclBootFPGA()
{
    int ret;
    ret = ioctl( mMgtHandle, XCLMGMT_IOCREBOOT );
    return ret ? -errno : ret;
}

/*
 * xclCreateWriteQueue()
 */
int xocl::XOCLShim::xclCreateWriteQueue(xclQueueContext *q_ctx, uint64_t *q_hdl)
{
    struct xocl_qdma_ioc_create_queue q_info;
    int rc;

    memset(&q_info, 0, sizeof (q_info));
    q_info.write = 1;
    q_info.rid = q_ctx->route;
    q_info.flowid = q_ctx->flow;
    q_info.flags = q_ctx->flags;

    rc = ioctl(mStreamHandle, XOCL_QDMA_IOC_CREATE_QUEUE, &q_info);
    if (rc) {
        std::cout << __func__ << " ERROR: Create Write Queue IOCTL failed" << std::endl;
    } else
        *q_hdl = q_info.handle;

     return rc ? -errno : rc;
}

/*
 * xclCreateReadQueue()
 */
int xocl::XOCLShim::xclCreateReadQueue(xclQueueContext *q_ctx, uint64_t *q_hdl)
{
    struct xocl_qdma_ioc_create_queue q_info;
    int rc;

    memset(&q_info, 0, sizeof (q_info));

    q_info.rid = q_ctx->route;
    q_info.flowid = q_ctx->flow;
    q_info.flags = q_ctx->flags;

    rc = ioctl(mStreamHandle, XOCL_QDMA_IOC_CREATE_QUEUE, &q_info);
    if (rc) {
        std::cout << __func__ << " ERROR: Create Read Queue IOCTL failed" << std::endl;
    } else
        *q_hdl = q_info.handle;

    return rc ? -errno : rc;
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

/*
 * xclPollCompletion()
 */
int xocl::XOCLShim::xclPollCompletion(int min_compl, int max_compl, struct xclReqCompletion *comps, int* actual, int timeout /*ms*/)
{
    /* TODO: populate actual and timeout args correctly */
    struct timespec time, *ptime = NULL;
    int num_evt, i;

    *actual = 0;
    if (!mAioEnabled) {
        num_evt = -EINVAL;
        std::cout << __func__ << "ERROR: async io is not enabled" << std::endl;
        goto done;
    }
    if (timeout > 0) {
        memset(&time, 0, sizeof(time));
        time.tv_sec = timeout / 1000;
        time.tv_nsec = (timeout % 1000) * 1000000;
        ptime = &time;
    }

    num_evt = io_getevents(mAioContext, min_compl, max_compl, (struct io_event *)comps, ptime);
    if (num_evt < min_compl) {
        std::cout << __func__ << " ERROR: failed to poll Queue Completions" << std::endl;
        goto done;
    }
    *actual = num_evt;

    for (i = num_evt - 1; i >= 0; i--) {
        comps[i].priv_data = (void *)((struct io_event *)comps)[i].data;
    if (((struct io_event *)comps)[i].res < 0){
            /* error returned by AIO framework */
            comps[i].nbytes = 0;
        comps[i].err_code = ((struct io_event *)comps)[i].res;
    } else {
            comps[i].nbytes = ((struct io_event *)comps)[i].res;
        comps[i].err_code = ((struct io_event *)comps)[i].res2;
    }
    }
    num_evt = 0;

done:
    return num_evt;
}

/*
 * xclWriteQueue()
 */
ssize_t xocl::XOCLShim::xclWriteQueue(uint64_t q_hdl, xclQueueRequest *wr)
{
    ssize_t rc = 0;

    for (unsigned i = 0; i < wr->buf_num; i++) {
        void *buf = (void *)wr->bufs[i].va;
        struct iovec iov[2];
        struct xocl_qdma_req_header header;

        header.flags = wr->flag;
        iov[0].iov_base = &header;
        iov[0].iov_len = sizeof(header);
        iov[1].iov_base = buf;
        iov[1].iov_len = wr->bufs[i].len;

        if (wr->flag & XCL_QUEUE_REQ_NONBLOCKING) {
            struct iocb cb;
            struct iocb *cbs[1];

            if (!mAioEnabled) {
                std::cout << __func__ << "ERROR: async io is not enabled" << std::endl;
                break;
            }

            if (!(wr->flag & XCL_QUEUE_REQ_EOT) && (wr->bufs[i].len & 0xfff)) {
                std::cerr << "ERROR: write without EOT has to be multiple of 4k" << std::endl;
                break;
            }

            memset(&cb, 0, sizeof(cb));
            cb.aio_fildes = (int)q_hdl;
            cb.aio_lio_opcode = IOCB_CMD_PWRITEV;
            cb.aio_buf = (uint64_t)iov;
            cb.aio_offset = 0;
            cb.aio_nbytes = 2;
            cb.aio_data = (uint64_t)wr->priv_data;

            cbs[0] = &cb;
            if (io_submit(mAioContext, 1, cbs) > 0)
                rc++;
            else {
                std::cerr << "ERROR: async write stream failed" << std::endl;
                break;
            }
        } else {
            if (!(wr->flag & XCL_QUEUE_REQ_EOT) && (wr->bufs[i].len & 0xfff)) {
                std::cerr << "ERROR: write without EOT has to be multiple of 4k" << std::endl;
                rc = -EINVAL;
                break;
            }

            rc = writev((int)q_hdl, iov, 2);
            if (rc < 0) {
                std::cerr << "ERROR: write stream failed: " << rc << std::endl;
                break;
            } else if ((size_t)rc != wr->bufs[i].len) {
                std::cerr << "ERROR: only " << rc << "/" << wr->bufs[i].len;
                std::cerr << " bytes is written" << std::endl;
                break;
            }
        }
    }
    return rc;
}

/*
 * xclReadQueue()
 */
ssize_t xocl::XOCLShim::xclReadQueue(uint64_t q_hdl, xclQueueRequest *wr)
{
    ssize_t rc = 0;

    for (unsigned i = 0; i < wr->buf_num; i++) {
        void *buf = (void *)wr->bufs[i].va;
        struct iovec iov[2];
        struct xocl_qdma_req_header header;

        header.flags = wr->flag;
        iov[0].iov_base = &header;
        iov[0].iov_len = sizeof(header);
        iov[1].iov_base = buf;
        iov[1].iov_len = wr->bufs[i].len;

        if (wr->flag & XCL_QUEUE_REQ_NONBLOCKING) {
            struct iocb cb;
            struct iocb *cbs[1];

            if (!mAioEnabled) {
                std::cout << __func__ << "ERROR: async io is not enabled" << std::endl;
                break;
            }

            memset(&cb, 0, sizeof(cb));
            cb.aio_fildes = (int)q_hdl;
            cb.aio_lio_opcode = IOCB_CMD_PREADV;
            cb.aio_buf = (uint64_t)iov;
            cb.aio_offset = 0;
            cb.aio_nbytes = 2;
            cb.aio_data = (uint64_t)wr->priv_data;

            cbs[0] = &cb;
            if (io_submit(mAioContext, 1, cbs) > 0)
                rc++;
            else {
                std::cerr << "ERROR: async read stream failed" << std::endl;
                break;
            }
        } else {
            rc = readv((int)q_hdl, iov, 2);
            if (rc < 0) {
                std::cerr << "ERROR: read stream failed: " << rc << std::endl;
                break;
            }
        }
    }
    return rc;

}


int xocl::XOCLShim::xclReClockUser(unsigned short region, const unsigned short *targetFreqMHz)
{
    int ret;
    drm_xocl_reclock_info reClockInfo;
    std::memset(&reClockInfo, 0, sizeof(drm_xocl_reclock_info));
    reClockInfo.region = region;
    reClockInfo.ocl_target_freq[0] = targetFreqMHz[0];
    reClockInfo.ocl_target_freq[1] = targetFreqMHz[1];
    reClockInfo.ocl_target_freq[2] = targetFreqMHz[2];
    ret = ioctl(mUserHandle, DRM_IOCTL_XOCL_RECLOCK, &reClockInfo);
    return ret ? -errno : ret;
}

uint xocl::XOCLShim::xclGetNumLiveProcesses()
{
  std::string errmsg;
  auto dev = pcidev::get_dev(mBoardNumber);

  // Below info from user pf.
  if(dev->user) {
    std::vector<std::string> stringVec;
    dev->user->sysfs_get("", "kdsstat", errmsg, stringVec);
    // Dependent on message format built in kdsstat_show. Checking number of "context" in kdsstat.
    // kdsstat has "context: <number_of_live_processes>"
    if(stringVec.size() >= 4) {
        std::size_t p = stringVec[3].find_first_of("0123456789");
        std::string subStr = stringVec[3].substr(p);
        uint number = std::stoul(subStr);
        return number;
    }
  }
  return 0;
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
    int mgmt_slot = -1;

    for(int i = 0; i < 16; ++i) {
        std::string mgmtFile = "/dev/xclmgmt"+ std::to_string(i);
        int mgmt_fd = open(mgmtFile.c_str(), O_RDWR | O_SYNC);
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
    return pcidev::get_dev_ready();
}

xclDeviceHandle xclOpen(unsigned deviceIndex, const char *logFileName, xclVerbosityLevel level)
{
    if(pcidev::get_dev_total() <= deviceIndex) {
        printf("Cannot find index %u \n", deviceIndex);
        return nullptr;
    }

    xocl::XOCLShim *handle = new xocl::XOCLShim(deviceIndex, logFileName, level);

    return static_cast<xclDeviceHandle>(handle);
}

void xclClose(xclDeviceHandle handle)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    if (drv) {
        delete drv;
        return;
    }
    xocl::XOCLShim *mgmt_drv = xocl::XOCLShim::handleCheckMgmt(handle);
    if (mgmt_drv) {
        delete mgmt_drv;
        return;
    }
}

int xclLoadXclBinMgmt(xclDeviceHandle handle, const xclBin *buffer)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheckMgmt(handle);

    return drv ? drv->xclLoadXclBinMgmt(buffer) : -ENODEV;
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    auto ret = drv ? drv->xclLoadXclBin(buffer) : -ENODEV;
    if (!ret)
      ret = xrt_core::scheduler::init(handle, buffer);
    return ret;
}

int xclLogMsg(xclDeviceHandle handle, xclLogMsgLevel level, const char* tag, const char* format, ...)
{
    va_list args;
    va_start(args, format);

    int ret = xocl::XOCLShim::xclLogMsg(handle, level, tag, format, args);
    va_end(args);

    return ret;
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
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheckMgmt(handle);
    std::memset(info, 0, sizeof(xclErrorStatus));
    if(!drv)
        return 0;
    return drv->xclGetErrorStatus(info);
}

int xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
    xocl::XOCLShim *drv = (xocl::XOCLShim *) handle;
    return drv ? drv->xclGetDeviceInfo2(info) : -ENODEV;
}

unsigned int xclVersion ()
{
    return 2;
}

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, xclBOKind domain, unsigned flags)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclAllocBO(size, domain, flags) : -ENODEV;
}

unsigned int xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
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
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheckMgmt(handle);
    std::cout<<"xclReClock2"<<std::endl;
    return drv ? drv->xclReClock2(region, targetFreqMHz) : -ENODEV;
}

int xclReClockUser(xclDeviceHandle handle, unsigned short region, const unsigned short *targetFreqMHz)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclReClockUser(region, targetFreqMHz) : -ENODEV;
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
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheckMgmt(handle);
    return drv ? drv->resetDevice(kind) : -ENODEV;
}

int xclP2pEnable(xclDeviceHandle handle, bool enable, bool force)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->p2pEnable(enable, force) : -ENODEV;
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

    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheckMgmt(handle);
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
        pcidev::rescan();
    }

    return retVal;
}

int xclRemoveAndScanFPGA(void)
{
    const std::string input = "1\n";

    // remove devices "echo 1 > /sys/bus/pci/devices/<deviceHandle>/remove"
    for (unsigned int i = 0; i < pcidev::get_dev_total(); i++)
    {
        std::string err;
        auto dev = pcidev::get_dev(i);
        if(dev->user)
            dev->user->sysfs_put("", "remove", err, input);
        if(dev->mgmt)
            dev->mgmt->sysfs_put("", "remove", err, input);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // initiate rescan "echo 1 > /sys/bus/pci/rescan"
    const std::string rescan_path = "/sys/bus/pci/rescan";
    std::ofstream rescanFile(rescan_path);
    if(!rescanFile.is_open()) {
        perror(rescan_path.c_str());
        return -errno;
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

int xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclExecWait(timeoutMilliSec) : -ENODEV;
}

int xclOpenContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclOpenContext(xclbinId, ipIndex, shared) : -ENODEV;
}

int xclCloseContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned ipIndex)
{
  xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
  return drv ? drv->xclCloseContext(xclbinId, ipIndex) : -ENODEV;
}

const axlf_section_header* wrap_get_axlf_section(const axlf* top, axlf_section_kind kind)
{
    return xclbin::get_axlf_section(top, kind);
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

int xclPollCompletion(xclDeviceHandle handle, int min_compl, int max_compl, xclReqCompletion *comps, int* actual, int timeout)
{
        xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
        return drv ? drv->xclPollCompletion(min_compl, max_compl, comps, actual, timeout) : -ENODEV;
}

xclDeviceHandle xclOpenMgmt(unsigned deviceIndex, const char *logFileName, xclVerbosityLevel level)
{
    if(pcidev::get_dev_total() <= deviceIndex) {
        printf("Cannot find index %u \n", deviceIndex);
        return nullptr;
    }

    xocl::XOCLShim *handle = new xocl::XOCLShim(deviceIndex, logFileName, level);
    return static_cast<xclDeviceHandle>(handle);
}

char *xclMapMgmt(xclDeviceHandle handle)
{
  xocl::XOCLShim *drv = static_cast<xocl::XOCLShim *>(handle);
  return drv ? drv->xclMapMgmt() :   nullptr;
}

uint xclGetNumLiveProcesses(xclDeviceHandle handle)
{
    xocl::XOCLShim *drv = xocl::XOCLShim::handleCheck(handle);
    return drv ? drv->xclGetNumLiveProcesses() : 0;
}

int xclMailbox(unsigned deviceIndex)
{
    uint16_t dom  = pcidev::get_dev(deviceIndex)->user->domain;
    uint16_t bus  = pcidev::get_dev(deviceIndex)->user->bus;
    uint16_t dev  = pcidev::get_dev(deviceIndex)->user->dev;
    uint16_t func = pcidev::get_dev(deviceIndex)->user->func;
    const int instance = ((dom<<16) + (bus<<8) + (dev<<3) + (func));
    const int fd = open(std::string("/dev/mailbox.u" + std::to_string(instance)).c_str(), O_RDWR);
    if (fd == -1) {
        perror("open");
        return errno;
    }
    return fd;
}

int xclMailboxMgmt(unsigned deviceIndex)
{
    uint16_t dom  = pcidev::get_dev(deviceIndex)->mgmt->domain;
    uint16_t bus  = pcidev::get_dev(deviceIndex)->mgmt->bus;
    uint16_t dev  = pcidev::get_dev(deviceIndex)->mgmt->dev;
    uint16_t func = pcidev::get_dev(deviceIndex)->mgmt->func;
    const int instance = ((dom<<16) + (bus<<8) + (dev<<3) + (func));
    const int fd = open(std::string("/dev/mailbox.m" + std::to_string(instance)).c_str(), O_RDWR);
    if (fd == -1) {
        perror("open");
        return errno;
    }
    return fd;
}
