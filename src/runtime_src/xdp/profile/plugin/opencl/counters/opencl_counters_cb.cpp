
#include <map>
//#include <iostream>

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

    (db->getStats()).logFunctionCallStart(functionName, timestamp) ;
  }

  static void log_function_call_end(const char* functionName)
  {
    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    auto timestamp = xrt_core::time_ns() ;

    (db->getStats()).logFunctionCallEnd(functionName, timestamp) ;
  }

  static void log_kernel_execution(const char* kernelName, bool isStart)
  {
    static std::map<std::string, uint64_t> storedTimestamps ;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    uint64_t timestamp = xrt_core::time_ns() ;

    if (getFlowMode() == HW_EMU)
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

  static void log_compute_unit_execution(const char* cuName,
					 const char* localWorkGroup,
					 const char* globalWorkGroup,
					 bool isStart)
  {
    static std::map<std::tuple<std::string, std::string, std::string>, 
		    uint64_t> storedTimestamps ;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    uint64_t timestamp = xrt_core::time_ns() ;

    if (getFlowMode() == HW_EMU)
    {
      timestamp =
	openclCountersPluginInstance.convertToEstimatedTimestamp(timestamp) ;
    }

    std::tuple<std::string, std::string, std::string> combinedName =
      std::make_tuple(cuName, localWorkGroup, globalWorkGroup) ;

    if (isStart)
    {
      storedTimestamps[combinedName] = timestamp ;
    }
    else
    {
      auto executionTime = timestamp - storedTimestamps[combinedName] ;
      storedTimestamps.erase(combinedName) ;

      (db->getStats()).logComputeUnitExecution(cuName,
					       localWorkGroup,
					       globalWorkGroup,
					       executionTime) ;
    }
  }

  static void counter_action_read(uint64_t contextId,
				  const char* deviceName,
				  uint64_t size,
				  bool isStart)
  {
    static std::map<std::pair<uint64_t, std::string>, uint64_t> 
      storedTimestamps ;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    uint64_t timestamp = xrt_core::time_ns() ;

    std::pair<uint64_t, std::string> identifier =
      std::make_pair(contextId, std::string(deviceName)) ;

    if (isStart)
    {
      storedTimestamps[identifier] = timestamp ;
    }
    else
    {
      uint64_t transferTime = timestamp - storedTimestamps[identifier] ;
      storedTimestamps.erase(identifier) ;

      uint64_t deviceId = 0 ; // TODO - lookup the device ID from device name

      (db->getStats()).logHostRead(contextId, deviceId, size, transferTime) ;
    }
  }

  static void counter_action_write(uint64_t contextId,
				   const char* deviceName,
				   uint64_t size,
				   bool isStart)
  {
    static std::map<std::pair<uint64_t, std::string>, uint64_t> 
      storedTimestamps ;

    VPDatabase* db = openclCountersPluginInstance.getDatabase() ;
    uint64_t timestamp = xrt_core::time_ns() ;

    std::pair<uint64_t, std::string> identifier =
      std::make_pair(contextId, std::string(deviceName)) ;

    if (isStart)
    {
      storedTimestamps[identifier] = timestamp ;
    }
    else
    {
      uint64_t transferTime = timestamp - storedTimestamps[identifier] ;
      storedTimestamps.erase(identifier) ;

      uint64_t deviceId = 0 ; // TODO - lookup the device ID from device name

      (db->getStats()).logHostWrite(contextId, deviceId, size, transferTime) ;
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
void log_kernel_execution(const char* kernelName, bool isStart)
{
  xdp::log_kernel_execution(kernelName, isStart) ;
}

extern "C"
void log_compute_unit_execution(const char* cuName,
				const char* localWorkGroupConfiguration,
				const char* globalWorkGroupConfiguration,
				bool isStart)
{
  xdp::log_compute_unit_execution(cuName,
				  localWorkGroupConfiguration,
				  globalWorkGroupConfiguration,
				  isStart) ;
}

extern "C"
void counter_action_read(unsigned long int contextId,
			 const char* deviceName,
			 unsigned long int size,
			 bool isStart)
{
  xdp::counter_action_read(contextId, deviceName, size, isStart);
}

extern "C"
void counter_action_write(unsigned long int contextId,
			  const char* deviceName,
			  unsigned long int size,
			  bool isStart)
{
  xdp::counter_action_write(contextId, deviceName, size, isStart);
}
