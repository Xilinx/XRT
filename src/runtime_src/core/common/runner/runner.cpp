// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
#define XCL_DRIVER_DLL_EXPORT  // in same dll as exported xrt apis
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#define XRT_API_SOURCE         // in same dll as coreutil

#define XRT_VERBOSE
#include "runner.h"
#include "cpu.h"

#include "core/common/debug.h"
#include "core/common/dlfcn.h"
#include "core/common/error.h"
#include "core/common/module_loader.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_device.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"
#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/experimental/xrt_ext.h"
#include "core/include/xrt/experimental/xrt_kernel.h"
#include "core/include/xrt/experimental/xrt_module.h"
#include "core/include/xrt/experimental/xrt_queue.h"
#include "core/include/xrt/experimental/xrt_xclbin.h"

#ifdef _WIN32
# pragma warning (push)
# pragma warning (disable: 4702)
#endif
#include "boost/property_tree/json_parser.hpp"
#include "boost/property_tree/ptree.hpp"
#ifdef _WIN32
# pragma warning (pop)
#endif

#include <istream>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#ifdef _WIN32
# pragma warning (disable: 4100 4189 4505)
#endif

namespace {

const boost::property_tree::ptree default_ptree;

// struct streambuf - wrap a std::streambuf around an external buffer
//
// This is used create elf files from memory through a std::istream
struct streambuf : public std::streambuf
{
  streambuf(char* begin, char* end)
  {
    setg(begin, begin, end);
  }

  template <typename T>
  streambuf(T* begin, T* end)
    : streambuf(reinterpret_cast<char*>(begin), reinterpret_cast<char*>(end))
  {}

  template <typename T>
  streambuf(const T* begin, const T* end) // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    : streambuf(const_cast<T*>(begin), const_cast<T*>(end))
  {}

  std::streampos
  seekpos(std::streampos pos, std::ios_base::openmode which) override
  {
    setg(eback(), eback() + pos, egptr());
    return gptr() - eback();
  }

  std::streampos
  seekoff(std::streamoff off, std::ios_base::seekdir way, std::ios_base::openmode which) override
  {
    if (way == std::ios_base::cur)
      gbump(static_cast<int>(off));
    else if (way == std::ios_base::end)
      setg(eback(), egptr() + off, egptr());
    else if (way == std::ios_base::beg)
      setg(eback() + off, gptr(), egptr());
    return gptr() - eback();
  }
};

// Artifacts are encoded / referenced in recipe by string.
// The artifacts can be stored in a file system or in memory
// depending on how the recipe is loaded
namespace artifacts {

// class repo - artifact repository
class repo
{
protected:
  mutable std::map<std::string, std::vector<char>> m_data;

public:
  virtual ~repo() = default;

  virtual const std::vector<char>&
  get(const std::string& path) const = 0;
};

// class file_repo - file system artifact repository
// Artifacts are loaded from disk and stored in persistent storage  
class file_repo : public repo
{
public:
  const std::vector<char>&
  get(const std::string& path) const override
  {
    if (auto it = m_data.find(path); it != m_data.end())
      return (*it).second;

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs)
      throw std::runtime_error{"Failed to open file: " + path};

    ifs.seekg(0, std::ios::end);
    std::vector<char> data(ifs.tellg());
    ifs.seekg(0, std::ios::beg);
    ifs.read(data.data(), data.size());
    auto [itr, success] = m_data.emplace(path, std::move(data));
    
    return (*itr).second;
  }
};

// class ram_repo - in-memory artifact repository
// Used artifacts are copied to persistent storage
class ram_repo : public repo
{
  const std::map<std::string, std::vector<char>>& m_reference;
public:
  explicit ram_repo(const std::map<std::string, std::vector<char>>& data)
    : m_reference{data}
  {}

  const std::vector<char>&
  get(const std::string& path) const override
  {
    if (auto it = m_data.find(path); it != m_data.end())
      return (*it).second;

    if (auto it = m_reference.find(path); it != m_reference.end()) {
      auto [itr, success] = m_data.emplace(path, it->second);
      return (*itr).second;
    }

    throw std::runtime_error{"Failed to find artifact: " + path};
  }
};

} // namespace artifacts

