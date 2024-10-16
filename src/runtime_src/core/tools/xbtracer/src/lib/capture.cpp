// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "capture.h"
#include "logger.h"

#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <utility>

namespace xrt::tools::xbtracer {

xrt_ftbl& xrt_ftbl::get_instance()
{
  static xrt_ftbl instance;
  return instance;
}

/* Warning: initialization of 'dtbl' with static storage duration may throw
 * an exception that cannot be caught
 * The singleton pattern using a static method-local variable ensures safe,
 * lazy, and thread-safe initialization.
 * The static variable inside get_instance avoids issues related to static
 * initialization order by deferring initialization until first use.
 */
// NOLINTNEXTLINE(cert-err58-cpp)
const static xrt_ftbl& dtbl = xrt_ftbl::get_instance();
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::unordered_map <void*, std::string> fptr2fname_map;

/* This will create association between function name
 * and function pointer of the original library file
 * which will be used to invoke API's from original library.
 */
/* Static initialization might depend on the order of initialization of other
 * static objects. However, in this case, the initialization of fname2fptr_map to
 * an empty map is straightforward and does not depend on any other static objects.
 */
// NOLINTNEXTLINE(cert-err58-cpp)
const static std::unordered_map < std::string, void **> fname2fptr_map = {
  /* Warning: do not use C-style cast to convert between unrelated types
   * Suppressing the warning is justified because it can simplify code in
   * performance-critical sections where types are well understood, and
   * maintains compatibility with legacy code and third-party libraries.
   */
  // NOLINTBEGIN(cppcoreguidelines-pro-type-cstyle-cast)
  /* device class maps */
  {"xrt::device::device(unsigned int)", (void **) &dtbl.device.ctor},
  {"xrt::device::device(std::string const&)", (void **) &dtbl.device.ctor_bdf},
  {"xrt::device::device(void*)", (void **) &dtbl.device.ctor_dhdl},
  {"xrt::device::register_xclbin(xrt::xclbin const&)", (void **) &dtbl.device.register_xclbin},
  {"xrt::device::load_xclbin(axlf const*)", (void **) &dtbl.device.load_xclbin_axlf},
  {"xrt::device::load_xclbin(std::string const&)", (void **) &dtbl.device.load_xclbin_fnm},
  {"xrt::device::load_xclbin(xrt::xclbin const&)", (void **) &dtbl.device.load_xclbin_obj},
  {"xrt::device::get_xclbin_uuid() const", (void **) &dtbl.device.get_xclbin_uuid},
  {"xrt::device::reset()", (void **) &dtbl.device.reset},

  /* bo class maps */
  {"xrt::bo::bo(xrt::device const&, void*, unsigned long, xrt::bo::flags, unsigned int)",  (void **) &dtbl.bo.ctor_dev_up_s_f_g},
  {"xrt::bo::bo(xrt::device const&, void*, unsigned long, unsigned int)",  (void **) &dtbl.bo.ctor_dev_up_s_g},
  {"xrt::bo::bo(xrt::device const&, unsigned long, xrt::bo::flags, unsigned int)", (void **) &dtbl.bo.ctor_dev_s_f_g},
  {"xrt::bo::bo(xrt::device const&, unsigned long, unsigned int)", (void **) &dtbl.bo.ctor_dev_s_g},
  {"xrt::bo::bo(xrt::hw_context const&, void*, unsigned long, xrt::bo::flags, unsigned int)", (void **) &dtbl.bo.ctor_cxt_up_s_f_g},
  {"xrt::bo::bo(xrt::hw_context const&, void*, unsigned long, unsigned int)", (void **) &dtbl.bo.ctor_cxt_up_s_g},
  {"xrt::bo::bo(xrt::hw_context const&, unsigned long, xrt::bo::flags, unsigned int)", (void **) &dtbl.bo.ctor_cxt_s_f_g},
  {"xrt::bo::bo(xrt::hw_context const&, unsigned long, unsigned int)", (void **) &dtbl.bo.ctor_cxt_s_g},
  {"xrt::bo::bo(void*, int)", (void **) &dtbl.bo.ctor_exp_bo},
  {"xrt::bo::bo(void*, xrt::pid_type, int)", (void **) &dtbl.bo.ctor_exp_bo_pid},
  {"xrt::bo::bo(xrt::bo const&, unsigned long, unsigned long)", (void **) &dtbl.bo.ctor_bo_s_o},
  {"xrt::bo::bo(void*, xcl_buffer_handle)", (void **) &dtbl.bo.ctor_xcl_bh},
  {"xrt::bo::size() const", (void **) &dtbl.bo.size},
  {"xrt::bo::address() const", (void **) &dtbl.bo.address},
  {"xrt::bo::get_memory_group() const", (void **) &dtbl.bo.get_memory_group},
  {"xrt::bo::get_flags() const", (void **) &dtbl.bo.get_flags},
  {"xrt::bo::export_buffer()", (void **) &dtbl.bo.export_buffer},
  {"xrt::bo::async(xclBOSyncDirection, unsigned long, unsigned long)", (void **) &dtbl.bo.async},
  {"xrt::bo::sync(xclBOSyncDirection, unsigned long, unsigned long)",  (void **) &dtbl.bo.sync},
  {"xrt::bo::map()",  (void **) &dtbl.bo.map},
  {"xrt::bo::write(void const*, unsigned long, unsigned long)", (void **) &dtbl.bo.write},
  {"xrt::bo::read(void*, unsigned long, unsigned long)", (void **) &dtbl.bo.read},
  {"xrt::bo::copy(xrt::bo const&, unsigned long, unsigned long, unsigned long)", (void **) &dtbl.bo.copy},
  {"xrt::bo::bo(void*)", (void **) &dtbl.bo.ctor_xcl_bh},
  {"xrt::ext::bo::bo(xrt::hw_context const&, unsigned long, xrt::ext::bo::access_mode)", (void **) &dtbl.ext.bo_ctor_cxt_s_a},

  /* run class maps */
  {"xrt::run::run(xrt::kernel const&)", (void **)  &dtbl.run.ctor},
  {"xrt::run::start()", (void **) &dtbl.run.start},
  {"xrt::run::start(xrt::autostart const&)", (void **) &dtbl.run.start_itr},
  {"xrt::run::stop()", (void **) &dtbl.run.stop},
  {"xrt::run::abort()", (void **) &dtbl.run.abort},
  {"xrt::run::wait(std::chrono::duration<long, std::ratio<1l, 1000l> > const&) const", (void **) &dtbl.run.wait},
  {"xrt::run::wait2(std::chrono::duration<long, std::ratio<1l, 1000l> > const&) const", (void **) &dtbl.run.wait2},
  {"xrt::run::state() const", (void **) &dtbl.run.state},
  {"xrt::run::return_code() const", (void **) &dtbl.run.return_code},
  {"xrt::run::add_callback(ert_cmd_state, std::function<void (void const*, ert_cmd_state, void*)>, void*)", (void **) &dtbl.run.add_callback},
  {"xrt::run::submit_wait(xrt::fence const&)", (void **) &dtbl.run.submit_wait},
  {"xrt::run::submit_signal(xrt::fence const&)", (void **) &dtbl.run.submit_signal},
  {"xrt::run::get_ert_packet() const", (void **) &dtbl.run.get_ert_packet},
  {"xrt::run::set_arg_at_index(int, void const*, unsigned long)", (void **) &dtbl.run.set_arg3},
  {"xrt::run::set_arg_at_index(int, xrt::bo const&)", (void **) &dtbl.run.set_arg2},
  {"xrt::run::update_arg_at_index(int, void const*, unsigned long)", (void **) &dtbl.run.update_arg3},
  {"xrt::run::update_arg_at_index(int, xrt::bo const&)", (void **) &dtbl.run.update_arg2},

  /* kernel class maps */
  {"xrt::kernel::kernel(xrt::device const&, xrt::uuid const&, std::string const&, xrt::kernel::cu_access_mode)", (void **) &dtbl.kernel.ctor},
  {"xrt::kernel::kernel(xrt::hw_context const&, std::string const&)",  (void **) &dtbl.kernel.ctor2},
  {"xrt::kernel::group_id(int) const",  (void **) &dtbl.kernel.group_id},
  {"xrt::kernel::offset(int) const",  (void **) &dtbl.kernel.offset},
  {"xrt::kernel::write_register(unsigned int, unsigned int)",  (void **) &dtbl.kernel.write_register},
  {"xrt::kernel::read_register(unsigned int) const",  (void **) &dtbl.kernel.read_register},
  {"xrt::kernel::get_name() const",  (void **) &dtbl.kernel.get_name},
  {"xrt::kernel::get_xclbin() const",  (void **) &dtbl.kernel.get_xclbin},
  {"xrt::ext::kernel::kernel(xrt::hw_context const&, xrt::module const&, std::string const&)", (void **) &dtbl.ext.kernel_ctor_ctx_m_s},

  /* xclbin class maps */
  {"xrt::xclbin::xclbin(std::string const&)", (void **) &dtbl.xclbin.ctor_fnm },
  {"xrt::xclbin::xclbin(axlf const*)", (void **) &dtbl.xclbin.ctor_axlf },
  {"xrt::xclbin::xclbin(std::vector<char, std::allocator<char> > const&)", (void **) &dtbl.xclbin.ctor_raw},

  /*hw_context class maps*/
  {"xrt::hw_context::hw_context(xrt::device const&, xrt::uuid const&, xrt::hw_context::cfg_param_type const&)", (void **) &dtbl.hw_context.ctor_frm_cfg},
  {"xrt::hw_context::hw_context(xrt::device const&, xrt::uuid const&, xrt::hw_context::access_mode)", (void **) &dtbl.hw_context.ctor_frm_mode},
  {"xrt::hw_context::update_qos(xrt::hw_context::cfg_param_type const&)", (void **) &dtbl.hw_context.update_qos},
  // NOLINTEND(cppcoreguidelines-pro-type-cstyle-cast)

  /*module class maps*/
  {"xrt::module::module(xrt::elf const&)", (void **) &dtbl.module.ctor_elf},
  {"xrt::module::module(void*, size_t, xrt::uuid const&)", (void **) &dtbl.module.ctor_usr_sz_uuid},
  {"xrt::module::module(xrt::module const&, xrt::hw_context const&);", (void **) &dtbl.module.ctor_mod_ctx},
  {"xrt::module::get_cfg_uuid();", (void **) &dtbl.module.get_cfg_uuid},
  {"xrt::module::get_module();", (void **) &dtbl.module.get_hw_context},

    /*elf class maps*/
  {"xrt::elf::elf(std::string const&)", (void **) &dtbl.elf.ctor_str},
  {"xrt::elf::elf(std::istream& stream)", (void **) &dtbl.elf.ctor_ist},
  {"xrt::elf::get_cfg_uuid();", (void **) &dtbl.elf.get_cfg_uuid},
};

} //namespace xrt::tools::xbtracer

