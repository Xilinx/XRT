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
    CallbackMarker payload = {local_idcode, (uint64_t)handle};
    cb(HalCallbackType::ALLOC_BO_START, &payload);
}

AllocBOCallLogger::~AllocBOCallLogger() {
    if (!cb_valid()) return;
    cb(HalCallbackType::ALLOC_BO_END, nullptr);
}

FreeBOCallLogger::FreeBOCallLogger(xclDeviceHandle handle, unsigned int boHandle) {
    if (!cb_valid()) return;
    local_idcode = global_idcode++;
    cb(HalCallbackType::FREE_BO_START, nullptr);
}

FreeBOCallLogger::~FreeBOCallLogger() {
    if (!cb_valid()) return;
    cb(HalCallbackType::FREE_BO_END, nullptr);
}

WriteBOCallLogger::WriteBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, const void *src, size_t size, size_t seek) {
    
}

WriteBOCallLogger::~WriteBOCallLogger() {

}

ReadBOCallLogger::ReadBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, void *dst, size_t size, size_t skip) {

}

ReadBOCallLogger::~ReadBOCallLogger() {

}

MapBOCallLogger::MapBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, bool write) {

}

MapBOCallLogger::~MapBOCallLogger() {

}

SyncBOCallLogger::SyncBOCallLogger(xclDeviceHandle handle, unsigned int boHandle, xclBOSyncDirection dir, size_t size, size_t offset) {

}

SyncBOCallLogger::~SyncBOCallLogger() {

}

UnmgdPwriteCallLogger::UnmgdPwriteCallLogger(xclDeviceHandle handle, unsigned flags, const void *buf, size_t count, uint64_t offset) {

}

UnmgdPwriteCallLogger::~UnmgdPwriteCallLogger() {

}

UnmgdPreadCallLogger::UnmgdPreadCallLogger(xclDeviceHandle handle, unsigned flags, void *buf, size_t count, uint64_t offset) {

}

UnmgdPreadCallLogger::~UnmgdPreadCallLogger() {

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
