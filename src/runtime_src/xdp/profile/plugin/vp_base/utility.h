
#ifndef UTILITY_DOT_H
#define UTILITY_DOT_H

#include <string>

// Functions that can be used in the database, the plugins, and the writers

namespace xdp {

  std::string getCurrentDateTime() ;
  const char* getToolVersion() ;

} // end namespace xdp

#endif
