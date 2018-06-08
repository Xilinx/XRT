/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

/**
 * Copyright (C) 2015 Xilinx, Inc
 * Author: Sonal Santan
 * CAPI HAL Driver layered on top of CAPI kernel driver
 */

#include "shim.h"
#include "libcxl.h"

/*
 * Define GCC version macro so we can use newer C++11 features
 * if possible
 */
#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)

#include <sys/types.h>

#ifndef _WINDOWS
// TODO: Windows build support
//    sys/mman.h is linux only header file
//    it is included for mmap
#include <sys/mman.h>
#endif

#ifndef _WINDOWS
// TODO: Windows build support
//    unistd.h is linux only header file
//    it is included for read, write, close, lseek64
#include <unistd.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>

#ifndef _WINDOWS
// TODO: Windows build support
//    sys/ioctl.h is linux only header file
//    it is included for ioctl
#include <sys/ioctl.h>
#endif

#ifndef _WINDOWS
// TODO: Windows build support
//    sys/file.h is linux only header file
//    it is included for flock
#include <sys/file.h>
#endif


#include <cstdio>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <thread>
#include <iostream>
#include <string>

#include "xclbin.h"

#ifdef _WINDOWS
#define __func__ __FUNCTION__
#endif

#ifdef _WINDOWS
#define MAP_FAILED (void *)-1
#endif

#define MMIO_STOP_AFU         0x0000008  //0X0000002

namespace xclcapi {
    const unsigned CAPIShim::TAG = 0X586C0C6C; // XL OpenCL X->58(ASCII), L->6C(ASCII), O->0 C->C L->6C(ASCII);

    void CAPIShim::exitAFU() {
      if(mAFU == nullptr)
        return;
      cxl_mmio_write64(mAFU, MMIO_STOP_AFU, 1); //stop and reset afu
      cxl_mmio_unmap (mAFU);
      cxl_afu_free (mAFU);
    }

    int CAPIShim::initAFU() {
/*
        mAFU = cxl_adapter_afu_next(mAdapter, 0);
        if (!mAFU)
            return -1;

        mAFU = cxl_afu_open_h(mAFU, CXL_VIEW_DEDICATED);
        if (!mAFU)
            return -1;
*/
        mAFU = cxl_afu_open_dev((char *)"/dev/cxl/afu0.0d");
        if (!mAFU)
            return -1;

        if (cxl_afu_attach(mAFU, (__u64)mWed)) {
            cxl_afu_free(mAFU);
            mAFU = 0;
            return -1;
        }

        if (cxl_mmio_map(mAFU, CXL_MMIO_BIG_ENDIAN)) {
            cxl_afu_free(mAFU);
            mAFU = 0;
            return -1;
        }

        long val;
        if (cxl_get_mmio_size(mAFU, &val)) {
            cxl_afu_free(mAFU);
            mAFU = 0;
            return -1;
        }

        mMmioSize = (unsigned long)val;
        return 0;
    }

//    int CAPIShim::xclLoadBitstream(const char *fileName) {
//        if (mLogStream.is_open()) {
//            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << fileName << std::endl;
//        }
//
//        std::ifstream stream(fileName);
//        if (!stream.is_open()) {
//            return -1;
//        }
//
//        stream.seekg(0, stream.end);
//        int length = stream.tellg();
//        stream.seekg(0, stream.beg);
//        char *buffer = new char[length];
//        stream.read(buffer, length);
//        stream.close();
//        xclBin *header = (xclBin *)buffer;
//        if (std::memcmp(header->m_magic, "xclbin0", 8)) {
//            return -EINVAL;
//        }
//
//        return xclLoadXclBin(header);
//    }


