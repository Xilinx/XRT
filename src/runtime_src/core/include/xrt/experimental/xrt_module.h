// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
#ifndef XRT_MODULE_H_
#define XRT_MODULE_H_

#include "xrt/detail/config.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_uuid.h"
#include "xrt/detail/pimpl.h"
#include "xrt/experimental/xrt_elf.h"

#ifdef __cplusplus
# include <cstdint>
# include <string>
#endif

#ifdef __cplusplus
namespace xrt {

/*!
 * @class module
 *
 * @brief
 * Represents executable control code derived from an xrt::elf to be
 * executed in a specific hardware context.
 *
 * @details
 * An xrt::module is constructed from an xrt::elf and becomes associated
 * with an xrt::hw_context when used to create an xrt::run object.
 * It manages buffer objects containing parsed ELF section data (ctrlcode, ctrlpacket, etc.)
 * that are patched with kernel arguments and submitted to hardware for execution.
 */
class module_impl;
class module : public detail::pimpl<module_impl>
{
public:
  /**
   */
  module()
  {}

  /**
   * module() - Constructor from elf
   *
   * @param elf
   *  An elf binary with functions to execute
   *
   * The elf binary contains instructions for functions to be executed
   * in some hardware context.
   *
   * The constructor retains ownership of the elf object.
   */
  XRT_API_EXPORT
  explicit
  module(const xrt::elf& elf);


  /**
   * get_hw_context() - Get the hardware context associated with the module
   *
   * @return
   *  Hardware context associated with the module.
   *  Returns an empty xrt::hw_context if the module is not
   *  associated with a hardware context yet.
   */
  XRT_API_EXPORT
  xrt::hw_context
  get_hw_context() const;

  ///@cond
  // Undocumented converting constructor using impl only
  XRT_API_EXPORT
  explicit
  module(std::shared_ptr<module_impl> impl)
    : detail::pimpl<module_impl>(std::move(impl))
  {}
  /// @endcond

private:
};

} // namespace xrt

#else
# error xrt::module is only implemented for C++
#endif // __cplusplus

#endif
