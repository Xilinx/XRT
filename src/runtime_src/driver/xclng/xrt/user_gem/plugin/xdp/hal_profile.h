#ifndef XDP_PROFILE_HAL_PLUGIN_H_
#define XDP_PROFILE_HAL_PLUGIN_H_

#include <functional>
#include <iostream>

namespace xdphal {

using cb_probe_func_type = std::function<void()>;
using cb_open_func_type = std::function<void(int)>;
using cb_close_func_type = std::function<void(int)>;
using cb_load_xclbin_func_type = std::function<void()>;
using cb_lock_device_func_type = std::function<void()>;

typedef void (* cb_probe_load_func_type)();

extern cb_probe_func_type cb_probe;

class HalCallLogger {
public:
  HalCallLogger();
  ~HalCallLogger();
  static bool loaded;
};

void load_xdp_plugin_library();

} //  xdphal


/**
 * The declaration of the macros
 * to be inserted into the shim 
 * implementations
 */
#define XDP_LOG_PROBE_CALL() xdphal::HalCallLogger hal_plugin_object();

#endif