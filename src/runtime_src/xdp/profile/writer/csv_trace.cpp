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

namespace xdp {

  CSVTraceWriter::CSVTraceWriter( const std::string& traceFileName,
                                  const std::string& platformName,
                                  XDPPluginI* Plugin) :
      TraceFileName(traceFileName),
      PlatformName(platformName)
  {
    mPluginHandle = Plugin;
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

    std::string trString;
    ofs << "Footer,begin\n";
    mPluginHandle->getTraceFooterString(trString);
    ofs << trString;
    ofs << "Footer,end\n";

    writeDocumentFooter(ofs);
  }
} // xdp
