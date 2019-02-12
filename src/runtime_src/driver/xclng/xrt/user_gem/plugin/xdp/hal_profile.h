#ifndef XDP_PROFILE_HAL_PLUGIN_H_
#define XDP_PROFILE_HAL_PLUGIN_H_

#include <functional>
#include <iostream>

namespace xdphal {

using cb_probe_type = std::function<void()>;

typedef void (* cb_probe_load_type)();

extern cb_probe_type cb_test_probe;

extern int test;

void register_cb_probe(cb_probe_type cb);

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