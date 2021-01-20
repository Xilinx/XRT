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

#ifndef runtime_src_xocl_xclbin_h_
#define runtime_src_xocl_xclbin_h_

#include "xocl/config.h"
#include "core/include/xclbin.h" // definition of binary structs
#include "core/common/device.h"
#include "core/common/uuid.h"

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <array>
#include <bitset>

namespace xocl {

class device;
class kernel;

class xclbin
{
public:
  using addr_type = uint64_t;
  // Max 256 memory indicies for now. This number must be >= to number
  // of mem_topology.m_count.  Unfortunately it is a compile time constant.
  // A better solution must be found (boost::dynamic_bitset<>???)
  using memidx_type = int32_t;
  using connidx_type = int32_t;
  static constexpr memidx_type max_banks = 256;
  using memidx_bitmask_type = std::bitset<max_banks>;

  enum class target_type{ bin,x86,zynqps7,csim,cosim,hwem,invalid};

  // A symbol captures all data required to construct an xocl::kernel
  // object.  It is associated with all kernel objects in the xclbin.
  // The symbol is returned up stream via xclbin::lookup_kernel(name).
  struct symbol
  {
    // Wrap data associated with a kernel argument
    struct arg {
      enum class argtype { indexed, printf, rtinfo };
      std::string name;
      size_t address_qualifier;
      std::string id;
      std::string port;
      size_t port_width;
      size_t size;
      size_t offset;
      size_t hostoffset;
      size_t hostsize;
      std::string type;
      size_t memsize;
      argtype atype;        // optimization to avoid repeated string cmp
      symbol* host;
    };

    // Wrap data associated with kernel instances
    struct instance {
      std::string name;   // inst name
      size_t base;        // base addr
    };

    std::map<uint32_t,std::string> stringtable;

    std::string name;                // name of kernel
    unsigned int uid;                // unique id for this symbol, some symbols have same name??
    std::string attributes;          // attributes as per .cl file
    std::string hash;                // kernel conformance hash
    size_t workgroupsize = 0;
    size_t compileworkgroupsize[3] = {0};   //
    size_t maxworkgroupsize[3] = {0};// xilinx extension
    std::vector<arg> arguments;      // the args of this kernel
    std::vector<instance> instances; // the kernel instances
    target_type target;              // xclbin target
  };

public:
  xclbin();

  /**
   * xclbin() - construct xclbin meta data
   */
  xclbin(const xrt_core::device* core_device, const xrt_core::uuid& uuid);

  bool
  operator==(const xclbin& rhs) const
  {
    return m_impl == rhs.m_impl;
  }
 
  operator bool() const
  {
    return m_impl != nullptr;
  }

  /**
   * Get uuid of xclbin
   */
  xrt_core::uuid
  uuid() const;

  /**
   * Access the project name per xml meta data
   */
  XRT_XOCL_EXPORT
  std::string
  project_name() const;

  /**
   * What target is this xclbin compiled for
   */
  target_type
  target() const;

  /**
   * Number of kernels
   */
  unsigned int
  num_kernels() const;

  /**
   * Get list of kernels function in this xclbin
   *
   * The names returned are exactly as they appear in xclbin, e.g.
   * there is no name mangling
   */
  std::vector<std::string>
  kernel_names() const;

  /**
   * Get list of kernel symbols in this xclbin
   */
  std::vector<const symbol*>
  kernel_symbols() const;

  /**
   * Get kernel with specified name.
   *
   * The lifetime of the returned object is tied to the lifetime
   * of this xclbin object, which is tied to the lifetime of the
   * xocl::program that constructs this object.
   *
   * This function is analogous to dlsym.
   */
  const symbol&
  lookup_kernel(const std::string& name) const;

  /**
   * Get the mem topology section in xclbin
   */
  const mem_topology*
  get_mem_topology() const;

  /**
   * Get memory connection indeces for CU argument at specified index
   *
   * @param cuaddr
   *   The base address of CU
   * @param arg
   *   The index of the argument for which to return DDR bank connectivity
   * @return
   *   Bitset with memory connection indeces per connectivity sections that
   *   match [cuaddr,arg] pair
   */
  memidx_bitmask_type
  cu_address_to_memidx(addr_type cuaddr, int32_t arg) const;

  /**
   * @return
   *   Memory connection indeces as the union of connections for all
   *   arguments of CU as specified address.
   */
  xocl::xclbin::memidx_bitmask_type
  cu_address_to_memidx(addr_type cu_addr) const;

  /**
   * Get a bitmask with mem_data indices that maps to specified address
   * @param memaddr
   *   Address of device side ddr location
   * @return
   *   Bitmask inditifying the memory indices where the memaddr argument
   *   is within the mem_data address range.
   */
  memidx_bitmask_type
  mem_address_to_memidx(addr_type memaddr) const;

  /**
   * Get the index for first mem matching the given address
   * @param memaddr
   *   Address of device side ddr location
   * @return
   *   index in the range 0,1,2...31
   *   -1 if no match
  */
  memidx_type
  mem_address_to_first_memidx(addr_type addr) const;

  /**
   * Get the bank string tag for the memory at specified index
   *
   * @param memidx
       Index of memory to retrieve tag name for
   * @return
   *   Tag name for memory idx
   */
  XRT_XOCL_EXPORT
  std::string
  memidx_to_banktag(memidx_type memidx) const;

  /**
   * Get the memory index for a given kernel name for specific arg.
   *
   * @param kernel_name
       Kernel name to  retrieve the memory index for
   * @param arg
       Index of arg to retrieve the memory index for
   * @param conn
   *   Index into the connectivity section allocated.
   * @return
   *   Memory idx
   */
  memidx_type
  get_memidx_from_arg(const std::string& kernel_name, int32_t arg, connidx_type& conn);

  void
  clear_connection(connidx_type index);

  /**
   * Get the memory index with the specified tag.
   *
   * @param tag
       The tag name to look for
   * @return
   *   Tag memory index corresponding to tag name
   */
  memidx_type
  banktag_to_memidx(const std::string& tag) const;

private:
  struct impl;
  std::shared_ptr<impl> m_impl;

  impl*
  impl_or_error() const;

};

} // xocl

#endif
