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
#include <string>
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

   void 
     disable_escape_codes( bool _disable );
};

#endif

