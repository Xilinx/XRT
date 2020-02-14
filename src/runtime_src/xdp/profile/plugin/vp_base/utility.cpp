
#include <chrono>
#include <ctime>

#include "xdp/profile/plugin/vp_base/utility.h"

namespace xdp {

  std::string getCurrentDateTime()
  {
    auto time = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());

    struct tm *p_tstruct = std::localtime(&time);
    if(p_tstruct) {
        char buf[80] = {0};
        strftime(buf, sizeof(buf), "%Y-%m-%d %X", p_tstruct);
        return std::string(buf);
    }
    return std::string("0000-00-00 0000");
  }

  const char* getToolVersion()
  {
    return "2019.2" ;
  }

} // end namespace xdp
