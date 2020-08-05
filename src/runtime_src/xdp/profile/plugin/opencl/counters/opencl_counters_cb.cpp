
#include <map>

#include "xdp/profile/database/database.h"
#include "xdp/profile/plugin/opencl/counters/opencl_counters_cb.h"
#include "xdp/profile/plugin/opencl/counters/opencl_counters_plugin.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include "core/common/time.h"

namespace xdp {

  static OpenCLCountersProfilingPlugin openclCountersPluginInstance ;

  static void log_function_call_start(const char* functionName)
  {
    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    auto timestamp = xrt_core::time_ns() ;

    if (getFlowMode() == SW_EMU || getFlowMode() == HW_EMU)
    {
      timestamp = 
	openclCountersPluginInstance.convertToEstimatedTimestamp(timestamp) ;
    }

    (db->getStats()).logFunctionCallStart(functionName, timestamp) ;
  }

  static void log_function_call_end(const char* functionName)
  {
    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    auto timestamp = xrt_core::time_ns() ;

    if (getFlowMode() == SW_EMU || getFlowMode() == HW_EMU)
    {
      timestamp = 
	openclCountersPluginInstance.convertToEstimatedTimestamp(timestamp) ;
    }

    (db->getStats()).logFunctionCallEnd(functionName, timestamp) ;
  }

  static void counter_action_ndrange(const char* kernelName, bool isStart)
  {
    static std::map<std::string, uint64_t> storedTimestamps ;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    uint64_t timestamp = xrt_core::time_ns() ;

    if (getFlowMode() == SW_EMU || getFlowMode() == HW_EMU)
    {
      timestamp = 
	openclCountersPluginInstance.convertToEstimatedTimestamp(timestamp) ;
    }

    if (isStart)
    {
      storedTimestamps[kernelName] = timestamp ;
    }
    else
    {
      auto executionTime = timestamp - storedTimestamps[kernelName] ;
      storedTimestamps.erase(kernelName) ;

      (db->getStats()).logKernelExecution(kernelName, executionTime) ;
    }
    
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

extern "C"
void counter_action_ndrange(const char* kernelName, bool isStart)
{
  xdp::counter_action_ndrange(kernelName, isStart) ;
}
