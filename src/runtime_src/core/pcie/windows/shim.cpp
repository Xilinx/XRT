/**
 * Copyright (C) 2019 Xilinx, Inc
 * Copyright (C) 2019 Samsung Semiconductor, Inc
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
#define XCL_DRIVER_DLL_EXPORT
#define XRT_CORE_PCIE_WINDOWS_SOURCE
#include "shim.h"
#include "xrt_mem.h"
#include "xclfeatures.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"

#include <windows.h>
#include <winioctl.h>
#include <setupapi.h>
#include <strsafe.h>

// To be simplified
#include "core/pcie/driver/windows/include/XoclUser_INTF.h"

#include <cstring>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <string>
#include <regex>

#pragma warning(disable : 4100 4996)
#pragma comment (lib, "Setupapi.lib")

namespace { // private implementation details

inline bool
is_multiprocess_mode()
{
  static bool val = xrt_core::config::get_multiprocess() || std::getenv("XCL_MULTIPROCESS_MODE") != nullptr;
  return val;
}

struct shim
{
  using buffer_handle_type = xclBufferHandle; // xrt.h
  unsigned int m_devidx;
  XOCL_MAP_BAR_RESULT	mappedBar[3];
  bool m_locked = false;
  HANDLE m_dev;

  // create shim object, open the device, store the device handle
  shim(unsigned int devidx)
    : m_devidx(devidx)
  {
    // open device associated with devidx
    m_dev = CreateFileW(L"\\\\.\\XOCL_USER-0" XOCL_USER_DEVICE_DEVICE_NAMESPACE,
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        0,
                        OPEN_EXISTING,
                        0,
                        0);

    if (m_dev == INVALID_HANDLE_VALUE) {
      auto error = GetLastError();
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR,"XRT", "CreateFile failed with error %d",error);
      throw std::runtime_error("CreateFile failed with error " + std::to_string(error));
    }

    DWORD bytesRead;
    XOCL_MAP_BAR_ARGS mapBar = { 0 };
    XOCL_MAP_BAR_RESULT mapBarResult = { 0 };
    DWORD  error;
    PCHAR barNames[] = { "User", "Config", "Bypass" };

    for (DWORD i = 0; i < XOCL_MAP_BAR_TYPE_MAX; i++) {

      if (i == XOCL_MAP_BAR_TYPE_BYPASS) {

        //
        // Not a supported BAR on this device...
        //
        continue;

      }

      mapBar.BarType = i;

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "Mapping %s BAR...", barNames[i]);

      if (!DeviceIoControl(m_dev,
                           IOCTL_XOCL_MAP_BAR,
                           &mapBar,
                           sizeof(XOCL_MAP_BAR_ARGS),
                           &mapBarResult,
                           sizeof(XOCL_MAP_BAR_RESULT),
                           &bytesRead,
                           nullptr)) {

        error = GetLastError();

        xrt_core::message::
          send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl failed with error %d", error);

        continue;
      }

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "BAR mapped at 0x%p (0x%llu)"
             ,mapBarResult.Bar, mapBarResult.BarLength);

      mappedBar[i].Bar = (PUCHAR)mapBarResult.Bar;
      mappedBar[i].BarLength = mapBarResult.BarLength;
    }

  }

  // destruct shim object, close the device
  ~shim()
  {
    // close the device
    CloseHandle(m_dev);
  }

  buffer_handle_type
  alloc_bo(size_t size, unsigned int flags)
  {
    HANDLE bufferHandle;
    DWORD error = ERROR_UNABLE_TO_CLEAN;
    XOCL_CREATE_BO_ARGS createBOArgs;
    DWORD bytesWritten;

    bufferHandle = CreateFileW(L"\\\\.\\XOCL_USER-0" XOCL_USER_DEVICE_BUFFER_OBJECT_NAMESPACE,
                              GENERIC_READ | GENERIC_WRITE,
                              0,
                              0,
                              OPEN_EXISTING,
                              0,
                              0);

    //
    // If this call fails, check to figure out what the error is and report it.
    //
    if (bufferHandle == INVALID_HANDLE_VALUE) {

        error = GetLastError();

        xrt_core::message::
          send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "CreateFile failed with error %d", error);

        goto done;
    }

    //'size' needs to be multiple of 4K
    createBOArgs.Size = ((size % 4096) == 0) ? size : (((4096 + size) / 4096) * 4096);
    createBOArgs.BankNumber = flags & 0xFFFFFFLL;
    createBOArgs.BufferType = (flags & XCL_BO_FLAGS_P2P) ? XOCL_BUFFER_TYPE_P2P : XOCL_BUFFER_TYPE_NORMAL;

    if (!DeviceIoControl(bufferHandle,
                         IOCTL_XOCL_CREATE_BO,
                         &createBOArgs,
                         sizeof(XOCL_CREATE_BO_ARGS),
                         0,
                         0,
                         &bytesWritten,
                         nullptr)) {

        error = GetLastError();

        xrt_core::message::
          send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl 4 failed with error %d", error);

        goto done;
    }

    error = ERROR_SUCCESS;

done:

    if (error != ERROR_SUCCESS) {

        if (bufferHandle != INVALID_HANDLE_VALUE) {

            CloseHandle(bufferHandle);
            bufferHandle = INVALID_HANDLE_VALUE;

        }

    }

    return bufferHandle;
  }

  buffer_handle_type
  alloc_user_ptr_bo(void* userptr, size_t size, unsigned int flags)
  {
    HANDLE bufferHandle;
    DWORD error = ERROR_UNABLE_TO_CLEAN;
    XOCL_USERPTR_BO_ARGS userPtrBO;
    DWORD bytesWritten;

    bufferHandle = CreateFileW(L"\\\\.\\XOCL_USER-0" XOCL_USER_DEVICE_BUFFER_OBJECT_NAMESPACE,
                               GENERIC_READ | GENERIC_WRITE,
                               0,
                               0,
                               OPEN_EXISTING,
                               0,
                               0);

    //
    // If this call fails, check to figure out what the error is and report it.
    //
    if (bufferHandle == INVALID_HANDLE_VALUE) {

      error = GetLastError();

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR,"XRT", "CreateFile failed with error %d",error);

      goto done;

    }

    userPtrBO.Address = userptr;
    userPtrBO.Size = ((size % 4096) == 0) ? size : (((4096 + size) / 4096) * 4096);
    userPtrBO.BankNumber = flags & 0xFFFFFFLL;
    userPtrBO.BufferType = XOCL_BUFFER_TYPE_USERPTR;

    if (!DeviceIoControl(bufferHandle,
                         IOCTL_XOCL_USERPTR_BO,
                         &userPtrBO,
                         sizeof(XOCL_USERPTR_BO_ARGS),
                         0,
                         0,
                         &bytesWritten,
                         nullptr)) {

      error = GetLastError();

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR,"XRT", "DeviceIoControl 4 failed with error %d", error);

      goto done;
    }

    error = ERROR_SUCCESS;

  done:

    if (error != ERROR_SUCCESS) {

      if (bufferHandle != INVALID_HANDLE_VALUE) {

        CloseHandle(bufferHandle);
        bufferHandle = INVALID_HANDLE_VALUE;

      }

    }

    return bufferHandle;
  }


  void*
  map_bo(buffer_handle_type handle, bool write)
  {
    DWORD bytesWritten;
    XOCL_MAP_BO_RESULT mapBO;
    DWORD  code;

    if (handle)
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "IOCTL_XOCL_MAP_BO");
    else {
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "IOCTL_XOCL_MAP_BO: Invalid Handle");
      return nullptr;
    }

    if (!DeviceIoControl(handle,
                         IOCTL_XOCL_MAP_BO,
                         0,
                         0,
                         &mapBO,
                         sizeof(XOCL_MAP_BO_RESULT),
                         &bytesWritten,
                         nullptr)) {

      code = GetLastError();

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl 3 failed with error %d", code);
      return nullptr;
    }
    else {

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "Mapped Address = 0x%p"
             ,mapBO.MappedUserVirtualAddress);

      //
      // Now zero it...
      //
      //RP   memset(mapBO.MappedUserVirtualAddress,
      //RP	   0,
      //RP	   (size_t)sizeToAllocate);

      return (void *)mapBO.MappedUserVirtualAddress;
    }
  }

  int
  unmap_bo(buffer_handle_type handle, void* addr)
  {
    // TODO : Implement
    return 0;
  }

  void
  free_bo(buffer_handle_type handle)
  {
    //As per OSR, just close the handle of BO.
    if(handle)
      CloseHandle(handle);
  }

  int
  sync_bo(buffer_handle_type handle, xclBOSyncDirection dir, size_t size, size_t offset)
  {
    DWORD bytesWritten;
    DWORD  error;
    XOCL_SYNC_BO_ARGS syncBo = { 0 };

    syncBo.Direction = (dir == XCL_BO_SYNC_BO_TO_DEVICE) ? XOCL_BUFFER_DIRECTION_TO_DEVICE : XOCL_BUFFER_DIRECTION_FROM_DEVICE;
    syncBo.Offset = offset;
    syncBo.Size = size;

    if (!DeviceIoControl(handle,
                         IOCTL_XOCL_SYNC_BO,
                         &syncBo,
                         sizeof(XOCL_SYNC_BO_ARGS),
                         nullptr,
                         0,
                         &bytesWritten,
                         nullptr)) {

      error = GetLastError();

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "Sync write failed with error %d", error);

      return error;
    }

    return 0;
  }

  // {D8E1267B-5041-BA45-A8AC-D93D3CCA1378}
 // unsigned char GUID_VALIDATE_XCLBIN[16]	  {0xD8,0xE1,0x26,0x7B, 0x50,0x41, 0xBA,0x45, 0xA8, 0xAC, 0xD9, 0x3D, 0x3C, 0xCA, 0x13, 0x78};


  int
  open_context(xuid_t xclbin_id, unsigned int ip_idx, bool shared)
  {
    HANDLE deviceHandle = m_dev;
    XOCL_CTX_ARGS ctxArgs = { 0 };
    DWORD bytesRet;

    ctxArgs.Operation = XOCL_CTX_OP_ALLOC_CTX;
    ctxArgs.Flags = (shared) ? XOCL_CTX_FLAG_SHARED : XOCL_CTX_FLAG_EXCLUSIVE;
    ctxArgs.CuIndex = ip_idx;
    memcpy(&ctxArgs.XclBinUuid, xclbin_id, sizeof(xuid_t));

    char str[512] = { 0 };
    uuid_unparse_lower(ctxArgs.XclBinUuid, str);
    xrt_core::message::
      send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclbin_uuid = %s\n", str);

    if (!DeviceIoControl(deviceHandle,
                         IOCTL_XOCL_CTX,
                         &ctxArgs,
                         sizeof(XOCL_CTX_ARGS),
                         NULL,
                         0,
                         &bytesRet,
                         NULL)) {

      auto error = GetLastError();
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "CTX failed with error %d", error);
      return error;
    }

    return 0;
  }

  int
  close_context(xuid_t xclbin_id, unsigned int ip_idx)
  {
    HANDLE deviceHandle = m_dev;
    XOCL_CTX_ARGS ctxArgs = { 0 };
    DWORD bytesRet;

    ctxArgs.Operation = XOCL_CTX_OP_FREE_CTX;
	ctxArgs.CuIndex = ip_idx;
    memcpy(&ctxArgs.XclBinUuid, xclbin_id, sizeof(xuid_t));

    if (!DeviceIoControl(deviceHandle,
                         IOCTL_XOCL_CTX,
                         &ctxArgs,
                         sizeof(XOCL_CTX_ARGS),
                         NULL,
                         0,
                         &bytesRet,
                         NULL)) {

      auto error = GetLastError();
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "CTX failed with error %d", error);
      return error;
    }

    return 0;
  }

  int
  exec_buf(buffer_handle_type handle)
  {
    HANDLE deviceHandle = m_dev;
    XOCL_EXECBUF_ARGS execArgs = { 0 };
    DWORD bytesRet;
    execArgs.ExecBO = handle;

    if (!DeviceIoControl(deviceHandle,
                         IOCTL_XOCL_EXECBUF,
                         &execArgs,
                         sizeof(XOCL_EXECBUF_ARGS),
                         NULL,
                         0,
                         &bytesRet,
                         NULL)) {

      auto error = GetLastError();
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "CTX failed with error %d", error);

      if (GetLastError() == ERROR_BAD_COMMAND) {

        //
        // Device is already configured, not really a problem...
        //
        xrt_core::message::
          send(xrt_core::message::severity_level::XRT_INFO, "XRT", "Device already configured!");
        return -1; //ERROR_SUCCESS;
      }

      return error;

    }
    return 0;
  }

  int
  exec_wait(int msec)
  {
    HANDLE deviceHandle = m_dev;
    BOOLEAN workToDo;
    XOCL_EXECPOLL_ARGS pollArgs;
    DWORD error;
    DWORD commandsCompleted;

    workToDo = FALSE;
    commandsCompleted = 0;

    pollArgs.DelayInMS = msec;

    if (!DeviceIoControl(deviceHandle,
                         IOCTL_XOCL_EXECPOLL,
                         &pollArgs,
                         sizeof(XOCL_EXECPOLL_ARGS),
                         NULL,
                         0,
                         &commandsCompleted,
                         NULL)) {

      error = GetLastError();

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT"
             ,"DeviceIoControl IOCTL_XOCL_EXECPOLL failed with error %d", error);

      goto done;
    }

    workToDo = TRUE;

  done:

    return workToDo;

  }

  int
  get_bo_properties(buffer_handle_type handle, struct xclBOProperties* properties)
  {
    XOCL_INFO_BO_RESULT infoBo = { 0 };
    DWORD error;
    DWORD bytesRet;

    if (!DeviceIoControl(handle,
                         IOCTL_XOCL_INFO_BO,
                         NULL,
                         0,
                         &infoBo,
                         sizeof(XOCL_INFO_BO_RESULT),
                         &bytesRet,
                         NULL)) {

      error = GetLastError();
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT"
             ,"get_bo_Properties - DeviceIoControl failed with error %d", error);
    }

    properties->handle = 0;
    properties->flags = 0;
    properties->size = infoBo.Size;
    properties->paddr = infoBo.Paddr;

    return 0;
  }

  bool SendIoctlReadAxlf(PUCHAR ImageBuffer, DWORD BuffSize)
  {
    HANDLE deviceHandle = m_dev;
    DWORD error;
    DWORD bytesWritten;

    if (!DeviceIoControl(deviceHandle,
                         IOCTL_XOCL_READ_AXLF,
                         ImageBuffer,
                         BuffSize,
                         0,
                         0,
                         &bytesWritten,
                         nullptr)) {

      error = GetLastError();

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl failed with error %d", error);

      goto out;
    }

    error = 0;

  out:

    return error ? false : true;

  }

  bool SendIoctlStatMemTopo()
  {
    HANDLE deviceHandle = m_dev;
    DWORD error;
    DWORD bytesWritten;
    DWORD bytesToRead;
    XOCL_STAT_CLASS statClass = XoclStatMemTopology;
    XOCL_MEM_TOPOLOGY_INFORMATION topoInfo;
    XOCL_MEM_RAW_INFORMATION memRaw;

    bytesToRead = sizeof(topoInfo);

    if (!DeviceIoControl(deviceHandle,
                         IOCTL_XOCL_STAT,
                         &statClass,
                         sizeof(statClass),
                         &topoInfo,
                         sizeof(topoInfo),
                         &bytesWritten,
                         nullptr)) {

      error = GetLastError();

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl failed with error %d", error);

      goto out;
    }

    printf("Got XoclStatMemTopology Data:\n");
    printf("Memory regions: %d\n", topoInfo.MemTopoCount);
    for (size_t i = 0; i < topoInfo.MemTopoCount; i++) {
      printf("\ttag=%s, start=0x%llx, size=0x%llx\n",
             topoInfo.MemTopo[i].m_tag,
             topoInfo.MemTopo[i].m_base_address,
             topoInfo.MemTopo[i].m_size);
    }

    statClass = XoclStatMemRaw;

    if (!DeviceIoControl(deviceHandle,
                         IOCTL_XOCL_STAT,
                         &statClass,
                         sizeof(statClass),
                         &memRaw,
                         sizeof(memRaw),
                         &bytesWritten,
                         nullptr)) {

      error = GetLastError();

      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl failed with error %d", error);

      goto out;
    }

    printf("Got XoclStatMemRaw Data:\n");
    printf("Count: %d\n", memRaw.MemRawCount);
    for (unsigned int i = 0; i < memRaw.MemRawCount; i++) {
      printf("\t(%d) BOCount=%llu, MemoryUsage=0x%llx\n"
             ,i, memRaw.MemRaw[i].BOCount, memRaw.MemRaw[i].MemoryUsage);
    }

    error = 0;

  out:

    return error ? false : true;
  }

  int
  load_xclbin(const struct axlf* buffer)
  {
    DWORD buffSize = 0;
    bool succeeded;

    //
    // FIrst test
    //
    buffSize = (DWORD) buffer->m_header.m_length;

    xrt_core::message::
      send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "Calling IOCTL_XOCL_READ_AXLF... ");

    succeeded = SendIoctlReadAxlf((PUCHAR)buffer, buffSize);

    if (succeeded) {
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "OK");
    }
    else {
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "FAILED");
      return 1;
    }

    //
    // Second test...
    //
    xrt_core::message::
      send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "Calling IOCTL_XOCL_STAT (XoclStatMemTopology)... ");

    succeeded = SendIoctlStatMemTopo();

    if (succeeded) {
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "OK");
    }
    else {
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "FAILED");
      return 1;
    }

    return 0;
  }

  /*
   * wordcopy()
   *
   * Copy bytes word (32bit) by word.
   * Neither memcpy, nor std::copy work as they become byte copying
   * on some platforms.
   */
  inline void* wordcopy(void *dst, const void* src, size_t bytes)
  {
    // assert dest is 4 byte aligned
    //  assert((reinterpret_cast<intptr_t>(dst) % 4) == 0);

    using word = uint32_t;
    auto d = reinterpret_cast<word*>(dst);
    auto s = reinterpret_cast<const word*>(src);
    auto w = bytes / sizeof(word);

    for (size_t i = 0; i < w; ++i)
      d[i] = s[i];

    return dst;
  }

  int
  write(enum xclAddressSpace space, uint64_t offset, const void *hostbuf, size_t size)
  {
    switch (space) {
    case XCL_ADDR_KERNEL_CTRL:
      //Todo: offset += mOffsets[XCL_ADDR_KERNEL_CTRL];
      (void *)wordcopy(((char *)mappedBar[0].Bar + offset), hostbuf, size);
      break;
    default:
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "Unsupported Address Space: Write failed");
      return 1;
    }
    return 0;
  }

  int
  read(enum xclAddressSpace space, uint64_t offset, void *hostbuf, size_t size)
  {
    switch (space) {
    case XCL_ADDR_KERNEL_CTRL:
      //Todo: offset += mOffsets[XCL_ADDR_KERNEL_CTRL];
      (void *)wordcopy(hostbuf, ((char *)mappedBar[0].Bar + offset), size);
      break;
    default:
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "Unsupported Address Space: Read failed");
      return 1;
    }
    return 0;
  }

  ssize_t
  unmgd_pwrite(unsigned flags, const void *buf, size_t count, uint64_t offset)
  {
      if (flags) {  // make compatible with Linux code
          return false;
      }

      XOCL_PWRITE_BO_UNMGD_ARGS pwriteBO;
      DWORD  code;
      DWORD bytesWritten;

      pwriteBO.Offset = offset;

      if (!DeviceIoControl(m_dev,
          IOCTL_XOCL_PWRITE_UNMGD,
          &pwriteBO,
          sizeof(XOCL_PWRITE_BO_UNMGD_ARGS),
          (void *)buf,
          (DWORD)count,
          &bytesWritten,
          nullptr)) {

          code = GetLastError();

          xrt_core::message::
              send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl PWRITE unmanaged failed with error %d", code);
          return false;
      }

      return true;
  }

  ssize_t
  unmgd_pread(unsigned int flags, void *buf, size_t size, uint64_t offset)
  {

      XOCL_PREAD_BO_UNMGD_ARGS preadBO;
      DWORD  code;
      DWORD bytesRead;

      if (flags) {  // make compatible with Linux code
          return false;
      }

      preadBO.Offset = offset;

      if (!DeviceIoControl(m_dev,
          IOCTL_XOCL_PREAD_UNMGD,
          &preadBO,
          sizeof(XOCL_PREAD_BO_UNMGD_ARGS),
          buf,
          (DWORD)size,
          &bytesRead,
          nullptr)) {

          code = GetLastError();
          xrt_core::message::
              send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl PREAD unmanaged failed with error %d", code);
          return false;
      }

      return true;
  }

  int
  write_bo(xclBufferHandle boHandle, const void *src, size_t size, size_t seek)
  {
      XOCL_PWRITE_BO_ARGS pwriteBO;
      DWORD  code;
      DWORD bytesWritten;

      pwriteBO.Offset = seek;

      if (!DeviceIoControl(boHandle,
          IOCTL_XOCL_PWRITE_BO,
          &pwriteBO,
          sizeof(XOCL_PWRITE_BO_ARGS),
          (void *)src,
          (DWORD)size,
          &bytesWritten,
          nullptr)) {

          code = GetLastError();

          xrt_core::message::
              send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl PWRITE failed with error %d", code);
          return code;
      }
      // Ignoring bytesWritten as API only expects 0 as success
      return 0;
  }

  int
  read_bo(xclBufferHandle boHandle, void *dst, size_t size, size_t skip)
  {
      XOCL_PREAD_BO_ARGS preadBO;
      DWORD  code;
      DWORD bytesRead;

      preadBO.Offset = skip;

      if (!DeviceIoControl(boHandle,
          IOCTL_XOCL_PREAD_BO,
          &preadBO,
          sizeof(XOCL_PREAD_BO_ARGS),
          dst,
          (DWORD)size,
          &bytesRead,
          nullptr)) {

          code = GetLastError();
          xrt_core::message::
              send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "DeviceIoControl PREAD failed with error %d", code);
          return code;
      }
      // Ignoring bytesWritten as API only expects 0 as success
      return 0;
  }

  bool
  lock_device()
  {
    if (!is_multiprocess_mode() && m_locked)
        return false;

    return m_locked = true;
  }

  bool
  unlock_device()
  {
    m_locked = false;
    return true;
  }

  void
  get_rom_info(FeatureRomHeader* value)
  {
    XOCL_STAT_CLASS stat_class =  XoclStatRomInfo;
    XOCL_ROM_INFORMATION device_info;

    DWORD bytes = 0;
    auto status = DeviceIoControl(m_dev,
        IOCTL_XOCL_STAT,
        &stat_class, sizeof(stat_class),
        &device_info, sizeof(device_info),
        &bytes,
        nullptr);

    if (!status || bytes != sizeof(device_info))
      throw std::runtime_error("DeviceIoControl IOCTL_XOCL_STAT (rom_info) failed");

    std::memcpy(value->FPGAPartName, device_info.FPGAPartName, sizeof(device_info.FPGAPartName));
    std::memcpy(value->VBNVName, device_info.VBNVName, sizeof(device_info.VBNVName));
    value->DDRChannelCount = device_info.DDRChannelCount;
    value->DDRChannelSize = device_info.DDRChannelSize;
  }

  void
  get_device_info(XOCL_DEVICE_INFORMATION* value)
  {
    XOCL_STAT_CLASS stat_class =  XoclStatDevice;

    DWORD bytes = 0;
    auto status = DeviceIoControl(m_dev,
        IOCTL_XOCL_STAT,
        &stat_class, sizeof(stat_class),
        value, sizeof(XOCL_DEVICE_INFORMATION),
        &bytes,
        nullptr);

    if (!status || bytes != sizeof(XOCL_DEVICE_INFORMATION))
      throw std::runtime_error("DeviceIoControl IOCTL_XOCL_STAT (get_device_info) failed");
  }

  void
  get_mem_topology(char* buffer, size_t size, size_t* size_ret)
  {
    XOCL_MEM_TOPOLOGY_INFORMATION mem_info;
    XOCL_STAT_CLASS_ARGS statargs;

    statargs.StatClass = XoclStatMemTopology;

    DWORD bytes = 0;
    auto status = DeviceIoControl(m_dev,
        IOCTL_XOCL_STAT,
        &statargs, sizeof(XOCL_STAT_CLASS_ARGS),
        &mem_info, sizeof(XOCL_MEM_TOPOLOGY_INFORMATION),
        &bytes,
        nullptr);

    if (!status || bytes != sizeof(XOCL_MEM_TOPOLOGY_INFORMATION))
      throw std::runtime_error("DeviceIoControl IOCTL_XOCL_STAT (get_mem_topology) failed");

    size_t mem_topology_size = sizeof(XOCL_MEM_TOPOLOGY_INFORMATION);

    if (size_ret)
      *size_ret = mem_topology_size;

    if (!buffer)
      return;  // size_ret has required size

    if (size < mem_topology_size)
      throw std::runtime_error
        ("DeviceIoControl IOCTL_XOCL_STAT (get_mem_topology) failed "
         "size (" + std::to_string(size) + ") of buffer too small, "
         "required size (" + std::to_string(mem_topology_size) + ")");

    std::memcpy(buffer, &mem_info, sizeof(XOCL_MEM_TOPOLOGY_INFORMATION));
  }

  void
  get_ip_layout(char* buffer, size_t size, size_t* size_ret)
  {
    XU_IP_LAYOUT iplayout_hdr;
    XOCL_STAT_CLASS_ARGS statargs;

    statargs.StatClass =  XoclStatIpLayout;

    DWORD bytes = 0;
    auto status = DeviceIoControl(m_dev,
        IOCTL_XOCL_STAT,
        &statargs, sizeof(XOCL_STAT_CLASS_ARGS),
        &iplayout_hdr, sizeof(XU_IP_LAYOUT),
        &bytes,
        nullptr);

    if (!status || bytes != sizeof(XU_IP_LAYOUT))
      throw std::runtime_error("DeviceIoControl IOCTL_XOCL_STAT (get_ip_layout hdr) failed");

    DWORD ip_layout_size = sizeof(XU_IP_LAYOUT) + iplayout_hdr.m_count * sizeof(XU_IP_DATA);

    if (size_ret)
      *size_ret = ip_layout_size;

    if (!buffer)
      return;  // size_ret has the required size

    if (size < ip_layout_size)
      throw std::runtime_error
        ("DeviceIoControl IOCTL_XOCL_STAT (get_ip_layout) failed "
         "size (" + std::to_string(size) + ") of buffer too small, "
         "required size (" + std::to_string(ip_layout_size) + ")");

    auto iplayout = reinterpret_cast<PXU_IP_LAYOUT>(buffer);

    status = DeviceIoControl(m_dev,
       IOCTL_XOCL_STAT,
       &statargs, sizeof(XOCL_STAT_CLASS_ARGS),
       iplayout, ip_layout_size,
       &bytes,
       nullptr);

    if (!status || bytes != ip_layout_size)
      throw std::runtime_error("DeviceIoControl IOCTL_XOCL_STAT (get_ip_layout) failed");
  }

  void
  get_bdf_info(uint16_t bdf[3])
  {
    // TODO: code share with mgmt
    GUID guid = GUID_DEVINTERFACE_XOCL_USER;
    auto hdevinfo = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
    SP_DEVINFO_DATA dev_info_data;
    dev_info_data.cbSize = sizeof(dev_info_data);
    DWORD size;
    SetupDiEnumDeviceInfo(hdevinfo, m_devidx, &dev_info_data);
    SetupDiGetDeviceRegistryProperty(hdevinfo, &dev_info_data, SPDRP_LOCATION_INFORMATION,
                                     nullptr, nullptr, 0, &size);
    std::string buf(static_cast<size_t>(size), 0);
    SetupDiGetDeviceRegistryProperty(hdevinfo, &dev_info_data, SPDRP_LOCATION_INFORMATION,
                                     nullptr, (PBYTE)buf.data(), size, nullptr);

    std::regex regex("\\D+(\\d+)\\D+(\\d+)\\D+(\\d+)");
    std::smatch match;
    if (std::regex_search(buf, match, regex))
      std::transform(match.begin() + 1, match.end(), bdf,
                     [](const auto& m) {
                       return static_cast<uint16_t>(std::stoi(m.str()));
                     });
  }

}; // struct shim