#ifdef __linux__
#include <cxxabi.h>
#include <dlfcn.h>
#include <elf.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <fstream>

namespace xrt::tools::xbtracer {

constexpr const char* lib_name = "libxrt_coreutil.so";

// mutex to serialize dlerror and getenv
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::mutex dlerror_mutex;
extern std::mutex env_mutex;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

/**
 * This class will perform following operations.
 * 1. Read mangled symbols from .so file.
 * 2. perform demangling operation.
 * 3. update xrt_dtable which will be used to invoke original API's
 */
class router
{
  private:
  void* handle = nullptr;
  /* library Path */
  std::string m_path;
  /*
   * This will create association between function name (key)
   * and mangled function name (value).
   */
  std::unordered_map<std::string, std::string> func_mangled;

  public:
  router(const router&);
  router& operator = (const router&);
  router(router && other) noexcept;
  router& operator = (router&& other) noexcept;
  void load_func_addr();
  void load_symbols();

  static std::shared_ptr<router> get_instance()
  {
    static auto ptr = std::make_shared<router>();
    return ptr;
  }

  router()
  : m_path("")
  {
    load_symbols();
    load_func_addr();
    /**
     * Unseting the LD_PRELOAD to avoid the multiple instance of loading same library.
     */
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    unsetenv("LD_PRELOAD");
  }

