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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "XBHelpMenusCore.h"
#include "XBUtilitiesCore.h"

namespace XBU = XBUtilities;


// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <algorithm>
#include <numeric>

// ------ N A M E S P A C E ---------------------------------------------------
using namespace XBUtilities;

// Temporary color objects until the supporting color library becomes available
namespace ec {
  class fgcolor
    {
    public:
      fgcolor(uint8_t _color) : m_color(_color) {};
      std::string string() const { return "\033[38;5;" + std::to_string(m_color) + "m"; }
      static const std::string reset() { return "\033[39m"; };
      friend std::ostream& operator <<(std::ostream& os, const fgcolor & _obj) { return os << _obj.string(); }
  
   private:
     uint8_t m_color;
  };

  class bgcolor
    {
    public:
      bgcolor(uint8_t _color) : m_color(_color) {};
      std::string string() const { return "\033[48;5;" + std::to_string(m_color) + "m"; }
      static const std::string reset() { return "\033[49m"; };
      friend std::ostream& operator <<(std::ostream& os, const bgcolor & _obj) { return  os << _obj.string(); }

   private:
     uint8_t m_color;
  };
}
// ------ C O L O R S ---------------------------------------------------------
static const uint8_t FGC_HEADER           = 3;   // 3
static const uint8_t FGC_HEADER_BODY      = 111; // 111
                                                  
static const uint8_t FGC_USAGE_BODY       = 252; // 252
                                                  
static const uint8_t FGC_OPTION           = 65;  // 65 
static const uint8_t FGC_OPTION_BODY      = 111; // 111
                                                  
static const uint8_t FGC_SUBCMD           = 140; // 140
static const uint8_t FGC_SUBCMD_BODY      = 111; // 111
                                                  
static const uint8_t FGC_POSITIONAL       = 140; // 140
static const uint8_t FGC_POSITIONAL_BODY  = 111; // 111
                                                  
static const uint8_t FGC_OOPTION          = 65;  // 65
static const uint8_t FGC_OOPTION_BODY     = 70;  // 70
                                                  
static const uint8_t FGC_EXTENDED_BODY    = 70;  // 70


// ------ S T A T I C   V A R I A B L E S -------------------------------------
static unsigned int m_maxColumnWidth = 100;

// ------ F U N C T I O N S ---------------------------------------------------
static bool
isPositional(const std::string &_name, 
             const boost::program_options::positional_options_description & _pod)
{
  // Look through the list of positional arguments
  for (unsigned int index = 0; index < _pod.max_total_count(); ++index) {
    if ( _name.compare(_pod.name_for_position(index)) == 0) {
      return true;
    }
  }
  return false;
}


