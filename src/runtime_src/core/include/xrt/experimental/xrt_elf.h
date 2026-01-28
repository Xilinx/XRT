// Copyright (C) 2023-2026 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
#ifndef XRT_ELF_H_
#define XRT_ELF_H_

#include "xrt/detail/config.h"
#include "xrt/detail/pimpl.h"
#include "xrt/xrt_uuid.h"

#ifdef __cplusplus
# include <cstdint>
# include <istream>
# include <string>
# include <vector>
#endif

#ifdef __cplusplus
namespace xrt {

/*!
 * @class elf
 *
 * @brief
 * An elf contains instructions for functions to execute in some
 * pre-configured hardware.  The xrt::elf class provides APIs to mine
 * the elf itself for relevant data.
 */
class elf_impl;
class elf : public detail::pimpl<elf_impl>
{
public:
  /*!
   * class kernel
   *
   * @brief
   * xrt::elf::kernel represents a kernel in an elf.
   *
   * @details
   * The kernel corresponds to an compute function that can be
   * executed on the hardware. Each kernel has a signature that shows
   * the arguments in the function and also each kernel can have multiple
   * instances.
   */
  class kernel_impl;
  class kernel : public detail::pimpl<kernel_impl>
  {
  public:
    class instance_impl;
    class instance : public detail::pimpl<instance_impl>
    {
    public:
      instance() = default;

      explicit
      instance(std::shared_ptr<instance_impl> handle)
        : detail::pimpl<instance_impl>(std::move(handle))
      {}

      std::string
      get_name() const;
    };

  public:
    kernel() = default;

    explicit
    kernel(std::shared_ptr<kernel_impl> handle)
      : detail::pimpl<kernel_impl>(std::move(handle))
    {}

    /**
     * get_name() - Get kernel name
     *
     * @return
     *  The name of the kernel
     */
    XRT_API_EXPORT
    std::string
    get_name() const;

    /**
     * get_num_args() - Number of arguments
     *
     * @return
     *  Number of arguments for this kernel.
     */
    XRT_API_EXPORT
    size_t
    get_num_args() const;

    /**
     * @enum data_type
     * @brief Data type of argument of a kernel
     */
    enum class data_type : uint8_t { scalar = 0, global = 1 };

    /**
     * get_arg_data_type() - Get data type of argument at index
     *
     * @param index
     *  Index of argument
     * @return
     *  Data type of argument at index
     */
    XRT_API_EXPORT
    data_type
    get_arg_data_type(size_t index) const;

    /**
     * get_instances() - Get list of instances of a kernel
     *
     * @return
     *  List of instances of a kernel
     */
    XRT_API_EXPORT
    std::vector<instance>
    get_instances() const;
  };

public:
  elf() = default;

  /**
   * elf() - Constructor from ELF file
   */
  XRT_API_EXPORT
  explicit
  elf(const std::string& fnm);

  /**
   * elf() - Constructor from raw data
   *
   * @param data
   *  Raw data of elf
   *
   * The raw data of the elfcan be deleted after calling the
   * constructor.
   */
  XRT_API_EXPORT
  explicit
  elf(const std::string_view& data);

  /**
   * elf() - Constructor from ELF file
   *
   * Avoid ambiguity between std::string and std::string_view.
   */
  explicit
  elf(const char* fnm)
    : elf(std::string(fnm))
  {}

  /**
   * elf() - Constructor from raw ELF data stream
   *
   * @param stream
   *  Raw data stream of elf
   *
   */
  XRT_API_EXPORT
  explicit
  elf(std::istream& stream);

  /**
   * elf() - Constructor from raw ELF data
   *
   * @param data
   *  Pointer to raw elf data
   * @param size
   *  Size of raw elf data
   *
   */
  XRT_API_EXPORT
  elf(const void *data, size_t size);

  /// @cond
  // Undocumented converting constructor using impl only
  XRT_API_EXPORT
  elf(std::shared_ptr<elf_impl> impl)
    : detail::pimpl<elf_impl>(std::move(impl))
  {}
  /// @endcond

  /**
   * get_cfg_uuid() - Get the configuration UUID of the elf
   *
   * @return
   *  The configuration UUID of the elf
   */
  XRT_API_EXPORT
  xrt::uuid
  get_cfg_uuid() const;

  /**
   * is_full_elf() - Check if the elf is a full ELF
   *
   * A full ELF can be used as a replacement for xclbin, it contains
   * all the information required to create a hardware context like
   * partition size, kernel signatures, etc.
   *
   * @return
   *  True if the elf is a full ELF, false otherwise
   */
  XRT_API_EXPORT
  bool
  is_full_elf() const;

 /**
   * @enum platform
   * @brief ELF OS/ABI values identifying the target AIE platform.
   *
   * These values correspond to the ELF header e_ident[EI_OSABI] field
   * and identify which AIE architecture the ELF was compiled for.
   */
   enum class platform : uint8_t {
    aie2ps       = 64,   // AIE2PS architecture
    aie2p        = 69,   // AIE2P architecture
    aie2ps_group = 70    // AIE2PS group variant
  };

  /**
   * get_platform() - Get the target AIE platform
   *
   * Returns the platform enum value corresponding to the ELF's
   * e_ident[EI_OSABI] field.
   *
   * @return
   *  The platform enum value for the ELF
   *  throws std::runtime_error if the ELF contains an unknown
   *  or unsupported platform
   */
  XRT_API_EXPORT
  platform
  get_platform() const;

  /**
   * get_partition_size() - Get the partition size (number of columns)
   *
   * The partition size is stored in the .note.xrt.configuration section
   * of the ELF file.
   *
   * @return
   *  The partition size (number of columns) for the ELF
   *  throws std::runtime_error if the ELF is missing xrt configuration info
   */
   XRT_API_EXPORT
   uint32_t
   get_partition_size() const;

   /**
    * get_kernels() - Get list of kernels from ELF
    *
    * @return
    *  List of kernels from ELF
    */
   XRT_API_EXPORT
   std::vector<kernel>
   get_kernels() const;
};

} // namespace xrt

#else
# error xrt::elf is only implemented for C++
#endif // __cplusplus

#endif
