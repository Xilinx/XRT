/* 
 * SPDX-License-Identifier: Apache-2.0
   Copyright (C) 2019-2021 Xilinx, Inc. All rights reserved.
 */

#ifndef __SYSTEM_UTILS_H__
#define __SYSTEM_UTILS_H__
#if __has_include(<filesystem>)
  #include <filesystem>
  namespace fs = std::filesystem;
#else
  error "Missing the <filesystem> header."
#endif
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

  void makeSystemCall(std::string &operand1, systemOperation operation, std::string operand2 = "", std::string LineNo = "");

}
#endif