shim*
get_shim_object(xclDeviceHandle handle)
{
  // TODO: Do some sanity check
  return reinterpret_cast<shim*>(handle);
}

}

namespace userpf {

void
get_rom_info(xclDeviceHandle hdl, FeatureRomHeader* value)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "get_rom_info()");
  auto shim = get_shim_object(hdl);
  shim->get_rom_info(value);
}

void
get_device_info(xclDeviceHandle hdl, XOCL_DEVICE_INFORMATION* value)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "get_device_info()");
  auto shim = get_shim_object(hdl);
  shim->get_device_info(value);
}

void
get_mem_topology(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "get_mem_topology()");
  auto shim = get_shim_object(hdl);
  shim->get_mem_topology(buffer, size, size_ret);
}

void
get_ip_layout(xclDeviceHandle hdl, char* buffer, size_t size, size_t* size_ret)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "get_ip_layout()");
  auto shim = get_shim_object(hdl);
  shim->get_ip_layout(buffer, size, size_ret);
}

void
get_bdf_info(xclDeviceHandle hdl, uint16_t bdf[3])
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "get_bdf_info()");
  auto shim = get_shim_object(hdl);
  shim->get_bdf_info(bdf);
}

} // namespace userpf

// Basic
unsigned int
xclProbe()
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclProbe()");
  GUID guid = GUID_DEVINTERFACE_XOCL_USER;

  HDEVINFO device_info =
    SetupDiGetClassDevs((LPGUID) &guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (device_info == INVALID_HANDLE_VALUE) {
    xrt_core::message::
      send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "GetDevices INVALID_HANDLE_VALUE");
    return 0;
  }

  SP_DEVICE_INTERFACE_DATA device_interface;
  device_interface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

  // enumerate through devices
  DWORD index;
  for (index = 0;
       SetupDiEnumDeviceInterfaces(device_info, NULL, &guid, index, &device_interface);
       ++index) {

    // get required buffer size
    ULONG detailLength = 0;
    if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, NULL, 0, &detailLength, NULL)
        && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "SetupDiGetDeviceInterfaceDetail - get length failed");
      break;
    }

    // allocate space for device interface detail
    auto dev_detail = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, detailLength));
    if (!dev_detail) {
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "HeapAlloc failed");
      break;
    }
    dev_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

    // get device interface detail
    if (!SetupDiGetDeviceInterfaceDetail(device_info, &device_interface, dev_detail, detailLength, NULL, NULL)) {
      xrt_core::message::
        send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "SetupDiGetDeviceInterfaceDetail - get detail failed");
      HeapFree(GetProcessHeap(), 0, dev_detail);
      break;
    }

    HeapFree(GetProcessHeap(), 0, dev_detail);
  }

  SetupDiDestroyDeviceInfoList(device_info);

  return index;
}