namespace module_cache {

// Cache of elf files to modules to avoid recreating modules
// referring to the same elf file.
static std::map<std::string, xrt::elf> s_path2elf; // NOLINT
static std::map<xrt::elf, xrt::module> s_elf2mod;  // NOLINT

static xrt::module
get(const xrt::elf& elf)
{
  if (auto it = s_elf2mod.find(elf); it != s_elf2mod.end())
    return (*it).second;

  xrt::module mod{elf};
  s_elf2mod.emplace(elf, mod);
  return mod;
}

static xrt::module
get(const std::string& path, const artifacts::repo& repo)
{
  if (auto it = s_path2elf.find(path); it != s_path2elf.end())
    return get((*it).second);

  auto& data = repo.get(path);
  streambuf buf{data.data(), data.data() + data.size()};
  std::istream is{&buf};
  xrt::elf elf{is};
  s_path2elf.emplace(path, elf);

  return get(elf);
}

} // module_cache

class recipe
{
  // class header - header section of the recipe
  class header
  {
    xrt::xclbin m_xclbin;

    static xrt::xclbin
    read_xclbin(const boost::property_tree::ptree& pt, const artifacts::repo& repo)
    {
      auto path = pt.get<std::string>("xclbin_path");
      auto& data = repo.get(path);
      return xrt::xclbin{data};
    }

  public:
    header(const boost::property_tree::ptree& pt, const artifacts::repo& repo)
      : m_xclbin{read_xclbin(pt, repo)}
    {
      XRT_DEBUGF("Loaded xclbin: %s\n", m_xclbin.get_uuid().to_string().c_str());
    }

    header(const header&) = default;

    xrt::xclbin
    get_xclbin() const
    {
      return m_xclbin;
    }
  }; // class recipe::header

  // class resources - resource section of the recipe
  class resources
  {
  public:
    class buffer
    {
      std::string m_name;

      enum class type { input, output, internal };
      type m_type;

      size_t m_size;

      // Buffer object is created for internal nodes, but not for
      // input/output which are bound during execution.
      xrt::bo m_xrt_bo;

      // Only internal buffers have a size and are created during
      // as part of loading the recipe.  External buffers are bound
      // during execution.
      buffer(const xrt::device& device, std::string name, type t, size_t sz)
        : m_name(std::move(name))
        , m_type(t)
        , m_size(sz)
        , m_xrt_bo{m_type == type::internal ? xrt::ext::bo{device, m_size} : xrt::bo{}}
      {
        XRT_DEBUGF("recipe::resources::buffer(%s)\n", m_name.c_str());
      }

      // Copy constructor creates a new buffer with same properties as other
      // The xrt::bo is not copied, but a new one is created.
      buffer(const xrt::device& device, const buffer& other)
        : m_name(other.m_name)
        , m_type(other.m_type)
        , m_size(other.m_size)
        , m_xrt_bo{m_type == type::internal ? xrt::ext::bo{device, m_size} : xrt::bo{}}
      {}

      static type
      to_type(const std::string& t)
      {
        if (t == "input")
          return type::input;
        if (t == "output")
          return type::output;
        if (t == "internal")
          return type::internal;

        throw std::runtime_error("Unknown buffer type '" + t + "'");
      }
    public:
      buffer(const buffer& rhs) = default;
      buffer(buffer&& rhs) = default;

      // create_buffer - create a buffer object from a property tree node
      static buffer
      create_buffer(const xrt::device& device, const boost::property_tree::ptree& pt)
      {
        auto tp = to_type(pt.get<std::string>("type")); // required, input/output/internal
        auto sz = (tp == type::internal) ? pt.get<size_t>("size") : 0; // required for internal buffers
        return {device, pt.get<std::string>("name"), tp, sz};
      }

