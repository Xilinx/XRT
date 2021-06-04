
#include <functional>

#include "hal_device_offload.h"
#include "core/common/module_loader.h"
#include "core/common/dlfcn.h"
#include "core/common/config_reader.h"
#include "core/common/message.h"

namespace xdp {
namespace hal {
namespace device_offload {

  void load()
  {
    static xrt_core::module_loader
      xdp_hal_device_offload_loader("xdp_hal_device_offload_plugin",
                                    register_functions,
                                    warning_function,
                                    error_function);
  }

  std::function<void (void*)> update_device_cb ;
  std::function<void (void*)> flush_device_cb ;
 
  void register_functions(void* handle) 
  {
    typedef void (*ftype)(void*) ;
    update_device_cb = (ftype)(xrt_core::dlsym(handle, "updateDeviceHAL")) ;
    if (xrt_core::dlerror() != NULL) update_device_cb = nullptr ;

    flush_device_cb = (ftype)(xrt_core::dlsym(handle, "flushDeviceHAL")) ;
    if (xrt_core::dlerror() != NULL) flush_device_cb = nullptr ;
  }

  void warning_function()
  {
    // No warnings at this level
  }

  int error_function()
  {
    if(xrt_core::config::get_aie_trace()) {
      xrt_core::message::send(xrt_core::message::severity_level::warning, "XRT",
          "Enabling both AIE Trace and PL Trace is not supported now. Only AIE Trace will be enabled.");
      return 1;
    }
    return 0 ;
  }
} // end namespace device_offload

  void flush_device(void* handle)
  {
    if (device_offload::flush_device_cb != nullptr)
    {
      device_offload::flush_device_cb(handle) ;
    }
  }

  void update_device(void* handle)
  {
    if (device_offload::update_device_cb != nullptr)
    {
      device_offload::update_device_cb(handle) ;
    }
  }

} // end namespace hal
} // end namespace xdp
