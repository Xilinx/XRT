#include "hal_api_interface.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"
#include "core/common/dlfcn.h"
#include "core/common/module_loader.h"

namespace bfs = boost::filesystem;

namespace xdphalinterface {

  std::function<void(unsigned int, void*)> cb ;

  std::atomic<unsigned> global_idcode(0);
  
  static bool cb_valid() {
    return cb != nullptr ;
  }

  APIInterfaceLoader::APIInterfaceLoader()
  {
    if (xrt_core::config::get_profile_api())
    {
      load_xdp_hal_interface_plugin_library(nullptr) ;
    }
  }

  APIInterfaceLoader::~APIInterfaceLoader()
  {
  }
    
  StartDeviceProfilingCls::StartDeviceProfilingCls(xclDeviceHandle handle)
  {
    APIInterfaceLoader loader ;
    if(!cb_valid()) return;
    CBPayload payload = {0, handle};
    cb(HalInterfaceCallbackType::START_DEVICE_PROFILING, &payload);
  }

  StartDeviceProfilingCls::~StartDeviceProfilingCls()
  {}

  CreateProfileResultsCls::CreateProfileResultsCls(xclDeviceHandle handle, ProfileResults** results, int& status)
  {
    APIInterfaceLoader loader ;
    if(!cb_valid()) { status = (-1); return; }
    
    ProfileResultsCBPayload payload = {{0, handle}, static_cast<void*>(results)};   // pass ProfileResults** as void*
    cb(HalInterfaceCallbackType::CREATE_PROFILE_RESULTS, &payload);
    status = 0;
  }

  CreateProfileResultsCls::~CreateProfileResultsCls()
  {}

  GetProfileResultsCls::GetProfileResultsCls(xclDeviceHandle handle, ProfileResults* results, int& status)
  {
    APIInterfaceLoader loader ;
    if(!cb_valid()) { status = (-1); return; }

    ProfileResultsCBPayload payload = {{0, handle}, static_cast<void*>(results)};
    cb(HalInterfaceCallbackType::GET_PROFILE_RESULTS, &payload);
    status = 0;
  }

  GetProfileResultsCls::~GetProfileResultsCls()
  {}

  DestroyProfileResultsCls::DestroyProfileResultsCls(xclDeviceHandle handle, ProfileResults* results, int& status)
  {
    APIInterfaceLoader loader ;
    if(!cb_valid()) { status = (-1); return; }

    ProfileResultsCBPayload payload = {{0, handle}, static_cast<void*>(results)};
    cb(HalInterfaceCallbackType::DESTROY_PROFILE_RESULTS, &payload);
    status = 0;
  }

  DestroyProfileResultsCls::~DestroyProfileResultsCls()
  {}

  void register_hal_interface_callbacks(void* handle)
  {
    typedef void (*ftype)(unsigned int, void*) ;
    cb = (ftype)(xrt_core::dlsym(handle, "hal_api_interface_cb_func")) ;
    if (xrt_core::dlerror() != NULL) cb = nullptr ;
  }

  int error_hal_interface_callbacks()
  {
    if (xrt_core::config::get_profile())
    {
      xrt_core::message::send(xrt_core::message::severity_level::warning,
			      "XRT",
			      std::string("Both profile=true and profile_api=true set in xrt.ini config. Currently these flows are not supported to work together. Hence, retrieving profile results using APIs will not be available in this run.  To enable profiling with APIs, please set profile_api=true only and re-run.")) ;
      return 1 ;
    }
    return 0 ;
  }

  void load_xdp_hal_interface_plugin_library(HalPluginConfig* )
  {
    static xrt_core::module_loader
      xdp_hal_interface_loader("xdp_hal_api_interface_plugin",
			       register_hal_interface_callbacks,
			       nullptr, // warning function
			       error_hal_interface_callbacks) ;
  }

}
