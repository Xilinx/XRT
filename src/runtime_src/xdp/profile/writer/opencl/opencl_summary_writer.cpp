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

#include "xdp/profile/database/database.h"
#include "xdp/profile/database/statistics_database.h"
#include "xdp/profile/database/static_info_database.h"
#include "xdp/profile/plugin/vp_base/utility.h"
#include "xdp/profile/writer/opencl/opencl_summary_writer.h"

#include "core/common/system.h"

namespace xdp {

  OpenCLSummaryWriter::OpenCLSummaryWriter(const char* filename)
    : VPSummaryWriter(filename)
  {
    // The OpenCL Summary Writer will be responsible for
    //  summarizing information from host code API calls as well
    //  as any information on any devices that have been monitored
    //  by other plugins.  This will not instantiate any devices that don't
    //  already exist.
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
	  
	  fout << (device->platformInfo.deviceName) << ","
	       << masterPort << ","
	       << masterArgs << ","
	       << slavePort << ","
	       << slaveArgs << ","
	       << numTranx << ","
	       << "" << "," // TODO: Transfer Rate
	       << (values.StrDataBytes[ASMIndex] / numTranx) << ","
	       << "" << "," // TODO: Link utilization
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
  }

  void OpenCLSummaryWriter::writeGuidance()
  {
    // Caption
    fout << "Guidance Parameters" << std::endl ;
    
    // Columns
    fout << "Parameter" << ","
	 << "Element"   << ","
	 << "Value"     << std::endl ;
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

} // end namespace xdp
