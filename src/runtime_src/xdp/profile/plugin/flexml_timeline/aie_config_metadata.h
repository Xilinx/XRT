/**
 * Copyright (C) 2023 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef XDP_FLEXML_TIMELINE_AIE_CONFIG_METADATA_H
#define XDP_FLEXML_TIMELINE_AIE_CONFIG_METADATA_H

#include <boost/property_tree/ptree.hpp>

#include "core/include/xrt/xrt_hw_context.h"
#include "xdp/config.h"

namespace xdp {


  class AieConfigMetadata {
    boost::property_tree::ptree aieMetadata;
    xrt::hw_context hwContext;

    public:
      AieConfigMetadata();
      ~AieConfigMetadata() {}

      boost::property_tree::ptree getAieConfigMetadata(std::string);
      void setHwContext(xrt::hw_context ctx)
      {
        hwContext = std::move(ctx);
      }
      xrt::hw_context getHwContext()
      {
        return hwContext;
      }
  };

}

#endif
 
