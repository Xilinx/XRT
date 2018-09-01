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

#ifndef xocl_core_kernel_h_
#define xocl_core_kernel_h_

#include "xocl/core/object.h"
#include "xocl/core/refcount.h"
#include "xocl/core/memory.h"
#include "xocl/xclbin/xclbin.h"

#include "xrt/util/td.h"
#include <limits>

#include <iostream>

namespace xocl {

class kernel : public refcount, public _cl_kernel
{
public:

  /**
   * class argument is a class hierarchy that represents a kernel
   * object argument constructed from xclbin::symbol::arg meta data.
   *
   * The class is in flux, and will change much as upstream cu_ffa
   * and execution context are adapated.
   */
  class argument
  {
  public:
    using argtype = xclbin::symbol::arg::argtype;
    using arginfo_type = const xclbin::symbol::arg*;
    using arginfo_vector_type = std::vector<arginfo_type>;
    using arginfo_iterator_type = arginfo_vector_type::const_iterator;
    using arginfo_range_type = range<arginfo_iterator_type>;

  private:
    /**
     * Get the type of the argument
     * enum class argtype { indexed, printf, progvar, rtinfo };
     */
    virtual argtype
    get_argtype() const
    { throw std::runtime_error("not implemented"); }

  public:

    argument(kernel* kernel) : m_kernel(kernel) {}

    bool
    is_set() const
    { return m_set; }

    void
    set(unsigned long argidx, size_t sz, const void* arg)
    {
      m_argidx = argidx;
      set(sz,arg);
    }

    /**
     * @return
     *   The kernel argument index of this argument
     */
    unsigned long
    get_argidx() const
    {
      return m_argidx;
    }

    /**
     * Get argument address qualifier
     *
     * This is a simple translation of address space per
     * get_address_space
     */
    cl_kernel_arg_address_qualifier
    get_address_qualifier() const;

    bool
    is_indexed() const
    { return get_argtype()==argtype::indexed; }

    bool
    is_printf() const
    { return get_argtype()==argtype::printf; }

    bool
    is_progvar() const
    { return get_argtype()==argtype::progvar; }

    bool
    is_rtinfo() const
    { return get_argtype()==argtype::rtinfo; }

    virtual ~argument() {}

    /**
     * Clone an argument when a kernel object is bound to an
     * execution context.   This allows the same kernel object
     * to be used by multiple contexts at the same time, per
     * OpenCL requirements.
     *
     * Asserts that the argument has been set, otherwise
     * it makes no sense to clone it.
     */
    virtual std::unique_ptr<argument>
    clone() = 0;

    /**
     * Set an argument (clSetKernelArg) to some value.
     */
    virtual void
    set(size_t sz, const void* arg) = 0;

    /**
     * Set an svm argument (clSetKernelArgSVMPointer) to some value.
     */
    virtual void
    set_svm(size_t sz, const void* arg)
    { throw std::runtime_error("not implemented"); }

    /**
     * Add a component to an existing argument.
     *
     * Implemented for scalar args only.  Some arguments, e.g. long2,
     * int4, etc. are associated with mutiple arginfo meta entries.
     *
     * @param arg
     *   The arginfo meta data for this component of the argument
     * @return
     *   Argument size after component was added.
     */
    virtual size_t
    add(arginfo_type arg)
    { throw xocl::error(CL_INVALID_BINARY,"Cannot add component to argument"); }

    /**
     */
    virtual size_t
    get_address_space() const
    { throw std::runtime_error("not implemented"); }

    /**
     * Get argument name
     */
    virtual std::string
    get_name() const
    { throw std::runtime_error("not implemented"); }

    /**
     * Get the backing memory object of the argument if any.
     *
     * Implmented for constant and global args.
     *
     * @return
     *   xocl::memory pointer or nullptr if argument is not backed
     *   by a cl_mem object
     */
    virtual memory*
    get_memory_object() const
    { return nullptr; }

    /**
     * Get the svm pointer
     *
     * Implmented for global args.
     *
     * @return
     *   void* pointer or nullptr if argument is not svm
     */
    virtual void*
    get_svm_object() const
    { throw std::runtime_error("not implemented"); }

    /**
     *
     */
    virtual size_t
    get_size() const
    { return 0; }

    /**
     */
    virtual const void*
    get_value() const
    { return nullptr; }

    virtual const std::string
    get_string_value() const
    { throw std::runtime_error("not implemented"); }

    virtual size_t
    get_baseaddr() const
    { throw std::runtime_error("not implemented"); }

    virtual std::string
    get_linkage() const
    { throw std::runtime_error("not implemented"); }

    /**
     * Get component argument info range
     */
    virtual arginfo_range_type
    get_arginfo_range() const
    { throw std::runtime_error("not implemented"); }

    static std::unique_ptr<kernel::argument>
      create(arginfo_type arg,kernel* kernel);

  protected:
    kernel* m_kernel = nullptr;
    unsigned long m_argidx = std::numeric_limits<unsigned long>::max();
    bool m_set = false;
  };

