/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#define XDP_SOURCE

#include <chrono>
#include <ctime>
#include <vector>
#include <thread>
#include <map>
#include <tuple>
#include <limits>
#include <sstream>

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/statistics_database.h"
#include "xdp/profile/database/static_info_database.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/opencl/opencl_summary_writer.h"

#include "core/common/system.h"
#include "core/common/time.h"
#include "core/common/config_reader.h"

#ifdef _WIN32
/* Disable warning for use of localtime */
#pragma warning(disable : 4996)
#endif

namespace xdp {

  OpenCLSummaryWriter::OpenCLSummaryWriter(const char* filename)
    : VPSummaryWriter(filename)
  {
    // The OpenCL Summary Writer will be responsible for
    //  summarizing information from host code API calls as well
    //  as any information on any devices that have been monitored
    //  by other plugins.  This will not instantiate any devices that don't
    //  already exist.

    guidanceRules.push_back(guidanceDeviceExecTime) ;
    guidanceRules.push_back(guidanceCUCalls) ;
    guidanceRules.push_back(guidanceNumMonitors) ;
    guidanceRules.push_back(guidanceMigrateMem) ;
    guidanceRules.push_back(guidanceMemoryUsage) ;
    guidanceRules.push_back(guidancePLRAMDevice) ;
    guidanceRules.push_back(guidanceHBMDevice) ;
    guidanceRules.push_back(guidanceKDMADevice) ;
    guidanceRules.push_back(guidanceP2PDevice) ;
    guidanceRules.push_back(guidanceP2PHostTransfers) ;
    guidanceRules.push_back(guidancePortBitWidth) ;
    guidanceRules.push_back(guidanceKernelCount) ;
    guidanceRules.push_back(guidanceObjectsReleased) ;
    guidanceRules.push_back(guidanceCUContextEn) ;
    guidanceRules.push_back(guidanceTraceMemory) ;
    guidanceRules.push_back(guidanceMaxParallelKernelEnqueues) ;
    guidanceRules.push_back(guidanceCommandQueueOOO) ;
    guidanceRules.push_back(guidancePLRAMSizeBytes) ;
    guidanceRules.push_back(guidanceKernelBufferInfo) ;
    guidanceRules.push_back(guidanceTraceBufferFull) ;
    guidanceRules.push_back(guidanceMemoryTypeBitWidth) ;
    guidanceRules.push_back(guidanceXrtIniSetting) ;
    guidanceRules.push_back(guidanceBufferRdActiveTimeMs) ;
    guidanceRules.push_back(guidanceBufferWrActiveTimeMs) ;
    guidanceRules.push_back(guidanceBufferTxActiveTimeMs) ;
    guidanceRules.push_back(guidanceApplicationRunTimeMs) ;
    guidanceRules.push_back(guidanceTotalKernelRunTimeMs) ;

    // One of our guidance rules requires us to output the xrt.ini settings.
    //  Since they rely on static variables that may be destroyed by
    //  the time our destructor is called, we need to initialize them here
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,profile," << xrt_core::config::get_profile() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,opencl_summary," 
	      << xrt_core::config::get_opencl_summary() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,opencl_device_counter," 
	      << xrt_core::config::get_opencl_device_counter() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,timeline_trace," 
	      << xrt_core::config::get_timeline_trace() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,opencl_trace,"
	      << xrt_core::config::get_opencl_trace() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,xrt_trace,"
	      << xrt_core::config::get_xrt_trace() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,data_transfer_trace,"
	      << xrt_core::config::get_data_transfer_trace() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,power_profile,"
	      << xrt_core::config::get_power_profile() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,power_profile_interval_ms,"
	      << xrt_core::config::get_power_profile_interval_ms() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,stall_trace,"
	      << xrt_core::config::get_stall_trace() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,trace_buffer_size,"
	      << xrt_core::config::get_trace_buffer_size() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,verbosity,"
	      << xrt_core::config::get_verbosity() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,continuous_trace,"
	      << xrt_core::config::get_continuous_trace() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,continuous_trace_interval_ms,"
	      << xrt_core::config::get_continuous_trace_interval_ms() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,lop_trace,"
	      << xrt_core::config::get_lop_trace() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,debug_mode,"
	      << xrt_core::config::get_launch_waveform() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,aie_trace,"
	      << xrt_core::config::get_aie_trace() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,aie_trace_buffer_size,"
	      << xrt_core::config::get_aie_trace_buffer_size() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,aie_profile,"
	      << xrt_core::config::get_aie_profile() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,aie_profile_interval_us,"
	      << xrt_core::config::get_aie_profile_interval_us() ;
      iniSettings.push_back(setting.str()) ;
    }
    {
      std::stringstream setting ;
      setting << "XRT_INI_SETTING,vitis_ai_profile,"
	      << xrt_core::config::get_vitis_ai_profile() ;
      iniSettings.push_back(setting.str()) ;
    }
  }

  OpenCLSummaryWriter::~OpenCLSummaryWriter()
  {
  }

  void OpenCLSummaryWriter::writeHeader()
  {
    fout << "Profile Summary" << std::endl ;

    std::string currentTime = "0000-00-00 0000" ;

    auto time = 
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) ;
    struct tm* p_tstruct = std::localtime(&time) ;
    if (p_tstruct)
    {
      char buf[80] = {0} ;
      strftime(buf, sizeof(buf), "%Y-%m-%d %X", p_tstruct) ;
      currentTime = std::string(buf) ;
    }

    fout << "Generated on: " << currentTime << std::endl ;

    std::string msecSinceEpoch = "" ;
    auto timeSinceEpoch = (std::chrono::system_clock::now()).time_since_epoch();
    auto value =
      std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceEpoch) ;
    msecSinceEpoch = std::to_string(value.count()) ;

    fout << "Msec since Epoch: " << msecSinceEpoch << std::endl ;

    std::string execName = "" ;
#if defined(__linux__) && defined (__x86_64__)
    const int maxLength = 1024 ;
    char buf[maxLength] ;
    ssize_t len ;
    if ((len=readlink("/proc/self/exe", buf, maxLength-1)) != -1)
    {
      buf[len] = '\0' ;
      execName = buf ;
    }

    auto slashLocation = execName.find_last_of("/") ;
    if (slashLocation != std::string::npos)
    {
      execName = execName.substr(slashLocation + 1) ;
    }