    int CAPIShim::xclLoadXclBin(const axlf* buffer)
    {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << buffer << std::endl;
	    mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << " : Old xclbin0 is not supported "  << std::endl;
        }
	return -EINVAL;

//        char binFile[32];
//        std::strcpy(binFile, "/tmp/sdaccel-XXXXXX");
//        int handle = mkstemp(binFile);
//        const void *buf = (const char *)buffer + buffer->m_primaryFirmwareOffset;
//        int result = 0;
//        if (write(handle, buf, buffer->m_primaryFirmwareLength) != (ssize_t)buffer->m_primaryFirmwareLength)
//            result = errno;
//        close(handle);
//        if (result) {
//            unlink(binFile);
//            return result;
//        }
//        std::string cmd("capi_flash_xilinx_ad");
//        cmd += " ";
//        cmd += binFile;
//        cmd += " ";
//        cmd += std::to_string(mBoardNumber);
//        result = std::system(cmd.c_str());
//        unlink(binFile);
//        if (result)
//            return result;
//        sleep(5);
//        return resetDevice(XCL_RESET_FULL);
    }

    int CAPIShim::xclLoadBitstreamWorker(const char *fileName) {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << fileName << std::endl;
        }

        std::ifstream stream(fileName);
        if (!stream.is_open()) {
            return errno;
        }

        stream.seekg(0, stream.end);
        int length = stream.tellg();
        stream.seekg(0, stream.beg);
        char *buffer = new char[length];
        stream.read(buffer, length);
        stream.close();
        xclBin *header = (xclBin *)buffer;
        if (std::memcmp(header->m_magic, "xclbin0", 8)) {
            return -EINVAL;
        } else {
	    if (mLogStream.is_open()) {
		mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << fileName << " : Old xclbin0 is not supported "  << std::endl;
	    }

	    return -EINVAL;
	}

        return 0;
    }

    size_t CAPIShim::xclWrite(xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size) {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << space << ", "
                       << offset << ", " << hostBuf << ", " << size << std::endl;
        }
        switch (space) {
        case XCL_ADDR_SPACE_DEVICE_RAM:
        {
            return xclCopyBufferHost2Device(offset, hostBuf, size, 0);
        }
        case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
            return -1;
        }
        case XCL_ADDR_KERNEL_CTRL:
        {
            if (pcieBarWrite(offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        default:
        {
            return -1;
        }
        }
    }

    size_t CAPIShim::xclRead(xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size) {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << space << ", "
                       << offset << ", " << hostBuf << ", " << size << std::endl;
        }
        switch (space) {
        case XCL_ADDR_SPACE_DEVICE_RAM:
        {
            return xclCopyBufferDevice2Host(hostBuf, offset, size, 0);
        }
        case XCL_ADDR_SPACE_DEVICE_PERFMON:
        {
            return -1;
        }
        case XCL_ADDR_SPACE_DEVICE_CHECKER:
        {
            return -1;
        }
        case XCL_ADDR_KERNEL_CTRL:
        {
            if (pcieBarRead(offset, hostBuf, size) == 0) {
                return size;
            }
            return -1;
        }
        default:
        {
            return -1;
        }
        }
    }

    uint64_t CAPIShim::xclAllocDeviceBuffer(size_t size) {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << size << std::endl;
        }

        if (size == 0)
            size = BUFFER_ALIGNMENT;

        uint64_t result = ~0;
        if (posix_memalign(reinterpret_cast<void **>(&result), BUFFER_ALIGNMENT, size)) {
            return result;
        }
        std::lock_guard<std::mutex> lock(mMemManagerMutex);
        mBusyBufferList.push_back(std::make_pair(result, size));
        return result;
    }


    void CAPIShim::xclFreeDeviceBuffer(uint64_t buf) {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << buf << std::endl;
        }

        std::lock_guard<std::mutex> lock(mMemManagerMutex);
#if GCC_VERSION >= 40800
        PairList::iterator i = std::find_if(mBusyBufferList.begin(), mBusyBufferList.end(), [&] (const PairList::value_type& s)
                                            { return s.first == buf; });
