#ifndef XDP_PROFILE_HAL_PLUGIN_H_
#define XDP_PROFILE_HAL_PLUGIN_H_

#include <functional>
#include <iostream>
#include <atomic>
#include <mutex>
#include <dlfcn.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include "core/include/xclperf.h"
#include "core/include/xclhal2.h"

namespace xdphal {

class CallLogger
{
  public:
    CallLogger(uint64_t id = 0);
    ~CallLogger();

    uint64_t m_local_idcode;  
};

/**
 * Loggers are all alike except they each have different
 * constructor arguments so that they can capture various
 * information from different hal APIs.
 * 
 * @param local_idcode for identifying unique function calls
 */
class AllocBOCallLogger : public CallLogger
{
public:
  AllocBOCallLogger(xclDeviceHandle handle /*, size_t size , int unused, unsigned flags*/);
  ~AllocBOCallLogger();
};

class AllocUserPtrBOCallLogger : public CallLogger
{
public:
  AllocUserPtrBOCallLogger(xclDeviceHandle handle /*, void *userptr, size_t size, unsigned flags*/);
  ~AllocUserPtrBOCallLogger();
};

class FreeBOCallLogger : public CallLogger
{
public:
  FreeBOCallLogger(xclDeviceHandle handle /*, unsigned int boHandle*/);
  ~FreeBOCallLogger();
};

/**
 * WriteBOCallLogger logs two events : 1 for the API call and 1 for the buffer transfer.
 * So, in addition to CallLogger:m_local_idcode, it needs another unique identifier for buffer transfer.
 */
class WriteBOCallLogger : public CallLogger
{
  uint64_t m_buffer_transfer_id;
public:
  WriteBOCallLogger(xclDeviceHandle handle, size_t size /*, unsigned int boHandle, const void *src, size_t size, size_t seek*/);
  ~WriteBOCallLogger();
};

/**
 * ReadBOCallLogger logs two events : 1 for the API call and 1 for the buffer transfer.
 * So, in addition to CallLogger:m_local_idcode, it needs another unique identifier for buffer transfer.
 */
class ReadBOCallLogger : public CallLogger
{
  uint64_t m_buffer_transfer_id;
public:
  ReadBOCallLogger(xclDeviceHandle handle, size_t size /*, unsigned int boHandle, void *dst, size_t size, size_t skip*/);
  ~ReadBOCallLogger();
};

class MapBOCallLogger : public CallLogger
{
public:
  MapBOCallLogger(xclDeviceHandle handle /*, unsigned int boHandle, bool write*/);
  ~MapBOCallLogger();
};

class SyncBOCallLogger : public CallLogger
{
  uint64_t m_buffer_transfer_id;
  bool     m_is_write_to_device;
public:
  SyncBOCallLogger(xclDeviceHandle handle, size_t size, xclBOSyncDirection dir /*, unsigned int boHandle, xclBOSyncDirection dir, size_t offset*/);
  ~SyncBOCallLogger();
};

class CopyBOCallLogger : public CallLogger
{
public:
  CopyBOCallLogger(xclDeviceHandle handle /*, unsigned int dst_boHandle,
					   unsigned int src_bohandle, size_t size, size_t dst_offset, size_t src_offset*/);
  ~CopyBOCallLogger();
};

class GetBOPropCallLogger : public CallLogger
{
public:
  GetBOPropCallLogger(xclDeviceHandle handle);
  ~GetBOPropCallLogger();
};

class ExecBufCallLogger : public CallLogger
{
public:
  ExecBufCallLogger(xclDeviceHandle handle);
  ~ExecBufCallLogger();
};

class ExecWaitCallLogger : public CallLogger
{
public:
  ExecWaitCallLogger(xclDeviceHandle handle);
  ~ExecWaitCallLogger();
};

class UnmgdPwriteCallLogger : public CallLogger
{
public:
  UnmgdPwriteCallLogger(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset);
  ~UnmgdPwriteCallLogger();
};

class UnmgdPreadCallLogger : public CallLogger
{
public:
  UnmgdPreadCallLogger(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset);
  ~UnmgdPreadCallLogger();
};

class ReadCallLogger : public CallLogger
{
public:
  ReadCallLogger(xclDeviceHandle handle, size_t size /*, xclAddressSpace space, uint64_t offset, void *hostBuf */);
  ~ReadCallLogger();
};

class WriteCallLogger : public CallLogger
{
public:
  WriteCallLogger(xclDeviceHandle handle, size_t size /*, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size*/);
  ~WriteCallLogger();
};

class RegReadCallLogger : public CallLogger
{
public:
  RegReadCallLogger(xclDeviceHandle handle,  uint32_t ipIndex, uint32_t offset);
  ~RegReadCallLogger();
};

class RegWriteCallLogger : public CallLogger
{
public:
  RegWriteCallLogger(xclDeviceHandle handle,  uint32_t ipIndex, uint32_t offset);
  ~RegWriteCallLogger();
};

class ProbeCallLogger : public CallLogger
{
  public:
  ProbeCallLogger();
  ~ProbeCallLogger();
};

class LockDeviceCallLogger : public CallLogger
{
  public:
  LockDeviceCallLogger(xclDeviceHandle handle);
  ~LockDeviceCallLogger();
};

class UnLockDeviceCallLogger : public CallLogger
{
  public:
  UnLockDeviceCallLogger(xclDeviceHandle handle);
  ~UnLockDeviceCallLogger();
};


class OpenCallLogger : public CallLogger
{
  public:
  OpenCallLogger();
  ~OpenCallLogger();
};

class CloseCallLogger : public CallLogger
{
  public:
  CloseCallLogger(xclDeviceHandle handle);
  ~CloseCallLogger();
};

class OpenContextCallLogger : public CallLogger
{
  public:
  OpenContextCallLogger();
  ~OpenContextCallLogger();
};

class CloseContextCallLogger : public CallLogger
{
  public:
  CloseContextCallLogger(xclDeviceHandle handle);
  ~CloseContextCallLogger();
};

