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
#include <string>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/program_options.hpp>



namespace XBUtilities {

template<typename ... Args>

std::string format(const std::string& format, Args ... args) {
  const static size_t NULL_CHAR_SIZE = 1;
  size_t size = NULL_CHAR_SIZE + snprintf(nullptr, 0, format.c_str(), args ...);
  std::unique_ptr<char[]> buf(new char[size]);
  snprintf(buf.get(), size, format.c_str(), args ...);
  
  return std::string(buf.get());
}

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

  // ---------
  std::string create_usage_string( const std::string &_executableName,
                                   const std::string &_subCommand,
                                   const boost::program_options::options_description &_od);
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
  void subcommand_help( const std::string &_executableName,
                        const std::string &_subCommand,
                        const std::string &_description, 
                        const boost::program_options::options_description &_od, 
                        const std::string &_examples);
};

#endif

