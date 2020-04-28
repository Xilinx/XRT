#include "plugin/xdp/hal_profile.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/dlfcn.h"

namespace bfs = boost::filesystem;

namespace xdphal {

cb_func_type cb;

static bool loaded = false;
std::atomic<uint64_t> global_idcode(0);
std::mutex lock;

static bool cb_valid() {
  return loaded && cb;
}

static boost::filesystem::path& dllExt() {
#ifdef _WIN32
  static boost::filesystem::path sDllExt(".dll");
#else
  static boost::filesystem::path sDllExt(".so");
#endif
  return sDllExt;
}

inline bool isDLL(const bfs::path& path) {
  return (bfs::exists(path)
          && bfs::is_regular_file(path)
          && path.extension()==dllExt());
}

boost::filesystem::path
modulepath(const boost::filesystem::path& root, const std::string& libnm)
{
#ifdef _WIN32
  return root / "bin" / (libnm + ".dll");
#else
  return root / "lib" / "xrt" / "module" / ("lib" + libnm + ".so");
#endif
}

static void directoryOrError(const bfs::path& path) {
  if (!bfs::is_directory(path)) {
    throw std::runtime_error("No such directory '" + path.string() + "'");
  }
}

static const char* emptyOrValue(const char* cstr) {
  return cstr ? cstr : "";
}


CallLogger::CallLogger(uint64_t id)
           : m_local_idcode(id)
{
  load_xdp_plugin_library(nullptr);
}

CallLogger::~CallLogger()
{}

AllocBOCallLogger::AllocBOCallLogger(xclDeviceHandle handle /*, size_t size, int unused, unsigned flags*/) 
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

AllocUserPtrBOCallLogger::AllocUserPtrBOCallLogger(xclDeviceHandle handle /*, void *userptr, size_t size, unsigned flags*/)
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::ALLOC_USERPTR_BO_START, &payload);
}

AllocUserPtrBOCallLogger::~AllocUserPtrBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::ALLOC_USERPTR_BO_END, &payload);
}

FreeBOCallLogger::FreeBOCallLogger(xclDeviceHandle handle /*, unsigned int boHandle*/) 
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

WriteBOCallLogger::WriteBOCallLogger(xclDeviceHandle handle, size_t size /*, unsigned int boHandle, const void *src, size_t seek*/) 
    : CallLogger(global_idcode)
      ,m_buffer_transfer_id(0)
{
    if (!cb_valid()) return;
    // increment global_idcode only if valid calllback
    m_buffer_transfer_id = ++global_idcode;
    ++global_idcode;

    BOTransferCBPayload payload = {{m_local_idcode, handle}, m_buffer_transfer_id, size} ;
    cb(HalCallbackType::WRITE_BO_START, &payload);
}

WriteBOCallLogger::~WriteBOCallLogger() {
    if (!cb_valid()) return;

    BOTransferCBPayload payload = {{m_local_idcode, 0}, m_buffer_transfer_id, 0};
    cb(HalCallbackType::WRITE_BO_END, &payload);
}

ReadBOCallLogger::ReadBOCallLogger(xclDeviceHandle handle, size_t size /*, unsigned int boHandle, void *dst, size_t skip*/) 
    : CallLogger(global_idcode)
      ,m_buffer_transfer_id(0)
{
    if (!cb_valid()) return;
    // increment global_idcode only if valid calllback
    m_buffer_transfer_id = ++global_idcode;
    ++global_idcode;
    
    BOTransferCBPayload payload = {{m_local_idcode, handle}, m_buffer_transfer_id, size} ;
    cb(HalCallbackType::READ_BO_START, &payload);
}

ReadBOCallLogger::~ReadBOCallLogger() {
    if (!cb_valid()) return;

    BOTransferCBPayload payload = {{m_local_idcode, 0}, m_buffer_transfer_id, 0};
    cb(HalCallbackType::READ_BO_END, &payload);
}  

MapBOCallLogger::MapBOCallLogger(xclDeviceHandle handle /*, unsigned int boHandle, bool write*/) 
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

SyncBOCallLogger::SyncBOCallLogger(xclDeviceHandle handle, size_t size, xclBOSyncDirection dir /*, unsigned int boHandle, size_t offset*/) 
    : CallLogger(global_idcode)
      ,m_buffer_transfer_id(0)
      ,m_is_write_to_device((XCL_BO_SYNC_BO_TO_DEVICE == dir) ? true : false)
{
    if (!cb_valid()) return;
    // increment global_idcode only if valid calllback
    m_buffer_transfer_id = ++global_idcode;
    ++global_idcode;

    SyncBOCBPayload payload = {{m_local_idcode, handle}, m_buffer_transfer_id, size, m_is_write_to_device};
    cb(HalCallbackType::SYNC_BO_START, &payload);
}

SyncBOCallLogger::~SyncBOCallLogger() {
    if (!cb_valid()) return;
    SyncBOCBPayload payload = {{m_local_idcode, 0}, m_buffer_transfer_id, 0, m_is_write_to_device};
    cb(HalCallbackType::SYNC_BO_END, &payload);
}

CopyBOCallLogger::CopyBOCallLogger(xclDeviceHandle handle /*, unsigned int dst_boHandle,
                                   unsigned int src_bohandle, size_t size, size_t dst_offset, size_t src_offset*/) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::COPY_BO_START, &payload);
}

