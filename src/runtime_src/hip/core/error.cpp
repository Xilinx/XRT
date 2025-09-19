// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "hip/config.h"
#include "hip/hip_runtime_api.h"
#include "error.h"

namespace xrt::core::hip
{
error::
error()
  : m_last_error(hipSuccess)
{}

error&
error::instance()
{
  thread_local static error err_st;
  return err_st;
}

const char*
error::
get_error_name(hipError_t err)
{
  const std::map<hipError_t, std::string> hip_error_names =
  {
    {hipSuccess, "hipSuccess"},
    {hipErrorInvalidValue, "hipErrorInvalidValue"},
    {hipErrorOutOfMemory, "hipErrorOutOfMemory"},
    {hipErrorMemoryAllocation, "hipErrorMemoryAllocation"},
    {hipErrorNotInitialized, "hipErrorNotInitialized"},
    {hipErrorInitializationError, "hipErrorInitializationError"},
    {hipErrorDeinitialized, "hipErrorDeinitialized"},
    {hipErrorProfilerDisabled, "hipErrorProfilerDisabled"},
    {hipErrorProfilerNotInitialized, "hipErrorProfilerNotInitialized"},
    {hipErrorProfilerAlreadyStarted, "hipErrorProfilerAlreadyStarted"},
    {hipErrorProfilerAlreadyStopped, "hipErrorProfilerAlreadyStopped"},
    {hipErrorInvalidConfiguration, "hipErrorInvalidConfiguration"},
    {hipErrorInvalidPitchValue, "hipErrorInvalidPitchValue"},
    {hipErrorInvalidSymbol, "hipErrorInvalidSymbol"},
    {hipErrorInvalidDevicePointer, "hipErrorInvalidDevicePointer"},
    {hipErrorInvalidMemcpyDirection, "hipErrorInvalidMemcpyDirection"},
    {hipErrorInsufficientDriver, "hipErrorInsufficientDriver"},
    {hipErrorMissingConfiguration, "hipErrorMissingConfiguration"},
    {hipErrorPriorLaunchFailure, "hipErrorPriorLaunchFailure"},
    {hipErrorInvalidDeviceFunction, "hipErrorInvalidDeviceFunction"},
    {hipErrorNoDevice, "hipErrorNoDevice"},
    {hipErrorInvalidDevice, "hipErrorInvalidDevice"},
    {hipErrorInvalidImage, "hipErrorInvalidImage"},
    {hipErrorInvalidContext, "hipErrorInvalidContext"},
    {hipErrorContextAlreadyCurrent, "hipErrorContextAlreadyCurrent"},
    {hipErrorMapFailed, "hipErrorMapFailed"},
    {hipErrorMapBufferObjectFailed, "hipErrorMapBufferObjectFailed"},
    {hipErrorUnmapFailed, "hipErrorUnmapFailed"},
    {hipErrorArrayIsMapped, "hipErrorArrayIsMapped"},
    {hipErrorAlreadyMapped, "hipErrorAlreadyMapped"},
    {hipErrorNoBinaryForGpu, "hipErrorNoBinaryForGpu"},
    {hipErrorAlreadyAcquired, "hipErrorAlreadyAcquired"},
    {hipErrorNotMapped, "hipErrorNotMapped"},
    {hipErrorNotMappedAsArray, "hipErrorNotMappedAsArray"},
    {hipErrorNotMappedAsPointer, "hipErrorNotMappedAsPointer"},
    {hipErrorECCNotCorrectable, "hipErrorECCNotCorrectable"},
    {hipErrorUnsupportedLimit, "hipErrorUnsupportedLimit"},
    {hipErrorContextAlreadyInUse, "hipErrorContextAlreadyInUse"},
    {hipErrorPeerAccessUnsupported, "hipErrorPeerAccessUnsupported"},
    {hipErrorInvalidKernelFile, "hipErrorInvalidKernelFile"},
    {hipErrorInvalidGraphicsContext, "hipErrorInvalidGraphicsContext"},
    {hipErrorInvalidSource, "hipErrorInvalidSource"},
    {hipErrorFileNotFound, "hipErrorFileNotFound"},
    {hipErrorSharedObjectSymbolNotFound, "hipErrorSharedObjectSymbolNotFound"},
    {hipErrorSharedObjectInitFailed, "hipErrorSharedObjectInitFailed"},
    {hipErrorOperatingSystem, "hipErrorOperatingSystem"},
    {hipErrorInvalidHandle, "hipErrorInvalidHandle"},
    {hipErrorInvalidResourceHandle, "hipErrorInvalidResourceHandle"},
    {hipErrorIllegalState, "hipErrorIllegalState"},
    {hipErrorNotFound, "hipErrorNotFound"},
    {hipErrorNotReady, "hipErrorNotReady"},
    {hipErrorIllegalAddress, "hipErrorIllegalAddress"},
    {hipErrorLaunchOutOfResources, "hipErrorLaunchOutOfResources"},
    {hipErrorLaunchTimeOut, "hipErrorLaunchTimeOut"},
    {hipErrorPeerAccessAlreadyEnabled, "hipErrorPeerAccessAlreadyEnabled"},
    {hipErrorPeerAccessNotEnabled, "hipErrorPeerAccessNotEnabled"},
    {hipErrorSetOnActiveProcess, "hipErrorSetOnActiveProcess"},
    {hipErrorContextIsDestroyed, "hipErrorContextIsDestroyed"},
    {hipErrorAssert, "hipErrorAssert"},
    {hipErrorHostMemoryAlreadyRegistered, "hipErrorHostMemoryAlreadyRegistered"},
    {hipErrorHostMemoryNotRegistered, "hipErrorHostMemoryNotRegistered"},
    {hipErrorLaunchFailure, "hipErrorLaunchFailure"},
    {hipErrorCooperativeLaunchTooLarge, "hipErrorCooperativeLaunchTooLarge"},
    {hipErrorNotSupported, "hipErrorNotSupported"},
    {hipErrorStreamCaptureUnsupported, "hipErrorStreamCaptureUnsupported"},
    {hipErrorStreamCaptureInvalidated, "hipErrorStreamCaptureInvalidated"},
    {hipErrorStreamCaptureMerge, "hipErrorStreamCaptureMerge"},
    {hipErrorStreamCaptureUnmatched, "hipErrorStreamCaptureUnmatched"},
    {hipErrorStreamCaptureUnjoined, "hipErrorStreamCaptureUnjoined"},
    {hipErrorStreamCaptureIsolation, "hipErrorStreamCaptureIsolation"},
    {hipErrorStreamCaptureImplicit, "hipErrorStreamCaptureImplicit"},
    {hipErrorCapturedEvent, "hipErrorCapturedEvent"},
    {hipErrorStreamCaptureWrongThread, "hipErrorStreamCaptureWrongThread"},
    {hipErrorGraphExecUpdateFailure, "hipErrorGraphExecUpdateFailure"},
    {hipErrorUnknown, "hipErrorUnknown"},
    {hipErrorRuntimeMemory, "hipErrorRuntimeMemory"},
    {hipErrorRuntimeOther, "hipErrorRuntimeOther"},
    {hipErrorTbd, "hipErrorTbd"},
  };

  const char *error_name = nullptr;

  auto itr = hip_error_names.find(err);
  if (itr != hip_error_names.end())
    error_name = itr->second.c_str();

  return error_name;
}

void
error::
record_local_error(hipError_t err, const std::string& err_str)
{
  auto it = m_local_errors.find(err);
  if (it != m_local_errors.end()) {
    it->second += "; ";
    it->second += err_str;
  }
  else {
    m_local_errors[err] = err_str;
  }
}

void
error::
reset_local_errors()
{
  if (!m_local_errors.empty())
    m_local_errors.clear();
}

const char*
error::
get_local_error_string(hipError_t err)
{
  auto it = m_local_errors.find(err);
  if (it != m_local_errors.end())
    return it->second.c_str();
  else
    return nullptr;
}

//////////////////////////////////////////////
// Class XRT HIP exception
hip_exception::
hip_exception(hipError_t ec, const char* what)
  : m_code(ec), m_what(what)
{}

hipError_t
hip_exception::
value() const noexcept
{
  return m_code;
}

const char*
hip_exception::
what() const noexcept
{
  return m_what.c_str();
}

hipError_t
system_to_hip_error(int serror)
{
  // TODO: there can be extra checking to return better HIP error code based on the
  // value of exception. However, there will be work needs to do in the platform dependent
  // driver, e.g. today, on windows, it only throws std::runtime_error if it fails to
  // allocate buffer with no specific error code, once, it provides error code, we can
  // extend this implementation to provide proper HIP error code.
  const std::map<int, hipError_t> sys_to_hip_error_map =
  {
  #ifdef __linux__
    { ENOENT, hipErrorFileNotFound },
    { EACCES, hipErrorInvalidHandle },
    { ENOMEM, hipErrorOutOfMemory },
    { EINVAL, hipErrorInvalidValue },
    { ENODEV, hipErrorInvalidDevice },
  #endif
  };

  auto it = sys_to_hip_error_map.find(serror);
  if (it == sys_to_hip_error_map.end())
    return hipErrorOperatingSystem;

  return it->second;
}
}
