#ifndef XDP_UTIL_LOGGER_H_
#define XDP_UTIL_LOGGER_H_

#include <fstream>

#include "xrt/util/config_reader.h"

namespace xdp {

extern std::ofstream log_file;

void init_xdp_log();

}

#endif