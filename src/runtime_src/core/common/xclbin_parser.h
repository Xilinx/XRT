/**
 * Copyright (C) 2019-2022 Xilinx, Inc
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

#include "config.h"
#include "cuidx_type.h"
#include "core/include/xrt/detail/xclbin.h"
#include "core/include/xrt/xrt_uuid.h"

#include <array>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace xrt_core { namespace xclbin {

// struct kernel_argument - kernel argument meta data
struct kernel_argument
{
  static constexpr size_t no_index { std::numeric_limits<size_t>::max() };
  // numbering must match that of meta data addressQualifier
  enum class argtype { scalar = 0, global = 1, constant=2, local=3, stream = 4 };
  enum class direction { input = 0, output = 1};

  std::string name;
  std::string hosttype;
  std::string port;
  size_t port_width = 0;
  size_t index = no_index;
  size_t offset = 0;
  size_t size = 0;
  size_t hostsize = 0;
  size_t fa_desc_offset = 0;
  argtype type;
  direction dir;
};

// struct kernel_properties - kernel property metadata
struct kernel_properties
{
  enum class kernel_type { none, pl, ps, dpu };
  enum class mailbox_type { none, in , out, inout };
  using restart_type = size_t;
  std::string name;
  kernel_type type = kernel_type::none;
  restart_type counted_auto_restart = 0;
  mailbox_type mailbox = mailbox_type::none;
  size_t address_range = 0x10000;  // NOLINT, default address range
  bool sw_reset = false;
  size_t functional = 0;
  size_t kernel_id = 0;

  // opencl specifics
  size_t workgroupsize = 0;
  std::array<size_t, 3> compileworkgroupsize {0};
  std::array<size_t, 3> maxworkgroupsize {0};
  std::map<uint32_t, std::string> stringtable;
};

struct kernel_object
{
  std::string name;
  std::vector<kernel_argument> args;
  size_t range;
  bool sw_reset;
};

// struct softkernel_object - wrapper for a soft kernel object
//
// @ninst: number of instances
// @symbol_name: soft kernel symbol name
// @size: size of soft kernel image
// @sk_buf: pointer to the soft kernel buffer
struct softkernel_object
{
  uint32_t ninst = 0;
  std::string mpo_name;
  std::string mpo_version;
  std::string symbol_name;
  size_t size = 0;
  char *sk_buf = nullptr;
};

// struct aie_cdo_obj - wrapper for an AIE CDO group object
//
// @cdo_name: CDO group name
// @cdo_type: CDO group type
// @pdi_id: ID of this CDO group in PDI
struct aie_cdo_group_obj
{
  std::string cdo_name;
  uint8_t cdo_type;
  uint64_t pdi_id;
  std::vector<uint64_t> kernel_ids;
};

// struct aie_pdi_obj - wrapper for an AIE PDI object
//
// @uuid: PDI UUID
// @cdo_groups: Array of CDO groups
// @pdi: PDI blob
struct aie_pdi_obj
{
  xrt::uuid uuid;
  std::vector<aie_cdo_group_obj> cdo_groups;
  std::vector<uint8_t> pdi;
};

// struct aie_partition_obj - wrapper for an AIE Partition object
//
// @ncol: number of columns in this partition
// @start_col_list: Array of start column for partition relocation
// @name: partition name
// @ops_per_cycle: Operations per AIE cycle
// @pdis: PDIs (blob and metadata) associated with this partition
struct aie_partition_obj
{
  uint16_t ncol;
  std::vector<uint16_t> start_col_list;
  std::string name;
  uint32_t ops_per_cycle;
  std::vector<aie_pdi_obj> pdis;
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

// get_cu_indices() - mapping from cu name to its index type
//
// Compute index type for all controllable IPs in IP_LAYOUT Normally
// indexing is determined by KDS in driver via kds_cu_info query
// request, but in emulation mode that query request is not not
// implemented
std::map<std::string, cuidx_type>
get_cu_indices(const ip_layout* ip_layout);

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

XRT_CORE_COMMON_EXPORT
aie_partition_obj
get_aie_partition(const axlf* top);

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
 * get_kernel_properties() -  Get kernel property meta data
 *
 * @xml_data: XML metadata from xclbin
 * @xml_size: Size of XML metadata from xclbin
 * @kname : Name of kernel
 * Return: Properties for kernel extracted from XML meta data
 */
XRT_CORE_COMMON_EXPORT
kernel_properties
get_kernel_properties(const char* xml_data, size_t xml_size, const std::string& kname);

/**
 * get_kernel_properties() -  Get kernel property meta data
 *
 * @top : Full axlf
 * @kname : Name of kernel
 * Return: Properties for kernel extracted from XML meta data
 */
XRT_CORE_COMMON_EXPORT
kernel_properties
get_kernel_properties(const axlf* top, const std::string& kname);

/**
 * get_kernels() - Get meta data for all kernels
 *
 * Return: List of struct kernel_object
 */
XRT_CORE_COMMON_EXPORT
std::vector<kernel_object>
get_kernels(const char* xml_data, size_t xml_size);

/**
 * get_kernels() - Get meta data for all kernels
 *
 * Return: List of struct kernel_object
 */
XRT_CORE_COMMON_EXPORT
std::vector<kernel_object>
get_kernels(const axlf* top);

/**
 * is_aie_only() - check if xclbin passed is aie only xclbin
 */
XRT_CORE_COMMON_EXPORT
bool
is_aie_only(const axlf* top);

/**
 * get_vbnv() - Get VBNV of xclbin
 */
XRT_CORE_COMMON_EXPORT
std::string
get_vbnv(const axlf* top);

/**
 * get_project_name() - Get the project name from the XML
 */
XRT_CORE_COMMON_EXPORT
std::string
get_project_name(const char* xml_data, size_t xml_size);

/**
 * get_project_name() - Get the project name from the XML
 */
XRT_CORE_COMMON_EXPORT
std::string
get_project_name(const axlf* top);

/**
 * get_project_name() - Get the project name from the XML
 */
std::string
get_fpga_device_name(const char* xml_data, size_t xml_size);

}} // xclbin, xrt_core

#endif
