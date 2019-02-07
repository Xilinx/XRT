#ifndef XDP_HAL_PLUGIN_H_
#define XDP_HAL_PLUGIN_H_

#include <functional>
#include <iostream>

namespace xocl {

namespace xdp {

using cb_open_type = std::function<void(void)>;

void register_cb_open(cb_open_type && cb);

class HalCallLogger
{
  HalCallLogger(int x);
  ~HalCallLogger();
};

} //  xdp

} //  xocl

#define XDP_LOG_API_CALL(x) xocl::xdp::HalCallLogger hal_plugin_object(x);

#endif