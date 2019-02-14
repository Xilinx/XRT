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

#include "driver/include/xclbin.h" // definition of binary structs

#include "xocl/core/refcount.h"
#include "xclbin/binary.h"
#include "xrt/util/uuid.h"

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <array>
#include <bitset>

namespace xocl {

class device;
class kernel;

/**
 * xocl::xclbin is a class that encapsulates the meta data of a binary
 * xclbin file (::xclbin::binary).  The binary file format must have
 * no meta data xml dependencies.
 *
 *   runtime/xocl/xclbin                runtime/xclbin
 *       xml-parsing                        binary
 *     [xocl::xclbin] <>------------ [ ::xclbin::binary ]
 *           ^                             ^       ^
 *           |                             |       |
 *          uses                          uses    uses
 *           |                             |       |
 *         [xocl]                        [xrt]    [hal]
 *
 */
class xclbin
{
  struct impl;
  std::shared_ptr<impl> m_impl;

  impl*
  impl_or_error() const;

public:
  using addr_type = uint64_t;
  //Max 64 mem banks for now.
  using memidx_bitmask_type = std::bitset<64>;
  using memidx_type = int32_t;
  using connidx_type = int32_t;

  enum class target_type{ bin,x86,zynqps7,csim,cosim,hwem,invalid};

  struct clocks
  {
    std::string region_name;
    std::string clock_name;
    unsigned int frequency;

    clocks(std::string&& rn,std::string&& cn,unsigned int f)
    : region_name(std::move(rn)), clock_name(std::move(cn)), frequency(f)
    {}
  };

  struct profiler
  {
    using slot_type = std::tuple<int, std::string, std::string>; // index, cuname, type
    std::vector<slot_type> slots;
    std::string name;
  };

  // A symbol captures all data required to construct an xocl::kernel
  // object.  It is associated with all kernel objects in the xclbin.
  // The symbol is returned up stream via xclbin::lookup_kernel(name).
  struct symbol
  {
    // Wrap data associated with a kernel argument
    struct arg {
      enum class argtype { indexed, printf, progvar, rtinfo };
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
      size_t baseaddr;      // progvar base addr
      std::string linkage;  // progvar linkage per meminst
      argtype atype;        // optimization to avoid repeated string cmp
      symbol* host;

      /**
       * Convert kernel arg data to string per type of argument
       *
       * @param data
       *   Kernel argument data to convert to string
       */
      std::string get_string_value (const unsigned char*) const;
    };

    // Wrap data associated with kernel instances
    struct instance {
      std::string name;   // inst name
      size_t base;        // base addr
      std::string port;   // port name
    };

    std::map<uint32_t,std::string> stringtable;

    std::string name;                // name of kernel
    unsigned int uid;                // unique id for this symbol, some symbols have same name??
    std::string dsaname;             // name of dsa
    std::string attributes;          // attributes as per .cl file
    std::string hash;                // kernel conformance hash
    std::string controlport;         // kernel axi slave control port
    size_t workgroupsize = 0;
    size_t compileworkgroupsize[3] = {0};   //
    size_t maxworkgroupsize[3] = {0};// xilinx extension
    std::vector<arg> arguments;      // the args of this kernel
    std::vector<instance> instances; // the kernel instances
    bool cu_interrupt = false;       // cu have interrupt support
    target_type target;              // xclbin target
  };

public:
  xclbin();

  /**
   * The underlying binary type that represents the raw
   * binary xclbin file per xclBin structs.
   */
  // implicit
  xclbin(std::vector<char>&& xb);
  xclbin(xclbin&& rhs);

  xclbin(const xclbin& rhs);
  ~xclbin();

  xclbin&
  operator=(const xclbin&& rhs);

  xclbin&
  operator=(const xclbin& rhs);

  bool
  operator==(const xclbin& rhs) const;

  /**
   * Access the raw binary xclbin
   *
   * The binary type API conforms to the xclBin struct interface
   */
  using binary_type = ::xclbin::binary;
  binary_type
  binary() const;

  /**
   * Get dsa name
   */
  std::string
  dsa_name() const;

  /**
   * Get uuid of xclbin
   */
  using uuid_type = xrt::uuid;
  uuid_type
  uuid() const;

  /**
   * Check if unified platform
   */
  bool
  is_unified() const;

  /**
   * Access the project name per xml meta data
   */
  std::string
  project_name() const;

  /**
   * What target is this xclbin compiled for
   */
  target_type
  target() const;

  /**
   * Access the kernel clocks per OCL region
   *
   * This is meta data extraction
   */
  using kernel_clocks_type = std::vector<clocks>;
  kernel_clocks_type
  kernel_clocks();

  /**
   * Access the system clocks per OCL region
   *
   * This is meta data extraction
   */
  using system_clocks_type = std::vector<clocks>;
  system_clocks_type
  system_clocks();

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
   * @return
   *   Size (in bytes) of largest kernel register map in the xclbin
   */
  size_t
  kernel_max_regmap_size() const;

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
   * Get the list of profilers
   *
   * @return
   *  Vector of profiler struct objects constructed from the xclbin meta data.
   */
  using profilers_type = std::vector<profiler>;
  profilers_type
  profilers() const;

  /**
   * Get the clock frequency sections in xclbin
   */
  const clock_freq_topology*
  get_clk_freq_topology() const;

  /**
   * Get the mem topology section in xclbin
   */
  const mem_topology*
  get_mem_topology() const;

  /**
   * Get the CU base offset
   *
   * CU addresses are sequential and separated  by a fixed size
   * The starting address is the base offset and may differ from
   * xclbin to xclbin
   *
   * @return
   *   Base address of CU address space.
   */
  size_t
  cu_base_offset() const;

  /**
   * Get the CU address space size
   *
   * @return
   *   Size of cu addressspace in power of 2
   */
  size_t
  cu_size() const;

  /**
   * Check if all CUs support completion interrupt
   *
   * @return
   *   True if completion interrupt supported by all CUs, false otherwise
   */
  bool
  cu_interrupt() const;

  /**
   * Get a sorted address map of all CUs in this xclbin
   *
   * The map is sorted in order of increasing base addresses.
   */
  std::vector<uint32_t>
  cu_base_address_map() const;

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

  ////////////////////////////////////////////////////////////////
  // Conformance helpers
  ////////////////////////////////////////////////////////////////
  unsigned int
  conformance_rename_kernel(const std::string& hash);

  std::vector<std::string>
  conformance_kernel_hashes() const;
};

} // xocl

#endif