CopyBOCallLogger::~CopyBOCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::COPY_BO_END, &payload);
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

ReadCallLogger::ReadCallLogger(xclDeviceHandle handle, size_t size /*, xclAddressSpace space, uint64_t offset, void *hostBuf */) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    ReadWriteCBPayload payload = {{m_local_idcode, handle}, size};
    cb(HalCallbackType::READ_START, &payload);
}

ReadCallLogger::~ReadCallLogger()
{
    if (!cb_valid()) return;

    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::READ_END, &payload);
}

WriteCallLogger::WriteCallLogger(xclDeviceHandle handle, size_t size /*, xclAddressSpace space, uint64_t offset, const void *hostBuf */) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    ReadWriteCBPayload payload = { {m_local_idcode, handle}, size};
    cb(HalCallbackType::WRITE_START, (void*)(&payload));
}

WriteCallLogger::~WriteCallLogger()
{
    if (!cb_valid()) return;

    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::WRITE_END, &payload);
}

ProbeCallLogger::ProbeCallLogger() 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, nullptr};
    cb(HalCallbackType::PROBE_START, &payload);
}

ProbeCallLogger::~ProbeCallLogger()
{
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::PROBE_END, &payload);
}

LockDeviceCallLogger::LockDeviceCallLogger(xclDeviceHandle handle) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::LOCK_DEVICE_START, &payload);
}

LockDeviceCallLogger::~LockDeviceCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::LOCK_DEVICE_END, &payload);
}

UnLockDeviceCallLogger::UnLockDeviceCallLogger(xclDeviceHandle handle) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::UNLOCK_DEVICE_START, &payload);
}

UnLockDeviceCallLogger::~UnLockDeviceCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::UNLOCK_DEVICE_END, &payload);
}

OpenCallLogger::OpenCallLogger(/*unsigned deviceIndex*/)
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::OPEN_START, &payload);
}

OpenCallLogger::~OpenCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::OPEN_END, &payload);
}

CloseCallLogger::CloseCallLogger(xclDeviceHandle handle) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::CLOSE_START, &payload);
}

CloseCallLogger::~CloseCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::CLOSE_END, &payload);
}

OpenContextCallLogger::OpenContextCallLogger(/*unsigned deviceIndex*/)
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::OPEN_CONTEXT_START, &payload);
}

OpenContextCallLogger::~OpenContextCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::OPEN_CONTEXT_END, &payload);
}

CloseContextCallLogger::CloseContextCallLogger(xclDeviceHandle handle) 
    : CallLogger(global_idcode)
{
    if (!cb_valid()) return;
    global_idcode++;    // increment only if valid calllback
    CBPayload payload = {m_local_idcode, handle};
    cb(HalCallbackType::CLOSE_CONTEXT_START, &payload);
}

CloseContextCallLogger::~CloseContextCallLogger() {
    if (!cb_valid()) return;
    CBPayload payload = {m_local_idcode, 0};
    cb(HalCallbackType::CLOSE_CONTEXT_END, &payload);
}

LoadXclbinCallLogger::LoadXclbinCallLogger(xclDeviceHandle handle, const void* buffer) 
                    : CallLogger(global_idcode), 
                      h(handle), mBuffer(buffer)
{
  if (!cb_valid()) return ;
  ++global_idcode ; // increment only if valid callback
  XclbinCBPayload payload = { {m_local_idcode, handle}, buffer } ;
  cb(HalCallbackType::LOAD_XCLBIN_START, &payload) ;
}

LoadXclbinCallLogger::~LoadXclbinCallLogger()
{
  if (!cb_valid()) return ;
  XclbinCBPayload payload = { {m_local_idcode, h}, mBuffer } ;
  cb(HalCallbackType::LOAD_XCLBIN_END, &payload) ;
}


void load_xdp_plugin_library(HalPluginConfig* )
{
    std::lock_guard<std::mutex> loader_guard(lock);
    if(loaded) {
      return;
    }

    if(!xrt_core::config::get_xrt_profile()) {
      loaded = true ;
      return;
    }

    if(xrt_core::config::get_profile()) {
      // "profile=true" is also set. This enables OpenCL based flow for profiling. 
      // Currently, mix of OpenCL and HAL based profiling is not supported.
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT",
                std::string("Both profile=true and xrt_profile=true set in xrt.ini config. Currently, these flows are not supported to work together."));
      return;
    }

    bfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));
    if (xrt.empty()) {
      throw std::runtime_error("Library xdp_hal_plugin not found! XILINX_XRT not set");
    }
    bfs::path xrtlib(xrt / "lib" / "xrt" / "module");
    directoryOrError(xrtlib);
    auto libname = modulepath(xrt, "xdp_hal_plugin");
    if (!isDLL(libname)) {
      throw std::runtime_error("Library " + libname.string() + " not found!");
    }
    auto handle = xrt_core::dlopen(libname.string().c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle)
      throw std::runtime_error("Failed to open XDP HAL Profile library '" + libname.string() + "'\n" + xrt_core::dlerror());

    const std::string cb_func_name = "hal_level_xdp_cb_func";
    cb = cb_func_type(reinterpret_cast<cb_load_func_type>(xrt_core::dlsym(handle, cb_func_name.c_str())));
    if(!cb) {
      throw std::runtime_error("Failed to find callback function symbol in XDP HAL Profile library '" + libname.string() + "'\n" + xrt_core::dlerror());
    }
    loaded = true;
}

}
