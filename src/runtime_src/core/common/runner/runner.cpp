// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024-2025 Advanced Micro Devices, Inc. All rights reserved.
#define XCL_DRIVER_DLL_EXPORT  // in same dll as exported xrt apis
#define XRT_CORE_COMMON_SOURCE // in same dll as coreutil
#define XRT_API_SOURCE         // in same dll as coreutil

#ifdef _DEBUG
//# define XRT_VERBOSE
#endif

#include "runner.h"
#include "cpu.h"

#include "core/common/debug.h"
#include "core/common/dlfcn.h"
#include "core/common/error.h"
#include "core/common/module_loader.h"
#include "core/common/time.h"
#include "core/common/api/bo_int.h"
#include "core/common/api/hw_context_int.h"
#include "core/include/xrt/xrt_bo.h"
#include "core/include/xrt/xrt_device.h"
#include "core/include/xrt/xrt_hw_context.h"
#include "core/include/xrt/xrt_kernel.h"
#include "core/include/xrt/xrt_uuid.h"
#include "core/include/xrt/detail/span.h"
#include "core/include/xrt/experimental/xrt_aie.h"
#include "core/include/xrt/experimental/xrt_elf.h"
#include "core/include/xrt/experimental/xrt_ext.h"
#include "core/include/xrt/experimental/xrt_kernel.h"
#include "core/include/xrt/experimental/xrt_module.h"
#include "core/include/xrt/experimental/xrt_queue.h"
#include "core/include/xrt/experimental/xrt_xclbin.h"

#include "core/common/json/nlohmann/json.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <istream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>

#ifdef _WIN32
# pragma warning (disable: 4100 4189 4505)
#endif

namespace {

// The recipe will use xrt::runlist when the number of runs
// exceed this threshold; otherwise use std::vector<xrt::run>
constexpr size_t default_runlist_threshold = 6;
  
using json = nlohmann::json;
const json empty_json;

// load_json() - Load a JSON from in-memory string or file
static json
load_json(const std::string& input)
{
  using json_error = xrt_core::runner::json_error;
  try {
    // Try parse as in-memory json
    return json::parse(input);
  }
  catch (const json::parse_error&)
  {
    // Not a valid JSON - treat input as a file path
  }

  try {
    if (std::ifstream f{input})
      return json::parse(f);
  }
  catch (const std::exception& ex) {
    throw json_error(ex.what());
  }

  throw std::runtime_error("Failed to load json, unknown error");
}

inline void
insert_json_object(json& dest, const json& src)
{
  dest.insert(src.begin(), src.end());
}

// Lifted from xrt_kernel.cpp
// Helper for converting an arbitrary sequence of bytes into
// a range that be iterated byte-by-byte (or by ValueType)
// Used during profile initialziation of a buffer.
template <typename ValueType>
class arg_range
{
  const ValueType* uval;
  size_t words;

  // Number of bytes must multiple of sizeof(ValueType)
  size_t
  validate_bytes(size_t bytes)
  {
    if (bytes % sizeof(ValueType))
      throw std::runtime_error("arg_range unaligned bytes");
    return bytes;
  }

public:
  arg_range(const void* value, size_t bytes)
    : uval(reinterpret_cast<const ValueType*>(value))
    , words(validate_bytes(bytes) / sizeof(ValueType))
  {}

  const ValueType*
  begin() const
  {
    return uval;
  }

  const ValueType*
  end() const
  {
    return uval + words;
  }

  size_t
  size() const
  {
    return words;
  }

  size_t
  bytes() const
  {
    return words * sizeof(ValueType);
  }

  const ValueType*
  data() const
  {
    return uval;
  }
};

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
  using repo_error = xrt_core::runner::repo_error;
  mutable std::map<std::string, std::vector<char>> m_data;
  std::string m_id;

  static std::string
  init_id()
  {
    static uint64_t count = 0;
    return std::to_string(count++);
  }

public:
  repo() : m_id(init_id()) {}
  virtual ~repo() = default;

  std::string
  get_id() const
  {
    return m_id;
  }

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
      throw repo_error{"File not found: " + full_path.string()};

    auto key = full_path.string();
    if (auto it = m_data.find(key); it != m_data.end())
      return to_sv((*it).second);

    std::ifstream ifs(key, std::ios::binary);
    if (!ifs)
      throw repo_error{"Failed to open file: " + key};

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

    throw repo_error{"Failed to find artifact: " + path};
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
  auto key = repo->get_id() + path; // must be unique to repo
  if (auto it = s_path2elf.find(key); it != s_path2elf.end())
    return get((*it).second);

  auto data = repo->get(path);
  streambuf buf{data.data(), data.data() + data.size()};
  std::istream is{&buf};
  xrt::elf elf{is};
  s_path2elf.emplace(key, elf);

  return get(elf);
}

} // module_cache

// class recipe - Runner recipe
class recipe
{
  using recipe_error = xrt_core::runner::recipe_error;
  
  // class header - header section of the recipe
  class header
  {
    xrt::xclbin m_xclbin;
    xrt::aie::program m_program;

    static xrt::xclbin
    read_xclbin(const json& j, const artifacts::repo* repo)
    {
      if (!j.contains("xclbin"))
        return {};
        
      auto path = j.at("xclbin").get<std::string>();
      auto data = repo->get(path);
      return xrt::xclbin{data};
    }

    static xrt::aie::program
    read_program(const json& j, const artifacts::repo* repo)
    {
      if (!j.contains("program"))
        return {};

      auto path = j.at("program").get<std::string>();
      auto data = repo->get(path);
      return xrt::aie::program{data};
    }

  public:
    header(const json& j, const artifacts::repo* repo)
      : m_xclbin{read_xclbin(j, repo)}
      , m_program{read_program(j, repo)}
    {
      XRT_DEBUGF("Loaded xclbin: %s\n", m_xclbin.get_uuid().to_string().c_str());
    }

    header(const header&) = default;

    xrt::xclbin
    get_xclbin() const
    {
      return m_xclbin;
    }

    xrt::aie::program
    get_program() const
    {
      return m_program;
    }

    json
    get_report() const
    {
      json rpt;
      rpt["xclbin"]["uuid"] = m_xclbin.get_uuid().to_string();
      return rpt;
    }
  }; // class recipe::header

  // class resources - resource section of the recipe
  class resources
  {
  public:
    class buffer
    {
      std::string m_name;

      enum class type { input, output, inout, internal, weight, spill, unknown, debug };
      type m_type;

      size_t m_size;

      // Buffer object is created for internal nodes, but not for
      // input/output which are bound during execution.  The member is
      // declared mutable to allow calling sync() through otherwise
      // const function (map()).  Technically calling sync has no side
      // effect other than making sure the buffer content is valid.
      mutable xrt::bo m_xrt_bo;

      static xrt::bo
      create_bo(const xrt::device& device, type, size_t sz)
      {
        return xrt::ext::bo{device, sz};
      }

