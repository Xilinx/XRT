/**
 * Copyright (C) 2016-2019 Xilinx, Inc
 * Author(s): Umang Parekh
 *          : Sonal Santan
 *          : Ryan Radjabi
 *
 * XRT PCIe library layered on top of xocl kernel driver
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
#include "scan.h"
#include "core/common/message.h"
#include "core/common/xclbin_parser.h"
#include "core/common/scheduler.h"
#include "core/common/bo_cache.h"
#include "core/common/config_reader.h"
#include "core/common/AlignedAllocator.h"

#include "plugin/xdp/hal_profile.h"


#include "xclbin.h"
#include "ert.h"

#include "core/pcie/driver/linux/include/mgmt-reg.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <cerrno>

#include <unistd.h>
#include <poll.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <sys/file.h>
#include <linux/aio_abi.h>

#ifdef NDEBUG
# undef NDEBUG
# include<cassert>
#endif

#if defined(__GNUC__)
#define SHIM_UNUSED __attribute__((unused))
#endif

#define GB(x)           ((size_t) (x) << 30)
#define ARRAY_SIZE(x)   (sizeof (x) / sizeof (x[0]))

#define SHIM_QDMA_AIO_EVT_MAX   1024 * 64


// Profiling
#define AXI_FIFO_RDFD_AXI_FULL          0x1000
#define MAX_TRACE_NUMBER_SAMPLES                        16384
#define XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH            64


inline bool
is_multiprocess_mode()
{
  static bool val = std::getenv("XCL_MULTIPROCESS_MODE") != nullptr;
  return val;
}

/*
 * numClocks()
 */
