/**
 * Copyright (C) 2016-2017 Xilinx, Inc
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

#include "binary.h"

namespace xclbin {

std::unique_ptr<binary::impl>
create_xclbin0(std::vector<char>&& xb);

std::unique_ptr<binary::impl>
create_xclbin2(std::vector<char>&& xb);

binary::
binary(std::vector<char>&& xb)
  : m_content(nullptr)
{
  if (xb.size()<8)
    throw error("bad binary");

  const char* raw = &xb[0];

  // magic version
  std::string v(raw,raw+7);
  if (v=="xclbin2")
    m_content = create_xclbin2(std::move(xb));
  else 
    throw error("bad binary version '" + v + "'");
}

}