xclDeviceHandle
xclOpen(unsigned int deviceIndex, const char *logFileName, xclVerbosityLevel level)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclOpen()");
  try {
    return new shim(deviceIndex);
  }
  catch (const std::exception& ex) {
    xrt_core::message::
      send(xrt_core::message::severity_level::XRT_ERROR, "XRT", "xclOpen failed with `%s`", ex.what());
    return nullptr;
  }
}

void
xclClose(xclDeviceHandle handle)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclClose()");
  auto shim = get_shim_object(handle);
  delete shim;
}


// XRT Buffer Management APIs
xclBufferHandle
xclAllocBO(xclDeviceHandle handle, size_t size, int unused, unsigned int flags)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclAllocBO()");
  auto shim = get_shim_object(handle);
  return shim->alloc_bo(size, flags);
}

xclBufferHandle
xclAllocUserPtrBO(xclDeviceHandle handle, void *userptr, size_t size, unsigned int flags)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclAllocUserPtrBO()");
  auto shim = get_shim_object(handle);
  return shim->alloc_user_ptr_bo(userptr, size, flags);
}

void*
xclMapBO(xclDeviceHandle handle, xclBufferHandle boHandle, bool write)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclMapBO()");
  auto shim = get_shim_object(handle);
  return shim->map_bo(boHandle, write);
}

