// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_AIE_COREDUMP_H_
#define XRT_AIE_COREDUMP_H_

#ifdef __cplusplus

#include <cstdint>
#include <string>

namespace xrt::aie {

/**
 * enum class context_status - State of an AIE hardware context at the time
 * a coredump was captured.
 *
 * This enum is embedded in coredump_meta and describes what the hardware
 * context was doing when the dump was triggered.  It mirrors the internal
 * aiebu::aie_context_status enum and must stay in sync with it.
 *
 * @var idle
 *   The context has no work queued or executing on the hardware.
 *   This is the default value used when the status cannot be determined.
 *
 * @var ready
 *   The context has been prepared and is waiting to be scheduled onto
 *   the hardware but has not yet started executing.
 *
 * @var running
 *   The context is actively executing on the AIE hardware at the time
 *   the coredump was captured.
 *
 * @var timeout
 *   The context timed out while waiting for hardware to complete an
 *   operation.  The coredump was likely captured as part of timeout
 *   error handling.
 *
 * @var error
 *   The context encountered an error other than a timeout (e.g. a
 *   hardware fault or driver error).
 */
enum class context_status : uint32_t {
  idle    = 0,
  ready   = 1,
  running = 2,
  timeout = 3,
  error   = 4,
};

/**
 * struct coredump_meta - Metadata embedded in an AIE ET_CORE coredump ELF.
 *
 * This structure carries diagnostic metadata that is written into the
 * coredump ELF produced by xrt::hw_context::get_aie_coredump_elf().
 * The metadata is stored in a dedicated ELF note section alongside the
 * raw AIE register/memory dump, allowing post-mortem tools to correlate
 * the dump with the system state at capture time.
 *
 * Callers may supply a fully populated coredump_meta to
 * get_aie_coredump_elf(); if none is provided, XRT populates the fields
 * it can determine automatically (timestamp, firmware version, device
 * info, ELF UUID) and defaults context_status to idle.
 *
 * All string fields are UTF-8 and may be empty if the information is
 * not available on a given platform.
 */
struct coredump_meta {
  /**
   * Monotonic capture timestamp in nanoseconds since system epoch.
   * Populated automatically from the system clock when metadata is
   * built internally.  Callers that supply their own coredump_meta
   * should set this to the time the error or timeout was detected.
   */
  uint64_t timestamp_ns = 0;

  /**
   * Human-readable driver version string
   * May be empty if the driver does not expose version information
   * on this platform.
   */
  std::string driver_version;

  /**
   * Human-readable NPU/AIE firmware version string
   * (e.g. "major.minor.patch.build").
   * Populated automatically from the firmware_version device query
   * when metadata is built internally.  May be empty if the platform
   * does not support the query.
   */
  std::string fw_version;

  /**
   * Device identification string
   * Populated automatically from device query when
   * metadata is built internally. May be empty if not available.
   */
  std::string device_info;

  /**
   * State of the AIE hardware context at the time of the dump.
   * See xrt::aie::context_status for valid values.
   * Defaults to context_status::timeout when metadata is built internally,
   * as the driver does not currently expose context state.
   */
  context_status ctx_status = context_status::timeout;

  /**
   * UUID string of the AIE configuration ELF that was loaded into this
   * hardware context (e.g. "550e8400-e29b-41d4-a716-446655440000").
   * Populated automatically from the loaded ELF when metadata is built
   * internally. 
   */
  std::string uuid;
};

} // namespace xrt::aie

#else
# error xrt_aie_coredump.h is only implemented for C++
#endif // __cplusplus

#endif