  ~router()
  {
    if (handle)
      dlclose(handle);
  }
};

/* Warning: initialization of 'dptr' with static storage duration may throw
 * an exception that cannot be caught
 * The singleton pattern implemented with a static local variable in
 * get_instance ensures safe, one-time initialization.
 */
// NOLINTNEXTLINE(cert-err58-cpp)
const auto dptr = router::get_instance();

/**
 * This function demangles the input mangled function.
 */
static std::string demangle(const char* mangled_name)
{
  int status = 0;
  char* demangled_name =
      abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status);
  if (status == 0)
  {
    // Use std::unique_ptr with a custom deleter to manage demangled_name
    std::unique_ptr<char, decltype(&std::free)> demangled_ptr(demangled_name,
                                                              std::free);

    /* Conditioning of demangled name because of some platform based deviation
      */
    std::vector<std::pair<std::string, std::string>> replacements =
      {
        {"std::__cxx11::basic_string<char, std::char_traits<char>, "
              "std::allocator<char> >", "std::string"},
        {"[abi:cxx11]", ""},
        {"std::map<std::string, unsigned int, std::less<std::string >, "
            "std::allocator<std::pair<std::string const, unsigned int> > >",
            "xrt::hw_context::cfg_param_type"}
      };

    std::string result =
        find_and_replace_all(std::string(demangled_name), replacements);

    return result;
  }
  else
    // Demangling failed
    return {mangled_name};  // Return the original mangled name
}