#else
        PairList::iterator first = mBusyBufferList.begin();
        PairList::iterator last  = mBusyBufferList.end();
        while(first != last) {
            if (first->first == buf)
                break;
            ++first;
        }
        PairList::iterator i = first;
#endif
        //assert(i != mBusyBufferList.end());
        if (i == mBusyBufferList.end())
            return;
        mBusyBufferList.erase(i);
        std::free(reinterpret_cast<void *>(buf));
    }


    size_t CAPIShim::xclCopyBufferHost2Device(uint64_t dest, const void *src, size_t size, size_t seek) {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
                       << src << ", " << size << ", " << seek << std::endl;
        }
#ifdef DEBUG
        {
            // Ensure that this buffer was allocated by memory manager before
            std::lock_guard<std::mutex> lock(mMemManagerMutex);
#if GCC_VERSION >= 40800
            PairList::iterator i = std::find_if(mBusyBufferList.begin(), mBusyBufferList.end(), [&] (const PairList::value_type& s)
                                                { return s.first == dest; });
#else
            PairList::iterator first = mBusyBufferList.begin();
            PairList::iterator last  = mBusyBufferList.end();

            while(first != last) {
                if (first->first == dest)
                    break;
                ++first;
            }
            PairList::iterator i = first;
#endif
            assert(i != mBusyBufferList.end());
            if (i == mBusyBufferList.end())
                return -1;
            if (i->second < (size + seek))
                return -1;
        }
#endif
        dest += seek;
        std::memcpy((void *)dest, src, size);
        return size;
    }


    size_t CAPIShim::xclCopyBufferDevice2Host(void *dest, uint64_t src, size_t size, size_t skip) {
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << ", " << dest << ", "
                       << src << ", " << size << ", " << skip << std::endl;
        }
#ifdef DEBUG
        {
            // Ensure that this buffer was allocated by memory manager before
            std::lock_guard<std::mutex> lock(mMemManagerMutex);
#if GCC_VERSION >= 40800
            PairList::iterator i = std::find_if(mBusyBufferList.begin(), mBusyBufferList.end(), [&] (const PairList::value_type& s)
                                                { return s.first == src; });
#else
            PairList::iterator first = mBusyBufferList.begin();
            PairList::iterator last  = mBusyBufferList.end();

            while(first != last) {
                if (first->first == src)
                    break;
                ++first;
            }
            PairList::iterator i = first;
#endif
            assert(i != mBusyBufferList.end());
            if (i == mBusyBufferList.end())
                return -1;
            if (i->second < (size + skip))
                return -1;
        }
