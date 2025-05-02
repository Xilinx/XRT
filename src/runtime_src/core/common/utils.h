/**
 * Copyright (C) 2016-2025 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 * Common XRT SAK Util functions
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef _xrt_core_common_utils_h_
#define _xrt_core_common_utils_h_

#include "config.h"
#include "scope_guard.h"

#include <string>
#include <sstream>
#include <functional>
#include <iostream>
#include <vector>

/* HLS CU bit status */
#define CU_AP_START	(0x1 << 0)
#define CU_AP_DONE	(0x1 << 1)
#define CU_AP_IDLE	(0x1 << 2)
#define CU_AP_READY	(0x1 << 3)
#define CU_AP_CONTINUE	(0x1 << 4)
#define CU_AP_RESET	(0x1 << 5)

namespace xrt_core { namespace utils {

/**
 * ios_flags_restore() - scope guard for ios flags
 */
inline scope_guard<std::function<void()>>
ios_restore(std::ostream& ostr)
{
  auto restore = [](std::ostream& ostr, std::ios_base::fmtflags f) { ostr.flags(f); };
  return {std::bind(restore, std::ref(ostr), ostr.flags())};
}

inline std::string
to_hex(const void* addr)
{
  std::stringstream str;
  str << std::hex << addr;
  return str.str();
}

XRT_CORE_COMMON_EXPORT
std::string
get_hostname();

/**
 * parse_cu_status() -
 */
XRT_CORE_COMMON_EXPORT
std::string
parse_cu_status(unsigned int val);

/**
 * parse_firewall_status() -
 */
XRT_CORE_COMMON_EXPORT
std::string
parse_firewall_status(unsigned int val);

/**
 * parse_firewall_status() -
 */
XRT_CORE_COMMON_EXPORT
std::string
parse_dna_status(unsigned int val);

/**
 * unit_covert() -
 */
XRT_CORE_COMMON_EXPORT
std::string
unit_convert(size_t size);

XRT_CORE_COMMON_EXPORT
std::string
format_base10_shiftdown3(uint64_t value);

XRT_CORE_COMMON_EXPORT
std::string
format_base10_shiftdown6(uint64_t value);

XRT_CORE_COMMON_EXPORT
std::string
format_base10_shiftdown(uint64_t value, int decimal, int digit_precision);

/**
 * bdf2index() - convert bdf to device index
 *
 * @bdf:   BDF string in [domain:]bus:device.function format
 * Return: Corresponding device index
 *
 * Throws std::runtime_error if core library is not loaded.
 */
XRT_CORE_COMMON_EXPORT
uint16_t
bdf2index(const std::string& bdf, bool _inUserDomain);

XRT_CORE_COMMON_EXPORT
uint64_t
issue_id();

XRT_CORE_COMMON_EXPORT
bool
load_host_trace();

XRT_CORE_COMMON_EXPORT
std::string
parse_clock_id(const std::string& id);

/**
 * parse_cmc_status() -
 */
XRT_CORE_COMMON_EXPORT
std::string
parse_cmc_status(unsigned int val);

XRT_CORE_COMMON_EXPORT
uint64_t
mac_addr_to_value(std::string mac_addr);

XRT_CORE_COMMON_EXPORT
std::string
value_to_mac_addr(uint64_t mac_addr_value);

XRT_CORE_COMMON_EXPORT
int
get_pid();

/**
 * @brief Retrieve the last system error message.
 *
 * This function fetches the last error message from the operating system,
 * which could originate from a Linux or Windows system call. It provides
 * a human-readable string describing the error.
 *
 * @return A string containing the last system error message.
 */
XRT_CORE_COMMON_EXPORT
std::string
get_sys_last_err_msg();

}} // utils, xrt_core

#endif