#endif

    fout << "Profiled application: " << execName << std::endl ;

    fout << "Target platform: " << "Xilinx" << std::endl ;
    fout << "Tool version: " << getToolVersion() << std::endl ;

    boost::property_tree::ptree xrtInfo ;
    xrt_core::get_xrt_build_info(xrtInfo) ;

    fout << "XRT build version: " 
	 << (xrtInfo.get<std::string>("version", "N/A"))
	 << std::endl ;
    fout << "Build version branch: " 
	 << (xrtInfo.get<std::string>("branch", "N/A"))
	 << std::endl ;
    fout << "Build version hash: "
	 << (xrtInfo.get<std::string>("hash", "N/A"))
	 << std::endl ;
    fout << "Build version date: "
	 << (xrtInfo.get<std::string>("date", "N/A"))
	 << std::endl ;

    std::vector<std::string> deviceNames = 
      (db->getStaticInfo()).getDeviceNames() ;

    fout << "Target devices: " ;
    if (getFlowMode() == SW_EMU)
    {
      fout << (db->getStaticInfo()).getSoftwareEmulationDeviceName()
	   << std::endl ;
    }
    else
    {
      for (unsigned int i = 0 ; i < deviceNames.size() ; ++i)
      {
	if (i != 0) fout << ", " ;
	fout << deviceNames[i] ;
      }
      fout << std::endl ;
    }

    fout << "Flow mode: " ;
    switch(getFlowMode())
    {
    case SW_EMU:  fout << "Software Emulation" ; break ;
    case HW_EMU:  fout << "Hardware Emulation" ; break ;
    case HW:      fout << "System Run" ;         break ;
    case UNKNOWN: fout << "Unknown" ;            break ;
    default:      fout << "Unknown" ;            break ;
    }
    fout << std::endl ;
  }

  // *** Functions for printing out specific tables ***
  void OpenCLSummaryWriter::writeAPICallSummary()
  {
    // Caption
    fout << "OpenCL API Calls" << std::endl ;

    // Columns
    fout << "API Name"          << "," 
	 << "Number Of Calls"   << ","
	 << "Total Time (ms)"   << ","
	 << "Minimum Time (ms)" << ","
	 << "Average Time (ms)" << ","
	 << "Maximum Time (ms)" << "," << std::endl ;
    
    // For each function call, across all of the threads, 
    //  consolidate all the information into what we need
    std::map<std::string,
	     std::tuple<uint64_t,
			double,
			double,
			double> > rows ;

    std::map<std::pair<std::string, std::thread::id>,
	     std::vector<std::pair<double, double>>> callCount =
      (db->getStats()).getCallCount() ;
    
    for (auto call : callCount)
    {
      auto callAndThread = call.first ;
      auto APIName = callAndThread.first ;

      std::vector<std::pair<double, double>> timesOfCalls = call.second ;

      if (rows.find(APIName) == rows.end())
      {
	std::tuple<uint64_t, double, double, double> blank = 
	  std::make_tuple<uint64_t, double, double, double>(0,0,std::numeric_limits<double>::max(),0) ;

	rows[APIName] = blank ;
      }

      for (auto executionTime : timesOfCalls)
      {
	auto timeTaken = executionTime.second - executionTime.first ;

	++(std::get<0>(rows[APIName])) ;
	std::get<1>(rows[APIName]) += timeTaken ;
	if (timeTaken < std::get<2>(rows[APIName]))
	  std::get<2>(rows[APIName]) = timeTaken ;
	if (timeTaken > std::get<3>(rows[APIName]))
	  std::get<3>(rows[APIName]) = timeTaken ;
      }

    }

    for (auto row : rows)
    {
      auto averageTime = 
	(double)(std::get<1>(row.second)) / (double)(std::get<0>(row.second)) ;
      fout << row.first                      << ","         // API Name
	   << std::get<0>(row.second)        << ","         // Number of calls
	   << (std::get<1>(row.second)/1e06) << ","         // Total time
	   << (std::get<2>(row.second)/1e06) << ","         // Minimum time
	   << (averageTime/1e06)             << ","         // Average time
	   << (std::get<3>(row.second)/1e06) << "," // Maximum time
	   << std::endl ;
    }

  }

  void OpenCLSummaryWriter::writeKernelExecutionSummary()
  {
    // Caption
    fout << "Kernel Execution" ;
    if (getFlowMode() == HW_EMU)
    {
      fout << " (includes estimated device time)" ;
    }
    fout << std::endl ;

    // Column headers
    fout << "Kernel"             << ","
	 << "Number Of Enqueues" << ","
	 << "Total Time (ms)"    << ","
	 << "Minimum Time (ms)"  << ","
	 << "Average Time (ms)"  << ","
	 << "Maximum Time (ms)"  << "," 
	 << std::endl ;

    // We can get kernel executions from purely host information
    std::map<std::string, TimeStatistics> kernelExecutions = 
      (db->getStats()).getKernelExecutionStats() ;

    for (auto execution : kernelExecutions)
    {
      fout << execution.first                         << ","
	   << (execution.second).numExecutions        << ","
	   << ((execution.second).totalTime / 1e06)   << ","
	   << ((execution.second).minTime / 1e06)     << ","
	   << ((execution.second).averageTime / 1e06) << ","
	   << ((execution.second).maxTime / 1e06)     << ","
	   << std::endl ;
    }
  }

  void OpenCLSummaryWriter::writeSoftwareEmulationComputeUnitUtilization()
  {
    // Caption
    fout << "Compute Unit Utilization" << std::endl ;

    // Column headers
    fout << "Device"                     << ","
	 << "Compute Unit"               << ","
	 << "Kernel"                     << ","
	 << "Global Work Size"           << ","
	 << "Local Work Size"            << ","
	 << "Number Of Calls"            << ","
	 << "Dataflow Execution"         << ","
	 << "Max Overlapping Executions" << ","
	 << "Dataflow Acceleration"      << ","
	 << "Total Time (ms)"            << ","
	 << "Minimum Time (ms)"          << ","
	 << "Average Time (ms)"          << ","
	 << "Maximum Time (ms)"          << ","
	 << "Clock Frequency (MHz)"      << "," 
	 << std::endl ;

    std::map<std::tuple<std::string, std::string, std::string>,
	     TimeStatistics> cuStats = 
      (db->getStats()).getComputeUnitExecutionStats() ;

    for (auto stat : cuStats)
    {
      std::string cuName          = (std::get<0>(stat.first)) ;
      std::string localWorkGroup  = (std::get<1>(stat.first)) ;
      std::string globalWorkGroup = (std::get<2>(stat.first)) ;

      double averageTime = (stat.second).averageTime ;
      double totalTime   = static_cast<double>((stat.second).totalTime) ;
      double minTime     = static_cast<double>((stat.second).minTime) ;
      double maxTime     = static_cast<double>((stat.second).maxTime) ;
      uint64_t execCount = (stat.second).numExecutions ;

      // Temporarily, just strip away the _# of the compute unit to get
      //  the kernel name
      std::string kernelName = cuName ;
      auto usPosition = kernelName.find("_") ;
      if (usPosition != std::string::npos)
      {
	kernelName = kernelName.substr(0, usPosition - 1) ;
      }

      double speedup = (averageTime*execCount)/totalTime ;
      std::string speedup_string = std::to_string(speedup) + "x" ;

      fout << (db->getStaticInfo()).getSoftwareEmulationDeviceName() << ","
	   << cuName                              << ","
	   << (cuName.substr(0, cuName.size()-2)) << "," 
	   << globalWorkGroup                     << "," 
	   << localWorkGroup                      << ","
	   << execCount                           << ","
	   << "No"                                << ","
	   << 0                                   << "," // TODO?
	   << speedup_string                      << ","
	   << (totalTime / 1e06)                  << ","
	   << (minTime / 1e06)                    << ","
	   << (averageTime / 1e06)                << ","
	   << (maxTime / 1e06)                    << ","
	   << 300                                 << "," 
	   << std::endl ; // TODO?
    }

  }

  void OpenCLSummaryWriter::writeComputeUnitUtilization()
  {
    // Caption
    fout << "Compute Unit Utilization" ;
    if (getFlowMode() == HW_EMU)
    {
      fout << " (includes estimated device times)" ;
    }
    fout << std::endl ;

    // Column headers
    fout << "Device"                     << ","
	 << "Compute Unit"               << ","
	 << "Kernel"                     << ","
	 << "Global Work Size"           << ","
	 << "Local Work Size"            << ","
	 << "Number Of Calls"            << ","
	 << "Dataflow Execution"         << ","
	 << "Max Overlapping Executions" << ","
	 << "Dataflow Acceleration"      << ","
	 << "Total Time (ms)"            << ","
	 << "Minimum Time (ms)"          << ","
	 << "Average Time (ms)"          << ","
	 << "Maximum Time (ms)"          << ","
	 << "Clock Frequency (MHz)"      << "," 
	 << std::endl ;

    // The static portion of this output has to come from the
    //  static database.  The counter portion has to come from the
    //  dynamic database.  Right now, the compute units are not
    //  aligned between the two.  We have to make sure this information
    //  is accessible.
    
    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    // For every device that is connected...
    for (auto device : infos)
    {
      uint64_t deviceId = device->deviceId ;

      // For every xclbin that was loaded on this device
      for (auto xclbin : device->loadedXclbins) {
	xclCounterResults values =
	  (db->getDynamicInfo()).getCounterResults(deviceId, xclbin->uuid) ;

	// For every compute unit in the xclbin
	for (auto cuInfo : xclbin->cus)
	{
	  // This info is the same for every execution call
	  uint64_t amSlotID = (uint64_t)((cuInfo.second)->getAccelMon()) ;
	  std::string cuName = (cuInfo.second)->getName() ;
	  std::string kernelName = (cuInfo.second)->getKernelName() ;
	  std::string cuLocalDimensions = (cuInfo.second)->getDim() ;
	  std::string dataflowEnabled = 
	    (cuInfo.second)->dataflowEnabled() ? "Yes" : "No" ;
	  
	  // For each compute unit, we can have executions from the host
	  //  with different global work sizes.  Determine the number of 
	  //  execution types here
	  std::vector<std::pair<std::string, TimeStatistics>> cuCalls = 
	    (db->getStats()).getComputeUnitExecutionStats(cuName) ;

	  for (auto cuCall : cuCalls)
	  {
	    std::string globalWorkDimensions = cuCall.first ;

	    auto kernelClockMHz = xclbin->clockRateMHz ;
	    double deviceCyclesMsec = (double)(kernelClockMHz) * 1000.0 ;

	    double cuRunTimeMsec =
	      (double)(values.CuBusyCycles[amSlotID]) / deviceCyclesMsec ;
	    double cuRunTimeAvgMsec = (double)(values.CuExecCycles[amSlotID]) / deviceCyclesMsec / (double)(values.CuExecCount[amSlotID]) ;
	    double cuMaxExecCyclesMsec = (double)(values.CuMaxExecCycles[amSlotID]) / deviceCyclesMsec ;
	    double cuMinExecCyclesMsec = (double)(values.CuMinExecCycles[amSlotID]) / deviceCyclesMsec ;

	    double speedup = (cuRunTimeAvgMsec * (double)(values.CuExecCount[amSlotID])) / cuRunTimeMsec ;

	    //double speedup =
	    // (averageTime*(values.CuExecCount[cuIndex]))/totalTime ;
	    std::string speedup_string = std::to_string(speedup) + "x" ;

	    fout << (device->deviceName) << "," 
		 << cuName << ","
		 << kernelName << ","
		 << globalWorkDimensions << ","
		 << cuLocalDimensions << ","
		 << values.CuExecCount[amSlotID] << ","
		 << dataflowEnabled << ","
		 << values.CuMaxParallelIter[amSlotID] << ","
		 << speedup_string << ","
		 << cuRunTimeMsec << "," //<< (totalTime / 1e06) << ","
		 << cuMinExecCyclesMsec << "," //<< (minTime / 1e06) << ","
		 << cuRunTimeAvgMsec << "," //<< (averageTime /1e06) << ","
		 << cuMaxExecCyclesMsec << "," //<< (maxTime / 1e06) << "," 
		 << (xclbin->clockRateMHz) << ","
		 << std::endl ;
	  }
	}
      }
    }
  }

  void OpenCLSummaryWriter::writeComputeUnitStallInformation()
  {
    if (!(db->getStaticInfo().hasStallInfo())) return ;

    // Caption
    fout << "Compute Units: Stall Information" << std::endl ;

    // Column headers
    fout << "Compute Unit"                      << ","
	 << "Execution Count"                   << ","
	 << "Running Time (ms)"                 << ","
	 << "Intra-Kernel Dataflow Stalls (ms)" << ","
	 << "External Memory Stalls (ms)"       << ","
	 << "Inter-Kernel Pipe Stalls (ms)"     << "," << std::endl ;

    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : infos)
    {
      for (auto xclbin : device->loadedXclbins)
      {
	xclCounterResults values = (db->getDynamicInfo()).getCounterResults(device->deviceId, xclbin->uuid) ;
	uint64_t j = 0 ;      
	for (auto cu : (xclbin->cus))
	{
          double deviceCyclesMsec = (double)(xclbin->clockRateMHz * 1000.0);

	  fout << (cu.second)->getName()     << "," 
	       << values.CuExecCount[j]      << ","
	       << (values.CuExecCycles[j] / deviceCyclesMsec)     << ","
	       << (values.CuStallIntCycles[j] / deviceCyclesMsec) << ","
	       << (values.CuStallExtCycles[j] / deviceCyclesMsec) << ","
	       << (values.CuStallStrCycles[j] / deviceCyclesMsec) << std::endl ;
	  ++j ;
	}
      }
    }
  }

  void OpenCLSummaryWriter::writeDataTransferHostToGlobalMemory()
  {
    // Caption
    fout << "Data Transfer: Host to Global Memory" << std::endl ;

    // Column headers
    fout << "Context:Number of Devices"         << ","
	 << "Transfer Type"                     << ","
	 << "Number Of Buffer Transfers"        << ","
	 << "Transfer Rate (MB/s)"              << ","
	 << "Average Bandwidth Utilization (%)" << ","
	 << "Average Buffer Size (KB)"          << ","
	 << "Total Time (ms)"                   << ","
	 << "Average Time (ms)"                 << "," 
	 << std::endl ;

    std::map<std::pair<uint64_t, uint64_t>, BufferStatistics> hostReads =
      (db->getStats()).getHostReads() ;
    std::map<std::pair<uint64_t, uint64_t>, BufferStatistics> hostWrites =
      (db->getStats()).getHostWrites() ;

    for (auto read : hostReads)
    {
      std::string contextName = "context" + std::to_string(read.first.first) ;
      uint64_t numDevices =
	(db->getStaticInfo()).getNumDevices(read.first.first) ;
      if (getFlowMode() == HW_EMU)
      {
	fout << contextName << ":" << numDevices << ","
	     << "READ" << ","
	     << (read.second).count << ","
	     << "N/A" << ","
	     << "N/A" << ","
	     << ((double)((read.second).averageSize) / 1000.0) << ","
	     << "N/A" << ","
	     << "N/A" << "," << std::endl ;
      }
      else
      {
        double totalTimeInS  = (double)((read.second).totalTime / 1e09);
        double totalSizeInMB = (double)((read.second).totalSize / 1e06);
        double transferRate  = totalSizeInMB / totalTimeInS; 

	double maxReadBW =
	  (db->getStaticInfo()).getMaxReadBW(read.first.second) ;
	double aveBWUtil = (100.0 * transferRate) / maxReadBW ;

	fout << contextName << ":" << numDevices << ","
	     << "READ" << ","
	     << (read.second).count << ","
	     << transferRate << ","
	     << aveBWUtil << ","
	     << ((double)((read.second).averageSize) / 1000.0) << ","
	     << ((read.second).totalTime / 1e06) << ","
	     << ((read.second).averageTime / 1e06) << "," << std::endl ;
      }
    }

    for (auto write : hostWrites)
    {
      std::string contextName = "context" + std::to_string(write.first.first) ;
      uint64_t numDevices =
	(db->getStaticInfo()).getNumDevices(write.first.first) ;

      if (getFlowMode() == HW_EMU)
      {
	fout << contextName << ":" << numDevices << ","
	     << "WRITE" << ","
	     << (write.second).count << ","
	     << "N/A" << ","
	     << "N/A" << ","
	     << ((double)((write.second).averageSize) / 1000.0) << ","
	     << "N/A" << ","
	     << "N/A" << "," << std::endl ;
      }
      else
      {
        double totalTimeInS  = (double)((write.second).totalTime / 1e09);
        double totalSizeInMB = (double)((write.second).totalSize / 1e06);
        double transferRate  = totalSizeInMB / totalTimeInS; 

	double maxWriteBW =
	  (db->getStaticInfo()).getMaxWriteBW(write.first.second);
	double aveBWUtil = (100.0 * transferRate) / maxWriteBW ;

	fout << contextName << ":" << numDevices << "," 
	     << "WRITE" << ","
	     << (write.second).count << ","
	     << transferRate << ","
	     << aveBWUtil << ","
	     << ((double)((write.second).averageSize) / 1000.0) << ","
	     << ((write.second).totalTime / 1e06) << ","
	     << ((write.second).averageTime / 1e06) << "," << std::endl ;
      }
    }
  }

  void OpenCLSummaryWriter::writeDataTransferKernelsToGlobalMemory()
  {
    // Caption
    fout << "Data Transfer: Kernels to Global Memory" << std::endl ;

    // Column headers
    fout << "Device"                            << ","
	 << "Compute Unit/Port Name"            << ","
	 << "Kernel Arguments"                  << ","
	 << "Memory Resources"                  << ","
	 << "Transfer Type"                     << ","
	 << "Number Of Transfers"               << ","
	 << "Transfer Rate (MB/s)"              << ","
	 << "Average Bandwidth Utilization (%)" << ","
	 << "Average Size (KB)"                 << ","
	 << "Average Latency (ns)"              << "," 
	 << std::endl ;

    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : infos)
    {
      for (auto xclbin : device->loadedXclbins)
      {
	xclCounterResults values = (db->getDynamicInfo()).getCounterResults(device->deviceId, xclbin->uuid) ;

	// Counter results don't use the slotID.  Instead, they are filled
	//  in the struct in the order in which we found them.
	uint64_t monitorId = 0 ;
	for (auto monitor : xclbin->aimList) {
	  if (monitor->cuIndex == -1) {
	    // This AIM is either a shell or floating 
	    ++monitorId ;
	    continue ;
	  }

	  auto writeTranx = values.WriteTranx[monitorId] ;
	  auto readTranx  = values.ReadTranx[monitorId] ;

	  uint64_t totalReadBusyCycles  = values.ReadBusyCycles[monitorId] ;
	  uint64_t totalWriteBusyCycles = values.WriteBusyCycles[monitorId] ;

	  double totalReadTime = 
	    (double)(totalReadBusyCycles) / (1000.0 * xclbin->clockRateMHz) ;
	  double totalWriteTime =
	    (double)(totalWriteBusyCycles) / (1000.0 * xclbin->clockRateMHz) ;

	  // Use the name of the monitor to determine the port and memory
	  std::string portName   = "" ;
	  std::string memoryName = "" ;
	  size_t slashPosition = (monitor->name).find("/") ;
	  if (slashPosition != std::string::npos) {
	    auto position = slashPosition + 1 ;
	    auto length = (monitor->name).size() - position ;

	    // Split the monitor name into port and memory position
	    std::string lastHalf = (monitor->name).substr(position, length) ;
	      
	    size_t dashPosition = lastHalf.find("-") ;
	    if (dashPosition != std::string::npos) {
	      auto remainingLength = lastHalf.size() - dashPosition - 1 ;
	      portName = lastHalf.substr(0, dashPosition) ;
	      memoryName = lastHalf.substr(dashPosition + 1, remainingLength);
	    }
	    else {
	      portName = lastHalf ;
	    }
	  }
	  if (writeTranx > 0) {
	    double transferRate = (totalWriteTime == 0.0) ? 0 :
	      (double)(values.WriteBytes[monitorId]) / (1000.0 * totalWriteTime);
	    double aveBW =
	      (100.0 * transferRate) / xclbin->maxWriteBW ;
	    if (aveBW > 100.0) aveBW = 100.0 ;

	    fout << (device->deviceName) << ","
		 << xclbin->cus[monitor->cuIndex]->getName() << "/"
		 << portName << ","
		 << (monitor->args) << ","
		 << memoryName << ","
		 << "WRITE" << ","
		 << writeTranx << ","
		 << transferRate << ","
		 << aveBW << ","
		 << (double)(values.WriteBytes[monitorId] / writeTranx) / 1000.0 << ","
		 << (values.WriteLatency[monitorId] / writeTranx) << "," 
		 << std::endl ;
	  }
	  if (readTranx > 0) {
	      double transferRate = (totalReadTime == 0.0) ? 0 :
		(double)(values.ReadBytes[monitorId]) / (1000.0 * totalReadTime);
	      double aveBW =
		(100.0 * transferRate) / xclbin->maxReadBW ;
	      if (aveBW > 100.0) aveBW = 100.0 ;

	      fout << (device->deviceName) << ","
		   << xclbin->cus[monitor->cuIndex]->getName() << "/"
		   << portName << ","
		   << (monitor->args) << ","
		   << memoryName << ","
		   << "READ" << ","
		   << readTranx << ","
		   << transferRate << ","
		   << aveBW << ","
		   << (double)(values.ReadBytes[monitorId] / readTranx) / 1000.0 << ","
		   << (values.ReadLatency[monitorId] / readTranx) << "," 
		   << std::endl ;
	  }
	  ++monitorId ;
	}
      }
    }
  }

  void OpenCLSummaryWriter::writeStreamDataTransfers()
  {
    // Caption
    fout << "Data Transfer: Streams" << std::endl ;
    
    // Column headers
    fout << "Device"                  << ","
         << "Master Port"             << ","
         << "Master Kernel Arguments" << ","
         << "Slave Port"              << ","
         << "Slave Kernel Arguments"  << ","
         << "Number Of Transfers"     << ","
         << "Transfer Rate (MB/s)"    << ","
         << "Average Size (KB)"       << ","
         << "Link Utilization (%)"    << ","
         << "Link Starve (%)"         << ","
         << "Link Stall (%)"          << "," 
         << std::endl ;
    
    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;
    
    for (auto device : infos) 
    {
      for (auto xclbin : device->loadedXclbins)
      {
        xclCounterResults values = (db->getDynamicInfo()).getCounterResults(device->deviceId, xclbin->uuid) ;
        for (auto cu : xclbin->cus)
        {
          //std::vector<Monitor*> monitors = (cu.second)->getMonitors() ;
          std::vector<uint32_t>* asmMonitors = (cu.second)->getASMs() ;
          
          //for (auto monitor : monitors)
          for (auto asmMonitorId : (*asmMonitors))
          {
            //if (monitor->type != AXI_STREAM_MONITOR) continue ;
            Monitor* monitor = (db->getStaticInfo()).getASMonitor(device->deviceId, xclbin, asmMonitorId) ;
            
            uint64_t numTranx = values.StrNumTranx[asmMonitorId] ;
            uint64_t busyCycles = values.StrBusyCycles[asmMonitorId];
            if(0 == numTranx) {
              continue;
            }
 
            std::string masterPort = "" ;
            std::string slavePort = "" ;
            std::string masterArgs = "" ;
            std::string slaveArgs = "" ;
            
            size_t dashPosition = (monitor->name).find("-") ;
            if (dashPosition != std::string::npos)
            {
              std::string firstHalf = (monitor->name).substr(0, dashPosition);
              std::string secondHalf = (monitor->name).substr(dashPosition + 1,
                  (monitor->name).size()-dashPosition-1) ;
              size_t slashPosition = firstHalf.find("/") ;
              masterPort = firstHalf;
              masterArgs = firstHalf.substr(slashPosition + 1, firstHalf.size()-slashPosition-1) ;
              
              slashPosition = secondHalf.find("/") ;
              slavePort = secondHalf;
              slaveArgs = secondHalf.substr(slashPosition + 1, secondHalf.size()-slashPosition-1) ;
            }
            
            double transferTime = busyCycles / xclbin->clockRateMHz ;
            double transferRate = (transferTime == 0.0) ? 0 : values.StrDataBytes[asmMonitorId] / transferTime ;
            
            double linkStarve = (0 == busyCycles) ? 0 : 
                (double)(values.StrStarveCycles[asmMonitorId]) / (double)(busyCycles) * 100.0 ;
            double linkStall = (0 == busyCycles) ? 0 : 
                (double)(values.StrStallCycles[asmMonitorId]) / (double)(busyCycles) * 100.0 ;
            double linkUtil = 100.0 - linkStarve - linkStall ;
            double avgSizeInKB = ((values.StrDataBytes[asmMonitorId] / numTranx)) / 1000.0;
            
            fout << (device->deviceName) << ","
                 << masterPort << ","
                 << masterArgs << ","
                 << slavePort << ","
                 << slaveArgs << ","
                 << numTranx << ","
                 << transferRate << ","
                 << avgSizeInKB << ","
                 << linkUtil << "," 
                 << linkStarve << ","
                 << linkStall << ","
                 << std::endl ;
          }
        }
      }
    }
  }

  void OpenCLSummaryWriter::writeDataTransferDMA()
  {
    // Only output this table and header if some device has 
    //  DMA monitors in the shell
    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    if (infos.size() == 0) return ;
    bool printTable = false ;
    for (auto device : infos) {
      if (device->hasDMAMonitor()) {
	printTable = true ;
	break ;
      }
    }
    if (!printTable) return ;

    // Caption
    fout << "Data Transfer: DMA" << std::endl ;

    // Columns
    fout << "Device"                   << ","
	 << "Transfer Type"            << ","
	 << "Number Of Transfers"      << ","
	 << "Transfer Rate (MB/s)"     << ","
	 << "Total Data Transfer (MB)" << ","
	 << "Total Time (ms)"          << ","
	 << "Average Size (KB)"        << ","
	 << "Average Latency (ns)"     << "," 
	 << std::endl ;


    for (auto device : infos)
    {
      for (auto xclbin : device->loadedXclbins)
      {
      
      uint64_t AIMIndex = 0 ;
      for (auto monitor : device->currentXclbin()->aimList)
      {
	if (monitor->name.find("Host to Device") != std::string::npos)
	{
	  // This is the monitor we are looking for
	  xclCounterResults values =
	    (db->getDynamicInfo()).getCounterResults(device->deviceId, xclbin->uuid) ;

	  if (values.WriteTranx[AIMIndex] > 0)
	  {
	    uint64_t totalWriteBusyCycles = values.WriteBusyCycles[AIMIndex] ;
	    double totalWriteTime =
	      (double)(totalWriteBusyCycles) / (1000.0 * xclbin->clockRateMHz);
	    double writeTransferRate = (totalWriteTime == 0.0) ? 0 :
	      (double)(values.WriteBytes[AIMIndex]) / (1000.0 * totalWriteTime);

	    fout << device->deviceName << ","
		 << "WRITE" << ","
		 << values.WriteTranx[AIMIndex] << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << writeTransferRate << "," ;
	    }

	    fout << ((double)(values.WriteBytes[AIMIndex] / 1.0e6)) << "," ;

	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << (totalWriteTime / 1e06) << "," ;
	    }
	    fout << (double)(values.WriteBytes[AIMIndex]) / (double)(values.WriteTranx[AIMIndex]) << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," << std::endl ;
	    }
	    else
	    {
	      fout << ((1000.0 * values.WriteLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.WriteTranx[AIMIndex]) << "," << std::endl ;
	    }
	  }
	  if (values.ReadTranx[AIMIndex] > 0)
	  {
	    uint64_t totalReadBusyCycles = values.ReadBusyCycles[AIMIndex] ;
	    double totalReadTime =
	      (double)(totalReadBusyCycles) / (1000.0 * xclbin->clockRateMHz);
	    double readTransferRate = (totalReadTime == 0.0) ? 0 :
	      (double)(values.ReadBytes[AIMIndex]) / (1000.0 * totalReadTime);

	    fout << device->deviceName << ","
		 << "READ" << ","
		 << values.ReadTranx[AIMIndex] << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << readTransferRate << "," ;
	    }

	    fout << ((double)(values.ReadBytes[AIMIndex] / 1.0e6)) << "," ;

	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << (totalReadTime / 1e06) << "," ;
	    }
	    fout << (double)(values.ReadBytes[AIMIndex]) / (double)(values.ReadTranx[AIMIndex]) << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," << std::endl ;
	    }
	    else
	    {
	      fout << ((1000.0 * values.ReadLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.ReadTranx[AIMIndex]) << "," << std::endl ;
	    }
	  }
	}
	++AIMIndex ;
      }
      }
    }
  }

  void OpenCLSummaryWriter::writeDataTransferDMABypass()
  {
    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    if (infos.size() == 0) return ;
    bool printTable = false ;
    for (auto device : infos) {
      if (device->hasDMABypassMonitor()) {
	printTable = true ;
	break ;
      }
    }
    if (!printTable) return ;

    // Caption
    fout << "Data Transfer: DMA Bypass" << std::endl ;

    // Columns
    fout << "Device"                   << ","
	 << "Transfer Type"            << ","
	 << "Number Of Transfers"      << ","
	 << "Transfer Rate (MB/s)"     << ","
	 << "Total Data Transfer (MB)" << ","
	 << "Total Time (ms)"          << ","
	 << "Average Size (KB)"        << ","
	 << "Average Latency (ns)"     << "," 
	 << std::endl ;

    for (auto device : infos)
    {
      for (auto xclbin : device->loadedXclbins)
	{
      uint64_t AIMIndex = 0 ;
      for (auto monitor : device->currentXclbin()->aimList)
      {
	if (monitor->name.find("Peer to Peer") != std::string::npos)
	{
	  // This is the monitor we are looking for
	  xclCounterResults values =
	    (db->getDynamicInfo()).getCounterResults(device->deviceId, xclbin->uuid) ;

	  if (values.WriteTranx[AIMIndex] > 0)
	  {
	    uint64_t totalWriteBusyCycles = values.WriteBusyCycles[AIMIndex] ;
	    double totalWriteTime =
	      (double)(totalWriteBusyCycles) / (1000.0 * xclbin->clockRateMHz);
	    double writeTransferRate = (totalWriteTime == 0.0) ? 0 :
	      (double)(values.WriteBytes[AIMIndex]) / (1000.0 * totalWriteTime);

	    fout << device->deviceName << ","
		 << "WRITE" << ","
		 << values.WriteTranx[AIMIndex] << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << writeTransferRate << "," ;
	    }

	    fout << ((double)(values.WriteBytes[AIMIndex] / 1.0e6)) << "," ;

	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << (totalWriteTime / 1e06) << "," ;
	    }
	    fout << (double)(values.WriteBytes[AIMIndex]) / (double)(values.WriteTranx[AIMIndex]) << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," << std::endl ;
	    }
	    else
	    {
	      fout << ((1000.0 * values.WriteLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.WriteTranx[AIMIndex]) << "," << std::endl ;
	    }
	  }
	  if (values.ReadTranx[AIMIndex] > 0)
	  {
	    uint64_t totalReadBusyCycles = values.ReadBusyCycles[AIMIndex] ;
	    double totalReadTime =
	      (double)(totalReadBusyCycles) / (1000.0 * xclbin->clockRateMHz);
	    double readTransferRate = (totalReadTime == 0.0) ? 0 :
	      (double)(values.ReadBytes[AIMIndex]) / (1000.0 * totalReadTime);

	    fout << device->deviceName << ","
		 << "READ" << ","
		 << values.ReadTranx[AIMIndex] << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << readTransferRate << "," ;
	    }

	    fout << ((double)(values.ReadBytes[AIMIndex] / 1.0e6)) << "," ;

	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << (totalReadTime / 1e06) << "," ;
	    }
	    fout << (double)(values.ReadBytes[AIMIndex]) / (double)(values.ReadTranx[AIMIndex]) << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," << std::endl ;
	    }
	    else
	    {
	      fout << ((1000.0 * values.ReadLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.ReadTranx[AIMIndex]) << "," << std::endl ;
	    }
	  }
	}
	++AIMIndex ;
      }
	}
    }
  }

  void OpenCLSummaryWriter::writeDataTransferGlobalMemoryToGlobalMemory()
  {
    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    if (infos.size() == 0) return ;
    bool printTable = false ;
    for (auto device : infos) {
      if (device->hasKDMAMonitor()) {
	printTable = true ;
	break ;
      }
    }
    if (!printTable) return ;

    // Caption
    fout << "Data Transfer: Global Memory to Global Memory" << std::endl ;

    // Columns
    fout << "Device"                   << ","
	 << "Transfer Type"            << ","
	 << "Number Of Transfers"      << ","
	 << "Transfer Rate (MB/s)"     << ","
	 << "Total Data Transfer (MB)" << ","
	 << "Total Time (ms)"          << ","
	 << "Average Size (KB)"        << ","
	 << "Average Latency (ns)"     << ","
	 << std::endl ;

    for (auto device : infos)
    {
      for (auto xclbin : device->loadedXclbins)
	{
      uint64_t AIMIndex = 0 ;
      for (auto monitor : device->currentXclbin()->aimList)
      {
	if (monitor->name.find("Memory to Memory") != std::string::npos)
	{
	  // This is the monitor we are looking for
	  xclCounterResults values =
	    (db->getDynamicInfo()).getCounterResults(device->deviceId, xclbin->uuid) ;

	  if (values.WriteTranx[AIMIndex] > 0)
	  {
	    uint64_t totalWriteBusyCycles = values.WriteBusyCycles[AIMIndex] ;
	    double totalWriteTime =
	      (double)(totalWriteBusyCycles) / (1000.0 * xclbin->clockRateMHz);
	    double writeTransferRate = (totalWriteTime == 0.0) ? 0 :
	      (double)(values.WriteBytes[AIMIndex]) / (1000.0 * totalWriteTime);

	    fout << device->deviceName << ","
		 << "WRITE" << ","
		 << values.WriteTranx[AIMIndex] << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << writeTransferRate << "," ;
	    }

	    fout << ((double)(values.WriteBytes[AIMIndex] / 1.0e6)) << "," ;

	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << (totalWriteTime / 1e06) << "," ;
	    }
	    fout << (double)(values.WriteBytes[AIMIndex]) / (double)(values.WriteTranx[AIMIndex]) << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," << std::endl ;
	    }
	    else
	    {
	      fout << ((1000.0 * values.WriteLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.WriteTranx[AIMIndex]) << "," << std::endl ;
	    }
	  }
	  if (values.ReadTranx[AIMIndex] > 0)
	  {
	    uint64_t totalReadBusyCycles = values.ReadBusyCycles[AIMIndex] ;
	    double totalReadTime =
	      (double)(totalReadBusyCycles) / (1000.0 * xclbin->clockRateMHz);
	    double readTransferRate = (totalReadTime == 0.0) ? 0 :
	      (double)(values.ReadBytes[AIMIndex]) / (1000.0 * totalReadTime);

	    fout << device->deviceName << ","
		 << "READ" << ","
		 << values.ReadTranx[AIMIndex] << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << readTransferRate << "," ;
	    }

	    fout << ((double)(values.ReadBytes[AIMIndex] / 1.0e6)) << "," ;

	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << (totalReadTime / 1e06) << "," ;
	    }
	    fout << (double)(values.ReadBytes[AIMIndex]) / (double)(values.ReadTranx[AIMIndex]) << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," << std::endl ;
	    }
	    else
	    {
	      fout << ((1000.0 * values.ReadLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.ReadTranx[AIMIndex]) << "," << std::endl ;
	    }
	  }
	}
	++AIMIndex ;
      }
	}
    }
  }

  void OpenCLSummaryWriter::writeTopDataTransferKernelAndGlobal()
  {
    // Caption
    fout << "Top Data Transfer: Kernels to Global Memory" << std::endl ;

    // Columns
    fout << "Device"                     << ","
	 << "Compute Unit"               << ","
	 << "Number of Transfers"        << ","
	 << "Average Bytes per Transfer" << ","
	 << "Transfer Efficiency (%)"    << ","
	 << "Total Data Transfer (MB)"   << ","
	 << "Total Write (MB)"           << ","
	 << "Total Read (MB)"            << ","
	 << "Total Transfer Rate (MB/s)" << "," 
	 << std::endl ;

    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : infos)
    {
      uint64_t deviceId = device->deviceId ;

      for (auto xclbin : device->loadedXclbins)
      {
	xclCounterResults values =
	  (db->getDynamicInfo()).getCounterResults(deviceId, xclbin->uuid) ;

	for (auto cu : xclbin->cus)
	{
	  // For each CU, we need to find the monitor that has 
	  //  the most transactions
	  std::string computeUnitName = (cu.second)->getName() ;
	  std::vector<uint32_t>* aimMonitors = (cu.second)->getAIMs() ;

	  // These are the max we have seen so far
	  uint64_t numTransfers = 0 ;
	  double aveBytesPerTransfer = 0 ;
	  double transferEfficiency = 0 ;
	  uint64_t totalDataTransfer = 0 ;
	  uint64_t totalWriteBytes = 0 ;
	  uint64_t totalReadBytes = 0 ;
	  double totalTransferRate = 0 ;

	  for (auto AIMIndex : (*aimMonitors))
	  {
	    auto writeTranx = values.WriteTranx[AIMIndex] ;
	    auto readTranx = values.ReadTranx[AIMIndex] ;
	    auto totalTranx = writeTranx + readTranx ;

	    if (totalTranx > numTransfers) {
	      numTransfers = totalTranx ;
	      totalReadBytes = values.ReadBytes[AIMIndex] ;
	      totalWriteBytes = values.WriteBytes[AIMIndex] ;
	      aveBytesPerTransfer =
		(double)(totalReadBytes + totalWriteBytes)/(double)(numTransfers);
	      // TODO: Fix bit width calculation here
	      transferEfficiency = (100.0 * aveBytesPerTransfer) / 4096 ; 
	      totalDataTransfer = totalReadBytes + totalWriteBytes ;
	      auto totalBusyCycles =
		values.ReadBusyCycles[AIMIndex]+values.WriteBusyCycles[AIMIndex];
	      double totalTimeMSec = 
		(double)(totalBusyCycles) /(1000.0 * xclbin->clockRateMHz) ;
	      totalTransferRate =
		(totalTimeMSec == 0) ? 0.0 :
		(double)(totalDataTransfer) / (1000.0 * totalTimeMSec) ;
	    }
	  }

	  // Verify that this CU actually had some data transfers registered
	  if (computeUnitName != "" && numTransfers != 0) {
	    fout << device->deviceName << ","
		 << computeUnitName << ","
		 << numTransfers << ","
		 << aveBytesPerTransfer << ","
		 << transferEfficiency << ","
		 << (double)(totalDataTransfer) / 1.0e6 << ","
		 << (double)(totalWriteBytes) / 1.0e6 << ","
		 << (double)(totalReadBytes) / 1.0e6 << ","
		 << totalTransferRate << "," << std::endl ;
	  }
	}
      }
    }
  }

  void OpenCLSummaryWriter::writeTopKernelExecution()
  {
    // Caption
    fout << "Top Kernel Execution" << std::endl ;

    // Columns
    fout << "Kernel Instance Address" << ","
	 << "Kernel"                  << ","
	 << "Context ID"              << ","
	 << "Command Queue ID"        << ","
	 << "Device"                  << ","
	 << "Start Time (ms)"         << ","
	 << "Duration (ms)"           << ","
	 << "Global Work Size"        << ","
	 << "Local Work Size"         << "," << std::endl ;

    for (std::list<KernelExecutionStats>::iterator iter = (db->getStats()).getTopKernelExecutions().begin() ;
	 iter != (db->getStats()).getTopKernelExecutions().end() ;
	 ++iter)
    {
      fout << (*iter).kernelInstanceAddress << ","
	   << (*iter).kernelName << ","
	   << (*iter).contextId << ","
	   << (*iter).commandQueueId << "," 
	   << (*iter).deviceName << ","
	   << (double)((*iter).startTime) / 1.0e6 << ","
	   << (double)((*iter).duration) / 1.0e6 << ","
	   << (*iter).globalWorkSize << ","
	   << (*iter).localWorkSize << "," << std::endl ;
    }
  }

  void OpenCLSummaryWriter::writeTopMemoryWrites()
  {
    // Caption
    fout << "Top Memory Writes: Host to Global Memory" << std::endl ;

    // Columns
    fout << "Buffer Address"     << ","
	 << "Context ID"         << ","
	 << "Command Queue ID"   << ","
	 << "Start Time (ms)"    << ","
	 << "Duration (ms)"      << ","
	 << "Buffer Size (KB)"   << ","
	 << "Writing Rate(MB/s)" << "," << std::endl ;

    for (std::list<BufferTransferStats>::iterator iter = (db->getStats()).getTopHostWrites().begin() ;
	 iter != (db->getStats()).getTopHostWrites().end() ;
	 ++iter)
    {
      double durationMS = (double)((*iter).duration) / 1.0e6 ;
      double rate = ((double)((*iter).size) / 1000.0) * durationMS ;

      fout << (*iter).address << ","
	   << (*iter).contextId << ","
	   << (*iter).commandQueueId << ","
	   << (double)((*iter).startTime) / 1.0e6 << "," ;
      if (getFlowMode() == HW)
	fout << durationMS << "," ;
      else
	fout << "N/A," ;
      fout << (double)((*iter).size) / 1000.0 << "," ;
      if (getFlowMode() == HW)
	fout << rate << "," << std::endl ;
      else
	fout << "N/A" << "," << std::endl ;
    }
  }

  void OpenCLSummaryWriter::writeTopMemoryReads()
  {
    // Caption
    fout << "Top Memory Reads: Host to Global Memory" << std::endl ;

    // Columns
    fout << "Buffer Address"     << ","
	 << "Context ID"         << "," 
	 << "Command Queue ID"   << ","
	 << "Start Time (ms)"    << ","
	 << "Duration (ms)"      << ","
	 << "Buffer Size (KB)"   << ","
	 << "Reading Rate(MB/s)" << "," << std::endl ;

    for (std::list<BufferTransferStats>::iterator iter = (db->getStats()).getTopHostReads().begin() ;
	 iter != (db->getStats()).getTopHostReads().end() ;
	 ++iter)
    {
      double durationMS = (double)((*iter).duration) / 1.0e6 ;
      double rate = ((double)((*iter).size) / 1000.0) * durationMS ;

      fout << (*iter).address << "," 
	   << (*iter).contextId << ","
	   << (*iter).commandQueueId << ","
	   << (double)((*iter).startTime) / 1.0e6 << "," ;
      if (getFlowMode() == HW)
	fout << durationMS << "," ;
      else
	fout << "N/A," ;
      fout << (double)((*iter).size) / 1000.0 << "," ;
      if (getFlowMode() == HW)
	fout << rate << "," << std::endl ;
      else
	fout << "N/A" << "," << std::endl ;
    }
  }

  void OpenCLSummaryWriter::writeUserLevelEvents()
  {
    if (!(db->getStats()).eventInformationPresent()) return ;

    fout << "User Level Events" << std::endl ;
    fout << "Label" << ","
	 << "Count" << ","
	 << std::endl ;

    std::map<const char*, uint64_t>& counts = (db->getStats()).getEventCounts();
    for (auto iter : counts) {
      const char* label = (iter.first == nullptr) ? " " : iter.first ;
      fout << label       << ","
	   << iter.second << ","
	   << std::endl ;
    }
  }

  void OpenCLSummaryWriter::writeUserLevelRanges()
  {
    if (!(db->getStats()).rangeInformationPresent()) return ;

    fout << "User Level Ranges" << std::endl ;
    fout << "Label"   << ","
	 << "Tooltip" << ","
	 << "Count"   << ","
	 << "Min Duration (ms)" << ","
	 << "Max Duration (ms)" << ","
	 << "Total Time Duration (ms)" << ","
	 << "Average Duration (ms)" << ","
	 << std::endl ;

    std::map<std::pair<const char*, const char*>, uint64_t>& counts =
      (db->getStats()).getRangeCounts() ;
    std::map<std::pair<const char*, const char*>, uint64_t>& minDurations =
      (db->getStats()).getMinRangeDurations() ;
    std::map<std::pair<const char*, const char*>, uint64_t>& maxDurations =
      (db->getStats()).getMaxRangeDurations() ;
    std::map<std::pair<const char*, const char*>, uint64_t>& totalDurations =
      (db->getStats()).getTotalRangeDurations() ;

    for (auto iter : counts) {
      const char* label =
	(iter.first.first == nullptr) ? " " : iter.first.first;
      const char* tooltip =
	(iter.first.second == nullptr) ? " " : iter.first.second ;
      fout << label       << ","
	   << tooltip     << ","
	   << iter.second << ","
	   << (double)minDurations[iter.first] / 1e06 << ","
	   << (double)maxDurations[iter.first] / 1e06 << ","
	   << (double)totalDurations[iter.first] / 1e06<< ","
	   << ((double)totalDurations[iter.first]/(double)(iter.second)) / 1e06 << ","
	   << std::endl ;
    }
  }

  void OpenCLSummaryWriter::writeGuidance()
  {
    // Caption
    fout << "Guidance Parameters" << std::endl ;
    
    // Columns
    fout << "Parameter" << ","
	 << "Element"   << ","
	 << "Value"     << "," << std::endl ;

    for (auto rule : guidanceRules)
    {
      rule(this) ;
    } 
  }

  void OpenCLSummaryWriter::write(bool openNewFile)
  {
    writeHeader() ;                                 fout << std::endl ;
    writeAPICallSummary() ;                         fout << std::endl ;
    writeKernelExecutionSummary() ;                 fout << std::endl ;
    if (getFlowMode() == SW_EMU)
    {
      writeSoftwareEmulationComputeUnitUtilization() ; fout << std::endl ;
    }
    if ((db->getStaticInfo()).getNumDevices() > 0)
    {
      writeComputeUnitUtilization() ;                 fout << std::endl ;
      writeComputeUnitStallInformation() ;            fout << std::endl ;
      writeDataTransferHostToGlobalMemory() ;         fout << std::endl ;
      writeDataTransferKernelsToGlobalMemory() ;      fout << std::endl ;
      writeStreamDataTransfers() ;                    fout << std::endl ;
      writeDataTransferDMA() ;                        fout << std::endl ;
      writeDataTransferDMABypass() ;                  fout << std::endl ;
      writeDataTransferGlobalMemoryToGlobalMemory() ; fout << std::endl ;
      writeTopDataTransferKernelAndGlobal() ;         fout << std::endl ;
    }
    writeTopKernelExecution() ;                       fout << std::endl ;
    writeTopMemoryWrites() ;                          fout << std::endl ;
    writeTopMemoryReads() ;                           fout << std::endl ;
    writeUserLevelEvents() ;                          fout << std::endl ;
    writeUserLevelRanges() ;                          fout << std::endl ;
    writeGuidance() ;

    if (openNewFile)
    {
      switchFiles() ;
    }
  }

  void OpenCLSummaryWriter::guidanceDeviceExecTime(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos) {
      std::string deviceName = device->deviceName ;
      uint64_t execTime = (t->db->getStats()).getDeviceActiveTime(deviceName) ;
      
      (t->fout) << "DEVICE_EXEC_TIME" << "," 
		<< deviceName << ","
		<< ((double)execTime / 1e06)  << "," << std::endl ;
    }

    if (getFlowMode() == SW_EMU) {
      std::string deviceName =
	(t->db->getStaticInfo()).getSoftwareEmulationDeviceName() ;
      uint64_t execTime = (t->db->getStats()).getDeviceActiveTime(deviceName) ;
      (t->fout) << "DEVICE_EXEC_TIME"        << ","
		<< deviceName                << ","
		<< ((double)execTime / 1e06) << "," << std::endl ;
    }
  }

  void OpenCLSummaryWriter::guidanceCUCalls(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
 
    for (auto device : deviceInfos) {
      for (auto xclbin : device->loadedXclbins) {
        for (auto cu : xclbin->cus) {
          std::string cuName = (cu.second)->getName();
          std::vector<std::pair<std::string, TimeStatistics>> cuCalls = 
                               (t->db->getStats()).getComputeUnitExecutionStats(cuName);

          uint64_t execCount = 0;
          for(auto cuCall : cuCalls) {
            execCount += cuCall.second.numExecutions;
          }

          (t->fout) << "CU_CALLS" << ","
                    << device->deviceName << "|"
                    << ((cu.second)->getName()) << ","
                    << execCount
                    << "," << std::endl ;
        }
      }
    }

    if (getFlowMode() == SW_EMU) {
      std::map<std::tuple<std::string, std::string, std::string>,
	       TimeStatistics> cuStats =
	(t->db->getStats()).getComputeUnitExecutionStats() ;

      for (auto iter : cuStats) {
	(t->fout) << "CU_CALLS" << ","
		  << (t->db->getStaticInfo()).getSoftwareEmulationDeviceName()
		  << "|" 
		  << std::get<0>(iter.first) << "," 
		  << iter.second.numExecutions << "," << std::endl ;
      }
    }
  }

  void OpenCLSummaryWriter::guidanceNumMonitors(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos();
    struct MonInfo {
      std::string type;
      uint64_t numTotal;
      uint64_t numTraceEnabled;

      MonInfo(std::string s, uint64_t total, uint64_t trace)
        : type(s), numTotal(total), numTraceEnabled(trace)
      {}
      ~MonInfo() {}
    };

    std::map<uint8_t, MonInfo*> monitors;

    monitors[ACCEL_MONITOR]      = new MonInfo("XCL_PERF_MON_ACCEL", 0, 0);
    monitors[AXI_MM_MONITOR]     = new MonInfo("XCL_PERF_MON_MEMORY", 0, 0);
    monitors[AXI_STREAM_MONITOR] = new MonInfo("XCL_PERF_MON_STR", 0, 0);

    for (auto device : deviceInfos)
    {
      for (auto xclbin : device->loadedXclbins)
      {

        monitors[ACCEL_MONITOR]->numTotal += xclbin->amList.size();
        monitors[ACCEL_MONITOR]->numTraceEnabled += xclbin->amMap.size();

        monitors[AXI_MM_MONITOR]->numTotal += xclbin->aimList.size();
        monitors[AXI_MM_MONITOR]->numTraceEnabled += xclbin->aimMap.size();

        monitors[AXI_STREAM_MONITOR]->numTotal += xclbin->asmList.size();
        monitors[AXI_STREAM_MONITOR]->numTraceEnabled += xclbin->asmMap.size();
        
      }

      for (auto mon : monitors)
      {
        (t->fout) << "NUM_MONITORS" << ","
                  << device->deviceName << "|"
                  << (mon.second)->type << "|"
                  << (mon.second)->numTraceEnabled << ","
                  << (mon.second)->numTotal << "," << std::endl;

        // Reset the numbers
        (mon.second)->numTotal        = 0;
        (mon.second)->numTraceEnabled = 0;
      }
    }
    for(auto mon : monitors) {
      delete mon.second;
    }
    monitors.clear(); 
  }

  void OpenCLSummaryWriter::guidanceMigrateMem(OpenCLSummaryWriter* t)
  {
    uint64_t numCalls = (t->db->getStats()).getNumMigrateMemCalls();

    (t->fout) << "MIGRATE_MEM" << ","
	      << "host" << ","
	      << numCalls << "," << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceMemoryUsage(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      for (auto xclbin : device->loadedXclbins)
      {
	for (auto memory : xclbin->memoryInfo)
	{
	  std::string memName = (memory.second)->name ;
	  if (memName.rfind("bank", 0) == 0)
	    memName = "DDR[" + memName.substr(4,4) + "]" ;

	  (t->fout) << "MEMORY_USAGE" << ","
		    << (device->deviceName) << "|"
		    << memName
		    << ","
		    << (memory.second)->used
		    << "," << std::endl ;
	}
      }
    }

    if (getFlowMode() == SW_EMU) {
      std::map<std::string, bool> memUsage =
	(t->db->getStaticInfo()).getSoftwareEmulationMemUsage() ;
      for (auto iter : memUsage) {
	(t->fout) << "MEMORY_USAGE" << ","
		  << iter.first     << ","
		  << iter.second    << "," << std::endl ;
      }
    }
  }

  void OpenCLSummaryWriter::guidancePLRAMDevice(OpenCLSummaryWriter* t)
  {
    bool hasPLRAM = false ;
    
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos) {
      for (auto xclbin : device->loadedXclbins) {
        for (auto memory : xclbin->memoryInfo) {
          if((memory.second)->name.find("PLRAM") != std::string::npos) {
            hasPLRAM = true ;
            break ;
	      }
        }
        if (hasPLRAM) break ;
      }
      if (hasPLRAM) break ;
    }

    // Today, all devices in SW emulation are assumed to have PLRAM
    if (getFlowMode() == SW_EMU) hasPLRAM = true ;

    (t->fout) << "PLRAM_DEVICE" << ","
              << "all" << ","
              << (uint64_t)(hasPLRAM) << "," << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceHBMDevice(OpenCLSummaryWriter* t)
  {
    bool hasHBM = false ;
    
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos) {
      for (auto xclbin : device->loadedXclbins) {
        for (auto memory : xclbin->memoryInfo) {
          if((memory.second)->name.find("HBM") != std::string::npos) {
            hasHBM = true ;
            break ;
	      }
        }
        if (hasHBM) break ;
      }
      if (hasHBM) break ;
    }

    // To match backward compatability, we will have to check the name
    if (getFlowMode() == SW_EMU) {
      std::string deviceName =
	(t->db->getStaticInfo()).getSoftwareEmulationDeviceName() ;

      if (deviceName.find("u280") != std::string::npos ||
          deviceName.find("u50") != std::string::npos)
        hasHBM = true ;
    }

    (t->fout) << "HBM_DEVICE" << ","
              << "all" << ","
              << (uint64_t)(hasHBM) << "," << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceKDMADevice(OpenCLSummaryWriter* t)
  {
    bool hasKDMA = false ;
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos) {
      std::string deviceName = device->deviceName;
      if(deviceName.find("xilinx_u200_xdma_201830_2") != std::string::npos
         || deviceName.find("xilinx_u200_xdma_201830_3") != std::string::npos
         || deviceName.find("xilinx_vcu1525_xdma_201830_2") != std::string::npos) {
        hasKDMA = true;
        break;
      }
    }

    // To match backward compatability, we will have to check the name
    if (getFlowMode() == SW_EMU) {
      std::string deviceName =
           (t->db->getStaticInfo()).getSoftwareEmulationDeviceName() ;

      if (deviceName.find("xilinx_u200_xdma_201830_2") != std::string::npos ||
          deviceName.find("xilinx_u200_xdma_201830_3") != std::string::npos ||
          deviceName.find("xilinx_vcu1525_xdma_201830_2") != std::string::npos)
        hasKDMA = true ;
    }

    (t->fout) << "KDMA_DEVICE" << ","
              << "all" << ","
              << (uint64_t)(hasKDMA) << "," << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceP2PDevice(OpenCLSummaryWriter* t)
  {
    // Currently, we have a hard-coded list of platforms that we know
    //  have the P2P functionality.  This will have to be improved
    //  and stored as a property in the static info database
    bool hasP2P = false ;

    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      std::string deviceName = device->deviceName ;
      if (deviceName.find("xilinx_u200_xdma_201830_2") != std::string::npos ||
	  deviceName.find("xilinx_u200_xdma_201830_3") != std::string::npos ||
	  deviceName.find("xilinx_u250_xdma_201830_2") != std::string::npos ||
	  deviceName.find("samsung")                   != std::string::npos ||
	  deviceName.find("xilinx_vcu1525_xdma_201830_2") != std::string::npos)
      {
	hasP2P = true ;
	break ;
      }
    }

    if (getFlowMode() == SW_EMU) {
      std::string deviceName =
	(t->db->getStaticInfo()).getSoftwareEmulationDeviceName() ;
      if (deviceName.find("xilinx_u200_xdma_201830_2") != std::string::npos ||
	  deviceName.find("xilinx_u200_xdma_201830_3") != std::string::npos ||
	  deviceName.find("xilinx_u250_xdma_201830_2") != std::string::npos ||
	  deviceName.find("samsung")                   != std::string::npos ||
	  deviceName.find("xilinx_vcu1525_xdma_201830_2") != std::string::npos)
	hasP2P = true ;
    }

    (t->fout) << "P2P_DEVICE" << ","
	      << "all" << ","
	      << (uint64_t)(hasP2P) << "," << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceP2PHostTransfers(OpenCLSummaryWriter* t)
  {
    uint64_t hostP2PTransfers = (t->db->getStats()).getNumHostP2PTransfers() ;

    (t->fout) << "P2P_HOST_TRANSFERS" << ","
	      << "host" << ","
	      << hostP2PTransfers << "," << std::endl ;
  }

  void OpenCLSummaryWriter::guidancePortBitWidth(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
    
    for (auto device : deviceInfos) {
      for (auto xclbin : device->loadedXclbins) {
	for (auto cu : xclbin->cus) {
	  std::vector<uint32_t>* aimIds = (cu.second)->getAIMs() ;
	  std::vector<uint32_t>* asmIds = (cu.second)->getASMs() ;

	  for (auto aim : (*aimIds)) {
	    Monitor* monitor = (t->db->getStaticInfo()).getAIMonitor(device->deviceId, xclbin, aim) ;
	    (t->fout) << "PORT_BIT_WIDTH" << ","
		      << (cu.second)->getName() << "/" << monitor->port << ","
		      << monitor->portWidth << "," << std::endl ;
	  }

	  for (auto asmId : (*asmIds)) {
	    Monitor* monitor = (t->db->getStaticInfo()).getASMonitor(device->deviceId, xclbin, asmId) ;
	    (t->fout) << "PORT_BIT_WIDTH" << ","
		      << (cu.second)->getName() << "/" << monitor->port << ","
		      << monitor->portWidth << "," << std::endl ;
	  }
	}
      }
    }
    if (getFlowMode() == SW_EMU) {
      std::vector<std::string> portBitWidths =
	(t->db->getStaticInfo()).getSoftwareEmulationPortBitWidths() ;
      for (auto width : portBitWidths) {
	(t->fout) << "PORT_BIT_WIDTH" << "," << width << "," << std::endl ;
      }
    }
  }

  void OpenCLSummaryWriter::guidanceKernelCount(OpenCLSummaryWriter* t)
  {
    // This guidance rule is actually stating how many compute units
    //  on each device correspond to kernels

    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
    std::map<std::string, uint64_t> kernelCounts ;

    if (getFlowMode() == SW_EMU) {
      kernelCounts = (t->db->getStaticInfo()).getSoftwareEmulationCUCounts() ;
    }
    else {
      for (auto device : deviceInfos) {
	for (auto xclbin : device->loadedXclbins) {
	  for (auto cu : xclbin->cus) {
	    if (kernelCounts.find((cu.second)->getKernelName()) == kernelCounts.end()) {
	      kernelCounts[(cu.second)->getKernelName()] = 1 ;
	    }
	    else {
	      kernelCounts[(cu.second)->getKernelName()] += 1 ;
	    }
	  }
	}
      }
    }

    for (auto kernel : kernelCounts)
    {
      (t->fout) << "KERNEL_COUNT" << ","
		<< (kernel.first) << "," 
		<< (kernel.second) << "," << std::endl ;
    }
  }

  void OpenCLSummaryWriter::guidanceObjectsReleased(OpenCLSummaryWriter* t)
  {
    uint64_t numReleased = (t->db->getStats()).getNumOpenCLObjectsReleased() ;

    (t->fout) << "OBJECTS_RELEASED" << ","
	      << "all" << ","
	      << numReleased << "," 
	      << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceCUContextEn(OpenCLSummaryWriter* t)
  {
    bool isContextEnabled = (t->db->getStats()).getContextEnabled() ;

    (t->fout) << "CU_CONTEXT_EN" << ","
	      << "all" << ","
	      << (uint64_t)(isContextEnabled) << ","
	      << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceTraceMemory(OpenCLSummaryWriter* t)
  {
    std::string memType = "FIFO" ;

    if(HW_EMU == getFlowMode() || SW_EMU == getFlowMode()) {
      memType = "NA";
    } else {
      auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
 
      for (auto device : deviceInfos) {
        for (auto xclbin : device->loadedXclbins) {
          if (xclbin->usesTs2mm)
          {
            memType = "TS2MM" ;
            break ;
          }
        }
      }
    }

    (t->fout) << "TRACE_MEMORY" << ","
	      << "all" << ","
	      << memType << "," << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceMaxParallelKernelEnqueues(OpenCLSummaryWriter* t)
  {
    auto maxExecs = (t->db->getStats()).getAllMaxExecutions();

    for(auto mExec : maxExecs) {
      (t->fout) << "MAX_PARALLEL_KERNEL_ENQUEUES" << ","
                << mExec.first  << ","
                << mExec.second << "," << std::endl;
    }
  }

  void OpenCLSummaryWriter::guidanceCommandQueueOOO(OpenCLSummaryWriter* t)
  {
    auto commandQueueInfo = (t->db->getStats()).getCommandQueuesAreOOO() ;

    for (auto cq : commandQueueInfo)
    {
      (t->fout) << "COMMAND_QUEUE_OOO" << "," 
		<< cq.first << "," 
		<< cq.second << "," << std::endl ;
    }
  }

  void OpenCLSummaryWriter::guidancePLRAMSizeBytes(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
    bool done = false;

    for (auto device : deviceInfos) {
      std::string deviceName = device->deviceName ;
      for (auto xclbin : device->loadedXclbins) {
        for (auto memory : xclbin->memoryInfo) {
          if ((memory.second)->name.find("PLRAM") != std::string::npos) {
            (t->fout) << "PLRAM_SIZE_BYTES," 
                      << deviceName << ","
                      << (memory.second)->size*1024 << "," << std::endl;
             done = true;
             /* To match old flow iand tools, print PLRAM_SIZE_BYTES for the first match only */
             break;
          }
        }
        if(done) break;
      }
      if(done) break;
    }
  }

  void OpenCLSummaryWriter::guidanceKernelBufferInfo(OpenCLSummaryWriter* t)
  {
    // This reports the memory bank, argument, alignment, and size of 
    //  each buffer.

    for (auto& iter : (t->db->getStats()).getBufferInfo()) {
      for (auto& info : iter.second) {
	(t->fout) << "KERNEL_BUFFER_INFO" << "," << info << "," << std::endl ;
      }
    }
  }

  void OpenCLSummaryWriter::guidanceTraceBufferFull(OpenCLSummaryWriter* /*t*/)
  {
    // TODO
    // This has a race condition.  If we are dumping profile summary 
    //  before trace, then we will not see this guidance rule.
    /*
    (t->fout) << "TRACE_BUFFER_FULL" << ","
	      << std::endl ;
    */
  }

  void OpenCLSummaryWriter::guidanceMemoryTypeBitWidth(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      if (device->isEdgeDevice)
      {
	(t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		  << device->deviceName << "|DDR" << ","
		  << 64 << "," << std::endl ;
      }
      else
      {
	(t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		  << device->deviceName << "|HBM" << ","
		  << 256 << "," << std::endl ;
	(t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		  << device->deviceName << "|DDR" << ","
		  << 512 << "," << std::endl ;
	(t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		  << device->deviceName << "|PLRAM" << ","
		  << 512 << "," << std::endl ;	  
      }
    }

    if (getFlowMode() == SW_EMU) {
      std::string deviceName =
	(t->db->getStaticInfo()).getSoftwareEmulationDeviceName() ;
      (t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		<< deviceName << "|HBM" << ","
		<< 256 << "," << std::endl ;
      (t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		<< deviceName << "|DDR" << ","
		<< 512 << "," << std::endl ;
      (t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		<< deviceName << "|PLRAM" << ","
		<< 512 << "," << std::endl ;	  
    }
  }

  void OpenCLSummaryWriter::guidanceXrtIniSetting(OpenCLSummaryWriter* t)
  {
    for (auto setting : t->iniSettings)
    {
      (t->fout) << setting << "," << std::endl ;
    }
  }

  void OpenCLSummaryWriter::guidanceBufferRdActiveTimeMs(OpenCLSummaryWriter* t)
  {
    (t->fout) << "BUFFER_RD_ACTIVE_TIME_MS" << ","
	      << "all" << ","
	      << (double)((t->db->getStats()).getTotalHostReadTime()) / 1e06
	      << ","
	      << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceBufferWrActiveTimeMs(OpenCLSummaryWriter* t)
  {
    (t->fout) << "BUFFER_WR_ACTIVE_TIME_MS" << ","
	      << "all" << ","
	      << ((double)((t->db->getStats()).getTotalHostWriteTime())) / 1e06
	      << ","
	      << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceBufferTxActiveTimeMs(OpenCLSummaryWriter* t)
  {
    (t->fout) << "BUFFER_TX_ACTIVE_TIME_MS" << ","
	      << "all" << ","
	      << ((double)(t->db->getStats()).getTotalBufferTxTime()) / 1e06
	      << ","
	      << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceApplicationRunTimeMs(OpenCLSummaryWriter* t)
  {
    uint64_t startTime = (t->db->getStaticInfo()).getApplicationStartTime() ;
    uint64_t endTime = xrt_core::time_ns() ;
    
    (t->fout) << "APPLICATION_RUN_TIME_MS" << ","
	      << "all" << ","
	      << ((double)(endTime - startTime) / 1.0e6)
	      << ","
	      << std::endl ;
    
  }

  void OpenCLSummaryWriter::guidanceTotalKernelRunTimeMs(OpenCLSummaryWriter* t)
  {
    double firstKernelStartTime =
      (t->db->getStats()).getFirstKernelStartTime() ;
    double lastKernelEndTime =
      (t->db->getStats()).getLastKernelEndTime() ;

    (t->fout) << "TOTAL_KERNEL_RUN_TIME_MS" << ","
	      << "all" << ","
	      << (lastKernelEndTime - firstKernelStartTime)
	      << ","
	      << std::endl ;
  }

} // end namespace xdp