  class scalar_argument : public argument
  {
  public:
    scalar_argument(arginfo_type arg,kernel* kernel)
      : argument(kernel), m_sz(arg->hostsize)
    {
      m_components.push_back(arg);
    }
    virtual std::string get_name() const { return (*m_components.begin())->name; }
    virtual argtype get_argtype() const  { return (*m_components.begin())->atype; }
    virtual size_t get_address_space() const { return 0; }
    virtual std::unique_ptr<argument> clone();
    virtual size_t add(arginfo_type arg);
    virtual void set(size_t sz, const void* arg);
    virtual size_t get_size() const { return m_sz; }
    virtual const void* get_value() const { return m_value.data(); }
    virtual const std::string get_string_value() const;
    virtual arginfo_range_type get_arginfo_range() const
    { return arginfo_range_type(m_components.begin(),m_components.end()); }
  private:
    size_t m_sz;
    std::vector<uint8_t> m_value;

    // components of the argument (long2, int4, etc)
    arginfo_vector_type m_components;
  };

  class global_argument : public argument
  {
  public:
    global_argument(arginfo_type arg, kernel* kernel)
      : argument(kernel), m_arg_info(arg) {}
    virtual argtype get_argtype() const { return m_arg_info->atype; }
    virtual std::string get_name() const { return m_arg_info->name; }
    virtual size_t get_address_space() const { return 1; }
    virtual std::unique_ptr<argument> clone();
    void set(size_t sz, const void* arg) ;
    void set_svm(size_t sz, const void* arg) ;
    virtual memory* get_memory_object() const { return m_buf.get(); }
    virtual void* get_svm_object() const { return m_svm_buf; }
    virtual size_t get_size() const { return sizeof(memory*); }
    virtual const void* get_value() const { return m_buf.get(); }
    virtual size_t get_baseaddr() const { return m_arg_info->baseaddr; }
    virtual std::string get_linkage() const { return m_arg_info->linkage; }
    virtual arginfo_range_type get_arginfo_range() const
    { return arginfo_range_type(&m_arg_info,&m_arg_info+1); }
  private:
    ptr<memory> m_buf;   // retain ownership
    void* m_svm_buf = nullptr;
    arginfo_type m_arg_info;
  };

#if 0 // not necessary?
  class progvar_argument : public global_argument
  {
  public:
  progvar_argument(arginfo_type arg, kernel* kernel)
    : m_kernel(kernel), global_argument(arg) {}
  private:
  };
#endif

  class local_argument : public argument
  {
  public:
    local_argument(arginfo_type arg, kernel* kernel)
      : argument(kernel), m_arg_info(arg) {}
    virtual argtype get_argtype() const { return m_arg_info->atype; }
    virtual std::string get_name() const { return m_arg_info->name; }
    virtual size_t get_address_space() const { return 3; }
    virtual std::unique_ptr<argument> clone();
    virtual void set(size_t sz, const void* arg);
    virtual arginfo_range_type get_arginfo_range() const
    { return arginfo_range_type(&m_arg_info,&m_arg_info+1); }
  private:
    arginfo_type m_arg_info;
  };

  class constant_argument : public argument
  {
  public:
    constant_argument(arginfo_type arg, kernel* kernel)
      : argument(kernel), m_arg_info(arg) {}
    virtual std::string get_name() const { return m_arg_info->name; }
    virtual argtype get_argtype() const { return m_arg_info->atype; }
    virtual size_t get_address_space() const { return 2; }
    virtual std::unique_ptr<argument> clone();
    virtual void set(size_t sz, const void* arg);
    virtual memory* get_memory_object() const { return m_buf.get(); }
    virtual size_t get_size() const { return sizeof(memory*); }
    virtual const void* get_value() const { return m_buf.get(); }
    virtual arginfo_range_type get_arginfo_range() const
    { return arginfo_range_type(&m_arg_info,&m_arg_info+1); }
  private:
    ptr<memory> m_buf;  // retain ownership
    arginfo_type m_arg_info;
  };

  class image_argument : public argument
  {
  public:
    image_argument(arginfo_type arg, kernel* kernel)
      : argument(kernel) {}
    virtual std::unique_ptr<argument> clone();
    virtual void set(size_t sz, const void* arg);
  };

  class sampler_argument : public argument
  {
  public:
    sampler_argument(arginfo_type arg, kernel* kernel)
      : argument(kernel) {}
    virtual std::unique_ptr<argument> clone();
    virtual void set(size_t sz, const void* arg);
  };

  class stream_argument : public argument
  {
  public:
    stream_argument(arginfo_type arg, kernel* kernel)
      : argument(kernel), m_arg_info(arg) {}
    virtual std::unique_ptr<argument> clone();
    virtual void set(size_t sz, const void* arg);
    virtual argtype get_argtype() const { return m_arg_info->atype; }
    virtual size_t get_address_space() const { return 4; }
  private:
    arginfo_type m_arg_info;
  };

private:
  using argument_value_type = std::unique_ptr<argument>;
  using argument_vector_type = std::vector<argument_value_type>;
  using argument_iterator_type = argument_vector_type::const_iterator;
  using argument_filter_type = std::function<bool(const argument_value_type&)>;

public:
  // only program constructs kernels, but private doesn't work as long
  // xrt::make_unique is used
  friend class program; // only program constructs kernels
  kernel(program* prog, const std::string& name,const xclbin::symbol&);
  kernel(program* prog, const std::string& name);
  explicit kernel(program* prog);

public:
  virtual ~kernel();

