/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include "core/common/device.h"

#include <string>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

namespace XBUtilities {
  typedef enum {
    MT_MESSAGE,
    MT_INFO,
    MT_WARNING,
    MT_ERROR,
    MT_VERBOSE,
    MT_FATAL,
    MT_TRACE,
    MT_UNKNOWN, 
  } MessageType;

  /**
   * Enables / Disables verbosity
   * 
   * @param _bVerbose true - enable verbosity
   *                  false - disable verbosity (default)
   */
  void setVerbose(bool _bVerbose);
  void setTrace(bool _bVerbose);
  void disable_escape_codes( bool _disable );
  bool is_esc_enabled();  

  void message_(MessageType _eMT, const std::string& _msg, bool _endl = true);

  void message(const std::string& _msg, bool _endl = true); 
  void info(const std::string& _msg, bool _endl = true);
  void warning(const std::string& _msg, bool _endl = true);
  void error(const std::string& _msg, bool _endl = true);
  void verbose(const std::string& _msg, bool _endl = true);
  void fatal(const std::string& _msg, bool _endl = true);
  void trace(const std::string& _msg, bool _endl = true);

  void trace_print_tree(const std::string & _name, 
                        const boost::property_tree::ptree & _pt);

  bool can_proceed();
  // ---------
  void wrap_paragraph( const std::string & _unformattedString, 
                       unsigned int _indentWidth, 
                       unsigned int _columnWidth, 
                       bool _indentFirstLine,
                       std::string &_formattedString);
  void wrap_paragraphs( const std::string & _unformattedString, 
                        unsigned int _indentWidth, 
                        unsigned int _columnWidth, 
                        bool _indentFirstLine,
                        std::string &_formattedString);

  void collect_devices( const std::set<std::string>  &_deviceBDFs,
                        bool _inUserDomain,
                        xrt_core::device_collection &_deviceCollection);
  void report_available_devices();
};

#endif

