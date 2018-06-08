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

#ifndef xocl_core_platform_h_
#define xocl_core_platform_h_

#include "xocl/core/object.h"
#include "xocl/core/range.h"
#include "xocl/core/refcount.h"
#include <vector>

namespace xocl {

class platform : public _cl_platform_id
{
public:
  using device_vector_type = std::vector<ptr<device>>;
  using device_iterator_type = ptr_iterator<device_vector_type::iterator>;
  using device_const_iterator_type = ptr_iterator<device_vector_type::const_iterator>;

public:
  platform();
  virtual ~platform();

  void 
  add_device(device* d);

  bool 
  has_device(device* d) const 
  { 
    return std::find(m_devices.begin(),m_devices.end(),d)!=m_devices.end();
  }

  range<device_iterator_type>
  get_device_range()
  {
    return range<device_iterator_type>(m_devices.begin(),m_devices.end());
  }

  range<device_const_iterator_type>
  get_device_range() const
  {
    return range<device_const_iterator_type>(m_devices.begin(),m_devices.end());
  }

  /**
   * Share a reference to the global platform object
   *
   * This function is the only way to construct the global platform.
   */
  static std::shared_ptr<platform>
  get_shared_platform();

private:
  unsigned int m_uid = 0;

  device_vector_type m_devices;

  // Manage xrt devices loaded by platform
  class xrt_device_manager;
  std::unique_ptr<xrt_device_manager> m_device_mgr;
};

/**
 * Get all available platforms
 */
std::vector<platform*>
get_platforms();

/**
 * Get a pointer to the global platform without participating
 * in ownership
 *
 * The global platform is constructed if necessary.
 *
 * @return
 *  Pointer to global platform.  The pointer becomes invalid
 *  arbitrarily at program exit.
 */
platform*
get_global_platform();

/**
 * Get a shared ownership of the global platform.
 *
 * The global platform is constructed if necessary.
 *
 * @return
 *  Shared pointer to global platform object. 
 */
std::shared_ptr<platform>
get_shared_platform();

/**
 * Get number of platforms
 * 
 * @return
 *  Number of platform objects.  This is either 1 or 0 
 *  depending on whether or not the global platform has
 *  been constructed
 *  
 */
unsigned int
get_num_platforms();

/**
 * @return
 *   The value of XILINX_OPENCL if set, empty otherwise
 */
std::string
get_xilinx_opencl();

/**
 * @return
 *   The value of XILINX_SDX if set, empty otherwise
 */
std::string
get_xilinx_sdx();

/**
 * @return
 *   The install root (same as xilinx_opencl)
 */
inline std::string
get_install_root() 
{ 
  return get_xilinx_opencl();
}

////////////////////////////////////////////////////////////////
// Conformance
////////////////////////////////////////////////////////////////
std::string
conformance_get_xclbin(const std::string& hash);


} // xocl

#endif