inline unsigned numClocks(const std::string& name)
{
    return name.compare(0, 15, "xilinx_adm-pcie", 15) ? 2 : 1;
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


namespace xocl {

/*
 * shim()
 */
shim::shim(unsigned index, const char *logfileName, xclVerbosityLevel verbosity)
  : mVerbosity(verbosity),
    mUserHandle(-1),
    mStreamHandle(-1),
    mBoardNumber(index),
    mLocked(false),
    mLogfileName(nullptr),
    mOffsets{0x0, 0x0, OCL_CTLR_BASE, 0x0, 0x0},
    mMemoryProfilingNumberSlots(0),
    mAccelProfilingNumberSlots(0),
    mStallProfilingNumberSlots(0),
    mStreamProfilingNumberSlots(0),
    mCmdBOCache(nullptr),
    mCuMaps(128, nullptr)
{
    init(index, logfileName, verbosity);
}

int shim::dev_init()
{
    auto dev = pcidev::get_dev(mBoardNumber);
    if(dev == nullptr) {
        xrt_logmsg(XRT_ERROR, "%s: Card [%d] not found", __func__, mBoardNumber);
        return -ENOENT;
    }

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

    mUserHandle = dev->open("", O_RDWR);
    if (mUserHandle == -1)
        return -errno;

    int result = dev->ioctl(mUserHandle, DRM_IOCTL_VERSION, &version);
    if (result) {
        dev->close(mUserHandle);
        return -errno;
    }

    // We're good now.
    mDev = dev;
    (void) xclGetDeviceInfo2(&mDeviceInfo);
    mCmdBOCache = std::make_unique<xrt_core::bo_cache>(this, xrt_core::config::get_cmdbo_cache());

    mStreamHandle = mDev->open("dma.qdma", O_RDWR | O_SYNC);
    memset(&mAioContext, 0, sizeof(mAioContext));
    mAioEnabled = (io_setup(SHIM_QDMA_AIO_EVT_MAX, &mAioContext) == 0);

    return 0;
}

int shim::xrt_logmsg(xrtLogMsgLevel level, const char* format, ...)
{
    static auto verbosity = xrt_core::config::get_verbosity();
    if (level <= verbosity) {
        va_list args;
        va_start(args, format);
        int ret = xclLogMsg(level, "XRT", format, args);
        va_end(args);
        return ret;
    } else {
        return 0;
    }
}

void shim::dev_fini()
{
    if (mStreamHandle > 0) {
        close(mStreamHandle);
        mStreamHandle = 0;
    }

    if (mAioEnabled) {
        io_destroy(mAioContext);
            mAioEnabled = false;
    }

    if (mUserHandle != -1)
        mDev->close(mUserHandle);
}

/*
 * init()
 */
void shim::init(unsigned index, const char *logfileName,
    xclVerbosityLevel verbosity)
{
    if(logfileName != nullptr) {
        xrt_logmsg(XRT_WARNING, "%s: logfileName is no longer supported", __func__);
    }

    xrt_logmsg(XRT_INFO, "%s", __func__);

    int ret = dev_init();
    if (ret) {
        xrt_logmsg(XRT_WARNING, "XRT", "dev_init failed: %d", ret);
        return;
    }

    // Profiling - defaults
    // Class-level defaults: mIsDebugIpLayoutRead = mIsDeviceProfiling = false
    mDevUserName = mDev->sysfs_name;
    mMemoryProfilingNumberSlots = 0;
    mPerfMonFifoCtrlBaseAddress = 0x00;
    mPerfMonFifoReadBaseAddress = 0x00;
}

/*
 * ~shim()
 */
shim::~shim()
{
    xrt_logmsg(XRT_INFO, "%s", __func__);

    dev_fini();

    for (auto p : mCuMaps) {
        if (p)
            (void) munmap(p, mCuMapSize);
    }
}

/*
 * xclLogMsg()
 */
int shim::xclLogMsg(xrtLogMsgLevel level, const char* tag, const char* format, va_list args)
{
    static auto verbosity = xrt_core::config::get_verbosity();
    if (level <= verbosity) {
        va_list args_bak;
        // vsnprintf will mutate va_list so back it up
        va_copy(args_bak, args);
        int len = std::vsnprintf(nullptr, 0, format, args_bak);
        va_end(args_bak);

        if (len < 0) {
          //illegal arguments
          std::string err_str = "ERROR: Illegal arguments in log format string. ";
          err_str.append(std::string(format));
          xrt_core::message::send((xrt_core::message::severity_level)level, tag, err_str);
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
    }

    return 0;
}

/*
 * xclWrite()
 */
size_t shim::xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
    switch (space) {
        case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
            if (mDev->pcieBarWrite(offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        case XCL_ADDR_KERNEL_CTRL:
        {
            offset += mOffsets[XCL_ADDR_KERNEL_CTRL];
            const unsigned *reg = static_cast<const unsigned *>(hostBuf);
            size_t regSize = size / 4;
            if (regSize > 32)
            regSize = 32;
            for (unsigned i = 0; i < regSize; i++) {
                xrt_logmsg(XRT_INFO, "%s: space: %d, offset:0x%x, reg:%d",
                        __func__, space, offset+i, reg[i]);
            }
            if (mDev->pcieBarWrite(offset, hostBuf, size) == 0) {
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
size_t shim::xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
    xrt_logmsg(XRT_INFO, "%s, space: %d, offset: %d, hostBuf: %s, size: %d",
            __func__, space, offset, hostBuf, size);

    switch (space) {
        case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
            //offset += mOffsets[XCL_ADDR_SPACE_DEVICE_PERFMON];
            if (mDev->pcieBarRead(offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        case XCL_ADDR_KERNEL_CTRL:
        {
            offset += mOffsets[XCL_ADDR_KERNEL_CTRL];
            int result = mDev->pcieBarRead(offset, hostBuf, size);
            const unsigned *reg = static_cast<const unsigned *>(hostBuf);
            size_t regSize = size / 4;
            if (regSize > 4)
            regSize = 4;
            for (unsigned i = 0; i < regSize; i++) {
                xrt_logmsg(XRT_INFO, "%s: space: %d, offset:0x%x, reg:%d",
                    __func__, space, offset+i, reg[i]);
            }
            return !result ? size : 0;
        }
        case XCL_ADDR_SPACE_DEVICE_CHECKER:
        {
            if (mDev->pcieBarRead(offset, hostBuf, size) == 0) {
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
unsigned int shim::xclAllocBO(size_t size, int unused, unsigned flags)
{
    drm_xocl_create_bo info = {size, mNullBO, flags};
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_CREATE_BO, &info);
    return result ? mNullBO : info.handle;
}

/*
 * xclAllocUserPtrBO()
 */
unsigned int shim::xclAllocUserPtrBO(void *userptr, size_t size, unsigned flags)
{
    drm_xocl_userptr_bo user =
        {reinterpret_cast<uint64_t>(userptr), size, mNullBO, flags};
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_USERPTR_BO, &user);
    return result ? mNullBO : user.handle;
}

/*
 * xclFreeBO()
 */
void shim::xclFreeBO(unsigned int boHandle)
{
    drm_gem_close closeInfo = {boHandle, 0};
    (void) mDev->ioctl(mUserHandle, DRM_IOCTL_GEM_CLOSE, &closeInfo);
}

/*
 * xclWriteBO()
 */
int shim::xclWriteBO(unsigned int boHandle, const void *src, size_t size, size_t seek)
{
    int ret;
    drm_xocl_pwrite_bo pwriteInfo = { boHandle, 0, seek, size, reinterpret_cast<uint64_t>(src) };
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_PWRITE_BO, &pwriteInfo);
    return ret ? -errno : ret;
}

/*
 * xclReadBO()
 */
int shim::xclReadBO(unsigned int boHandle, void *dst, size_t size, size_t skip)
{
    int ret;
    drm_xocl_pread_bo preadInfo = { boHandle, 0, skip, size, reinterpret_cast<uint64_t>(dst) };
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_PREAD_BO, &preadInfo);
    return ret ? -errno : ret;
}

/*
 * xclMapBO()
 */
void *shim::xclMapBO(unsigned int boHandle, bool write)
{
    drm_xocl_info_bo info = { boHandle, 0, 0 };
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &info);
    if (result) {
        return nullptr;
    }

    drm_xocl_map_bo mapInfo = { boHandle, 0, 0 };
    result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_MAP_BO, &mapInfo);
    if (result) {
        return nullptr;
    }

    return mDev->mmap(mUserHandle, info.size,
        (write ? (PROT_READ | PROT_WRITE) : PROT_READ), MAP_SHARED,
        mapInfo.offset);
}

/*
 * xclMapBO()
 */
int shim::xclUnmapBO(unsigned int boHandle, void* addr)
{
    drm_xocl_info_bo info = { boHandle, 0, 0 };
    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &info);
    if (ret)
      return -errno;

    return mDev->munmap(mUserHandle, addr, info.size);
}

/*
 * xclSyncBO()
 */
int shim::xclSyncBO(unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
    drm_xocl_sync_bo_dir drm_dir = (dir == XCL_BO_SYNC_BO_TO_DEVICE) ?
            DRM_XOCL_SYNC_BO_TO_DEVICE :
            DRM_XOCL_SYNC_BO_FROM_DEVICE;
    drm_xocl_sync_bo syncInfo = {boHandle, 0, size, offset, drm_dir};
    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_SYNC_BO, &syncInfo);
    return ret ? -errno : ret;
}

/*
 * xclCopyBO()
 */
int shim::xclCopyBO(unsigned int dst_bo_handle,
    unsigned int src_bo_handle, size_t size, size_t dst_offset,
    size_t src_offset)
{
    auto bo = mCmdBOCache->alloc<ert_start_copybo_cmd>();
    ert_fill_copybo_cmd(bo.second, src_bo_handle, dst_bo_handle,
                        src_offset, dst_offset, size);

    int ret = xclExecBuf(bo.first);
    if (ret) {
        mCmdBOCache->release(bo);
        return ret;
    }

    do {
        ret = xclExecWait(1000);
        if (ret == -1)
            break;
    }
    while (bo.second->state < ERT_CMD_STATE_COMPLETED);

    ret = (ret == -1) ? -errno : 0;
    if (!ret && (bo.second->state != ERT_CMD_STATE_COMPLETED))
        ret = -EINVAL;
    mCmdBOCache->release<ert_start_copybo_cmd>(bo);
    return ret;
}

int shim::xclUpdateSchedulerStat()
{
    auto bo = mCmdBOCache->alloc<ert_packet>();
    bo.second->opcode = ERT_CU_STAT;
    bo.second->type = ERT_CTRL;

    int ret = xclExecBuf(bo.first);
    if (ret) {
        mCmdBOCache->release(bo);
        return ret;
    }

    do {
        ret = xclExecWait(1000);
        if (ret == -1)
            break;
    } while (bo.second->state < ERT_CMD_STATE_COMPLETED);

    ret = (ret == -1) ? -errno : 0;
    if (!ret && (bo.second->state != ERT_CMD_STATE_COMPLETED))
        ret = -EINVAL;
    mCmdBOCache->release<ert_packet>(bo);
    return ret;
}

/*
 * xclSysfsGetErrorStatus()
 */
void shim::xclSysfsGetErrorStatus(xclErrorStatus& stat)
{
    std::string errmsg;
    unsigned int status;
    unsigned int level;
    unsigned long time;

    mDev->sysfs_get<unsigned int>("firewall", "detected_status", errmsg, status, static_cast<unsigned int>(-1));
    mDev->sysfs_get<unsigned int>("firewall", "detected_level", errmsg, level, static_cast<unsigned int>(-1));
    mDev->sysfs_get<unsigned long>("firewall", "detected_time", errmsg, time, static_cast<unsigned long>(-1));

    stat.mNumFirewalls = XCL_FW_MAX_LEVEL;
    if (level < XCL_FW_MAX_LEVEL)
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
int shim::xclGetErrorStatus(xclErrorStatus *info)
{
    xclSysfsGetErrorStatus(*info);
    return 0;
}

/*
 * xclSysfsGetDeviceInfo()
 */
void shim::xclSysfsGetDeviceInfo(xclDeviceInfo2 *info)
{
    std::string s;
    std::string errmsg;

    mDev->sysfs_get<unsigned short>("", "vendor", errmsg, info->mVendorId, static_cast<unsigned short>(-1));
    mDev->sysfs_get<unsigned short>("", "device", errmsg, info->mDeviceId, static_cast<unsigned short>(-1));
    mDev->sysfs_get<unsigned short>("", "subsystem_device", errmsg, info->mSubsystemId, static_cast<unsigned short>(-1));
    info->mDeviceVersion = info->mSubsystemId & 0xff;
    mDev->sysfs_get<unsigned short>("", "subsystem_vendor", errmsg, info->mSubsystemVendorId, static_cast<unsigned short>(-1));
    info->mDataAlignment = getpagesize();
    mDev->sysfs_get<size_t>("rom", "ddr_bank_size", errmsg, info->mDDRSize, static_cast<unsigned short>(-1));
    info->mDDRSize = GB(info->mDDRSize);

    mDev->sysfs_get("rom", "VBNV", errmsg, s);
    snprintf(info->mName, sizeof (info->mName), "%s", s.c_str());
    mDev->sysfs_get("rom", "FPGA", errmsg, s);
    snprintf(info->mFpga, sizeof (info->mFpga), "%s", s.c_str());
    mDev->sysfs_get<unsigned long long>("rom", "timestamp", errmsg, info->mTimeStamp, static_cast<unsigned long long>(-1));
    mDev->sysfs_get<unsigned short>("rom", "ddr_bank_count_max", errmsg, info->mDDRBankCount, static_cast<unsigned short>(-1));
    info->mDDRSize *= info->mDDRBankCount;

    info->mNumClocks = numClocks(info->mName);

    mDev->sysfs_get<unsigned short>("mb_scheduler", "kds_numcdmas", errmsg, info->mNumCDMA, static_cast<unsigned short>(-1));

    //get sensors
    unsigned int m12VPex, m12VAux, mPexCurr, mAuxCurr, mDimmTemp_0, mDimmTemp_1, mDimmTemp_2, 
        mDimmTemp_3, mSE98Temp_0, mSE98Temp_1, mSE98Temp_2, mFanTemp, mFanRpm, m3v3Pex, m3v3Aux,
        mDDRVppBottom, mDDRVppTop, mSys5v5, m1v2Top, m1v8Top, m0v85, mMgt0v9, m12vSW, mMgtVtt,
        m1v2Bottom, mVccIntVol,mOnChipTemp;
    mDev->sysfs_get_sensor("xmc", "xmc_12v_pex_vol", m12VPex);
    mDev->sysfs_get_sensor("xmc", "xmc_12v_aux_vol", m12VAux);
    mDev->sysfs_get_sensor("xmc", "xmc_12v_pex_curr", mPexCurr);
    mDev->sysfs_get_sensor("xmc", "xmc_12v_aux_curr", mAuxCurr);
    mDev->sysfs_get_sensor("xmc", "xmc_dimm_temp0", mDimmTemp_0);
    mDev->sysfs_get_sensor("xmc", "xmc_dimm_temp1", mDimmTemp_1);
    mDev->sysfs_get_sensor("xmc", "xmc_dimm_temp2", mDimmTemp_2);
    mDev->sysfs_get_sensor("xmc", "xmc_dimm_temp3", mDimmTemp_3);
    mDev->sysfs_get_sensor("xmc", "xmc_se98_temp0", mSE98Temp_0);
    mDev->sysfs_get_sensor("xmc", "xmc_se98_temp1", mSE98Temp_1);
    mDev->sysfs_get_sensor("xmc", "xmc_se98_temp2", mSE98Temp_2);
    mDev->sysfs_get_sensor("xmc", "xmc_fan_temp", mFanTemp);
    mDev->sysfs_get_sensor("xmc", "xmc_fan_rpm", mFanRpm);
    mDev->sysfs_get_sensor("xmc", "xmc_3v3_pex_vol", m3v3Pex);
    mDev->sysfs_get_sensor("xmc", "xmc_3v3_aux_vol", m3v3Aux);
    mDev->sysfs_get_sensor("xmc", "xmc_ddr_vpp_btm", mDDRVppBottom);
    mDev->sysfs_get_sensor("xmc", "xmc_ddr_vpp_top", mDDRVppTop);
    mDev->sysfs_get_sensor("xmc", "xmc_sys_5v5", mSys5v5);
    mDev->sysfs_get_sensor("xmc", "xmc_1v2_top", m1v2Top);
    mDev->sysfs_get_sensor("xmc", "xmc_1v8", m1v8Top);
    mDev->sysfs_get_sensor("xmc", "xmc_0v85", m0v85);
    mDev->sysfs_get_sensor("xmc", "xmc_mgt0v9avcc", mMgt0v9);
    mDev->sysfs_get_sensor("xmc", "xmc_12v_sw", m12vSW);
    mDev->sysfs_get_sensor("xmc", "xmc_mgtavtt", mMgtVtt);
    mDev->sysfs_get_sensor("xmc", "xmc_vcc1v2_btm", m1v2Bottom);
    mDev->sysfs_get_sensor("xmc", "xmc_vccint_vol", mVccIntVol);
    mDev->sysfs_get_sensor("xmc", "xmc_fpga_temp", mOnChipTemp);
    info->m12VPex = m12VPex;
    info->m12VAux = m12VAux;
    info->mPexCurr = mPexCurr;
    info->mAuxCurr = mAuxCurr;
    info->mDimmTemp[0] = mDimmTemp_0;
    info->mDimmTemp[1] = mDimmTemp_1;
    info->mDimmTemp[2] = mDimmTemp_2;
    info->mDimmTemp[3] = mDimmTemp_3;
    info->mSE98Temp[0] = mSE98Temp_0;
    info->mSE98Temp[1] = mSE98Temp_1;
    info->mSE98Temp[2] = mSE98Temp_2;
    info->mFanTemp = mFanTemp;
    info->mFanRpm = mFanRpm;
    info->m3v3Pex = m3v3Pex;
    info->m3v3Aux = m3v3Aux;
    info->mDDRVppBottom = mDDRVppBottom;
    info->mDDRVppTop = mDDRVppTop;
    info->mSys5v5 = mSys5v5;
    info->m1v2Top = m1v2Top;
    info->m1v8Top = m1v8Top;
    info->m0v85 = m0v85;
    info->mMgt0v9 = mMgt0v9;
    info->m12vSW = m12vSW;
    info->mMgtVtt = mMgtVtt;
    info->m1v2Bottom = m1v2Bottom;
    info->mVccIntVol = mVccIntVol;
    info->mOnChipTemp = mOnChipTemp;

    //get sensors end

    mDev->sysfs_get("", "link_width", errmsg, info->mPCIeLinkWidth, static_cast<unsigned short>(-1));
    mDev->sysfs_get("", "link_speed", errmsg, info->mPCIeLinkSpeed, static_cast<unsigned short>(-1));
    mDev->sysfs_get("", "link_speed_max", errmsg, info->mPCIeLinkSpeedMax, static_cast<unsigned short>(-1));
    mDev->sysfs_get("", "link_width_max", errmsg, info->mPCIeLinkWidthMax, static_cast<unsigned short>(-1));
    mDev->sysfs_get("", "mig_calibration", errmsg, info->mMigCalib, false);
    std::vector<uint64_t> freqs;
    mDev->sysfs_get("icap", "clock_freqs", errmsg, freqs);
    for (unsigned i = 0;
        i < std::min(freqs.size(), ARRAY_SIZE(info->mOCLFrequency));
        i++) {
        info->mOCLFrequency[i] = freqs[i];
    }
}

/*
 * xclGetDeviceInfo2()
 */
int shim::xclGetDeviceInfo2(xclDeviceInfo2 *info)
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
int shim::resetDevice(xclResetKind kind)
{
    // Only XCL_USER_RESET is supported on user pf.
    if (kind != XCL_USER_RESET)
        return -EINVAL;

    std::string err;
    int dev_offline = 1;
    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_HOT_RESET);
    if (ret)
        return -errno;

    dev_fini();

    while (dev_offline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        pcidev::get_dev(mBoardNumber)->sysfs_get<int>("",
            "dev_offline", err, dev_offline, -1);
    }

    dev_init();

    return 0;
}

int shim::p2pEnable(bool enable, bool force)
{
    const std::string input = "1\n";
    std::string err;

    if (enable)
        mDev->sysfs_put("", "p2p_enable", err, "1");
    else
        mDev->sysfs_put("", "p2p_enable", err, "0");

    if (force) {
        dev_fini();
        /* remove root bus and rescan */
        mDev->sysfs_put("", "root_dev/remove", err, input);

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

    int p2p_enable = EINVAL;
    mDev->sysfs_get<int>("", "p2p_enable", err, p2p_enable, EINVAL);

    return p2p_enable;
}

/*
 * xclLockDevice()
 */
bool shim::xclLockDevice()
{
    if (!is_multiprocess_mode() &&
        mDev->flock(mUserHandle, LOCK_EX | LOCK_NB) == -1)
        return false;

    mLocked = true;
    return true;
}

/*
 * xclUnlockDevice()
 */
bool shim::xclUnlockDevice()
{
    if (!is_multiprocess_mode())
      mDev->flock(mUserHandle, LOCK_UN);

    mLocked = false;
    return true;
}

/*
 * xclReClock2()
 */
int shim::xclReClock2(unsigned short region, const unsigned short *targetFreqMHz)
{
    int ret;
    drm_xocl_reclock_info reClockInfo;
    std::memset(&reClockInfo, 0, sizeof(drm_xocl_reclock_info));
    reClockInfo.region = region;
    reClockInfo.ocl_target_freq[0] = targetFreqMHz[0];
    reClockInfo.ocl_target_freq[1] = targetFreqMHz[1];
    reClockInfo.ocl_target_freq[2] = targetFreqMHz[2];
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_RECLOCK, &reClockInfo);
    return ret ? -errno : ret;
}

/*
 * zeroOutDDR()
 */
bool shim::zeroOutDDR()
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
int shim::xclLoadXclBin(const xclBin *buffer)
{
    int ret = 0;
    const char *xclbininmemory = reinterpret_cast<char*> (const_cast<xclBin*> (buffer));

    ret = xclLoadAxlf(reinterpret_cast<const axlf*>(xclbininmemory));
    if (ret != 0) {
        if (ret == -EOPNOTSUPP) {
            xrt_logmsg(XRT_ERROR, "Xclbin does not match Shell on card.");
            xrt_logmsg(XRT_ERROR, "Use 'xbmgmt flash' to update Shell.");
        } else if (ret == -EBUSY) {
            xrt_logmsg(XRT_ERROR, "Xclbin on card is in use, can't change.");
        } else if (ret == -EKEYREJECTED) {
            xrt_logmsg(XRT_ERROR, "Xclbin isn't signed properly");
        } else if (ret == -ETIMEDOUT) {
            xrt_logmsg(XRT_ERROR,
                "Can't reach out to mgmt for xclbin downloading");
            xrt_logmsg(XRT_ERROR,
                "Is xclmgmt driver loaded? Or is MSD/MPD running?");
        }
        xrt_logmsg(XRT_ERROR, "See dmesg log for details. err=%d", ret);
    }

    mIsDebugIpLayoutRead = false;

    return ret;
}

/*
 * xclLoadAxlf()
 */
int shim::xclLoadAxlf(const axlf *buffer)
{
    xrt_logmsg(XRT_INFO, "%s, buffer: %s", __func__, buffer);

    if (!mLocked) {
        xrt_logmsg(XRT_ERROR, "%s: Device is not locked", __func__);
        return -EPERM;
    }

    drm_xocl_axlf axlf_obj = {const_cast<axlf *>(buffer)};
    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_READ_AXLF, &axlf_obj);
    if(ret)
        return -errno;

    // If it is an XPR DSA, zero out the DDR again as downloading the XCLBIN
    // reinitializes the DDR and results in ECC error.
    if(isXPR())
    {
        xrt_logmsg(XRT_INFO, "%s, XPR Device found, zeroing out DDR again..", __func__);

        if (zeroOutDDR() == false)
        {
            xrt_logmsg(XRT_ERROR, "%s, zeroing out DDR again..", __func__);
            return -EIO;
        }
    }
    return ret;
}

/*
 * xclExportBO()
 */
int shim::xclExportBO(unsigned int boHandle)
{
    drm_prime_handle info = {boHandle, 0, -1};
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_PRIME_HANDLE_TO_FD, &info);
    return !result ? info.fd : result;
}

/*
 * xclImportBO()
 */
unsigned int shim::xclImportBO(int fd, unsigned flags)
{
    drm_prime_handle info = {mNullBO, flags, fd};
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_PRIME_FD_TO_HANDLE, &info);
    if (result) {
        xrt_logmsg(XRT_ERROR, "%s: FD to handle IOCTL failed", __func__);
    }
    return !result ? info.handle : mNullBO;
}

/*
 * xclGetBOProperties()
 */
int shim::xclGetBOProperties(unsigned int boHandle, xclBOProperties *properties)
{
    drm_xocl_info_bo info = {boHandle, 0, mNullBO, mNullAddr};
    int result = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_INFO_BO, &info);
    properties->handle = info.handle;
    properties->flags  = info.flags;
    properties->size   = info.size;
    properties->paddr  = info.paddr;
    return result ? -errno : result;
}

int shim::xclGetSectionInfo(void* section_info, size_t * section_size,
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
        xrt_logmsg(XRT_ERROR, "%s: Unhandled section found", __func__);
        return -EINVAL;
    }

    std::string err;
    std::vector<char> buf;
    mDev->sysfs_get("icap", entry, err, buf);
    if (!err.empty()) {
        xrt_logmsg(XRT_ERROR, "%s: %s", __func__, err);
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
void shim::xclSysfsGetUsageInfo(drm_xocl_usage_stat& stat)
{
    std::string errmsg;
    std::vector<std::string> dmaStatStrs;
    std::vector<std::string> mmStatStrs;
    std::vector<std::string> xmcStatStrs;
    mDev->sysfs_get("dma", "channel_stat_raw", errmsg, dmaStatStrs);
    mDev->sysfs_get("", "memstat_raw", errmsg, mmStatStrs);
    mDev->sysfs_get("microblaze", "version", errmsg, xmcStatStrs);
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
int shim::xclGetUsageInfo(xclDeviceUsage *info)
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
bool shim::isGood() const
{
    return (mDev != nullptr);
}

/*
 * handleCheck()
 *
 * Returns pointer to valid handle on success, 0 on failure.
 */
shim *shim::handleCheck(void *handle)
{
    if (!handle) {
        return 0;
    }
    if (!((shim *) handle)->isGood()) {
        return 0;
    }
    return (shim *) handle;
}

/*
 * xclUnmgdPwrite()
 */
ssize_t shim::xclUnmgdPwrite(unsigned flags, const void *buf, size_t count, uint64_t offset)
{
    if (flags) {
        return -EINVAL;
    }
    drm_xocl_pwrite_unmgd unmgd = {0, 0, offset, count, reinterpret_cast<uint64_t>(buf)};
    return mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_PWRITE_UNMGD, &unmgd);
}

/*
 * xclUnmgdPread()
 */
ssize_t shim::xclUnmgdPread(unsigned flags, void *buf, size_t count, uint64_t offset)
{
    if (flags) {
        return -EINVAL;
    }
    drm_xocl_pread_unmgd unmgd = {0, 0, offset, count, reinterpret_cast<uint64_t>(buf)};
    return mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_PREAD_UNMGD, &unmgd);
}

/*
 * xclExecBuf()
 */
int shim::xclExecBuf(unsigned int cmdBO)
{
    int ret;
    xrt_logmsg(XRT_INFO, "%s, cmdBO: %d", __func__, cmdBO);
    drm_xocl_execbuf exec = {0, cmdBO, 0,0,0,0,0,0,0,0};
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_EXECBUF, &exec);
    return ret ? -errno : ret;
}

