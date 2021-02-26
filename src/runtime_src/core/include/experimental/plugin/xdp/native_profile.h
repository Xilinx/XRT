/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef NATIVE_PROFILE_DOT_H
#define NATIVE_PROFILE_DOT_H

/**
 * This file contains the callback mechanisms for connecting the
 * Native XRT API (C layer) to the XDP plugin
 */

namespace xdpnative {

  // The top level function that loads the library.  This should only
  //  be executed once
  void load_xdp_native() ;

  void register_native_functions(void* handle) ;

  void native_warning_function() ;

  class NativeFunctionCallLogger
  {
  private:
    unsigned int m_funcid ;
    const char* m_name = nullptr ;
    const char* m_type = nullptr ;
  public:
    NativeFunctionCallLogger(const char* function, const char* type = nullptr) ;
    ~NativeFunctionCallLogger() ;
  } ;

  // In order to capture object functions like constructors, we need
  //  two different hooks
  void profiling_start(void* object, const char* function, const char* type);
  void profiling_end(void* object, const char* function, const char* type);

} // end namespace xdpnative

#define NATIVE_LOG_FUNCTION_CALL xdpnative::NativeFunctionCallLogger LogObject(__func__);
#define NATIVE_MEMBER_LOG_FUNCTION_CALL xdpnative::NativeFunctionCallLogger LogObject(__func__, typeid(*this).name());

#endif
