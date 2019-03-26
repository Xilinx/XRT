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

#ifndef xrt_uuid_h_
#define xrt_uuid_h_

#include <uuid/uuid.h>

namespace xrt {

/**
 * uuid wrapper to treat uuid_t as a value type
 * supports copying
 */
struct uuid
{
  uuid_t m_uuid;

  uuid()
  {
    uuid_clear(m_uuid);
  }

  uuid(const uuid_t val)
  {
    uuid_copy(m_uuid,val);
  }

  uuid(const uuid& rhs)
  {
    uuid_copy(m_uuid,rhs.m_uuid);
  }

  uuid(uuid&&) = default;
  uuid& operator=(uuid&&) = default;

  uuid& operator=(const uuid& rhs)
  {
    uuid source(rhs);
    std::swap(*this,source);
    return *this;
  }

  const uuid_t& get() const
  {
    return m_uuid;
  }

  std::string
  to_string() const
  {
    char str[40] = {0};
    uuid_unparse_lower(m_uuid,str);
    return str;
  }
};

} // xrt


#endif