      static xrt::bo
      create_bo(const xrt::hw_context& hwctx, type, size_t sz)
      {
        return xrt_core::bo_int::create_bo(hwctx, sz, xrt_core::bo_int::use_type::debug);
      }

      // Internal buffers must specify a size and are created during
      // as part of loading the recipe.  External buffers do not
      // require a specified size if they are are bound during
      // execution. Since size is the trigger for creating a xrt::bo
      // for the buffer, specifying size for externally bound buffers
      // wastes the buffer created here.
      buffer(const xrt::device& device, std::string name, type t, size_t sz)
        : m_name(std::move(name))
        , m_type(t)
        , m_size(sz)
        , m_xrt_bo{m_size ? create_bo(device, m_type, m_size) : xrt::bo{}}
      {
        XRT_DEBUGF("recipe::resources::buffer(%s), size(%d)\n", m_name.c_str(), m_size);
      }

      buffer(const xrt::hw_context& hwctx, std::string name, type t, size_t sz)
        : m_name(std::move(name))
        , m_type(t)
        , m_size(sz)
        , m_xrt_bo{create_bo(hwctx, m_type, m_size)}
      {
        XRT_DEBUGF("recipe::resources::buffer(%s), size(%d) type(debug)\n", m_name.c_str(), m_size);
      }

      // Copy constructor creates a new buffer with same properties as other
      // The xrt::bo is not copied, but a new one is created.
      buffer(const xrt::device& device, const buffer& other)
        : m_name(other.m_name)
        , m_type(other.m_type)
        , m_size(other.m_size)
        , m_xrt_bo{m_size ? create_bo(device, m_type, m_size) : xrt::bo{}}
      {}

      buffer(const xrt::hw_context& hwctx, const buffer& other)
        : m_name(other.m_name)
        , m_type(other.m_type)
        , m_size(other.m_size)
        , m_xrt_bo{create_bo(hwctx, m_type, m_size)}
      {}

      static type
      to_type(const std::string& t)
      {
        static const std::map<std::string, buffer::type> s2t = {
          { "input", type::input },
          { "output", type::output },
          { "inout", type::inout },
          { "internal", type::internal },
          { "weight", type::weight },
          { "spill", type::spill },
          { "unknown", type::unknown },
          { "debug", type::debug },
        };
        return s2t.at(t);
      }

    public:
      buffer(const buffer& rhs) = default;
      buffer(buffer&& rhs) = default;

      // create_buffer - create a buffer object from a property tree node
      static buffer
      create_buffer(const xrt::device& device, const xrt::hw_context& hwctx, const json& j)
      {
        auto tp = to_type(j.at("type").get<std::string>()); // required, input/output/internal
        auto sz = (tp == type::internal || tp == type::debug)
          ? j.at("size").get<size_t>()  // required for internal or debug buffers
          : j.value<size_t>("size", 0); // optional otherwise

        return (tp == type::debug)
          ? buffer{hwctx, j.at("name").get<std::string>(), tp, sz}
          : buffer{device, j.at("name").get<std::string>(), tp, sz};
      }

      // create_buffer - create a buffer object from another buffer object
      // This will create a new buffer object with the same properties as the
      // other buffer, but with a new xrt::bo object.
      static buffer
      create_buffer(const xrt::device& device, const xrt::hw_context& hwctx, const buffer& other)
      {
        return (other.m_type == type::debug)
          ? buffer{hwctx, other}
          : buffer{device, other};
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
        // Require that if size is specified for externally bound buffer,
        // then it must match the size of the binding buffer.
        if (m_size && m_size != bo.size())
          throw recipe_error("Invalid size (" + std::to_string(bo.size())
                             + ") of bo bound to '" + m_name + "', expected "
                             + std::to_string(m_size));

        XRT_DEBUGF("recipe::resources::buffer::bind(0x%p) buffer(%s)\n", bo.address(), m_name.c_str());
        m_xrt_bo = bo;
      }

      xrt::detail::span<const std::byte>
      map() const
      {
        m_xrt_bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        return xrt::detail::span{m_xrt_bo.map<const std::byte*>(), m_xrt_bo.size()};
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

      // Legacy kernel (alveo) or elf file was used when the hwctx was constructed.
      kernel(const xrt::hw_context& ctx, std::string name, std::string xname)
        : m_name(std::move(name))
        , m_instance(std::move(xname))
        , m_xclbin_kernel{ctx.get_xclbin().get_kernel(m_instance)}
        , m_xrt_kernel{xrt_core::hw_context_int::get_elf_flow(ctx) ? xrt::ext::kernel{ctx, m_instance} : xrt::kernel{ctx, m_instance}}
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
        auto instance = j.value("instance", name);
        if (elf.empty())
          return kernel{hwctx, name, instance};

        auto mod = module_cache::get(elf, repo);
        return kernel{hwctx, mod, name, instance};
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
    create_buffers(const xrt::device& device, const xrt::hw_context& hwctx, const json& j)
    {
      std::map<std::string, buffer> buffers;
      for (const auto& [name, node] : j.items())
        buffers.emplace(node.at("name").get<std::string>(),
                        buffer::create_buffer(device, hwctx, node));

      return buffers;
    }

    // create_buffers - create buffer objects from buffer objects
    // This will create new buffer objects with the same properties as the
    // other buffers, but with new xrt::bo objects.
    static std::map<std::string, buffer>
    create_buffers(const xrt::device& device, const xrt::hw_context& hwctx,
                   const std::map<std::string, buffer>& others)
    {
      std::map<std::string, buffer> buffers;
      for (const auto& [name, other] : others)
        buffers.emplace(name, buffer::create_buffer(device, hwctx, other));

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

    static xrt::hw_context
    create_hwctx(xrt::device device, const xrt::xclbin& xclbin,
                 const xrt::hw_context::qos_type& qos)
    {
      try {
        return {device, device.register_xclbin(xclbin), qos};
      }
      catch (const std::exception& ex) {
        throw xrt_core::runner::hwctx_error{ex.what()};
      }
    }

    static xrt::hw_context
    create_hwctx(const xrt::device& device, const xrt::aie::program& program,
                 const xrt::hw_context::qos_type& qos)
    {
      try {
        return {device, program, qos, xrt::hw_context::access_mode::shared};
      }
      catch (const std::exception& ex) {
        throw xrt_core::runner::hwctx_error{ex.what()};
      }
    }

    static xrt::hw_context
    create_hwctx(const xrt::device& device, const header& header, 
                 const xrt::hw_context::qos_type& qos)
    {
      if (auto xclbin = header.get_xclbin())
        return create_hwctx(device, xclbin, qos);

      if (auto program = header.get_program())
        return create_hwctx(device, program, qos);

      throw recipe_error("No program or xclbin specified");
    }

  public:
    resources(xrt::device device, const header& header,
              const xrt::hw_context::qos_type& qos,
              const json& recipe, const artifacts::repo* repo)
      : m_device{std::move(device)}
      , m_hwctx{create_hwctx(m_device, header, qos)}
      , m_buffers{create_buffers(m_device, m_hwctx, recipe.at("buffers"))}
      , m_kernels{create_kernels(m_device, m_hwctx, recipe.at("kernels"), repo)}
      , m_cpus{create_cpus(recipe.value("cpus", empty_json))} // optional
    {}

    resources(const resources& other)
      : m_device{other.m_device}                             // share device
      , m_hwctx{other.m_hwctx}                               // share hwctx
      , m_buffers{create_buffers(m_device, m_hwctx, other.m_buffers)} // new buffers
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
        throw recipe_error("Unknown kernel '" + name + "'");
      return it->second.get_xrt_kernel();
    }

    xrt_core::cpu::function
    get_cpu_function_or_error(const std::string& name) const
    {
      auto it = m_cpus.find(name);
      if (it == m_cpus.end())
        throw recipe_error("Unknown cpu '" + name + "'");
      return it->second.get_function();
    }

    resources::buffer
    get_buffer_or_error(const std::string& name) const
    {
      auto it = m_buffers.find(name);
      if (it == m_buffers.end())
        throw recipe_error("Unknown buffer '" + name + "'");

      return it->second;
    }

    xrt::detail::span<const std::byte>
    map_buffer(const std::string& name) const
    {
      if (auto it = m_buffers.find(name); it != m_buffers.end())
        return (*it).second.map();

      return {nullptr, 0};
    }

    json
    get_report() const
    {
      json rpt;
      rpt["resources"]["buffers"] = m_buffers.size();
      rpt["resources"]["total_buffer_size"] =
        std::accumulate(m_buffers.begin(), m_buffers.end(), size_t(0), [] (size_t value, const auto& b) {
          if (auto bo = b.second.get_xrt_bo())
            return value + bo.size();

          return value;
        });
      rpt["resources"]["kernels"] =  m_kernels.size();
      rpt["resources"]["hwctx_coloumns"] = xrt_core::hw_context_int::get_partition_size(m_hwctx);
      return rpt;
    }
  }; // class recipe::resources

public:
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

