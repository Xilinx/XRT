/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef __XBUtilities_h_
#define __XBUtilities_h_

// Include files
// Please keep these to the bare minimum
#include <string>
#include <memory>

namespace XBUtilities {

template<typename ... Args>

std::string format(const std::string& format, Args ... args) {
  size_t size = 1 + snprintf(nullptr, 0, format.c_str(), args ...);
  std::unique_ptr<char[]> buf(new char[size]);
  snprintf(buf.get(), size, format.c_str(), args ...);
  
  return std::string(buf.get(), buf.get() + size);
}


  /**
   * Enables / Disables verbosity
   * 
   * @param _bVerbose true - enable verbosity
   *                  false - disable verbosity (default)
   */
  void setVerbose(bool _bVerbose);

  void message(const std::string& _msg, bool _endl = true);
  void error(const std::string& _msg, bool _endl = true);
  void warning(const std::string& _msg, bool _endl = true);
  void verbose(const std::string& _msg, bool _endl = true);
};

#endif

