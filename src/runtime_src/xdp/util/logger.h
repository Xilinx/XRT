#ifndef XDP_UTIL_LOGGER_H_
#define XDP_UTIL_LOGGER_H_

#include <fstream>
#include <iostream>

#include "xrt/util/config_reader.h"

namespace xdp {

extern std::ofstream xdp_log_file;

void init_xdp_log();

void close_xdp_log();

}

#endif