 class LoadXclbinCallLogger : public CallLogger
 {
 private:
   xclDeviceHandle h ;
   const void* mBuffer ;
 public:
   LoadXclbinCallLogger(xclDeviceHandle handle, const void* buffer) ;
   ~LoadXclbinCallLogger() ;
 } ;

/** End of the loggers */

void load_xdp_plugin_library(HalPluginConfig* config);
void register_hal_callbacks(void* handle) ;
void warning_hal_callbacks() ; 

} //  xdphal

/**
 * The declaration of the macros to be inserted into 
 * the shim implementations
 */
#define ALLOC_BO_CB         xdphal::AllocBOCallLogger alloc_bo_call_logger(handle /*, size, unused, flags*/);
#define ALLOC_USERPTR_BO_CB xdphal::AllocUserPtrBOCallLogger alloc_userptr_bo_call_logger(handle /*, userptr, size, flags*/);
#define FREE_BO_CB  xdphal::FreeBOCallLogger  free_bo_call_logger(handle /*, boHandle*/);
#define WRITE_BO_CB xdphal::WriteBOCallLogger write_bo_call_logger(handle, size /*, boHandle, src, seek*/);
#define READ_BO_CB  xdphal::ReadBOCallLogger  read_bo_call_logger(handle, size /*, boHandle, dst, skip*/);
#define MAP_BO_CB   xdphal::MapBOCallLogger   map_bo_call_logger(handle /*, boHandle, write*/);
#define SYNC_BO_CB  xdphal::SyncBOCallLogger  sync_bo_call_logger(handle, size, dir /*, boHandle, offset*/); 
#define COPY_BO_CB  xdphal::CopyBOCallLogger  copy_bo_call_logger(handle /*, dst_boHandle, src_boHandle, size, dst_offset, src_offset*/);
#define GET_BO_PROP_CB   xdphal::GetBOPropCallLogger   get_bo_prop_call_logger(handle);
#define EXEC_BUF_CB   xdphal::ExecBufCallLogger   exec_buf_call_logger(handle);
#define EXEC_WAIT_CB  xdphal::ExecWaitCallLogger  exec_wait_call_logger(handle);
#define UNMGD_PWRITE_CB xdphal::UnmgdPwriteCallLogger unmgd_pwrite_call_logger(handle, flags, buf, count, offset);
#define UNMGD_PREAD_CB  xdphal::UnmgdPreadCallLogger  unnmgd_pread_call_logger(handle, flags, buf, count, offset);
#define WRITE_CB xdphal::WriteCallLogger write_call_logger(handle, size /*, space, offset, hostBuf */);
#define READ_CB  xdphal::ReadCallLogger  read_call_logger(handle, size /*, space, offset, hostBuf*/);
#define REG_WRITE_CB xdphal::RegWriteCallLogger reg_write_call_logger(handle, ipIndex, offset);
#define REG_READ_CB  xdphal::RegReadCallLogger  reg_read_call_logger(handle, ipIndex, offset);
#define PROBE_CB xdphal::ProbeCallLogger probe_call_logger();
#define LOCK_DEVICE_CB   xdphal::LockDeviceCallLogger   lock_device_call_logger(handle);
#define UNLOCK_DEVICE_CB xdphal::UnLockDeviceCallLogger unlock_device_call_logger(handle);
#define OPEN_CB  xdphal::OpenCallLogger  open_call_logger();
#define CLOSE_CB xdphal::CloseCallLogger close_call_logger(handle);
#define OPEN_CONTEXT_CB  xdphal::OpenContextCallLogger  open_context_call_logger();
#define CLOSE_CONTEXT_CB xdphal::CloseContextCallLogger close_context_call_logger(handle);
#define LOAD_XCLBIN_CB   xdphal::LoadXclbinCallLogger xclbin_call_logger(handle, buffer) ;

#endif
