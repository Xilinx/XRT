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

#ifndef __XBHelpMenus_h_
#define __XBHelpMenus_h_

// Include files
// Please keep these to the bare minimum
#include "SubCmd.h"
#include "Report.h"

#include <string>
#include <vector>
#include <utility> // Pair template
#include <boost/program_options.hpp>

// ----------------------- T Y P E D E F S -----------------------------------
using SubCmdsCollection = std::vector<std::shared_ptr<SubCmd>>;

namespace XBUtilities {
  void 
    report_commands_help( const std::string &_executable, 
                          const std::string &_description,
                          const boost::program_options::options_description& _optionDescription,
                          const SubCmdsCollection &_subCmds );
  void 
    report_subcommand_help( const std::string &_executableName,
                            const std::string &_subCommand,
                            const std::string &_description, 
                            const std::string &_extendedHelp,
                            const boost::program_options::options_description & _optionDescription,
                            const boost::program_options::positional_options_description & _positionalDescription );

  void 
    report_subcommand_help( const std::string &_executableName,
                            const std::string &_subCommand,
                            const std::string &_description, 
                            const std::string &_extendedHelp,
                            const boost::program_options::options_description &_optionDescription,
                            const SubCmd::SubOptionOptions & _subOptionOptions);

  void 
    report_option_help( const std::string & _groupName, 
                        const boost::program_options::options_description& _optionDescription,
                        const boost::program_options::positional_options_description & _positionalDescription,
                        bool _bReportParameter = true);

  std::string 
    create_usage_string( const boost::program_options::options_description &_od,
                         const boost::program_options::positional_options_description & _pod );

  std::string 
    create_suboption_list_string(const ReportCollection &_reportCollection, bool _addAll = false);

  using VectorPairStrings = std::vector< std::pair< std::string, std::string > >;

  std::string 
    create_suboption_list_string(const VectorPairStrings &_collection);

  std::string 
    create_suboption_list_string(const ReportCollection &_reportCollection, bool _addVerboseOption);

  std::string 
    create_suboption_list_string(const Report::SchemaDescriptionVector &_formatCollection);

  void 
    collect_and_validate_reports( const ReportCollection & allReportsAvailable,
                                  const std::vector<std::string> &reportNamesToAdd,
                                  ReportCollection & reportsToUse);

  void 
     produce_reports( xrt_core::device_collection _devices, 
                      const ReportCollection & _reportsToProcess, 
                      Report::SchemaVersion _schema, 
                      std::vector<std::string> & _elementFilter,
                      std::ostream &_ostream);
};

#endif