      // create_buffer - create a buffer object from another buffer object
      // This will create a new buffer object with the same properties as the
      // other buffer, but with a new xrt::bo object.
      static buffer
      create_buffer(const xrt::device& device, const buffer& other)
      {
        return {device, other};
      }

      xrt::bo
      get_xrt_bo() const
      {
        return m_xrt_bo;
      }

      std::string
      get_name() const
      {
        return m_name;
      }

      void
      bind(const xrt::bo& bo)
      {
        m_xrt_bo = bo;
      }
    }; // class recipe::resources::buffer

    class kernel
    {
      std::string m_name;
      std::string m_xclbin_name;
      xrt::xclbin::kernel m_xclbin_kernel;
      xrt::kernel m_xrt_kernel;

      // Kernel must be in xclbin.  The xclbin was used when the hwctx was
      // constructed.  Here we lookup the xclbin kernel object for additional
      // meta data (may not be needed).
      kernel(const xrt::hw_context& ctx, const xrt::module& mod, std::string name, std::string xname)
        : m_name{std::move(name)}
        , m_xclbin_name{std::move(xname)}
        , m_xclbin_kernel{ctx.get_xclbin().get_kernel(m_xclbin_name)}
        , m_xrt_kernel{xrt::ext::kernel{ctx, mod, m_xclbin_name}}
      {
        XRT_DEBUGF("recipe::resources::kernel(%s, %s)\n", m_name.c_str(), m_xclbin_name.c_str());
      }

      // Legacy kernel (alveo)
      kernel(const xrt::hw_context& ctx, std::string name, std::string xname)
        : m_name(std::move(name))
        , m_xclbin_name(std::move(xname))
        , m_xclbin_kernel{ctx.get_xclbin().get_kernel(m_xclbin_name)}
        , m_xrt_kernel{xrt::kernel{ctx, m_xclbin_name}}
      {
        XRT_DEBUGF("recipe::resources::kernel(%s, %s)\n", m_name.c_str(), m_xclbin_name.c_str());
      }

    public:
      kernel(const kernel& rhs) = default;
      kernel(kernel&& rhs) = default;
      
      // create_kernel - create a kernel object from a property tree node
      // The kernel control module is created if necessary.
      static kernel
      create_kernel(const xrt::hw_context& hwctx, const boost::property_tree::ptree& pt,
                    const artifacts::repo& repo)
      {
        auto name = pt.get<std::string>("name"); // required, default xclbin kernel name
        auto elf = pt.get<std::string>("ctrlcode", ""); // optional elf file
        if (elf.empty())
          return kernel{hwctx, name, pt.get<std::string>("xclbin_kernel_name", name)};

        auto mod = module_cache::get(elf, repo);
        return kernel{hwctx, mod, name, pt.get<std::string>("xclbin_kernel_name", name)};
      }

      xrt::kernel
      get_xrt_kernel() const
      {
        return m_xrt_kernel;
      }
    }; // class recipe::resources::kernel

    class cpu
    {
    private:
      std::string m_name;
      std::string m_path;
      xrt_core::cpu::function m_fcn;

      cpu(std::string name, std::string path)
        : m_name{std::move(name)}
        , m_path{std::move(path)}
        , m_fcn{m_name, m_path}
      {
        XRT_DEBUGF("recipe::resources::cpu(%s, %s)\n", m_name.c_str(), m_path.c_str());
      }

    public:
      cpu(const cpu& rhs) = default;
      cpu(cpu&& rhs) = default;

      // create_cpu - create a cpu object from a property tree node
      static cpu
      create_cpu(const boost::property_tree::ptree& pt)
      {
        auto name = pt.get<std::string>("name"); // required
        auto library_path = xrt_core::environment::xilinx_xrt()
          / pt.get<std::string>("library_path"); // required
        return cpu{name, library_path.string()};
      }

      xrt_core::cpu::function
      get_function() const
      {
        return m_fcn;
      }
    }; // class recipe::resources::cpu

    xrt::device m_device;
    xrt::hw_context m_hwctx;
    std::map<std::string, buffer> m_buffers;
    std::map<std::string, kernel> m_kernels;
    std::map<std::string, cpu>    m_cpus;

