/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef OPENCL_SUMMARY_WRITER_DOT_H
#define OPENCL_SUMMARY_WRITER_DOT_H

#include <functional>

#include "xdp/profile/writer/vp_base/vp_summary_writer.h"
#include "xdp/config.h"

namespace xdp {

  class OpenCLSummaryWriter : public VPSummaryWriter
  {
  private:
    OpenCLSummaryWriter() = delete ;

    // For the one guidance rule that needs xrt.ini options
    std::vector<std::string> iniSettings ;

    // Guidance Rules
    std::vector<std::function<void (OpenCLSummaryWriter*)>> guidanceRules ;

    static void guidanceDeviceExecTime(OpenCLSummaryWriter* t) ;
    static void guidanceCUCalls(OpenCLSummaryWriter* t) ;
    static void guidanceNumMonitors(OpenCLSummaryWriter* t) ;
    static void guidanceMigrateMem(OpenCLSummaryWriter* t) ;
    static void guidanceMemoryUsage(OpenCLSummaryWriter* t) ;
    static void guidancePLRAMDevice(OpenCLSummaryWriter* t) ;
    static void guidanceHBMDevice(OpenCLSummaryWriter* t) ;
    static void guidanceKDMADevice(OpenCLSummaryWriter* t) ;
    static void guidanceP2PDevice(OpenCLSummaryWriter* t) ;
    static void guidanceP2PHostTransfers(OpenCLSummaryWriter* t) ;
    static void guidancePortBitWidth(OpenCLSummaryWriter* t) ;
    static void guidanceKernelCount(OpenCLSummaryWriter* t) ;
    static void guidanceObjectsReleased(OpenCLSummaryWriter* t) ;
    static void guidanceCUContextEn(OpenCLSummaryWriter* t) ;
    static void guidanceTraceMemory(OpenCLSummaryWriter* t) ;
    static void guidanceMaxParallelKernelEnqueues(OpenCLSummaryWriter* t) ;
    static void guidanceCommandQueueOOO(OpenCLSummaryWriter* t) ;
    static void guidancePLRAMSizeBytes(OpenCLSummaryWriter* t) ;
    static void guidanceKernelBufferInfo(OpenCLSummaryWriter* t) ;
    static void guidanceTraceBufferFull(OpenCLSummaryWriter* t) ;
    static void guidanceMemoryTypeBitWidth(OpenCLSummaryWriter* t) ;
    static void guidanceXrtIniSetting(OpenCLSummaryWriter* t) ;
    static void guidanceBufferRdActiveTimeMs(OpenCLSummaryWriter* t) ;
    static void guidanceBufferWrActiveTimeMs(OpenCLSummaryWriter* t) ;
    static void guidanceBufferTxActiveTimeMs(OpenCLSummaryWriter* t) ;
    static void guidanceApplicationRunTimeMs(OpenCLSummaryWriter* t) ;
    static void guidanceTotalKernelRunTimeMs(OpenCLSummaryWriter* t) ;

    // Generate the specific header for profile summary
    void writeHeader() ;

    // Profile summary is a set of tables that get generated.
    //  Each function here covers the generation of a single table.
    void writeAPICallSummary() ;
    void writeKernelExecutionSummary() ;
    void writeSoftwareEmulationComputeUnitUtilization() ;
    void writeComputeUnitUtilization() ;
    void writeComputeUnitStallInformation() ;
    void writeDataTransferHostToGlobalMemory() ;
    void writeDataTransferKernelsToGlobalMemory() ;
    void writeStreamDataTransfers() ;
    void writeDataTransferDMA() ;
    void writeDataTransferDMABypass() ;
    void writeDataTransferGlobalMemoryToGlobalMemory() ;
    void writeTopDataTransferKernelAndGlobal() ;
    void writeTopKernelExecution() ;
    void writeTopMemoryWrites() ;
    void writeTopMemoryReads() ;
    void writeUserLevelEvents() ;
    void writeUserLevelRanges() ;
    void writeGuidance() ;

  public:
    XDP_EXPORT OpenCLSummaryWriter(const char* filename) ;
    XDP_EXPORT ~OpenCLSummaryWriter() ;

    XDP_EXPORT virtual bool write(bool openNewFile) ;
  } ;

} // end namespace xdp

#endif
