// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.

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
     produce_reports( const std::shared_ptr<xrt_core::device>& device, 
                      const ReportCollection & reportsToProcess, 
                      const Report::SchemaVersion schema, 
                      const std::vector<std::string> & elementFilter,
                      std::ostream & consoleStream,
                      std::ostream & schemaStream);
};

#endif