        // create_xrt_bo() - return xrt::bo object or create sub-buffer
        // An argument is associated with a resources::buffer. If the
        // resources::buffer was created with an xrt::bo object (size
        // was specified in the recipe), then this function can be
        // used to create a sub-buffer from that bo object.  Otherwise
        // this function simply returns the bo managed by the
        // resources::buffer, which may be a null bo if the buffer is
        // ubound.
        static xrt::bo
        create_xrt_bo(const resources::buffer& buffer, size_t offset, size_t size)
        {
          auto bo = buffer.get_xrt_bo();
          if (bo && (bo.size() < size))
            throw recipe_error("buffer size mismatch for buffer: " + buffer.get_name());

          if (bo && size && (size < bo.size()))
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
                     (resources.get_device(),
                      resources.get_xrt_hwctx(),
                      other.m_buffer)}                          // (see earlier comment)
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
      using constant_type = std::variant<int, std::string>;
      std::string m_name;
      run_type m_run;
      std::map<std::string, argument> m_args;
      std::map<int, constant_type> m_constants;

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

      // Set constant args on a run.
      static void
      set_constant_args(run_type run, const std::map<int, constant_type>& constants)
      {
        for (auto [argidx, value] : constants) {
          if (std::holds_alternative<int>(value))
            std::visit(set_arg_visitor{argidx, std::get<int>(std::move(value))}, run);
          else if (std::holds_alternative<std::string>(value))
            std::visit(set_arg_visitor{argidx, std::get<std::string>(std::move(value))}, run);
        }
      }

      // Read recipe::runs::constants from json.
      // Return a map of argidx to constant value.
      static std::map<int, constant_type>
      create_constant_args(const json& j)
      {
        std::map<int, constant_type> constants;

        for (const auto& [name, node] : j.items()) {
          auto argidx = node.at("argidx").get<int>();
          auto type = node.at("type").get<std::string>();
          if (type == "int")
            constants.emplace(argidx, node.at("value").get<int>());
          else if (type == "string")
            constants.emplace(argidx, node.at("value").get<std::string>());
          else
            throw recipe_error("Unknown constant argument type '" + type + "'");
        }

        return constants;
      }

      // Create constant args from json and set them on the run
      static std::map<int, constant_type>
      create_and_set_constant_args(run_type run, const json& j)
      {
        auto constants = create_constant_args(j);
        set_constant_args(run, constants);
        return constants;
      }