    // create_buffers - create buffer objects from buffer property tree nodes
    static std::map<std::string, buffer>
    create_buffers(const xrt::device& device, const boost::property_tree::ptree& pt)
    {
      std::map<std::string, buffer> buffers;
      for (const auto& [name, node] : pt)
        buffers.emplace(node.get<std::string>("name"), buffer::create_buffer(device, node));

      return buffers;
    }

    // create_buffers - create buffer objects from buffer objects
    // This will create new buffer objects with the same properties as the
    // other buffers, but with new xrt::bo objects.
    static std::map<std::string, buffer>
    create_buffers(const xrt::device& device, const std::map<std::string, buffer>& others)
    {
      std::map<std::string, buffer> buffers;
      for (const auto& [name, other] : others)
        buffers.emplace(name, buffer::create_buffer(device, other));

      return buffers;
    }

    // create_kernels - create kernel objects from kernel property tree nodes
    static std::map<std::string, kernel>
    create_kernels(xrt::device device, const xrt::hw_context& hwctx,
                   const boost::property_tree::ptree& pt, const artifacts::repo& repo)
    {
      std::map<std::string, kernel> kernels;
      for (const auto& [name, node] : pt)
        kernels.emplace(node.get<std::string>("name"), kernel::create_kernel(hwctx, node, repo));

      return kernels;
    }

    // create_cpus - create cpu objects from cpu property tree nodes
    static std::map<std::string, cpu>
    create_cpus(const boost::property_tree::ptree& pt)
    {
      std::map<std::string, cpu> cpus;
      for (const auto& [name, node] : pt)
        cpus.emplace(node.get<std::string>("name"), cpu::create_cpu(node));

      return cpus;
    }

  public:
    resources(xrt::device device, const xrt::xclbin& xclbin,
              const boost::property_tree::ptree& recipe, const artifacts::repo& repo)
      : m_device{std::move(device)}
      , m_hwctx{m_device, m_device.register_xclbin(xclbin)}
      , m_buffers{create_buffers(m_device, recipe.get_child("buffers"))}
      , m_kernels{create_kernels(m_device, m_hwctx, recipe.get_child("kernels"), repo)}
      , m_cpus{create_cpus(recipe.get_child("cpus", default_ptree))} // optional
    {}

    resources(const resources& other)
      : m_device{other.m_device}                             // share device
      , m_hwctx{other.m_hwctx}                               // share hwctx
      , m_buffers{create_buffers(m_device, other.m_buffers)} // new buffers
      , m_kernels{other.m_kernels}                           // share kernels
      , m_cpus{other.m_cpus}                                 // share cpus
    {}

    xrt::hw_context
    get_xrt_hwctx() const
    {
      return m_hwctx;
    }

    xrt::kernel
    get_xrt_kernel_or_error(const std::string& name) const
    {
      auto it = m_kernels.find(name);
      if (it == m_kernels.end())
        throw std::runtime_error("Unknown kernel '" + name + "'");
      return it->second.get_xrt_kernel();
    }

    xrt_core::cpu::function
    get_cpu_function_or_error(const std::string& name) const
    {
      auto it = m_cpus.find(name);
      if (it == m_cpus.end())
        throw std::runtime_error("Unknown cpu '" + name + "'");
      return it->second.get_function();
    }

    resources::buffer
    get_buffer_or_error(const std::string& name) const
    {
      auto it = m_buffers.find(name);
      if (it == m_buffers.end())
        throw std::runtime_error("Unknown buffer '" + name + "'");

      return it->second;
    }
  }; // class recipe::resources

  // class execution - execution section of the recipe
  class execution
  {
    class run
    {
      struct argument
      {
        resources::buffer m_buffer;

        // Buffer object for the argument.  This can be a sub-buffer
        // if the argument is sliced or it can be null bo if the
        // argument is unbound.
        size_t m_offset;
        size_t m_size;   // 0 indicates the entire buffer
        int m_argidx;

        xrt::bo m_xrt_bo;

