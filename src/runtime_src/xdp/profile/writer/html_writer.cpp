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

// Copyright 2014 Xilinx, Inc. All rights reserved.

#include "html_writer.h"
#include "xdp/rt_singleton.h"

namespace XCL {

  HTMLWriter::HTMLWriter(const std::string& summaryFileName, const std::string& timelineFileName,
      const std::string& platformName) :
        SummaryFileName(summaryFileName),
        TimelineFileName(timelineFileName),
        PlatformName(platformName)
  {
    if(SummaryFileName != "") {
      assert(!Summary_ofs.is_open());
      SummaryFileName += FileExtension;
      openStream(Summary_ofs, SummaryFileName);
      writeDocumentHeader(Summary_ofs, "SDAccel Profile Summary");
    }


    if (TimelineFileName != "") {
      assert(!Timeline_ofs.is_open());
      TimelineFileName += FileExtension;
      openStream(Timeline_ofs, TimelineFileName);
      writeDocumentHeader(Timeline_ofs, "SDAccel Timeline Trace");
      std::vector<std::string> TimelineTraceColumnLabels = {
          "Time (msec)", "Name", "Event", "Address/Port",
          "Size (Bytes or Num)", "Latency (cycles)",
          "Start (cycles)", "End (cycles)", "Latency (usec)",
          "Start (msec)", "End (msec)"
      };
      writeTableHeader(Timeline_ofs, "", TimelineTraceColumnLabels);
    }
  }

  HTMLWriter::~HTMLWriter()
  {
    if (Summary_ofs.is_open()) {
      writeDocumentFooter(Summary_ofs);
      Summary_ofs.close();
    }
    if (Timeline_ofs.is_open()) {
      writeTableFooter(Timeline_ofs);
      writeDocumentFooter(Timeline_ofs);
      Timeline_ofs.close();
    }
  }

  void HTMLWriter::writeSummary(RTProfile* profile)
  {
    WriterI::writeSummary(profile);
  }

  void HTMLWriter::writeDocumentFooter(std::ofstream& ofs)
  {
    if (ofs.is_open()) {
      // Close the document
      ofs << "</BODY>" << "\n" << "</HTML>" << "\n";
    }
  }

  void HTMLWriter::writeDocumentHeader(std::ofstream& ofs,
      const std::string& docName)
  {
    if (!ofs.is_open())
      return;

    // Opening of the document
    ofs << "<!DOCTYPE html>" << "\n" << "<HTML>" << "\n" << "<BODY>"
        << "\n";

    // Style Sheet
    ofs << "<STYLE>\n" << "\th1 {\n" << "\t\tfont-size:200%;\n" << "\t}\n";

    ofs << "\ttable th,tr,td {\n";
    ofs << "\t\tborder-collapse: collapse; /* share common border between cells */\n";
    ofs << "\t\tpadding: 4px; /* padding within cells */\n";
    ofs << "\t\ttable-layout : fixed\n";
    ofs << "\t}\n";

    ofs << "\ttable th {\n" << "\tbackground-color:lightsteelblue\n"
        << "\t}\n";
    ofs << "</STYLE>\n";

    // Header of document
    ofs << "<h1>" << docName << "</h1>\n";
    ofs << "<br>\n";
    ofs << "<h3>Generated on: " << WriterI::getCurrentDateTime() << "</h3>\n";
    if (!WriterI::getCurrentExecutableName().empty()) {
      ofs << "<h3>Profiled application: "
          << WriterI::getCurrentExecutableName() << "</h3>\n";
    }
    ofs << "<h3>Target platform: " << PlatformName << "</h3>\n";
    ofs << "<h3>Tool version: " << getToolVersion() << "</h3>\n";
    ofs.flush();
  }

  // Write sub-header to profile summary
  // NOTE: this part of the header must be written after a run is completed.
  void HTMLWriter::writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile)
  {
    if (!ofs.is_open())
      return;

    // Sub-header of document
    ofs << "<h3>Target devices: " << profile->getDeviceNames() << "</h3>\n";

    std::string flowMode;
    XCL::RTSingleton::Instance()->getFlowModeName(flowMode);
    ofs << "<h3>Flow mode: " << flowMode << "</h3>\n";
    ofs << "<br>\n";
    ofs.flush();
  }

  void HTMLWriter::writeTableHeader(
      std::ofstream& ofs, const std::string& caption,
      const std::vector<std::string>& columnLabels)
  {
    if (!ofs.is_open())
      return;

    ofs.flush();
    ofs << "<br>\n";
    ofs << "<h2>" << caption << "</h2>\n";

    ofs << "\n<TABLE border=\"1\">\n";
    ofs << "<TR>\n";
    for (auto str : columnLabels) {
      ofs << "<TH>" << str << "</TH>\n";
    }
    ofs << "</TR>\n";
    ofs.flush();
  }

};