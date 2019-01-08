/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "csv_trace.h"
#include "util.h"
#include "xdp/rt_singleton.h"

namespace xdp {

  CSVTraceWriter::CSVTraceWriter( const std::string& traceFileName,
                                  const std::string& platformName,
                                  XDPPluginI* Plugin) :
      TraceFileName(traceFileName),
      PlatformName(platformName),
      mPluginHandle(Plugin)
  {
    if (TraceFileName != "") {
      assert(!Trace_ofs.is_open());
      TraceFileName += FileExtension;
      openStream(Trace_ofs, TraceFileName);
      writeDocumentHeader(Trace_ofs, "SDAccel Timeline Trace");
      std::vector<std::string> TimelineTraceColumnLabels = {
          "Time_msec", "Name", "Event", "Address_Port", "Size",
          "Latency_cycles", "Start_cycles", "End_cycles",
          "Latency_usec", "Start_msec", "End_msec"
      };
      writeTableHeader(Trace_ofs, "", TimelineTraceColumnLabels);
    }
  }

  CSVTraceWriter::~CSVTraceWriter()
  {
    if (Trace_ofs.is_open()) {
      writeTimelineFooter(Trace_ofs);
      Trace_ofs.close();
    }
  }

  void CSVTraceWriter::writeDocumentHeader(std::ofstream& ofs, const std::string& docName)
  {
    if (!ofs.is_open())
      return;

    // Header of document
    ofs << docName << "\n";
    ofs << "Generated on: " << xdp::WriterI::getCurrentDateTime() << "\n";
    ofs << "Msec since Epoch: " << xdp::WriterI::getCurrentTimeMsec() << "\n";
    if (!xdp::WriterI::getCurrentExecutableName().empty()) {
      ofs << "Profiled application: " << xdp::WriterI::getCurrentExecutableName() << "\n";
    }
    ofs << "Target platform: " << PlatformName << "\n";
    ofs << "Tool version: " << xdp::WriterI::getToolVersion() << "\n";
  }

  void CSVTraceWriter::writeTableHeader(std::ofstream& ofs, const std::string& caption,
                                        const std::vector<std::string>& columnLabels)
  {
    if (!ofs.is_open())
      return;

    ofs << "\n" << caption << "\n";
    for (const auto& str : columnLabels) {
      ofs << str << ",";
    }
    ofs << "\n";
  }

  void CSVTraceWriter::writeDocumentFooter(std::ofstream& ofs)
  {
    if (ofs.is_open())
      ofs << "\n";
  }

  void CSVTraceWriter::writeTimelineFooter(std::ofstream& ofs)
  {
    if (!ofs.is_open())
      return;

    // TODO: get this info much earlier in flow!!!!!
    auto rts = xdp::RTSingleton::Instance();
    auto profile = rts->getProfileManager();

    ofs << "Footer,begin\n";

    //
    // Settings (project name, stalls, target, & platform)
    //
    std::string projectName = profile->getProjectName();
    ofs << "Project," << projectName << ",\n";

    std::string stallProfiling = (profile->getStallTrace() == xdp::RTUtil::STALL_TRACE_OFF) ?
        "false" : "true";
    ofs << "Stall profiling," << stallProfiling << ",\n";

    std::string flowMode;
    xdp::RTUtil::getFlowModeName(mPluginHandle->getFlowMode(), flowMode);
    ofs << "Target," << flowMode << ",\n";

    std::string deviceNames = profile->getDeviceNames("|");
    ofs << "Platform," << deviceNames << ",\n";

    for (auto& threadId : profile->getThreadIds())
      ofs << "Read/Write Thread," << std::showbase << std::hex << std::uppercase
	      << threadId << std::endl;

    //
    // Platform/device info
    //
    auto platform = rts->getcl_platform_id();
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();
      ofs << "Device," << deviceName << ",begin\n";

      // DDR Bank addresses
      // TODO: this assumes start address of 0x0 and evenly divided banks
      unsigned int ddrBanks = device_id->get_ddr_bank_count();
      if (ddrBanks == 0) ddrBanks = 1;
      size_t ddrSize = device_id->get_ddr_size();
      size_t bankSize = ddrSize / ddrBanks;
      ofs << "DDR Banks,begin\n";
      for (unsigned int b=0; b < ddrBanks; ++b)
        ofs << "Bank," << std::dec << b << ","
		    << (boost::format("0X%09x") % (b * bankSize)) << std::endl;
      ofs << "DDR Banks,end\n";

#if 0
      std::string binaryName = "binary_container";
      auto deviceIter = DeviceBinaryNameMap.find(deviceName);
      if (deviceIter != DeviceBinaryNameMap.end())
        binaryName = deviceIter->second;
      ofs << "BinaryContainer," << binaryName << ",begin\n";

      // Traverse all CUs on current device
      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        std::string cuName = cu->getname();
        std::string kernelName;
        profile->getKernelFromComputeUnit(cuName, kernelName);

        ofs << "ComputeUnit," << cuName << "|" << kernelName << ",begin\n";
        // TODO: add any CU-specific values
        ofs << "ComputeUnit," << cuName << "|" << kernelName << ",end\n";
      }

      ofs << "BinaryContainer," << binaryName << ",end\n";
#endif
      ofs << "Device," << deviceName << ",end\n";
    }

    //
    // Unused CUs
    //
    //auto platform = rts->getcl_platform_id();
    for (auto device_id : platform->get_device_range()) {
      std::string deviceName = device_id->get_unique_name();

      for (auto& cu : xocl::xocl(device_id)->get_cus()) {
        auto cuName = cu->get_name();

        if (profile->getComputeUnitCalls(deviceName, cuName) == 0)
          ofs << "UnusedComputeUnit," << cuName << ",\n";
      }
    }

    ofs << "Footer,end\n";

    writeDocumentFooter(ofs);
  }
} // xdp