#endif
        src += skip;
        std::memcpy(dest, (void *)src, size);
        return size;
    }


    CAPIShim *CAPIShim::handleCheck(void *handle) {
        // Sanity checks
        if (!handle)
            return 0;
        if (*(unsigned *)handle != TAG)
            return 0;
        if (!((CAPIShim *)handle)->isGood()) {
            return 0;
        }

        return (CAPIShim *)handle;
    }

    unsigned CAPIShim::xclProbe() {
        unsigned count = 0;
        for (cxl_adapter_h *adapter = cxl_adapter_next(0); adapter; adapter = cxl_adapter_next(adapter)) {
            count++;
        }
        return count;
    }


    unsigned CAPIShim::getTAG() const {
        return mTag;
    }

    CAPIShim::~CAPIShim() {

        //cxl_afu_free(mAFU);
        exitAFU();
        mAFU = 0;
        cxl_adapter_free(mAdapter);
        mAdapter = 0;
        for (PairList::iterator i = mBusyBufferList.begin(), e = mBusyBufferList.end(); i != e; ++i) {
            std::free(reinterpret_cast<void *>(i->first));
        }
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
            mLogStream.close();
        }
    }

    CAPIShim::CAPIShim(unsigned index, const char *logfileName,
                       xclVerbosityLevel verbosity) : mTag(TAG), mBoardNumber(index),
                                                      mOffsets{0x0, 0x0, 0x0, 0x0},
                                                      mDSAMajorVersion(DSA_MAJOR_VERSION),
                                                      mDSAMinorVersion(DSA_MINOR_VERSION)
    {
        if (logfileName && (logfileName[0] != '\0')) {
            mLogStream.open(logfileName);
            mLogStream << "FUNCTION, THREAD ID, ARG..."  << std::endl;
            mLogStream << __func__ << ", " << std::this_thread::get_id() << std::endl;
        }

        unsigned count = 0;
        cxl_adapter_h *prev = 0;
        for (mAdapter = cxl_adapter_next(0); mAdapter; mAdapter = cxl_adapter_next(prev)) {
            cxl_adapter_free(prev);
            if (count == index)
                break;
            count++;
            prev = mAdapter;
        }

        if (!mAdapter)
            return;

        if (posix_memalign ((void **) &mWed, BUFFER_ALIGNMENT, BUFFER_ALIGNMENT)) {
            cxl_adapter_free(mAdapter);
            mAdapter = 0;
            return;
        }

        initAFU();
    }

    bool CAPIShim::isGood() const {
        if (!mAdapter || !mAFU)
            return false;
        // TODO: Add sanity check for card state
        return true;
    }

    int CAPIShim::pcieBarRead(unsigned long long offset, void* buffer, unsigned long long length) {

        if ((length + offset) > mMmioSize)
                return -1;

        char *qBuf = (char *)buffer;
	bool isOffsetZeroSkipped = false;

        while (length > 8)
	{
	    if(offset == 0) {
		isOffsetZeroSkipped = true;
		offset += 8;
		qBuf += 8;
		length -= 8;
		continue;
	    }
            uint64_t val;
            uint64_t low = 0;
            uint64_t high = 0;
            cxl_mmio_read64(mAFU, offset * 4 + 16, &high);
            cxl_mmio_read64(mAFU, offset * 4, &low);
            val = low | high << 32;
            *(uint64_t *)qBuf = val;
            offset += 8;
            qBuf += 8;
            length -= 8;
        }
        if (length) {
	    //Offset zero case taken care of by reversing the order of reads.
            uint64_t val;
            uint64_t low = 0;
            uint64_t high = 0;
            cxl_mmio_read64(mAFU, offset * 4 + 16, &high);
            cxl_mmio_read64(mAFU, offset * 4, &low);
            val = low | high << 32;
            std::memcpy(qBuf, (char *)&val, length);
        }

	if(isOffsetZeroSkipped) {
	    //Will only come here if original length was >=8 and offset was zero
            uint64_t val;
            uint64_t low = 0;
            uint64_t high = 0;
            cxl_mmio_read64(mAFU, offset * 4 + 16, &high);
            cxl_mmio_read64(mAFU, offset * 4, &low);
            val = low | high << 32;
            *(uint64_t *)buffer = val;
	}
        return 0;
    }

    int CAPIShim::pcieBarWrite(unsigned long long offset, const void* buffer, unsigned long long length) {
        if ((length + offset) > mMmioSize)
                return -1;

        char *qBuf = (char *)buffer;
        while (length >= 8) {
            uint64_t val = *(uint64_t *)qBuf;
            cxl_mmio_write64(mAFU, offset * 4, val);
            cxl_mmio_write64(mAFU, offset * 4 + 16, (uint64_t)val >> 32);
            offset += 8;
            qBuf += 8;
            length -= 8;
        }
        if (length) {
            // Read-Modify-Write
            uint64_t val;
            uint64_t low = 0;
            uint64_t high = 0;
            cxl_mmio_read64(mAFU, offset * 4, &low);
            cxl_mmio_read64(mAFU, offset * 4 + 16, &high);
            val = low | high << 32;
            std::memcpy((char *)&val, qBuf, length);
            cxl_mmio_write64(mAFU, offset * 4, val);
            cxl_mmio_write64(mAFU, offset * 4 + 16, (uint64_t)val >> 32);
        }
        return 0;
    }


    int CAPIShim::xclGetDeviceInfo2(xclDeviceInfo2 *info) {
        std::memset(info, 0, sizeof(xclDeviceInfo2));

        info->mMagic = 0X586C0C6C;
        info->mHALMajorVersion = XCLHAL_MAJOR_VER;
        info->mHALMajorVersion = XCLHAL_MINOR_VER;
        info->mMinTransferSize = 32;
        long val;
        cxl_get_cr_vendor(mAFU, 0, &val);
        cxl_get_cr_device(mAFU, 0, &val);
        info->mVendorId = val;
        info->mDeviceId = val;
        info->mSubsystemId = 0xffff;
        info->mSubsystemVendorId = 0xffff;
        cxl_get_psl_revision(mAdapter, &val);
        info->mDeviceVersion = val;

        info->mDDRSize = 0;
        info->mDataAlignment = BUFFER_ALIGNMENT;

	info->mDDRBankCount = 1;
	info->mOCLFrequency[0] = 200;

	std::string deviceName = "xilinx:adm-pcie-7v3:CAPI:1.1";
	std::size_t length = deviceName.copy(info->mName, deviceName.length(),0);
        info->mName[length] = '\0';

	
        // TODO: replace with lspci call to get device version
        char * env = std::getenv("XCL_PLATFORM");
        if (env != NULL) {
            std::string envStr(env);
            if (envStr.find("xilinx_adm-pcie-7v3_1ddr_1_0") != std::string::npos)
                mDSAMinorVersion = 0;
            else if (envStr.find("xilinx_adm-pcie-7v3_1ddr_1_1") != std::string::npos)
                mDSAMinorVersion = 1;
            else if (envStr.find("xilinx_adm-pcie-7v3_1ddr_1_2") != std::string::npos)
                mDSAMinorVersion = 2;
        }
        if (mLogStream.is_open()) {
            mLogStream << __func__ << ": XCL_PLATFORM = " << env << ", DSA version = "
                       << mDSAMajorVersion << "." << mDSAMinorVersion << std::endl;
        }

        return 0;
    }
    /*
     * Based on capi_reset.sh code
     */
    int CAPIShim::resetDevice(xclResetKind kind) {
        std::lock_guard<std::mutex> lock(mMemManagerMutex);
        mBusyBufferList.clear();

        std::string nodeName("/sys/class/cxl/card");
        nodeName += std::to_string(mBoardNumber);
        nodeName += "/load_image_on_perst";
        std::ofstream node(nodeName.c_str());
        node.write("user\n", 5);
        node.close();
        nodeName.resize(nodeName.size() - std::strlen("/load_image_on_perst"));
        nodeName += "/reset";
        node.open(nodeName.c_str());
        node.write("1\n", 2);
        node.close();
        sleep(30);
        return 0;
    }
}


