/**
 * Copyright (C) 2019 Xilinx, Inc
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

#ifndef xclbin_parser_h_
#define xclbin_parser_h_

#include "core/common/config.h"
#include "xclbin.h"
#include <string>
#include <vector>

namespace xrt_core { namespace xclbin {

/**
 * struct kernel_argument -
 */
struct kernel_argument
{
  static constexpr size_t no_index { std::numeric_limits<size_t>::max() };
  // numbering must match that of meta data addressQualifier
  enum class argtype { scalar = 0, global = 1, constant=2, local=3, stream = 4 };
  enum class direction { input = 0, output = 1};

  std::string name;
  std::string hosttype;
  size_t index = no_index;
  size_t offset = 0;
  size_t size = 0;
  size_t hostsize = 0;
  size_t fa_desc_offset = 0;
  argtype type;
  direction dir;
};

struct kernel_object
{
    std::string name;
    std::vector<kernel_argument> args;
};

/**
 * struct softkernel_object - wrapper for a soft kernel object
 *
 * @ninst: number of instances
 * @symbol_name: soft kernel symbol name
 * @size: size of soft kernel image
 * @sk_buf: pointer to the soft kernel buffer
 */
struct softkernel_object
{
  uint32_t ninst;
  std::string mpo_name;
  std::string mpo_version;
  std::string symbol_name;
  size_t size;
  char *sk_buf;
};

/**
 * get_axlf_section_header() - retrieve axlf section header
 *
 * @top: axlf to retrieve section from
 * @kind: section kind to retrieve
 *
 * This function treats group sections conditionally based on
 * xrt.ini settings
 */ 
XRT_CORE_COMMON_EXPORT
const axlf_section_header*
get_axlf_section(const axlf* top, axlf_section_kind kind);

/**
 * Get specific binary section of the axlf structure
 *
 * auto data = axlf_section_type::get<const ip_layout*>(top,axlf_section_kind::IP_LAYOUT);
 */
template <typename SectionType>
struct axlf_section_type;

template <typename SectionType>
struct axlf_section_type<SectionType*>
{
  static SectionType*
  get(const axlf* top, axlf_section_kind kind)
  {
    if (auto header = get_axlf_section(top, kind)) {
      auto begin = reinterpret_cast<const char*>(top) + header->m_sectionOffset ;
      return reinterpret_cast<SectionType*>(begin);
    }
    return nullptr;
  }
};

/**
 * memidx_to_name() - Convert mem topology memory index to name
 */
XRT_CORE_COMMON_EXPORT
std::string
memidx_to_name(const mem_topology* mem, int32_t midx);

/**
 * get_first_used_mem() - Get the first used memory bank index
 */
int32_t
get_first_used_mem(const axlf* top);

/**
 * address_to_memidx() - Map memory address to memory bank index
 */
int32_t
address_to_memidx(const mem_topology* mem, uint64_t address);

/**
 * get_max_cu_size() - Compute max register map size of CUs in xclbin
 */
XRT_CORE_COMMON_EXPORT
size_t
get_max_cu_size(const char* xml_data, size_t xml_size);

/**
 * get_cus() - Get sorted list of CU base addresses in xclbin.
 *
 * @encode: If true encode control protocol in lower address bit
 */
XRT_CORE_COMMON_EXPORT
std::vector<uint64_t>
get_cus(const char* xml_data, size_t xml_size, bool encode=false);

XRT_CORE_COMMON_EXPORT
std::vector<uint64_t>
get_cus(const ip_layout* ip_layout, bool encode=false);

XRT_CORE_COMMON_EXPORT
std::vector<uint64_t>
get_cus(const axlf* top, bool encode=false);

/**
 * get_cus() - Get list of ip_data matching name
 *
 * @kname:  Name of compute unit to match
 *
 * The kernel name can optionally specify which kernel instance(s) to
 * match "kernel:{cu1,cu2,...} syntax.
 */