int
xclUnmapBO(xclDeviceHandle handle, xclBufferHandle boHandle, void* addr)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclUnmapBO()");
  auto shim = get_shim_object(handle);
  return shim->unmap_bo(boHandle, addr);
}

void
xclFreeBO(xclDeviceHandle handle, xclBufferHandle boHandle)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclFreeBO()");
  auto shim = get_shim_object(handle);
  return shim->free_bo(boHandle);
}

int
xclSyncBO(xclDeviceHandle handle, xclBufferHandle boHandle, xclBOSyncDirection dir, size_t size, size_t offset)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclSyncBO()");
  auto shim = get_shim_object(handle);
  return shim->sync_bo(boHandle, dir, size, offset);
}

// Compute Unit Execution Management APIs
int
xclOpenContext(xclDeviceHandle handle, xuid_t xclbinId, unsigned int ipIndex,bool shared)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclOpenContext()");
  auto shim = get_shim_object(handle);

  //Virtual resources are not currently supported by driver
  return (ipIndex == (unsigned int)-1)
	  ? 0
	  : shim->open_context(xclbinId, ipIndex, shared);
}

int xclCloseContext(xclDeviceHandle handle, xuid_t xclbinId, unsigned int ipIndex)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclCloseContext()");
  auto shim = get_shim_object(handle);

  //Virtual resources are not currently supported by driver
  return (ipIndex == (unsigned int) -1)
	  ? 0
	  : shim->close_context(xclbinId, ipIndex);
}

