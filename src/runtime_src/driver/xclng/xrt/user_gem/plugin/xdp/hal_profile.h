#ifndef XDP_PROFILE_HAL_PLUGIN_H_
#define XDP_PROFILE_HAL_PLUGIN_H_

#include <functional>
#include <iostream>
#include <atomic>
#include <mutex>
#include <dlfcn.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include "driver/include/xclperf.h"
#include "driver/include/xclhal2.h"

namespace xdphal {

/**
 * This function type definition is used for
 * dynamically loading the plugin function.
 */
typedef void(*cb_load_func_type)(unsigned, void*);

using cb_func_type = std::function<void(unsigned, void*)>;

/**
 * Loggers are all alike except they each have different
 * constructor arguments so that they can capture various
 * information from different hal APIs.
 * 
 * @param local_idcode for identifying unique function calls
 */
class AllocBOCallLogger {
public:
  AllocBOCallLogger(xclDeviceHandle handle, size_t size, xclBOKind domain, unsigned flags);
  ~AllocBOCallLogger();
  unsigned local_idcode;
};

class FreeBOCallLogger {
public:
  FreeBOCallLogger(xclDeviceHandle handle, unsigned int boHandle);
  ~FreeBOCallLogger();
  unsigned local_idcode;
};

class WriteBOCallLogger {
public:
  WriteBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, const void *src, size_t size, size_t seek);
  ~WriteBOCallLogger();
  unsigned local_idcode;
};

class ReadBOCallLogger {
public:
  ReadBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, void *dst, size_t size, size_t skip);
  ~ReadBOCallLogger();
  unsigned local_idcode;
};

class MapBOCallLogger {
public:
  MapBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, bool write);
  ~MapBOCallLogger();
  unsigned local_idcode;
};

class SyncBOCallLogger {
public:
  SyncBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset);
  ~SyncBOCallLogger();
  unsigned local_idcode;
};

class UnmgdPwriteCallLogger {
public:
  UnmgdPwriteCallLogger(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset);
  ~UnmgdPwriteCallLogger();
  unsigned local_idcode;
};

class UnmgdPreadCallLogger {
public:
  UnmgdPreadCallLogger(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset);
  ~UnmgdPreadCallLogger();
  unsigned local_idcode;
};

class ReadCallLogger {
public:
  ReadCallLogger(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size);
  ~ReadCallLogger();
  unsigned local_idcode;
};

class WriteCallLogger {
public:
  WriteCallLogger(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size);
  ~WriteCallLogger();
  unsigned local_idcode;
};
/** End of the loggers */

void load_xdp_plugin_library(HalPluginConfig* config);

} //  xdphal

/**
 * The declaration of the macros to be inserted into 
 * the shim implementations
 */
#define ALLOC_BO_CB xdphal::AllocBOCallLogger alloc_bo_call_logger(handle, size, domain, flags);
#define FREE_BO_CB xdphal::FreeBOCallLogger free_bo_call_logger(handle, boHandle);
#define WRITE_BO_CB xdphal::WriteBOCallLogger write_bo_call_logger(handle, boHandle, src, size, seek);
#define READ_BO_CB xdphal::ReadBOCallLogger read_bo_call_logger(handle, boHandle, dst, size, skip);
#define MAP_BO_CB xdphal::MapBOCallLogger map_bo_call_logger(handle, boHandle, write);
#define UNMGD_PWRITE_CB xdphal::UnmgdPwriteCallLogger unmgd_pwrite_call_logger(handle, flags, buf, count, offset);
#define UNMGD_PREAD_CB xdphal::UnmgdPreadCallLogger unnmgd_pread_call_logger(handle, flags, buf, count, offset);
#define WRITE_CB xdphal::WriteCallLogger write_call_logger(handle, space, offset, hostBuf, size);
#define READ_CB xdphal::ReadCallLogger read_call_logger(handle, space, offset, hostBuf, size);

#endif