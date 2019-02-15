#include "plugin/xdp/hal_profile.h"

namespace bfs = boost::filesystem;

namespace xdphal {

cb_func_type cb;

bool loaded = false;
std::atomic<unsigned> global_idcode(0);
std::mutex lock;

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

AllocBOCallLogger::AllocBOCallLogger(size_t size, xclBOKind domain, unsigned flags) {
    if (!loaded) {
        return;
    }
    local_idcode = global_idcode++;
    if (cb) {
        cb(HalCallbackType::ALLOC_BO_START, nullptr);
    }
}

AllocBOCallLogger::~AllocBOCallLogger() {
    if (!loaded) {
        return;
    }
    if (cb) {
        cb(HalCallbackType::ALLOC_BO_END, nullptr);
    }
}

FreeBOCallLogger::FreeBOCallLogger(unsigned int boHandle) {
    if (!loaded) {
        return;
    }
    if (cb) {
        cb(HalCallbackType::FREE_BO_START, nullptr);
    }
}

FreeBOCallLogger::~FreeBOCallLogger() {
    if (!loaded) {
        return;
    }
    if (cb) {
        cb(HalCallbackType::FREE_BO_END, nullptr);
    }
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
