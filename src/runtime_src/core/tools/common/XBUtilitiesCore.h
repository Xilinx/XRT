// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __XBUtilitiesCore_h_
#define __XBUtilitiesCore_h_

#include <string>
#include <memory>
#include <map>
#include <iostream>
#include <vector>

#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

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
  bool getVerbose();

  void setElf(bool _bElf);
  bool getElf();

  void setTrace(bool _bVerbose);

  void setAdvance(bool _bAdvance);
  bool getAdvance();

  void setForce(bool _bForce);
  bool getForce();

  void disable_escape_codes( bool _disable );
  bool is_escape_codes_disabled();  

  void message_(MessageType _eMT, const std::string& _msg, bool _endl = true, std::ostream & _ostream = std::cout);

  void message(const std::string& _msg, bool _endl = true, std::ostream & _ostream = std::cout); 
  void info(const std::string& _msg, bool _endl = true);
  void warning(const std::string& _msg, bool _endl = true);
  void error(const std::string& _msg, bool _endl = true);
  void verbose(const std::string& _msg, bool _endl = true);
  void verbose(const boost::format& _msg, bool _endl = true);
  void fatal(const std::string& _msg, bool _endl = true);
  void trace(const std::string& _msg, bool _endl = true);

  void trace_print_tree(const std::string & _name, 
                        const boost::property_tree::ptree & _pt);
  std::string wrap_paragraphs( const std::string & unformattedString,
                               unsigned int indentWidth,
                               unsigned int columnWidth,
                               bool indentFirstLine);


  bool can_proceed(bool force = false);
  void sudo_or_throw(const std::string& msg);
  void throw_cancel(const std::string& msg);
  void throw_cancel(const boost::format& format);

  template <typename T>
  std::vector<T> as_vector( boost::property_tree::ptree const& pt, 
                            boost::property_tree::ptree::key_type const& key) 
  {
    std::vector<T> r;

    boost::property_tree::ptree::const_assoc_iterator it = pt.find(key);

    if( it != pt.not_found()) {
      for (auto& item : pt.get_child(key)) 
        r.push_back(item.second);
    }
    return r;
  }
};

#endif

