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

#define XDP_SOURCE

#include "xdp/profile/writer/vp_base/summary_writer.h"
#include "xdp/profile/plugin/vp_base/info.h"
#include "xdp/profile/plugin/vp_base/utility.h"

#include "core/common/config_reader.h"

#ifdef _WIN32
/* Disable warning for use of localtime */
#pragma warning(disable : 4996)
#endif

namespace xdp {

  SummaryWriter::SummaryWriter(const char* filename) 
    : VPSummaryWriter(filename), guidance()
  {
    initializeAPIs() ;
  }

  SummaryWriter::SummaryWriter(const char* filename, VPDatabase* inst) :
    VPSummaryWriter(filename, inst), guidance()
  {
    initializeAPIs() ;
  }

  SummaryWriter::~SummaryWriter()
  {
  }

  void SummaryWriter::initializeAPIs()
  {
    // For each of the APIs, initialize the sets with the hard coded names
    OpenCLAPIs.emplace("clBuildProgram") ;
    OpenCLAPIs.emplace("clCompileProgram") ;
    OpenCLAPIs.emplace("clCreateBuffer") ;
    OpenCLAPIs.emplace("clCreateCommandQueue") ;
    OpenCLAPIs.emplace("clCreateContext") ;
    OpenCLAPIs.emplace("clCreateContextFromType") ;
    OpenCLAPIs.emplace("clCreateImage2D") ;
    OpenCLAPIs.emplace("clCreateImage3D") ;
    OpenCLAPIs.emplace("clCreateImage") ;
    OpenCLAPIs.emplace("clCreateKernel") ;
    OpenCLAPIs.emplace("clCreateKernelsInProgram") ;
    OpenCLAPIs.emplace("clCreatePipe") ;
    OpenCLAPIs.emplace("clCreateProgramWithBinary") ;
    OpenCLAPIs.emplace("clCreateProgramWithBuiltInKernels") ;
    OpenCLAPIs.emplace("clCreateProgramWithSource") ;
    OpenCLAPIs.emplace("clCreateSampler") ;
    OpenCLAPIs.emplace("clCreateSubBuffer") ;
    OpenCLAPIs.emplace("clCreateSubDevices") ;
    OpenCLAPIs.emplace("clCreateUserEvent") ;
    OpenCLAPIs.emplace("clEnqueueBarrier") ;
    OpenCLAPIs.emplace("clEnqueueBarrierWithWaitList") ;
    OpenCLAPIs.emplace("clEnqueueCopyBuffer") ;
    OpenCLAPIs.emplace("clEnqueueCopyBufferRect") ;
    OpenCLAPIs.emplace("clEnqueueCopyBufferToImage") ;
    OpenCLAPIs.emplace("clEnqueueCopyImage") ;
    OpenCLAPIs.emplace("clEnqueueCopyImageToBuffer") ;
    OpenCLAPIs.emplace("clEnqueueFillBuffer") ;
    OpenCLAPIs.emplace("clEnqueueFillImage") ;
    OpenCLAPIs.emplace("clEnqueueMapBuffer") ;
    OpenCLAPIs.emplace("clEnqueueMapImage") ;
    OpenCLAPIs.emplace("clEnqueueMarker") ;
    OpenCLAPIs.emplace("clEnqueueMarkerWithWaitList") ;
    OpenCLAPIs.emplace("clEnqueueMigrateMemObjects") ;
    OpenCLAPIs.emplace("clEnqueueNativeKernel") ;
    OpenCLAPIs.emplace("clEnqueueNDRangeKernel") ;
    OpenCLAPIs.emplace("clEnqueueReadBuffer") ;
    OpenCLAPIs.emplace("clEnqueueReadBufferRect") ;
    OpenCLAPIs.emplace("clEnqueueReadImage") ;
    OpenCLAPIs.emplace("clEnqueueSVMMap") ;
    OpenCLAPIs.emplace("clEnqueueSVMUnmap") ;
    OpenCLAPIs.emplace("clEnqueueTask") ;
    OpenCLAPIs.emplace("clEnqueueUnmapMemObject") ;
    OpenCLAPIs.emplace("clEnqueueWaitForEvents") ;
    OpenCLAPIs.emplace("clEnqueueWriteBuffer") ;
    OpenCLAPIs.emplace("clEnqueueWriteBufferRect") ;
    OpenCLAPIs.emplace("clEnqueueWriteImage") ;
    OpenCLAPIs.emplace("clFinish") ;
    OpenCLAPIs.emplace("clFlush") ;
    OpenCLAPIs.emplace("clGetCommandQueueInfo") ;
    OpenCLAPIs.emplace("clGetContextInfo") ;
    OpenCLAPIs.emplace("clGetDeviceIDs") ;
    OpenCLAPIs.emplace("clGetDeviceInfo") ;
    OpenCLAPIs.emplace("clGetEventInfo") ;
    OpenCLAPIs.emplace("clGetEventProfilingInfo") ;
    OpenCLAPIs.emplace("clGetExtensionFunctionAddress") ;
    OpenCLAPIs.emplace("clGetExtensionFunctionAddressForPlatform") ;
    OpenCLAPIs.emplace("clGetImageInfo") ;
    OpenCLAPIs.emplace("clGetKernelArgInfo") ;
    OpenCLAPIs.emplace("clGetKernelInfo") ;
    OpenCLAPIs.emplace("clGetKernelWorkGroupInfo") ;
    OpenCLAPIs.emplace("clGetMemObjectInfo") ;
    OpenCLAPIs.emplace("clGetPipeInfo") ;
    OpenCLAPIs.emplace("clGetPlatformIDs") ;
    OpenCLAPIs.emplace("clGetPlatformInfo") ;
    OpenCLAPIs.emplace("clGetSamplerInfo") ;
    OpenCLAPIs.emplace("clGetSupportedImageFormats") ;
    OpenCLAPIs.emplace("clLinkProgram") ;
    OpenCLAPIs.emplace("clReleaseCommandQueue") ;
    OpenCLAPIs.emplace("clReleaseContext") ;
    OpenCLAPIs.emplace("clReleaseDevice") ;
    OpenCLAPIs.emplace("clReleaseEvent") ;
    OpenCLAPIs.emplace("clReleaseKernel") ;
    OpenCLAPIs.emplace("clReleaseMemObject") ;
    OpenCLAPIs.emplace("clReleaseProgram") ;
    OpenCLAPIs.emplace("clReleaseSampler") ;
    OpenCLAPIs.emplace("clRetainContext") ;
    OpenCLAPIs.emplace("clRetainDevice") ;
    OpenCLAPIs.emplace("clRetainEvent") ;
    OpenCLAPIs.emplace("clRetainKernel") ;
    OpenCLAPIs.emplace("clRetainMemObject") ;
    OpenCLAPIs.emplace("clRetainProgram") ;
    OpenCLAPIs.emplace("clRetainSampler") ;
    OpenCLAPIs.emplace("clSetCommandQueueProperty") ;
    OpenCLAPIs.emplace("clSetEventCallback") ;
    OpenCLAPIs.emplace("clSetKernelArg") ;
    OpenCLAPIs.emplace("clSetKernelArgSMPointer") ;
    OpenCLAPIs.emplace("clSetMemObjectDestructorCallback") ;
    OpenCLAPIs.emplace("clSetPrintfCallback") ;
    OpenCLAPIs.emplace("clSetUserEventStatus") ;
    OpenCLAPIs.emplace("clSVMAlloc") ;
    OpenCLAPIs.emplace("clSVMFree") ;
    OpenCLAPIs.emplace("clUnloadCompiler") ;
    OpenCLAPIs.emplace("clUnloadPlatformCompiler") ;
    OpenCLAPIs.emplace("clWaitForEvents") ;
    OpenCLAPIs.emplace("clCreateStream") ;
    OpenCLAPIs.emplace("clCreateStreamBuffer") ;
    OpenCLAPIs.emplace("clPollStream") ;
    OpenCLAPIs.emplace("clPollStreams") ;
    OpenCLAPIs.emplace("clReadStream") ;
    OpenCLAPIs.emplace("clReleaseStream") ;
    OpenCLAPIs.emplace("clReleaseStreamBuffer") ;
    OpenCLAPIs.emplace("clSetStreamOpt") ;
    OpenCLAPIs.emplace("clWriteStream") ;
    OpenCLAPIs.emplace("xclGetComputeUnitInfo") ;

    NativeAPIs.emplace("xrt::bo::bo");
    NativeAPIs.emplace("xrt::bo::size");
    NativeAPIs.emplace("xrt::bo::address");
    NativeAPIs.emplace("xrt::bo::export_buffer");
    NativeAPIs.emplace("xrt::bo::sync");
    NativeAPIs.emplace("xrt::bo::map");
    NativeAPIs.emplace("xrt::bo::write");
    NativeAPIs.emplace("xrt::bo::read");
    NativeAPIs.emplace("xrt::bo::copy");
    NativeAPIs.emplace("xrtBOAllocUserPtr");
    NativeAPIs.emplace("xrtBOAlloc");
    NativeAPIs.emplace("xrtBOSubAlloc");
    NativeAPIs.emplace("xrtBOImport");
    NativeAPIs.emplace("xrtBOExport");
    NativeAPIs.emplace("xrtBOFree");
    NativeAPIs.emplace("xrtBOSize");
    NativeAPIs.emplace("xrtBOSync");
    NativeAPIs.emplace("xrtBOMap");
    NativeAPIs.emplace("xrtBOWrite");
    NativeAPIs.emplace("xrtBORead");
    NativeAPIs.emplace("xrtBOCopy");
    NativeAPIs.emplace("xrtBOAddress");
    NativeAPIs.emplace("xrt::device::device");
    NativeAPIs.emplace("xrt::device::load_xclbin");
    NativeAPIs.emplace("xrt::device::get_xclbin_uuid");
    NativeAPIs.emplace("xrt::device::reset");
    NativeAPIs.emplace("xrt::device::get_xclbin_section");
    NativeAPIs.emplace("xrtDeviceOpen");
    NativeAPIs.emplace("xrtDeviceOpenByBDF");
    NativeAPIs.emplace("xrtDeviceClose");
    NativeAPIs.emplace("xrtDeviceLoadXclbin");
    NativeAPIs.emplace("xrtDeviceLoadXclbinFile");
    NativeAPIs.emplace("xrtDeviceLoadXclbinHandle");
    NativeAPIs.emplace("xrtDeviceLoadXclbinUUID");
    NativeAPIs.emplace("xrtDeviceGetXclbinUUID");
    NativeAPIs.emplace("xrtDeviceToXclDevice");
    NativeAPIs.emplace("xrtDeviceOpenFromXcl");
    NativeAPIs.emplace("xrt::error::error");
    NativeAPIs.emplace("xrt::error::get_timestamp");
    NativeAPIs.emplace("xrt::error::get_error_code");
    NativeAPIs.emplace("xrt::error::to_string");
    NativeAPIs.emplace("xrtErrorGetLast");
    NativeAPIs.emplace("xrtErrorGetString");
    NativeAPIs.emplace("xrt::run::run");
    NativeAPIs.emplace("xrt::run::start");
    NativeAPIs.emplace("xrt::run::wait");
    NativeAPIs.emplace("xrt::run::state");
    NativeAPIs.emplace("xrt::run::set_event");
    NativeAPIs.emplace("xrt::run::get_ert_packet");
    NativeAPIs.emplace("xrt::kernel::kernel");
    NativeAPIs.emplace("xrt::kernel::read_register");
    NativeAPIs.emplace("xrt::kernel::write_register");
    NativeAPIs.emplace("xrt::kernel::group_id");
    NativeAPIs.emplace("xrt::kernel::offset");
    NativeAPIs.emplace("xrtPLKernelOpen");
    NativeAPIs.emplace("xrtPLKernelOpenExclusive");
    NativeAPIs.emplace("xrtKernelClose");
    NativeAPIs.emplace("xrtRunOpen");
    NativeAPIs.emplace("xrtKernelArgGroupId");
    NativeAPIs.emplace("xrtKernelArgOffset");
    NativeAPIs.emplace("xrtKernelReadRegister");
    NativeAPIs.emplace("xrtKernelWriteRegister");
    NativeAPIs.emplace("xrtKernelRun");
    NativeAPIs.emplace("xrtRunClose");
    NativeAPIs.emplace("xrtRunState");
    NativeAPIs.emplace("xrtRunWait");
    NativeAPIs.emplace("xrtRunWaitFor");
    NativeAPIs.emplace("xrtRunSetCallback");
    NativeAPIs.emplace("xrtRunStart");
    NativeAPIs.emplace("xrtRunUpdateArg");
    NativeAPIs.emplace("xrtRunUpdateArgV");
    NativeAPIs.emplace("xrtRunSetArg");
    NativeAPIs.emplace("xrtRunSetArgV");
    NativeAPIs.emplace("xrtRunGetArgV");
    NativeAPIs.emplace("xrtRunGetArgVPP");
    NativeAPIs.emplace("xrtXclbinAllocFilename");
    NativeAPIs.emplace("xrtXclbinAllocRawData");
    NativeAPIs.emplace("xrtXclbinFreeHandle");
    NativeAPIs.emplace("xrtXclbinGetXSAName");
    NativeAPIs.emplace("xrtXclbinGetUUID");
    NativeAPIs.emplace("xrtXclbinGetData");
    NativeAPIs.emplace("xrtXclbinUUID");

    HALAPIs.emplace("xclLoadXclbin") ;
    HALAPIs.emplace("xclProbe") ;
    HALAPIs.emplace("xclOpen") ;
    HALAPIs.emplace("xclClose") ;
    HALAPIs.emplace("xclWrite") ;
    HALAPIs.emplace("xclRead") ;
    HALAPIs.emplace("xclAllocBO") ;
    HALAPIs.emplace("xclAllocUserPtrBO") ;
    HALAPIs.emplace("xclFreeBO") ;
    HALAPIs.emplace("xclWriteBO") ;
    HALAPIs.emplace("xclReadBO") ;
    HALAPIs.emplace("xclMapBO") ;
    HALAPIs.emplace("xclSyncBO") ;
    HALAPIs.emplace("xclCopyBO") ;
    HALAPIs.emplace("xclLockDevice") ;
    HALAPIs.emplace("xclUnlockDevice") ;
    HALAPIs.emplace("xclUnmgdPwrite") ;
    HALAPIs.emplace("xclUnmgdPread") ;
    HALAPIs.emplace("xclOpenContext") ;
    HALAPIs.emplace("xclExecBuf") ;
    HALAPIs.emplace("xclExecWait") ;
    HALAPIs.emplace("xclCloseContext") ;
    HALAPIs.emplace("xclGetBOProperties") ;
  }