/*
 * xclExecBuf()
 */
int shim::xclExecBuf(unsigned int cmdBO, size_t num_bo_in_wait_list, unsigned int *bo_wait_list)
{
    xrt_logmsg(XRT_INFO, "%s, cmdBO: %d, num_bo_in_wait_list: %d, bo_wait_list: %d",
            __func__, cmdBO, num_bo_in_wait_list, bo_wait_list);
    int ret;
    unsigned int bwl[8] = {0};
    std::memcpy(bwl,bo_wait_list,num_bo_in_wait_list*sizeof(unsigned int));
    drm_xocl_execbuf exec = {0, cmdBO, bwl[0],bwl[1],bwl[2],bwl[3],bwl[4],bwl[5],bwl[6],bwl[7]};
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_EXECBUF, &exec);
    return ret ? -errno : ret;
}

/*
 * xclRegisterEventNotify()
 */
int shim::xclRegisterEventNotify(unsigned int userInterrupt, int fd)
{
    int ret ;
    drm_xocl_user_intr userIntr = {0, fd, (int)userInterrupt};
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_USER_INTR, &userIntr);
    return ret ? -errno : ret;
}

/*
 * xclExecWait()
 */
int shim::xclExecWait(int timeoutMilliSec)
{
    return mDev->poll(mUserHandle, POLLIN, timeoutMilliSec);
}

