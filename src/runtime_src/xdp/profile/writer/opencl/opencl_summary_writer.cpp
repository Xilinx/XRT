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
      setting << "XRT_INI_SETTING,timeline_trace," 
	      << xrt_core::config::get_timeline_trace() ;
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
      setting << "XRT_INI_SETTING,launch_waveform,"
	      << xrt_core::config::get_launch_waveform() ;
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
    for (unsigned int i = 0 ; i < deviceNames.size() ; ++i)
    {
      if (i != 0) fout << ", " ;
      fout << deviceNames[i] ;
    }
    fout << std::endl ;

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
	 << "Maximum Time (ms)" << std::endl ;
    
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
	   << (std::get<3>(row.second)/1e06) << std::endl ; // Maximum time
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
	 << "Maximum Time (ms)"  << std::endl ;

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
	   << ((execution.second).maxTime / 1e06)     << std::endl ;
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
	 << "Clock Frequency (MHz)"      << std::endl ;

    std::map<std::tuple<std::string, std::string, std::string>,
	     TimeStatistics> cuStats = 
      (db->getStats()).getComputeUnitExecutionStats() ;

    for (auto stat : cuStats)
    {
      std::string cuName          = (std::get<0>(stat.first)) ;
      std::string localWorkGroup  = (std::get<1>(stat.first)) ;
      std::string globalWorkGroup = (std::get<2>(stat.first)) ;

      double averageTime = (stat.second).averageTime ;
      double totalTime   = (stat.second).totalTime ;
      double minTime     = (stat.second).minTime ;
      double maxTime     = (stat.second).maxTime ;
      uint64_t execCount = (stat.second).numExecutions ;

      fout << "deviceName"                        << "," // TODO
	   << cuName                              << ","
	   << "kernelName"                        << "," // TODO
	   << globalWorkGroup                     << "," 
	   << localWorkGroup                      << ","
	   << execCount                           << ","
	   << "No"                                << ","
	   << 1                                   << "," // TODO?
	   << ((averageTime*execCount)/totalTime) << ","
	   << (totalTime / 1e06)                  << ","
	   << (minTime / 1e06)                    << ","
	   << (averageTime / 1e06)                << ","
	   << (maxTime / 1e06)                    << ","
	   << 300                                 << std::endl ; // TODO?
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
	 << "Clock Frequency (MHz)"      << std::endl ;

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
      xclCounterResults values =
	(db->getDynamicInfo()).getCounterResults(deviceId) ;

      // For every compute unit in the device
      for (auto cuInfo : device->cus)
      {
	// This info is the same for every execution call
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

	uint64_t cuIndex = 0 ;

	for (auto cuCall : cuCalls)
	{
	  std::string globalWorkDimensions = cuCall.first ;
	  double averageTime = (cuCall.second).averageTime ;
	  double totalTime = (cuCall.second).totalTime ;
	  double minTime = (cuCall.second).minTime ;
	  double maxTime = (cuCall.second).maxTime ;

	  fout << (device->platformInfo.deviceName) << "," 
	       << cuName << ","
	       << kernelName << ","
	       << globalWorkDimensions << ","
	       << cuLocalDimensions << ","
	       << values.CuExecCount[cuIndex] << ","
	       << dataflowEnabled << ","
	       << values.CuMaxParallelIter[cuIndex] << ","
	       << ((averageTime*(values.CuExecCount[cuIndex]))/totalTime) << ","
	       << (totalTime / 1e06) << ","
	       << (minTime / 1e06) << ","
	       << (averageTime /1e06) << ","
	       << (maxTime / 1e06) << "," 
	       << (device->clockRateMHz) << std::endl ;
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
	 << "Inter-Kernel Pipe Stalls (ms)"     << std::endl ;

    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    uint64_t i = 0 ;
    for (auto device : infos)
    {
      xclCounterResults values = (db->getDynamicInfo()).getCounterResults(i) ;

      uint64_t j = 0 ;
      for (auto cu : (device->cus))
      {
	fout << (cu.second)->getName()     << "," 
	     << values.CuExecCount[j]      << ","
	     << values.CuExecCycles[j]     << ","
	     << values.CuStallIntCycles[j] << ","
	     << values.CuStallExtCycles[j] << ","
	     << values.CuStallStrCycles[j] << std::endl ;
	++j ;
      }
      ++i ;
    }
  }

  void OpenCLSummaryWriter::writeDataTransferHostToGlobalMemory()
  {
    // Caption
    fout << "Data Transfer: Host to Global Memory" << std::endl ;

    // Column headers
    fout << "Context: Number of Devices"        << ","
	 << "Transfer Type"                     << ","
	 << "Number Of Buffer Transfers"        << ","
	 << "Transfer Rate (MB/s)"              << ","
	 << "Average Bandwidth Utilization (%)" << ","
	 << "Average Buffer Size (KB)"          << ","
	 << "Total Time (ms)"                   << ","
	 << "Average Time (ms)"                 << std::endl ;

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
	     << "N/A" << std::endl ;
      }
      else
      {
	double transferRate = 
	  (double)((read.second).totalSize)/(double)((read.second).totalTime) ;
	double maxReadBW =
	  (db->getStaticInfo()).getMaxReadBW(read.first.second) ;
	double aveBWUtil = (100.0 * transferRate) / maxReadBW ;

	fout << contextName << ":" << numDevices << ","
	     << "READ" << ","
	     << (read.second).count << ","
	     << transferRate << ","
	     << aveBWUtil << ","
	     << ((double)((read.second).averageSize) / 1000.0) << ","
	     << (read.second).totalTime << ","
	     << (read.second).averageTime << std::endl ;
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
	     << "N/A" << std::endl ;
      }
      else
      {
	double transferRate = 
	  (double)((write.second).totalSize)/(double)((write.second).totalTime);
	double maxWriteBW =
	  (db->getStaticInfo()).getMaxWriteBW(write.first.second);
	double aveBWUtil = (100.0 * transferRate) / maxWriteBW ;

	fout << contextName << ":" << numDevices << "," 
	     << "WRITE" << ","
	     << (write.second).count << ","
	     << transferRate << ","
	     << aveBWUtil << ","
	     << ((double)((write.second).averageSize) / 1000.0) << ","
	     << (write.second).totalTime << ","
	     << (write.second).averageTime << std::endl ;
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
	 << "Transfer Rate (MB)"                << ","
	 << "Average Bandwidth Utilization (%)" << ","
	 << "Average Size (KB)"                 << ","
	 << "Average Latency (ns)"              << std::endl ;

    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    uint64_t i = 0 ;
    for (auto device : infos)
    {
      xclCounterResults values = (db->getDynamicInfo()).getCounterResults(i) ;

      for (auto cu : (device->cus))
      {
	std::vector<Monitor*> monitors = (cu.second)->getMonitors() ;

	uint64_t AIMIndex = 0 ;
	for (auto monitor : monitors)
	{
	  if (monitor->type != AXI_MM_MONITOR) continue ;

	  auto writeTranx = values.WriteTranx[AIMIndex] ;
	  auto readTranx = values.ReadTranx[AIMIndex] ;

	  uint64_t totalReadBusyCycles = values.ReadBusyCycles[AIMIndex] ;
	  uint64_t totalWriteBusyCycles = values.WriteBusyCycles[AIMIndex] ;

	  double totalReadTime = 
	    (double)(totalReadBusyCycles) / (1000.0 * device->clockRateMHz) ;
	  double totalWriteTime =
	    (double)(totalWriteBusyCycles) / (1000.0 * device->clockRateMHz) ;

	  // Use the name of the monitor to determine the port and memory
	  std::string portName = "" ;
	  std::string memoryName = "" ;

	  size_t slashPosition = (monitor->name).find("/") ;
	  if (slashPosition != std::string::npos)
	  {
	    auto position = slashPosition + 1 ;
	    auto length = (monitor->name).size() - position ;

	    // Split the monitor name into port and memory position
	    std::string lastHalf = (monitor->name).substr(position, length) ;

	    size_t dashPosition = lastHalf.find("-") ;
	    if (dashPosition != std::string::npos)
	    {
	      auto remainingLength = lastHalf.size() - dashPosition - 1 ;
	      portName = lastHalf.substr(0, dashPosition) ;
	      memoryName = lastHalf.substr(dashPosition + 1, remainingLength);
	    }
	    else
	    {
	      portName = lastHalf ;
	    }
	  }

	  if (writeTranx > 0)
	  {
	    double transferRate = (totalWriteTime == 0.0) ? 0 :
	      (double)(values.WriteBytes[AIMIndex]) / (1000.0 * totalWriteTime);
	    double aveBW =
	      (100.0 * transferRate) / device->maxWriteBW ;
	    if (aveBW > 100.0) aveBW = 100.0 ;

	    fout << (device->platformInfo.deviceName) << ","
		 << (cu.second)->getName() << ":" << portName << ","
		 << (monitor->args) << ","
		 << memoryName << ","
		 << "WRITE" << ","
		 << writeTranx << ","
		 << transferRate << ","
		 << aveBW << ","
		 << (values.WriteBytes[AIMIndex] / writeTranx) << ","
		 << (values.WriteLatency[AIMIndex] / writeTranx) << "," 
		 << std::endl ;
	  }
	  if (readTranx > 0)
	  {
	    double transferRate = (totalReadTime == 0.0) ? 0 :
	      (double)(values.ReadBytes[AIMIndex]) / (1000.0 * totalReadTime);
	    double aveBW =
	      (100.0 * transferRate) / device->maxReadBW ;
	    if (aveBW > 100.0) aveBW = 100.0 ;

	    fout << (device->platformInfo.deviceName) << ","
		 << (cu.second)->getName() << ":" << portName << ","
		 << (monitor->args) << ","
		 << memoryName << ","
		 << "READ" << ","
		 << readTranx << ","
		 << transferRate << ","
		 << aveBW << ","
		 << (values.ReadBytes[AIMIndex] / readTranx) << ","
		 << (values.ReadLatency[AIMIndex] / readTranx) << "," 
		 << std::endl ;
	  }

	  ++AIMIndex ;
	}
      }
      ++i ;
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
	 << "Link Stall (%)"          << std::endl ;

    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    uint64_t i = 0 ;
    for (auto device : infos)
    {
      xclCounterResults values = (db->getDynamicInfo()).getCounterResults(i) ;

      for (auto cu : (device->cus))
      {
	std::vector<Monitor*> monitors = (cu.second)->getMonitors() ;

	uint64_t ASMIndex = 0 ;
	for (auto monitor : monitors)
	{
	  if (monitor->type != AXI_STREAM_MONITOR) continue ;

	  uint64_t numTranx = values.StrNumTranx[ASMIndex] ;

	  std::string masterPort = "" ;
	  std::string slavePort = "" ;
	  std::string masterArgs = "" ;
	  std::string slaveArgs = "" ;

	  size_t dashPosition = (monitor->name).find("-") ;
	  if (dashPosition != std::string::npos)
	  {
	    std::string firstHalf = (monitor->name).substr(0, dashPosition);
	    std::string secondHalf =
	      (monitor->name).substr(dashPosition + 1,
				     (monitor->name).size()-dashPosition-1) ;
	    size_t slashPosition = firstHalf.find("/") ;
	    masterPort = firstHalf.substr(0, slashPosition) ;
	    masterArgs = firstHalf.substr(slashPosition + 1,
					  firstHalf.size()-slashPosition-1) ;

	    slashPosition = secondHalf.find("/") ;
	    slavePort = secondHalf.substr(0, slashPosition) ;
	    slaveArgs = secondHalf.substr(slashPosition + 1,
					  secondHalf.size()-slashPosition-1) ;
	  }

	  double transferTime =
	    values.StrBusyCycles[ASMIndex] / device->clockRateMHz ;
	  double transferRate = (transferTime == 0.0) ? 0 :
	    values.StrDataBytes[ASMIndex] / transferTime ;

	  double linkStarve =
	    (double)(values.StrStarveCycles[ASMIndex]) / (double)(values.StrBusyCycles[ASMIndex]) * 100.0 ;
	  double linkStall =
	    (double)(values.StrStallCycles[ASMIndex]) / (double)(values.StrBusyCycles[ASMIndex]) * 100.0 ;
	  double linkUtil = 100.0 - linkStarve - linkStall ;

	  fout << (device->platformInfo.deviceName) << ","
	       << masterPort << ","
	       << masterArgs << ","
	       << slavePort << ","
	       << slaveArgs << ","
	       << numTranx << ","
	       << transferRate << ","
	       << (values.StrDataBytes[ASMIndex] / numTranx) << ","
	       << linkUtil << "," 
	       << (values.StrStarveCycles[ASMIndex]) << ","
	       << (values.StrStallCycles[ASMIndex])
	       << std::endl ;

	  ++ASMIndex ;
	}
      }
    }
  }

  void OpenCLSummaryWriter::writeDataTransferDMA()
  {
    // For all devices, if (deviceIntf->getNumMonitors(XCL_PERF_MON_SHELL) ==0)
    //  return

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
	 << "Average Latency (ns)"     << std::endl ;

    // TODO
  }

  void OpenCLSummaryWriter::writeDataTransferDMABypass()
  {
    // For all devices, if (deviceIntf->getNumMonitors(XCL_PERF_MON_SHELL) ==0)
    //  return

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
	 << "Average Latency (ns)"     << std::endl ;

    // TODO
  }

  void OpenCLSummaryWriter::writeDataTransferGlobalMemoryToGlobalMemory()
  {
    // For all devices, if (deviceIntf->getNumMonitors(XCL_PERF_MON_SHELL) ==0)
    //  return

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
	 << "Average Latency (ns)"     << std::endl ;

    // TODO
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
	 << "Total Transfer Rate (MB/s)" << std::endl ;

    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    uint64_t i = 0 ;
    for (auto device : infos)
    {
      // For this device, find the monitor that has the most total number of
      //  read/write transactions
      xclCounterResults values = (db->getDynamicInfo()).getCounterResults(i) ;

      std::string computeUnitName = "" ;
      uint64_t numTransfers = 0 ;
      double aveBytesPerTransfer = 0 ;
      double transferEfficiency = 0 ;
      uint64_t totalDataTransfer = 0 ;
      uint64_t totalWrite = 0 ;
      uint64_t totalRead = 0 ;
      double totalTransferRate = 0 ;

      uint64_t maxNumTranx = 0 ;
      for (auto cu : device->cus)
      {
	std::vector<Monitor*> monitors = (cu.second)->getMonitors() ;

	uint64_t totalTranx = 0 ;
	uint64_t totalReadBytes = 0 ;
	uint64_t totalWriteBytes = 0 ;
	uint64_t totalBusyCycles = 0 ;

	uint64_t AIMIndex = 0 ;
	for (auto monitor : monitors)
	{
	  if (monitor->type != AXI_MM_MONITOR) continue ;

	  auto writeTranx = values.WriteTranx[AIMIndex] ;
	  auto readTranx = values.ReadTranx[AIMIndex] ;
	  
	  totalTranx += writeTranx + readTranx ;
	  totalReadBytes += values.ReadBytes[AIMIndex] ;
	  totalWriteBytes += values.WriteBytes[AIMIndex] ;

	  totalBusyCycles += values.ReadBusyCycles[AIMIndex] ;
	  totalBusyCycles += values.WriteBusyCycles[AIMIndex] ;

	  ++AIMIndex ;
	}

	if (totalTranx > maxNumTranx)
	{
	  // For now, this CU has the most transactions that we monitored,
	  //  so update our information
	  computeUnitName = (cu.second)->getName() ;
	  numTransfers = totalTranx ;
	  aveBytesPerTransfer =
	    (double)(totalReadBytes + totalWriteBytes)/(double)(numTransfers) ;
	  // TODO: Fix bit width calculation here
	  transferEfficiency = (100.0 * aveBytesPerTransfer) / 4096 ; 
	  totalDataTransfer = totalReadBytes + totalWriteBytes ;
	  totalRead = totalReadBytes ;
	  totalWrite = totalWriteBytes ;	  
	  double totalTimeMSec = 
	    (double)(totalBusyCycles) /(1000.0 * device->clockRateMHz) ;
	  totalTransferRate =
	    (totalTimeMSec == 0) ? 0.0 :
	    (double)(totalDataTransfer) / (1000.0 * totalTimeMSec) ;
	}
      }
      
      fout << device->platformInfo.deviceName << ","
	   << computeUnitName << "," 
	   << numTransfers << ","
	   << aveBytesPerTransfer << ","
	   << transferEfficiency << ","
	   << (double)(totalDataTransfer) / 1.0e6 << ","
	   << (double)(totalWrite) / 1.0e6 << ","
	   << (double)(totalRead) / 1.0e6 << ","
	   << totalTransferRate << std::endl ;

      ++i ;
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
	 << "Local Work Size"         << std::endl ;

    // TODO
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
	 << "Writing Rate(MB/s)" << std::endl ;

    // TODO
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
	 << "Reading Rate(MB/s)" << std::endl ;

    // TODO
  }

  void OpenCLSummaryWriter::writeGuidance()
  {
    // Caption
    fout << "Guidance Parameters" << std::endl ;
    
    // Columns
    fout << "Parameter" << ","
	 << "Element"   << ","
	 << "Value"     << std::endl ;

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
    writeGuidance() ;

    if (openNewFile)
    {
      switchFiles() ;
    }
  }

  void OpenCLSummaryWriter::guidanceDeviceExecTime(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      (t->fout) << "DEVICE_EXEC_TIME" << "," 
		<< (device->platformInfo).deviceName << ","
		<< 0 << std::endl ; // TODO - Total device execution time
    }
  }

  void OpenCLSummaryWriter::guidanceCUCalls(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
    
    for (auto device : deviceInfos)
    {
      for (auto cu : device->cus)
      {
	(t->fout) << "CU_CALLS" << ","
		  << ((cu.second)->getName()) << ","
		  << 0 // TODO: Execution count
		  << std::endl ;
      }
    }
  }

  void OpenCLSummaryWriter::guidanceNumMonitors(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
    std::map<uint8_t, uint64_t> accelCounter ;

    for (auto device : deviceInfos)
    {
      // Collect the number of the different types of monitors
      for (auto cu : device->cus)
      {
	for (auto monitor : (cu.second)->getMonitors())
	{
	  accelCounter[monitor->type] += 1 ;
	}
      }

      for (auto numCounters : accelCounter)
      {
	(t->fout) << "NUM_MONITORS" << ","
		  << device->platformInfo.deviceName 
		  << "|"
		  << (uint64_t)(numCounters.first) << "," // TODO: Type
		  << (numCounters.second) << std::endl ;
      }
    }
  }

  void OpenCLSummaryWriter::guidanceMigrateMem(OpenCLSummaryWriter* t)
  {
    uint64_t numCalls = (t->db->getStats()).getNumMigrateMemCalls();

    (t->fout) << "MIGRATE_MEM" << ","
	      << "host" << ","
	      << numCalls << std::endl ; // TODO: Make the connection
  }

  void OpenCLSummaryWriter::guidanceMemoryUsage(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      (t->fout) << "MEMORY_USAGE" << ","
		<< (device->platformInfo.deviceName) << ","
		<< 0 // TODO: Fill in memory usage
		<< std::endl ;
    }
  }

  void OpenCLSummaryWriter::guidancePLRAMDevice(OpenCLSummaryWriter* t)
  {
    bool hasPLRAM = false ;
    
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      for (auto memory : device->memoryInfo)
      {
	if ((memory.second)->type == MEM_BRAM || 
	    (memory.second)->type == MEM_URAM)
	{
	  hasPLRAM = true ;
	  break ;
	}
      }
      if (hasPLRAM) break ;
    }

    (t->fout) << "PLRAM_DEVICE" << ","
	      << "all" << ","
	      << (uint64_t)(hasPLRAM) << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceHBMDevice(OpenCLSummaryWriter* t)
  {
    bool hasHBM = false ;
    
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      for (auto memory : device->memoryInfo)
      {
	if ((memory.second)->type == MEM_HBM)
	{
	  hasHBM = true ;
	  break ;
	}
      }
      if (hasHBM) break ;
    }

    (t->fout) << "HBM_DEVICE" << ","
	      << "all" << ","
	      << (uint64_t)(hasHBM) << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceKDMADevice(OpenCLSummaryWriter* t)
  {
    bool hasKDMA = false ;
    
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      if (device->platformInfo.kdmaCount > 0)
      {
	hasKDMA = true ;
	break ;
      }
      // We previously had a few hard-coded KDMA platform checks.  If
      //  necessary we can do the same here.
      /*
      std::string deviceName = (device->platformInfo).deviceName ;
      if (deviceName.find("xilinx_u200_xdma_201830_2") != std::string::npos ||
	  deviceName.find("xilinx_vcu1525_xdma_201830_2") != std::string::npos)
      {
	hasKDMA = true ;
	break ;
      }
      */
    }

    (t->fout) << "KDMA_DEVICE" << ","
	      << "all" << ","
	      << (uint64_t)(hasKDMA) << std::endl ;
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
      std::string deviceName = device->platformInfo.deviceName ;
      if (deviceName.find("xilinx_u200_xdma_201830_2") != std::string::npos ||
	  deviceName.find("xilinx_u250_xdma_201830_2") != std::string::npos ||
	  deviceName.find("samsung")                   != std::string::npos ||
	  deviceName.find("xilinx_vcu1525_xdma_201830_2") != std::string::npos)
      {
	hasP2P = true ;
	break ;
      }
    }

    (t->fout) << "P2P_DEVICE" << ","
	      << "all" << ","
	      << (uint64_t)(hasP2P) ;
  }

  void OpenCLSummaryWriter::guidanceP2PHostTransfers(OpenCLSummaryWriter* t)
  {
    uint64_t hostP2PTransfers = (t->db->getStats()).getNumHostP2PTransfers() ;

    (t->fout) << "P2P_HOST_TRANSFERS" << ","
	      << "host" << ","
	      << hostP2PTransfers << std::endl ; // TODO: Make the connection
  }

  void OpenCLSummaryWriter::guidancePortBitWidth(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
    
    for (auto device : deviceInfos)
    {
      for (auto cu : device->cus)
      {
	for (auto monitor : (cu.second)->getMonitors())
	{
	  if (monitor->type == ACCEL_MONITOR) continue ;

	  (t->fout) << "PORT_BIT_WIDTH" << ","
		    << (cu.second)->getName() << "/" << monitor->args << ","
		    << monitor->portWidth << std::endl ;
	}
      }
    }
  }

  void OpenCLSummaryWriter::guidanceKernelCount(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
    std::map<std::string, uint64_t> kernelCounts ;
    
    for (auto device : deviceInfos)
    {
      for (auto cu : device->cus)
      {
	uint64_t totalExecutions = 0 ;
	std::vector<std::pair<std::string, TimeStatistics>> cuCalls = 
	  (t->db->getStats()).getComputeUnitExecutionStats((cu.second)->getName());
	for (auto cuCall : cuCalls)
	{
	  totalExecutions += (cuCall.second).numExecutions ;
	}

	// TODO: Set up the compute unit executions
	kernelCounts[(cu.second)->getName()] += totalExecutions ; 
      }
    }

    for (auto kernel : kernelCounts)
    {
      (t->fout) << "KERNEL_COUNT" << ","
		<< (kernel.first) << "," // Name
		<< (kernel.second) << "," // Count
		<< std::endl ;
    }
  }

  void OpenCLSummaryWriter::guidanceObjectsReleased(OpenCLSummaryWriter* t)
  {
    uint64_t numReleased = (t->db->getStats()).getNumOpenCLObjectsReleased() ;

    (t->fout) << "OBJECTS_RELEASED" << ","
	      << "all" << ","
	      << numReleased // TODO: Make the connection
	      << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceCUContextEn(OpenCLSummaryWriter* t)
  {
    bool isContextEnabled = (t->db->getStats()).getContextEnabled() ;

    (t->fout) << "CU_CONTEXT_EN" << ","
	      << "all" << ","
	      << (uint64_t)(isContextEnabled)
	      << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceTraceMemory(OpenCLSummaryWriter* t)
  {
    std::string memType = "FIFO" ;

    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;
    
    for (auto device : deviceInfos)
    {
      if (device->usesTs2mm)
      {
	memType = "TS2MM" ;
	break ;
      }
    }

    (t->fout) << "TRACE_MEMORY" << ","
	      << memType << std::endl ;
  }

  void OpenCLSummaryWriter::guidanceMaxParallelKernelEnqueues(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      for (auto cuInfo : device->cus)
      {
	std::string kernelName = (cuInfo.second)->getKernelName() ;
	uint64_t maxExecutions = 
	  (t->db->getStats()).getMaxExecutions(kernelName) ;

	(t->fout) << "MAX_PARALLEL_KERNEL_ENQUEUES" << ","
		  << kernelName << ","
		  << maxExecutions << std::endl ;
      }
    }
  }

  void OpenCLSummaryWriter::guidanceCommandQueueOOO(OpenCLSummaryWriter* t)
  {
    auto commandQueueInfo = (t->db->getStats()).getCommandQueuesAreOOO() ;

    for (auto cq : commandQueueInfo)
    {
      (t->fout) << "COMMAND_QUEUE_OOO" << "," 
		<< "0x" << std::hex << cq.first << "," << std::dec
		<< cq.second << std::endl ;
    }
  }

  void OpenCLSummaryWriter::guidancePLRAMSizeBytes(OpenCLSummaryWriter* t)
  {
    auto deviceInfos = (t->db->getStaticInfo()).getDeviceInfos() ;

    for (auto device : deviceInfos)
    {
      for (auto memory : device->memoryInfo)
      {
	if ((memory.second)->name.find("PLRAM") != std::string::npos)
	{
	  (t->fout) << "PLRAM_SIZE_BYTES" << ","
		    << (memory.second)->name << ","
		    << (memory.second)->size << std::endl ;
	}
      }
    }
  }

  void OpenCLSummaryWriter::guidanceKernelBufferInfo(OpenCLSummaryWriter* t)
  {
    // This reports the memory bank, argument, alignment, and size of 
    //  each buffer.

    // TODO

    // uint64_t -> std::vector<std::string>
    /*
    (t->fout) << "KERNEL_BUFFER_INFO" << ","
	      << std::endl ;
    */
  }

  void OpenCLSummaryWriter::guidanceTraceBufferFull(OpenCLSummaryWriter* t)
  {
    // TODO
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
		  << device->platformInfo.deviceName << "|DDR" << ","
		  << 64 << std::endl ;
      }
      else
      {
	(t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		  << device->platformInfo.deviceName << "|HBM" << ","
		  << 256 << std::endl ;
	(t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		  << device->platformInfo.deviceName << "|DDR" << ","
		  << 512 << std::endl ;
	(t->fout) << "MEMORY_TYPE_BIT_WIDTH" << "," 
		  << device->platformInfo.deviceName << "|PLRAM" << ","
		  << 512 << std::endl ;
	  
      }
    }
  }

  void OpenCLSummaryWriter::guidanceXrtIniSetting(OpenCLSummaryWriter* t)
  {
    for (auto setting : t->iniSettings)
    {
      (t->fout) << setting << std::endl ;
    }
  }

  void OpenCLSummaryWriter::guidanceBufferRdActiveTimeMs(OpenCLSummaryWriter* t)
  {
    // TODO
    /*
    (t->fout) << "BUFFER_RD_ACTIVE_TIME_MS" << ","
	      << std::endl ;
    */
  }

  void OpenCLSummaryWriter::guidanceBufferWrActiveTimeMs(OpenCLSummaryWriter* t)
  {
    // TODO
    /*
    (t->fout) << "BUFFER_WR_ACTIVE_TIME_MS" << ","
	      << std::endl ;
    */
  }

  void OpenCLSummaryWriter::guidanceBufferTxActiveTimeMs(OpenCLSummaryWriter* t)
  {
    // TODO
    /*
    (t->fout) << "BUFFER_TX_ACTIVE_TIME_MS" << ","
	      << "all" << ","
	      << std::endl ;
    */
  }

  void OpenCLSummaryWriter::guidanceApplicationRunTimeMs(OpenCLSummaryWriter* t)
  {
    uint64_t startTime = (t->db->getStaticInfo()).getApplicationStartTime() ;
    uint64_t endTime = xrt_core::time_ns() ;
    
    (t->fout) << "APPLICATION_RUN_TIME_MS" << ","
	      << "all" << ","
	      << ((double)(endTime - startTime) / 1.0e6)
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
	      << ((lastKernelEndTime - firstKernelStartTime) / 1.0e06)
	      << std::endl ;
  }

} // end namespace xdp
