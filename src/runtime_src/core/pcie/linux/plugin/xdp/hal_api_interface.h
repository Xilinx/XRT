
#ifndef XDP_PROFILE_HAL_INTERFACE_PLUGIN_H_
#define XDP_PROFILE_HAL_INTERFACE_PLUGIN_H_

#include <functional>
#include <iostream>
#include <atomic>
#include <mutex>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include "core/include/xclperf.h"
#include "core/include/xclhal2.h"

namespace xdphalinterface {

  /**
   * This function type definition is used for
   * dynamically loading the plugin function.
   */
  typedef void(*cb_load_func_type)(unsigned, void*);

  /**
   * This standard function is for storing the function
   * loaded. Using cpp standard for robustness across 
   * function calls and context sharing.
   */
  using cb_func_type = std::function<void(unsigned, void*)>;

  class StartDeviceProfilingCls
  {
  public:
    StartDeviceProfilingCls(xclDeviceHandle handle);
    ~StartDeviceProfilingCls();
  };
  
  class CreateProfileResultsCls
  {
  public:
    CreateProfileResultsCls(xclDeviceHandle handle, ProfileResults**, int& status);
    ~CreateProfileResultsCls();
  };
  
  class GetProfileResultsCls
  {
  public:
    GetProfileResultsCls(xclDeviceHandle handle, ProfileResults*, int& status);
    ~GetProfileResultsCls();
  };
  
  class DestroyProfileResultsCls
  {
  public:
    DestroyProfileResultsCls(xclDeviceHandle handle, ProfileResults*, int& status);
    ~DestroyProfileResultsCls();
  };

  class APIInterfaceLoader
  {
  public:
    APIInterfaceLoader() ;
    ~APIInterfaceLoader() ;
  } ;
  
  void load_xdp_hal_interface_plugin_library(HalPluginConfig* config);
  void register_hal_interface_callbacks(void* handle) ;
  int error_hal_interface_callbacks() ;

} //  xdphalinterface

/**
 * The declaration of the macros to be inserted into 
 * the shim implementations
 */

#define START_DEVICE_PROFILING_CB(handle) xdphalinterface::StartDeviceProfilingCls start_device_profiling_inst(handle);
#define CREATE_PROFILE_RESULTS_CB(handle, results, status) xdphalinterface::CreateProfileResultsCls create_profile_results_inst(handle, results, status);
#define GET_PROFILE_RESULTS_CB(handle, results, status) xdphalinterface::GetProfileResultsCls get_profile_results_inst(handle, results, status);
#define DESTROY_PROFILE_RESULTS_CB(handle, results, status) xdphalinterface::DestroyProfileResultsCls destroy_profile_results_inst(handle, results, status);

#endif
