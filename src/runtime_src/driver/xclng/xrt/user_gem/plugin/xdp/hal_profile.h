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

typedef void(*cb_load_func_type)(unsigned, void*);

using cb_func_type = std::function<void(unsigned, void*)>;

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
  UnmgdPreadCallLogger(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset);
  ~UnmgdPreadCallLogger();
  unsigned local_idcode;
};

void load_xdp_plugin_library();

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

#endif