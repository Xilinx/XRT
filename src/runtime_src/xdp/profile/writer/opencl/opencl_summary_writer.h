
#ifndef OPENCL_SUMMARY_WRITER_DOT_H
#define OPENCL_SUMMARY_WRITER_DOT_H

#include "xdp/profile/writer/vp_base/vp_summary_writer.h"
#include "xdp/config.h"

namespace xdp {

  class OpenCLSummaryWriter : public VPSummaryWriter
  {
  private:
    OpenCLSummaryWriter() = delete ;

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
    void writeGuidance() ;

  public:
    XDP_EXPORT OpenCLSummaryWriter(const char* filename) ;
    XDP_EXPORT ~OpenCLSummaryWriter() ;

    XDP_EXPORT virtual void write(bool openNewFile) ;
  } ;

} // end namespace xdp

#endif
