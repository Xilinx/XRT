#ifndef XDP_PROFILE_HAL_PLUGIN_H_
#define XDP_PROFILE_HAL_PLUGIN_H_

#include <functional>
#include <iostream>

namespace xdphal {

using cb_probe_type = std::function<void()>;

extern cb_probe_type cb_test_probe;

extern int test;

void register_cb_probe(cb_probe_type cb);

class HalCallLogger {
public:
  HalCallLogger(int x);
  ~HalCallLogger();
  static bool loaded;
};

void load_xdp_plugin_library();

}

#define XDP_LOG_API_CALL(x) HalCallLogger hal_plugin_object(x);

#endif