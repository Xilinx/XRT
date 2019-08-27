#ifndef _XRT_CORE_PROFILE_RESULTS_h
#define _XRT_CORE_PROFILE_RESULTS_h

#if defined(_WIN32)
#ifdef XCL_DRIVER_DLL_EXPORT
#define XCL_DRIVER_DLLESPEC __declspec(dllexport)
#else
#define XCL_DRIVER_DLLESPEC __declspec(dllimport)
#endif
#else
#define XCL_DRIVER_DLLESPEC __attribute__((visibility("default")))
#endif





struct KernelTransferData
{
   char* deviceName;
   char* cuPortName;
   char* argName;
   char* memoryName;
   
   uint64_t totalReadBytes; 
   uint64_t totalReadTranx; 
   uint64_t totalReadLatency; 
   uint64_t totalReadBusyCycles; 
   uint64_t minReadLatency; 
   uint64_t maxReadLatency; 

   uint64_t totalWriteBytes; 
   uint64_t totalWriteTranx; 
   uint64_t totalWriteLatency; 
   uint64_t totalWriteBusyCycles; 
   uint64_t minWriteLatency; 
   uint64_t maxWriteLatency; 

};

struct CuExecData
{
   char* cuName;
   char* kernelName;

   uint64_t cuExecCount;
   uint64_t cuExecCycles;
   uint64_t cuBusyCycles;
   uint64_t cuMaxExecCycles;
   uint64_t cuMinExecCycles;
   uint64_t cuMaxParallelIter;
   uint64_t cuStallExtCycles;
   uint64_t cuStallIntCycles;
   uint64_t cuStallStrCycles;

}; 

struct StreamTransferData
{
  char* deviceName;
  char* masterPortName;
  char* slavePortName;

  uint64_t strmNumTranx;
  uint64_t strmBusyCycles;
  uint64_t strmDataBytes;
  uint64_t strmStallCycles;
  uint64_t strmStarveCycles;
};

struct ProfileResults
{
  unsigned int   numAIM;
  KernelTransferData* kernelTransferData;
  unsigned int   numAM;
  CuExecData* cuExecData;
  unsigned int   numASM;
  StreamTransferData* streamData;

  // total Transfer data ?
};


XCL_DRIVER_DLLESPEC int xclGetProfileResults(xclDeviceHandle, ProfileResults*);
#endif