/*
 * xclOpenContext
 */
int shim::xclOpenContext(const uuid_t xclbinId, unsigned int ipIndex, bool shared) const
{
    unsigned int flags = shared ? XOCL_CTX_SHARED : XOCL_CTX_EXCLUSIVE;
    int ret;
    drm_xocl_ctx ctx = {XOCL_CTX_OP_ALLOC_CTX};
    std::memcpy(ctx.xclbin_id, xclbinId, sizeof(uuid_t));
    ctx.cu_index = ipIndex;
    ctx.flags = flags;
    ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_CTX, &ctx);
    return ret ? -errno : ret;
}

/*
 * xclCloseContext
 */
int shim::xclCloseContext(const uuid_t xclbinId, unsigned int ipIndex)
{
    std::lock_guard<std::mutex> l(mCuMapLock);

    if (ipIndex < mCuMaps.size()) {
	    // Make sure no MMIO register space access when CU is released.
	    uint32_t *p = mCuMaps[ipIndex];
	    if (p) {
		(void) munmap(p, mCuMapSize);
		mCuMaps[ipIndex] = nullptr;
	    }
    }

    drm_xocl_ctx ctx = {XOCL_CTX_OP_FREE_CTX};
    std::memcpy(ctx.xclbin_id, xclbinId, sizeof(uuid_t));
    ctx.cu_index = ipIndex;
    int ret = mDev->ioctl(mUserHandle, DRM_IOCTL_XOCL_CTX, &ctx);
    return ret ? -errno : ret;
}

