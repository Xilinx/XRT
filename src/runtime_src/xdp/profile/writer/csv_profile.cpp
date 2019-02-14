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

#include "csv_profile.h"
#include "util.h"

namespace xdp {

  CSVProfileWriter::CSVProfileWriter( const std::string& summaryFileName,
                                      const std::string& platformName,
                                      XDPPluginI* Plugin) :
      SummaryFileName(summaryFileName),
      PlatformName(platformName)
  {
    mPluginHandle = Plugin;
    if (SummaryFileName != "") {
      assert(!Summary_ofs.is_open());
      SummaryFileName += FileExtension;
      openStream(Summary_ofs, SummaryFileName);
      writeDocumentHeader(Summary_ofs, "SDAccel Profile Summary");
    }
  }

  CSVProfileWriter::~CSVProfileWriter()
  {
    if (Summary_ofs.is_open()) {
      writeDocumentFooter(Summary_ofs);
      Summary_ofs.close();
    }
  }

  void CSVProfileWriter::writeSummary(RTProfile* profile)
  {
    ProfileWriterI::writeSummary(profile);

    //Table 7: Top Kernel Summary.
    std::vector<std::string> TopKernelSummaryColumnLabels = {
        "Kernel Instance Address", "Kernel", "Context ID", "Command Queue ID",
        "Device", "Start Time (ms)", "Duration (ms)",
        "Global Work Size", "Local Work Size"};
    writeTableHeader(getStream(), "Top Kernel Execution",
        TopKernelSummaryColumnLabels);
    profile->writeTopKernelSummary(this);
    writeTableFooter(getStream());

    //Table 8: Top Buffer Write Summary
    std::vector<std::string> TopBufferWritesColumnLabels = {
        "Buffer Address", "Context ID", "Command Queue ID", "Start Time (ms)",
        "Duration (ms)", "Buffer Size (KB)", "Writing Rate(MB/s)"};
    writeTableHeader(getStream(), "Top Buffer Writes",
        TopBufferWritesColumnLabels);
    profile->writeTopDataTransferSummary(this, false); // Writes
    writeTableFooter(getStream());

    //Table 9: Top Buffer Read Summary
    std::vector<std::string> TopBufferReadsColumnLabels = {
        "Buffer Address", "Context ID", "Command Queue ID", "Start Time (ms)",
        "Duration (ms)", "Buffer Size (KB)", "Reading Rate(MB/s)"};
    writeTableHeader(getStream(), "Top Buffer Reads",
        TopBufferReadsColumnLabels);
    profile->writeTopDataTransferSummary(this, true); // Reads
    writeTableFooter(getStream());

    //Table 10: Parameters used in PRCs
    std::vector<std::string> PRCSummaryColumnLabels = {
      "Parameter", "Element", "Value"
    };
    writeTableHeader(getStream(), "PRC Parameters", PRCSummaryColumnLabels);
    writeGuidanceMetadataSummary(profile, mPluginHandle->getDeviceExecTimesMap(), 
                                          mPluginHandle->getComputeUnitCallsMap(),
                                          mPluginHandle->getKernelCountsMap());
    writeTableFooter(getStream());
  }

  void CSVProfileWriter::writeDocumentHeader(std::ofstream& ofs,
      const std::string& docName)
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

  // Write sub-header to profile summary
  // NOTE: this part of the header must be written after a run is completed.
  void CSVProfileWriter::writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile)
  {
    if (!ofs.is_open())
      return;

    // Sub-header of profile summary
    ofs << "Target devices: " << profile->getDeviceNames(", ") << "\n";

    std::string flowMode;
    xdp::RTUtil::getFlowModeName(mPluginHandle->getFlowMode(), flowMode);
    ofs << "Flow mode: " << flowMode << "\n";
  }

  void CSVProfileWriter::writeTableHeader(std::ofstream& ofs, const std::string& caption,
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

  void CSVProfileWriter::writeDocumentFooter(std::ofstream& ofs)
  {
    if (ofs.is_open())
      ofs << "\n";
  }
} // xdp
