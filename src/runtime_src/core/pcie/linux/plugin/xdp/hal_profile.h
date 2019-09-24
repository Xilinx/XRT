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

/**
 * This function type definition is used for
 * dynamically loading the plugin function.
 */
typedef void(*cb_load_func_type)(unsigned, void*);

/**
 * This standard function is for storing the function
 * loaded. Using cpp standard for robustness across 
 * function calls and context sharing.
 */
using cb_func_type = std::function<void(unsigned, void*)>;

class CallLogger
{
  public:
    CallLogger(unsigned id = 0);
    ~CallLogger();

    unsigned m_local_idcode;  
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
  AllocBOCallLogger(xclDeviceHandle handle, size_t size, int unused, unsigned flags);
  ~AllocBOCallLogger();
};

class FreeBOCallLogger : public CallLogger
{
public:
  FreeBOCallLogger(xclDeviceHandle handle, unsigned int boHandle);
  ~FreeBOCallLogger();
};

class WriteBOCallLogger : public CallLogger
{
public:
  WriteBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, const void *src, size_t size, size_t seek);
  ~WriteBOCallLogger();
};

class ReadBOCallLogger : public CallLogger
{
public:
  ReadBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, void *dst, size_t size, size_t skip);
  ~ReadBOCallLogger();
};

class MapBOCallLogger : public CallLogger
{
public:
  MapBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, bool write);
  ~MapBOCallLogger();
};

class SyncBOCallLogger : public CallLogger
{
public:
  SyncBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset);
  ~SyncBOCallLogger();
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
  ReadCallLogger(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
  ~ReadCallLogger();
};

class WriteCallLogger : public CallLogger
{
public:
  WriteCallLogger(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
  ~WriteCallLogger();
};
/** End of the loggers */

class StartDeviceProfilingCls
{
public:
  StartDeviceProfilingCls(xclDeviceHandle handle);
  ~StartDeviceProfilingCls();
};

class CreateProfileResultsCls
{
public:
  CreateProfileResultsCls(xclDeviceHandle handle, ProfileResults**, int& status);
  ~CreateProfileResultsCls();
};

class GetProfileResultsCls
{
public:
  GetProfileResultsCls(xclDeviceHandle handle, ProfileResults*, int& status);
  ~GetProfileResultsCls();
};

class DestroyProfileResultsCls
{
public:
  DestroyProfileResultsCls(xclDeviceHandle handle, ProfileResults*, int& status);
  ~DestroyProfileResultsCls();
};

void load_xdp_plugin_library(HalPluginConfig* config);

} //  xdphal

/**
 * The declaration of the macros to be inserted into 
 * the shim implementations
 */
#define ALLOC_BO_CB xdphal::AllocBOCallLogger alloc_bo_call_logger(handle, size, unused, flags);
#define FREE_BO_CB xdphal::FreeBOCallLogger free_bo_call_logger(handle, boHandle);
#define WRITE_BO_CB xdphal::WriteBOCallLogger write_bo_call_logger(handle, boHandle, src, size, seek);
#define READ_BO_CB xdphal::ReadBOCallLogger read_bo_call_logger(handle, boHandle, dst, size, skip);
#define MAP_BO_CB xdphal::MapBOCallLogger map_bo_call_logger(handle, boHandle, write);
#define UNMGD_PWRITE_CB xdphal::UnmgdPwriteCallLogger unmgd_pwrite_call_logger(handle, flags, buf, count, offset);
#define UNMGD_PREAD_CB xdphal::UnmgdPreadCallLogger unnmgd_pread_call_logger(handle, flags, buf, count, offset);
#define WRITE_CB xdphal::WriteCallLogger write_call_logger(handle, space, offset, hostBuf, size);
#define READ_CB xdphal::ReadCallLogger read_call_logger(handle, space, offset, hostBuf, size);

#define START_DEVICE_PROFILING_CB(handle) xdphal::StartDeviceProfilingCls start_device_profiling_inst(handle);
#define CREATE_PROFILE_RESULTS_CB(handle, results, status) xdphal::CreateProfileResultsCls create_profile_results_inst(handle, results, status);
#define GET_PROFILE_RESULTS_CB(handle, results, status) xdphal::GetProfileResultsCls get_profile_results_inst(handle, results, status);
#define DESTROY_PROFILE_RESULTS_CB(handle, results, status) xdphal::DestroyProfileResultsCls destroy_profile_results_inst(handle, results, status);

#endif
