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

#define XDP_SOURCE

#include <boost/property_tree/json_parser.hpp>

#include "aie_config_metadata.h"

namespace xdp {

  AieConfigMetadata::AieConfigMetadata()
  {
#ifdef XDP_MINIMAL_BUILD
    boost::property_tree::read_json("aie_control_config.json", aieMetadata);
#endif
  }

  boost::property_tree::ptree AieConfigMetadata::getAieConfigMetadata(std::string configName) {
    std::string query = "aie_metadata.driver_config." + configName;
    return aieMetadata.get_child(query);
  }

}