/*
 * xclBootFPGA()
 */
int shim::xclBootFPGA()
{
    return -EOPNOTSUPP;
}

/*
 * xclCreateWriteQueue()
 */
int shim::xclCreateWriteQueue(xclQueueContext *q_ctx, uint64_t *q_hdl)
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
        xrt_logmsg(XRT_ERROR, "%s: Create Write Queue IOCTL failed", __func__);
    } else
        *q_hdl = q_info.handle;

     return rc ? -errno : rc;
}

/*
 * xclCreateReadQueue()
 */
int shim::xclCreateReadQueue(xclQueueContext *q_ctx, uint64_t *q_hdl)
{
    struct xocl_qdma_ioc_create_queue q_info;
    int rc;

    memset(&q_info, 0, sizeof (q_info));

    q_info.rid = q_ctx->route;
    q_info.flowid = q_ctx->flow;
    q_info.flags = q_ctx->flags;

    rc = ioctl(mStreamHandle, XOCL_QDMA_IOC_CREATE_QUEUE, &q_info);
    if (rc) {
        xrt_logmsg(XRT_ERROR, "%s: Create Read Queue IOCTL failed", __func__);
    } else
        *q_hdl = q_info.handle;

    return rc ? -errno : rc;
}

/*
 * xclDestroyQueue()
 */
int shim::xclDestroyQueue(uint64_t q_hdl)
{
    int rc;

    rc = close((int)q_hdl);
    if (rc)
        xrt_logmsg(XRT_ERROR, "%s: Destroy Queue failed", __func__);

    return rc;
}

/*
 * xclAllocQDMABuf()
 */