std::string 
XBUtilities::create_usage_string( const boost::program_options::options_description &_od,
                                  const boost::program_options::positional_options_description & _pod,
                                  bool removeLongOptDashes)
{
  const static int SHORT_OPTION_STRING_SIZE = 2;
  std::stringstream buffer;

  auto &options = _od.options();

  // Gather up the short simple flags
  {
    bool firstShortFlagFound = false;
    for (auto & option : options) {
      // Get the option name
      std::string optionDisplayName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);

      // See if we have a long flag
      if (optionDisplayName.size() != SHORT_OPTION_STRING_SIZE)
        continue;

      // We are not interested in any arguments
      if (option->semantic()->max_tokens() > 0)
        continue;

      // This option shouldn't be required
      if (option->semantic()->is_required() == true) 
        continue;

      if (!firstShortFlagFound) {
        buffer << " [-";
        firstShortFlagFound = true;
      }

      buffer << optionDisplayName[1];
    }

    if (firstShortFlagFound == true) 
      buffer << "]";
  }

   
  // Gather up the long simple flags (flags with no short versions)
  {
    for (auto & option : options) {
      // Get the option name
      std::string optionDisplayName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);

      // See if we have a short flag
      if (optionDisplayName.size() == SHORT_OPTION_STRING_SIZE)
        continue;

      // We are not interested in any arguments
      if (option->semantic()->max_tokens() > 0)
        continue;

      // This option shouldn't be required
      if (option->semantic()->is_required() == true) 
        continue;

      
      const std::string completeOptionName = removeLongOptDashes ? option->long_name() : 
				option->canonical_display_name(po::command_line_style::allow_long);
      buffer << boost::format(" [%s]") % completeOptionName;
    }
  }

  // Gather up the options with arguments
  for (auto & option : options) {
    // Skip if there are no arguments
    if (option->semantic()->max_tokens() == 0)
      continue;

    // This option shouldn't be required
    if (option->semantic()->is_required()) 
      continue;

    std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);
    
    // See if we have a long flag
    if (completeOptionName.size() != SHORT_OPTION_STRING_SIZE)
      continue;

    buffer << boost::format(" [%s arg]") % completeOptionName;
  }

  // Gather up the options with arguments (options with no short versions)
  for (auto & option : options) {
    // Skip if there are no arguments
    if (option->semantic()->max_tokens() == 0)
      continue;

    // This option shouldn't be required
    if (option->semantic()->is_required()) 
      continue;

    const std::string optionDisplayName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);

    // See if we have a short flag
    if (optionDisplayName.size() == SHORT_OPTION_STRING_SIZE)
      continue;

    const std::string completeOptionName = removeLongOptDashes ? option->long_name() : 
      option->canonical_display_name(po::command_line_style::allow_long);
      buffer << boost::format(" [%s arg]") % completeOptionName;
  }

  // Gather up the required options with arguments
  for (auto & option : options) {
    // Skip if there are no arguments
    if (option->semantic()->max_tokens() == 0)
      continue;

    // This option is required
    if (option->semantic()->is_required() == false) 
      continue;

    std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);

    // We don't wish to have positional options
    if ( ::isPositional(completeOptionName, _pod) ) {
      continue;
    }

    buffer << boost::format(" %s arg") % completeOptionName;
  }

  // Report the positional arguments
  for (auto & option : options) {
    std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);
    if ( ! ::isPositional(completeOptionName, _pod) ) {
      continue;
    }

    buffer << " " << completeOptionName;
  }
  

  return buffer.str();
}

