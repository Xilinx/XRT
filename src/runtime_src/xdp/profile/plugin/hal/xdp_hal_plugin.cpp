#include <iostream>
#include "xdp_hal_plugin_interface.h"
#include "xdp_hal_plugin.h"
#include "driver/xclng/xrt/user_gem/plugin/xdp/profile.h"

void probe_cb_func() {
    std::cout << "the probe callback is called" << std::endl;
    return;
}

void register_cb_funcs() {
    std::cout << "register_cb_funcs called" << std::endl;
    register_cb_probe(probe_cb_func);
    return;
}

void init_xdp_hal_plugin() {
    std::cout << "init_xdp_hal_plugin called" << std::endl;
    register_cb_funcs();
    return;
}

