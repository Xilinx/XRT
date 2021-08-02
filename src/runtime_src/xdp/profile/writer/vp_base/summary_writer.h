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

#ifndef SUMMARY_WRITER_DOT_H
#define SUMMARY_WRITER_DOT_H

#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <functional>
#include <set>

#include "xdp/config.h"
#include "xdp/profile/writer/vp_base/vp_summary_writer.h"
#include "xdp/profile/writer/vp_base/guidance_rules.h"

namespace xdp {

  class SummaryWriter : public VPSummaryWriter
  {
  private:
    SummaryWriter() = delete ;
    GuidanceRules guidance ;

    std::set<std::string> OpenCLAPIs ;
    std::set<std::string> NativeAPIs ;
    std::set<std::string> HALAPIs ;

    void initializeAPIs() ;

    void writeHeader() ;

    // OpenCL host tables
    void writeOpenCLAPICalls() ;
    void writeKernelExecutionSummary() ;
    void writeTopKernelExecution() ;
    void writeTopMemoryWrites() ;
    void writeTopMemoryReads() ;

    // Generic host tables
    enum APIType { OPENCL, NATIVE, HAL, ALL } ;
    void writeAPICalls(APIType type) ;

    // OpenCL specific device tables
    void writeSoftwareEmulationComputeUnitUtilization() ;
    void writeComputeUnitStallInformation() ;
    void writeDataTransferHostToGlobalMemory() ;

    // Generic device tables
    void writeDataTransferDMA() ;
    void writeDataTransferDMABypass() ;
    void writeStreamDataTransfers() ;
    void writeDataTransferKernelsToGlobalMemory() ;
    void writeTopDataTransferKernelAndGlobal() ;
    void writeDataTransferGlobalMemoryToGlobalMemory() ;
    void writeComputeUnitUtilization() ;

    // User event tables
    void writeUserLevelEvents() ;
    void writeUserLevelRanges() ;

    // Native XRT tables
    void writeNativeAPICalls() ;
    void writeHostReadsFromGlobalMemory() ;
    void writeHostWritesToGlobalMemory() ;
    void writeTopSyncReads() ;
    void writeTopSyncWrites() ;

    // HAL tables
    void writeHALAPICalls() ;
    void writeHALTransfers() ;

    // Handy values used for conversion
    const double zero         = 0.0 ;
    const double one_hundred  = 100.0 ;
    const double one_thousand = 1000.0 ;
    const double one_million  = 1.0e06 ;
    const double one_billion  = 1.0e09 ;

  public:
    XDP_EXPORT SummaryWriter(const char* filename) ;
    XDP_EXPORT SummaryWriter(const char* filename, VPDatabase* inst) ;
    XDP_EXPORT ~SummaryWriter() ;

    XDP_EXPORT virtual bool write(bool openNewFile) ;
  } ;

} // end namespace xdp

#endif
