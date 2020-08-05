
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

  uint64_t OpenCLCountersProfilingPlugin::convertToEstimatedTimestamp(uint64_t realTimeStamp)
  {
    return realTimeStamp ;
  }

} // end namespace xdp
