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

#include <vector>
#include <string>
#include <chrono>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#define XDP_SOURCE

#include "xdp/profile/writer/vp_base/vp_run_summary.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info_database.h"

#include "core/common/time.h"

namespace xdp {

  VPRunSummaryWriter::VPRunSummaryWriter(const char* filename) :
    VPWriter(filename)
  {
  }

  VPRunSummaryWriter::~VPRunSummaryWriter()
  {
  }

  void VPRunSummaryWriter::switchFiles()
  {
    // Don't actually do anything
  }

  void VPRunSummaryWriter::write(bool /*openNewFile*/)
  {
    // Ignore openNewFile
    
    // We could be called to write multiple times, so refresh before
    //  we dump
    refreshFile() ;

    if (!fout) return ;

    

    // Collect all the files that have been created in this host execution
    //  run and dump their information in the run summary file
    std::vector<std::pair<std::string, std::string> > files = 
      (db->getStaticInfo()).getOpenedFiles() ;

    // If there are no files, don't dump anything
    if (files.empty()) return ; 

    boost::property_tree::ptree ptRunSummary ;
    {
      boost::property_tree::ptree ptSchema ;
      ptSchema.put("major", "1") ;
      ptSchema.put("minor", "1") ;
      ptSchema.put("patch", "0") ; 
      ptRunSummary.add_child("schema_version", ptSchema) ;
    }

    {
      auto pid = (db->getStaticInfo()).getPid() ;
      auto timestamp = (std::chrono::system_clock::now()).time_since_epoch() ;
      auto value =
	std::chrono::duration_cast<std::chrono::milliseconds>(timestamp) ;
      uint64_t timeMsec = value.count() ;

      boost::property_tree::ptree ptGeneration ;
      ptGeneration.put("source", "vp") ;
      ptGeneration.put("PID", std::to_string(pid)) ;
      ptGeneration.put("timestamp", std::to_string(timeMsec)) ;
      ptRunSummary.add_child("generation", ptGeneration) ;
    }

    boost::property_tree::ptree ptFiles ;
    for (auto f : files)
    {
      boost::property_tree::ptree ptFile ;
      ptFile.put("name", f.first.c_str()) ;
      ptFile.put("type", f.second.c_str()) ;
      ptFiles.push_back(std::make_pair("", ptFile)) ;
    }
    ptRunSummary.add_child("files", ptFiles) ;

    // Add the system diagram information if available
    std::string systemDiagram = (db->getStaticInfo()).getSystemDiagram() ;
    if (systemDiagram != "")
    {
      boost::property_tree::ptree ptSystemDiagram ;
      ptSystemDiagram.put("payload_16bitEnc", systemDiagram.c_str()) ;
      ptRunSummary.add_child("system_diagram", ptSystemDiagram) ;
    }

    boost::property_tree::write_json(fout, ptRunSummary, true) ;
  }

} // end namespace xdp

