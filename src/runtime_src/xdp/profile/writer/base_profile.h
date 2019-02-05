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

#ifndef __XDP_BASE_PROFILE_WRITER_H
#define __XDP_BASE_PROFILE_WRITER_H

#include <boost/format.hpp>
#include "xdp/profile/device/trace_parser.h"
#include "xdp/profile/plugin/base_plugin.h"
#include "xdp/profile/core/rt_profile.h"
#include "xdp/profile/plugin/base_plugin.h"

#include <cstdlib>
#include <cstdio>
#include <string>
#include <chrono>
#include <iostream>
#include <map>

// Use this class to build run time user services functions
// such as debugging and profiling

namespace xdp {
    class RTProfile;

    // Writer interface for generating profile data
    class ProfileWriterI {

    public:
      ProfileWriterI();
      virtual ~ProfileWriterI() {};

      // A derived class can choose to write less or more, or write differently
      // But a default implementation is provided. This may be preferred to keep
      // consistency across all formats of reports
      virtual void writeSummary(RTProfile* profile);

    public:
      inline void enableStallTable() { mEnStallTable = true; }
      inline void enableStreamTable() { mEnStreamTable = true; }
      // Functions for Summary
      // Write Kernel Execution Time stats
      virtual void writeTimeStats(const std::string& name, const TimeStats& stats);
      // Write Read Buffer of Write Buffer transfer stats
      virtual void writeBufferStats(const std::string& name, const BufferStats& stats);
      // Write Kernel Execution Time Trace
      virtual void writeKernel(const KernelTrace& trace);
      // Write Read or Write Buffer Time Trace
      virtual void writeBuffer(const BufferTrace& trace);
      // Write Device Read or Write Data Transfer Time Trace
      virtual void writeDeviceTransfer(const DeviceTrace& trace);
      // Write compute unit utilization table
      virtual void writeComputeUnitSummary(const std::string& name, const TimeStats& stats);
      // Write accelerator table
      virtual void writeAcceleratorSummary(const std::string& name, const TimeStats& stats);

      // Write Read/Write Buffer transfer stats
      virtual void writeHostTransferSummary(const std::string& name,
          const BufferStats& stats, uint64_t totalTranx, uint64_t totalBytes,
          double totalTimeMsec, double maxTransferRateMBps);
      // Write Read/Write Kernel transfer stats
      void writeKernelTransferSummary(
          const std::string& deviceName,
          const std::string& cuPortName, const std::string& argNames, const std::string& memoryName,
          const std::string& transferType, uint64_t totalBytes, uint64_t totalTranx,
          double totalKernelTimeMsec, double totalTransferTimeMsec, double maxTransferRateMBps);
      void writeStallSummary(std::string& cuName, uint32_t cuRunCount, double cuRunTimeMsec,
          double cuStallExt, double cuStallStr, double cuStallInt);
      void writeKernelStreamSummary(std::string& deviceName, std::string& cuPortName, std::string& argNames,
          uint64_t strNumTranx, double transferRateMBps, double avgSize, double avgUtil,
          double linkStarve, double linkStall);
      // Write Top Kernel Read and Write transfer stats
      virtual void writeTopKernelTransferSummary(
          const std::string& deviceName, const std::string& cuName,
          uint64_t totalWriteTranx, uint64_t totalReadTranx,
          uint64_t totalWriteBytes, uint64_t totalReadBytes,
          double totalWriteTimeMsec, double totalReadTimeMsec,
          uint32_t maxBytesPerTransfer, double maxTransferRateMBps);

      // Functions for device counters
      void writeDeviceCounters(xclPerfMonType type, xclCounterResults& results,
          double timestamp, uint32_t sampleNum, bool firstReadAfterProgram);

      // Function for guidance metadata
      void writeGuidanceMetadataSummary(RTProfile *profile,
          const XDPPluginI::GuidanceMap  &deviceExecTimesMap,
          const XDPPluginI::GuidanceMap  &computeUnitCallsMap,
          const XDPPluginI::GuidanceMap2 &kernelCountsMap);

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
      std::ofstream& getStream() {return Summary_ofs;}
        
    protected:
      // Document is assumed to consist of Document Header, one or more tables and document footer
      // Table has header, rows and footer
      // A Binary writer such as write to Xilinx's WDB format (say WDBWriter class) could be
      // derived from ProfileWriterI. The writing of such a file can still be organized in following steps.
      // Any additional intelligence such as streaming compression can be built in the WDBWriter
      virtual void writeDocumentHeader(std::ofstream& ofs, const std::string& docName)  { ofs << docName;}
      virtual void writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile) {}
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

    protected:
      bool mEnStallTable = false;
      bool mEnStreamTable = false;

    protected:
      XDPPluginI * mPluginHandle;
    };

} // xdp

#endif
