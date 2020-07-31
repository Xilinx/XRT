
#define XDP_SOURCE

#include <vector>
#include <thread>
#include <map>
#include <tuple>

#include "xdp/profile/writer/opencl/opencl_summary_writer.h"
#include "xdp/profile/database/database.h"

namespace xdp {

  OpenCLSummaryWriter::OpenCLSummaryWriter(const char* filename)
    : VPSummaryWriter(filename)
  {
  }

  OpenCLSummaryWriter::~OpenCLSummaryWriter()
  {
  }

  void OpenCLSummaryWriter::writeHeader()
  {
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
	  std::make_tuple<uint64_t, double, double, double>(0,0,0,0) ;

	rows[APIName] = blank ;
      }

      for (auto executionTime : timesOfCalls)
      {
	auto timeTaken = executionTime.second = executionTime.first ;

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
    
  }

  void OpenCLSummaryWriter::writeComputeUnitUtilization()
  {
  }

  void OpenCLSummaryWriter::writeComputeUnitStallInformation()
  {
  }

  void OpenCLSummaryWriter::writeDataTransferHostToGlobalMemory()
  {
  }

  void OpenCLSummaryWriter::writeDataTransferKernelsToGlobalMemory()
  {
  }

  void OpenCLSummaryWriter::writeStreamDataTransfers()
  {
  }

  void OpenCLSummaryWriter::writeDataTransferDMA()
  {
  }

  void OpenCLSummaryWriter::writeDataTransferDMABypass()
  {
  }

  void OpenCLSummaryWriter::writeDataTransferGlobalMemoryToGlobalMemory()
  {
  }

  void OpenCLSummaryWriter::writeTopDataTransferKernelAndGlobal()
  {
  }

  void OpenCLSummaryWriter::write(bool openNewFile)
  {
    writeHeader() ;
    writeAPICallSummary() ;
    writeKernelExecutionSummary() ;
    writeComputeUnitUtilization() ;
    writeComputeUnitStallInformation() ;
    writeDataTransferHostToGlobalMemory() ;
    writeDataTransferKernelsToGlobalMemory() ;
    writeStreamDataTransfers() ;
    writeDataTransferDMA() ;
    writeDataTransferDMABypass() ;
    writeDataTransferGlobalMemoryToGlobalMemory() ;
    writeTopDataTransferKernelAndGlobal() ;

    if (openNewFile)
    {
      switchFiles() ;
    }
  }

} // end namespace xdp