      // Set existing constant args on a run.  This function is used
      // by the run constructor that creates a run from an existing run
      // Return argument constant map, which is stored as a data member
      // of the created run.
      static std::map<int, constant_type>
      create_and_set_constant_args(run_type run, const std::map<int, constant_type>& other)
      {
        set_constant_args(run, other);
        return other;
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
        , m_constants{create_and_set_constant_args(m_run, j.value("constants", json::object()))}
      {
        XRT_DEBUGF("recipe::execution::run(%s)\n", m_name.c_str());
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
        , m_constants{create_and_set_constant_args(m_run, other.m_constants)}
      {
        XRT_DEBUGF("recipe::execution::run(other) name(%s)\n", m_name.c_str());
      }

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

        throw recipe_error("recipe::execution::run::get_xrt_run() called on a CPU run");
      }

      xrt_core::cpu::run
      get_cpu_run() const
      {
        if (std::holds_alternative<xrt_core::cpu::run>(m_run))
          return std::get<xrt_core::cpu::run>(m_run);

        throw recipe_error("recipe::execution::run::get_cpu_run() called on a NPU run");
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
      virtual void add(const run& run) = 0;
      virtual void execute(size_t) = 0;
      virtual void wait() {}
    };

    struct cpu_runlist : runlist
    {
      std::vector<xrt_core::cpu::run> m_runs;

      void
      add(const run& run) override
      {
        m_runs.push_back(run.get_cpu_run());
      }
      
      void
      execute(size_t) override
      {
        // CPU runs are synchronous, nothing to wait on
        for (auto& run : m_runs)
          run.execute();
      }
    };

    // The NPU runlist starts out as a std::vector<xrt::run> but
    // morphs to an xrt::runlist if number of runs exceeds
    // runlist_threshold
    struct npu_runlist : runlist
    {
      struct impl : runlist
      {
        virtual const std::vector<xrt::run>&
        get_rl() const
        {
          throw std::runtime_error("Internal error");
        }
      };

      // Vector implementation of the NPU runlist
      struct vrl : impl
      {
        std::vector<xrt::run> m_rl;

        vrl()
        {
          XRT_DEBUGF("recipe::execution creating std::vector<xrt::run>\n");
        }

        const std::vector<xrt::run>&
        get_rl() const override
        {
          return m_rl;
        }

        void
        add(const run& run) override
        {
          m_rl.push_back(run.get_xrt_run());
        }

        void
        execute(size_t iteration) override
        {
          // First iteration, just start all runs
          if (iteration == 0) {
            for (auto& run : m_rl)
              run.start();

            return;
          }

          // Wait until previous iteration run is done
          for (auto& run : m_rl) {
            run.wait2();
            run.start();
          }
        }

        void
        wait() override
        {
          // While waiting for last to complete is enough, all runs
          // must be marked completed
          for (auto itr = m_rl.rbegin(); itr != m_rl.rend(); ++itr)
            (*itr).wait2();
        }
      }; // vrl

      // xrt::runlist implementation of NPU runlist
      struct xrl : impl
      {
        xrt::runlist m_rl;

        explicit xrl(const xrt::hw_context& hwctx)
          : m_rl{hwctx}
        {
          XRT_DEBUGF("recipe::execution creating xrt::runlist\n");
        }

        void
        add(const std::vector<xrt::run>& runs)
        {
          for (auto run : runs)
            m_rl.add(std::move(run));
        }

        void
        add(const run& run) override
        {
          m_rl.add(run.get_xrt_run());
        }

        void
        execute(size_t iteration) override
        {
          // Wait until previous iteration is done
          if (iteration > 0)
            m_rl.wait();
          
          m_rl.execute();
        }

        void
        wait() override
        {
          m_rl.wait();
        }
      }; // xrl

      std::unique_ptr<impl> m_impl;
      xrt::hw_context m_hwctx;
      size_t m_runlist_threshold;
      uint32_t m_count = 0;

      npu_runlist(xrt::hw_context hwctx, size_t rlt)
        : m_impl(std::make_unique<vrl>())
        , m_hwctx(std::move(hwctx))
        , m_runlist_threshold{rlt}
      {}

      void
      add(const run& run) override
      {
        // morph to xrt::runlist when threshold reached
        XRT_DEBUGF("(count, threshold)=(%d, %d)\n", m_count, m_runlist_threshold);
        if (++m_count == m_runlist_threshold) {
          XRT_DEBUGF("recipe::execution switching to xrt::runlist\n");
          auto xrlist = std::make_unique<xrl>(m_hwctx);
          xrlist->add(m_impl->get_rl());
          m_impl = std::move(xrlist);
        }

        m_impl->add(run);
      }

      void
      execute(size_t iteration) override
      {
        m_impl->execute(iteration);
      }

      void wait() override
      {
        m_impl->wait();
      }
    }; // npu_runlist

    std::vector<run> m_runs;
    std::exception_ptr m_eptr;
    size_t m_runlist_threshold = default_runlist_threshold;
    std::vector<std::unique_ptr<runlist>> m_runlists;
    std::unique_ptr<xrt::queue> m_queue;      // Queue that executes the runlists in sequence
    std::vector<xrt::queue::event> m_events;  // Events that signal complettion of a runlist

    static std::vector<std::unique_ptr<runlist>>
    create_runlists(const resources& resources, const std::vector<run>& runs, size_t rlt)
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
            auto rl = std::make_unique<npu_runlist>(resources.get_xrt_hwctx(), rlt);
            nrl = rl.get();
            runlists.push_back(std::move(rl));
          }

          nrl->add(run);
        }
        else if (run.is_cpu_run()) {
          if (nrl) 
            nrl = nullptr;

          if (!crl) {
            auto rl = std::make_unique<cpu_runlist>();
            crl = rl.get();
            runlists.push_back(std::move(rl));
          }

          crl->add(run);
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
    execution(const resources& resources, const json& j, size_t runlist_threshold)
      : m_runs{create_runs(resources, j.at("runs"))}
      , m_runlist_threshold{runlist_threshold}
      , m_runlists{create_runlists(resources, m_runs, m_runlist_threshold)}
      , m_queue{m_runlists.size() > 1 ? std::make_unique<xrt::queue>() : nullptr}
    {}

    // execution() - create an execution object from existing runs
    // New run objects are created from the existing runs.
    execution(const resources& resources, const execution& other)
      : m_runs{create_runs(resources, other.m_runs)}
      , m_runlists{create_runlists(resources, m_runs, other.m_runlist_threshold)}
      , m_queue{m_runlists.size() > 1 ? std::make_unique<xrt::queue>() : nullptr}
    {}

    size_t
    num_runs() const
    {
      return m_runs.size();
    }

    void
    bind(const std::string& name, const xrt::bo& bo)
    {
      // Iterate over all runs and bind the buffer.
      // Note, that not all runs need to use the buffer.
      // Maybe some optimization could be done here.
      for (auto& run : m_runs)
        run.bind(name, bo);
    }

    
    // execute_runlist() - execute a runlist synchronously
    // The lambda function is executed asynchronously by an
    // xrt::queue object. The wait is necessary for an NPU runlist,
    // which must complete before next enqueue operation can be
    // executed.  Execution of an NPU runlist is itself asynchronous.
    static void
    execute_runlist(size_t iteration, runlist* runlist, std::exception_ptr& eptr)
    {
      try {
        runlist->execute(iteration);
        runlist->wait(); // needed for NPU runlists, noop for CPU
      }
      catch (const xrt::runlist::command_error&) {
        eptr = std::current_exception();
      }
      catch (const std::exception&) {
        eptr = std::current_exception();
      }
    }

    // Execute a run-recipe iteration
    void
    execute(size_t iteration)
    {
      // If single runlist then avoid the overhead of xrt::queue
      if (m_runlists.size() == 1) {
        m_runlists[0]->execute(iteration);
        return;
      }

      // The recipe has multiple runlists (a mix of NPU and CPU).
      // Restart the recipes, but ensure that a runlist has completed
      // its previous iteration before restarting it.
      int count = 0;
      for (auto& runlist : m_runlists) {
        if (iteration > 0)
          m_events[count].wait();

        m_events[count++] = m_queue->enqueue([this, iteration, &runlist] {
          execute_runlist(iteration, runlist.get(), m_eptr);
        });
      }
    }

    void
    wait()
    {
      // If single runlist then it was submitted explicitly, so
      // wait explicitly
      if (m_runlists.size() == 1) {
        m_runlists[0]->wait();
        return;
      }

      // Sufficient to wait for last runlist to finish since last list
      // must have waited for all previous lists to finish.
      const auto& event = m_events.back();
      if (event)
        event.wait();

      if (m_eptr)
        std::rethrow_exception(m_eptr);
    }

    json
    get_report() const
    {
      json rpt;
      rpt["resources"]["runlist_threshold"] = m_runlist_threshold;
      rpt["resources"]["runlist"] = m_runlist_threshold;
      return rpt;
    }
  }; // class recipe::execution

  xrt::device m_device;

  json m_recipe_json;
  header m_header;
  resources m_resources;
  execution m_execution;

public:
  recipe(xrt::device device, json recipe,
         const xrt::hw_context::qos_type& qos, size_t runlist_threshold,
         const artifacts::repo* repo)
    : m_device{std::move(device)}
    , m_recipe_json(std::move(recipe)) // paren required, else initialized as array
    , m_header{m_recipe_json.at("header"), repo}
    , m_resources{m_device, m_header, qos, m_recipe_json.at("resources"), repo}
    , m_execution{m_resources, m_recipe_json.at("execution"), runlist_threshold }
  {}