void 
XBUtilities::report_commands_help( const std::string &_executable, 
                                   const std::string &_description,
                                   const boost::program_options::options_description& _optionDescription,
                                   const boost::program_options::options_description& _optionHidden,
                                   const SubCmdsCollection &_subCmds)
{ 
  // Formatting color parameters
  // Color references: https://en.wikipedia.org/wiki/ANSI_escape_code
  const std::string fgc_header     = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_HEADER).string();
  const std::string fgc_headerBody = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_HEADER_BODY).string();
  const std::string fgc_usageBody  = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_USAGE_BODY).string();
  const std::string fgc_subCmd     = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_SUBCMD).string();
  const std::string fgc_subCmdBody = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_SUBCMD_BODY).string();
  const std::string fgc_reset      = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor::reset();

  // Helper variable
  static std::string sHidden = "(Hidden)";

  // -- Command description
  {
    static const std::string key = "DESCRIPTION: ";
    auto formattedString = XBU::wrap_paragraphs(_description, static_cast<unsigned int>(key.size()), m_maxColumnWidth - static_cast<unsigned int>(key.size()), false);
    boost::format fmtHeader(fgc_header + "\n" + key + fgc_headerBody + "%s\n" + fgc_reset);
    if ( !formattedString.empty() )
      std::cout << fmtHeader % formattedString;
  }

  // -- Command usage
  boost::program_options::positional_options_description emptyPOD;
  std::string usage = XBU::create_usage_string(_optionDescription, emptyPOD);
  usage += " [command [commandArgs]]";
  boost::format fmtUsage(fgc_header + "\nUSAGE: " + fgc_usageBody + "%s%s\n" + fgc_reset);
  std::cout << fmtUsage % _executable % usage;

  // -- Sort the SubCommands
  SubCmdsCollection subCmdsReleased;
  SubCmdsCollection subCmdsDepricated;
  SubCmdsCollection subCmdsPreliminary;

  for (auto& subCmdEntry : _subCmds) {
    // Filter out hidden subcommand
    if (!XBU::getShowHidden() && subCmdEntry->isHidden()) 
      continue;

    // Depricated sub-command
    if (subCmdEntry->isDeprecated()) {
      subCmdsDepricated.push_back(subCmdEntry);
      continue;
    }

    // Preliminary sub-command
    if (subCmdEntry->isPreliminary()) {
      subCmdsPreliminary.push_back(subCmdEntry);
      continue;
    }

    // Released sub-command
    subCmdsReleased.push_back(subCmdEntry);
  }

  // Sort the collections by name
  auto sortByName = [](const auto& d1, const auto& d2) { return d1->getName() < d2->getName(); };
  std::sort(subCmdsReleased.begin(), subCmdsReleased.end(), sortByName);
  std::sort(subCmdsPreliminary.begin(), subCmdsPreliminary.end(), sortByName);
  std::sort(subCmdsDepricated.begin(), subCmdsDepricated.end(), sortByName);


  // -- Report the SubCommands
  boost::format fmtSubCmdHdr(fgc_header + "\n%s COMMANDS:\n" + fgc_reset);  
  boost::format fmtSubCmd(fgc_subCmd + "  %-10s " + fgc_subCmdBody + "- %s\n" + fgc_reset); 
  unsigned int subCmdDescTab = 15;

  if (!subCmdsReleased.empty()) {
    std::cout << fmtSubCmdHdr % "AVAILABLE";
    for (auto & subCmdEntry : subCmdsReleased) {
      std::string sPreAppend = subCmdEntry->isHidden() ? sHidden + " " : "";
      auto formattedString = XBU::wrap_paragraphs(sPreAppend + subCmdEntry->getShortDescription(), subCmdDescTab, m_maxColumnWidth, false);
      std::cout << fmtSubCmd % subCmdEntry->getName() % formattedString;
    }
  }

  if (!subCmdsPreliminary.empty()) {
    std::cout << fmtSubCmdHdr % "PRELIMINARY";
    for (auto & subCmdEntry : subCmdsPreliminary) {
      std::string sPreAppend = subCmdEntry->isHidden() ? sHidden + " " : "";
      auto formattedString = XBU::wrap_paragraphs(sPreAppend + subCmdEntry->getShortDescription(), subCmdDescTab, m_maxColumnWidth, false);
      std::cout << fmtSubCmd % subCmdEntry->getName() % formattedString;
    }
  }

  if (!subCmdsDepricated.empty()) {
    std::cout << fmtSubCmdHdr % "DEPRECATED";
    for (auto & subCmdEntry : subCmdsDepricated) {
      std::string sPreAppend = subCmdEntry->isHidden() ? sHidden + " " : "";
      auto formattedString = XBU::wrap_paragraphs(sPreAppend + subCmdEntry->getShortDescription(), subCmdDescTab, m_maxColumnWidth, false);
      std::cout << fmtSubCmd % subCmdEntry->getName() % formattedString;
    }
  }

  report_option_help("OPTIONS", _optionDescription, emptyPOD);

  if (XBU::getShowHidden()) 
    report_option_help(std::string("OPTIONS ") + sHidden, _optionHidden, emptyPOD);
}

static std::string 
create_option_format_name(const boost::program_options::option_description * _option,
                          bool _reportParameter = true,
                          bool removeLongOptDashes = false)
{
  if (_option == nullptr) 
    return "";

  std::string optionDisplayName = _option->canonical_display_name(po::command_line_style::allow_dash_for_short);

  // Determine if we really got the "short" name (might not exist and a long was returned instead)
  if (!optionDisplayName.empty() && optionDisplayName[0] != '-')
    optionDisplayName.clear();

  // Get the long name (if it exists)
  std::string longName = _option->canonical_display_name(po::command_line_style::allow_long);
  if ((longName.size() > 2) && (longName[0] == '-') && (longName[1] == '-')) {
    if (!optionDisplayName.empty()) 
      optionDisplayName += ", ";

    optionDisplayName += removeLongOptDashes ? _option->long_name() : longName;
  }

  if (_reportParameter && !_option->format_parameter().empty()) 
    optionDisplayName += " " + _option->format_parameter();

  return optionDisplayName;
}

