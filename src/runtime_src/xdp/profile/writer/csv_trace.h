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

#ifndef __XDP_CSV_TRACE_WRITER_H
#define __XDP_CSV_TRACE_WRITER_H

#include "base_trace.h"

namespace xdp {

    class CSVTraceWriter: public TraceWriterI {

	public:
      CSVTraceWriter(const std::string& traceFileName, const std::string& platformName, XDPPluginI* Plugin);
      ~CSVTraceWriter();

	protected:
      void writeDocumentHeader(std::ofstream& ofs, const std::string& docName) override;
      void writeTableHeader(std::ofstream& ofs, const std::string& caption,
	      const std::vector<std::string>& columnLabels) override;
      void writeTableRowStart(std::ofstream& ofs) override { ofs << "";}
      void writeTableRowEnd(std::ofstream& ofs) override { ofs << "\n";}
      void writeDocumentFooter(std::ofstream& ofs) override;
      void writeTimelineFooter(std::ofstream& ofs);
      // Rest of the cell and row parameters are default in base class
      const char* cellEnd() override { return ","; } 

	private:
      std::string TraceFileName;
      std::string PlatformName;
      const std::string FileExtension = ".csv";
    };

} // xdp

#endif