static std::string find_library_path()
{
  std::lock_guard<std::mutex> lock(env_mutex);
  // NOLINTNEXTLINE(concurrency-mt-unsafe) - protected by env_mutex
  char* ld_preload = getenv("LD_PRELOAD");
  if (!ld_preload)
  {
    std::cout << "LD_PRELOAD is not set." << std::endl;
    return "";
  }

  std::string full_path = std::string(ld_preload);
  full_path.erase(std::remove(full_path.begin(), full_path.end(), ' '),
                  full_path.end());
  return full_path;
}

/**
 * This function will update the dispatch table
 * with address of the functions from original
 * library.
 */
void router::load_func_addr()
{
  // Load the shared object file
  handle = dlopen(lib_name, RTLD_LAZY);
  if (!handle)
  {
    std::lock_guard<std::mutex> lock(dlerror_mutex);
    // NOLINTNEXTLINE(concurrency-mt-unsafe) - protected by dlerror_mutex
    const char* error_msg = dlerror();
    throw std::runtime_error("Error loading shared library: "
      + std::string(error_msg));
  }

  /**
   * Get Function address's which are of interest and ignore others.
   */
  for (auto& [demangled_name, mangled_name] : func_mangled)
  {
    auto ptr_itr = fname2fptr_map.find(demangled_name);

    if (ptr_itr != fname2fptr_map.end())
    {
      void** func_address_slot = ptr_itr->second;
      /* update the original function address in the dispatch table */
      *func_address_slot = (dlsym(handle, mangled_name.c_str()));
      if (!func_address_slot)
        std::cout << "Null Func address received " << std::endl;
      else
        fptr2fname_map[*func_address_slot] =
          std::regex_replace(demangled_name, std::regex(R"(\)\s*const)"),")");
    }
    else
    {
      //std::cout << "func :: \"" << demangled_name << "\" not found in"
      //          << "fname2fptr_map\n";
    }
  }
}