void
XBUtilities::report_option_help( const std::string & _groupName, 
                                 const boost::program_options::options_description& _optionDescription,
                                 const boost::program_options::positional_options_description & _positionalDescription,
                                 bool _bReportParameter,
                                 bool removeLongOptDashes)
{
  // Formatting color parameters
  // Color references: https://en.wikipedia.org/wiki/ANSI_escape_code
  const std::string fgc_header     = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_HEADER).string();
  const std::string fgc_optionName = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_OPTION).string();
  const std::string fgc_optionBody = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_OPTION_BODY).string();
  const std::string fgc_reset      = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor::reset();

  // Determine if there is anything to report
  if (_optionDescription.options().empty())
    return;

  // Report option group name (if defined)
  boost::format fmtHeader(fgc_header + "\n%s:\n" + fgc_reset);
  if ( !_groupName.empty() )
    std::cout << fmtHeader % _groupName;

  // Report the options
  boost::format fmtOption(fgc_optionName + "  %-18s " + fgc_optionBody + "- %s\n" + fgc_reset);
  for (auto & option : _optionDescription.options()) {
    if ( ::isPositional( option->canonical_display_name(po::command_line_style::allow_dash_for_short),
                         _positionalDescription) )  {
      continue;
    }

    std::string optionDisplayFormat = create_option_format_name(option.get(), _bReportParameter, removeLongOptDashes);
    unsigned int optionDescTab = 23;
    auto formattedString = XBU::wrap_paragraphs(option->description(), optionDescTab, m_maxColumnWidth - optionDescTab, false);
    std::cout << fmtOption % optionDisplayFormat % formattedString;
  }
}

void 
XBUtilities::report_subcommand_help( const std::string &_executableName,
                                     const std::string &_subCommand,
                                     const std::string &_description, 
                                     const std::string &_extendedHelp,
                                     const boost::program_options::options_description &_optionDescription,
                                     const boost::program_options::options_description &_optionHidden,
                                     const boost::program_options::positional_options_description & _positionalDescription,
                                     const boost::program_options::options_description &_globalOptions,
                                     bool removeLongOptDashes,
                                     const std::string& customHelpSection)
{
  // Formatting color parameters
  // Color references: https://en.wikipedia.org/wiki/ANSI_escape_code
  const std::string fgc_header      = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_HEADER).string();
  const std::string fgc_headerBody  = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_HEADER_BODY).string();
  const std::string fgc_poption      = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_POSITIONAL).string();
  const std::string fgc_poptionBody  = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_POSITIONAL_BODY).string();
  const std::string fgc_usageBody   = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_USAGE_BODY).string();
  const std::string fgc_extendedBody = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_EXTENDED_BODY).string();
  const std::string fgc_reset       = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor::reset();

  // -- Command description
  {
    static const std::string key = "DESCRIPTION: ";
    auto formattedString = XBU::wrap_paragraphs(_description, static_cast<unsigned int>(key.size()), m_maxColumnWidth - static_cast<unsigned int>(key.size()), false);
    boost::format fmtHeader(fgc_header + "\n" + key + fgc_headerBody + "%s\n" + fgc_reset);
    if ( !formattedString.empty() )
      std::cout << fmtHeader % formattedString;
  }

  // -- Command usage
  const std::string usage = XBU::create_usage_string(_optionDescription, _positionalDescription, removeLongOptDashes);
  boost::format fmtUsage(fgc_header + "\nUSAGE: " + fgc_usageBody + "%s %s%s\n" + fgc_reset);
  std::cout << fmtUsage % _executableName % _subCommand % usage;
  
  // -- Add positional arguments
  boost::format fmtOOSubPositional(fgc_poption + "  %-15s" + fgc_poptionBody + " - %s\n" + fgc_reset);
  for (auto option : _optionDescription.options()) {
    if ( !::isPositional( option->canonical_display_name(po::command_line_style::allow_dash_for_short),
                          _positionalDescription))  {
      continue;
    }

    std::string optionDisplayFormat = create_option_format_name(option.get(), false);
    unsigned int optionDescTab = 33;
    auto formattedString = XBU::wrap_paragraphs(option->description(), optionDescTab, m_maxColumnWidth, false);

    std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);
    std::cout << fmtOOSubPositional % ("<" + option->long_name() + ">") % formattedString;
  }


  // -- Options
  report_option_help("OPTIONS", _optionDescription, _positionalDescription, false, removeLongOptDashes);

  // -- Custom Section
  std::cout << customHelpSection << "\n";

  // -- Global Options
  report_option_help("GLOBAL OPTIONS", _globalOptions, _positionalDescription, false);

  if (XBU::getShowHidden()) 
    report_option_help("OPTIONS (Hidden)", _optionHidden, _positionalDescription, false);

  // Extended help
  {
    boost::format fmtExtHelp(fgc_extendedBody + "\n  %s\n" +fgc_reset);
    auto formattedString = XBU::wrap_paragraphs(_extendedHelp, 2, m_maxColumnWidth, false);
    if (!formattedString.empty()) 
      std::cout << fmtExtHelp % formattedString;
  }
}