  recipe(xrt::device device, json recipe, const artifacts::repo* repo)
    : recipe::recipe(std::move(device), std::move(recipe), {}, default_runlist_threshold, repo)
  {}

  recipe(xrt::device device, const std::string& rr, const artifacts::repo* repo)
    : recipe::recipe(std::move(device), load_json(rr), {}, default_runlist_threshold, repo)
  {}

  recipe(const recipe&) = default;

  execution*
  get_execution()
  {
    return &m_execution;
  }

  execution
  clone_execution() const
  {
    return {m_resources, m_execution};
  }

  size_t
  num_runs() const
  {
    return m_execution.num_runs();
  }

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

  void
  bind(const std::string& name, const xrt::bo& bo)
  {
    XRT_DEBUGF("recipe::bind(%s) bo::size(%d)\n", name.c_str(), bo.size());
    m_execution.bind(name, bo);
  }

  void
  execute(size_t iteration)
  {
    XRT_DEBUGF("recipe::execute(%d)\n", iteration);
    m_execution.execute(iteration);
  }

  void
  execute()
  {
    execute(0);
  }

  void
  wait()
  {
    XRT_DEBUGF("recipe::wait()\n");
    m_execution.wait();
  }

  json
  get_report() const
  {
    json rpt = json::object();
    insert_json_object(rpt, m_header.get_report());
    insert_json_object(rpt, m_resources.get_report());
    insert_json_object(rpt, m_execution.get_report());
    rpt["resources"]["runs"] = num_runs();
    return rpt;
  }

  xrt::detail::span<const std::byte>
  map_buffer(const std::string& nm) const
  {
    return m_resources.map_buffer(nm);
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
  using profile_error = xrt_core::runner::profile_error;
  using validation_error = xrt_core::runner::validation_error;

  // class bindings - represents the bindings sections of a profile json
  //
  // {
  //   "name": buffer name in recipe
  //   "size": (required w/o fle initialization) the size of the buffer
  //   "init": (optional) how to initialize a buffer
  //   "validate": (optional) how to validate a buffer after execution
  // }
  // 
  // The bindings section specify what xrt::bo objects to create for
  // external buffers. The buffers are bound to the recipe prior to
  // first execution.
  //
  // If "size" is specified it will be the size of the buffer.
  // "size" is required unless the buffer is initialzed from a file,
  // in which case the size (if not explicit) is inferred from the
  // size of the file.
  //
  // If "init" is specified, then it defines how the buffer should be
  // initialzed. There are several different ways in which a buffer
  // can be initialized.
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

    xrt::device m_device;

    // Cache the repo for file access during init
    const artifacts::repo* m_repo;

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
        xrt::bo bo = size ? xrt::ext::bo{device, size} : xrt::bo{};
        bos.emplace(node.at("name").get<std::string>(), std::move(bo));
      }
      return bos;
    }

    // Validate a resource buffer per profile.json validate json node
    // "validate": {
    //   "file": "gold.bin", // path to file
    //   "skip": 0,          // skip fist bytes of file (optional)
    //   "begin": 0,         // bo offset to start validating at (optional)
    //   "end": bo.size()    // bo offset to start validating at (optional)
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
        auto skip = node.value<size_t>("skip", 0);
        if (skip > golden_data.size())
          throw std::runtime_error("skip bytes large than file");