/**
 * This function will read mangled API's from library and perform
 * Demangling operation.
 */
void router::load_symbols()
{
  constexpr unsigned int symbol_len = 1024;

  m_path = find_library_path();

  // Open the ELF file
  std::ifstream elf_file(m_path, std::ios::binary);
  if (!elf_file.is_open())
    throw std::runtime_error("Failed to open ELF file: " + m_path);

  // Read the ELF header
  Elf64_Ehdr elf_header;
  std::array<char, sizeof(Elf64_Ehdr)> buffer{0};
  elf_file.read(buffer.data(), buffer.size());
  memcpy(&elf_header, buffer.data(), buffer.size());

  // Check ELF magic number
  // NOLINTNEXTLINE (bugprone-suspicious-string-compare)
  if (memcmp(static_cast<const void*>(elf_header.e_ident),
              static_cast<const void*>(ELFMAG), SELFMAG))
    throw std::runtime_error("Not an ELF file");

  // Get the section header table
  elf_file.seekg((std::streamoff)elf_header.e_shoff);
  std::vector<Elf64_Shdr> section_headers(elf_header.e_shnum);

  /* The reinterpret cast is necessary for performing low-level binary file I/O
   * operations involving a specific memory layout.
   * The memory layout and size of section_headers match the binary data being
   * read, ensuring safety and correctness.
   */
  // NOLINTNEXTLINE (cppcoreguidelines-pro-type-reinterpret-cast)
  elf_file.read(reinterpret_cast<char*>(section_headers.data()),
                (std::streamsize)(elf_header.e_shnum * sizeof(Elf64_Shdr)));
  if (!elf_file)
    throw std::runtime_error("Failed to read section header table");

  // Find the symbol table section
  Elf64_Shdr* symtab_section = nullptr;
  for (int i = 0; i < elf_header.e_shnum; ++i)
  {
    if (section_headers[i].sh_type == SHT_DYNSYM)
    {
      symtab_section = &section_headers[i];
      break;
    }
  }

  if (!symtab_section)
    throw std::runtime_error("Symbol table section not found");

  // Read and print the mangled function names from the symbol table section
  unsigned long num_symbols = symtab_section->sh_size / sizeof(Elf64_Sym);
  for (unsigned long i = 0; i < num_symbols; ++i)
  {
    Elf64_Sym symbol;
    elf_file.seekg(
        (std::streamoff)(symtab_section->sh_offset + i * sizeof(Elf64_Sym)));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    elf_file.read(reinterpret_cast<char*>(&symbol), sizeof(Elf64_Sym));
    if (!elf_file)
      throw std::runtime_error("Failed to read symbol table entry");

    // Check if the symbol is a function
    if ((ELF64_ST_TYPE(symbol.st_info) == STT_FUNC) &&
        (ELF64_ST_BIND(symbol.st_info) == STB_GLOBAL) &&
        (ELF64_ST_VISIBILITY(symbol.st_other) == STV_DEFAULT) &&
        (symbol.st_shndx != SHN_UNDEF))
    {
      std::array<char, symbol_len> symbol_name{0};
      elf_file.seekg(
          (std::streamoff)section_headers[symtab_section->sh_link].sh_offset +
          symbol.st_name);
      elf_file.read(symbol_name.data(), symbol_len);
      if (!elf_file)
        throw std::runtime_error("Failed to read symbol name");

      // std::cout <<"Mangled name: "<<symbol_name.data() <<std::endl;
      std::string demangled_name = demangle(symbol_name.data());
      // std::cout <<"De-Mangled name: " << demangled_name << "\n";
      func_mangled[demangled_name] = symbol_name.data();
    }
  }
}
}  // namespace xrt::tools::xbtracer

#elif _WIN32
#include <Dbghelp.h>
#include <windows.h>
#pragma comment(lib, "Dbghelp.lib")

