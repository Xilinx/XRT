/*
 * Copyright (C) 2021 Xilinx, Inc
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
#define XRT_CORE_COMMON_SOURCE
#include <regex>
#include "pskernel_parse.h"


namespace xrt_core { namespace pskernel {

// Function to match function name
// and extract formal parameters
std::vector<xrt_core::pskernel::kernel_argument>
extract_args (Dwarf_Die *die)
{
  // Mapping for FFI types
  static const std::map<std::pair<Dwarf_Word, Dwarf_Word>, ffi_type*> typeTable = {
    { {DW_ATE_unsigned_char, 1 }, &ffi_type_uint8 },
    { {DW_ATE_signed_char, 1 }, &ffi_type_sint8 },
    { {DW_ATE_unsigned, 2 }, &ffi_type_uint16 },
    { {DW_ATE_signed, 2 }, &ffi_type_sint16 },
    { {DW_ATE_unsigned, 4 }, &ffi_type_uint32 },
    { {DW_ATE_signed, 4 }, &ffi_type_sint32 },
    { {DW_ATE_unsigned, 8 }, &ffi_type_uint64 },
    { {DW_ATE_signed, 8 }, &ffi_type_sint64 },
    { {DW_ATE_float, 4 }, &ffi_type_float },
    { {DW_ATE_float, 8 }, &ffi_type_double }
  };
  
  Dwarf_Die child;
  Dwarf_Attribute attr_mem;
  Dwarf_Die type_mem;
  Dwarf_Die *type = nullptr;
  Dwarf_Word var_size;
  const char *var_name;
  std::vector<xrt_core::pskernel::kernel_argument> return_args;
  int offset = 4;
  int index = 0;

  if (dwarf_child (die, &child) == 0)
    do
      switch (dwarf_tag (&child)) {
	case DW_TAG_formal_parameter:
	  // Extract parameter name and type
	  xrt_core::pskernel::kernel_argument arg;
	  var_name = dwarf_diename(&child);
	  arg.name = var_name;
	  type = dwarf_formref_die(dwarf_attr(&child, DW_AT_type, &attr_mem),&type_mem);
	  while (dwarf_tag(type) == DW_TAG_typedef) {
	    type = dwarf_formref_die(dwarf_attr(type, DW_AT_type, &attr_mem),&type_mem);
	  }
	  if (dwarf_tag(type) == DW_TAG_base_type) {
	    Dwarf_Attribute encoding;
	    Dwarf_Word enctype = 0;
	    if (dwarf_attr (type, DW_AT_encoding, &encoding) == nullptr
		|| dwarf_formudata (&encoding, &enctype) != 0)
	      throw std::runtime_error("base type without encoding");
	    
	    Dwarf_Attribute bsize;
	    Dwarf_Word bits;
	    if (dwarf_attr (type, DW_AT_byte_size, &bsize) != nullptr
		&& dwarf_formudata (&bsize, &bits) == 0) {
	      arg.size = bits;
	    } else if (dwarf_attr (type, DW_AT_bit_size, &bsize) == nullptr
		     || dwarf_formudata (&bsize, &bits) != 0)
	      throw std::runtime_error("base type without byte or bit size");

            auto result = typeTable.find({ enctype, bits});
            ffi_type *enc_ffi_type = result->second;
            switch(enctype) {
              case DW_ATE_signed_char:
                arg.hosttype = "int8_t";
                break;
              case DW_ATE_unsigned_char:
                arg.hosttype = "uint8_t";
                break;
              case DW_ATE_signed:
                if (bits==2)
                  arg.hosttype = "int16_t";
                if (bits==4)
                  arg.hosttype = "int";
                if (bits==8)
                  arg.hosttype = "int64_t";
                break;
              case DW_ATE_unsigned:
                if (bits==2)
                  arg.hosttype = "uint16_t";
                if (bits==4)
                  arg.hosttype = "uint32_t";
                if (bits==8)
                  arg.hosttype = "uint64_t";
                break;
              case DW_ATE_float:
                if (bits==4)
                  arg.hosttype = "float";
                if (bits==8)
                  arg.hosttype = "double";
                break;
            }
            arg.ffitype = *enc_ffi_type;
            arg.offset = offset;
            arg.type = xrt_core::pskernel::kernel_argument::argtype::scalar;
            offset = offset + bits;
            arg.index = index;
            index++;
          } else if (dwarf_tag(type) == DW_TAG_pointer_type) {
            arg.size = 16;
            arg.offset = offset;
            offset = offset + 16;  // Next offset increase by 3 words to account for 64-bit address and 64-bit size
            arg.index = index;
            arg.type = xrt_core::pskernel::kernel_argument::argtype::global;
            ffi_type *enc_ffi_type = &ffi_type_pointer;
            arg.ffitype = *enc_ffi_type;
            index++;
          }
          if (dwarf_aggregate_size(type, &var_size) < 0)
            throw std::runtime_error("ERROR: Variable Size incorrect!\n");

          return_args.emplace_back(arg);
          break;
        } while (dwarf_siblingof (&child, &child) == 0);

  return return_args;
}

// Function to parse object from file on disk
std::vector<xrt_core::pskernel::kernel_argument> 
pskernel_parse(const char *so_file, const char *func_name) 
{

  int fd = open(so_file, O_RDONLY);
  Dwarf *dw = dwarf_begin(fd, DWARF_C_READ);
  std::vector<xrt_core::pskernel::kernel_argument> args;

  if (dw != nullptr) {
    Dwarf_Off offset;
    Dwarf_Off old_offset = 0;
    size_t h_size;
    Dwarf_Off abbrev;
    uint8_t address_size;
    uint8_t offset_size;
    
    while (dwarf_nextcu (dw, old_offset, &offset, &h_size, &abbrev, &address_size, &offset_size) == 0) {
      Dwarf_Die cudie_mem;
      Dwarf_Die *cudie = dwarf_offdie (dw, old_offset + h_size, &cudie_mem);
      Dwarf_Die child;
      Dwarf_Die *func_die;
      
      if (dwarf_child (cudie, &child) == 0)
        do {
          if(dwarf_tag(&child) == DW_TAG_subprogram) {
            const char *name = dwarf_diename (&child);
            if (fnmatch(func_name,name,0) == 0) {
              func_die = &child;
              args = extract_args (func_die);
            }
          }
        } while (dwarf_siblingof (&child, &child) == 0);
      
      old_offset = offset;
    }
    
    dwarf_end(dw);
  }

  if (args.empty())
    throw std::runtime_error("No PS kernel arguments found!");

  return args;
}

// Function to parse from object in memory
std::vector<xrt_core::pskernel::kernel_argument> 
pskernel_parse(char *so_file, size_t size, const char *func_name) 
{

  Elf *ehandle = elf_memory(const_cast<char *>(so_file), size);
  Dwarf *dw = dwarf_begin_elf(ehandle, DWARF_C_READ, nullptr);
  std::vector<xrt_core::pskernel::kernel_argument> args;
  
  if (dw != nullptr) {
    Dwarf_Off offset;
    Dwarf_Off old_offset = 0;
    size_t h_size;
    Dwarf_Off abbrev;
    uint8_t address_size;
    uint8_t offset_size;
    
    while (dwarf_nextcu (dw, old_offset, &offset, &h_size, &abbrev, &address_size, &offset_size) == 0) {
        Dwarf_Die cudie_mem;
        Dwarf_Die *cudie = dwarf_offdie (dw, old_offset + h_size, &cudie_mem);
        Dwarf_Die child;
        Dwarf_Die *func_die = nullptr;
	
	if (dwarf_child (cudie, &child) == 0)
	  do {
	    if (dwarf_tag(&child) == DW_TAG_subprogram) {
	      const char *name = dwarf_diename (&child);
	      if (fnmatch(func_name,name,0)==0 ) {
		func_die = &child;
		args = extract_args (func_die);
	      }
	    }
	  } while (dwarf_siblingof (&child, &child) == 0);
	
	old_offset = offset;
      }
    
    dwarf_end(dw);
  }

  if (args.empty())
    throw std::runtime_error("No PS kernel arguments found!");
 
  return args;
}
}}
