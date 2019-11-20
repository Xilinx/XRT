#ifndef XDP_HAL_PLUGIN_INTERFACE_H_
#define XDP_HAL_PLUGIN_INTERFACE_H_

#include "xclperf.h"

extern "C" {

void hal_level_xdp_cb_func(HalCallbackType cb_type, void* payload);

}

#endif
