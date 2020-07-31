
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
  } ;

} // end namespace xdp

#endif
