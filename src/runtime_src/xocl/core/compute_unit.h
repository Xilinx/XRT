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

#ifndef xocl_core_compute_unit_h_
#define xocl_core_compute_unit_h_

#include "xocl/xclbin/xclbin.h"
#include <string>

namespace xocl {

class device;

// Compute unit
//
// Ownership of cus is shared between program and device with
// latter constructing the compute units as a program is
// loaded.
class compute_unit
{
  // device owns compute units and needs private access to some state
  // information managed exclusively by the device implementation.
  friend class device;
  enum class context_type : unsigned short { shared, exclusive, none };
public:
  const size_t max_index = 128;

private:
  // construct through static create only
  compute_unit(const xclbin::symbol* s, const std::string& n, size_t base, size_t idx, const device* d);
public:
  ~compute_unit();

  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  /**
   * Address extracted from xclbin
   */
  size_t
  get_base_addr() const
  {
    return m_address;
  }

  /**
   * Encode this CU physical address into an index
   */
  size_t
  get_index() const
  {
    return m_index;
  }

  std::string
  get_name() const
  {
    return m_name;
  }

  std::string
  get_kernel_name() const
  {
    return m_symbol->name;
  }

  /**
   * Get memory index for indexed kernel argument
   *
   * @param idx
   *   Argument index
   * @return Memory index identifying DDR bank for argument
   */
  XRT_XOCL_EXPORT
  xclbin::memidx_bitmask_type
  get_memidx(unsigned int arg) const;

  /**
   * Get memory indeces intersection of DDR banks for CU args
   *
   * @return
   *   Memory indeces identifying intersection of DDR banks for all CU arguments
   */
  xclbin::memidx_bitmask_type
  get_memidx_intersect() const;

  /**
   * Get memory indeces union of DDR banks for CU args
   *
   * @return
   *   Memory indeces identifying union of DDR banks for all CU arguments
   */
  xclbin::memidx_bitmask_type
  get_memidx_union() const;

  const xocl::xclbin::symbol*
  get_symbol() const
  {
    return m_symbol;
  }

  unsigned int
  get_symbol_uid() const
  {
    return m_symbol->uid;
  }

  context_type
  get_context_type() const
  {
    return m_context_type;
  }

  uint32_t
  get_control_type() const
  {
    return m_control;
  }

  const device*
  get_device() const
  {
    return m_device;
  }

  /**
   * Static constructor for compute units.
   *
   * @symbol: The kernel symbol gather from xclbin meta data
   * @inst: The kernel instance
   * @device: The device constructing this compute unit
   * @cuaddr: Sorted base addresses of all CUs in xclbin
   *
   * The kernel instance base address is checked against @cuaddr to
   * determine its index.  If the instance base address is not in
   * @cuaddr it is ignored, e.g. no compute unit object constructed.
   */
  static std::unique_ptr<compute_unit>
  create(const xclbin::symbol*, const xclbin::symbol::instance&,
         const device*, const std::vector<uint64_t>&);

private:

  // Shared implementation by outer locking routines
  xclbin::memidx_bitmask_type
  get_memidx_nolock(unsigned int arg) const;

  // Used by xocl::device to cache the acquire context for
  void
  set_context_type(bool shared) const
  {
    m_context_type = shared ? compute_unit::context_type::shared : compute_unit::context_type::exclusive;
  }

  // Used by xocl::device when context is released for this CU
  void
  reset_context_type() const
  {
    m_context_type = compute_unit::context_type::none;
  }

  unsigned int m_uid = 0;
  const xclbin::symbol* m_symbol = nullptr;
  std::string m_name;
  const device* m_device = nullptr;
  size_t m_address = 0;
  size_t m_index = 0;
  uint32_t m_control = 0;  // IP_CONTROL type per xclbin ip_layout
  mutable context_type m_context_type = context_type::none;

  // Map CU arg to memory bank indicies. An argument can
  // be connected to multiple memory banks.
  mutable std::map<unsigned int,xclbin::memidx_bitmask_type> m_memidx_mask;

  // Intersection of all argument masks
  mutable bool cached = false;
  mutable xclbin::memidx_bitmask_type m_memidx;
  mutable std::mutex m_mutex;
};

} // xocl

#endif
