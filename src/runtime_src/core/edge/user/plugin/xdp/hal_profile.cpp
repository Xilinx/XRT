/* HAL profiling is not yet supported for Edge and no callbacks are added to Edge SHIM.
 * The following implementation is targeted for future use
 */

#include "hal_profile.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"

namespace bfs = boost::filesystem;

namespace xdphal {

cb_func_type cb;

bool loaded = false;
std::atomic<unsigned> global_idcode(0);
std::mutex lock;

static bool cb_valid() {
    return loaded && cb;
}

static boost::filesystem::path& dllExt() {
  static boost::filesystem::path sDllExt(".so");
  return sDllExt;
}

inline bool isDLL(const bfs::path& path) {
  return (bfs::exists(path)
          && bfs::is_regular_file(path)
          && path.extension()==dllExt());
}

static int directoryOrError(const bfs::path& path) {
  if (!bfs::is_directory(path)) {
    xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", std::string("No such directory '" + path.string() + "'"));
    return -1;
  }
  return 0;
}

static const char* emptyOrValue(const char* cstr) {
  return cstr ? cstr : "";
}

CallLogger::CallLogger(unsigned id)
           : m_local_idcode(id)
{
  load_xdp_plugin_library(nullptr);
}

CallLogger::~CallLogger()
{}

AllocBOCallLogger::AllocBOCallLogger(xclDeviceHandle handle, size_t size, int unused, unsigned flags) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::ALLOC_BO_START, &payload);
}

AllocBOCallLogger::~AllocBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::ALLOC_BO_END, &payload);
}

FreeBOCallLogger::FreeBOCallLogger(xclDeviceHandle handle, unsigned int boHandle) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::FREE_BO_START, &payload);
}

FreeBOCallLogger::~FreeBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::FREE_BO_END, &payload);
}

WriteBOCallLogger::WriteBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, const void *src, size_t size, size_t seek) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::WRITE_BO_START, &payload);
}

WriteBOCallLogger::~WriteBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::WRITE_BO_END, &payload);
}

ReadBOCallLogger::ReadBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, void *dst, size_t size, size_t skip) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::READ_BO_START, &payload);
}

ReadBOCallLogger::~ReadBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::READ_BO_END, &payload);
}  

MapBOCallLogger::MapBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, bool write) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::MAP_BO_START, &payload);
}

MapBOCallLogger::~MapBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::MAP_BO_END, &payload);
}

SyncBOCallLogger::SyncBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::SYNC_BO_START, &payload);
}

SyncBOCallLogger::~SyncBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::SYNC_BO_END, &payload);
}

UnmgdPwriteCallLogger::UnmgdPwriteCallLogger(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    UnmgdPreadPwriteCBPayload payload = {{m_local_idcode, handle}, flags, count, offset};
    cb(HalCallbackType::UNMGD_WRITE_START, &payload);
}

UnmgdPwriteCallLogger::~UnmgdPwriteCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::UNMGD_WRITE_END, &payload);
}

UnmgdPreadCallLogger::UnmgdPreadCallLogger(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    UnmgdPreadPwriteCBPayload payload = {{m_local_idcode, handle}, flags, count, offset};
    cb(HalCallbackType::UNMGD_READ_START, &payload);
}

UnmgdPreadCallLogger::~UnmgdPreadCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::UNMGD_READ_END, &payload);
}

ReadCallLogger::ReadCallLogger(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    ReadWriteCBPayload payload = {{m_local_idcode, handle}, size};
    cb(HalCallbackType::READ_START, &payload);
}

ReadCallLogger::~ReadCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::READ_END, &payload);
}

WriteCallLogger::WriteCallLogger(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    ReadWriteCBPayload payload = { {m_local_idcode, handle}, size};
    cb(HalCallbackType::WRITE_START, (void*)(&payload));
}

WriteCallLogger::~WriteCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::WRITE_END, &payload);
}

void load_xdp_plugin_library(HalPluginConfig* )
{
#ifdef XRT_LOAD_XDP_HAL_PLUGIN

    std::lock_guard<std::mutex> loader_guard(lock);
    if (loaded) {
        return;
    }

    if(!xrt_core::config::get_xrt_profile()) {
      // xrt_profile is not set to correct configuration. Skip loading xdp_hal_plugin.
      return;
    }

    // xrt_profile is set to "true". Try to load xdp_hal_plugin library
    if(xrt_core::config::get_profile()) {
      // "profile=true" is also set. This enables OpenCL based flow for profiling. 
      // Currently, mix of OpenCL and HAL based profiling is not supported.
      // So, give error and skip loading of xdp_hal_plugin library
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", std::string("Both profile=true and xrt_profile=true set in xrt.ini config. Currently, these flows are not supported to work together. Hence, retrieving profile results using APIs will not be available in this run. To enable profiling with APIs, please set profile_api=true only and re-run."));
      return;
    }

    bfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));
    bfs::path libname("libxdp_hal_plugin.so");
    if (xrt.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", std::string("Library " + libname.string() + " not found! XILINX_XRT not set"));
      exit(EXIT_FAILURE);
    }
    bfs::path p(xrt / "lib");
    p /= "xrt" ;
    p /= "module" ;
    if(directoryOrError(p)) {
      exit(EXIT_FAILURE);
    }
    p /= libname;
    if (!isDLL(p)) {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", std::string("Library " + p.string() + " not found!"));
      exit(EXIT_FAILURE);
    }
    auto handle = dlopen(p.string().c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", std::string("Failed to open XDP hal plugin library '" + p.string() + "'\n" + dlerror()));
      exit(EXIT_FAILURE);
    }
    const std::string cb_func_name = "hal_level_xdp_cb_func";    
    dlerror();
    cb = cb_func_type(reinterpret_cast<cb_load_func_type>(dlsym(handle, cb_func_name.c_str())));
    if(dlerror() != NULL) { // check if dlsym was successful
      cb = nullptr;
    }
    loaded = true;
#endif
}

}