xclDeviceHandle xclOpen(unsigned index, const char *logfileName, xclVerbosityLevel level)
{
    xclcapi::CAPIShim *handle = new xclcapi::CAPIShim(index, logfileName, level);
    if (!xclcapi::CAPIShim::handleCheck(handle)) {
        delete handle;
        handle = 0;
    }

    return (xclDeviceHandle *)handle;
}

void xclClose(xclDeviceHandle handle)
{
    if (xclcapi::CAPIShim::handleCheck(handle)) {
        delete ((xclcapi::CAPIShim *)handle);
    }
}


int xclGetDeviceInfo2(xclDeviceHandle handle, xclDeviceInfo2 *info)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return drv->xclGetDeviceInfo2(info);
}

//int xclLoadBitstream(xclDeviceHandle handle, const char *fileName)
//{
//    //TODO:
//    return 0;
//    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
//    if (!drv)
//        return -1;
//    return drv->xclLoadBitstream(fileName);
//}

int xclLoadXclBin(xclDeviceHandle handle, const xclBin *buffer)
{
    //TODO:
    return 0;
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return drv->xclLoadXclBin(buffer);
}


size_t xclWrite(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return drv->xclWrite(space, offset, hostBuf, size);
}

size_t xclRead(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return drv->xclRead(space, offset, hostBuf, size);
}


