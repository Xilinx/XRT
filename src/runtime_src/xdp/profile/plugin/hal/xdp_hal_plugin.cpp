#include <iostream>
#include "xdp_hal_plugin_interface.h"
#include "xdp_hal_plugin.h"
#include "driver/xclng/xrt/user_gem/plugin/xdp/hal_profile.h"

void hal_level_xdp_cb_func(HalCallbackType cb_type, void* payload) {
    std::cout << "the probe callback is called" << std::endl;
    return;
}
