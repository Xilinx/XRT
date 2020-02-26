/**
 * Copyright (C) 2016-2020 Xilinx, Inc
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

#ifndef VP_SUMMARY_WRITER_DOT_H
#define VP_SUMMARY_WRITER_DOT_H

#include "xdp/profile/writer/vp_base/vp_writer.h"

#include "xdp/config.h"

namespace xdp {

  class VPSummaryWriter : public VPWriter
  {
  private:
    VPSummaryWriter() = delete ;

  protected:
    XDP_EXPORT virtual void switchFiles() ;
  public:
    XDP_EXPORT VPSummaryWriter(const char* filename) ;
    XDP_EXPORT ~VPSummaryWriter() ;
  } ;
  
}

#endif
