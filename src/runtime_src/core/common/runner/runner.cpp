// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
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

#include "core/common/json/nlohmann/json.hpp"

#include <algorithm>
#include <fstream>
#include <istream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

#ifdef _WIN32
# pragma warning (disable: 4100 4189 4505)
#endif

namespace {

using json = nlohmann::json;
const json empty_json;

// load_json() - Load a JSON from in-memory string or file
static json
load_json(const std::string& input)
{
  try {
    // Try parse as in-memory json
    return json::parse(input);
  }
  catch (const json::parse_error&)
  {
    // Not a valid JSON - treat input as a file path
  }

  if (std::ifstream f{input})
    return json::parse(f);

  throw std::runtime_error("Failed to open JSON file: " + input);
}

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

  // Should be std::span, but not until c++20
  virtual const std::string_view
  get(const std::string& path) const = 0;

  // Should be std::span, but not until c++20
  static std::string_view
  to_sv(const std::vector<char>& vec)
  {
    // return {vec.begin(), vec.end()};
    return {vec.data(), vec.size()};
  }
};

// class file_repo - file system artifact repository
// Artifacts are loaded from disk and stored in persistent storage  
class file_repo : public repo
{
  std::filesystem::path base_dir;

public:
  file_repo()
    : base_dir{"."}
  {}

  explicit
  file_repo(std::filesystem::path basedir)
    : base_dir{std::move(basedir)}
  {}

