/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef XCLBIN_INFO_DOT_H
#define XCLBIN_INFO_DOT_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/common/system.h"

#include "xdp/config.h"
#include "xdp/profile/database/static_info/aie_constructs.h"

namespace xdp {

  // Forward declarations
  class DeviceIntf ;
  struct Monitor ;
  struct Memory ;
  class ComputeUnitInstance ;

  // The PLInfo struct keeps track of all the information in the PL section
  //  of an xclbin.  This includes information on compute units, memories,
  //  and all of the added debug IP.
  struct PLInfo
  {
    // Max read/write bandwidth information is retrieved from either
    //  a call to the shim functions xclGetReadMaxBandwidthMBps and
    //  xclGetWriteMaxBandwidthMBps, or from from the higher level 
    //  xrt_xocl::device functions getDeviceMaxRead and getDeviceMaxWrite
    //  in OpenCL applications.
    double maxReadBW  = 0.0 ;
    double maxWriteBW = 0.0 ;

    // By default, we assume a PL clock rate of 300 MHz.  We try to set this
    //  to the true value based on device information gotten from either the
    //  shim functions or the xrt_xocl::device functions.
    double clockRatePLMHz = 300.0 ;

    // For trace offload, we can either use FIFO or a memory resource.  If
    //  we use a memory resource, we must have a TS2MM.  We cannot mix FIFO
    //  and memory resources for trace offload.
    bool usesTs2mm = false ;

    // Our AIMs and ASMs can be attached to any AXI-MM or AXI-S connection.
    //  If we cannot associate the AXI-MM or AXI-S connection to a specific
    //  compute unit, we consider them to be "floating" and we lump their
    //  information in a different section of our final trace output
    bool hasFloatingAIMWithTrace = false ;
    bool hasFloatingASMWithTrace = false ;

    // AIMs attached to memory connections are configured just with counters
    //  (no trace) and will have their information reported in a different
    //  section in the summary file.
    bool hasMemoryAIM = false ;

    // Compute unit information
    std::map<int32_t, ComputeUnitInstance*> cus ;

    // Memory information
    std::map<int32_t, Memory*> memoryInfo ;

    // Information on all our Monitor IPs (including shell monitors)
    std::vector<Monitor*> ams ;
    std::vector<Monitor*> aims ;
    std::vector<Monitor*> asms ;

    ~PLInfo() ;
  } ;

  // The AIEInfo struct keeps track of all of the information associated
  //  with AIE constructs in an xclbin.  This includes all configured
  //  counters in the AIE, used GMIO connections, and any PLIO connections
  //  from the AIE to our trace IP.
  struct AIEInfo
  {
    // By default, we assume that the AIE is running at 1 GHz.  This can be
    //  set if different based on information from the device.
    double clockRateAIEMHz = 1000.0 ;

    // The number of PLIO ports on the AIE used for trace.  This should be
    //  equivalent to the number of AIE TS2MMs in the PL portion.
    uint32_t numTracePLIO = 0 ;

    // isGMIORead keeps track of whether or not the AIE GMIO trace ports
    //  have been configured.  We only want to configure once per xclbin,
    //  so it defaults to false and is set after configuration.
    bool isGMIORead = false ;

    // isAIEcounterRead keeps track of whether or not the AIE counters have
    //  been configured.  We only want to configure once per xclbin, 
    //  so it defaults to false and is set after configuration.
    bool isAIEcounterRead = false ;

    // Information on the specific configuration of performance counters
    //  and trace events.
    std::vector<AIECounter*> aieList ;
    std::vector<TraceGMIO*> gmioList ;
    std::map<uint32_t, uint32_t> aieCoreCountersMap ;
    std::map<uint32_t, uint32_t> aieMemoryCountersMap ;
    std::map<uint32_t, uint32_t> aieShimCountersMap ;
    std::map<uint32_t, uint32_t> aieCoreEventsMap ;
    std::map<uint32_t, uint32_t> aieMemoryEventsMap ;
    std::map<uint32_t, uint32_t> aieShimEventsMap ;
    std::vector<std::unique_ptr<aie_cfg_tile>> aieCfgList ;

    // A list of all the NoC nodes identified at compile time used by
    //  our design.  Eventually, these can be configured and polled to
    //  gain information on NoC traffic, but today is unused.
    std::vector<NoCNode*> nocList ;

    ~AIEInfo() ;
  } ;

  // The struct XclbinInfo contains all of the information and configuration
  //  for a single xclbin.  Since an application may load many xclbins, and
  //  we need to output summary information on all of the application at
  //  the end of execution, we need to some configuration data for
  //  all the xclbins that are encountered.  An xclbin can contain PL-specific
  //  information and AIE-specific information
  struct XclbinInfo
  {
    // The unique ID for this xclbin.  We use this to see if the same
    //  xclbin is loaded multiple times in the same application.
    xrt_core::uuid uuid ;
    std::string name ;

    // The interface with actually communicating with the device.  This
    //  handles the abstractions necessary for communicating in emulation,
    //  actual hardware, and through different mechanisms.
    DeviceIntf* deviceIntf = nullptr ;

    // The configuration of the PL portion of the design
    PLInfo pl ;

    // The configuration of the AIE portion of the design (if applicable)
    AIEInfo aie ;

    ~XclbinInfo() ;
  } ;

} // end namespace xdp

#endif
