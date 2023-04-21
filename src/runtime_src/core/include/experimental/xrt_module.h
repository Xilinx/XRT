// Copyright (C) 2023 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#ifndef XRT_MODULE_H_
#define XRT_MODULE_H_

#include "xrt/detail/config.h"
#include "xrt/xrt_bo.h"
#include "xrt/xrt_hw_context.h"
#include "xrt/xrt_uuid.h"
#include "xrt/detail/pimpl.h"
#include "experimental/xrt_elf.h"

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
 * An xrt::module contains functions an application will execute in
 * hardware.  In Alveo the xclbin is the container that configures the
 * hardware and provides the functions.  In AIE the functions are a
 * set of instructions that are run on configured hardware, the
 * instructions are embedded in an elf file, which is parsed for meta
 * data determining how the functions are invoked.
 *
 * An xrt::module is constructed from the object that contains the
 * functions to execute.  In the case of Alveo, the module is
 * definitely constructed from an xrt::xclbin , in case of AIE the
 * module is constructed from an xrt::elf or from a user pointer.
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
   * @param hwctx
   *   Hardware context that can execute the module functions
   * @param elf
   *   An elf binary with functions to execute within the hwctx
   *
   * The elf binary contains instructions for functions to be executed
   * in the specified hardware context.  The elf binary has text
   * segments with meta data to be mined for function arguments and
   * type.
   *
   * The constructor allocates an instruction buffer object within the
   * hardware context.  When extracting a function from the module a
   * sub-buffer into the instruction buffer is created and returned.
   *
   * Throws an exception if the elf cannot be used with specified
   * hardware context.
   */
  XRT_API_EXPORT
  module(const xrt::hw_context& hwctx, const xrt::elf& elf);

  /**
   * module() - Constructor from user ptr
   *
   * @param hwctx
   *   Hardware context that can execute the module functions
   * @param userptr
   *   A pointer to an opaque representation of the instructions
   *   to execute on hardware configured by an xclbin with uuid
   * @param sz
   *   Size in bytes of the userptr buffer
   * @param uuid
   *   Unique id of the hardware configuration.  Must match the
   *   xclbin uuid use to configure the hardware.
   *
   * The user pointer is an opaque representation of the instructions
   * to execute on hardware configured by xclbin.
   *
   * The constructor allocates an instruction buffer object within
   * the hardware context.  When extracting a function from the module
   * a sub-buffer into the instruction buffer is created and returned.
   *
   * Throws an exception if the specified uuid doesn't match the uuid
   * of the hardware context configuration.
   */
  XRT_API_EXPORT
  module(const xrt::hw_context& hwctx, void* userptr, size_t sz, const xrt::uuid& uuid);

  /**
   * module() - Sub module
   *
   * @param parent
   *   Parent module to dissect
   * @param size
   *   Size of new sub module
   * @param offset
   *   Offset from base of parent module
   *
   * Create a module carved out of the instruction buffer associated
   * with the parent module.
   */
  XRT_API_EXPORT
  module(const xrt::module& parent, size_t size, size_t offset);

  /**
   * get_cfg_uuid() - Get the uuid of the hardware configuration
   *
   * @return
   *   UUID of matching hardware configuration
   *
   * An module is associated with exactly one hardware configuration.
   * This function returns the uuid that identifies the configuration.
   */
  XRT_API_EXPORT
  xrt::uuid
  get_cfg_uuid() const;

  /**
   * get_symbol() - Returns instruction buffer for symbol
   *
   * @param nm
   *   Name of symbol to get instructions for
   * @return
   *   Instruction buffer for the specified symbol
   */
  XRT_API_EXPORT
  xrt::bo
  get_instruction_buffer(const std::string& nm) const;

  XRT_API_EXPORT
  xrt::hw_context
  get_hw_context() const;

private:
};

} // namespace xrt

#else
# error xrt::module is only implemented for C++
#endif // __cplusplus

#endif