        static xrt::bo
        create_xrt_bo(const resources::buffer& buffer, size_t offset, size_t size)
        {
          auto bo = buffer.get_xrt_bo();
          if (bo && (bo.size() < size))
            throw std::runtime_error("buffer size mismatch for buffer: " + buffer.get_name());

          if (bo && (size < bo.size()))
            // sub-buffer
            return xrt::bo{bo, size, offset};

          return bo; // may be null bo for unbound buffer arguments
        }

        argument(const resources& resources, const boost::property_tree::ptree& pt)
          : m_buffer{resources.get_buffer_or_error(pt.get<std::string>("name"))}
          , m_offset{pt.get<size_t>("offset", 0)}
          , m_size{pt.get<size_t>("size", 0)}
          , m_argidx{pt.get<int>("argidx")}
          , m_xrt_bo{create_xrt_bo(m_buffer, m_offset, m_size)}
        {
          XRT_DEBUGF("recipe::execution::run::argument(%s, %lu, %lu, %d) bound(%s)\n",
                     m_buffer.get_name().c_str(), m_offset, m_size, m_argidx, m_xrt_bo ? "true" : "false");
        }

        void
        bind(const xrt::bo& bo)
        {
          m_buffer.bind(bo);
          m_xrt_bo = create_xrt_bo(m_buffer, m_offset, m_size);
        }

        xrt::bo
        get_xrt_bo() const
        {
          return m_xrt_bo;
        }
      }; // class recipe::execution::run::argument

      using run_type = std::variant<xrt::run, xrt_core::cpu::run>;
      std::string m_name;
      run_type m_run;
      std::map<std::string, argument> m_args;

      template <typename ArgType>
      struct set_arg_visitor {
        int m_idx;
        ArgType m_value;
        set_arg_visitor(int idx, ArgType&& arg) : m_idx(idx), m_value(std::move(arg)) {}
        void operator() (xrt::run& run) const { run.set_arg(m_idx, m_value); }
        void operator() (xrt_core::cpu::run& run) const { run.set_arg(m_idx, m_value); }
      };

      struct copy_visitor {
        const std::string& m_name;
        const resources& m_res;
        copy_visitor(const std::string& nm, const resources& res) : m_name{nm}, m_res{res} {}
        run_type operator() (const xrt::run&)
        { return xrt::run{m_res.get_xrt_kernel_or_error(m_name)}; };
        run_type operator() (const xrt_core::cpu::run&)
        { return xrt_core::cpu::run{m_res.get_cpu_function_or_error(m_name)}; };
      };

      static std::map<std::string, argument>
      create_and_set_args(const resources& resources, run_type run, const boost::property_tree::ptree& pt)
      {
        std::map<std::string, argument> args;
        for (const auto& [name, node] : pt) {
          argument arg {resources, node};
          if (auto bo = arg.get_xrt_bo())
            std::visit(set_arg_visitor{arg.m_argidx, std::move(bo)}, run);

          args.emplace(node.get<std::string>("name"), std::move(arg));
        }
        return args;
      }

      static void
      set_constant_args(run_type run, const boost::property_tree::ptree& pt)
      {
        for (const auto& [name, node] : pt) {
          auto argidx = node.get<int>("argidx");
          auto type = node.get<std::string>("type");
          if (type == "int")
            std::visit(set_arg_visitor{argidx, node.get<int>("value")}, run);
          else if (type == "string")
            std::visit(set_arg_visitor{argidx, node.get<std::string>("value")}, run);
          else
            throw std::runtime_error("Unknown constant argument type '" + type + "'");
        }
      }

      static xrt_core::cpu::run
      create_cpu_run(const resources& resources, const boost::property_tree::ptree& pt)
      {
        auto name = pt.get<std::string>("name");
        return xrt_core::cpu::run{resources.get_cpu_function_or_error(name)};
      }

      static xrt::run
      create_kernel_run(const resources& resources, const boost::property_tree::ptree& pt)
      {
        auto name = pt.get<std::string>("name");
        return xrt::run{resources.get_xrt_kernel_or_error(name)};
      }

