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

#ifndef __XDP_UNIFIED_CSV_PROFILE_WRITER_H
#define __XDP_UNIFIED_CSV_PROFILE_WRITER_H

#include "base_profile.h"

namespace xdp {

    class UnifiedCSVProfileWriter: public ProfileWriterI {

	public:
      UnifiedCSVProfileWriter(const std::string& summaryFileName, const std::string& platformFileName, XDPPluginI* Plugin);
      ~UnifiedCSVProfileWriter();

	  virtual void writeSummary(RTProfile* profile);

	protected:
      void writeDocumentHeader(std::ofstream& ofs, const std::string& docName) override;
      void writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile) override;
      void writeTableHeader(std::ofstream& ofs, const std::string& caption,
   	      const std::vector<std::string>& columnLabels) override;
      void writeTableRowStart(std::ofstream& ofs) override { ofs << "";}
      void writeTableRowEnd(std::ofstream& ofs) override { ofs << "\n";}
      void writeTableFooter(std::ofstream& ofs) override { ofs << "\n";};
      void writeDocumentFooter(std::ofstream& ofs) override;

      void writeKernel(const KernelTrace& trace) override;
      void writeBuffer(const BufferTrace& trace) override;

      void writeTopKernelTransferSummary(
          const std::string& deviceName, const std::string& accelName,
          uint64_t totalWriteTranx, uint64_t totalReadTranx,
          uint64_t totalWriteBytes, uint64_t totalReadBytes,
          double totalWriteTimeMsec, double totalReadTimeMsec,
          uint32_t maxBytesPerTransfer, double maxTransferRateMBps) override;
      void writeHostTransferSummary(const std::string& name,
          const BufferStats& stats, uint64_t totalTranx, uint64_t totalBytes,
          double totalTimeMsec, double maxTransferRateMBps) override;
          
      // Cell and Row marking tokens
      const char* cellStart() override { return ""; }
      const char* cellEnd() override { return ","; }
      const char* rowStart() override { return ""; }
      const char* rowEnd() override { return ""; }
      const char* newLine() override { return "\n"; }

	private:
      std::string SummaryFileName;
      std::string PlatformName;
      const std::string FileExtension = ".csv";
    };

} // xdp

#endif
