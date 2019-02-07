#include "plugin/xdp/profile.h"

cb_open_type cb_open;

void register_cb_open (cb_open_type && cb) {
  cb_open = std::move(cb);
}

HalCallLogger::HalCallLogger(int x) {
    std::cout << "hal_api_call_logger is being called" << std::endl;
    return;
}

HalCallLogger::~HalCallLogger() {
    return;
}