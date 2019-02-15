#include <iostream>
#include "xdp_hal_plugin_interface.h"
#include "xdp_hal_plugin.h"
#include "driver/xclng/xrt/user_gem/plugin/xdp/hal_profile.h"

void alloc_bo_start(void* payload) {
    std::cout << "alloc bo start" << std::endl;
    CallbackMarker* payload = reinterpret_cast<CallbackMarker*>(payload);
    std::cout << "idcode: " << payload->idcode << std::endl;
    std::cout << "devcode: " << payload->devcode << std::endl;
    return;
}

void alloc_bo_end(void* payload) {
    std::cout << "alloc bo end" << std::endl;
    return;
}

void unknown_cb_type(void* payload) {
    std::cout << "unknown cb type" << std::endl;
    return;
}

void hal_level_xdp_cb_func(HalCallbackType cb_type, void* payload) {
    std::cout << "a callback is called" << std::endl;
    switch (cb_type) {
        case HalCallbackType::ALLOC_BO_START:
            alloc_bo_start(payload);
            break;
        case HalCallbackType::ALLOC_BO_END:
            alloc_bo_end(payload);
            break;
        default: 
            unknown_cb_type(payload);
            break;
    }
    return;
}