        // Adjust the view, skipping skip bytes
        golden_data = std::string_view{golden_data.data() + skip, golden_data.size() - skip};
      }

      bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);

      if (bo.size() != golden_data.size())
        throw validation_error("Size mismatch during validation");

      // Optional range of bo to validate
      auto bo_begin = node.value<size_t>("begin", 0);
      auto bo_end = node.value<size_t>("end", bo.size());
      if (bo_begin > bo_end || bo_end > bo.size())
        throw profile_error("bad validate begin/end values: " + std::to_string(bo_begin) + "/" + std::to_string(bo_end));

      auto bo_data = bo.map<char*>();
      auto bo_range_bytes = bo_end - bo_begin;

      XRT_DEBUGF("profile::bindings::validate_buffer() validating bo range [%d,%d[\n", bo_begin, bo_end);
      if (std::equal(bo_data + bo_begin, bo_data + bo_end, golden_data.data()))
        return;

      // Error
      for (uint64_t i = 0; i < bo_range_bytes; ++i) {
        if (golden_data[i] != bo_data[bo_begin + i])
          throw validation_error
            ("gold[" + std::to_string(i) + "] = " + std::to_string(golden_data[i])
             + " does not match bo value in bo " + std::to_string(bo_data[i]));
      }
    }

    // init_buffer_file() - Initialize bo from a content of a file
    //
    // "init": {
    //   "file": "path",  // path to file
    //   "skip": bytes,   // skip fist bytes of file (optional)
    //   "begin": 0,      // offset to start writing at (optional)
    //   "end": bo.size() // offset to end writing at (optional)
    // }
    //
    // This function fills all the bytes of the buffer with data
    // from file.  It wraps around the file if necessary to fill
    // the bo.
    //
    // The function supports initializing the buffer between iterations,
    // copying from the file from an offset (beg) corresponding to where
    // previous iteration reached.
    void
    init_buffer_file(xrt::bo& bo, const init_node& node, size_t iteration)
    {
      auto file = node.at("file").get<std::string>();
      auto skip = node.value<size_t>("skip", 0);
      auto bo_begin = node.value<size_t>("begin", 0);
      auto bo_end = node.value<size_t>("end", bo.size());
      auto data = m_repo->get(file);
      if (skip > data.size())
        throw profile_error("bad skip value: " + std::to_string(skip));

      if (bo_begin > bo_end || bo_end > bo.size())
        throw profile_error("bad init begin/end values: " + std::to_string(bo_begin) + "/" + std::to_string(bo_end));

      // Adjust the view, skipping skip bytes, then copy to bo
      data = std::string_view{data.data() + skip, data.size() - skip};

      // Create the bo from the size of the file unless it was already
      // created from explicit size.
      if (!bo)
        bo = xrt::ext::bo{m_device, data.size()};

      auto bo_data = bo.map<char*>();

      // Pad the bo with 0s outside the [bo_begin, bo_end[ range
      std::fill(bo_data, bo_data + bo_begin, char(0));
      std::fill(bo_data + bo_end, bo_data + bo.size(), char(0));

      // Copy bytes from file to bo starting at optional begin offset.
      // Wrap around file if needed to fill the bo with data from
      // file.  Copy at offset into bo based on number of bytes left
      // to copy.
      bo_data += bo_begin; // past optional begin
      auto bo_range_bytes = bo_end - bo_begin;  // default bo.size()

      // Must fill all bytes of bo in [begin, end[ range
      auto bytes = bo_range_bytes;

      // This loop wraps around the source data if necessary in order
      // to fill all bytes of the bo range.  The loop adjusts for iteration.
      while (bytes) {
        auto bo_offset = bo_range_bytes - bytes; // offset within bo_data
        auto beg = ((iteration * bo_range_bytes) + (bo_offset)) % data.size();
        auto end = std::min<size_t>(beg + bytes, data.size());
        bytes -= end - beg;

        XRT_DEBUGF("profile::bindings::init_buffer_file() (itr,beg,end,offset)=(%d,%d,%d,%d)\n",
                   iteration, beg, end, bo_offset);
    
        std::copy(data.data() + beg, data.data() + end, bo_data + bo_offset);
      }
    }

    // init_buffer_stride() - Initialize bo with value at stride
    // "init": {
    //   "stride": 1,   // write the value repeatedly at this stride
    //   "value": 239,  // the value to write
    //   "begin": 0,    // offset to start writing at (optional)
    //   "end": 524288, // offset to end writing at (optional)
    //   "debug": true  // undefined (optional)
    // }
    void
    init_buffer_stride(xrt::bo& bo, const init_node& node)
    {
      auto bo_data = bo.map<uint8_t*>();
      auto stride = node.at("stride").get<size_t>();
      auto value = node.at("value").get<uint64_t>();
      auto bo_begin = node.value("begin", 0);
      auto bo_end = node.value("end", bo.size());
      arg_range<uint8_t> vr{&value, sizeof(value)};
      for (size_t offset = bo_begin; offset < bo_end; offset += stride) 
        std::copy_n(vr.begin(), std::min<size_t>(bo.size() - offset, vr.size()), bo_data + offset);
    }

    void
    init_buffer_random(xrt::bo& bo, const init_node&)
    {
      auto bo_data = bo.map<uint8_t*>();
      static std::random_device rd;
      std::generate(bo_data, bo_data + bo.size(), [&]() { return static_cast<uint8_t>(rd()); });
    }

    // init_buffer() - Initialize a resource buffer per the binding json node
    // "init": {
    //   // "stride" stride initialization
    //   // "random" random initialization
    // }
    // The buffer is synced to device after iniitialization
    void
    init_buffer(xrt::bo& bo, const init_node& node, size_t iteration)
    {
      // stride initialization with specified value
      if (node.contains("file"))
        init_buffer_file(bo, node, iteration);
      else if (node.contains("stride"))
        init_buffer_stride(bo, node);
      else if (node.value<bool>("random", false))
        init_buffer_random(bo, node);
      else
        throw profile_error("Unsupported initialization node in profile");

      if (node.value<bool>("debug", false)) {
        static uint64_t count = 0;
        std::ofstream ostrm("profile.debug.init[" + std::to_string(count++) + "].bin", std::ios::binary);
        ostrm.write(bo.map<char*>(), static_cast<std::streamsize>(bo.size()));
      }

      bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
    }

    void
    init_buffer(xrt::bo& bo, const init_node& node)
    {
      init_buffer(bo, node, 0);
    }

  public:
    bindings() = default;

    bindings(xrt::device device, const json& j, const artifacts::repo* repo)
      : m_device{std::move(device)}
      , m_repo{repo}
      , m_bindings{init_bindings(j)}
      , m_xrt_bos{create_buffers(m_device, m_bindings, repo)}
    {
      // All bindings are initialized by default upon creation if they
      // have an "init" element.
      init();
    }

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
        if (node.contains("init")) {
          XRT_DEBUGF("profile::bindings::init(%s)\n", name.c_str());
          init_buffer(m_xrt_bos.at(name), node.at("init"));
        }
      }
    }

    // Binding buffers can be re-initialized before iterating
    // execution of the recipe.  Re-initialization is guarded
    // by execution::iteration::init and bindings::reinit.
    void
    reinit(size_t iteration)
    {
      for (auto& [name, node] : m_bindings) {
        if (node.value<bool>("reinit", false) && node.contains("init")) {
          XRT_DEBUGF("profile::bindings::reinit(%s)\n", name.c_str());
          init_buffer(m_xrt_bos.at(name), node.at("init"), iteration);
        }
      }
    }

    // Unconditioally bind all resources buffers to the recipe
    // execution.  This function is used for cloned recipe executions.
    void
    bind(recipe::execution& re)
    {
      for (auto& [name, node] : m_bindings)
        re.bind(name, m_xrt_bos.at(name));
    }

    // Binding buffers can be re-bound before iterating
    // execution of the recipe.  Re-binding is guarded by 
    // execution::iteration::bind and bindings::rebind
    void
    rebind(recipe::execution& re)
    {
      for (auto& [name, node] : m_bindings)
        if (node.value<bool>("rebind", false))
          re.bind(name, m_xrt_bos.at(name));
    }
  }; // class profile::bindings

  // class execution - represents the execution section of a profile json
  //
  // {
  //  "execution" : {
  //    "iterations": 2,    (1)
  //    "verbose": bool,    (true)
  //    "validate": bool,   (false)
  //    "mode" : mode       (none)
  //    "depth": depth      (1)
  //    "iteration" : {
  //      "bind": false,    (false)
  //      "init": true,     (false)
  //      "wait": true,     (false)
  //      "validate": true  (false)
  //    }
  //  }
  //
  // The execution section specifies how a recipe should be executed.
  // - "iterations" specfies how many times the recipe should be
  //    executed when the application calls xrt::runnner::execute().
  // - "verbose" can be used to turn off printing of metrics
  // - "validate" enables validation per binding nodes after all
  //   iterations have completed
  // - "mode" specifies the mode of execution.
  // - "depth" specifies the depth of the recipe runlist, e.g. how
  //    many times the runlist should be duplicated. A value of 1
  //    indicates no duplication.
  //
  // The behavior of an iteration is within the iteration sub-node.
  // - "bind" indicates if buffers should be re-bound to the
  //   recipe before an iteration.
  // - "init" indicates if buffer should be re-initialized per what is
  //    specified in the binding element.
  // - "wait" says that execution should wait for completion between
  //    iterations and and sleep for specified milliseconds before
  //    next iteration.
  // - "validate" means buffer validation per what is specified in
  //   the binding element.
  class execution
  {
    using iteration_node = json;

    // class executor - Manages execution of the profile
    //
    // Depending on the execution mode, it may be necessary to clone
    // the recipe execution section depth number of times.  This class
    // manages execution of the recipe whether its runs (runlist) is
    // cloned or not.
    class executor
    {
      profile* m_profile;

      recipe::execution* m_base;
      std::vector<recipe::execution> m_copies;

      static std::vector<recipe::execution>
      create_execution_copies(recipe* recipe, size_t depth)
      {
        std::vector<recipe::execution> copies;
        for (size_t i = 1; i < depth; ++i)
          copies.emplace_back(recipe->clone_execution());

        return copies;
      }
      
    public:
      executor(profile* profile, recipe* recipe, size_t depth)
        : m_profile{profile}
        , m_base{recipe->get_execution()}
        , m_copies{create_execution_copies(recipe, depth)}
      {
        // Bind buffers to the recipe execution objects prior to
        // executing the recipe. This will bind the buffers which have
        // binding::bind set to true.
        bind();
      }

      void
      execute_iteration(size_t iteration)
      {
        // First iteration, start all
        if (iteration == 0) {
          m_base->execute(iteration);

          for (auto& exec : m_copies)
            exec.execute(iteration);

          return;
        }
        
        // Wait until previous iteration run is done then restart
        // This operates under the assumption that execution is
        // sequential and in-order of submission.
        m_base->wait();
        m_base->execute(iteration);

        for (auto& exec : m_copies) {
          exec.wait();
          exec.execute(iteration);
        }
      }

      // Bind buffers to recipe and to recipe::execution copies.
      void
      bind()
      {
        m_profile->bind(*m_base);
        for (auto& exec : m_copies)
          m_profile->bind(exec);
      }

      // Re-bind buffers to recipe and to recipe::execution copies.
      void
      rebind()
      {
        m_profile->rebind(*m_base);
        for (auto& exec : m_copies)
          m_profile->rebind(exec);
      }

      // Wait for recipe execution completion and copies
      void
      wait()
      {
        m_base->wait();
        for (auto& exec : m_copies)
          exec.wait();
      }
    }; // class profile::execution::executor

    // Mode of execution
    enum class mode { none, latency, throughput };
    
    profile* m_profile;
    std::string m_name;
    mode m_mode = mode::none;
    size_t m_depth = 1;
    size_t m_recipe_runs = 1;  // legacy mode throughput calculation
    executor m_executor;
    size_t m_iterations = 1;
    iteration_node m_iteration;
    bool m_verbose = false;
    bool m_validate = false;
    bool m_legacy = false;
    mutable json m_report;

    static mode
    to_mode(const std::string& mstr)
    {
      static const std::map<std::string, mode> mode_map{
        {"default", mode::none},
        {"latency", mode::latency},
        {"throughput", mode::throughput}
      };

      if (auto itr = mode_map.find(mstr); itr != mode_map.end())
        return itr->second;

      throw profile_error("bad execution mode: " + mstr);
    };

    static std::string
    to_string(mode m)
    {
      static const std::map<mode, std::string> mode_map{
        {mode::none, "default"},
        {mode::latency, "latency"},
        {mode::throughput, "throughput"}
      };

      if (auto itr = mode_map.find(m); itr != mode_map.end())
        return itr->second;

      throw std::runtime_error("bad execution mode: " + std::to_string(static_cast<int>(m)));
    }

    static iteration_node
    get_iteration_node(mode m, const json& j)
    {
      if (m == mode::none)
        return j.value("iteration", json::object());

      // latency and throughput modes do not support iteration node
      return json::object();
    }

    static size_t
    get_depth(mode m, const json& j)
    {
      // For throughput, the default depth is 2
      if (m == mode::throughput)
        return j.value("depth", 2);

      // Only throughput mode supports depth of recipe
      return 1;
    }

    void
    execute_iteration(size_t iteration)
    {
      // Bind buffers to recipe if requested.  All buffers are bound
      // when created, so this is for subsequent iterations
      // only. Binding must to through executor which could have clone
      // the recipe.
      if (iteration > 0 && m_iteration.value("bind", false))
        m_executor.rebind();
      
      // Initialize buffers if requested.  All buffers are initialized
      // when created, so this is for subsequent iterations only
      if (iteration > 0 && m_iteration.value("init", false))
        m_profile->reinit(iteration);

      m_executor.execute_iteration(iteration);

      // Wait execution to complete if requested
      if (m_iteration.value("wait", false))
        m_executor.wait();

      if (auto sleep_ms = m_iteration.value("sleep", 0))
        m_profile->sleep(sleep_ms);

      // Validate if requested (implies wait)
      if (m_iteration.value("validate", false))
        m_profile->validate();
    }

  public:
    execution(profile* pr, recipe* rr, const json& j, bool legacy = false)
      : m_profile(pr)
      , m_name(j.value("name", j.value("mode", "default")))
      , m_mode(to_mode(j.value("mode", "default")))
      , m_depth(get_depth(m_mode, j))
      , m_recipe_runs(rr->num_runs())
      , m_executor{m_profile, rr, m_depth}
      , m_iterations(j.value("iterations", 1))
      , m_iteration(get_iteration_node(m_mode, j))
      , m_verbose(j.value("verbose", true))
      , m_validate(j.value("validate", false))
      , m_legacy(legacy)
    {}

    // Execute the profile
    void
    execute()
    {
      XRT_DEBUGF("execution::execute(%s) depth(%d) mode(%s)\n",  m_name.c_str(), m_depth, to_string(m_mode).c_str());
      unsigned long long time_ns = 0;
      {
        xrt_core::time_guard tg(time_ns);
        for (size_t i = 0; i < m_iterations; ++i)
          execute_iteration(i);
        
        m_executor.wait();
      }

      if (m_validate)
        m_profile->validate();

      // NOLINTBEGIN
      // In legacy mode, the number of recipe::runs is used for
      // throughput and latency calculatons.  In non-legacy mode, the
      // recipe runs is always considered as one runlist and
      // calculations are based on the how many times (depth) the
      // runlist is duplicated.
      auto depth = m_legacy ? m_recipe_runs : m_depth;
      auto elapsed = time_ns / 1000;
      auto latency = time_ns / (1000 * m_iterations * depth);
      auto throughput = (1000000000 * m_iterations * depth) / time_ns;
      // NOLINTEND

      m_report["cpu"]["elapsed"] = elapsed;
      if (m_legacy || m_mode == mode::latency)
        m_report["cpu"]["latency"] = latency;

      if (m_legacy || m_mode == mode::throughput)
        m_report["cpu"]["throughput"] = throughput;

      if (m_verbose) {
        std::cout << "Execution profile: " << m_name << "\n";
        std::cout << "Elapsed time (us): " << elapsed << "\n";
        if (m_legacy || m_mode == mode::latency)
          std::cout << "Average Latency (us): " << latency << "\n";

        if (m_legacy || m_mode == mode::throughput)
          std::cout << "Average Throughput (op/s): " << throughput << "\n";
      }
    }

    json
    get_report() const
    {
      m_report["name"] = m_name;
      m_report["iterations"] = m_iterations;
      if (!m_legacy) {
        m_report["depth"] = m_depth;
        m_report["mode"] = to_string(m_mode);
      }

      return m_report;
    }
  }; // class profile::execution
  
