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

#ifndef POWER_WRITER_DOT_H
#define POWER_WRITER_DOT_H

#include <string>

#include "xdp/profile/writer/vp_base/vp_writer.h"

namespace xdp {

  class PowerProfilingWriter : public VPWriter
  {
  private:
    std::string deviceName ;
    uint64_t deviceIndex ;
  public:
    PowerProfilingWriter(const char* filename, const char* d, uint64_t index) ;
    ~PowerProfilingWriter() ;

    virtual bool write(bool openNewFile) ;
  } ;

} // end namespace xdp

#endif