  const std::string_view
  get(const std::string& path) const override
  {
    std::filesystem::path full_path = base_dir / path;
    if (!std::filesystem::exists(full_path))
      throw std::runtime_error{"File not found: " + full_path.string()};

    auto key = full_path.string();
    if (auto it = m_data.find(key); it != m_data.end())
      return to_sv((*it).second);

    std::ifstream ifs(key, std::ios::binary);
    if (!ifs)
      throw std::runtime_error{"Failed to open file: " + key};

    ifs.seekg(0, std::ios::end);
    std::vector<char> data(ifs.tellg());
    ifs.seekg(0, std::ios::beg);
    ifs.read(data.data(), data.size());
    auto [itr, success] = m_data.emplace(key, std::move(data));
    XRT_DEBUGF("artifacts::file_repo::get(%s) -> %s\n", path.c_str(), success ? "success" : "failure");
    
    return to_sv((*itr).second);
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

  const std::string_view
  get(const std::string& path) const override
  {
    if (auto it = m_data.find(path); it != m_data.end())
      return to_sv((*it).second);

    if (auto it = m_reference.find(path); it != m_reference.end()) {
      auto [itr, success] = m_data.emplace(path, it->second);
      XRT_DEBUGF("artifacts::ram_repo::get(%s) -> %s\n", path.c_str(), success ? "success" : "failure");
      return to_sv((*itr).second);
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
get(const std::string& path, const artifacts::repo* repo)
{
  if (auto it = s_path2elf.find(path); it != s_path2elf.end())
    return get((*it).second);

  auto data = repo->get(path);
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
    read_xclbin(const json& j, const artifacts::repo* repo)
    {
      auto path = j.at("xclbin").get<std::string>();
      auto data = repo->get(path);
      return xrt::xclbin{data};
    }

  public:
    header(const json& j, const artifacts::repo* repo)
      : m_xclbin{read_xclbin(j, repo)}
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
      create_buffer(const xrt::device& device, const json& j)
      {
        auto tp = to_type(j.at("type").get<std::string>()); // required, input/output/internal
        auto sz = (tp == type::internal) ? j.at("size").get<size_t>() : 0; // required for internal buffers
        return {device, j.at("name").get<std::string>(), tp, sz};
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
      std::string m_instance;
      xrt::xclbin::kernel m_xclbin_kernel;
      xrt::kernel m_xrt_kernel;

      // Kernel must be in xclbin.  The xclbin was used when the hwctx was
      // constructed.  Here we lookup the xclbin kernel object for additional
      // meta data (may not be needed).
      kernel(const xrt::hw_context& ctx, const xrt::module& mod, std::string name, std::string xname)
        : m_name{std::move(name)}
        , m_instance{std::move(xname)}
        , m_xclbin_kernel{ctx.get_xclbin().get_kernel(m_instance)}
        , m_xrt_kernel{xrt::ext::kernel{ctx, mod, m_instance}}
      {
        XRT_DEBUGF("recipe::resources::kernel(%s, %s)\n", m_name.c_str(), m_instance.c_str());
      }

      // Legacy kernel (alveo)
      kernel(const xrt::hw_context& ctx, std::string name, std::string xname)
        : m_name(std::move(name))
        , m_instance(std::move(xname))
        , m_xclbin_kernel{ctx.get_xclbin().get_kernel(m_instance)}
        , m_xrt_kernel{xrt::kernel{ctx, m_instance}}
      {
        XRT_DEBUGF("recipe::resources::kernel(%s, %s)\n", m_name.c_str(), m_instance.c_str());
      }

    public:
      kernel(const kernel& rhs) = default;
      kernel(kernel&& rhs) = default;
      
      // create_kernel - create a kernel object from a property tree node
      // The kernel control module is created if necessary.
      static kernel
      create_kernel(const xrt::hw_context& hwctx, const json& j,
                    const artifacts::repo* repo)
      {
        auto name = j.at("name").get<std::string>(); // required, default xclbin kernel name
        auto elf = j.value<std::string>("ctrlcode", ""); // optional elf file
        if (elf.empty())
          return kernel{hwctx, name, j.value("instance", name)};

        auto mod = module_cache::get(elf, repo);
        return kernel{hwctx, mod, name, j.value("instance", name)};
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

      cpu(std::string&& name, std::string&& path)
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
      create_cpu(const json& j)
      {
        auto name = j.at("name").get<std::string>(); // required
        auto library_path = xrt_core::environment::xilinx_xrt()
          / j.at("library_name").get<std::string>(); // required
        return cpu{std::move(name), library_path.string()};
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
    create_buffers(const xrt::device& device, const json& j)
    {
      std::map<std::string, buffer> buffers;
      for (const auto& [name, node] : j.items())
        buffers.emplace(node.at("name").get<std::string>(), buffer::create_buffer(device, node));

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
                   const json& j, const artifacts::repo* repo)
    {
      std::map<std::string, kernel> kernels;
      for (const auto& [name, node] : j.items())
        kernels.emplace(node.at("name").get<std::string>(), kernel::create_kernel(hwctx, node, repo));

      return kernels;
    }

    // create_cpus - create cpu objects from cpu property tree nodes
    static std::map<std::string, cpu>
    create_cpus(const json& j)
    {
      std::map<std::string, cpu> cpus;
      for (const auto& [name, node] : j.items())
        cpus.emplace(node.at("name").get<std::string>(), cpu::create_cpu(node));

      return cpus;
    }

  public:
    resources(xrt::device device, const xrt::xclbin& xclbin,
              const json& recipe, const artifacts::repo* repo)
      : m_device{std::move(device)}
      , m_hwctx{m_device, m_device.register_xclbin(xclbin)}
      , m_buffers{create_buffers(m_device, recipe.at("buffers"))}
      , m_kernels{create_kernels(m_device, m_hwctx, recipe.at("kernels"), repo)}
      , m_cpus{create_cpus(recipe.value("cpus", empty_json))} // optional
    {}

    resources(const resources& other)
      : m_device{other.m_device}                             // share device
      , m_hwctx{other.m_hwctx}                               // share hwctx
      , m_buffers{create_buffers(m_device, other.m_buffers)} // new buffers
      , m_kernels{other.m_kernels}                           // share kernels
      , m_cpus{other.m_cpus}                                 // share cpus
    {}

    xrt::device
    get_device() const
    {
      return m_device;
    }

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
      // class argument - represents a execution::run argument
      //
      // The argument refers to a recipe resource buffer.
      //
      // Note, that resource buffers manage their own xrt::bo objects
      // either created as internal xrt::bo or bound from external
      // xrt::bo.  If an argument is copied, then the xrt::bo with the
      // resource buffer is also be copy created.
      struct argument
      {
        resources::buffer m_buffer;

        // Buffer object for the argument.  This can be a sub-buffer
        // if the argument is sliced or it can be null bo if the
        // argument is unbound.
        size_t m_offset;
        size_t m_size;      // 0 indicates the entire buffer
        int m_argidx;

        xrt::bo m_xrt_bo;   // sub-buffer if m_size > 0

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

        argument(const resources& resources, const json& j)
          : m_buffer{resources.get_buffer_or_error(j.at("name").get<std::string>())}
          , m_offset{j.value<size_t>("offset", 0)}
          , m_size{j.value<size_t>("size", 0)}
          , m_argidx{j.at("argidx").get<int>()}
          , m_xrt_bo{create_xrt_bo(m_buffer, m_offset, m_size)}
        {
            XRT_DEBUGF("recipe::execution::run::argument(json) (%s, %lu, %lu, %d) bound(%s)\n",
                     m_buffer.get_name().c_str(), m_offset, m_size, m_argidx,
                     m_xrt_bo ? "true" : "false");
        }

        // Copy constructor.  Allocates new resources::buffer and new
        // XRT buffer object.
        argument(const resources& resources, const argument& other)
          : m_buffer{resources::buffer::create_buffer           // new resources:buffer
                     (resources.get_device(), other.m_buffer)}  // (see earlier comment)
          , m_offset{other.m_offset}                            // same offset
          , m_size{other.m_size}                                // same size
          , m_argidx{other.m_argidx}                            // same argidx
          , m_xrt_bo{create_xrt_bo(m_buffer, m_offset, m_size)} // new xrt::bo, maybe null
        {
            XRT_DEBUGF("recipe::execution::run::argument(other) (%s, %lu, %lu, %d) bound(%s)\n",
                     m_buffer.get_name().c_str(), m_offset, m_size, m_argidx,
                     m_xrt_bo ? "true" : "false");
        }

        void
        bind(const xrt::bo& bo)
        {
          // The full bo is bound to the resource buffer.
          m_buffer.bind(bo);
          // The argument specific bo may be a sub-buffer per
          // specified offset and size
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
      create_and_set_args(const resources& resources, run_type run, const json& j)
      {
        std::map<std::string, argument> args;
        for (const auto& [name, node] : j.items()) {
          argument arg {resources, node};
          if (auto bo = arg.get_xrt_bo())
            std::visit(set_arg_visitor{arg.m_argidx, std::move(bo)}, run);

          args.emplace(node.at("name").get<std::string>(), std::move(arg));
        }
        return args;
      }

      static std::map<std::string, argument>
      create_and_set_args(const resources& resources, run_type run,
                          const std::map<std::string, argument>& other_args)
      {
        std::map<std::string, argument> args;
        for (const auto& [name, other_arg] : other_args) {
          argument arg{resources, other_arg};
          if (auto bo = arg.get_xrt_bo())
            std::visit(set_arg_visitor{arg.m_argidx, std::move(bo)}, run);

          args.emplace(name, std::move(arg));
        }
        return args;
      }

      static void
      set_constant_args(run_type run, const json& j)
      {
        for (const auto& [name, node] : j.items()) {
          auto argidx = node.at("argidx").get<int>();
          auto type = node.at("type").get<std::string>();
          if (type == "int")
            std::visit(set_arg_visitor{argidx, node.at("value").get<int>()}, run);
          else if (type == "string")
            std::visit(set_arg_visitor{argidx, node.at("value").get<std::string>()}, run);
          else
            throw std::runtime_error("Unknown constant argument type '" + type + "'");
        }
      }

      static xrt_core::cpu::run
      create_cpu_run(const resources& resources, const json& j)
      {
        auto name = j.at("name").get<std::string>();
        return xrt_core::cpu::run{resources.get_cpu_function_or_error(name)};
      }

      static xrt::run
      create_kernel_run(const resources& resources, const json& j)
      {
        auto name = j.at("name").get<std::string>();
        return xrt::run{resources.get_xrt_kernel_or_error(name)};
      }

      static run_type
      create_run(const resources& resources, const json& j)
      {
        std::string where = j.value("where", "npu");
        if (where == "cpu")
          return create_cpu_run(resources, j);

        return create_kernel_run(resources, j);
      }

      static run_type
      create_run(const resources& resources, const run& other)
      {
        return std::visit(copy_visitor{other.m_name, resources}, other.m_run);
      }

    public:
      run(const resources& resources, const json& j)
        : m_name{j.at("name").get<std::string>()}
        , m_run{create_run(resources, j)}
        , m_args{create_and_set_args(resources, m_run, j.at("arguments"))}
      {
        XRT_DEBUGF("recipe::execution::run(%s)\n", m_name.c_str());

        if (j.contains("constants"))
          set_constant_args(m_run, j.at("constants"));
      }

      // Create a run from another run using argument resources
      // The ctor creates a new xrt::run or cpu::run from other, these
      // runs refer to resources per argument resources.  Arguments
      // to the runs are copied, so this run along with the argument
      // other run are independent in regards to argument data.
      run(const resources& resources, const run& other)
        : m_name{other.m_name}
        , m_run{create_run(resources, other)}
        , m_args{create_and_set_args(resources, m_run, other.m_args)}
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
    create_runs(const resources& resources, const json& j)
    {
      std::vector<run> runs;
      for (const auto& [name, node] : j.items())
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
    execution(const resources& resources, const json& j)
      : m_runs{create_runs(resources, j.at("runs"))}
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

  json m_recipe_json;
  header m_header;
  resources m_resources;
  execution m_execution;

public:
  recipe(xrt::device device, json recipe, const artifacts::repo* repo)
    : m_device{std::move(device)}
    , m_recipe_json(std::move(recipe)) // paren required, else initialized as array
    , m_header{m_recipe_json.at("header"), repo}
    , m_resources{m_device, m_header.get_xclbin(), m_recipe_json.at("resources"), repo}
    , m_execution{m_resources, m_recipe_json.at("execution")}
  {}

  recipe(xrt::device device, const std::string& recipe, const artifacts::repo* repo)
    : recipe{std::move(device), load_json(recipe), repo}
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

// class profile - Execution profile
//
// The profile class controls how a run recipe is bound to external
// resources and how the recipe is executed.
//
// An execution profile can be used to initialize run recipe resources
// at runner initialization time by binding resources per the recipe.
// The calling application can still explicitly bind via the
// xrt::runner APIs, which may override the binding done by the
// execution profile.
class profile
{
  // class bindings - represents the bindings sections of a profile json
  //
  // {
  //   "name": buffer name in recipe
  //   "file": (optional with init) if present use to initialize the buffer
  //   "size": (required if no file) the size of the buffer
  //   "init": (optional) how to initialize a buffer
  //   "validate": how to validate a buffer after execution
  // }
  // 
  // The bindings section specify what xrt::bo objects to create for
  // external buffers. The buffers are bound to the recipe prior to
  // first execution.
  // 
  // A binding can specify a file from which the buffer should be
  // initialized.  If a "file" is specified, the buffer is created with
  // this size unless "size" is also specified, in which case the size
  // is exactly the size of the buffer and max size bytes of file is
  // used to initialize the buffer.
  //
  // If "init" is specified, then it defines how the buffer should be
  // initialzed. "init" takes precedence over "file" if "file" is also
  // specified, potentially overwriting already initialized buffer.
  //
  // If "validate" is specified then it has instructions on how to
  // validate a buffer after executing the recipe.
  class bindings
  {
    // Convenience types for readability
    using name_t = std::string;
    using path_t = std::string;
    using binding_node = json;
    using init_node = json;
    using validate_node = json;

    // Map of resource name to json binding element.
    std::map<name_t, binding_node> m_bindings;

    // Map of resource names to XRT buffer objects.
    std::map<name_t, xrt::bo> m_xrt_bos;

    // Create a map of resource names to json binding nodes
    static std::map<name_t, binding_node>
    init_bindings(const json& j)
    {
      std::map<name_t, binding_node> bindings;
      for (const auto& [name, node] : j.items())
        bindings.emplace(node.at("name").get<std::string>(), node);

      return bindings;
    }

    // Create a map of resource names to XRT buffer objects.
    // Initialize the BO with data from the file if any.
    // The size of the xrt::bo is either the size of the "file"
    // if present, or it is the "size" per json.  An explicit
    // "size" always has precedence.
    static std::map<name_t, xrt::bo>
    create_buffers(const xrt::device& device,
                   const std::map<name_t, binding_node>& bindings,
                   const artifacts::repo* repo)
    {
      std::map<name_t, xrt::bo> bos;
      for (const auto& [name, node] : bindings) {
        auto size = node.value<size_t>("size", 0);
        auto file = node.value<std::string>("file", "");
        auto data = file.empty() ? std::string_view{} : repo->get(file);
        size = size ? size : data.size(); // specified size has precedence
        xrt::bo bo = xrt::ext::bo{device, size};
        if (!data.empty()) {
          auto bo_data = bo.map<char*>();
          std::copy(data.data(), data.data() + std::min<size_t>(size, data.size()), bo_data);
          bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        }
        bos.emplace(node.at("name").get<std::string>(), std::move(bo));
      }
      return bos;
    }

    // Validate a resource buffer per profile.json validate json node
    // "validate": {
    //   "size": 0,   // unused for now
    //   "offset": 0, // unused for now
    //   "file": "gold.bin"
    //  }
    void
    validate_buffer(xrt::bo& bo, const validate_node& node, const artifacts::repo* repo)
    {
      std::string_view golden_data;

      if (node.contains("name")) {
        // validate against another resource
        auto golden_bo = m_xrt_bos.at(node["name"]);
        golden_data = std::string_view{golden_bo.map<char*>(), golden_bo.size()};
      }
      else {
        // validate against content of a file
        golden_data = repo->get(node.at("file").get<std::string>());
      }

      // here we could extract offset and size of region to validate
      
      bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
      auto bo_data = bo.map<char*>();
      if (bo.size() != golden_data.size())
        throw std::runtime_error("Size mismatch during validation");

      if (std::equal(golden_data.data(), golden_data.data() + golden_data.size(), bo_data))
        return;

      // Error
      for (uint64_t i = 0; i < golden_data.size(); ++i) {
        if (golden_data[i] != bo_data[i])
          throw std::runtime_error
            ("gold[" + std::to_string(i) + "] = " + std::to_string(golden_data[i])
             + " does not match bo value in bo " + std::to_string(bo_data[i]));
      }
    }

    // Initialize a resource buffer per the binding json node
    static void
    init_buffer(xrt::bo& bo, const init_node& node)
    {
      // Fill the resource buffer with data
      auto bo_data = bo.map<uint8_t*>();

      if (node.value<bool>("random", false)) {
        static std::random_device rd;
        std::generate(bo_data, bo_data + bo.size(), [&]() { return static_cast<uint8_t>(rd()); });
      }
      else {
        // Get the pattern, which must be one character
        auto pattern = node.at("pattern").get<std::string>();
        if (pattern.size() != 1)
          throw std::runtime_error("pattern size must be 1");
      
        std::fill(bo_data, bo_data + bo.size(), pattern[0]);
      }

      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

  public:
    bindings() = default;

    bindings(const xrt::device& device, const json& j, const artifacts::repo* repo)
      : m_bindings{init_bindings(j)}
      , m_xrt_bos{create_buffers(device, m_bindings, repo)}
    {}

    // Validate resource buffers per json.  Validation is per bound buffer
    // as defined in the profile json.
    void
    validate(const artifacts::repo* repo)
    {
      for (auto& [name, node] : m_bindings) {
        if (node.contains("validate"))
          validate_buffer(m_xrt_bos.at(name), node.at("validate"), repo);
      }
    }

    // Init bindings per json.  Initialization is done by filling a
    // pattern into a buffer that requires initialization.  The
    // pattern is currently limited to a single character.
    void
    init()
    {
      for (auto& [name, node] : m_bindings) {
        if (node.contains("init"))
          init_buffer(m_xrt_bos.at(name), node.at("init"));
      }
    }

    // Bind resources to the recipe per json
    void
    bind(recipe& rr)
    {
      for (auto& [name, node] : m_bindings) {
        if (node.value<bool>("bind", false))
          rr.bind(name, m_xrt_bos.at(name));
      }
    }
  }; // class profile::bindings

  // class execution - represents the execution section of a profile json
  //
  // {
  //  "execution" : {
  //    "iterations": 2,
  //    "iteration" : {
  //      "bind": false,
  //      "init": true,
  //      "wait": true,
  //      "validate": true
  //    }
  //  }
  //
  // The execution section specifies how a recipe should be executed.
  // Number of iterations specfies how many times the recipe should be
  // executed when the application calls xrt::runnner::execute().
  //
  // The behavior of an iteration is within the iteration sub-node.
  // - "bind" indicates if buffers should be re-bound to the
  //   recipe before an iteration.
  // - "init" indicates if buffer should be initialized per what is
  //    specified in the binding element.
  // - "wait" says that execution should wait for completion between
  //    iterations and after last iteration.
  // - "validate" means buffer validation per what is specified in
  //   the binding element.
  class execution
  {
    using iteration_node = json;
    profile* m_profile;
    size_t m_iterations;
    iteration_node m_iteration;

    void
    execute_iteration(size_t idx)
    {
      // (Re)bind buffers to recipe if requested
      if (m_iteration.value("bind", false))
        m_profile->bind();
      
      // Initialize buffers if requested
      if (m_iteration.value("init", false))
        m_profile->init();
      
      m_profile->execute_recipe();

      // Wait execution to complete if requested
      if (m_iteration.value("wait", false))
        m_profile->wait_recipe();

      // Validate if requested (implies wait)
      if (m_iteration.value("validate", false))
        m_profile->validate();
    }

  public:
    execution(profile* pr, const json& j)
      : m_profile(pr)
      , m_iterations(j.at("iterations").get<size_t>())
      , m_iteration(j.at("iteration"))
    {
      // Bind buffers to the recipe prior to executing the recipe. This
      // will bind the buffers which have binding::bind set to true.
      m_profile->bind();
    }

    // Execute the profile
    void
    execute()
    {
      for (size_t i = 0; i < m_iterations; ++i)
        execute_iteration(i);
    }
    
  }; // class profile::execution
  
private:
  friend class bindings;  // embedded class
  friend class execution; // embedded class
  json m_profile_json;
  std::shared_ptr<artifacts::repo> m_repo;

  recipe m_recipe;
  bindings m_bindings;
  execution m_execution;

  void
  bind()
  {
    m_bindings.bind(m_recipe);
  }

  void
  init()
  {
    m_bindings.init();
  }

  void
  validate()
  {
    m_bindings.validate(m_repo.get());
  }

  void
  execute_recipe()
  {
    m_recipe.execute();
  }

  void
  wait_recipe()
  {
    m_recipe.wait();
  }

public:
  // profile - constructor
  //
  // Reads json, creates xrt::bo bindings to recipe and initializes
  // execution. The respository is used for looking up artifacts.
  // The recipe is what the profile binds to and what it executes.
  profile(const xrt::device& device,
          const std::string& recipe,
          const std::string& profile,
          std::shared_ptr<artifacts::repo> repo)
    : m_profile_json{load_json(profile)}
    , m_repo{std::move(repo)}
    , m_recipe{device, load_json(recipe), m_repo.get()}
    , m_bindings{device, m_profile_json.at("bindings"), m_repo.get()}
    , m_execution(this, m_profile_json.at("execution"))
  {}

  void
  bind(const std::string& name, const xrt::bo& bo)
  {
    m_recipe.bind(name, bo);
  }

  void
  execute()
  {
    m_execution.execute();
  }

  void
  wait()
  {
    // waiting is controlled through execution in json
    // so a noop here
  }
}; // class profile

} // namespace

namespace xrt_core {

// class runner_impl - Base class API for implementations of
// xrt::runner
class runner_impl
{
public:
  virtual ~runner_impl() = default;

  void
  bind_input(const std::string& name, const xrt::bo& bo)
  {
    bind(name, bo);
  }

  void
  bind_output(const std::string& name, const xrt::bo& bo)
  {
    bind(name, bo);
  }

  virtual void
  bind(const std::string& name, const xrt::bo& bo) = 0;

  virtual void
  execute() = 0;

  virtual void
  wait() = 0;
};

// class recipe_impl - Insulated implementation of xrt::runner
//
// Manages a run recipe.
//
// The recipe defines resources and how to run a model.
class recipe_impl : public runner_impl
{
  recipe m_recipe;

public:
  recipe_impl(const xrt::device& device, const std::string& recipe,
              const std::shared_ptr<artifacts::repo>& repo)
    : m_recipe{device, recipe, repo.get()}
  {}

  void
  bind(const std::string& name, const xrt::bo& bo) override
  {
    m_recipe.bind(name, bo);
  }

  void
  execute() override
  {
    m_recipe.execute();
  }

  void
  wait() override
  {
    m_recipe.wait();
  }
}; // class runner_impl

// class profile_impl - Insulated implementaton of xrt::runner
//
// Manages a profile for how to run a recipe.
//
// The profile controls how resources are bound to a recipe and how
// the recipe is executed, e.g. number of times, debug info,
// validation, etc.
class profile_impl : public runner_impl
{
  profile m_profile;

public:
  profile_impl(const xrt::device& device,
               const std::string& recipe, const std::string& profile,
               const std::shared_ptr<artifacts::repo>& repo)
    : m_profile{device, recipe, profile, repo}
  {}

  void
  bind(const std::string& name, const xrt::bo& bo) override
  {
    m_profile.bind(name, bo);
  }

  void
  execute() override
  {
    m_profile.execute();
  }

  void
  wait() override
  {
    m_profile.wait();
  }
}; // class profile_impl

////////////////////////////////////////////////////////////////
// Public runner interface APIs
////////////////////////////////////////////////////////////////
runner::
runner(const xrt::device& device,
       const std::string& recipe)
  : m_impl{std::make_unique<recipe_impl>
           (device, recipe, std::make_shared<artifacts::file_repo>())}
{} 
  
runner::
runner(const xrt::device& device,
       const std::string& recipe,
       const std::filesystem::path& dir)
  : m_impl{std::make_unique<recipe_impl>
           (device, recipe, std::make_shared<artifacts::file_repo>(dir))}
{} 

runner::
runner(const xrt::device& device,
       const std::string& recipe,
       const artifacts_repository& repo)
  : m_impl{std::make_unique<recipe_impl>
           (device, recipe, std::make_shared<artifacts::ram_repo>(repo))}
{}

runner::
runner(const xrt::device& device,
       const std::string& recipe, const std::string& profile)
  : m_impl{std::make_unique<profile_impl>
           (device, recipe, profile, std::make_shared<artifacts::file_repo>())}
{}

runner::
runner(const xrt::device& device,
       const std::string& recipe, const std::string& profile,
       const std::filesystem::path& dir)
  : m_impl{std::make_unique<profile_impl>
           (device, recipe, profile, std::make_shared<artifacts::file_repo>(dir))}
{}

runner::
runner(const xrt::device& device,
       const std::string& recipe, const std::string& profile,
       const artifacts_repository& repo)
  : m_impl{std::make_unique<profile_impl>
           (device, recipe, profile, std::make_shared<artifacts::ram_repo>(repo))}
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
