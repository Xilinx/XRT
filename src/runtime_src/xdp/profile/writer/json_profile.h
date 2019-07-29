/**
 * Copyright (C) 2016-2019 Xilinx, Inc
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

#ifndef __XDP_JSON_PROFILE_WRITER_H
#define __XDP_JSON_PROFILE_WRITER_H

#include "base_profile.h"

#include <memory>
#include <boost/property_tree/ptree.hpp>

namespace xdp {

  class JSONProfileWriter: public ProfileWriterI {

  public:
    JSONProfileWriter(XDPPluginI* Plugin,
                      const std::string& platformName,
                      const std::string& summaryFileName);
    ~JSONProfileWriter();

    virtual void writeSummary(RTProfile* profile);

    virtual std::shared_ptr<boost::property_tree::ptree> getProfileTree() { return mTree; }

  protected:

    // In the following, ofs is simply ignored as we build the boost property_tree.
    // But we keep these for a similar structure to the other writers.
    void writeDocumentHeader(std::ofstream& ofs, const std::string& docName) override;
    void writeDocumentSubHeader(std::ofstream& ofs, RTProfile* profile) override;

    // We need to override the following, as these get called through a long
    // process when we call methods like writeTopKernelSummary in writeSummary.

    // Functions for Summary

    // Write execution time stats, used for both kernel and API reporting
    virtual void writeTimeStats(const std::string& name, const TimeStats& stats) override;

    // Write read buffer or write buffer transfer stats.
    // Like writeDeviceTransferBelow, it is not clear this is ever used.
    //virtual void writeBufferStats(const std::string& name, const BufferStats& stats);

    // Write Kernel Execution Time Trace
    virtual void writeKernel(const KernelTrace& trace) override;

    // Write Read or Write Buffer Time Trace
    virtual void writeBuffer(const BufferTrace& trace) override;

    // Write Device Read or Write Data Transfer Time Trace
    // It is not clear that this is ever needed. The ProfileCounters class has
    // four member variables that contain DeviceTrace data, TopKernelReadTimes,
    // TopKernelWriteTimes, TopDeviceBufferReadTimes, and TopDeviceBufferWriteTimes.
    // The first two are never written out or read from at all. The latter two
    // are accessed in ProfileCounters::writeTopDeviceTransferSummary, but this
    // method doesn't appear to ever be called. RTProfile and SummaryWriter have
    // methods that will invoke it, but none of the profile writers ever call
    // those.
    //virtual void writeDeviceTransfer(const DeviceTrace& trace) override;

    // Write compute unit utilization table
    virtual void writeComputeUnitSummary(const std::string& name, const TimeStats& stats) override;

    // Write accelerator table
    // This is used in unified_csv_profile, but not csv_profile, so we are
    // going to skip it for now.
    //virtual void writeAcceleratorSummary(const std::string& name, const TimeStats& stats) override;

    // Write Read/Write Buffer transfer stats
    virtual void writeHostTransferSummary(const std::string& name,
      const BufferStats& stats, uint64_t totalBytes, uint64_t totalTranx,
      double totalTimeMsec, double maxTransferRateMBps) override;

    // Write Read/Write Shell Internal transfer stats
    // Used by multiple different tables.
    void writeShellTransferSummary(const std::string& deviceName, const std::string& transferType,
      uint64_t totalBytes, uint64_t totalTranx, double totalLatencyNsec, double totalTimeMsec) override;

    // Write Read/Write Kernel transfer stats
    void writeKernelTransferSummary(const std::string& deviceName, const std::string& cuPortName,
      const std::string& argNames, const std::string& memoryName,
      const std::string& transferType, uint64_t totalBytes, uint64_t totalTranx,
      double totalKernelTimeMsec, double totalTransferTimeMsec, double maxTransferRateMBps) override;

    void writeStallSummary(std::string& cuName, uint32_t cuRunCount, double cuRunTimeMsec,
      double cuStallExt, double cuStallStr, double cuStallInt) override;

    void writeKernelStreamSummary(const std::string& deviceName,
      const std::string& MasterPort, const std::string& MasterArgs,
      const std::string& SlavePort, const std::string& SlaveArgs,
      uint64_t strNumTranx, double transferRateMBps,
      double avgSize, double avgUtil,
      double linkStarve, double linkStall) override;

    // Write Top Kernel Read and Write transfer stats
    virtual void writeTopKernelTransferSummary(
      const std::string& deviceName, const std::string& cuName,
      uint64_t totalWriteTranx, uint64_t totalReadTranx,
      uint64_t totalWriteBytes, uint64_t totalReadBytes,
      double totalWriteTimeMsec, double totalReadTimeMsec,
      uint32_t maxBytesPerTransfer, double maxTransferRateMBps) override;

    // Functions for device counters
    // This was in base_profile.h, but with no implementation in the .cpp file,
    //void writeDeviceCounters(xclPerfMonType type, xclCounterResults& results,
    //  double timestamp, uint32_t sampleNum, bool firstReadAfterProgram) override;

    // Function for guidance metadata
    void writeGuidanceMetadataSummary(RTProfile *profile) override;

    // We don't really care about these, but the first is a pure virtual,
    // so we have to provide an override.
    void writeTableHeader(std::ofstream& ofs, const std::string& caption,
      const std::vector<std::string>& columnLabels) override;
    //void writeTableRowStart(std::ofstream& ofs) override { ofs << "";}
    //void writeTableRowEnd(std::ofstream& ofs) override { ofs << "\n";}
    //void writeTableFooter(std::ofstream& ofs) override { ofs << "\n";};
    //void writeDocumentFooter(std::ofstream& ofs) override { ofs << "\n"; }
    //const char* cellStart() override { return ""; }
    //const char* cellEnd() override { return ","; }
    //const char* rowStart() override { return ""; }
    //const char* rowEnd() override { return ""; }
    //const char* newLine() override { return "\n"; }

  private:
    void makeCurrentBranch(const std::string& name);
    // The following didn't seem to work as intended, even when put "inline".
    // The resulting JSON would always have an extra empty "" entry. Not sure why.
    boost::property_tree::ptree& getCurrentBranch();

    std::shared_ptr<boost::property_tree::ptree> mTree;
    // In some cases common code is called to fill out different parts of the
    // profile data, and we don't control those calls and can't pass an
    // argument to distinguish the calls. So we use this to track that.
    // See @writeTimeStats.
    std::string mCurrentBranch;

    //const std::string FileExtension = ".json";
  };

} // xdp

#endif // #define __XDP_JSON_PROFILE_WRITER_H