private:
  friend class bindings;  // embedded class
  friend class execution; // embedded class
  json m_profile_json;
  std::shared_ptr<artifacts::repo> m_repo;

  xrt::hw_context::qos_type m_qos;
  size_t m_runlist_threshold;
  recipe m_recipe;
  bindings m_bindings;
  execution m_execution;
  std::vector<std::unique_ptr<execution>> m_executions;

  void
  bind(recipe::execution& exec)
  {
    m_bindings.bind(exec);
  }

  void
  rebind(recipe::execution& exec)
  {
    m_bindings.bind(exec);
  }

  void
  init()
  {
    m_bindings.init();
  }

  void
  reinit(size_t iteration)
  {
    m_bindings.reinit(iteration);
  }

  void
  validate()
  {
    m_bindings.validate(m_repo.get());
  }

  void
  sleep(uint32_t sleep_ms) const
  {
    XRT_DEBUGF("profile::sleep(%d)\n", sleep_ms);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }

private:
  static xrt::hw_context::qos_type
  init_qos(const json& j)
  {
    if (j.empty())
      return {};

    xrt::hw_context::qos_type qos;
    for (auto [key, value] : j.items()) {
      XRT_DEBUGF("qos[%s] = %d\n", key.c_str(), value.get<uint32_t>());
      qos.emplace(std::move(key), value.get<uint32_t>());
    }

    return qos;
  }

  static size_t
  init_runlist_threshold(const json& j)
  {
    return j.value("/execution/runlist_threshold"_json_pointer, default_runlist_threshold);
  }

  // There is a windows compiler bug here.  This function should not use
  // std::unique_ptr but rather by-value profile::execution, but that fails
  // to compile, even as it works as expected with GCC.
  // static std::vector<xecution>
  static std::vector<std::unique_ptr<execution>>
  create_executions(profile* profile, recipe* recipe, const json& j)
  {
    //std::vector<execution> executions;
    std::vector<std::unique_ptr<execution>> executions;
    for (const auto& [name, node] : j.items())
      //executions.emplace_back(profile, recipe, node);
      executions.emplace_back(std::make_unique<execution>(profile, recipe, node));

    return executions;
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
    : m_profile_json(load_json(profile)) // cannot use brace-initialization (see nlohmann FAQ)
    , m_repo{std::move(repo)}
    , m_qos{init_qos(m_profile_json.value("qos", json::object()))}
    , m_runlist_threshold{init_runlist_threshold(m_profile_json)}
    , m_recipe{device, load_json(recipe), m_qos, m_runlist_threshold, m_repo.get()}
    , m_bindings{device, m_profile_json.value("bindings", json::object()), m_repo.get()}
    , m_execution(this, &m_recipe, m_profile_json.value("execution", json::object()), true)
    , m_executions(create_executions(this, &m_recipe, m_profile_json.value("executions", json::object())))
  {}

  void
  bind(const std::string& name, const xrt::bo& bo)
  {
    m_recipe.bind(name, bo);
  }

  void
  execute()
  {
    // legacy
    if (m_executions.empty())
      m_execution.execute();
    
    for (auto& exec : m_executions)
      exec->execute();
  }

  void
  wait()
  {
    // waiting is controlled through execution in json
    // so a noop here
  }

  json
  get_report() const
  {
    json rpt = json::object();
    insert_json_object(rpt, m_recipe.get_report());

    if (m_executions.empty()) {
      insert_json_object(rpt, m_execution.get_report());
      return rpt;
    }
    else {
      rpt["executions"] = json::array();
      for (auto& exec : m_executions)
        rpt["executions"].push_back(exec->get_report());
    }

    return rpt;
  }

  xrt::detail::span<const std::byte>
  map_buffer(const std::string& nm) const
  {
    return m_recipe.map_buffer(nm);
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

  virtual std::string
  get_report() const = 0;

  virtual xrt::detail::span<const std::byte>
  map_buffer(const std::string&) const = 0;
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

  std::string
  get_report() const override
  {
    return m_recipe.get_report().dump();
  }

  xrt::detail::span<const std::byte>
  map_buffer(const std::string& nm) const override
  {
    return m_recipe.map_buffer(nm);
  }
}; // class recipe_impl

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

  std::string
  get_report() const override
  {
    return m_profile.get_report().dump();
  }

  xrt::detail::span<const std::byte>
  map_buffer(const std::string& nm) const override
  {
    return m_profile.map_buffer(nm);
  }
}; // class profile_impl