XRT_CORE_COMMON_EXPORT
std::vector<const ip_data*>
get_cus(const ip_layout* ip_layout, const std::string& kname);

/**
 * get_ip_name() - Get name of IP with specified base addr
 *
 * @ip_layout: Xclbin IP_LAYOUT
 * @addr: Base address of IP to look up
 * Return: Nname of IP
 */
XRT_CORE_COMMON_EXPORT
std::string
get_ip_name(const ip_layout* ip_layout, uint64_t addr);

XRT_CORE_COMMON_EXPORT
std::string
get_ip_name(const axlf* top, uint64_t addr);

XRT_CORE_COMMON_EXPORT
std::vector<std::pair<uint64_t, size_t>>
get_debug_ips(const axlf* top);

/**
 * get_cu_control() - Get the IP_CONTROL type of CU at specified address
 */
XRT_CORE_COMMON_EXPORT
uint32_t
get_cu_control(const ip_layout* ip_layout, uint64_t cuaddr);

/**
 * get_cu_base_offset() - Get minimum base offset of all IP_KERNEL objects
 */
XRT_CORE_COMMON_EXPORT
uint64_t
get_cu_base_offset(const ip_layout* ip_layout);

XRT_CORE_COMMON_EXPORT
uint64_t
get_cu_base_offset(const axlf* top);

/**
 * get_cuisr() - Check if all kernels support interrupt
 */
XRT_CORE_COMMON_EXPORT
bool
get_cuisr(const ip_layout* ip_layout);

XRT_CORE_COMMON_EXPORT
bool
get_cuisr(const axlf* top);

/**
 * get_dataflow() - Check if any kernel in xclbin is a dataflow kernel
 */
XRT_CORE_COMMON_EXPORT
bool
get_dataflow(const ip_layout* ip_layout);

XRT_CORE_COMMON_EXPORT
bool
get_dataflow(const axlf* top);

/**
 * get_cus_pair() - Get list CUs physical address & size pair
 */
XRT_CORE_COMMON_EXPORT
std::vector<std::pair<uint64_t, size_t>>
get_cus_pair(const axlf* top);

/**
 * get_dbg_ips_pair() - Get list of Debug IPs physical address & size pair
 */
XRT_CORE_COMMON_EXPORT
std::vector<std::pair<uint64_t, size_t>>
get_dbg_ips_pair(const axlf* top);

/**
 * get_softkernel() - Get soft kernels.
 */
std::vector<softkernel_object>
get_softkernels(const axlf* top);

/**
 * get_kernel_freq() - Get kernel frequency.
 */
size_t
get_kernel_freq(const axlf* top);

/**
 * get_kernel_arguments() - Get argument meta data for a kernel
 *
 * @xml_data: XML metadata from xclbin
 * @xml_size: Size of XML metadata from xclbin
 * @kname : Name of kernel
 * Return: List of argument per struct kernel_argument
 */
XRT_CORE_COMMON_EXPORT
std::vector<kernel_argument>
get_kernel_arguments(const char* xml_data, size_t xml_size, const std::string& kname);

/**
 * get_kernel_arguments() - Get argument meta data for a kernel
 *
 * @kname : Name of kernel
 * Return: List of argument per struct kernel_argument
 */
XRT_CORE_COMMON_EXPORT
std::vector<kernel_argument>
get_kernel_arguments(const axlf* top, const std::string& kname);

/**
 * get_kernels() - Get meta data for all kernels
 *
 * Return: List of struct kernel_object
 */
XRT_CORE_COMMON_EXPORT
std::vector<kernel_object>
get_kernels(const axlf* top);

/**
 * is_pdi_only() - If the xclbin has only one section and is PDI
 */
XRT_CORE_COMMON_EXPORT
bool
is_pdi_only(const axlf* top);

/**
 * get_vbnv() - Get VBNV of xclbin
 */
XRT_CORE_COMMON_EXPORT
std::string
get_vbnv(const axlf* top);

}} // xclbin, xrt_core

#endif
