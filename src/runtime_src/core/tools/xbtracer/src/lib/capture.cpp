// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#include "capture.h"
#include "logger.h"

#include <filesystem>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
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
  {"xrt::device::load_xclbin(std::string const&)", (void **) &dtbl.device.load_xclbin_fnm},
  // NOLINTEND(cppcoreguidelines-pro-type-cstyle-cast)
};

} //namespace xrt::tools::xbtracer


  std::array<char, sizeof(Elf64_Ehdr)> buffer{0};
  elf_file.read(buffer.data(), buffer.size());
  memcpy(&elf_header, buffer.data(), buffer.size());

  // Check ELF magic number
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
#endif /* #ifdef __linux__ */
