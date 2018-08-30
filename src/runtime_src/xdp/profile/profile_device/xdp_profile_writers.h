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
#ifndef __XILINX_XDP_PROFILE_WRITERS_H
#define __XILINX_XDP_PROFILE_WRITERS_H

#include <limits>
#include <cstdint>
#include <map>
#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <chrono>
// #include <unistd.h>
#include <cassert>
#include <thread>
#include <mutex>
#include <CL/opencl.h>

#include "xclhal2.h"

// Use this class to build run time user services functions
// such as debugging and profiling

namespace XDP {
    class XDPProfile;
    class DeviceTrace;

    typedef std::vector<DeviceTrace> TraceResultVector;

    // Writer interface for generating profile data
    class WriterI {

    public:
      WriterI();
      virtual ~WriterI() {};
      // A derived class can choose to write less or more, or write differently
      // But a default implementation is provided. This may be preferred to keep
      // consistency across all formats of reports
      virtual void writeSummary(XDPProfile* profile);

      static const char * getToolVersion() { return "2018.2"; }

    public:
      // Functions for Summary
      // Write Kernel Execution Time stats
      virtual void writeSummary(const std::string& name, const TimeStats& stats);
      // Write Read Buffer of Write Buffer transfer stats
      virtual void writeSummary(const std::string& name, const BufferStats& stats);
      // Write Kernel Execution Time Trace
      virtual void writeSummary(const KernelTrace& trace);
      // Write Read or Write Buffer Time Trace
      virtual void writeSummary(const BufferTrace& trace);
      // Write Device Read or Write Data Transfer Time Trace
      virtual void writeSummary(const DeviceTrace& trace);
      // Write compute unit utilization table
      virtual void writeComputeUnitSummary(const std::string& name, const TimeStats& stats);

      // Write Read/Write Buffer transfer stats
      virtual void writeHostTransferSummary(const std::string& name,
          const BufferStats& stats, uint64_t totalTranx, uint64_t totalBytes,
          double totalTimeMsec, double maxTransferRateMBps);
      // Write Read/Write Kernel transfer stats
      void writeKernelTransferSummary(const std::string& deviceName,
          const std::string& cuPortName, const std::string& argNames, uint32_t ddrBank,
		  const std::string& transferType, uint64_t totalBytes, uint64_t totalTranx,
		  double totalKernelTimeMsec, double totalTransferTimeMsec, double maxTransferRateMBps);
	  void writeStallSummary(std::string& cuName, uint32_t cuRunCount, double cuRunTimeMsec,
	      double cuStallExt, double cuStallStr, double cuStallInt);
      // Write Top Kernel Read and Write transfer stats
      virtual void writeTopKernelTransferSummary(
          const std::string& deviceName, const std::string& cuName,
          uint64_t totalWriteTranx, uint64_t totalReadTranx,
          uint64_t totalWriteBytes, uint64_t totalReadBytes,
          double totalWriteTimeMsec, double totalReadTimeMsec,
          uint32_t maxBytesPerTransfer, double maxTransferRateMBps);

      // Write data transfer event to trace
      void writeTimeline(double traceTime, const std::string& commandString,
                 const std::string& stageString, const std::string& eventString,
                 const std::string& dependString, size_t size, uint64_t address,
                 const std::string& bank, std::thread::id threadId);

      void writeDeviceTrace(XDPProfile* profile, const TraceResultVector &resultVector,
                 std::string deviceName, std::string binaryName);

	public:
      static std::string getCurrentDateTime();
      static std::string getCurrentTimeMsec();
      static std::string getCurrentExecutableName();

	protected:
      // Veraidic args function to take n number of any type of args and
      // stream it to a file
      // Move it to base class as new writer functionality needs it.
      // TODO: Windows doesnt support variadic functions till VS 2013.
      template<typename T>
      void writeTableCells(std::ofstream& ofs, T value)
      {
        ofs << cellStart();
	    ofs << value;
	    ofs << cellEnd();
      }

      template<typename T, typename... Args>
      void writeTableCells(std::ofstream& ofs, T first, Args... args)
      {
	    writeTableCells(ofs, first);
	    writeTableCells(ofs, args...);
      }

	protected:
      void openStream(std::ofstream& ofs, const std::string& fileName);
      std::ofstream& getSummaryStream() {return Summary_ofs;}
      std::ofstream& getTimelineStream(){return Timeline_ofs;}
	    
	protected:
      // Document is assumed to consist of Document Header, one or more tables and document footer
      // Table has header, rows and footer
      // A Binary writer such as write to Xilinx's WDB format (say WDBWriter class) could be
      // derived from WriterI. The writing of such a file can still be organized in following steps. Any additional
      // intelligence such as streaming compression can be built in the WDBWriter
      virtual void writeDocumentHeader(std::ofstream& ofs, const std::string& docName)  { ofs << docName;}
      virtual void writeDocumentSubHeader(std::ofstream& ofs, XDPProfile* profile) {}
      virtual void writeTableHeader(std::ofstream& ofs, const std::string& caption,
          const std::vector<std::string>& columnLabels) = 0;
      virtual void writeTableRowStart(std::ofstream& ofs) { ofs << rowStart(); }
      virtual void writeTableRowEnd(std::ofstream& ofs)   { ofs << rowEnd() << newLine(); }
      virtual void writeTableFooter(std::ofstream& ofs) {}
      virtual void writeDocumentFooter(std::ofstream& ofs) {}

      // Cell and Row marking tokens
      virtual const char* cellStart()  { return ""; }
      virtual const char* cellEnd()  { return ""; }
      virtual const char* rowStart() { return ""; }
      virtual const char* rowEnd()  { return ""; }
      virtual const char* newLine()  { return "\n"; }

	protected:
      std::ofstream Summary_ofs;
      std::ofstream Timeline_ofs;
      xclCounterResults CountersPrev;
    };

    //
    // CSV Writer
    //
    class CSVWriter: public WriterI {

	public:
      CSVWriter(const std::string& summaryFileName, const std::string& timelineFileName,
                const std::string& platformName);
      ~CSVWriter();

      virtual void writeSummary(XDPProfile* profile);

	protected:
      void writeDocumentHeader(std::ofstream& ofs, const std::string& docName) override;
      void writeDocumentSubHeader(std::ofstream& ofs, XDPProfile* profile) override;
      void writeTableHeader(std::ofstream& ofs, const std::string& caption, const std::vector<std::string>& columnLabels) override;
      void writeTableRowStart(std::ofstream& ofs) override { ofs << "";}
      void writeTableRowEnd(std::ofstream& ofs) override { ofs << "\n";}
      void writeTableFooter(std::ofstream& ofs) override { ofs << "\n";};
      void writeDocumentFooter(std::ofstream& ofs) override;
      void writeTimelineFooter(std::ofstream& ofs);

      // Cell and Row marking tokens
      const char* cellStart() override { return ""; }
      const char* cellEnd() override { return ","; }
      const char* rowStart() override { return ""; }
      const char* rowEnd() override { return ""; }
      const char* newLine() override { return "\n"; }

	private:
      std::string SummaryFileName;
      std::string TimelineFileName;
      std::string PlatformName;
      const std::string FileExtension = ".csv";
    };
};
#endif