  void SummaryWriter::writeHeader()
  {
    std::string currentTime = "0000-00-00 0000" ;

    auto time = 
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) ;
    struct tm* p_tstruct = std::localtime(&time) ;
    if (p_tstruct) {
      char buf[80] = {0} ;
      strftime(buf, sizeof(buf), "%Y-%m-%d %X", p_tstruct) ;
      currentTime = std::string(buf) ;
    }

    std::string msecSinceEpoch = "" ;
    auto timeSinceEpoch = (std::chrono::system_clock::now()).time_since_epoch();
    auto value =
      std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceEpoch) ;
    msecSinceEpoch = std::to_string(value.count()) ;

    std::string execName = "" ;
#if defined(__linux__) && defined (__x86_64__)
    const int maxLength = 1024 ;
    char buf[maxLength] ;
    ssize_t len ;
    if ((len=readlink("/proc/self/exe", buf, maxLength-1)) != -1) {
      buf[len] = '\0' ;
      execName = buf ;
    }

    auto slashLocation = execName.find_last_of("/") ;
    if (slashLocation != std::string::npos) {
      execName = execName.substr(slashLocation + 1) ;
    }
#endif

    boost::property_tree::ptree xrtInfo ;
    xrt_core::get_xrt_build_info(xrtInfo) ;

    fout << "Profile Summary\n" ;
    fout << "Generated on: " << currentTime << "\n" ;
    fout << "Msec since Epoch: " << msecSinceEpoch << "\n" ;
    fout << "Profiled application: " << execName << "\n" ;
    fout << "Target platform: " << "Xilinx" << "\n" ;
    fout << "Tool version: " << getToolVersion() << "\n" ;
    fout << "XRT build version: " 
	 << (xrtInfo.get<std::string>("version", "N/A")) << "\n" ;
    fout << "Build version branch: " 
	 << (xrtInfo.get<std::string>("branch", "N/A")) << "\n" ;
    fout << "Build version hash: "
	 << (xrtInfo.get<std::string>("hash", "N/A")) << "\n" ;
    fout << "Build version date: "
	 << (xrtInfo.get<std::string>("date", "N/A")) << "\n" ;

    fout << "Target devices: " ;
    if (getFlowMode() == SW_EMU) {
      fout << (db->getStaticInfo()).getSoftwareEmulationDeviceName() << "\n" ;
    }
    else {
      std::vector<std::string> deviceNames =
        (db->getStaticInfo()).getDeviceNames() ;
      for (unsigned int i = 0 ; i < deviceNames.size() ; ++i) {
	if (i != 0) fout << ", " ;
	fout << deviceNames[i] ;
      }
      fout << "\n" ;
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
    fout << "\n" ;
  }

  void
  SummaryWriter::writeAPICalls(APIType type)
  {
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
    
    for (auto call : callCount) {
      auto callAndThread = call.first ;
      auto APIName = callAndThread.first ;

      switch (type) {
      case OPENCL:
        if (OpenCLAPIs.find(APIName) == OpenCLAPIs.end()) continue ;
        break ;
      case NATIVE:
        if (NativeAPIs.find(APIName) == NativeAPIs.end()) continue ;
        break ;
      case HAL:
        if (HALAPIs.find(APIName) == HALAPIs.end()) continue ;
        break ;
      case ALL: // Intentionally fall through
      default:
        break ;
      }

      std::vector<std::pair<double, double>> timesOfCalls = call.second ;

      if (rows.find(APIName) == rows.end()) {
	std::tuple<uint64_t, double, double, double> blank = 
	  std::make_tuple<uint64_t, double, double, double>(0,0,std::numeric_limits<double>::max(),0) ;

	rows[APIName] = blank ;
      }

      for (auto executionTime : timesOfCalls) {
	auto timeTaken = executionTime.second - executionTime.first ;

	++(std::get<0>(rows[APIName])) ;
	std::get<1>(rows[APIName]) += timeTaken ;
	if (timeTaken < std::get<2>(rows[APIName]))
	  std::get<2>(rows[APIName]) = timeTaken ;
	if (timeTaken > std::get<3>(rows[APIName]))
	  std::get<3>(rows[APIName]) = timeTaken ;
      }
    }

    for (auto row : rows) {
      auto averageTime = 
	(double)(std::get<1>(row.second)) / (double)(std::get<0>(row.second)) ;
      if (type != OPENCL) fout << "ENTRY:" ;
      fout << row.first                      << ","     // API Name
	   << std::get<0>(row.second)        << ","     // Number of calls
	   << (std::get<1>(row.second)/one_million) << ","     // Total time
	   << (std::get<2>(row.second)/one_million) << ","     // Minimum time
	   << (averageTime/one_million)             << ","     // Average time
	   << (std::get<3>(row.second)/one_million) << ",\n" ; // Maximum time
    }
  }

  void SummaryWriter::writeOpenCLAPICalls()
  {
    // Title
    fout << "OpenCL API Calls\n" ;
    // Columns
    fout << "API Name,Number Of Calls,Total Time (ms),Minimum Time (ms),"
	 << "Average Time (ms),Maximum Time (ms),\n" ;
    writeAPICalls(OPENCL) ;
  }

  void SummaryWriter::writeNativeAPICalls()
  {
    fout << "TITLE:Native API Calls\n" ;
    fout << "SECTION:API Calls,Native API Calls\n" ;
    fout << "COLUMN:<html>API Name</html>,string,Name of Native XRT API,\n" ;
    fout << "COLUMN:<html>Number<br>Of Calls</html>,int,"
         << "Number of calls to API,\n" ;
    fout << "COLUMN:<html>Total<br>Time (ms)</html>,float,"
         << "Total execution time (in ms),\n" ;
    fout << "COLUMN:<html>Minimum<br>Time (ms)</html>,float,"
         << "Minimum execution time (in ms),\n";
    fout << "COLUMN:<html>Average<br>Time (ms)</html>,float,"
         << "Average execution time (in ms),\n";
    fout << "COLUMN:<html>Maximum<br>Time (ms)</html>,float,"
         << "Maximum execution time (in ms),\n";
    writeAPICalls(NATIVE) ;
  }

  void SummaryWriter::writeHALAPICalls()
  {
    fout << "TITLE:HAL API Calls\n" ;
    fout << "SECTION:API Calls,HAL API Calls\n" ;
    fout << "COLUMN:<html>API Name</html>,string,Name of HAL XRT API,\n" ;
    fout << "COLUMN:<html>Number<br>Of Calls</html>,int,"
         << "Number of calls to API,\n" ;
    fout << "COLUMN:<html>Total<br>Time (ms)</html>,float,"
         << "Total execution time (in ms),\n" ;
    fout << "COLUMN:<html>Minimum<br>Time (ms)</html>,float,"
         << "Minimum execution time (in ms),\n";
    fout << "COLUMN:<html>Average<br>Time (ms)</html>,float,"
         << "Average execution time (in ms),\n";
    fout << "COLUMN:<html>Maximum<br>Time (ms)</html>,float,"
         << "Maximum execution time (in ms),\n";
    writeAPICalls(HAL) ;
  }

  void SummaryWriter::writeHALTransfers()
  {
    fout << "HAL data transfers\n" ;
    fout << "Device ID,"
         << "Number of Unmanaged Read transactions,"
         << "Unmanaged Read bytes transferred,"
         << "Number of Unmanaged Write transactions,"
         << "Unmanaged Write bytes transferred,"
         << "Number of xclRead transactions,"
         << "xclRead bytes transferred,"
         << "Number of xclWrite transactions,"
         << "xclWrite bytes transferred,"
         << "Number of readBuffer transactions,"
         << "readBuffer bytes transferred,"
         << "Number of writeBuffer transactions,"
         << "writeBuffer bytes transferred,\n" ;

    auto memStats = db->getStats().getMemoryStats() ;
    for (auto iter : memStats) {
      fout << iter.first                               << ","
           << iter.second.channels[0].transactionCount << ","
           << iter.second.channels[0].totalByteCount   << ","
           << iter.second.channels[1].transactionCount << ","
           << iter.second.channels[1].totalByteCount   << ","
           << iter.second.channels[2].transactionCount << ","
           << iter.second.channels[2].totalByteCount   << ","
           << iter.second.channels[3].transactionCount << ","
           << iter.second.channels[3].totalByteCount   << ",\n" ;
    }
  }

  void SummaryWriter::writeKernelExecutionSummary()
  {
    // On Edge hardware emuation, the numbers for the top kernel executions
    //  don't align with the other numbers we display, so don't print this
    //  table.
    if (getFlowMode() == HW_EMU && isEdge()) return ;

    // We can get kernel executions from purely host information
    std::map<std::string, TimeStatistics> kernelExecutions = 
      (db->getStats()).getKernelExecutionStats() ;

    if (kernelExecutions.size() == 0)
      return ;

    // Caption
    fout << "Kernel Execution" ;
    if (getFlowMode() == HW_EMU) {
      fout << " (includes estimated device time)" ;
    }
    fout << "\n" ;

    // Column headers
    fout << "Kernel,Number Of Enqueues,Total Time (ms),Minimum Time (ms),"
	 << "Average Time (ms),Maximum Time (ms),\n" ; 

    for (auto execution : kernelExecutions) {
      fout << execution.first                         << ","
	   << (execution.second).numExecutions        << ","
	   << ((execution.second).totalTime / one_million)   << ","
	   << ((execution.second).minTime / one_million)     << ","
	   << ((execution.second).averageTime / one_million) << ","
	   << ((execution.second).maxTime / one_million)     << ",\n" ;
    }
  }

  void SummaryWriter::writeTopKernelExecution()
  {
    // On Edge hardware emuation, the numbers for the top kernel executions
    //  don't align with the other numbers we display, so don't print this
    //  table.
    if (getFlowMode() == HW_EMU && isEdge()) return ;

    if (db->getStats().getTopKernelExecutions().size() == 0)
      return ;

    // Caption
    fout << "Top Kernel Execution\n";

    // Columns
    fout << "Kernel Instance Address,Kernel,Context ID,Command Queue ID,"
	 << "Device,Start Time (ms),Duration (ms),Global Work Size,"
	 << "Local Work Size,\n" ;

    for (std::list<KernelExecutionStats>::iterator iter = (db->getStats()).getTopKernelExecutions().begin() ;
	 iter != (db->getStats()).getTopKernelExecutions().end() ;
	 ++iter) {
      fout << (*iter).kernelInstanceAddress << ","
	   << (*iter).kernelName << ","
	   << (*iter).contextId << ","
	   << (*iter).commandQueueId << "," 
	   << (*iter).deviceName << ","
	   << (double)((*iter).startTime) / one_million << ","
	   << (double)((*iter).duration) / one_million << ","
	   << (*iter).globalWorkSize << ","
	   << (*iter).localWorkSize << ",\n" ;
    }
  }

  void SummaryWriter::writeTopMemoryWrites()
  {
    if (db->getStats().getTopHostWrites().size() == 0)
      return ;

    // Caption
    fout << "Top Memory Writes: Host to Global Memory\n" ;

    // Columns
    fout << "Buffer Address,Context ID,Command Queue ID,Start Time (ms),"
	 << "Duration (ms),Buffer Size (KB),Writing Rate(MB/s),\n" ;

    for (std::list<BufferTransferStats>::iterator iter = (db->getStats()).getTopHostWrites().begin() ;
	 iter != (db->getStats()).getTopHostWrites().end() ;
	 ++iter) {
      double durationMS = (double)((*iter).duration) / one_million ;
      double rate = ((double)((*iter).size) / one_thousand) * durationMS ;

      fout << (*iter).address << ","
	   << (*iter).contextId << ","
	   << (*iter).commandQueueId << ","
	   << (double)((*iter).startTime) / one_million << "," ;
      if (getFlowMode() == HW)
	fout << durationMS << "," ;
      else
	fout << "N/A," ;
      fout << (double)((*iter).size) / one_thousand << "," ;
      if (getFlowMode() == HW)
	fout << rate << ",\n" ;
      else
	fout << "N/A" << ",\n" ;
    }
  }

  void SummaryWriter::writeTopMemoryReads()
  {
    if (db->getStats().getTopHostReads().size() == 0)
      return ;

    // Caption
    fout << "Top Memory Reads: Host to Global Memory\n" ;

    // Columns
    fout << "Buffer Address,Context ID,Command Queue ID,Start Time (ms),"
	 << "Duration (ms),Buffer Size (KB),Reading Rate(MB/s),\n" ;

    for (std::list<BufferTransferStats>::iterator iter = (db->getStats()).getTopHostReads().begin() ;
	 iter != (db->getStats()).getTopHostReads().end() ;
	 ++iter) {
      double durationMS = (double)((*iter).duration) / one_million ;
      double rate = ((double)((*iter).size) / one_thousand) * durationMS ;

      fout << (*iter).address << "," 
	   << (*iter).contextId << ","
	   << (*iter).commandQueueId << ","
	   << (double)((*iter).startTime) / one_million << "," ;
      if (getFlowMode() == HW)
	fout << durationMS << "," ;
      else
	fout << "N/A," ;
      fout << (double)((*iter).size) / one_thousand << "," ;
      if (getFlowMode() == HW)
	fout << rate << ",\n" ;
      else
	fout << "N/A" << ",\n" ;
    }
  }

  void SummaryWriter::writeSoftwareEmulationComputeUnitUtilization()
  {
    std::map<std::tuple<std::string, std::string, std::string>,
	     TimeStatistics> cuStats = 
      (db->getStats()).getComputeUnitExecutionStats() ;

    if (cuStats.size() == 0)
      return ;

    // Caption
    fout << "Compute Unit Utilization\n" ;

    // Column headers
    fout << "Device,Compute Unit,Kernel,Global Work Size,Local Work Size,"
	 << "Number Of Calls,Dataflow Execution,Max Overlapping Executions,"
	 << "Dataflow Acceleration,Total Time (ms),Minimum Time (ms),"
	 << "Average Time (ms),Maximum Time (ms),Clock Frequency (MHz),\n" ;

    for (auto stat : cuStats) {
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
      if (usPosition != std::string::npos) {
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
	   << (totalTime / one_million)                  << ","
	   << (minTime / one_million)                    << ","
	   << (averageTime / one_million)                << ","
	   << (maxTime / one_million)                    << ","
	   << 300                                 << ",\n" ;
    }
  }

  void SummaryWriter::writeComputeUnitUtilization()
  {
    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    // Check if we need to output this table at all...

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
	  uint64_t amSlotID = (uint64_t)((cuInfo.second)->getAccelMon()) ;

          // Stats don't make sense if runtime or executions = 0
          if ((values.CuBusyCycles[amSlotID] == 0) ||
              (values.CuExecCount[amSlotID] == 0))
            continue;

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

	  for (auto cuCall : cuCalls)
	  {
	    std::string globalWorkDimensions = cuCall.first ;

	    auto kernelClockMHz = xclbin->clockRateMHz ;
	    double deviceCyclesMsec = (double)(kernelClockMHz) * one_thousand ;

	    double cuRunTimeMsec =
	      (double)(values.CuBusyCycles[amSlotID]) / deviceCyclesMsec ;
	    double cuRunTimeAvgMsec = (double)(values.CuExecCycles[amSlotID]) / deviceCyclesMsec / (double)(values.CuExecCount[amSlotID]) ;
	    double cuMaxExecCyclesMsec = (double)(values.CuMaxExecCycles[amSlotID]) / deviceCyclesMsec ;
	    double cuMinExecCyclesMsec = (double)(values.CuMinExecCycles[amSlotID]) / deviceCyclesMsec ;

	    double speedup = (cuRunTimeAvgMsec * (double)(values.CuExecCount[amSlotID])) / cuRunTimeMsec ;

	    //double speedup =
	    // (averageTime*(values.CuExecCount[cuIndex]))/totalTime ;
	    std::string speedup_string = std::to_string(speedup) + "x" ;

	    fout << device->getUniqueDeviceName() << "," 
		 << cuName << ","
		 << kernelName << ","
		 << globalWorkDimensions << ","
		 << cuLocalDimensions << ","
		 << values.CuExecCount[amSlotID] << ","
		 << dataflowEnabled << ","
		 << values.CuMaxParallelIter[amSlotID] << ","
		 << speedup_string << ","
		 << cuRunTimeMsec << "," //<< (totalTime / one_million) << ","
		 << cuMinExecCyclesMsec << "," //<< (minTime / one_million) << ","
		 << cuRunTimeAvgMsec << "," //<< (averageTime /one_million) << ","
		 << cuMaxExecCyclesMsec << "," //<< (maxTime / one_million) << "," 
		 << (xclbin->clockRateMHz) << ","
		 << std::endl ;
	  }
	}
      }
    }
  }

  void SummaryWriter::writeComputeUnitStallInformation()
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
          double deviceCyclesMsec = (double)(xclbin->clockRateMHz * one_thousand);

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

  void SummaryWriter::writeDataTransferHostToGlobalMemory()
  {
    std::map<std::pair<uint64_t, uint64_t>, BufferStatistics> hostReads =
      (db->getStats()).getHostReads() ;
    std::map<std::pair<uint64_t, uint64_t>, BufferStatistics> hostWrites =
      (db->getStats()).getHostWrites() ;

    if (hostReads.size() == 0 && hostWrites.size() == 0)
      return ;

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
	     << ((double)((read.second).averageSize) / one_thousand) << ","
	     << "N/A" << ","
	     << "N/A" << "," << std::endl ;
      }
      else
      {
        double totalTimeInS  = (double)((read.second).totalTime / one_billion);
        double totalSizeInMB = (double)((read.second).totalSize / one_million);
        double transferRate  = totalSizeInMB / totalTimeInS; 

	double maxReadBW =
	  (db->getStaticInfo()).getMaxReadBW(read.first.second) ;
	double aveBWUtil = (one_hundred * transferRate) / maxReadBW ;

	fout << contextName << ":" << numDevices << ","
	     << "READ" << ","
	     << (read.second).count << ","
	     << transferRate << ","
	     << aveBWUtil << ","
	     << ((double)((read.second).averageSize) / one_thousand) << ","
	     << ((read.second).totalTime / one_million) << ","
	     << ((read.second).averageTime / one_million) << "," << std::endl ;
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
	     << ((double)((write.second).averageSize) / one_thousand) << ","
	     << "N/A" << ","
	     << "N/A" << "," << std::endl ;
      }
      else
      {
        double totalTimeInS  = (double)((write.second).totalTime / one_billion);
        double totalSizeInMB = (double)((write.second).totalSize / one_million);
        double transferRate  = totalSizeInMB / totalTimeInS; 

	double maxWriteBW =
	  (db->getStaticInfo()).getMaxWriteBW(write.first.second);
	double aveBWUtil = (one_hundred * transferRate) / maxWriteBW ;

	fout << contextName << ":" << numDevices << "," 
	     << "WRITE" << ","
	     << (write.second).count << ","
	     << transferRate << ","
	     << aveBWUtil << ","
	     << ((double)((write.second).averageSize) / one_thousand) << ","
	     << ((write.second).totalTime / one_million) << ","
	     << ((write.second).averageTime / one_million) << "," << std::endl ;
      }
    }
  }

  void SummaryWriter::writeHostReadsFromGlobalMemory()
  {
    std::map<std::pair<uint64_t, uint64_t>, BufferStatistics> hostReads =
      (db->getStats()).getHostReads() ;
    if (hostReads.size() == 0) return ;

    fout << "TITLE:Host Reads from Global Memory\n" ;
    fout << "SECTION:Host Data Transfers,Host Reads from Global Memory\n" ;
    fout << "COLUMN:<html>Number<br>of Reads</html>,int,"
         << "Number of host reads (note: may contain OpenCL printf transfers),"
         << "\n" ;
    fout << "COLUMN:<html>Maximum<br>Buffer<br>Size (KB)</html>,float,"
         << "Maximum buffer size of host reads,\n";
    fout << "COLUMN:<html>Minimum<br>Buffer<br>Size (KB)</html>,float,"
         << "Minimum buffer size of host reads,\n";
    fout << "COLUMN:<html>Average<br>Buffer<br>Size (KB)</html>,float,"
         << "Average buffer size of host reads: "
         << "Average Size = (Total KB) / (Number of Transfers),\n";

    if (getFlowMode() == HW) {
      fout << "COLUMN:<html>Transfer<br>Rate (MB/s)</html>,float,"
           << "Rate of host reads (in MB/s): "
           << "Transfer Rate = (Total Bytes) / (Total Time in us),\n" ;
      fout << "COLUMN:<html>Average<br>Bandwidth<br>Utilization (%)</html>,"
           << "float,Average bandwidth of host reads: "
           << "Bandwidth Utilization (%) = (100 * Transfer Rate) / (Max. Theoretical Rate),\n" ;
      fout << "COLUMN:<html>Maximum<br>Time (ms)</html>,float,"
           << "Maximum time of a single host read,\n" ;
      fout << "COLUMN:<html>Minimum<br>Time (ms)</html>,float,"
           << "Minimum time of a single host read,\n" ;
      fout << "COLUMN:<html>Total<br>Time (ms)</html>,float,"
           << "Combined time of all host reads,\n" ;
      fout << "COLUMN:<html>Average<br>Time (ms)</html>,float,"
           << "Average of read durations (in ms),\n" ;
    }

    for (auto read : hostReads)
    {
      auto contextAndDevice = read.first ;
      auto deviceId = contextAndDevice.second ;
      auto stats = read.second ;

      fout << "ENTRY:" << stats.count << "," ;
      fout << ((double)(stats.maxSize) / one_thousand) << "," ;
      fout << ((double)(stats.minSize) / one_thousand) << "," ;
      fout << ((double)(stats.averageSize) / one_thousand) << "," ;
      if (getFlowMode() == HW) {
        auto totalTimeInS  = (double)(stats.totalTime / one_billion);
        auto totalSizeInMB = (double)(stats.totalSize / one_million);
        double transferRate  = totalSizeInMB / totalTimeInS; 
	double maxReadBW =
	  (db->getStaticInfo()).getMaxReadBW(deviceId) ;
	double aveBWUtil = (one_hundred * transferRate) / maxReadBW ;

	fout << transferRate << "," ;
	fout << aveBWUtil << "," ;
	fout << (stats.maxTime / one_million) << "," ;
	fout << (stats.minTime / one_million) << "," ;
	fout << (stats.totalTime / one_million) << "," ;
	fout << (stats.averageTime / one_million) << "," ;
      }
      fout << "\n" ;
    }
  }

  void SummaryWriter::writeHostWritesToGlobalMemory()
  {
    std::map<std::pair<uint64_t, uint64_t>, BufferStatistics> hostWrites =
      (db->getStats()).getHostWrites() ;
    if (hostWrites.size() == 0) return ;

    fout << "TITLE:Host Writes to Global Memory\n" ;
    fout << "SECTION:Host Data Transfers,Host Writes to Global Memory\n" ;
    fout << "COLUMN:<html>Number<br>of Writes</html>,int,"
         << "Number of host writes,\n" ;
    fout << "COLUMN:<html>Maximum<br>Buffer<br>Size (KB)</html>,float,"
         << "Maximum buffer size of host writes,\n";
    fout << "COLUMN:<html>Minimum<br>Buffer<br>Size (KB)</html>,float,"
         << "Minimum buffer size of host writes,\n";
    fout << "COLUMN:<html>Average<br>Buffer<br>Size (KB)</html>,float,"
         << "Average buffer size of host writes: "
         << "Average Size = (Total KB) / (Number of Transfers),\n";

    if (getFlowMode() == HW) {
      fout << "COLUMN:<html>Transfer<br>Rate (MB/s)</html>,float,"
           << "Rate of host writes (in MB/s): "
           << "Transfer Rate = (Total Bytes) / (Total Time in us),\n" ;
      fout << "COLUMN:<html>Average<br>Bandwidth<br>Utilization (%)</html>,"
           << "float,Average bandwidth of host writes: "
           << "Bandwidth Utilization (%) = (100 * Transfer Rate) / (Max. Theoretical Rate),\n" ;
      fout << "COLUMN:<html>Maximum<br>Time (ms)</html>,float,"
           << "Maximum time of a single host write,\n" ;
      fout << "COLUMN:<html>Minimum<br>Time (ms)</html>,float,"
           << "Minimum time of a single host write,\n" ;
      fout << "COLUMN:<html>Total<br>Time (ms)</html>,float,"
           << "Combined time of all host write,\n" ;
      fout << "COLUMN:<html>Average<br>Time (ms)</html>,float,"
           << "Average of write durations (in ms),\n" ;
    }

    for (auto write : hostWrites)
    {
      auto contextAndDevice = write.first ;
      auto deviceId = contextAndDevice.second ;
      auto stats = write.second ;

      fout << "ENTRY:" << stats.count << "," ;
      fout << ((double)(stats.maxSize) / one_thousand) << "," ;
      fout << ((double)(stats.minSize) / one_thousand) << "," ;
      fout << ((double)(stats.averageSize) / one_thousand) << "," ;
      if (getFlowMode() == HW) {
        auto totalTimeInS  = (double)(stats.totalTime / one_billion);
        auto totalSizeInMB = (double)(stats.totalSize / one_million);
        double transferRate  = totalSizeInMB / totalTimeInS; 
	double maxReadBW =
	  (db->getStaticInfo()).getMaxReadBW(deviceId) ;
	double aveBWUtil = (one_hundred * transferRate) / maxReadBW ;

	fout << transferRate << "," ;
	fout << aveBWUtil << "," ;
	fout << (stats.maxTime / one_million) << "," ;
	fout << (stats.minTime / one_million) << "," ;
	fout << (stats.totalTime / one_million) << "," ;
	fout << (stats.averageTime / one_million) << "," ;
      }
      fout << "\n" ;
    }
  }

  void SummaryWriter::writeStreamDataTransfers()
  {
    std::vector<DeviceInfo*> infos = db->getStaticInfo().getDeviceInfos() ;
    
    bool printTable = false ;
    for (auto device : infos) {
      for (auto xclbin : device->loadedXclbins) {
        xclCounterResults values =
          db->getDynamicInfo().getCounterResults(device->deviceId,
                                                 xclbin->uuid) ;
        for (auto cu : xclbin->cus) {
          std::vector<uint32_t>* asmMonitors = (cu.second)->getASMs() ;
          
          for (auto asmMonitorId : (*asmMonitors)) {
            if (values.StrNumTranx[asmMonitorId] != 0) {
              printTable = true ;
              break ;
            }
	  }
          if (printTable) break ;
	}
        if (printTable) break ;
      }
      if (printTable) break ;
    }
    if (!printTable) return ;

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
    
    for (auto device : infos) 
    {
      for (auto xclbin : device->loadedXclbins)
      {
        xclCounterResults values = (db->getDynamicInfo()).getCounterResults(device->deviceId, xclbin->uuid) ;
        for (auto cu : xclbin->cus)
        {
          std::vector<uint32_t>* asmMonitors = (cu.second)->getASMs() ;
          
          for (auto asmMonitorId : (*asmMonitors))
          {
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
            double transferRate = (transferTime == zero) ? 0 : values.StrDataBytes[asmMonitorId] / transferTime ;
            
            double linkStarve = (0 == busyCycles) ? 0 : 
                (double)(values.StrStarveCycles[asmMonitorId]) / (double)(busyCycles) * one_hundred ;
            double linkStall = (0 == busyCycles) ? 0 : 
                (double)(values.StrStallCycles[asmMonitorId]) / (double)(busyCycles) * one_hundred ;
            double linkUtil = one_hundred - linkStarve - linkStall ;
            double avgSizeInKB = ((values.StrDataBytes[asmMonitorId] / numTranx)) / one_thousand;
            
            fout << device->getUniqueDeviceName() << ","
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

  void SummaryWriter::writeDataTransferDMA()
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
	      (double)(totalWriteBusyCycles) / (one_thousand * xclbin->clockRateMHz);
	    double writeTransferRate = (totalWriteTime == zero) ? 0 :
	      (double)(values.WriteBytes[AIMIndex]) / (one_thousand * totalWriteTime);

	    fout << device->getUniqueDeviceName() << ","
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

	    fout << ((double)(values.WriteBytes[AIMIndex] / one_million)) << "," ;

	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << (totalWriteTime / one_million) << "," ;
	    }
	    fout << ((double)(values.WriteBytes[AIMIndex]) / (double)(values.WriteTranx[AIMIndex])) / one_thousand << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," << std::endl ;
	    }
	    else
	    {
	      fout << ((one_thousand * values.WriteLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.WriteTranx[AIMIndex]) << "," << std::endl ;
	    }
	  }
	  if (values.ReadTranx[AIMIndex] > 0)
	  {
	    uint64_t totalReadBusyCycles = values.ReadBusyCycles[AIMIndex] ;
	    double totalReadTime =
	      (double)(totalReadBusyCycles) / (one_thousand * xclbin->clockRateMHz);
	    double readTransferRate = (totalReadTime == zero) ? 0 :
	      (double)(values.ReadBytes[AIMIndex]) / (one_thousand * totalReadTime);

	    fout << device->getUniqueDeviceName() << ","
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

	    fout << ((double)(values.ReadBytes[AIMIndex] / one_million)) << "," ;

	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," ;
	    }
	    else
	    {
	      fout << (totalReadTime / one_million) << "," ;
	    }
	    fout << ((double)(values.ReadBytes[AIMIndex]) / (double)(values.ReadTranx[AIMIndex])) / one_thousand << "," ;
	    if (getFlowMode() == HW_EMU)
	    {
	      fout << "N/A" << "," << std::endl ;
	    }
	    else
	    {
	      fout << ((one_thousand * values.ReadLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.ReadTranx[AIMIndex]) << "," << std::endl ;
	    }
	  }
	}
	++AIMIndex ;
      }
      }
    }
  }

  void SummaryWriter::writeDataTransferDMABypass()
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

    printTable = false ;
    for (auto device : infos) {
      for (auto xclbin : device->loadedXclbins) {
        uint64_t AIMIndex = 0 ;
        for (auto monitor : device->currentXclbin()->aimList) {
          if (monitor->name.find("Peer to Peer") != std::string::npos) {
            // This is the monitor we're looking for
            xclCounterResults values =
              db->getDynamicInfo().getCounterResults(device->deviceId,
                                                     xclbin->uuid) ;
            if (values.WriteTranx[AIMIndex] > 0 ||
                values.ReadTranx[AIMIndex] > 0) {
              printTable = true ;
              break ;
	    }
	  }
          ++AIMIndex ;
	}
        if (printTable) break ;
      }
      if (printTable) break ;
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

    for (auto device : infos) {
      for (auto xclbin : device->loadedXclbins) {
        uint64_t AIMIndex = 0 ;
        for (auto monitor : device->currentXclbin()->aimList) {
          if (monitor->name.find("Peer to Peer") != std::string::npos) {
            // This is the monitor we are looking for
	    xclCounterResults values =
	      db->getDynamicInfo().getCounterResults(device->deviceId,
                                                     xclbin->uuid) ;
            if (values.WriteTranx[AIMIndex] > 0) {
              uint64_t totalWriteBusyCycles = values.WriteBusyCycles[AIMIndex] ;
	      double totalWriteTime =
	        (double)(totalWriteBusyCycles) / (one_thousand * xclbin->clockRateMHz);
	      double writeTransferRate = (totalWriteTime == zero) ? 0 :
	        (double)(values.WriteBytes[AIMIndex]) / (one_thousand * totalWriteTime);

              fout << device->getUniqueDeviceName() << "," << "WRITE" << ","
                   << values.WriteTranx[AIMIndex] << "," ;
	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," ;
	      }
	      else {
                fout << writeTransferRate << "," ;
	      }

	      fout << ((double)(values.WriteBytes[AIMIndex] / one_million)) << "," ;

	      if (getFlowMode() == HW_EMU) {
  	        fout << "N/A" << "," ;
	      }
	      else {
 	        fout << (totalWriteTime / one_million) << "," ;
	      }
	      fout << ((double)(values.WriteBytes[AIMIndex]) / (double)(values.WriteTranx[AIMIndex])) / one_thousand << "," ;
	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," << std::endl ;
	      }
	      else {
	        fout << ((one_thousand * values.WriteLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.WriteTranx[AIMIndex]) << "," << std::endl ;
	      }
	    }
	    if (values.ReadTranx[AIMIndex] > 0) {
 	      uint64_t totalReadBusyCycles = values.ReadBusyCycles[AIMIndex] ;
	      double totalReadTime =
	        (double)(totalReadBusyCycles) / (one_thousand * xclbin->clockRateMHz);
	      double readTransferRate = (totalReadTime == zero) ? 0 :
	        (double)(values.ReadBytes[AIMIndex]) / (one_thousand * totalReadTime);

	      fout << device->getUniqueDeviceName() << ","
	 	   << "READ" << ","
		   << values.ReadTranx[AIMIndex] << "," ;
	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," ;
	      }
	      else {
	        fout << readTransferRate << "," ;
	      }

	      fout << ((double)(values.ReadBytes[AIMIndex] / one_million)) << "," ;

	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," ;
	      }
	      else {
	        fout << (totalReadTime / one_million) << "," ;
	      }
	      fout << ((double)(values.ReadBytes[AIMIndex]) / (double)(values.ReadTranx[AIMIndex])) / one_thousand << "," ;
	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," << std::endl ;
	      }
	      else {
	        fout << ((one_thousand * values.ReadLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.ReadTranx[AIMIndex]) << "," << std::endl ;
	      }
	    }
	  }
	  ++AIMIndex ;
        }
      }
    }
  }

  void SummaryWriter::writeDataTransferMemory()
  {
    std::vector<DeviceInfo*> infos = db->getStaticInfo().getDeviceInfos() ;
    if (infos.size() == 0) return ;

    bool hasMemoryMonitors = false ;
    for (auto device : infos) {
      for (auto xclbin : device->loadedXclbins) {
        hasMemoryMonitors |= xclbin->hasMemoryAIM ;
        if (hasMemoryMonitors) break ;
      }
      if (hasMemoryMonitors) break ;
    }

    if (!hasMemoryMonitors) return ;

    fout << "TITLE:Data Transfer: Memory Resource\n" ;
    fout << "SECTION:Memory Data Transfers,Memory Bank Data Transfer\n" ;
    fout << "COLUMN:<html>Device</html>,string,Name of device\n" ;
    fout << "COLUMN:<html>Memory<br>Resource</html>,string,"
         << "Memory resource on the device\n" ;
    fout << "COLUMN:<html>Transfer<br>Type</html>,string,"
         << "Read from this memory resource or write to this memory resource\n";
    fout << "COLUMN:<html>Number<br>of Transfers</html>,int,"
         << "Number of data transfers\n" ;
    fout << "COLUMN:<html>Transfer<br>Rate (MB/s)</html>,float,"
         << "Total transfer rate = (Total Data Transfer) / (Total active time)\n" ;
    fout << "COLUMN:<html>Total<br>Data<br>Transfer (MB)</html>,float,"
         << "Total data read and written on this memory resource\n" ;
    fout << "COLUMN:<html>Average<br>Size (KB)</html>,float,"
         << "Average Size in KB of each transaction\n" ;
    fout << "COLUMN:<html>Average<br>Latency (ns)</html>,float,"
         << "Average latency in ns of each transaction\n" ;

    for (auto device : infos) {
      for (auto xclbin : device->loadedXclbins) {
        uint64_t AIMIndex = 0;
        xclCounterResults values =
          db->getDynamicInfo().getCounterResults(device->deviceId,
                                                 xclbin->uuid) ;
        for (auto aim : xclbin->aimList) {
          auto loc = aim->name.find("memory_subsystem") ;
          if (loc != std::string::npos) {
	    std::string memoryResource = aim->name.substr(loc + 16) ;

	    if (values.ReadTranx[AIMIndex] > 0) {
 	      uint64_t totalReadBusyCycles = values.ReadBusyCycles[AIMIndex] ;
	      double totalReadTime =
	        (double)(totalReadBusyCycles) / (one_thousand * xclbin->clockRateMHz);
	      double readTransferRate = (totalReadTime == zero) ? 0 :
	        (double)(values.ReadBytes[AIMIndex]) / (one_thousand * totalReadTime);

              fout << "ENTRY:" ;
              fout << device->getUniqueDeviceName() << "," ;
              fout << memoryResource << "," ;
              fout << "READ," ;
              fout << values.ReadTranx[AIMIndex] << "," ;
              fout << readTransferRate << "," ;
              fout << ((double)(values.ReadBytes[AIMIndex] / one_million)) << "," ;
              fout << ((double)(values.ReadBytes[AIMIndex]) / (double)(values.ReadTranx[AIMIndex])) / one_thousand << "," ;
              fout << ((one_thousand * values.ReadLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.ReadTranx[AIMIndex]) << ",\n" ;
            }
            if (values.WriteTranx[AIMIndex] > 0) {
 	      uint64_t totalWriteBusyCycles = values.WriteBusyCycles[AIMIndex] ;
	      double totalWriteTime =
	        (double)(totalWriteBusyCycles) / (one_thousand * xclbin->clockRateMHz);
	      double writeTransferRate = (totalWriteTime == zero) ? 0 :
	        (double)(values.WriteBytes[AIMIndex]) / (one_thousand * totalWriteTime);
              fout << "ENTRY:" ;
              fout << device->getUniqueDeviceName() << "," ;
              fout << memoryResource << "," ;
              fout << "WRITE," ;
              fout << values.WriteTranx[AIMIndex] << "," ;
              fout << writeTransferRate << "," ;
              fout << ((double)(values.WriteBytes[AIMIndex] / one_million)) << "," ;
              fout << ((double)(values.WriteBytes[AIMIndex]) / (double)(values.WriteTranx[AIMIndex])) / one_thousand << "," ;
              fout << ((one_thousand * values.WriteLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.WriteTranx[AIMIndex]) << ",\n" ;
            }
          }
          ++AIMIndex ;
        }
      }
    }

  }

  void SummaryWriter::writeDataTransferGlobalMemoryToGlobalMemory()
  {
    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;

    if (infos.size() == 0) return ;
    bool printTable = false ;
    for (auto device : infos) {
      for (auto xclbin : device->loadedXclbins) {
        uint64_t AIMIndex = 0 ;
        for (auto monitor : xclbin->aimList) {
          if (monitor->name.find("Memory to Memory") != std::string::npos) {
            xclCounterResults values =
	      (db->getDynamicInfo()).getCounterResults(device->deviceId,
                                                       xclbin->uuid) ;
            if (values.WriteTranx[AIMIndex] > 0 ||
                values.ReadTranx[AIMIndex] > 0) {
              printTable = true ;
              break ;
	    }
	  }
          ++AIMIndex ;
	}
        if (printTable) break ;
      }
      if (printTable) break ;
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

    for (auto device : infos) {
      for (auto xclbin : device->loadedXclbins) {
        uint64_t AIMIndex = 0 ;
        for (auto monitor : xclbin->aimList) {
	  if (monitor->name.find("Memory to Memory") != std::string::npos) {
            // This is the monitor we are looking for
	    xclCounterResults values =
	      (db->getDynamicInfo()).getCounterResults(device->deviceId,
                                                       xclbin->uuid) ;
            if (values.WriteTranx[AIMIndex] > 0) {
	      uint64_t totalWriteBusyCycles = values.WriteBusyCycles[AIMIndex] ;
	      double totalWriteTime =
	        (double)(totalWriteBusyCycles) / (one_thousand*xclbin->clockRateMHz);
              double writeTransferRate = (totalWriteTime == zero) ? 0 :
	        (double)(values.WriteBytes[AIMIndex]) / (one_thousand*totalWriteTime);

              fout << device->getUniqueDeviceName() << "," << "WRITE" << ","
                   << values.WriteTranx[AIMIndex] << "," ;
	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," ;
              }
	      else {
	        fout << writeTransferRate << "," ;
	      }
	      fout << ((double)(values.WriteBytes[AIMIndex] / one_million)) << "," ;
              if (getFlowMode() == HW_EMU) {
                fout << "N/A" << "," ;
              }
	      else {
	        fout << (totalWriteTime / one_million) << "," ;
	      }
	      fout << ((double)(values.WriteBytes[AIMIndex]) / (double)(values.WriteTranx[AIMIndex])) / one_thousand << "," ;
	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," << std::endl ;
	      }
	      else {
	        fout << ((one_thousand * values.WriteLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.WriteTranx[AIMIndex]) << "," << std::endl ;
	      }
	    }
	    if (values.ReadTranx[AIMIndex] > 0) {
  	      uint64_t totalReadBusyCycles = values.ReadBusyCycles[AIMIndex] ;
	      double totalReadTime =
	        (double)(totalReadBusyCycles) / (one_thousand * xclbin->clockRateMHz);
	      double readTransferRate = (totalReadTime == zero) ? 0 :
	        (double)(values.ReadBytes[AIMIndex]) / (one_thousand * totalReadTime);

	      fout << device->getUniqueDeviceName() << ","
	  	   << "READ" << ","
		   << values.ReadTranx[AIMIndex] << "," ;
	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," ;
	      }
	      else {
	        fout << readTransferRate << "," ;
	      }

	      fout << ((double)(values.ReadBytes[AIMIndex] / one_million)) << "," ;

	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," ;
	      }
	      else {
	        fout << (totalReadTime / one_million) << "," ;
	      }
	      fout << ((double)(values.ReadBytes[AIMIndex]) / (double)(values.ReadTranx[AIMIndex])) / one_thousand << "," ;
	      if (getFlowMode() == HW_EMU) {
	        fout << "N/A" << "," << std::endl ;
	      }
	      else {
	        fout << ((one_thousand * values.ReadLatency[AIMIndex]) / xclbin->clockRateMHz) / (values.ReadTranx[AIMIndex]) << "," << std::endl ;
	      }
	    }
	  }
	  ++AIMIndex ;
        }
      }
    }
  }

  void SummaryWriter::writeDataTransferKernelsToGlobalMemory()
  {
    // Only print out if information exists
    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;
    if (infos.size() == 0) return ;

    bool monitorsExist = false ;
    for (auto device : infos) {
      for (auto xclbin : device->getLoadedXclbins()) {
        for (auto aim : xclbin->aimList) {
          if (aim->cuIndex != -1) {
            monitorsExist = true ;
            break ;
          }
          // else a floating monitor or shell monitor not reported in this table
        }
        if (monitorsExist) break ;
      }
      if (monitorsExist) break ;
    }

    if (!monitorsExist) return ;

    // Caption
    fout << "Data Transfer: Kernels to Global Memory\n" ;

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
	    (double)(totalReadBusyCycles) / (one_thousand * xclbin->clockRateMHz) ;
	  double totalWriteTime =
	    (double)(totalWriteBusyCycles) / (one_thousand * xclbin->clockRateMHz) ;

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
	    double transferRate = (totalWriteTime == zero) ? 0 :
	      (double)(values.WriteBytes[monitorId]) / (one_thousand * totalWriteTime);
	    double aveBW =
	      (one_hundred * transferRate) / xclbin->maxWriteBW ;
	    if (aveBW > one_hundred) aveBW = one_hundred ;
	    auto aveLatency =
              static_cast<double>(values.WriteLatency[monitorId]) /
              static_cast<double>(writeTranx) ;

	    fout << device->getUniqueDeviceName() << ","
		 << xclbin->cus[monitor->cuIndex]->getName() << "/"
		 << portName << ","
		 << (monitor->args) << ","
		 << memoryName << ","
		 << "WRITE" << ","
		 << writeTranx << ","
		 << transferRate << ","
		 << aveBW << ","
		 << (double)(values.WriteBytes[monitorId] / writeTranx) / one_thousand << ","
		 << aveLatency << ",\n" ;
	  }
	  if (readTranx > 0) {
	      double transferRate = (totalReadTime == zero) ? 0 :
		(double)(values.ReadBytes[monitorId]) / (one_thousand * totalReadTime);
	      double aveBW =
		(one_hundred * transferRate) / xclbin->maxReadBW ;
	      if (aveBW > one_hundred) aveBW = one_hundred ;
              auto aveLatency =
                static_cast<double>(values.ReadLatency[monitorId]) /
                static_cast<double>(readTranx) ;

	      fout << device->getUniqueDeviceName() << ","
		   << xclbin->cus[monitor->cuIndex]->getName() << "/"
		   << portName << ","
		   << (monitor->args) << ","
		   << memoryName << ","
		   << "READ" << ","
		   << readTranx << ","
		   << transferRate << ","
		   << aveBW << ","
		   << (double)(values.ReadBytes[monitorId] / readTranx) / one_thousand << ","
		   << aveLatency << ",\n" ;
	  }
	  ++monitorId ;
	}
      }
    }
  }

  void SummaryWriter::writeTopDataTransferKernelAndGlobal()
  {
    std::vector<DeviceInfo*> infos = (db->getStaticInfo()).getDeviceInfos() ;
    if (infos.size() == 0) return ;

    bool monitorsExist = false ;
    for (auto device : infos) {
      for (auto xclbin : device->getLoadedXclbins()) {
        for (auto aim : xclbin->aimList) {
          if (aim->cuIndex != -1) {
            monitorsExist = true ;
            break ;
          }
          // else a floating monitor or shell monitor not reported in this table
        }
        if (monitorsExist) break ;
      }
      if (monitorsExist) break ;
    }

    if (!monitorsExist) return ;

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
	      transferEfficiency = (one_hundred * aveBytesPerTransfer) / 4096 ; 
	      totalDataTransfer = totalReadBytes + totalWriteBytes ;
	      auto totalBusyCycles =
		values.ReadBusyCycles[AIMIndex]+values.WriteBusyCycles[AIMIndex];
	      double totalTimeMSec = 
		(double)(totalBusyCycles) /(one_thousand * xclbin->clockRateMHz) ;
	      totalTransferRate =
		(totalTimeMSec == 0) ? zero :
		(double)(totalDataTransfer) / (one_thousand * totalTimeMSec) ;
	    }
	  }

	  // Verify that this CU actually had some data transfers registered
	  if (computeUnitName != "" && numTransfers != 0) {
	    fout << device->getUniqueDeviceName() << ","
		 << computeUnitName << ","
		 << numTransfers << ","
		 << aveBytesPerTransfer << ","
		 << transferEfficiency << ","
		 << (double)(totalDataTransfer) / one_million << ","
		 << (double)(totalWriteBytes) / one_million << ","
		 << (double)(totalReadBytes) / one_million << ","
		 << totalTransferRate << "," << std::endl ;
	  }
	}
      }
    }
  }

  void SummaryWriter::writeTopSyncReads()
  {
    if (db->getStats().getTopHostReads().size() == 0)
      return ;

    fout << "TITLE:Top Memory Reads: Host from Global Memory\n" ;
    fout << "SECTION:Host Data Transfers,Top Memory Reads\n" ;
    fout << "COLUMN:<html>Start<br>Time (ms)</html>,float,"
         << "Start time of read transfer (in ms),\n" ;
    fout << "COLUMN:<html>Buffer<br>Size (KB)</html>,float,"
         << "Size of read transfer (in KB),\n" ;
    if (getFlowMode() == HW) {
      fout << "COLUMN:<html>Duration (ms)</html>,float,"
           << "Duration of read transfer (in ms),\n" ;
      fout << "COLUMN:<html>Reading<br>Rate (MB/s)</html>,float,"
           << "Transfer rate of reads: Reading Rate = (Buffer Size) / (Duration),\n";
    }

    for (auto& iter : db->getStats().getTopHostReads()) {
      fout << "ENTRY:" ;
      fout << (double)((iter).startTime) / one_million << "," ;
      fout << (double)((iter).size) / one_thousand << "," ;
      if (getFlowMode() == HW) {
        double durationMS = (double)((iter).duration) / one_million ;
        double rate = ((double)((iter).size) / one_thousand) * durationMS ;
        fout << durationMS << "," ;
        fout << rate << "," ;
      }
      fout << "\n" ;
    }    
  }

  void SummaryWriter::writeTopSyncWrites()
  {
    if (db->getStats().getTopHostWrites().size() == 0)
      return ;

    fout << "TITLE:Top Memory Writes: Host to Global Memory\n" ;
    fout << "SECTION:Host Data Transfers,Top Memory Writes\n" ;
    fout << "COLUMN:<html>Start<br>Time (ms)</html>,float,"
         << "Start time of write transfer (in ms),\n" ;
    fout << "COLUMN:<html>Buffer<br>Size (KB)</html>,float,"
         << "Size of write transfer (in KB),\n" ;
    if (getFlowMode() == HW) {
      fout << "COLUMN:<html>Duration (ms)</html>,float,"
           << "Duration of write transfer (in ms),\n" ;
      fout << "COLUMN:<html>Writing<br>Rate (MB/s)</html>,float,"
           << "Transfer rate of writes: Writing Rate = (Buffer Size) / (Duration),\n";
    }

    for (auto& iter : db->getStats().getTopHostWrites()) {
      fout << "ENTRY:" ;
      fout << (double)((iter).startTime) / one_million << "," ;
      fout << (double)((iter).size) / one_thousand << "," ;
      if (getFlowMode() == HW) {
        double durationMS = (double)((iter).duration) / one_million ;
        double rate = ((double)((iter).size) / one_thousand) * durationMS ;
        fout << durationMS << "," ;
        fout << rate << "," ;
      }
      fout << "\n" ;
    }
  }

  void SummaryWriter::writeUserLevelEvents()
  {
    if (!db->getStats().eventInformationPresent()) return ;

    fout << "User Level Events\n" ;
    fout << "Label,Count,\n" ;

    std::map<std::string, uint64_t>& counts = db->getStats().getEventCounts() ;
    for (auto iter : counts) {
      fout << iter.first << "," << iter.second << ",\n" ;
    }
  }

  void SummaryWriter::writeUserLevelRanges()
  {
    if (!db->getStats().rangeInformationPresent()) return ;

    fout << "User Level Ranges\n" ;
    fout << "Label,Tooltip,Count,Min Duration (ms),Max Duration (ms),"
         << "Total Time Duration (ms),Average Duration (ms),\n" ;

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
	   << (double)minDurations[iter.first] / one_million << ","
	   << (double)maxDurations[iter.first] / one_million << ","
	   << (double)totalDurations[iter.first] / one_million<< ","
	   << ((double)totalDurations[iter.first]/(double)(iter.second)) / one_million << ","
	   << std::endl ;
    }
  }

  bool SummaryWriter::write(bool /*openNewFile*/)
  {
    // Every summary has to have a header
    writeHeader() ;                                      fout << "\n" ;

    if (db->infoAvailable(info::opencl_counters)) {
      writeOpenCLAPICalls() ;                            fout << "\n" ;
      writeKernelExecutionSummary() ;                    fout << "\n" ;
      writeTopKernelExecution() ;                        fout << "\n" ;
      writeTopMemoryWrites() ;                           fout << "\n" ;
      writeTopMemoryReads() ;                            fout << "\n" ;
      if (getFlowMode() == SW_EMU) {
        writeSoftwareEmulationComputeUnitUtilization() ; fout << "\n" ;
      }
      else if (db->infoAvailable(info::device_offload)) {
        // OpenCL specific device tables
        writeDataTransferHostToGlobalMemory() ;          fout << "\n" ;
      }
    }

    // Generic device tables
    if (db->infoAvailable(info::device_offload)) {
      if (getFlowMode() != SW_EMU) {
        writeComputeUnitUtilization() ;                  fout << "\n" ;
      }
      writeDataTransferDMA() ;                           fout << "\n" ;
      writeDataTransferDMABypass() ;                     fout << "\n" ;
      writeDataTransferMemory() ;                        fout << "\n" ;
      writeStreamDataTransfers() ;                       fout << "\n" ;
      writeDataTransferKernelsToGlobalMemory() ;         fout << "\n" ;
      writeTopDataTransferKernelAndGlobal() ;            fout << "\n" ;
      writeDataTransferGlobalMemoryToGlobalMemory() ;    fout << "\n" ;
      writeComputeUnitStallInformation() ;               fout << "\n" ;
    }

    if (db->infoAvailable(info::user)) {
      writeUserLevelEvents() ;                           fout << "\n" ;
      writeUserLevelRanges() ;                           fout << "\n" ;
    }

    if (db->infoAvailable(info::native)) {
      writeNativeAPICalls() ;                            fout << "\n" ;
      writeHostReadsFromGlobalMemory() ;                 fout << "\n" ;
      writeHostWritesToGlobalMemory() ;                  fout << "\n" ;
      writeTopSyncReads() ;                              fout << "\n" ; 
      writeTopSyncWrites() ;                             fout << "\n" ;
    }

    if (db->infoAvailable(info::hal)) {
      writeHALAPICalls() ;                               fout << "\n" ;
    }

    // Generate all the applicable guidance rules
    guidance.write(db, fout) ;

    fout.flush() ;
    return true ;
  }

} // end namespace xdp
