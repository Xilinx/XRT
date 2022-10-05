/**
 * Copyright (C) 2022 Xilinx, Inc
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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "SubCmdJSON.h"
#include "tools/common/XBUtilitiesCore.h"
#include "tools/common/XBUtilities.h"
namespace XBU = XBUtilities;

#include "core/common/device.h"
#include "core/common/error.h"
#include "core/common/query_requests.h"
#include "core/common/system.h"
#include "xrt.h"


// 3rd Party Library - Include Files
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace po = boost::program_options;
namespace pt = boost::property_tree;

// System - Include Files
#include <cstdlib>
#include <fstream>
#include <iostream>

#ifdef _WIN32
# pragma warning( disable : 4996 )
#endif

// ----- C L A S S   M E T H O D S -------------------------------------------

SubCmdJSON::SubCmdJSON(bool _isHidden, bool _isDepricated, bool _isPreliminary, std::string& name, std::string& desc, std::vector<struct JSONCmd>& _subCmdOptions)
    : SubCmd(name, desc)
    , m_subCmdOptions(_subCmdOptions)
    , m_help(false)
{
  setLongDescription(desc);
  setExampleSyntax("");
  setIsHidden(_isHidden);
  setIsDeprecated(_isDepricated);
  setIsPreliminary(_isPreliminary);
  setIsDefaultDevValid(false);

  m_commonOptions.add_options()
     ("help", boost::program_options::bool_switch(&m_help), "Help to use this sub-command")
  ;

  for( auto &opt : m_subCmdOptions) {
    m_commonOptions.add_options()
      (opt.option.c_str(), opt.description.c_str())
    ;
  }

  m_hiddenOptions.add_options()
    ("subCmd", po::value<std::string>(), "Command to execute")
    ("subCmdArgs", po::value<std::vector<std::string> >(), "Arguments for command")
  ;

  m_positionals.
    add("subCmd", 1 /* max_count */).
    add("subCmdArgs", -1 /* Unlimited max_count */);
}

void
SubCmdJSON::execute(const SubCmdOptions& _options) const
{
  XBU::verbose("SubCommand: " + getName());

  po::variables_map vm;
  auto top_options = process_arguments(vm, _options);

  // Check to see if no command was found
  if ((vm.count("subCmd") == 0)) {
    printHelp();
    return;
  }

  // -- Now process the subcommand --------------------------------------------
  std::string sCommand = vm["subCmd"].as<std::string>();

  for ( auto &jsonCmd : m_subCmdOptions ) {
    if (sCommand.compare(jsonCmd.option) == 0) {
      top_options.erase(top_options.begin());

      if(m_help) {
        top_options.push_back("--help");
      }

      std::string finalCmd = jsonCmd.application + " " + jsonCmd.defaultArgs;
      for (auto &opt : top_options) {
        finalCmd += " ";
        finalCmd += opt;
      }

      std::cout << "\nInvoking application : " << jsonCmd.application << std::endl;
      std::cout << "\ncommand : " << finalCmd << "\n\n";

      int status = system(finalCmd.c_str());
      if(status)
        std::cout << "\nERROR: Failed to run the command - " << finalCmd << std::endl;

      return;
    }
  }

  std::cout << "\nERROR: Missing valid program operation. No action taken.\n\n";
  printHelp();
  throw xrt_core::error(std::errc::operation_canceled);
}

// ----- H E L P E R   F U N C T I O N S -------------------------------------------
static void collectJsonPaths(std::vector<std::string> &pathVec, std::string env)
{
    char del = ':';
    size_t start = 0;
    size_t end = env.find(del);
    while (end != std::string::npos) {
        pathVec.emplace_back(env.substr(start, end - start));
        start = end + 1;
        end = env.find(del, start);
    }
    pathVec.emplace_back(env.substr(start, env.length() - start));
}

/*
 * This function parses JSON file whose path is set using 'XRT_SUBCOMMANDS_JSON'
 * env variable and adds valid commands to command list.
 * Sample JSON file can be found at -
 * src\runtime_src\core\tools\common\xrt_subcommands.json
 *
 * Executable under which subcommands are populated acts just like a wrapper and
 * underlying application is invoked with respective command line options passed
 * as arguments, 'application' is one of the node entry for each sub command option.
 */
static void populateSubCommandsFromJSONHelper(SubCmdsCollection &subCmds, const std::string& jsonPath, const std::string& exeName)
{
    // parse JSON and add valid Sub Commands
    pt::ptree jtree;
    try {
        pt::read_json(jsonPath,jtree);

	pt::ptree exetree;
	// check exsistence of tree node for execuatble passed(eg: xbutil)
	try {
            exetree = jtree.get_child(exeName);
	} catch (std::exception &e) {
            throw std::runtime_error("Error: No JSON branch for executable '" + exeName + "'\n" + e.what());
	}

	// iterate over various sub commands
        for (pt::ptree::value_type &JSONsubCmd : exetree.get_child("sub_commands"))
        {
            std::string subCmdName = JSONsubCmd.first;
            std::string subCmdDesc = JSONsubCmd.second.get<std::string>("description");

            std::vector<struct JSONCmd> subCmdOpts;
	    // collect all the valid options of sub command
            for(pt::ptree::value_type &subCmdOpt : JSONsubCmd.second.get_child("options"))
            {
                struct JSONCmd cmd;
                cmd.parentName = subCmdName;
                cmd.description = subCmdOpt.second.get<std::string>("description");
                cmd.application = subCmdOpt.second.get<std::string>("application");
                cmd.defaultArgs = subCmdOpt.second.get<std::string>("default_args");
                cmd.option = subCmdOpt.second.get<std::string>("option");
                subCmdOpts.emplace_back(cmd);
            }
            subCmds.emplace_back(std::make_shared<  SubCmdJSON  >(false, false, false, subCmdName, subCmdDesc, subCmdOpts));
        }
    }
    catch(std::exception &e) {
        // Display message only when verbosity is enabled, exit silently otherwise
        XBU::verbose("Exception occured while parsing " + jsonPath + "JSON file : " + e.what());
    }
}

void populateSubCommandsFromJSON(SubCmdsCollection &subCmds, const std::string& exeName)
{
    auto envJson = std::getenv("XRT_SUBCOMMANDS_JSON");
    if(!envJson)
        return;

    // multiple json file paths may be appended to env variable
    std::vector<std::string> jsonPaths;
    collectJsonPaths(jsonPaths, envJson);

    for(auto &path : jsonPaths)
    {
        if(boost::filesystem::is_regular_file(path))
            populateSubCommandsFromJSONHelper(subCmds, path, exeName);
    }
}
