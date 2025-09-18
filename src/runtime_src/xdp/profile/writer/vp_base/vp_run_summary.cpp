/**
 * Copyright (C) 2016-2020 Xilinx, Inc
 * Copyright (C) 2022-2025 Advanced Micro Devices, Inc - All rights reserved
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

#define XDP_CORE_SOURCE

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "xdp/profile/writer/vp_base/vp_run_summary.h"
#include "xdp/profile/database/database.h"
#include "xdp/profile/database/static_info_database.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include "core/common/time.h"
#include "core/common/message.h"

namespace xdp {

  VPRunSummaryWriter::VPRunSummaryWriter(const char* filename, VPDatabase* inst)
    : VPWriter(filename, inst, false)
  {
  }

  void VPRunSummaryWriter::switchFiles()
  {
    // Don't actually do anything
  }

  bool VPRunSummaryWriter::write(bool /*openNewFile*/)
  {
    // Ignore openNewFile
    
    xrt_core::message::send(xrt_core::message::severity_level::debug, "XRT",
                            "VPRunSummaryWriter: write contents");

    // We could be called to write multiple times, so refresh before
    //  we dump
    refreshFile();

    if (!fout)
      return false;

    // Collect all the files that have been created in this host execution
    //  run and dump their information in the run summary file
    auto files = db->getOpenedFiles();
    auto contexts = db->getContextMapping();

    // If there are no files, don't dump anything
    if (files.empty())
      return false;

    boost::property_tree::ptree ptRunSummary;
    {
      boost::property_tree::ptree ptSchema;
      ptSchema.put("major", "1");
      ptSchema.put("minor", "5");
      ptSchema.put("patch", "0");
      ptRunSummary.add_child("schema_version", ptSchema);
    }

    {
      auto pid = db->getStaticInfo().getPid();
      bool aieApplication = db->getStaticInfo().getAieApplication();
      auto msecSinceEpoch = getMsecSinceEpoch();

      std::string pathToFile = "";

#ifdef _WIN32
      char buffer[MAX_PATH];
      char* result = _getcwd(buffer, sizeof(buffer));
#else
      char buffer[PATH_MAX];
      char* result = getcwd(buffer, sizeof(buffer));
#endif
      if (result != nullptr) {
        pathToFile = buffer;
        pathToFile += separator; // From base class
        pathToFile += getcurrentFileName();
      }

      boost::property_tree::ptree ptGeneration;
      if (pathToFile != "")
        ptGeneration.put("this_file", pathToFile);

      ptGeneration.put("source", "vp");
      ptGeneration.put("PID", std::to_string(pid));
      ptGeneration.put("timestamp", msecSinceEpoch);

      auto flow = getFlowMode();
      switch (flow) {
      case SW_EMU:
        ptGeneration.put("target", "TT_SW_EMU");
        break;
      case HW_EMU:
        ptGeneration.put("target", "TT_HW_EMU");
        break;
      case HW:
        ptGeneration.put("target", "TT_HW");
        break;
      case UNKNOWN: // Intentional fallthrough
      default:
        ptGeneration.put("target", "TT_UNKNOWN");
        break;
      }

      // Adding a generic flag field to handle arbitrary information
      boost::property_tree::ptree flags;
      if (aieApplication) {
        boost::property_tree::ptree flag;
        flag.put("", "aie");
        flags.push_back(std::make_pair("", flag));
      }
      else {
        // If empty, make sure we output a list with nothing in it
	boost::property_tree::ptree empty;
        flags.push_back(std::make_pair("", empty));
      }
      ptGeneration.add_child("flags", flags);
      ptRunSummary.add_child("generation", ptGeneration);
    }

    // A section to associate the individual context IDs with the
    // xclbin UUIDs in the context.  One of them will be specific to
    // host + PL.  This section is omitted in the old "loadXclbin"
    // style of applications.
    if (!contexts.empty()) {
      boost::property_tree::ptree ptContexts;
      for (const auto& c : contexts) {
        auto id = c.first;
        auto uuid = c.second;

        boost::property_tree::ptree ptContext;
        ptContext.put("id", std::to_string(id));
        ptContext.put("uuid", uuid.to_string());
        ptContexts.push_back(std::make_pair("", ptContext));
      }
      ptRunSummary.add_child("hw_contexts", ptContexts);
    }

    boost::property_tree::ptree ptFiles;
    for (const auto& f : files) {
      boost::property_tree::ptree ptFile;
      ptFile.put("name", f.name.c_str());
      ptFile.put("type", f.type.c_str());
      if (!contexts.empty())
        ptFile.put("hw_context", std::to_string(f.contextId));
      ptFiles.push_back(std::make_pair("", ptFile));
    }
    ptRunSummary.add_child("files", ptFiles);

    // Add the system diagram information if available

    auto systemDiagrams = db->getSystemDiagrams();
    if (!systemDiagrams.empty()) {
      boost::property_tree::ptree ptSystemDiagrams;
      for (const auto& s : systemDiagrams) {
	boost::property_tree::ptree ptSystemDiagram;
	ptSystemDiagram.put("hw_context", s.contextId);
	ptSystemDiagram.put("payload_16bitEnd", s.systemDiagram.c_str());
	ptSystemDiagrams.push_back(std::make_pair("", ptSystemDiagram));
      }
      ptRunSummary.add_child("system_diagrams", ptSystemDiagrams);
    }

    boost::property_tree::write_json(fout, ptRunSummary, true);
    return true;
  }

} // end namespace xdp