int
xclExecBuf(xclDeviceHandle handle, xclBufferHandle cmdBO)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclExecBuf()");
  auto shim = get_shim_object(handle);
  return shim->exec_buf(cmdBO);
}

int
xclExecWait(xclDeviceHandle handle, int timeoutMilliSec)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclExecWait()");
  auto shim = get_shim_object(handle);
  return shim->exec_wait(timeoutMilliSec);
}

int
xclGetBOProperties(xclDeviceHandle handle, xclBufferHandle boHandle,
		   struct xclBOProperties *properties)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclGetBOProperties()");
  auto shim = get_shim_object(handle);
  return shim->get_bo_properties(boHandle,properties);
}

int
xclLoadXclBin(xclDeviceHandle handle, const struct axlf *buffer)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclLoadXclbin()");
  auto shim = get_shim_object(handle);
  return shim->load_xclbin(buffer);
}

unsigned int
xclVersion()
{
  return 2;
}

int
xclGetDeviceInfo2(xclDeviceHandle handle, struct xclDeviceInfo2 *info)
{
  std::memset(info, 0, sizeof(xclDeviceInfo2));
  info->mMagic = 0;
  info->mHALMajorVersion = XCLHAL_MAJOR_VER;
  info->mHALMinorVersion = XCLHAL_MINOR_VER;
  info->mMinTransferSize = 0;
  info->mDMAThreads = 2;
  info->mDataAlignment = 4096; // 4k

  return 0;
}

