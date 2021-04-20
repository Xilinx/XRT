/**
 * Copyright (C) 2020 Xilinx, Inc
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

#ifndef xrt_core_common_query_reset_h
#define xrt_core_common_query_reset_h

#include "query.h"
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <boost/any.hpp>
#include <boost/format.hpp>

namespace xrt_core {

namespace query {

enum class reset_key {
  hot = 1,
  kernel = 2,
  ert = 3,
  ecc = 4,
  soft_kernel = 5,
  aie = 6,
  user = 7
};

class reset_type {
  reset_key mKey; 
  std::string mName;
  std::string mSubdev;
  std::string mEntry;
  std::string mWarning;
  std::string mValue;

public:
  reset_type(reset_key key, std::string name, std::string subdev, std::string entry, std::string warning, std::string value)
    : mKey(key), mName(name), mSubdev(subdev), mEntry(entry), mWarning(warning), mValue(value)
  {
  }

  reset_key get_key() {
    return mKey;
  }

  std::string get_name() {
    return mName;
  }

  std::string get_subdev() {
    return mSubdev;
  }

  std::string get_entry() {
    return mEntry;
  }

  void set_subdev(std::string str) {
    mSubdev = str;
  }

  void set_entry(std::string str) {
    mEntry = str;
  }

  std::string get_warning() {
    return mWarning;
  }

  std::string get_value() {
    return mValue;
  }
};

} // query

} // xrt_core


#endif