      static run_type
      create_run(const resources& resources, const boost::property_tree::ptree& pt)
      {
        auto where = pt.get<std::string>("where", "npu");
        if (where == "cpu")
          return create_cpu_run(resources, pt);

        return create_kernel_run(resources, pt);
      }

      static run_type
      create_run(const resources& resources, const run& other)
      {
        return std::visit(copy_visitor{other.m_name, resources}, other.m_run);
      }

    public:
      run(const resources& resources, const boost::property_tree::ptree& pt)
        : m_name{pt.get<std::string>("name")}
        , m_run{create_run(resources, pt)}
        , m_args{create_and_set_args(resources, m_run, pt.get_child("arguments"))}
      {
        XRT_DEBUGF("recipe::execution::run(%s)\n", pt.get<std::string>("name").c_str());

        if (auto constants = pt.get_child_optional("constants"))
#if BOOST_VERSION >= 105600
          set_constant_args(m_run, constants.value());
#else
          set_constant_args(m_run, constants.get());
#endif
      }

      // Create a run from another run but using argument resources
      // The ctor creates a new xrt::run or cpu::run from other, these
      // runs refer to resources per argument resources
      run(const resources& resources, const run& other)
        : m_name{other.m_name}
        , m_run{create_run(resources, other)}
      {}

      bool
      is_npu_run() const
      {
        return std::holds_alternative<xrt::run>(m_run);
      }

      bool
      is_cpu_run() const
      {
        return std::holds_alternative<xrt_core::cpu::run>(m_run);
      }

      xrt::run
      get_xrt_run() const
      {
        if (std::holds_alternative<xrt::run>(m_run))
          return std::get<xrt::run>(m_run);

        throw std::runtime_error("recipe::execution::run::get_xrt_run() called on a CPU run");
      }

      xrt_core::cpu::run
      get_cpu_run() const
      {
        if (std::holds_alternative<xrt_core::cpu::run>(m_run))
          return std::get<xrt_core::cpu::run>(m_run);

        throw std::runtime_error("recipe::execution::run::get_cpu_run() called on a GPU run");
      }

      void
      bind(const std::string& name, const xrt::bo& bo)
      {
        auto it = m_args.find(name);
        if (it == m_args.end())
          return; // the argument is not used in this run

        auto& arg = (*it).second;
        arg.bind(bo);
        std::visit(set_arg_visitor{arg.m_argidx, arg.get_xrt_bo()}, m_run);
      }
    }; // class recipe::execution::run

    // struct runlist - a list of runs to execute
    // Need to support CPU and NPU runlists.  The CPU runlist will be
    // a vector of xrt_core::cpu::run objects. The NPU runlist is
    // simply an xrt::runlist object.
    struct runlist
    {
      virtual ~runlist() = default;
      virtual void execute() = 0;
      virtual void wait() {}
    };

    struct cpu_runlist : runlist
    {
      std::vector<xrt_core::cpu::run> m_runs;
      
      void
      execute() override
      {
        for (auto& run : m_runs)
          run.execute();
      }
    };

    struct npu_runlist : runlist
    {
      xrt::runlist m_runlist;

      explicit npu_runlist(const xrt::hw_context& hwctx)
        : m_runlist{hwctx}
      {}

      void
      execute() override
      {
        m_runlist.execute();
      }

      void
      wait() override
      {
        m_runlist.wait();
      }
    };


    std::vector<run> m_runs;
    xrt::queue m_queue;        // Queue that executes the runlists in sequence
    xrt::queue::event m_event; // Event that signals the completion of the last runlist
    std::exception_ptr m_eptr;

    std::vector<std::unique_ptr<runlist>> m_runlists;

