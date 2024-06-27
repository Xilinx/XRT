// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#include "hip/config.h"
#include "hip/hip_runtime_api.h"
#include "error.h"

//#define HIP_ERROR_NAME_PAIR(x) {x, #x}
template <typename T>  
constexpr std::pair<T, const char*> HIP_ERROR_NAME_PAIR(T x, const char* name) {  
    return {x, name};  
} 

namespace xrt::core::hip
{
  //we should override clang-tidy warning by adding NOLINT since hip_error_state is non-const parameter
  thread_local static error* hip_error_state = nullptr; //NOLINT

  error::error()
    : m_last_error(hipSuccess)
  {
    if (hip_error_state)
    {
      throw std::runtime_error
        ("Multiple instances of hip error detected, only one per thread\n"
        "can be loaded at any given time.");
    }
    hip_error_state = this;    
  }

  error&
  error::instance()
  {
    if (!hip_error_state)
    {
      thread_local static error err_st;
    }

    if (hip_error_state)
    {
      return *hip_error_state;
    }

    throw std::runtime_error("error singleton is not loaded");  
  }

  //we should override clang-tidy warning by adding NOLINT since hip_error_names is non-const parameter
  static std::map<hipError_t, std::string> hip_error_names = //NOLINT
  {
    HIP_ERROR_NAME_PAIR(hipSuccess, "hipSuccess"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidValue, "hipErrorInvalidValue"),
    HIP_ERROR_NAME_PAIR(hipErrorOutOfMemory, "hipErrorOutOfMemory"),
    HIP_ERROR_NAME_PAIR(hipErrorMemoryAllocation, "hipErrorMemoryAllocation"),
    HIP_ERROR_NAME_PAIR(hipErrorNotInitialized, "hipErrorNotInitialized"),
    HIP_ERROR_NAME_PAIR(hipErrorInitializationError, "hipErrorInitializationError"),
    HIP_ERROR_NAME_PAIR(hipErrorDeinitialized, "hipErrorDeinitialized"),
    HIP_ERROR_NAME_PAIR(hipErrorProfilerDisabled, "hipErrorProfilerDisabled"),
    HIP_ERROR_NAME_PAIR(hipErrorProfilerNotInitialized, "hipErrorProfilerNotInitialized"),
    HIP_ERROR_NAME_PAIR(hipErrorProfilerAlreadyStarted, "hipErrorProfilerAlreadyStarted"),
    HIP_ERROR_NAME_PAIR(hipErrorProfilerAlreadyStopped, "hipErrorProfilerAlreadyStopped"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidConfiguration, "hipErrorInvalidConfiguration"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidPitchValue, "hipErrorInvalidPitchValue"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidSymbol, "hipErrorInvalidSymbol"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidDevicePointer, "hipErrorInvalidDevicePointer"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidMemcpyDirection, "hipErrorInvalidMemcpyDirection"),
    HIP_ERROR_NAME_PAIR(hipErrorInsufficientDriver, "hipErrorInsufficientDriver"),
    HIP_ERROR_NAME_PAIR(hipErrorMissingConfiguration, "hipErrorMissingConfiguration"),
    HIP_ERROR_NAME_PAIR(hipErrorPriorLaunchFailure, "hipErrorPriorLaunchFailure"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidDeviceFunction, "hipErrorInvalidDeviceFunction"),
    HIP_ERROR_NAME_PAIR(hipErrorNoDevice, "hipErrorNoDevice"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidDevice, "hipErrorInvalidDevice"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidImage, "hipErrorInvalidImage"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidContext, "hipErrorInvalidContext"),
    HIP_ERROR_NAME_PAIR(hipErrorContextAlreadyCurrent, "hipErrorContextAlreadyCurrent"),
    HIP_ERROR_NAME_PAIR(hipErrorMapFailed, "hipErrorMapFailed"),
    HIP_ERROR_NAME_PAIR(hipErrorMapBufferObjectFailed, "hipErrorMapBufferObjectFailed"),
    HIP_ERROR_NAME_PAIR(hipErrorUnmapFailed, "hipErrorUnmapFailed"),
    HIP_ERROR_NAME_PAIR(hipErrorArrayIsMapped, "hipErrorArrayIsMapped"),
    HIP_ERROR_NAME_PAIR(hipErrorAlreadyMapped, "hipErrorAlreadyMapped"),
    HIP_ERROR_NAME_PAIR(hipErrorNoBinaryForGpu, "hipErrorNoBinaryForGpu"),
    HIP_ERROR_NAME_PAIR(hipErrorAlreadyAcquired, "hipErrorAlreadyAcquired"),
    HIP_ERROR_NAME_PAIR(hipErrorNotMapped, "hipErrorNotMapped"),
    HIP_ERROR_NAME_PAIR(hipErrorNotMappedAsArray, "hipErrorNotMappedAsArray"),
    HIP_ERROR_NAME_PAIR(hipErrorNotMappedAsPointer, "hipErrorNotMappedAsPointer"),
    HIP_ERROR_NAME_PAIR(hipErrorECCNotCorrectable, "hipErrorECCNotCorrectable"),
    HIP_ERROR_NAME_PAIR(hipErrorUnsupportedLimit, "hipErrorUnsupportedLimit"),
    HIP_ERROR_NAME_PAIR(hipErrorContextAlreadyInUse, "hipErrorContextAlreadyInUse"),
    HIP_ERROR_NAME_PAIR(hipErrorPeerAccessUnsupported, "hipErrorPeerAccessUnsupported"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidKernelFile, "hipErrorInvalidKernelFile"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidGraphicsContext, "hipErrorInvalidGraphicsContext"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidSource, "hipErrorInvalidSource"),
    HIP_ERROR_NAME_PAIR(hipErrorFileNotFound, "hipErrorFileNotFound"),
    HIP_ERROR_NAME_PAIR(hipErrorSharedObjectSymbolNotFound, "hipErrorSharedObjectSymbolNotFound"),
    HIP_ERROR_NAME_PAIR(hipErrorSharedObjectInitFailed, "hipErrorSharedObjectInitFailed"),
    HIP_ERROR_NAME_PAIR(hipErrorOperatingSystem, "hipErrorOperatingSystem"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidHandle, "hipErrorInvalidHandle"),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidResourceHandle, "hipErrorInvalidResourceHandle"),
    HIP_ERROR_NAME_PAIR(hipErrorIllegalState, "hipErrorIllegalState"),
    HIP_ERROR_NAME_PAIR(hipErrorNotFound, "hipErrorNotFound"),
    HIP_ERROR_NAME_PAIR(hipErrorNotReady, "hipErrorNotReady"),
    HIP_ERROR_NAME_PAIR(hipErrorIllegalAddress, "hipErrorIllegalAddress"),
    HIP_ERROR_NAME_PAIR(hipErrorLaunchOutOfResources, "hipErrorLaunchOutOfResources"),
    HIP_ERROR_NAME_PAIR(hipErrorLaunchTimeOut, "hipErrorLaunchTimeOut"),
    HIP_ERROR_NAME_PAIR(hipErrorPeerAccessAlreadyEnabled, "hipErrorPeerAccessAlreadyEnabled"),
    HIP_ERROR_NAME_PAIR(hipErrorPeerAccessNotEnabled, "hipErrorPeerAccessNotEnabled"),
    HIP_ERROR_NAME_PAIR(hipErrorSetOnActiveProcess, "hipErrorSetOnActiveProcess"),
    HIP_ERROR_NAME_PAIR(hipErrorContextIsDestroyed, "hipErrorContextIsDestroyed"),
    HIP_ERROR_NAME_PAIR(hipErrorAssert, "hipErrorAssert"),
    HIP_ERROR_NAME_PAIR(hipErrorHostMemoryAlreadyRegistered, "hipErrorHostMemoryAlreadyRegistered"),
    HIP_ERROR_NAME_PAIR(hipErrorHostMemoryNotRegistered, "hipErrorHostMemoryNotRegistered"),
    HIP_ERROR_NAME_PAIR(hipErrorLaunchFailure, "hipErrorLaunchFailure"),
    HIP_ERROR_NAME_PAIR(hipErrorCooperativeLaunchTooLarge, "hipErrorCooperativeLaunchTooLarge"),
    HIP_ERROR_NAME_PAIR(hipErrorNotSupported, "hipErrorNotSupported"),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureUnsupported, "hipErrorStreamCaptureUnsupported"),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureInvalidated, "hipErrorStreamCaptureInvalidated"),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureMerge, "hipErrorStreamCaptureMerge"),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureUnmatched, "hipErrorStreamCaptureUnmatched"),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureUnjoined, "hipErrorStreamCaptureUnjoined"),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureIsolation, "hipErrorStreamCaptureIsolation"),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureImplicit, "hipErrorStreamCaptureImplicit"),
    HIP_ERROR_NAME_PAIR(hipErrorCapturedEvent, "hipErrorCapturedEvent"),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureWrongThread, "hipErrorStreamCaptureWrongThread"),
    HIP_ERROR_NAME_PAIR(hipErrorGraphExecUpdateFailure, "hipErrorGraphExecUpdateFailure"),
    HIP_ERROR_NAME_PAIR(hipErrorUnknown, "hipErrorUnknown"),
    HIP_ERROR_NAME_PAIR(hipErrorRuntimeMemory, "hipErrorRuntimeMemory"),
    HIP_ERROR_NAME_PAIR(hipErrorRuntimeOther, "hipErrorRuntimeOther"),
    HIP_ERROR_NAME_PAIR(hipErrorTbd, "hipErrorTbd"),
  };

  const char *
  error::get_error_name(hipError_t err)
  {
    const char *error_name = nullptr;

    auto itr = hip_error_names.find(err);
    if (itr != hip_error_names.end())
    {
      error_name = itr->second.c_str();
    }

    return error_name;
  }

}