  /**
   * @return
   *   Unique id for this kernel object
   */
  unsigned int
  get_uid() const
  {
    return m_uid;
  }

  /**
   * @return
   *   Unique id for the kernel symbol associated with this object
   */
  unsigned int
  get_symbol_uid() const
  {
    return m_symbol.uid;
  }

  /**
   * @return
   *   The kernel symbol used to create this kernel
   */
  const xclbin::symbol&
  get_symbol() const
  {
    return m_symbol;
  }

  program*
  get_program() const
  {
    return m_program.get();
  }

  context*
  get_context() const;

  /**
   * @return
   *   Name of kernel
   */
  const std::string&
  get_name() const
  {
    return m_symbol.name;
  }

  /**
   * @return
   *   Name of kernel
   */
  const std::string&
  get_name_from_constructor() const
  {
    // Remove this function, it is not needed
    // Remove m_name from data members
    // Fix ctor
    if (m_name != m_symbol.name)
      throw std::runtime_error("Internal Error");
    return get_name();
  }
  /**
   * Return list of instances (CUs) in this kernel
   *
   * This function directly access the kernel symbol to
   * extract the names of the embedded kernel instances
   *
   * @return
   *   A vector with the names of the CUs
   */
  std::vector<std::string>
  get_instance_names() const;

  const std::string&
  get_attributes() const
  {
    return m_symbol.attributes;
  }

  size_t
  get_wg_size() const
  {
    return m_symbol.workgroupsize;
  }

  /**
   * Get compile work group size per xclbin
   */
  range<const size_t*>
  get_compile_wg_size_range() const
  {
    return range<const size_t*>(m_symbol.compileworkgroupsize,m_symbol.compileworkgroupsize+3);
  }

  /**
   * Get max work group size per xclbin
   */
  range<const size_t*>
  get_max_wg_size_range() const
  {
    return range<const size_t*>(m_symbol.maxworkgroupsize,m_symbol.maxworkgroupsize+3);
  }

  auto
  get_stringtable() const -> decltype(xclbin::symbol::stringtable)
  {
    return m_symbol.stringtable;
  }

  bool
  has_printf() const
  {
    return m_printf_args.size()>0;
  }

  bool
  is_built_in() const
  {
    return false;
  }

  void
  set_argument(unsigned long idx, size_t sz, const void* arg)
  {
    m_indexed_args.at(idx)->set(idx,sz,arg);
  }

  void
  set_svm_argument(unsigned long idx, size_t sz, const void* arg)
  {
    m_indexed_args.at(idx)->set_svm(sz,arg);
  }

  void
  set_printf_argument(size_t sz, const void* arg)
  {
    m_printf_args.at(0)->set(sz,arg);
  }

  /**
   * Get range of all arguments that have a dynamic value
   * rtinfo args and progvars do not matter, they are static per kernel
   */
  joined_range<const argument_vector_type,joined_range<const argument_vector_type,const argument_vector_type>>
  get_argument_range() const
  {
    auto j1 = boost::join(m_printf_args,m_progvar_args);
    return boost::join(m_indexed_args,j1);
  }

  /**
   * @return Range of indexed arguments
   */
  range<argument_iterator_type>
  get_indexed_argument_range() const
  {
    return range<argument_iterator_type>(m_indexed_args.begin(),m_indexed_args.end());
  }

  /**
   * @return Range of progvar arguments
   */
  range<argument_iterator_type>
  get_progvar_argument_range() const
  {
    return range<argument_iterator_type>(m_progvar_args.begin(),m_progvar_args.end());
  }

  /**
   * @return Range of printf arguments
   */
  range<argument_iterator_type>
  get_printf_argument_range() const
  {
    return range<argument_iterator_type>(m_printf_args.begin(),m_printf_args.end());
  }

  /**
   * Get rtinfo args.
   *
   * This is used by cu_ffa, it does contain printf args, but only the
   * static part of the printf is used in cu_ffa.  Feels awkward, but
   * must wait for better cu_ffa refactoring
   *
   * @return Range of rtinfo arguments
   */
  joined_range<const argument_vector_type, const argument_vector_type>
  get_rtinfo_argument_range() const
  {
    return boost::join(m_printf_args,m_rtinfo_args);
  }

  ////////////////////////////////////////////////////////////////
  // Conformance helpers
  ////////////////////////////////////////////////////////////////
  const std::string& get_hash() const
  {
    return m_symbol.hash;
  }

private:
  unsigned int m_uid = 0;
  ptr<program> m_program;     // retain reference
  std::string m_name;
  const xclbin::symbol& m_symbol;
  argument_vector_type m_indexed_args;
  argument_vector_type m_printf_args;
  argument_vector_type m_progvar_args;
  argument_vector_type m_rtinfo_args;
};

} // xocl

#endif