    static std::vector<std::unique_ptr<runlist>>
    create_runlists(const resources& resources, const std::vector<run>& runs)
    {
      std::vector<std::unique_ptr<runlist>> runlists;

      // A CPU or NPU runlist is created for each contiguous sequence
      // of CPU runs or NPU runs. The runlist is inserted into a
      // vector of runlists where each individual runlist will be
      // executed in sequence.
      npu_runlist* nrl = nullptr;
      cpu_runlist* crl = nullptr;
      for (const auto& run : runs) {
        if (run.is_npu_run()) {
          if (crl)
            crl = nullptr;

          if (!nrl) {
            auto rl = std::make_unique<npu_runlist>(resources.get_xrt_hwctx());
            nrl = rl.get();
            runlists.push_back(std::move(rl));
          }

          nrl->m_runlist.add(run.get_xrt_run());
        }
        else if (run.is_cpu_run()) {
          if (nrl) 
            nrl = nullptr;

          if (!crl) {
            auto rl = std::make_unique<cpu_runlist>();
            crl = rl.get();
            runlists.push_back(std::move(rl));
          }

          crl->m_runs.push_back(run.get_cpu_run());
        }
      }
      return runlists;
    }

    // create_runs() - create a vector of runs from a property tree
    static std::vector<run>
    create_runs(const resources& resources, const boost::property_tree::ptree& pt)
    {
      std::vector<run> runs;
      for (const auto& [name, node] : pt)
        runs.emplace_back(resources, node);

      return runs;
    }

    // create_runs() - create a vector of runs from existing runs
    // A run object is a variant, the new run objects are created
    // from the variant matching the type of the existing run.
    static std::vector<run>
    create_runs(const resources& resources, const std::vector<run>& others)
    {
      std::vector<run> runs;
      for (const auto& run : others)
        runs.emplace_back(resources, run);

      return runs;
    }

  public:
    // execution() - create an execution object from a property tree
    // The runs are created from the property tree and either xrt::run
    // or cpu::run objects.
    execution(const resources& resources, const boost::property_tree::ptree& recipe)
      : m_runs{create_runs(resources, recipe.get_child("runs"))}
      , m_runlists{create_runlists(resources, m_runs)}
    {}

    // execution() - create an execution object from existing runs
    // New run objects are created from the existing runs.
    execution(const resources& resources, const execution& other)
      : m_runs{create_runs(resources, other.m_runs)}
      , m_runlists{create_runlists(resources, m_runs)}
    {}

    void
    bind(const std::string& name, const xrt::bo& bo)
    {
      // Iterate over all runs and bind the buffer.
      // Note, that not all runs need to use the buffer.
      // Maybe some optimization could be done here.
      for (auto& run : m_runs)
        run.bind(name, bo);
    }

    void
    execute()
    {
      XRT_DEBUGF("recipe::execution::execute()\n");

      // execute_runlist() - execute a runlist synchronously
      // The lambda function is executed asynchronously by an
      // xrt::queue object. The wait is necessary for an NPU runlist,
      // which must complete before next enqueue operation can be
      // executed.  Execution of an NPU runlist is itself asynchronous.
      static auto execute_runlist = [](runlist* runlist, std::exception_ptr& eptr) {
        try {
          runlist->execute();
          runlist->wait(); // needed for NPU runlists, noop for CPU
        }
        catch (const xrt::runlist::command_error&) {
          eptr = std::current_exception();
        }
        catch (const std::exception&) {
          eptr = std::current_exception();
        }
      };

      // A recipe can have multiple runlists. Each runlist can have
      // multiple runs.  Runlists are executed sequentially, execution
      // is orchestrated by xrt::queue which uses one thread to
      // asynchronously (from called pov) execute all runlists
      for (auto& runlist : m_runlists)
        m_event = m_queue.enqueue([this, &runlist] { execute_runlist(runlist.get(), m_eptr); });
    }

    void
    wait()
    {
      XRT_DEBUGF("recipe::execution::wait()\n");
      // Sufficient to wait for last runlist to finish since last list
      // must have waited for all previous lists to finish.
      auto runlist = m_runlists.back().get();
      if (runlist) 
        m_event.wait();

      if (m_eptr)
        std::rethrow_exception(m_eptr);
    }
  }; // class recipe::execution

  xrt::device m_device;

  boost::property_tree::ptree m_recipe;
  header m_header;
  resources m_resources;
  execution m_execution;

