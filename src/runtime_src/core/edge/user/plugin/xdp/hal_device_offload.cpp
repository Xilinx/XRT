
#include <functional>

#include "hal_device_offload.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"

namespace xdphaldeviceoffload {

  void load_xdp_hal_device_offload()
  {
    static xrt_core::module_loader
      xdp_hal_device_offload_loader("xdp_hal_device_offload_plugin",
                                    register_hal_device_offload_functions,
                                    hal_device_offload_warning_function,
                                    hal_device_offload_error_function);
  }

  std::function<void (void*)> update_device_cb ;
  std::function<void (void*)> flush_device_cb ;
 
  void register_hal_device_offload_functions(void* handle) 
  {
#ifdef XRT_CORE_BUILD_WITH_DL
    typedef void (*ftype)(void*) ;
    update_device_cb = (ftype)(xrt_core::dlsym(handle, "updateDeviceHAL")) ;
    if (xrt_core::dlerror() != NULL) update_device_cb = nullptr ;

    flush_device_cb = (ftype)(xrt_core::dlsym(handle, "flushDeviceHAL")) ;
    if (xrt_core::dlerror() != NULL) flush_device_cb = nullptr ;
#endif
  }

  void hal_device_offload_warning_function()
  {
    // No warnings at this level
  }

  int hal_device_offload_error_function()
  {
    if(xrt_core::config::get_profile() || xrt_core::config::get_timeline_trace()) {
      // OpenCL profiling and/or trace is enabled in xrt.ini config. So, disable HAL Device Trace Offload as both of these flows are not supported together.
      return 1;
    }
    return 0 ;
  }

} // end namespace xdphaldeviceoffload

namespace xdphal {

  void flush_device(void* handle)
  {
    if (xdphaldeviceoffload::flush_device_cb != nullptr)
    {
      xdphaldeviceoffload::flush_device_cb(handle) ;
    }
  }

  void update_device(void* handle)
  {
    if (xdphaldeviceoffload::update_device_cb != nullptr)
    {
      xdphaldeviceoffload::update_device_cb(handle) ;
    }
  }
}
