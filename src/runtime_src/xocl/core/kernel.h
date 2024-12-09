/**
 * Copyright (C) 2016-2021 Xilinx, Inc
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

#ifndef xocl_core_kernel_h_
#define xocl_core_kernel_h_

#include "xocl/core/object.h"
#include "xocl/core/refcount.h"
#include "xocl/core/memory.h"
#include "xocl/xclbin/xclbin.h"

#include "core/include/xrt/xrt_kernel.h"
#include "core/common/api/kernel_int.h"

#include "xrt/util/td.h"
#include <limits>

#include <iostream>

#ifdef _WIN32
#pragma warning( push )
#pragma warning ( disable : 4245 )
#endif

namespace xocl {

class compute_unit;

class kernel : public refcount, public _cl_kernel
{
  using memidx_bitmask_type = xclbin::memidx_bitmask_type;
  using memory_vector_type = std::vector<ptr<memory>>;
  using memory_iterator_type = ptr_iterator<memory_vector_type::iterator>;

 public:
  // OpenCL specific runtime argument types
  enum class rtinfo_type { dim, goff, gsize, lsize, ngrps, gid, lid, grid, printf };

  // Kernel arguments constructed from xclbin meta data.  Captures
  // argument types and data needed for OpenCL configuration of
  // xrt::run objects.
  class xargument
  {
  public:
    using arginfo_type = xrt_core::xclbin::kernel_argument;

    xargument(kernel* kernel, const arginfo_type* ainfo)
      : m_kernel(kernel), m_arginfo(ainfo) {}

    virtual ~xargument();

    virtual void set(const void* value, size_t sz)
    { throw error(CL_INVALID_BINARY, "Cannot set argument"); }
    virtual void set_svm(const void* value, size_t sz)
    { throw error(CL_INVALID_BINARY, "Cannot set svm argument"); }
    virtual void add(const arginfo_type* ainfo)
    { throw error(CL_INVALID_BINARY, "Cannot add component to argument"); }
    virtual memory* get_memory_object() const
    { return nullptr; }
    virtual size_t get_arginfo_idx() const 
    { throw error(CL_INVALID_BINARY, "arginfo index not accessible"); }
    virtual rtinfo_type get_rtinfo_type() const
    { throw error(CL_INVALID_BINARY, "rtinfo type not accessible"); }

    size_t get_argidx() const { return m_arginfo->index; }
    size_t get_hostsize() const { return m_arginfo->hostsize; }
    bool is_set() const { return m_set; }
    std::string get_name() const { return m_arginfo->name; }
    std::string get_hosttype() const { return m_arginfo->hosttype; }
    arginfo_type::argtype get_argtype() const { return m_arginfo->type; }

  protected:
    kernel* m_kernel;
    const arginfo_type* m_arginfo;
    bool m_set = false;
  };

  class scalar_xargument : public xargument
  {
  public:
    scalar_xargument(kernel* kernel, const arginfo_type* ainfo)
      : xargument(kernel, ainfo), m_sz(ainfo->hostsize) {}
    virtual void set(const void* value, size_t sz);
    virtual void add(const arginfo_type* ainfo) { m_sz += ainfo->hostsize; }
  private:
    size_t m_sz = 0;   // > m_arginfo.hostsize if components (long2)
  };

  class global_xargument : public xargument
  {
  public:
    global_xargument(kernel* kernel, const arginfo_type* ainfo)
      : xargument(kernel, ainfo) {}
    virtual void set(const void* value, size_t sz);
    virtual void set_svm(const void* value, size_t sz);
    virtual memory* get_memory_object() const { return m_buf.get(); }
  private:
    ptr<memory> m_buf;   // retain ownership
  };

  class local_xargument : public xargument
  {
  public:
    local_xargument(kernel* kernel, const arginfo_type* ainfo)
      : xargument(kernel, ainfo) {}
    virtual void set(const void* value, size_t sz);
  };

  class stream_xargument : public xargument
  {
  public:
    stream_xargument(kernel* kernel, const arginfo_type* ainfo)
      : xargument(kernel, ainfo) { m_set = true; }
    virtual void set(const void* value, size_t sz);
  };

  class rtinfo_xargument : public scalar_xargument
  {
  public:
    rtinfo_xargument(kernel* kernel, const arginfo_type* ainfo, rtinfo_type rtt, size_t index)
      : scalar_xargument(kernel, ainfo), m_rtinfo_type(rtt), m_arginfo_idx(index)
    {}
    virtual rtinfo_type get_rtinfo_type() const { return m_rtinfo_type; }
    virtual size_t get_arginfo_idx() const { return m_arginfo_idx; }
  private:
    rtinfo_type m_rtinfo_type;
    size_t m_arginfo_idx;
  };

  class printf_xargument : public global_xargument
  {
  public:
    printf_xargument(kernel* kernel, const arginfo_type* ainfo, size_t index)
      : global_xargument(kernel, ainfo), m_arginfo_idx(index)
    {}
    virtual rtinfo_type get_rtinfo_type() const { return rtinfo_type::printf; }
    virtual size_t get_arginfo_idx() const { return m_arginfo_idx; }
  private:
    size_t m_arginfo_idx;
  };

private:
  using xargument_value_type = std::unique_ptr<xargument>;
  using xargument_vector_type = std::vector<xargument_value_type>;
  using xargument_iterator_type = xargument_vector_type::const_iterator;
  using xargument_filter_type = std::function<bool(const xargument_value_type&)>;
public:
  // only program constructs kernels, but private doesn't work as long
  // std::make_unique is used
  friend class program; // only program constructs kernels
  kernel(program* prog, const std::string& name, xrt::xclbin::kernel xk);

public:
  virtual ~kernel();

  // Get unique id for this kernel object
  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  // Get unique id for the kernel symbol associated with this object
  const void*
  get_symbol_uid() const
  {
    return m_xkernel.get_handle().get();
  }

  // Get the program used to construct this kernel object
  program*
  get_program() const
  {
    return m_program.get();
  }

  // Get the context associated with the program from which
  // this kernel was contructed
  context*
  get_context() const;

  // Get name of kernel
  const std::string&
  get_name() const
  {
    return m_name;
  }

  std::string
  get_attributes() const
  {
    return "";
  }

  size_t
  get_wg_size() const
  {
    return m_properties.workgroupsize;
  }

  // Get compile work group size per xclbin
  range<const size_t*>
  get_compile_wg_size_range() const
  {
    return range<const size_t*>(m_properties.compileworkgroupsize.data(),m_properties.compileworkgroupsize.data()+3);
  }

  //  Get max work group size per xclbin
  range<const size_t*>
  get_max_wg_size_range() const
  {
    return range<const size_t*>(m_properties.maxworkgroupsize.data(),m_properties.maxworkgroupsize.data()+3);
  }

  decltype(xrt_core::xclbin::kernel_properties::stringtable)
  get_stringtable() const
  {
    return m_properties.stringtable;
  }

  bool
  has_printf() const
  {
    return m_printf_xargs.size()>0;
  }

  bool
  is_built_in() const
  {
    return false;
  }

  // Arguments (except global cl_mem) are set directly on the run
  // object in the kernel register map.  This avoids any local copies
  // of the data and simplifies starting of the kernel
  void
  set_run_arg_at_index(unsigned long idx, const void* cvalue, size_t sz);

  // Set kernel argument value for specified argument
  void
  set_argument(unsigned long idx, size_t sz, const void* arg);

  // Set the svm virtual memory argument 
  void
  set_svm_argument(unsigned long idx, size_t sz, const void* arg);

  // Set the printf global cl_memory argument 
  void
  set_printf_argument(size_t sz, const void* arg);

  // Get argument info meta data for specified argument
  const xrt_core::xclbin::kernel_argument*
  get_arg_info(unsigned long idx) const;

  // Get argument value (if any) for specified argument
  std::vector<uint32_t>
  get_arg_value(unsigned long idx) const;

  // Get iterateble range of all kernel arguments
  joined_range<const xargument_vector_type, const xargument_vector_type>
  get_xargument_range() const
  {
    return boost::join(m_indexed_xargs,m_printf_xargs);
  }

  const xargument_vector_type&
  get_indexed_xargument_range() const
  {
    return m_indexed_xargs;
  }

  const xargument_vector_type&
  get_rtinfo_xargument_range() const
  {
    return m_rtinfo_xargs;
  }

  const xargument_vector_type&
  get_printf_xargument_range() const
  {
    return m_printf_xargs;
  }

  // Get list of CUs that can be used by this kernel object
  std::vector<const compute_unit*>
  get_cus() const
  {
    return m_cus;
  }

  // Get number of CUs that can be used by this kernel object
  size_t
  get_num_cus() const
  {
    return m_cus.size();
  }

  // The the underlying xrt::kernel object for specified device
  // Default to first kernel object.
  const xrt::kernel&
  get_xrt_kernel(const device* device = nullptr) const;

  // The the underlying xrt::run object for specified device
  // Default to first run object.
  const xrt::run&
  get_xrt_run(const device* device = nullptr) const;


  // Get the set of memory banks an argument can connect to given the
  // current set of kernel compute units for specified device
  //
  // @param dev
  //  Targeted device for connectivity check
  // @param argidx
  //  The argument index to check connectivity for
  // @return
  //  Bitset with mapping indicies to possible bank connections
  memidx_bitmask_type
  get_memidx(const device* dev, unsigned int arg) const;

  // Validate current list of CUs that can be used by this kernel
  //
  // Internal validated list of CUs is updated / trimmed to those that
  // support argument at @argidx connected to memory bank at @memidx
  //
  // @param dev
  //  Targeted device for connectivity check
  // @param argidx
  //  The argument index to validate
  // @param memidx
  //  The memory index that must be used by argument
  size_t
  validate_cus(const device* dev, unsigned long argidx, int memidx) const;

  // Error message for exceptions when connectivity checks fail
  //
  // @return
  //  Current kernel argument connectivity
  std::string
  connectivity_debug() const;

private:
  // Compute units that can be used by this kernel object The list is
  // dynamically trimmed as kernel arguments are added and validated.
  // Mutable because it is an implementation detail that the list is
  // trimmed dynamically for the purpose of validation - yet not a cool
  // contract.
  mutable std::vector<const compute_unit*> m_cus;

  // Select a CU for argument buffer
  const compute_unit*
  select_cu(const device* dev) const;
  const compute_unit*
  select_cu(const memory* buf) const;

  // Assign a buffer argument to a argidx and if possible validate CUs
  // now otherwise postpone validate to later.
  void
  assign_buffer_to_argidx(memory* mem, unsigned long argidx);

private:
  unsigned int m_uid = 0;
  ptr<program> m_program;     // retain reference
  std::string m_name;

  // xclbin meta
  xrt::xclbin::kernel m_xkernel;
  const xrt_core::xclbin::kernel_properties& m_properties;

  xargument_vector_type m_indexed_xargs;
  xargument_vector_type m_rtinfo_xargs;
  xargument_vector_type m_printf_xargs;

  // One run object per device in m_program
  struct xkr { xrt::kernel xkernel; xrt::run xrun; };
  std::map<const device*, xkr> m_xruns;

  // Arguments in indexed order per xrt::kernel object
  using xarg = xrt_core::xclbin::kernel_argument;
  std::vector<const xarg*> m_arginfo;
};

namespace kernel_utils {

std::string
normalize_kernel_name(const std::string& kernel_name);

std::vector<std::string>
get_cu_names(const std::string& kernel_name);

} // kernel_utils

} // xocl

#ifdef _WIN32
#pragma warning( pop )
#endif

#endif
