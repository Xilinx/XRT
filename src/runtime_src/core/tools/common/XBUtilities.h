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
#include <map>
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

  void setShowHidden(bool _bShowHidden);
  bool getShowHidden();

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
  void can_proceed_or_throw(const std::string& info, const std::string& error);

  void sudo_or_throw(const std::string& msg);
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

  xrt_core::device_collection
  collect_devices(const std::vector<std::string>& _devices, bool _inUserDomain);
              
  xrt_core::device_collection
  collect_devices(const std::string& _devices, bool _inUserDomain);

  boost::property_tree::ptree
  get_available_devices(bool inUserDomain);
  std::string format_base10_shiftdown3(uint64_t value);
  std::string format_base10_shiftdown6(uint64_t value);
  
   /**
   * get_axlf_section() - Get section from the file passed in
   *
   * filename: file containing the axlf section
   *
   * Return: pair of section data and size in bytes
   */
  std::vector<char>
  get_axlf_section(const std::string& filename, axlf_section_kind section);

  /**
   * get_uuids() - Get UUIDs from the axlf section
   *
   * dtbuf: axlf section to be parsed
   *
   * Return: list of UUIDs
   */
  std::vector<std::string> get_uuids(const void *dtbuf);

  int check_p2p_config(const std::shared_ptr<xrt_core::device>& _dev, std::string &err);

  xrt_core::query::reset_type str_to_reset_obj(const std::string& str);

  /**
   * string_to_UUID(): convert a string to hyphen formatted UUID
   * 
   * Returns: 00000000-0000-0000-0000-000000000000 formatted uuid
   */
  std::string string_to_UUID(std::string str);
};

#endif

