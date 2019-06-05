#ifndef __SYSTEM_UTILS_H__
#define __SYSTEM_UTILS_H__
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

namespace systemUtil {
  
  enum systemOperation 
  {
    CREATE      = 0,
    REMOVE      = 1,
    COPY        = 2,
    APPEND      = 3,
    UNZIP       = 4,
    PERMISSIONS = 5
  };

  void makeSystemCall(std::string &operand1, systemOperation operation, std::string operand2 = "");

}
#endif
