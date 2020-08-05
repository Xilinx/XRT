
#ifndef OPENCL_COUNTERS_PLUGIN_DOT_H
#define OPENCL_COUNTERS_PLUGIN_DOT_H

#include "xdp/profile/plugin/vp_base/vp_base_plugin.h"

namespace xdp {

  class OpenCLCountersProfilingPlugin : public XDPPlugin
  {
  private:
  public:
    OpenCLCountersProfilingPlugin() ;
    ~OpenCLCountersProfilingPlugin() ;

    // For emulation based flows we need to convert real time into
    //  estimated device time to match what we reported previously
    uint64_t convertToEstimatedTimestamp(uint64_t realTimeStamp) ;
  } ;

} // end namespace xdp

#endif
