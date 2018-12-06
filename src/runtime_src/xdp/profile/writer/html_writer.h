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
#ifndef __XILINX_XDP_HTML_WRITERS_H
#define __XILINX_XDP_HTML_WRITERS_H

#include "base_writer.h"

namespace XCL {

  class HTMLWriter: public WriterI {

	public:
      HTMLWriter(const std::string& summaryFileName, const std::string& timelineFileName,
                 const std::string& platformName);
	    ~HTMLWriter();

	    virtual void writeSummary(RTProfile* profile);

	protected:
	    void writeTableRowStart(std::ofstream& ofs) override { ofs << "<TR>";}
	    void writeTableRowEnd(std::ofstream& ofs) override { ofs << "</TR>\n";}
	    void writeDocumentHeader(std::ofstream& ofs, const std::string& docName) override;
	    void writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile) override;
	    void writeTableHeader(std::ofstream& ofs, const std::string& caption, const std::vector<std::string>& columnLabels) override;
	    void writeTableFooter(std::ofstream& ofs) override { ofs << "</TABLE>\n";};
	    void writeDocumentFooter(std::ofstream& ofs) override;

	    // Cell and Row marking tokens
	    const char* cellStart() override {
	    	return "<TD>";
	    }
	    const char* cellEnd() override {
	    	return "</TD>";
	    }
	    const char* rowStart() override { return "<TR>"; }
	    const char* rowEnd() override { return "</TR>"; }

	private:
	  std::string SummaryFileName;
      std::string TimelineFileName;
      std::string PlatformName;
      const std::string FileExtension = ".html";
    };
};

#endif