/* HAL profiling is not yet supported for Edge and no callbacks are added to Edge SHIM.
 * The following implementation is targeted for future use
 */

#include "hal_profile.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/dlfcn.h"
#include "core/common/module_loader.h"

namespace bfs = boost::filesystem;

namespace xdphal {

  std::function<void(unsigned, void*)> cb ;
  std::atomic<unsigned> global_idcode(0);  

  static bool cb_valid() {
    return cb != nullptr ;
  }

CallLogger::CallLogger(unsigned id)
           : m_local_idcode(id)
{
  if (xrt_core::config::get_xrt_profile())
  {
    load_xdp_plugin_library(nullptr);
  }
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

  void register_hal_callbacks(void* handle)
  {
#ifdef XRT_LOAD_XDP_HAL_PLUGIN
    typedef void(*ftype)(unsigned int, void*) ;
    cb = (ftype)(xrt_core::dlsym(handle, "hal_level_xdp_cb_func")) ;
    if (xrt_core::dlerror() != NULL) cb = nullptr ;
#endif
  }

  void warning_hal_callbacks()
  {
    if(xrt_core::config::get_profile()) {
      // "profile=true" is also set. This enables OpenCL based flow for profiling. 
      // Currently, mix of OpenCL and HAL based profiling is not supported.
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT",
                std::string("Both profile=true and xrt_profile=true set in xrt.ini config. Currently, these flows are not supported to work together."));
      return;
    }
  }

void load_xdp_plugin_library(HalPluginConfig* )
{
#ifdef XRT_LOAD_XDP_HAL_PLUGIN
  static xrt_core::module_loader xdp_hal_loader("xdp_hal_plugin",
						register_hal_callbacks,
						warning_hal_callbacks) ;
#endif
}

}
