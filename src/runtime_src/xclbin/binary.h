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

#ifndef runtime_src_xclbin_binary_h_
#define runtime_src_xclbin_binary_h_

#include <string>
#include <vector>
#include <memory>

/**
 * This file contains a class for an xclbin binary.  It captures
 * the binary and exposes an API to access various pieces of the
 * xclbin binary.  The how-to extract specific data from the binary is
 * hidden in the implementation of the binary class.
 */
namespace xclbin {

using data_range = std::pair<const char*, const char*>;

inline bool 
valid_range(const xclbin::data_range& range)
{
  return (range.first!=nullptr && range.second!=nullptr && range.first < range.second);
}

class error : public std::runtime_error
{
 public:
  error(const std::string& what = "")
    : std::runtime_error(what)
  {}
};

/**
 * An xcl binary is managed by the binary class.
 *
 * This class can present any binary xclbin version.  The class API
 * may have functions that are valid for a particular version of 
 * an xclbin only.  If an invalid function is called, it will throw
 * an xclbin::error exception.
 * 
 * Then entire xclbin binary data is copied into this class, any data
 * returned through APIs maybe referencing a range of the data
 * maintained by the class, so the binary object must stay alive while
 * anything is referencing and sharing xclbin data.
 */
class binary
{
public:
  struct impl
  {
    virtual ~impl() {}
    virtual size_t size()                  const { throw error("not implemented"); }
    virtual std::string version()          const { throw error("not implemented"); }
    virtual data_range binary_data()       const { throw error("not implemented"); }
    virtual data_range meta_data()         const { throw error("not implemented"); }
    virtual data_range debug_data()        const { throw error("not implemented"); }
    virtual data_range connectivity_data() const { throw error("not implemented"); }
    virtual data_range mem_topology_data() const { throw error("not implemented"); }
    virtual data_range ip_layout_data()    const { throw error("not implemented"); }
    virtual data_range clk_freq_data()    const { throw error("not implemented"); }
  };

private:

  std::shared_ptr<impl> m_content;

public:
  binary()
    : m_content(nullptr)
  {}

  binary(const binary& rhs) 
    : m_content(rhs.m_content)
  {}

  /**
   * Construct from xclbin in memory.
   *
   * The ownership of the data is transferred to this class object.
   * 
   * @param xb
   *  xclbin file read into memory
   */
  binary(std::vector<char>&& xb);

  binary&
  operator=(const binary& rhs)
  {
    m_content = rhs.m_content;
    return *this;
  }

  /**
   * @return
   *   Size of xclbin binary
   */
  size_t
  size() const { return m_content->size(); }

  std::string
  version() const { return m_content->version(); }

  data_range
  binary_data() const { return m_content->binary_data(); }

  data_range
  meta_data() const { return m_content->meta_data(); }

  data_range
  debug_data() const { return m_content->debug_data(); }

  data_range
  connectivity_data() const { return m_content->connectivity_data(); }

  data_range 
  mem_topology_data() const { return m_content->mem_topology_data(); }

  data_range 
  ip_layout_data() const { return m_content->ip_layout_data(); }

  data_range 
  clk_freq_data() const { return m_content->clk_freq_data(); }
};

} // xclbin




#endif