int
xclLockDevice(xclDeviceHandle handle)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclLockDevice()");
  auto shim = get_shim_object(handle);
  return shim->lock_device() ? 0 : 1;
}

int
xclUnlockDevice(xclDeviceHandle handle)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclUnlockDevice()");
  auto shim = get_shim_object(handle);
  return shim->unlock_device() ? 0 : 1;
}

ssize_t
xclUnmgdPwrite(xclDeviceHandle handle, unsigned int flags, const void *buf, size_t count, uint64_t offset)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclUnmgdPwrite()");
  auto shim = get_shim_object(handle);
  return shim->unmgd_pwrite(flags, buf, count, offset);
}

ssize_t
xclUnmgdPread(xclDeviceHandle handle, unsigned int flags, void *buf, size_t count, uint64_t offset)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclUnmgdPread()");
  auto shim = get_shim_object(handle);
  return shim->unmgd_pread(flags, buf, count, offset);
}

size_t xclWriteBO(xclDeviceHandle handle, xclBufferHandle boHandle, const void *src, size_t size, size_t seek)
{
    xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclWriteBO()");
    auto shim = get_shim_object(handle);
    return shim->write_bo(boHandle, src, size, seek);
}

size_t xclReadBO(xclDeviceHandle handle, xclBufferHandle boHandle, void *dst, size_t size, size_t skip)
{
    xrt_core::message::
        send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclReadBO()");
    auto shim = get_shim_object(handle);
    return shim->read_bo(boHandle, dst, size, skip);
}

// Deprecated APIs
size_t
xclWrite(xclDeviceHandle handle, enum xclAddressSpace space, uint64_t offset, const void *hostbuf, size_t size)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclWrite()");
  auto shim = get_shim_object(handle);
  return shim->write(space,offset,hostbuf,size) ? 0 : size;
}

size_t
xclRead(xclDeviceHandle handle, enum xclAddressSpace space,
        uint64_t offset, void *hostbuf, size_t size)
{
  xrt_core::message::
    send(xrt_core::message::severity_level::XRT_DEBUG, "XRT", "xclRead()");
  auto shim = get_shim_object(handle);
  return shim->read(space,offset,hostbuf,size) ? 0 : size;
}