void *shim::xclAllocQDMABuf(size_t size, uint64_t *buf_hdl)
{
    struct xocl_qdma_ioc_alloc_buf req;
    void *buf;
    int rc;

    memset(&req, 0, sizeof (req));
    req.size = size;

    rc = ioctl(mStreamHandle, XOCL_QDMA_IOC_ALLOC_BUFFER, &req);
    if (rc) {
        xrt_logmsg(XRT_ERROR, "%s: Alloc buffer IOCTL failed", __func__);
        return NULL;
    }

    buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, req.buf_fd, 0);
    if (!buf) {
        xrt_logmsg(XRT_ERROR, "%s: Map buffer failed", __func__);

        close(req.buf_fd);
    } else {
        *buf_hdl = req.buf_fd;
    }

    return buf;
}

/*
 * xclFreeQDMABuf()
 */
int shim::xclFreeQDMABuf(uint64_t buf_hdl)
{
    int rc;

    rc = close((int)buf_hdl);
    if (rc)
        xrt_logmsg(XRT_ERROR, "%s: Destory Queue failed", __func__);


    return rc;
}

/*
 * xclPollCompletion()
 */
int shim::xclPollCompletion(int min_compl, int max_compl, struct xclReqCompletion *comps, int* actual, int timeout /*ms*/)
{
    /* TODO: populate actual and timeout args correctly */
    struct timespec time, *ptime = NULL;
    int num_evt, i;

    *actual = 0;
    if (!mAioEnabled) {
        num_evt = -EINVAL;
        xrt_logmsg(XRT_ERROR, "%s: async io is not enabled", __func__);
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
        xrt_logmsg(XRT_ERROR, "%s: failed to poll Queue Completions", __func__);
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
ssize_t shim::xclWriteQueue(uint64_t q_hdl, xclQueueRequest *wr)
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
                xrt_logmsg(XRT_ERROR, "%s: async io is not enabled", __func__);
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
ssize_t shim::xclReadQueue(uint64_t q_hdl, xclQueueRequest *wr)
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
                xrt_logmsg(XRT_ERROR, "%s: async io is not enabled", __func__);
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

uint32_t shim::xclGetNumLiveProcesses()
{
    std::string errmsg;

    std::vector<std::string> stringVec;
    mDev->sysfs_get("", "kdsstat", errmsg, stringVec);
    // Dependent on message format built in kdsstat_show. Checking number of
    // "context" in kdsstat.
    // kdsstat has "context: <number_of_live_processes>"
    if(stringVec.size() >= 4) {
        auto p = stringVec[3].find_first_of("0123456789");
        auto subStr = stringVec[3].substr(p);
        auto number = std::stoul(subStr);
        return number;
    }
    return 0;
}

int shim::xclGetDebugIPlayoutPath(char* layoutPath, size_t size)
{
  return xclGetSysfsPath("icap", "debug_ip_layout", layoutPath, size);
}

int shim::xclGetTraceBufferInfo(uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  uint32_t bytesPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 8);

  traceBufSz = MAX_TRACE_NUMBER_SAMPLES * bytesPerSample;   /* Buffer size in bytes */
  traceSamples = nSamples;

  return 0;
}

int shim::xclReadTraceData(void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
    // Create trace buffer on host (requires alignment)
    const int traceBufWordSz = traceBufSz / 4;  // traceBufSz is in number of bytes

    uint32_t size = 0;

    wordsPerSample = (XPAR_AXI_PERF_MON_0_TRACE_WORD_WIDTH / 32);
    uint32_t numWords = numSamples * wordsPerSample;

//    alignas is defined in c++11
#if GCC_VERSION >= 40800
    /* Alignment is limited to 16 by PPC64LE : so , should it be
    alignas(16) uint32_t hostbuf[traceBufSzInWords];
    */
    alignas(AXI_FIFO_RDFD_AXI_FULL) uint32_t hostbuf[traceBufWordSz];
#else
    xrt_core::AlignedAllocator<uint32_t> alignedBuffer(AXI_FIFO_RDFD_AXI_FULL, traceBufWordSz);
    uint32_t* hostbuf = alignedBuffer.getBuffer();
#endif

    // Now read trace data
    memset((void *)hostbuf, 0, traceBufSz);

    // Iterate over chunks
    // NOTE: AXI limits this to 4K bytes per transfer
    uint32_t chunkSizeWords = 256 * wordsPerSample;
    if (chunkSizeWords > 1024) chunkSizeWords = 1024;
    uint32_t chunkSizeBytes = 4 * chunkSizeWords;
    uint32_t words=0;

    // Read trace a chunk of bytes at a time
    if (numWords > chunkSizeWords) {
      for (; words < (numWords-chunkSizeWords); words += chunkSizeWords) {
          if(mLogStream.is_open())
            mLogStream << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
                          << std::hex << (ipBaseAddress + AXI_FIFO_RDFD_AXI_FULL) /*fifoReadAddress[0] or AXI_FIFO_RDFD*/ << " and writing it to 0x"
                          << (void *)(hostbuf + words) << std::dec << std::endl;

        xclUnmgdPread(0 /*flags*/, (void *)(hostbuf + words) /*buf*/, chunkSizeBytes /*count*/, ipBaseAddress + AXI_FIFO_RDFD_AXI_FULL /*offset : or AXI_FIFO_RDFD*/);

        size += chunkSizeBytes;
      }
    }

    // Read remainder of trace not divisible by chunk size
    if (words < numWords) {
      chunkSizeBytes = 4 * (numWords - words);

      if(mLogStream.is_open()) {
        mLogStream << __func__ << ": reading " << chunkSizeBytes << " bytes from 0x"
                      << std::hex << (ipBaseAddress + AXI_FIFO_RDFD_AXI_FULL) /*fifoReadAddress[0]*/ << " and writing it to 0x"
                      << (void *)(hostbuf + words) << std::dec << std::endl;
      }

      xclUnmgdPread(0 /*flags*/, (void *)(hostbuf + words) /*buf*/, chunkSizeBytes /*count*/, ipBaseAddress + AXI_FIFO_RDFD_AXI_FULL /*offset : or AXI_FIFO_RDFD*/);

      size += chunkSizeBytes;
    }

    if(mLogStream.is_open())
        mLogStream << __func__ << ": done reading " << size << " bytes " << std::endl;

    memcpy((char*)traceBuf, (char*)hostbuf, traceBufSz);

    return size;
}


int shim::xclRegRW(bool rd, uint32_t cu_index, uint32_t offset, uint32_t *datap)
{
    std::lock_guard<std::mutex> l(mCuMapLock);

    if (cu_index >= mCuMaps.size()) {
        xrt_logmsg(XRT_ERROR, "%s: invalid CU index: %d", __func__, cu_index);
        return -EINVAL;
    }
    if (offset >= mCuMapSize || (offset & (sizeof(uint32_t) - 1)) != 0) {
        xrt_logmsg(XRT_ERROR, "%s: invalid CU offset: %d", __func__, offset);
        return -EINVAL;
    }

    if (mCuMaps[cu_index] == nullptr) {
        void *p = mDev->mmap(mUserHandle, mCuMapSize,
            PROT_READ | PROT_WRITE, MAP_SHARED, (cu_index + 1) * getpagesize());
        if (p != MAP_FAILED)
            mCuMaps[cu_index] = (uint32_t *)p;
    }

    uint32_t *cumap = mCuMaps[cu_index];
    if (cumap == nullptr) {
        xrt_logmsg(XRT_ERROR, "%s: can't map CU: %d", __func__, cu_index);
        return -EINVAL;
    }

    if (rd)
        *datap = cumap[offset / sizeof(uint32_t)];
    else
        cumap[offset / sizeof(uint32_t)] = *datap;
    return 0;
}

int shim::xclRegRead(uint32_t cu_index, uint32_t offset, uint32_t *datap)
{
    return xclRegRW(true, cu_index, offset, datap);
}

int shim::xclRegWrite(uint32_t cu_index, uint32_t offset, uint32_t data)
{
    return xclRegRW(false, cu_index, offset, &data);
}

int shim::xclCuName2Index(const char *name, uint32_t& index)
{
    std::string errmsg;
    std::vector<char> buf;
    const uint64_t bad_addr = 0xffffffffffffffff;

    mDev->sysfs_get("icap", "ip_layout", errmsg, buf);
    if (!errmsg.empty()) {
        xrt_logmsg(XRT_ERROR, "can't read ip_layout sysfs node: %s",
            errmsg.c_str());
        return -EINVAL;
    }
    if (buf.empty())
        return -ENOENT;

    const ip_layout *map = (ip_layout *)buf.data();
    if(map->m_count < 0) {
        xrt_logmsg(XRT_ERROR, "invalid ip_layout sysfs node content");
        return -EINVAL;
    }

    uint64_t addr = bad_addr;
    int i;
    for(i = 0; i < map->m_count; i++) {
        if (strncmp((char *)map->m_ip_data[i].m_name, name,
                sizeof(map->m_ip_data[i].m_name)) == 0) {
            addr = map->m_ip_data[i].m_base_address;
            break;
        }
    }
    if (i == map->m_count)
        return -ENOENT;
    if (addr == bad_addr)
        return -EINVAL;

    auto cus = xrt_core::xclbin::get_cus(map);
    auto itr = std::find(cus.begin(), cus.end(), addr);
    if (itr == cus.end())
      return -ENOENT;

    index = std::distance(cus.begin(),itr);
    return 0;
}

} // namespace xocl

