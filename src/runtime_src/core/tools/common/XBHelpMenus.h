/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

#ifndef __XBHelpMenus_h_
#define __XBHelpMenus_h_

// Include files
// Please keep these to the bare minimum
#include "Report.h"

#include <string>
#include <vector>
#include <utility> // Pair template
#include <boost/program_options.hpp>

namespace XBUtilities {

  using VectorPairStrings = std::vector< std::pair< std::string, std::string > >;

  std::string 
    create_suboption_list_string(const VectorPairStrings &_collection);

  std::string 
    create_suboption_list_string(const ReportCollection &_reportCollection, bool _addAllOption);

  std::string 
    create_suboption_list_string(const Report::SchemaDescriptionVector &_formatCollection);

  void 
    collect_and_validate_reports( const ReportCollection & allReportsAvailable,
                                  const std::vector<std::string> &reportNamesToAdd,
                                  ReportCollection & reportsToUse);

  void 
     produce_reports( xrt_core::device_collection devices, 
                      const ReportCollection & reportsToProcess, 
                      Report::SchemaVersion schema, 
                      std::vector<std::string> & elementFilter,
                      std::ostream & consoleStream,
                      std::ostream & schemaStream);
  Report::NagiosStatus 
     produce_nagios_reports( xrt_core::device_collection devices, 
                      const ReportCollection & reportsToProcess, 
                      Report::SchemaVersion schema, 
                      std::vector<std::string> & elementFilter,
                      std::ostream & consoleStream,
                      std::ostream & schemaStream);
};

#endif
