#include "plugin/xdp/profile.h"

cb_open_type cb_open;

void register_cb_open (cb_open_type && cb) {
  cb_open = std::move(cb);
}

hal_api_call_logger::hal_api_call_logger() {
    std::cout << "hal_api_call_logger is being called" << std::endl;
    return;
}

hal_api_call_logger::~hal_api_call_logger() {
    return;
}

void test_plugin() {
    std::cout << "test" << std::endl;
}