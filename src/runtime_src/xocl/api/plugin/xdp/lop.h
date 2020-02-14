/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef LOP_DOT_H
#define LOP_DOT_H

/**
 * This file contains the callback mechanisms for connecting the OpenCL
 * layer to the low overhead profiling XDP plugin.
 */

#include <functional>
#include <atomic>

// This namespace contains the functions responsible for loading and
//  linking the LOP functions.
namespace xdplop {

  // The top level function that loads the library.  This should
  //  only be executed once
  void load_xdp_lop() ;

  // The function that makes connections via dynamic linking and dynamic symbols
  void register_lop_functions(void* handle) ;
  
  // Every OpenCL API we are interested in will have an instance
  //  of this class constructed at the start
  class LOPFunctionCallLogger
  {
  private:
    static std::atomic<unsigned int> m_funcid_global ;

    unsigned int m_funcid ;
    const char* m_name = nullptr ;
    long long int m_address = 0 ;
  public:
    LOPFunctionCallLogger(const char* function) ;
    LOPFunctionCallLogger(const char* function, long long int address) ;
    ~LOPFunctionCallLogger() ;
  } ;

} // end namespace xdplop

// Helpful defines
#define LOP_LOG_FUNCTION_CALL xdplop::LOPFunctionCallLogger LOPObject(__func__);
#define LOP_LOG_FUNCTION_CALL_WITH_QUEUE(Q) xdplop::LOPFunctionCallLogger LOPObject(__func__, (long long int)Q);

#endif
