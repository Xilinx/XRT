#include "plugin/xdp/hal_api_interface.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"

namespace bfs = boost::filesystem;

namespace xdphalinterface {

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

  StartDeviceProfilingCls::StartDeviceProfilingCls(xclDeviceHandle handle)
  {
    load_xdp_hal_interface_plugin_library(nullptr);
    if(!cb_valid()) return;
    CBPayload payload = {0, handle};
    cb(HalInterfaceCallbackType::START_DEVICE_PROFILING, &payload);
  }

  StartDeviceProfilingCls::~StartDeviceProfilingCls()
  {}

  CreateProfileResultsCls::CreateProfileResultsCls(xclDeviceHandle handle, ProfileResults** results, int& status)
  {
    load_xdp_hal_interface_plugin_library(nullptr);
    if(!cb_valid()) { status = (-1); return; }
    
    ProfileResultsCBPayload payload = {{0, handle}, static_cast<void*>(results)};   // pass ProfileResults** as void*
    cb(HalInterfaceCallbackType::CREATE_PROFILE_RESULTS, &payload);
    status = 0;
  }

  CreateProfileResultsCls::~CreateProfileResultsCls()
  {}

  GetProfileResultsCls::GetProfileResultsCls(xclDeviceHandle handle, ProfileResults* results, int& status)
  {
    load_xdp_hal_interface_plugin_library(nullptr);
    if(!cb_valid()) { status = (-1); return; }

    ProfileResultsCBPayload payload = {{0, handle}, static_cast<void*>(results)};
    cb(HalInterfaceCallbackType::GET_PROFILE_RESULTS, &payload);
    status = 0;
  }

  GetProfileResultsCls::~GetProfileResultsCls()
  {}

  DestroyProfileResultsCls::DestroyProfileResultsCls(xclDeviceHandle handle, ProfileResults* results, int& status)
  {
    load_xdp_hal_interface_plugin_library(nullptr);
    if(!cb_valid()) { status = (-1); return; }

    ProfileResultsCBPayload payload = {{0, handle}, static_cast<void*>(results)};
    cb(HalInterfaceCallbackType::DESTROY_PROFILE_RESULTS, &payload);
    status = 0;
  }

  DestroyProfileResultsCls::~DestroyProfileResultsCls()
  {}

  void load_xdp_hal_interface_plugin_library(HalPluginConfig* )
  {
#ifdef XRT_LOAD_XDP_HAL_PLUGIN

    std::lock_guard<std::mutex> loader_guard(lock);
    if (loaded) {
      return;
    }

    if(!xrt_core::config::get_profile_api()) {
      // profile_api is not set to correct configuration. Skip loading xdp_hal_plugin.
      // There will be no profile support in this run.
      return;
    }

    // profile_api is set to "true". Try to load xdp_hal_api_interface plugin library
    if(xrt_core::config::get_profile()) {
      // "profile=true" is also set. This enables OpenCL based flow for profiling. 
      // Currently, mix of OpenCL and HAL based profiling is not supported.
      // So, give error and skip loading of xdp_hal_api_interface_plugin library
      xrt_core::message::send(xrt_core::message::severity_level::XRT_WARNING, "XRT", std::string("Both profile=true and profile_api=true set in xrt.ini config. Currently, these flows are not supported to work together. Hence, retrieving profile results using APIs will not be available in this run. To enable profiling with APIs, please set profile_api=true only and re-run."));
      return;
    }
    
    bfs::path xrt(emptyOrValue(getenv("XILINX_XRT")));
    bfs::path libname("libxdp_hal_api_interface_plugin.so");
    if (xrt.empty()) {
      xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", std::string("Library " + libname.string() + " not found! XILINX_XRT not set"));
      exit(EXIT_FAILURE);
    }
    bfs::path p(xrt / "lib");
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
      xrt_core::message::send(xrt_core::message::severity_level::XRT_ERROR, "XRT", std::string("Failed to open XDP hal api interface plugin library '" + p.string() + "'\n" + dlerror()));
      exit(EXIT_FAILURE);
    }
    const std::string cb_func_name = "hal_api_interface_cb_func";    
    dlerror();
    cb = cb_func_type(reinterpret_cast<cb_load_func_type>(dlsym(handle, cb_func_name.c_str())));
    if(dlerror() != NULL) { // check if dlsym was successful
      cb = nullptr;
    }
    loaded = true;
#endif
}

}