void 
XBUtilities::report_subcommand_help( const std::string &_executableName,
                                     const std::string &_subCommand,
                                     const std::string &_description, 
                                     const std::string &_extendedHelp,
                                     const boost::program_options::options_description &_optionDescription,
                                     const boost::program_options::options_description &_optionHidden,
                                     const SubCmd::SubOptionOptions & _subOptionOptions,
                                     const boost::program_options::options_description &_globalOptions)
{
  // Formatting color parameters
  // Color references: https://en.wikipedia.org/wiki/ANSI_escape_code
  const std::string fgc_header       = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_HEADER).string();
  const std::string fgc_headerBody   = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_HEADER_BODY).string();
  const std::string fgc_commandBody  = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_SUBCMD).string();
  const std::string fgc_usageBody    = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_USAGE_BODY).string();

  const std::string fgc_ooption      = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_OOPTION).string();
  const std::string fgc_ooptionBody  = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_OOPTION_BODY).string();
  const std::string fgc_poption      = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_POSITIONAL).string();
  const std::string fgc_poptionBody  = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_POSITIONAL_BODY).string();
  const std::string fgc_extendedBody = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_EXTENDED_BODY).string();
  const std::string fgc_reset        = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor::reset();

  // -- Command
  boost::format fmtCommand(fgc_header + "\nCOMMAND: " + fgc_commandBody + "%s\n" + fgc_reset);
  if ( !_subCommand.empty() )
    std::cout << fmtCommand % _subCommand;
 
  // -- Command description
  {
    auto formattedString = XBU::wrap_paragraphs(_description, 15, m_maxColumnWidth, false);
    boost::format fmtHeader(fgc_header + "\nDESCRIPTION: " + fgc_headerBody + "%s\n" + fgc_reset);
    if ( !formattedString.empty() )
      std::cout << fmtHeader % formattedString;
  }

  // -- Usage
  std::string usageSubCmds;
  for (const auto & subCmd : _subOptionOptions) {
    if (subCmd->isHidden()) 
      continue;

    if (!usageSubCmds.empty()) 
      usageSubCmds.append(" | ");

    usageSubCmds.append(subCmd->longName());
  }

  std::cout << boost::format(fgc_header + "\nUSAGE: " + fgc_usageBody + "%s %s [-h] --[ %s ] [commandArgs]\n" + fgc_reset) % _executableName % _subCommand % usageSubCmds;

  // -- Options
  boost::program_options::positional_options_description emptyPOD;
  report_option_help("OPTIONS", _optionDescription, emptyPOD, false);

  // -- Global Options
  report_option_help("GLOBAL OPTIONS", _globalOptions, emptyPOD, false);

  if (XBU::getShowHidden()) 
    report_option_help("OPTIONS (Hidden)", _optionHidden, emptyPOD, false);

  // Extended help
  {
    boost::format fmtExtHelp(fgc_extendedBody + "\n  %s\n" +fgc_reset);
    auto formattedString = XBU::wrap_paragraphs(_extendedHelp, 2, m_maxColumnWidth, false);
    if (!formattedString.empty()) 
      std::cout << fmtExtHelp % formattedString;
  }
}
