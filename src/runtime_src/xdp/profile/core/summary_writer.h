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

#ifndef __XDP_CORE_WRITER_H
#define __XDP_CORE_WRITER_H

#include "driver/include/xclperf.h"
#include "xdp/profile/plugin/base_plugin.h"
#include "xdp/profile/device/trace_parser.h"

#include <map>
#include <vector>
#include <string>
#include <mutex>

namespace xdp {
  class ProfileWriterI;
  class ProfileCounters;
  class RTProfile;

  // **************************************************************************
  // Top-level XDP profile writer class
  // **************************************************************************
  class SummaryWriter {
  public:
    SummaryWriter(ProfileCounters* profileCounters, TraceParser * TraceParserHandle, XDPPluginI* Plugin);
    ~SummaryWriter();

  public:
    // Attach or detach observer writers
    // NOTE: the following functions are thread safe
    void attach(ProfileWriterI* writer);
    void detach(ProfileWriterI* writer);

  public:
    // Log device counters (used in profile summary)
    void logDeviceCounters(std::string deviceName, std::string binaryName, xclPerfMonType type,
        xclCounterResults& counterResults, uint64_t timeNsec, bool firstReadAfterProgram);

  public:
    void writeProfileSummary(RTProfile* profile);

    // Summaries of counts
    void writeAPISummary(ProfileWriterI* writer) const;
    void writeKernelSummary(ProfileWriterI* writer) const;
    void writeStallSummary(ProfileWriterI* writer) const;
    void writeKernelStreamSummary(ProfileWriterI* writer);
    void writeComputeUnitSummary(ProfileWriterI* writer) const;
    void writeHostTransferSummary(ProfileWriterI* writer) const;
    void writeKernelTransferSummary(ProfileWriterI* writer);
    void writeDeviceTransferSummary(ProfileWriterI* writer) const;
    // Top offenders lists
    void writeTopKernelSummary(ProfileWriterI* writer) const;
    void writeTopKernelTransferSummary(ProfileWriterI* writer) const;
    void writeTopDataTransferSummary(ProfileWriterI* writer, bool isRead) const;
    void writeTopDeviceTransferSummary(ProfileWriterI* writer, bool isRead) const;
    // Unified summaries
    void writeAcceleratorSummary(ProfileWriterI* writer) const;
    void writeTopHardwareSummary(ProfileWriterI* writer) const;

  private:
    unsigned int HostSlotIndex;
    std::mutex mLogMutex;
    ProfileCounters* mProfileCounters;
    std::vector<ProfileWriterI*> mProfileWriters;

    std::map<std::string, xclCounterResults> mFinalCounterResultsMap;
    std::map<std::string, xclCounterResults> mRolloverCounterResultsMap;
    std::map<std::string, xclCounterResults> mRolloverCountsMap;
    std::map<std::string, std::vector<std::string>> mDeviceBinaryDataSlotsMap;
    std::map<std::string, std::vector<std::string>> mDeviceBinaryCuSlotsMap;
    std::map<std::string, std::vector<std::string>> mDeviceBinaryStrSlotsMap;

  private:
    TraceParser * mTraceParserHandle;
    XDPPluginI * mPluginHandle;

  private:
    double getGlobalMemoryMaxBandwidthMBps() const;
  };

} // xdp

#endif
