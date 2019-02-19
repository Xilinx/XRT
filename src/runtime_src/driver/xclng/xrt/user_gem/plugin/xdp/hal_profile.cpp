#include "plugin/xdp/hal_profile.h"

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

static void directoryOrError(const bfs::path& path) {
  if (!bfs::is_directory(path))
    throw std::runtime_error("No such directory '" + path.string() + "'");
}

static const char* emptyOrValue(const char* cstr) {
  return cstr ? cstr : "";
}

AllocBOCallLogger::AllocBOCallLogger(xclDeviceHandle handle, size_t size, xclBOKind domain, unsigned flags) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::ALLOC_BO_START, &payload);
}

AllocBOCallLogger::~AllocBOCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::ALLOC_BO_END, &payload);
}

FreeBOCallLogger::FreeBOCallLogger(xclDeviceHandle handle, unsigned int boHandle) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::FREE_BO_START, &payload);
}

FreeBOCallLogger::~FreeBOCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::FREE_BO_END, &payload);
}

WriteBOCallLogger::WriteBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, const void *src, size_t size, size_t seek) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::WRITE_BO_START, &payload);
}

WriteBOCallLogger::~WriteBOCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::WRITE_BO_END, &payload);
}

ReadBOCallLogger::ReadBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, void *dst, size_t size, size_t skip) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::READ_BO_START, &payload);
}

ReadBOCallLogger::~ReadBOCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::READ_BO_END, &payload);
}  

MapBOCallLogger::MapBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, bool write) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::MAP_BO_START, &payload);
}

MapBOCallLogger::~MapBOCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::MAP_BO_END, &payload);
}

SyncBOCallLogger::SyncBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::SYNC_BO_START, &payload);
}

SyncBOCallLogger::~SyncBOCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::SYNC_BO_END, &payload);
}

UnmgdPwriteCallLogger::UnmgdPwriteCallLogger(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::UNMGD_WRITE_START, &payload);
}

UnmgdPwriteCallLogger::~UnmgdPwriteCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::UNMGD_WRITE_END, &payload);
}

UnmgdPreadCallLogger::UnmgdPreadCallLogger(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::UNMGD_READ_START, &payload);
}

UnmgdPreadCallLogger::~UnmgdPreadCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::UNMGD_READ_END, &payload);
}

ReadCallLogger::ReadCallLogger(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, void *hostBuf, size_t size) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::READ_START, &payload);
}

ReadCallLogger::~ReadCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::READ_END, &payload);
}

WriteCallLogger::WriteCallLogger(xclDeviceHandle handle, xclAddressSpace space, uint64_t offset, const void *hostBuf, size_t size) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    CallbackMarker payload = {local_idcode, (unsigned long)handle};
    cb(HalCallbackType::WRITE_START, &payload);
}

WriteCallLogger::~WriteCallLogger() {
    if (!cb_valid()) return;
    CallbackMarker payload = {local_idcode, 0};
    cb(HalCallbackType::WRITE_END, &payload);
}

void load_xdp_plugin_library() {
    std::cout << "Loading xdp plugins ..." << std::endl;
    std::lock_guard<std::mutex> loader_guard(lock);
    if (loaded) {
        return;
    }
    bfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));
    bfs::path libname ("libxdp_hal_plugin.so");
    if (xrt.empty()) {
        throw std::runtime_error("Library " + libname.string() + " not found! XILINX_XRT not set");
    }
    bfs::path p(xrt / "lib");
    directoryOrError(p);
    p /= libname;
    if (!isDLL(p)) {
        throw std::runtime_error("Library " + p.string() + " not found!");
    }
    auto handle = dlopen(p.string().c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle)
        throw std::runtime_error("Failed to open XDP hal plugin library '" + p.string() + "'\n" + dlerror());
    const std::string cb_func_name = "hal_level_xdp_cb_func";
    cb = cb_func_type(reinterpret_cast<cb_load_func_type>(dlsym(handle, cb_func_name.c_str())));
    loaded = true;
}

}
