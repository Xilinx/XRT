/**
 * Copyright (C) 2016-2018 Xilinx, Inc
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
#include <functional>
#include <iostream>

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

}} // utils, xrt_core

#endif