/*******************************/
/* GLOBAL DECLARATIONS *********/
/*******************************/

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

    xocl::shim *handle = new xocl::shim(deviceIndex, logFileName, level);

    return static_cast<xclDeviceHandle>(handle);
}

void xclClose(xclDeviceHandle handle)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (drv) {
        delete drv;
        return;
    }
}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    auto ret = drv ? drv->xclLoadXclBin(buffer) : -ENODEV;
    if (!ret) {
      ret = xrt_core::scheduler::init(handle, buffer);
      START_DEVICE_PROFILING_CB(handle);
    }
    if (!ret && xrt_core::config::get_ert() &&
      (xclbin::get_axlf_section(buffer, PDI) ||
      xclbin::get_axlf_section(buffer, BITSTREAM_PARTIAL_PDI)) &&
      xrt_core::config::get_pdi_load())
        ret = xrt_core::scheduler::loadXclbinToPS(handle, buffer);
    return ret;
}

int xclLogMsg(xclDeviceHandle handle, xrtLogMsgLevel level, const char* tag, const char* format, ...)
{
    static auto verbosity = xrt_core::config::get_verbosity();
    if (level <= verbosity) {
        va_list args;
        va_start(args, format);
        int ret = -1;
        if (handle) {
            xocl::shim *drv = xocl::shim::handleCheck(handle);
            ret = drv ? drv->xclLogMsg(level, tag, format, args) : -ENODEV;
        } else {
            ret = xocl::shim::xclLogMsg(level, tag, format, args);
        }
        va_end(args);

        return ret;
    }

    return 0;
}


size_t xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
//    WRITE_CB;
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclWrite(space, offset, hostBuf, size) : -ENODEV;
}

size_t xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
//    READ_CB;
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclRead(space, offset, hostBuf, size) : -ENODEV;
}

int xclRegWrite(xclDeviceHandle handle, uint32_t cu_index, uint32_t offset, uint32_t data)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclRegWrite(cu_index, offset, data) : -ENODEV;
}

int xclRegRead(xclDeviceHandle handle, uint32_t cu_index, uint32_t offset, uint32_t *datap)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclRegRead(cu_index, offset, datap) : -ENODEV;
}

int xclGetErrorStatus(xclDeviceHandle handle, xclErrorStatus *info)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    std::memset(info, 0, sizeof(xclErrorStatus));
    if(!drv)
        return 0;
    return drv->xclGetErrorStatus(info);
}

int xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
    xocl::shim *drv = (xocl::shim *) handle;
    return drv ? drv->xclGetDeviceInfo2(info) : -ENODEV;
}

unsigned int xclVersion ()
{
    return 2;
}

unsigned int xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned flags)
{
//    ALLOC_BO_CB;
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclAllocBO(size, unused, flags) : -ENODEV;
}

unsigned int xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned flags)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclAllocUserPtrBO(userptr, size, flags) : -ENODEV;
}

void xclFreeBO(xclDeviceHandle handle, unsigned int boHandle) {
//    FREE_BO_CB;
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (!drv) {
        return;
    }
    drv->xclFreeBO(boHandle);
}

size_t xclWriteBO(xclDeviceHandle handle, unsigned int boHandle, const void *src, size_t size, size_t seek)
{
//    WRITE_BO_CB;
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclWriteBO(boHandle, src, size, seek) : -ENODEV;
}

size_t xclReadBO(xclDeviceHandle handle, unsigned int boHandle, void *dst, size_t size, size_t skip)
{
//    READ_BO_CB;
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclReadBO(boHandle, dst, size, skip) : -ENODEV;
}

void *xclMapBO(xclDeviceHandle handle, unsigned int boHandle, bool write)
{
//    MAP_BO_CB;
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclMapBO(boHandle, write) : nullptr;
}

int xclUnmapBO(xclDeviceHandle handle, unsigned int boHandle, void* addr)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclUnmapBO(boHandle, addr) : -ENODEV;
}

int xclSyncBO(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclSyncBO(boHandle, dir, size, offset) : -ENODEV;
}

int xclCopyBO(xclDeviceHandle handle, unsigned int dst_boHandle,
            unsigned int src_boHandle, size_t size, size_t dst_offset, size_t src_offset)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ?
      drv->xclCopyBO(dst_boHandle, src_boHandle, size, dst_offset, src_offset) : -ENODEV;
}

int xclReClock2(xclDeviceHandle handle, unsigned short region, const unsigned short *targetFreqMHz)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclReClock2(region, targetFreqMHz) : -ENODEV;
}

