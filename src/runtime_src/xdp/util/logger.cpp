#include "xdp/util/logger.h"

namespace xdp {

std::ofstream xdp_log_file;

void init_xdp_log() {
    std::string xdp_log_filename = xrt::config::get_xdp_logging();
    if (xdp_log_filename != "null") {
        xdp_log_file.open(xdp_log_filename);
    }
    return;
}

void close_xdp_log() {
    if (xdp_log_file.is_open()) {
        xdp_log_file.close();
    }
}

}