  static boost::property_tree::ptree
  load(const std::string& path)
  {
    boost::property_tree::ptree pt;
    boost::property_tree::read_json(path, pt);
    return pt;
  }

public:
  recipe(xrt::device device, const std::string& path, const artifacts::repo& repo)
    : m_device{std::move(device)}
    , m_recipe{load(path)}
    , m_header{m_recipe.get_child("header"), repo}
    , m_resources{m_device, m_header.get_xclbin(), m_recipe.get_child("resources"), repo}
    , m_execution{m_resources, m_recipe.get_child("execution")}
  {}

  recipe(const recipe&) = default;

  void
  bind_input(const std::string& name, const xrt::bo& bo)
  {
    XRT_DEBUGF("recipe::bind_input(%s)\n", name.c_str());
    m_execution.bind(name, bo);
  }

  void
  bind_output(const std::string& name, const xrt::bo& bo)
  {
    XRT_DEBUGF("recipe::bind_output(%s)\n", name.c_str());
    m_execution.bind(name, bo);
  }

  void
  bind(const std::string& name, const xrt::bo& bo)
  {
    XRT_DEBUGF("recipe::bind(%s)\n", name.c_str());
    m_execution.bind(name, bo);
  }

  // The recipe can be executed with its currently bound
  // input and output resources
  void
  execute()
  {
    XRT_DEBUGF("recipe::execute()\n");
    // Verify that all required resources are bound
    // ...

    // Execute the runlist
    m_execution.execute();
  }

  void
  wait()
  {
    XRT_DEBUGF("recipe::wait()\n");
    m_execution.wait();
  }
}; // class recipe

} // namespace

namespace xrt_core {

// class runner_impl -
//
// A runner implementation is default created with one instance of a
// recipe.  But the runner can be used by multiple threads and new
// recipe instances are created for each thread as needed.
//
// The runner can be created from any thread, but member functions
// are thread specific.
class runner_impl
{
  //std::map<std::thread::id, recipe> m_recipes;
  recipe m_recipe;
  //thread_local recipe m_thread_recipe;

public:
  runner_impl(const xrt::device& device, const std::string& recipe)
    : m_recipe{device, recipe, artifacts::file_repo{}}
  {}

  runner_impl(const xrt::device& device, const std::string& recipe, const runner::artifacts_repository& artifacts)
    : m_recipe{device, recipe, artifacts::ram_repo(artifacts)}
  {}

  void
  bind_input(const std::string& name, const xrt::bo& bo)
  {
    m_recipe.bind_input(name, bo);
  }

  void
  bind_output(const std::string& name, const xrt::bo& bo)
  {
    m_recipe.bind_output(name, bo);
  }

  void
  bind(const std::string& name, const xrt::bo& bo)
  {
    m_recipe.bind(name, bo);
  }

  void
  execute()
  {
    m_recipe.execute();
  }

  void
  wait()
  {
    m_recipe.wait();
  }
};

////////////////////////////////////////////////////////////////
// Public runner interface APIs
////////////////////////////////////////////////////////////////
runner::
runner(const xrt::device& device, const std::string& recipe)
  : m_impl{std::make_unique<runner_impl>(device, recipe)}
{} 
  
runner::
runner(const xrt::device& device, const std::string& recipe, const artifacts_repository& repo)
  : m_impl{std::make_unique<runner_impl>(device, recipe, repo)}
{}

void
runner::
bind_input(const std::string& name, const xrt::bo& bo)
{
  m_impl->bind_input(name, bo);
}

// bind_output() - Bind a buffer object to an output tensor
void
runner::
bind_output(const std::string& name, const xrt::bo& bo)
{
  m_impl->bind_output(name, bo);
}

void
runner::
bind(const std::string& name, const xrt::bo& bo)
{
  m_impl->bind(name, bo);
}

// execute() - Execute the runner
void
runner::
execute()
{
  m_impl->execute();
}

void
runner::
wait()
{
  m_impl->wait();
}

} // namespace xrt_core
