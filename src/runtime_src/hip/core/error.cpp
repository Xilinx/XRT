// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Device, Inc. All rights reserved.
#include "hip/config.h"
#include "hip/hip_runtime_api.h"
#include "error.h"

#define HIP_ERROR_NAME_PAIR(x) \
  {                            \
    x, #x                      \
  }

namespace xrt::core::hip
{

  thread_local static error* hip_error_state = nullptr;

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

  static std::map<hipError_t, std::string> hip_error_names =
  {
    HIP_ERROR_NAME_PAIR(hipSuccess),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidValue),
    HIP_ERROR_NAME_PAIR(hipErrorOutOfMemory),
    HIP_ERROR_NAME_PAIR(hipErrorMemoryAllocation),
    HIP_ERROR_NAME_PAIR(hipErrorNotInitialized),
    HIP_ERROR_NAME_PAIR(hipErrorInitializationError),
    HIP_ERROR_NAME_PAIR(hipErrorDeinitialized),
    HIP_ERROR_NAME_PAIR(hipErrorProfilerDisabled),
    HIP_ERROR_NAME_PAIR(hipErrorProfilerNotInitialized),
    HIP_ERROR_NAME_PAIR(hipErrorProfilerAlreadyStarted),
    HIP_ERROR_NAME_PAIR(hipErrorProfilerAlreadyStopped),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidConfiguration),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidPitchValue),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidSymbol),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidDevicePointer),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidMemcpyDirection),
    HIP_ERROR_NAME_PAIR(hipErrorInsufficientDriver),
    HIP_ERROR_NAME_PAIR(hipErrorMissingConfiguration),
    HIP_ERROR_NAME_PAIR(hipErrorPriorLaunchFailure),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidDeviceFunction),
    HIP_ERROR_NAME_PAIR(hipErrorNoDevice),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidDevice),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidImage),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidContext),
    HIP_ERROR_NAME_PAIR(hipErrorContextAlreadyCurrent),
    HIP_ERROR_NAME_PAIR(hipErrorMapFailed),
    HIP_ERROR_NAME_PAIR(hipErrorMapBufferObjectFailed),
    HIP_ERROR_NAME_PAIR(hipErrorUnmapFailed),
    HIP_ERROR_NAME_PAIR(hipErrorArrayIsMapped),
    HIP_ERROR_NAME_PAIR(hipErrorAlreadyMapped),
    HIP_ERROR_NAME_PAIR(hipErrorNoBinaryForGpu),
    HIP_ERROR_NAME_PAIR(hipErrorAlreadyAcquired),
    HIP_ERROR_NAME_PAIR(hipErrorNotMapped),
    HIP_ERROR_NAME_PAIR(hipErrorNotMappedAsArray),
    HIP_ERROR_NAME_PAIR(hipErrorNotMappedAsPointer),
    HIP_ERROR_NAME_PAIR(hipErrorECCNotCorrectable),
    HIP_ERROR_NAME_PAIR(hipErrorUnsupportedLimit),
    HIP_ERROR_NAME_PAIR(hipErrorContextAlreadyInUse),
    HIP_ERROR_NAME_PAIR(hipErrorPeerAccessUnsupported),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidKernelFile),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidGraphicsContext),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidSource),
    HIP_ERROR_NAME_PAIR(hipErrorFileNotFound),
    HIP_ERROR_NAME_PAIR(hipErrorSharedObjectSymbolNotFound),
    HIP_ERROR_NAME_PAIR(hipErrorSharedObjectInitFailed),
    HIP_ERROR_NAME_PAIR(hipErrorOperatingSystem),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidHandle),
    HIP_ERROR_NAME_PAIR(hipErrorInvalidResourceHandle),
    HIP_ERROR_NAME_PAIR(hipErrorIllegalState),
    HIP_ERROR_NAME_PAIR(hipErrorNotFound),
    HIP_ERROR_NAME_PAIR(hipErrorNotReady),
    HIP_ERROR_NAME_PAIR(hipErrorIllegalAddress),
    HIP_ERROR_NAME_PAIR(hipErrorLaunchOutOfResources),
    HIP_ERROR_NAME_PAIR(hipErrorLaunchTimeOut),
    HIP_ERROR_NAME_PAIR(hipErrorPeerAccessAlreadyEnabled),
    HIP_ERROR_NAME_PAIR(hipErrorPeerAccessNotEnabled),
    HIP_ERROR_NAME_PAIR(hipErrorSetOnActiveProcess),
    HIP_ERROR_NAME_PAIR(hipErrorContextIsDestroyed),
    HIP_ERROR_NAME_PAIR(hipErrorAssert),
    HIP_ERROR_NAME_PAIR(hipErrorHostMemoryAlreadyRegistered),
    HIP_ERROR_NAME_PAIR(hipErrorHostMemoryNotRegistered),
    HIP_ERROR_NAME_PAIR(hipErrorLaunchFailure),
    HIP_ERROR_NAME_PAIR(hipErrorCooperativeLaunchTooLarge),
    HIP_ERROR_NAME_PAIR(hipErrorNotSupported),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureUnsupported),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureInvalidated),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureMerge),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureUnmatched),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureUnjoined),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureIsolation),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureImplicit),
    HIP_ERROR_NAME_PAIR(hipErrorCapturedEvent),
    HIP_ERROR_NAME_PAIR(hipErrorStreamCaptureWrongThread),
    HIP_ERROR_NAME_PAIR(hipErrorGraphExecUpdateFailure),
    HIP_ERROR_NAME_PAIR(hipErrorUnknown),
    HIP_ERROR_NAME_PAIR(hipErrorRuntimeMemory),
    HIP_ERROR_NAME_PAIR(hipErrorRuntimeOther),
    HIP_ERROR_NAME_PAIR(hipErrorTbd),
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
