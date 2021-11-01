#ifndef __AIE_STUBS_H__
#define __AIE_STUBS_H__

#include <sstream>

#include "core/include/experimental/xrt_aie.h"
#include "core/include/experimental/xrt_bo.h"
#include "core/include/xcl_graph.h"

#include "core/include/experimental/xrt_device.h"

//////////// START OF XCL XRT AIE APIS ///////////////////////////

static std::string function_not_implemented(const char* funcName) {
  std::stringstream excepStreamMsg;
  excepStreamMsg << funcName << " is not supported for hw and hw_emu PCIe targets";
  std::string exceptionStr(excepStreamMsg.str());
  return exceptionStr;
}

void*
xclGraphOpen(xclDeviceHandle /*handle*/, const uuid_t /*xclbin_uuid*/, const char* /*graph*/, xrt::graph::access_mode /*am*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

void
xclGraphClose(xclGraphHandle /*ghl*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGraphReset(xclGraphHandle /*ghl*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

uint64_t
xclGraphTimeStamp(xclGraphHandle /*ghl*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGraphRun(xclGraphHandle /*gh*/, int /*iterations*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGraphWaitDone(xclGraphHandle /*gh*/, int /*timeoutMilliSec*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGraphWait(xclGraphHandle /*gh*/, uint64_t /*cycle*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGraphSuspend(xclGraphHandle /*gh*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGraphResume(xclGraphHandle /*gh*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGraphEnd(xclGraphHandle /*gh*/, uint64_t /*cycle*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGraphUpdateRTP(xclGraphHandle /*ghdl*/, const char* /*port*/, const char* /*buffer*/, size_t /*size*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGraphReadRTP(xclGraphHandle /*ghdl*/, const char* /*port*/, char* /*buffer*/, size_t /*size*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclAIEOpenContext(xclDeviceHandle /*handle*/, xrt::aie::access_mode /*am*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclSyncBOAIE(xclDeviceHandle /*handle*/, xrt::bo& /*bo*/, const char* /*gmioName*/, enum xclBOSyncDirection /*dir*/, size_t /*size*/, size_t /*offset*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclResetAIEArray(xclDeviceHandle /*handle*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclSyncBOAIENB(xclDeviceHandle /*handle*/, xrt::bo& /*bo*/, const char* /*gmioName*/, enum xclBOSyncDirection /*dir*/, size_t /*size*/, size_t /*offset*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclGMIOWait(xclDeviceHandle /*handle*/, const char* /*gmioName*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclStartProfiling(xclDeviceHandle /*handle*/, int /*option*/, const char* /*port1Name*/, const char* /*port2Name*/, uint32_t /*value*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

uint64_t
xclReadProfiling(xclDeviceHandle /*handle*/, int /*phdl*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclStopProfiling(xclDeviceHandle /*handle*/, int /*phdl*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

int
xclLoadXclBinMeta(xclDeviceHandle /*handle*/, const xclBin* /*buffer*/)
{
  throw std::runtime_error(function_not_implemented(__func__));
}

#endif

//////////// END OF XCL XRT AIE APIS///////////////////////////