namespace xrt::tools::xbtracer {
  std::string demangle(const char* mangled)
  {
    constexpr const DWORD length = 512;  // Adjust the buffer size as needed
    char demangled_str[length];

    DWORD result = UnDecorateSymbolName(
        mangled, demangled_str, length,
        UNDNAME_NO_FUNCTION_RETURNS | UNDNAME_NO_ACCESS_SPECIFIERS |
            UNDNAME_NO_ALLOCATION_LANGUAGE | UNDNAME_NO_ALLOCATION_MODEL |
            UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_THROW_SIGNATURES);

    if (result != 0)
    {
      std::vector<std::pair<std::string, std::string>> replacements =
      {
        { "class std::basic_string<char,struct std::char_traits<char>,"
            "class std::allocator<char> >", "std::string" },
        {"const ", "const"},
        {"class ", ""},
        {",", ", "},
        {")const", ") const"},
        {"__int64", "long"},
        {"(void)", "()"},
        {"enum ", ""},
        {"struct std::ratio<1, 1000>", "std::ratio<1l, 1000l>"},
        {"std::map<std::string, unsigned int, struct std::less<std::string >, "
         "std::allocator<struct std::pair<std::string const, unsigned int> > >",
             "xrt::hw_context::cfg_param_type"},
        {"void *", "void*"}
      };

      std::string demangled_and_conditioned_str =
          find_and_replace_all(demangled_str, replacements);
      return demangled_and_conditioned_str;
    }
    else
      return mangled;
  }

  // Make the page writable and replace the function pointer. Once
  // replacement is completed restore the page protection.
  static void replace_func(PIMAGE_THUNK_DATA thunk, void* func_ptr)
  {
    // Make page writable temporarily:
    MEMORY_BASIC_INFORMATION mbinfo;
    VirtualQuery(thunk, &mbinfo, sizeof(mbinfo));
    if (!VirtualProtect(mbinfo.BaseAddress, mbinfo.RegionSize,
                        PAGE_EXECUTE_READWRITE, &mbinfo.Protect))
      return;

    // Replace function pointer with our implementation:
    thunk->u1.Function = (ULONG64)func_ptr;

    // Restore page protection:
    DWORD zero = 0;
    if (!VirtualProtect(mbinfo.BaseAddress, mbinfo.RegionSize,
                        mbinfo.Protect, &zero))
      return;
  }

  PIMAGE_IMPORT_DESCRIPTOR get_import_descriptor(LPVOID image_base)
  {
    PIMAGE_DOS_HEADER dos_header = (PIMAGE_DOS_HEADER)image_base;
    if (dos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
      std::cerr << "Invalid DOS signature\n";
      return nullptr;
    }

    PIMAGE_NT_HEADERS nt_headers =
        (PIMAGE_NT_HEADERS)((DWORD_PTR)image_base + dos_header->e_lfanew);
    if (nt_headers->Signature != IMAGE_NT_SIGNATURE)
    {
      std::cerr << "Invalid NT signature\n";
      return nullptr;
    }

    IMAGE_DATA_DIRECTORY importsDirectory =
        nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (importsDirectory.Size == 0)
    {
      std::cerr << "No import directory found\n";
      return nullptr;
    }

    return
        (PIMAGE_IMPORT_DESCRIPTOR)(importsDirectory.VirtualAddress +
                                   (DWORD_PTR)image_base);

  }

