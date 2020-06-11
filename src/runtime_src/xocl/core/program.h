/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef xocl_core_program_h_
#define xocl_core_program_h_
#include "xocl/config.h"
#include "xocl/core/object.h"
#include "xocl/core/refcount.h"
#include "xocl/core/range.h"
#include "xocl/xclbin/xclbin.h"

#include <vector>
#include <map>
#include <functional>

#ifdef _WIN32
# pragma warning( push )
# pragma warning ( disable : 4996 )
#endif


namespace xocl {

class program : public refcount, public _cl_program
{
  using device_vector_type = std::vector<ptr<device>>;
  using device_iterator_type = ptr_iterator<device_vector_type::iterator>;
  using device_const_iterator_type = ptr_iterator<device_vector_type::const_iterator>;

public:
  /**
   * @param ctx
   *   Context in which to contruct program
   * @param source
   *   The source for the program
   */
  program(context* ctx,const std::string& source);

  /**
   * This constructor is tailored for clCreateProgramWithBinary
   *
   * @param ctx
   *   Context in which to contruct program
   * @param num_devices
   *   Number of devices and binaries
   * @param devices
   *   List of devices to program
   * @param binaries
   *   List of raw binaries matching device entries in devices
   * @param lengths
   *   Length of each binary
   */
  program(context* ctx,cl_uint num_devices, const cl_device_id* devices
          ,const unsigned char** binaries, const size_t* lengths);

  /**
   * Delegating contstrutor
   */
  explicit
  program(context* ctx)
    : program(ctx,"")
  {}

  virtual ~program();

  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  range<device_iterator_type>
  get_device_range()
  {
    return range<device_iterator_type>(m_devices.begin(),m_devices.end());
  }

  range<device_const_iterator_type>
  get_device_range() const
  {
    return range<device_const_iterator_type>(m_devices.begin(),m_devices.end());
  }

  device*
  get_first_device() const
  {
    auto itr = range_find(m_devices,[](auto& d) { return d.get() != nullptr; });
    return (itr != m_devices.end()) ? (*itr).get() : nullptr;
  }

  context*
  get_context() const
  {
    return m_context.get();
  }

  const std::string&
  get_source() const
  {
    return m_source;
  }

  /**
   * For conformance flow only
   */
  size_t
  num_devices() const
  {
    return m_devices.size();
  }

  void
  add_device(device* d);

  bool
  has_device(const device* d) const
  {
    return std::find(m_devices.begin(),m_devices.end(),d)!=m_devices.end();
  }

  /**
   * Return a list of progvar names in this program
   *
   * @return
   *   List of std::strings corresponding to names of the prog vars
   */
  std::vector<std::string>
  get_progvar_names() const;

  /**
   * Return the xclbin for argument device
   *
   * @param d
   *   Device to key against
   * @return
   *   The xclbin associated with the device
   */
  XRT_XOCL_EXPORT
  xclbin
  get_xclbin(const device* d) const;

  /**
   * @return
   *   The uuid of xclbin for argument device
   */
  xrt_core::uuid
  get_xclbin_uuid(const device* d) const;

  /**
   * Return the xclbin binary for argument device
   *
   * @param d
   *   Device to key against
   * @return
   *   The xclbin binary object associated with the device
   */
  std::pair<const char*, const char*>
  get_xclbin_binary(const device* d) const;

  /**
   * Return the target type for this program
   *
   * This is the determined by one xclbin if any, otherwise
   * it is invalid.  Even though a program can have multiple
   * binaries (xclbins per device), a program is still compiled
   * for one target only.
   *
   *   enum class target_type{ bin,x86,zynqps7,csim,cosim,hwem,invalid};
   *
   * @param d
   *   Device to key against
   * @return
   *   The xclbin binary object associated with the device
   */
  using target_type = xclbin::target_type;
  target_type
  get_target() const;

  /**
   * Return a list of binary sizes stored in this program
   *
   * This function is wired for clGetProgramInfo. The order of sizes
   * in returned list corresponds to the order of devices returned by
   * get_device_range.
   *
   * Not a cool dependency on device range. Subject to removal.
   *
   * @return
   *   List of binary sizes
   */
  std::vector<size_t>
  get_binary_sizes() const;

  /**
   * Number of kernels declared in the program that can be created
   * using clCreateKernel.
   *
   * The kernels are the same for each binary in a program, otherwise
   * the program would not be wellformed.
   */
  unsigned int
  get_num_kernels() const;

  /**
   * Get list of names of kernels in this program.
   *
   * The kernels are the same for each binary in a program, otherwise
   * the program would not be wellformed.
   */
  std::vector<std::string>
  get_kernel_names() const;

  bool
  has_kernel(const std::string& kname) const;

  /**
   * Create a kernel.
   *
   * Look up kernel by name and create a kernel object.  The
   * function throws on error.
   *
   * @param kernel_name
   *   The name of the kernel to create
   * @return
   *   Un-managed kernel object.   Must be released when no longer
   *   needed.
   */
  std::unique_ptr<kernel,std::function<void(kernel*)>>
  create_kernel(const std::string& kernel_name);

  /**
   * How was this program created
   *
   * @return
   *   Creation type per enum value
   */
  enum class creation_type { source, binary, kernel };
  creation_type
  get_creation_type() const;

  /**
   * Get the options used to build this program for argument device
   *
   * @param dev
   *  Device for which to get build options.
   * @return
   *  Options used to build this program for argument device, or
   *  empty string if program wasn't explicitly build by runtime.
   */
  std::string
  get_build_options(const device* dev) const
  {
    auto itr = m_options.find(dev);
    return itr!=m_options.end()
      ? (*itr).second
      : "";
  }

  /**
   * Get the build log for argument device
   *
   * @param dev
   *  Device for which to get build log.
   * @return
   *  Build log argument device, or empty string if program wasn't
   *  explicitly build by runtime.
   */
  std::string
  get_build_log(const device* dev) const
  {
    auto itr = m_logs.find(dev);
    return itr!=m_logs.end()
      ? (*itr).second
      : "";
  }

  /**
   * Get build status
   */
  cl_build_status
  get_build_status(const device* dev) const
  {
    if (m_binaries.count(dev))
      return CL_BUILD_SUCCESS;
    else if (m_logs.count(dev))
      return CL_BUILD_ERROR;
    else
      return CL_BUILD_NONE;
  }

  ////////////////////////////////////////////////////////////////
  // Conformance helpers
  ////////////////////////////////////////////////////////////////
  unsigned int
  conformance_rename_kernel(const std::string&)
  {
    throw std::runtime_error("XCL_CONFORMANCE no longer supported");
  }

  void
  set_source(const std::string&)
  {
    throw std::runtime_error("XCL_CONFORMANCE no longer supported");
  }

  void
  build(const std::vector<device*>& devices,const std::string& options);

private:
  unsigned int m_uid = 0;

  ptr<context> m_context;
  device_vector_type m_devices;

  std::map<const device*,std::vector<char>> m_binaries;
  std::map<const device*,std::string> m_options;
  std::map<const device*,std::string> m_logs;    // build *error* logs

  std::string m_source;
public:
  // conformance
  std::string conformance_binaryfilename;
  std::string conformance_binaryhash;
};

/**
 * Get a locked range of current program objects.
 *
 * Do not attempt to create program objects while holding on to the
 * returned range, a deadlock would follow.
 *
 * This function is used in conformance mode.  May disappear if better
 * alternative is found.
 */
using program_iterator_type = std::vector<program*>::iterator;
range_lock<program_iterator_type>
get_global_programs();

} // xocl

#ifdef _WIN32
# pragma warning( pop )
#endif

#endif
