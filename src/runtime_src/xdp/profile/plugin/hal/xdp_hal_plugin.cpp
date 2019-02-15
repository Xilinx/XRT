#include <iostream>
#include "xdp_hal_plugin_interface.h"
#include "xdp_hal_plugin.h"
#include "driver/xclng/xrt/user_gem/plugin/xdp/hal_profile.h"

void alloc_bo_start(void* payload) {
    return;
}

void alloc_bo_end(void* payload) {
    return;
}

void unknown_cb_type(void* payload) {
    return;
}

void hal_level_xdp_cb_func(HalCallbackType cb_type, void* payload) {
    std::cout << "the probe callback is called" << std::endl;
    switch (cb_type) {
        case HalCallbackType::ALLOC_BO_START:
            alloc_bo_start(payload);
            break;
        default: 
            unknown_cb_type(payload);
            break;
    }
    return;
}