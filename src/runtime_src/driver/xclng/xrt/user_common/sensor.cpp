/**
 * Copyright (C) 2018 Xilinx, Inc
 * Author: Ryan Radjabi
 * Wrapper around boost::property_tree::ptree for storing data that can
 * be accessed easily and exported to formats like JSON and XML.
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
#include "sensor.h"
#include <boost/property_tree/json_parser.hpp>

namespace sensor_tree {

// Singleton
boost::property_tree::ptree&
instance()
{
  static boost::property_tree::ptree s_ptree;
  return s_ptree;
}

void
json_dump(std::ostream &ostr)
{
  boost::property_tree::json_parser::write_json( ostr, instance() );
}

} // namespace sensor_tree
