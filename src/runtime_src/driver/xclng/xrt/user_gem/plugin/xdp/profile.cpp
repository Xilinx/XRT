#include <dlfcn.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include "plugin/xdp/profile.h"

namespace bfs = boost::filesystem;

namespace xocl {

namespace xdp {

cb_open_type cb_open;

void register_cb_open (cb_open_type && cb) {
  cb_open = std::move(cb);
}

HalCallLogger::HalCallLogger(int x) {
    std::cout << "hal_api_call_logger is being called" << std::endl;
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

    typedef void (* xdpInitType)();

    const std::string s = "init_xdp_hal_plugin";
    auto initFunc = (xdpInitType)dlsym(handle, s.c_str());
    if (!initFunc)
    throw std::runtime_error("Failed to initialize XDP library, '" + s +"' symbol not found.\n" + dlerror());

    initFunc();
    return;
}

HalCallLogger::~HalCallLogger() {
    return;
}

} //  xdp

} //  xocl