  void update_function_entry(LPVOID image_base, HMODULE library,
	PIMAGE_THUNK_DATA original_first_thunk, PIMAGE_THUNK_DATA first_thunk,
    bool debug)
  {
    PIMAGE_IMPORT_BY_NAME function_name =
            (PIMAGE_IMPORT_BY_NAME)((DWORD_PTR)image_base +
                                    original_first_thunk->u1.AddressOfData);
    auto ptr_itr = xtx::fname2fptr_map.find(xtx::demangle(function_name->Name));
    if (ptr_itr != xtx::fname2fptr_map.end())
    {
      void** func_address_slot = ptr_itr->second;
      /* update the original function address in the dispatch table */
      *func_address_slot = reinterpret_cast<void*>(first_thunk->u1.Function);
      fptr2fname_map[*func_address_slot]
	= std::regex_replace(ptr_itr->first, std::regex(R"(\)\s*const)"), ")");
      void* func_ptr = GetProcAddress(library, function_name->Name);
      if (func_ptr)
      {
        if (debug)
          std::cout << demangle(function_name->Name).c_str()
            << "\n\tOrg = " << std::uppercase << std::hex << std::setw(16)
            << std::setfill('0') << first_thunk->u1.Function << " New = "
            << std::uppercase << std::hex << std::setw(16)
            << std::setfill('0') << (ULONG64)func_ptr <<"\n";

        replace_func(first_thunk, func_ptr);
      }
    }
    else if (debug)
      std::cout << "func :: \"" << demangle(function_name->Name)
                << "\" not found in fname2fptr_map\n";
  }
}  // namespace xrt::tools::xbtracer

namespace xtx = xrt::tools::xbtracer;

// Iterate through the IDT for all table entry corresponding to xrt_coreutil.dll
// and replace the function pointer in first_thunk by looking for the same name
// into the xrt_capture.dll for the same name.
void idt_fixup(void* dummy)
{
  static bool inst_debug = false;
  std::string filename("");
  TCHAR buffer[128];
  DWORD result = GetEnvironmentVariable(TEXT("INST_DEBUG"), buffer, 128);
  if (result > 0 && result < 128 && !strcmp(buffer, "TRUE"))
    inst_debug = true;

  LPVOID image_base;
  if (dummy != NULL)
  {
    std::filesystem::path path((const char*)dummy);
    filename = path.filename().string();
    image_base = GetModuleHandleA(filename.c_str());
  }
  else
    image_base = GetModuleHandleA(NULL);

  if (inst_debug)
    std::cout << "\nENTRY idt_fixup (" << filename << ")\nimage_base = "
              << image_base << "\n";

  PIMAGE_IMPORT_DESCRIPTOR import_descriptor
				= xtx::get_import_descriptor(image_base);
  if (!import_descriptor)
  {
    std::cerr << "idt_fixup : Failed to get import descriptor\n";
    return;
  }

  HMODULE library = NULL;
  GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCTSTR>(&idt_fixup), &library);
  if(!library)
  {
    std::cerr << "idt_fixup : Failed to get library handle\n";
    return;
  }

  while (import_descriptor->Name != NULL)
  {
    LPCSTR library_name =
        reinterpret_cast<LPCSTR>(reinterpret_cast<DWORD_PTR>(image_base) +
                                 import_descriptor->Name);

    #if defined(_MSC_VER)
    #define stricmp _stricmp
    #else
    #include <strings.h> // POSIX header for strcasecmp
    #define stricmp strcasecmp
    #endif
    if (!stricmp(library_name, "xrt_coreutil.dll"))
    {
      PIMAGE_THUNK_DATA original_first_thunk = NULL, first_thunk = NULL;
      original_first_thunk =
          (PIMAGE_THUNK_DATA)((DWORD_PTR)image_base +
                              import_descriptor->OriginalFirstThunk);
      first_thunk = (PIMAGE_THUNK_DATA)((DWORD_PTR)image_base +
                                       import_descriptor->FirstThunk);
      while (original_first_thunk->u1.AddressOfData != NULL)
      {
        xtx::update_function_entry(image_base, library, original_first_thunk,
                              first_thunk, inst_debug);
        ++original_first_thunk;
        ++first_thunk;
      }
    }

    import_descriptor++;
  }

  if (inst_debug)
    std::cout << "EXIT idt_fixup ("<< filename << ")\n\n";

  return;
}
#endif /* #ifdef __linux__ */
