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

#ifndef XRT_USER_COMMON_UTILS_H_
#define XRT_USER_COMMON_UTILS_H_

#include <string>
#include <iostream>

std::string parseCUStatus(unsigned int val);
std::string parseFirewallStatus(unsigned int val);
std::string parseDNAStatus(unsigned int val);
std::string unitConvert(size_t size);

namespace xrt_core {
    class ios_flags_restore {
    public:
        ios_flags_restore(std::ostream& _ios): ios(_ios), f(_ios.flags()) { }

        ~ios_flags_restore() { ios.flags(f); }

    private:
        std::ostream& ios;
        std::ios_base::fmtflags f;
    };
} //xrt_core

#endif /* _COMMON_UTILS_H_ */