uint64_t xclAllocDeviceBuffer(xclDeviceHandle handle, size_t size)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return drv->xclAllocDeviceBuffer(size);
}


void xclFreeDeviceBuffer(xclDeviceHandle handle, uint64_t buf)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return;
    return drv->xclFreeDeviceBuffer(buf);
}


size_t xclCopyBufferHost2Device(xclDeviceHandle handle, uint64_t dest, const void *src, size_t size, size_t seek)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return drv->xclCopyBufferHost2Device(dest, src, size, seek);
}


size_t xclCopyBufferDevice2Host(xclDeviceHandle handle, void *dest, uint64_t src, size_t size, size_t skip)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return drv->xclCopyBufferDevice2Host(dest, src, size, skip);
}


int xclUpgradeFirmware(xclDeviceHandle handle, const char *fileName)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return 0;
}


int xclBootFPGA(xclDeviceHandle handle)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return 0;
}

unsigned xclProbe()
{
    return xclcapi::CAPIShim::xclProbe();
}


int xclResetDevice(xclDeviceHandle handle, xclResetKind kind)
{
    xclcapi::CAPIShim *drv = xclcapi::CAPIShim::handleCheck(handle);
    if (!drv)
        return -1;
    return drv->resetDevice(kind);
}

//
// Place holders for device profiling
//
size_t xclPerfMonStartCounters(xclDeviceHandle handle, xclPerfMonType type)
{
    return 0;
}

size_t xclPerfMonStopCounters(xclDeviceHandle handle, xclPerfMonType type)
{
    return 0;
}

size_t xclPerfMonReadCounters(xclDeviceHandle handle, xclPerfMonType type, xclCounterResults& counterResults)
{
    return 0;
}

size_t xclDebugReadIPStatus(xclDeviceHandle handle, xclDebugReadType type, void* debugResults)
{
    return 0;
}

size_t xclPerfMonClockTraining(xclDeviceHandle handle, xclPerfMonType type)
{
    return 0;
}

size_t xclPerfMonStartTrace(xclDeviceHandle handle, xclPerfMonType type, uint32_t startTrigger)
{
    return 0;
}

size_t xclPerfMonStopTrace(xclDeviceHandle handle, xclPerfMonType type)
{
    return 0;
}

uint32_t xclPerfMonGetTraceCount(xclDeviceHandle handle, xclPerfMonType type)
{
    return 0;
}

size_t xclPerfMonReadTrace(xclDeviceHandle handle, xclPerfMonType type, xclTraceResultsVector& traceVector)
{
    traceVector.mLength = 0;
    return 0;
}

double xclGetDeviceClockFreqMHz(xclDeviceHandle handle)
{
    return 1.0;
}

// TODO: replace this with the max. read bandwidth between host and device
double xclGetReadMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 5000.0;
}

// TODO: replace this with the max. write bandwidth between host and device
double xclGetWriteMaxBandwidthMBps(xclDeviceHandle handle)
{
  return 5000.0;
}

size_t xclGetDeviceTimestamp(xclDeviceHandle handle)
{
  return 0;
}

void xclSetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type, uint32_t numSlots)
{
  // don't do anything
}

uint32_t xclGetProfilingNumberSlots(xclDeviceHandle handle, xclPerfMonType type)
{
  return 0;
}

void xclGetProfilingSlotName(xclDeviceHandle handle, xclPerfMonType type, uint32_t slotnum,
		                     char* slotName, uint32_t length)
{
  // do nothing
  return;
}

void xclWriteHostEvent(xclDeviceHandle handle, xclPerfMonEventType type,
                       xclPerfMonEventID id)
{
  // don't do anything
}