int xclLockDevice(xclDeviceHandle handle)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (!drv)
        return -ENODEV;
    return drv->xclLockDevice() ? 0 : 1;
}

int xclUnlockDevice(xclDeviceHandle handle)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (!drv)
        return -ENODEV;
    return drv->xclUnlockDevice() ? 0 : 1;
}

int xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->resetDevice(kind) : -ENODEV;
}

int xclP2pEnable(xclDeviceHandle handle, bool enable, bool force)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->p2pEnable(enable, force) : -ENODEV;
}

int xclBootFPGA(xclDeviceHandle handle)
{
    // Not doable from user side. Can be added to xbmgmt later.
    return -EOPNOTSUPP;
}

int xclExportBO(xclDeviceHandle handle, unsigned int boHandle)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclExportBO(boHandle) : -ENODEV;
}

unsigned int xclImportBO(xclDeviceHandle handle, int fd, unsigned flags)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    if (!drv) {
        std::cout << __func__ << ", " << std::this_thread::get_id() << ", handle & XOCL Device are bad" << std::endl;
    }
    return drv ? drv->xclImportBO(fd, flags) : -ENODEV;
}

ssize_t xclUnmgdPwrite(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset)
{
//    UNMGD_PWRITE_CB;
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclUnmgdPwrite(flags, buf, count, offset) : -ENODEV;
}

ssize_t xclUnmgdPread(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset)
{
//    UNMGD_PREAD_CB;
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclUnmgdPread(flags, buf, count, offset) : -ENODEV;
}

int xclGetBOProperties(xclDeviceHandle handle, unsigned int boHandle, xclBOProperties *properties)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclGetBOProperties(boHandle, properties) : -ENODEV;
}

int xclGetUsageInfo(xclDeviceHandle handle, xclDeviceUsage *info)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclGetUsageInfo(info) : -ENODEV;
}

int xclGetSectionInfo(xclDeviceHandle handle, void* section_info, size_t * section_size,
    enum axlf_section_kind kind, int index)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclGetSectionInfo(section_info, section_size, kind, index) : -ENODEV;
}

int xclExecBuf(xclDeviceHandle handle, unsigned int cmdBO)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclExecBuf(cmdBO) : -ENODEV;
}

int xclExecBufWithWaitList(xclDeviceHandle handle, unsigned int cmdBO, size_t num_bo_in_wait_list, unsigned int *bo_wait_list)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclExecBuf(cmdBO,num_bo_in_wait_list,bo_wait_list) : -ENODEV;
}

int xclRegisterEventNotify(xclDeviceHandle handle, unsigned int userInterrupt, int fd)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclRegisterEventNotify(userInterrupt, fd) : -ENODEV;
}

int xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclExecWait(timeoutMilliSec) : -ENODEV;
}

int xclOpenContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned int ipIndex, bool shared)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclOpenContext(xclbinId, ipIndex, shared) : -ENODEV;
}

int xclCloseContext(xclDeviceHandle handle, uuid_t xclbinId, unsigned ipIndex)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclCloseContext(xclbinId, ipIndex) : -ENODEV;
}

const axlf_section_header* wrap_get_axlf_section(const axlf* top, axlf_section_kind kind)
{
    return xclbin::get_axlf_section(top, kind);
}

// QDMA streaming APIs
int xclCreateWriteQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclCreateWriteQueue(q_ctx, q_hdl) : -ENODEV;
}

int xclCreateReadQueue(xclDeviceHandle handle, xclQueueContext *q_ctx, uint64_t *q_hdl)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclCreateReadQueue(q_ctx, q_hdl) : -ENODEV;
}

int xclDestroyQueue(xclDeviceHandle handle, uint64_t q_hdl)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclDestroyQueue(q_hdl) : -ENODEV;
}

void *xclAllocQDMABuf(xclDeviceHandle handle, size_t size, uint64_t *buf_hdl)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclAllocQDMABuf(size, buf_hdl) : NULL;
}

int xclFreeQDMABuf(xclDeviceHandle handle, uint64_t buf_hdl)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclFreeQDMABuf(buf_hdl) : -ENODEV;
}

ssize_t xclWriteQueue(xclDeviceHandle handle, uint64_t q_hdl, xclQueueRequest *wr)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclWriteQueue(q_hdl, wr) : -ENODEV;
}

ssize_t xclReadQueue(xclDeviceHandle handle, uint64_t q_hdl, xclQueueRequest *wr)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclReadQueue(q_hdl, wr) : -ENODEV;
}

int xclPollCompletion(xclDeviceHandle handle, int min_compl, int max_compl, xclReqCompletion *comps, int* actual, int timeout)
{
        xocl::shim *drv = xocl::shim::handleCheck(handle);
        return drv ? drv->xclPollCompletion(min_compl, max_compl, comps, actual, timeout) : -ENODEV;
}

uint xclGetNumLiveProcesses(xclDeviceHandle handle)
{
    xocl::shim *drv = xocl::shim::handleCheck(handle);
    return drv ? drv->xclGetNumLiveProcesses() : 0;
}

int xclGetDebugIPlayoutPath(xclDeviceHandle handle, char* layoutPath, size_t size)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return drv ? drv->xclGetDebugIPlayoutPath(layoutPath, size) : -ENODEV;
}

int xclGetTraceBufferInfo(xclDeviceHandle handle, uint32_t nSamples, uint32_t& traceSamples, uint32_t& traceBufSz)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return (drv) ? drv->xclGetTraceBufferInfo(nSamples, traceSamples, traceBufSz) : -ENODEV;
}

int xclReadTraceData(xclDeviceHandle handle, void* traceBuf, uint32_t traceBufSz, uint32_t numSamples, uint64_t ipBaseAddress, uint32_t& wordsPerSample)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return (drv) ? drv->xclReadTraceData(traceBuf, traceBufSz, numSamples, ipBaseAddress, wordsPerSample) : -ENODEV;
}

int xclCreateProfileResults(xclDeviceHandle handle, ProfileResults** results)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  if(!drv)
    return -ENODEV;

  int status = -1;
  CREATE_PROFILE_RESULTS_CB(handle, results, status);
  return status;
}

int xclGetProfileResults(xclDeviceHandle handle, ProfileResults* results)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  if(!drv)
    return -ENODEV;

  int status = -1;
  GET_PROFILE_RESULTS_CB(handle, results, status);
  return status;
}

int xclDestroyProfileResults(xclDeviceHandle handle, ProfileResults* results)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  if(!drv)
    return -ENODEV;

  int status = -1;
  DESTROY_PROFILE_RESULTS_CB(handle, results, status);
  return status;
}

int xclCuName2Index(xclDeviceHandle handle, const char *name, uint32_t *indexp)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return (drv) ? drv->xclCuName2Index(name, *indexp) : -ENODEV;
}

int xclUpdateSchedulerStat(xclDeviceHandle handle)
{
  xocl::shim *drv = xocl::shim::handleCheck(handle);
  return (drv) ? drv->xclUpdateSchedulerStat() : -ENODEV;
}