// Implementation of base device exception class
class runner::error_impl
{
public:
  std::string m_message;

  explicit
  error_impl(std::string message)
    : m_message(std::move(message))
  {}
};

////////////////////////////////////////////////////////////////
// Public runner interface APIs
////////////////////////////////////////////////////////////////
runner::error::
error(const std::string& message)
  : xrt::detail::pimpl<runner::error_impl>(std::make_shared<runner::error_impl>(message))
{}

const char*
runner::error::
what() const noexcept
{
  return handle->m_message.c_str();
}           

runner::
runner(const xrt::device& device,
       const std::string& recipe)
  : xrt::detail::pimpl<runner_impl>{std::make_unique<recipe_impl>
           (device, recipe, std::make_shared<artifacts::file_repo>())}
{} 
  
runner::
runner(const xrt::device& device,
       const std::string& recipe,
       const std::filesystem::path& dir)
  : xrt::detail::pimpl<runner_impl>{std::make_unique<recipe_impl>
           (device, recipe, std::make_shared<artifacts::file_repo>(dir))}
{} 

runner::
runner(const xrt::device& device,
       const std::string& recipe,
       const artifacts_repository& repo)
  : xrt::detail::pimpl<runner_impl>{std::make_unique<recipe_impl>
           (device, recipe, std::make_shared<artifacts::ram_repo>(repo))}
{}

runner::
runner(const xrt::device& device,
       const std::string& recipe, const std::string& profile)
  : xrt::detail::pimpl<runner_impl>{std::make_unique<profile_impl>
           (device, recipe, profile, std::make_shared<artifacts::file_repo>())}
{}

runner::
runner(const xrt::device& device,
       const std::string& recipe, const std::string& profile,
       const std::filesystem::path& dir)
  : xrt::detail::pimpl<runner_impl>{std::make_unique<profile_impl>
           (device, recipe, profile, std::make_shared<artifacts::file_repo>(dir))}
{}

runner::
runner(const xrt::device& device,
       const std::string& recipe, const std::string& profile,
       const artifacts_repository& repo)
  : xrt::detail::pimpl<runner_impl>{std::make_unique<profile_impl>
           (device, recipe, profile, std::make_shared<artifacts::ram_repo>(repo))}
{}

void
runner::
bind_input(const std::string& name, const xrt::bo& bo)
{
  handle->bind_input(name, bo);
}

// bind_output() - Bind a buffer object to an output tensor
void
runner::
bind_output(const std::string& name, const xrt::bo& bo)
{
  handle->bind_output(name, bo);
}

void
runner::
bind(const std::string& name, const xrt::bo& bo)
{
  handle->bind(name, bo);
}

// execute() - Execute the runner
void
runner::
execute()
{
  handle->execute();
}

void
runner::
wait()
{
  handle->wait();
}

std::string
runner::
get_report() const
{
  return handle->get_report();
}

xrt::detail::span<const std::byte>
runner::
map_buffer(const std::string& name) const
{
  return handle->map_buffer(name);
}

} // namespace xrt_core
