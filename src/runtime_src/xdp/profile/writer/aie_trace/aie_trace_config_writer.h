/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef AIE_TRACE_CONFIG_WRITER_DOT_H
#define AIE_TRACE_CONFIG_WRITER_DOT_H

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <iostream>
#include <string>
#include <regex>
#include <map>
#include "xdp/profile/writer/vp_base/vp_writer.h"

namespace bpt = boost::property_tree;

namespace xdp {

  class AieTraceConfigWriter : public VPWriter
  {
  private:
    uint64_t deviceIndex;
  public:
    AieTraceConfigWriter(const char* filename, uint64_t index);
    ~AieTraceConfigWriter();

  private:
  inline void write_jsonEx(const std::string& path, const bpt::ptree& ptree)
  {
    std::ostringstream oss;
    bpt::write_json(oss, ptree);

    // Remove quotes from value strings
    //   Patterns matched - "12" "null" "100.0" "-1" ""
    //   Patterns ignored - "12": "100.0":
    std::regex reg("\\\"((-?[0-9]+\\.{0,1}[0-9]*)|(null)|())\\\"(?!\\:)");
    std::string result = std::regex_replace(oss.str(), reg, "$1");

    std::ofstream file;
    file.open(path);
    file << result;
    file.close();
  }

  virtual bool write(bool openNewFile) ;
  } ;


} // end namespace xdp

#endif