
#include "xocl/core/platform.h"
#include "xocl/core/device.h"

#include "xdp/profile/plugin/opencl/counters/opencl_counters_plugin.h"
#include "xdp/profile/writer/opencl/opencl_summary_writer.h"

namespace xdp {

  OpenCLCountersProfilingPlugin::OpenCLCountersProfilingPlugin() : XDPPlugin()
  {
    db->registerPlugin(this) ;

    // OpenCL could be running hardware emulation or software emulation,
    //  so be sure to account for any peculiarities here.
    emulationSetup() ;

    writers.push_back(new OpenCLSummaryWriter("opencl_summary.csv")) ;
    (db->getStaticInfo()).addOpenedFile("opencl_summary.csv", "PROFILE_SUMMARY") ;

    platform = xocl::get_shared_platform() ;
  }

  OpenCLCountersProfilingPlugin::~OpenCLCountersProfilingPlugin()
  {
    if (VPDatabase::alive())
    {
      // Before writing, make sure that counters are read.
      db->broadcast(VPDatabase::READ_COUNTERS, nullptr) ;
      for (auto w : writers)
      {
	w->write(false) ;
      }
      db->unregisterPlugin(this) ;
    }
  }

  // This function is only called in hardware emulation.  For hardware
  //  emulation there should only ever be one device.
  uint64_t OpenCLCountersProfilingPlugin::convertToEstimatedTimestamp(uint64_t realTimestamp)
  {
    uint64_t convertedTimestamp = realTimestamp ;

    auto device = platform->get_device_range()[0] ;
    uint64_t deviceTimestamp = device->get_xrt_device()->getDeviceTime().get() ;

    if (deviceTimestamp != 0)
      convertedTimestamp = deviceTimestamp ;

    return convertedTimestamp ;
  }

} // end namespace xdp
