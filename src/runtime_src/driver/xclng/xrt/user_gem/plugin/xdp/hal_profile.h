#ifndef XDP_PROFILE_HAL_PLUGIN_H_
#define XDP_PROFILE_HAL_PLUGIN_H_

#include <functional>
#include <iostream>
#include <atomic>
#include <mutex>
#include <dlfcn.h>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include "driver/include/xclperf.h"
#include "driver/include/xclhal2.h"

namespace xdphal {

typedef void(*cb_load_func_type)(unsigned, void*);

using cb_func_type = std::function<void(unsigned, void*)>;

class AllocBOCallLogger {
public:
  AllocBOCallLogger(size_t size, xclBOKind domain, unsigned flags);
  ~AllocBOCallLogger();
  unsigned local_idcode;
};

void load_xdp_plugin_library();

} //  xdphal

/**
 * The declaration of the macros to be inserted into 
 * the shim implementations
 */
#define ALLOC_BO_CB xdphal::AllocBOCallLogger alloc_bo_call_logger(size, domain, flags);

#endif