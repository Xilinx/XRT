
#ifndef AIE_TRACE_CONFIG_DOT_H
#define AIE_TRACE_CONFIG_DOT_H

#include <string>

namespace xdp {
namespace built_in {

  enum MetricSet { FUNCTIONS, PARTIAL_STALLS, ALL_STALLS, ALL } ;

  enum ErrorCode {
    
  } ;

  struct ConfigurationParameters
  {
    std::string delayStr; // xrt_core::config::get_aie_trace_start_delay
    std::string counterScheme; // xrt_core::config::get_aie_trace_counter_scheme
    MetricSet metric;
    bool userControl;
  } ;

} // end namespace built_in
} // end namespace xdp

#endif
