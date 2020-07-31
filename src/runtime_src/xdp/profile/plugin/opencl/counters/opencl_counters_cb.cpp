
#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/opencl/counters/opencl_counters_cb.h"
#include "xdp/profile/plugin/opencl/counters/opencl_counters_plugin.h"
#include "core/common/time.h"

namespace xdp {

  static OpenCLCountersProfilingPlugin openclCountersPluginInstance ;

  static void log_function_call_start(const char* functionName)
  {
    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    auto timestamp = xrt_core::time_ns() ;

    (db->getStats()).logFunctionCallStart(functionName, timestamp) ;
  }

  static void log_function_call_end(const char* functionName)
  {
    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    auto timestamp = xrt_core::time_ns() ;

    (db->getStats()).logFunctionCallEnd(functionName, timestamp) ;
  }

} // end namespace xdp

extern "C"
void log_function_call_start(const char* functionName)
{
  xdp::log_function_call_start(functionName) ;
}

extern "C"
void log_function_call_end(const char* functionName)
{
  xdp::log_function_call_end(functionName) ;
}
