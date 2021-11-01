#ifndef __AIE_STUBS_H__
#define __AIE_STUBS_H__

#include <sstream>

#include "core/include/experimental/xrt_aie.h"
#include "core/include/experimental/xrt_bo.h"
#include "core/include/xcl_graph.h"

#include "core/include/experimental/xrt_device.h"

//////////// START OF XCL XRT AIE APIS ///////////////////////////

void excepStr(const char* funcName) {
  std::stringstream excepStreamMsg;
  excepStreamMsg << funcName << " is not supported for hw and hw_emu PCIe targets";
  std::string excepStr(excepStreamMsg.str());
  throw std::runtime_error(excepStr);
}

void*
xclGraphOpen(xclDeviceHandle /*handle*/, const uuid_t /*xclbin_uuid*/, const char* /*graph*/, xrt::graph::access_mode /*am*/)
{
  excepStr(__func__);
  return XRT_NULL_HANDLE;
}

void
xclGraphClose(xclGraphHandle /*ghl*/)
{
  excepStr(__func__);  
  return;
}

int
xclGraphReset(xclGraphHandle /*ghl*/)
{
  excepStr(__func__);
  return 0;
}

uint64_t
xclGraphTimeStamp(xclGraphHandle /*ghl*/)
{
  excepStr(__func__);
  return 0;
}

int
xclGraphRun(xclGraphHandle /*gh*/, int /*iterations*/)
{
  excepStr(__func__);
  return 0;
}

int
xclGraphWaitDone(xclGraphHandle /*gh*/, int /*timeoutMilliSec*/)
{
  excepStr(__func__);
  return 0;
}

int
xclGraphWait(xclGraphHandle /*gh*/, uint64_t /*cycle*/)
{
  excepStr(__func__);
  return 0;
}

int
xclGraphSuspend(xclGraphHandle /*gh*/)
{
  excepStr(__func__);
  return 0;
}

int
xclGraphResume(xclGraphHandle /*gh*/)
{
  excepStr(__func__);
  return 0;
}

int
xclGraphEnd(xclGraphHandle /*gh*/, uint64_t /*cycle*/)
{
  excepStr(__func__);
  return 0;
}

int
xclGraphUpdateRTP(xclGraphHandle /*ghdl*/, const char* /*port*/, const char* /*buffer*/, size_t /*size*/)
{
  excepStr(__func__);
  return 0;
}

int
xclGraphReadRTP(xclGraphHandle /*ghdl*/, const char* /*port*/, char* /*buffer*/, size_t /*size*/)
{
  excepStr(__func__);
  return 0;
}

int
xclAIEOpenContext(xclDeviceHandle /*handle*/, xrt::aie::access_mode /*am*/)
{
  excepStr(__func__);
  return 0;
}

int
xclSyncBOAIE(xclDeviceHandle /*handle*/, xrt::bo& /*bo*/, const char* /*gmioName*/, enum xclBOSyncDirection /*dir*/, size_t /*size*/, size_t /*offset*/)
{
  excepStr(__func__);
  return 0;
}

int
xclResetAIEArray(xclDeviceHandle /*handle*/)
{
  excepStr(__func__);
  return 0;
}

int
xclSyncBOAIENB(xclDeviceHandle /*handle*/, xrt::bo& /*bo*/, const char* /*gmioName*/, enum xclBOSyncDirection /*dir*/, size_t /*size*/, size_t /*offset*/)
{
  excepStr(__func__);
  return 0;
}

int
xclGMIOWait(xclDeviceHandle /*handle*/, const char* /*gmioName*/)
{
  excepStr(__func__);
  return 0;
}

int
xclStartProfiling(xclDeviceHandle /*handle*/, int /*option*/, const char* /*port1Name*/, const char* /*port2Name*/, uint32_t /*value*/)
{
  excepStr(__func__);
  return 0;
}

uint64_t
xclReadProfiling(xclDeviceHandle /*handle*/, int /*phdl*/)
{
  excepStr(__func__);
  return 0;
}

int
xclStopProfiling(xclDeviceHandle /*handle*/, int /*phdl*/)
{
  excepStr(__func__);
  return 0;
}

int
xclLoadXclBinMeta(xclDeviceHandle /*handle*/, const xclBin* /*buffer*/)
{
  excepStr(__func__);
  return 0;
}

#endif

//////////// END OF XCL XRT AIE APIS///////////////////////////
