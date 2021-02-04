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

// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "XBHelpMenus.h"
#include "XBUtilities.h"
#include "core/common/time.h"
#include "core/common/query_requests.h"

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
static unsigned int m_maxColumnWidth = 90;
static unsigned int m_shortDescriptionColumn = 24;


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
                                  const boost::program_options::positional_options_description & _pod)
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

      std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_long);
      buffer << " [" << completeOptionName << "]";
    }
  }

  // Gather up the options with arguments
  for (auto & option : options) {
    // Skip if there are no arguments
    if (option->semantic()->max_tokens() == 0)
      continue;

    // This option shouldn't be required
    if (option->semantic()->is_required() == true) 
      continue;

    std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);

    buffer << " [" << completeOptionName << " arg]";
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

    buffer << " " << completeOptionName << " arg";
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
  const std::string fgc_header     = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_HEADER).string();
  const std::string fgc_headerBody = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_HEADER_BODY).string();
  const std::string fgc_usageBody  = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_USAGE_BODY).string();
  const std::string fgc_subCmd     = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_SUBCMD).string();
  const std::string fgc_subCmdBody = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_SUBCMD_BODY).string();
  const std::string fgc_reset      = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor::reset();

  // Helper variable
  std::string formattedString;
  static std::string sHidden = "(Hidden)";

  // -- Command description
  XBU::wrap_paragraphs(_description, 13, m_maxColumnWidth, false, formattedString);
  boost::format fmtHeader(fgc_header + "\nDESCRIPTION: " + fgc_headerBody + "%s\n" + fgc_reset);
  if ( !formattedString.empty() )
    std::cout << fmtHeader % formattedString;

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
      XBU::wrap_paragraphs(sPreAppend + subCmdEntry->getShortDescription(), subCmdDescTab, m_maxColumnWidth, false, formattedString);
      std::cout << fmtSubCmd % subCmdEntry->getName() % formattedString;
    }
  }

  if (!subCmdsPreliminary.empty()) {
    std::cout << fmtSubCmdHdr % "PRELIMINARY";
    for (auto & subCmdEntry : subCmdsPreliminary) {
      std::string sPreAppend = subCmdEntry->isHidden() ? sHidden + " " : "";
      XBU::wrap_paragraphs(sPreAppend + subCmdEntry->getShortDescription(), subCmdDescTab, m_maxColumnWidth, false, formattedString);
      std::cout << fmtSubCmd % subCmdEntry->getName() % formattedString;
    }
  }

  if (!subCmdsDepricated.empty()) {
    std::cout << fmtSubCmdHdr % "DEPRECATED";
    for (auto & subCmdEntry : subCmdsDepricated) {
      std::string sPreAppend = subCmdEntry->isHidden() ? sHidden + " " : "";
      XBU::wrap_paragraphs(sPreAppend + subCmdEntry->getShortDescription(), subCmdDescTab, m_maxColumnWidth, false, formattedString);
      std::cout << fmtSubCmd % subCmdEntry->getName() % formattedString;
    }
  }

  report_option_help("OPTIONS", _optionDescription, emptyPOD);

  if (XBU::getShowHidden()) 
    report_option_help(std::string("OPTIONS ") + sHidden, _optionHidden, emptyPOD);
}

static std::string 
create_option_format_name(const boost::program_options::option_description * _option,
                          bool _reportParameter = true)
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
    optionDisplayName += longName;
  }

  if (_reportParameter && !_option->format_parameter().empty()) 
    optionDisplayName += " " + _option->format_parameter();

  return optionDisplayName;
}

void
XBUtilities::report_option_help( const std::string & _groupName, 
                                 const boost::program_options::options_description& _optionDescription,
                                 const boost::program_options::positional_options_description & _positionalDescription,
                                 bool _bReportParameter)
{
  // Formatting color parameters
  // Color references: https://en.wikipedia.org/wiki/ANSI_escape_code
  const std::string fgc_header     = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_HEADER).string();
  const std::string fgc_optionName = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_OPTION).string();
  const std::string fgc_optionBody = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_OPTION_BODY).string();
  const std::string fgc_reset      = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor::reset();

  // Determine if there is anything to report
  if (_optionDescription.options().empty())
    return;

  // Report option group name (if defined)
  boost::format fmtHeader(fgc_header + "\n%s:\n" + fgc_reset);
  if ( !_groupName.empty() )
    std::cout << fmtHeader % _groupName;

  // Helper string
  std::string formattedString;

  // Report the options
  boost::format fmtOption(fgc_optionName + "  %-18s " + fgc_optionBody + "- %s\n" + fgc_reset);
  for (auto & option : _optionDescription.options()) {
    if ( ::isPositional( option->canonical_display_name(po::command_line_style::allow_dash_for_short),
                         _positionalDescription) )  {
      continue;
    }

    std::string optionDisplayFormat = create_option_format_name(option.get(), _bReportParameter);
    unsigned int optionDescTab = 23;
    XBU::wrap_paragraphs(option->description(), optionDescTab, m_maxColumnWidth, false, formattedString);
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
                                     const boost::program_options::positional_options_description & _positionalDescription)
{
  // Formatting color parameters
  // Color references: https://en.wikipedia.org/wiki/ANSI_escape_code
  const std::string fgc_header      = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_HEADER).string();
  const std::string fgc_headerBody  = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_HEADER_BODY).string();
  const std::string fgc_poption      = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_POSITIONAL).string();
  const std::string fgc_poptionBody  = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_POSITIONAL_BODY).string();
  const std::string fgc_usageBody   = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_USAGE_BODY).string();
  const std::string fgc_extendedBody = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_EXTENDED_BODY).string();
  const std::string fgc_reset       = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor::reset();

  // Helper string
  std::string formattedString;

  // -- Command description
  XBU::wrap_paragraphs(_description, 13, m_maxColumnWidth, false, formattedString);
  boost::format fmtHeader(fgc_header + "\nDESCRIPTION: " + fgc_headerBody + "%s\n" + fgc_reset);
  if ( !formattedString.empty() )
    std::cout << fmtHeader % formattedString;

  // -- Command usage
  std::string usage = XBU::create_usage_string(_optionDescription, _positionalDescription);
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
    XBU::wrap_paragraphs(option->description(), optionDescTab, m_maxColumnWidth, false, formattedString);

    std::string completeOptionName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);
    std::cout << fmtOOSubPositional % ("<" + option->long_name() + ">") % formattedString;
  }


  // -- Options
  report_option_help("OPTIONS", _optionDescription, _positionalDescription, false);

  if (XBU::getShowHidden()) 
    report_option_help("OPTIONS (Hidden)", _optionHidden, _positionalDescription, false);

  // Extended help
  boost::format fmtExtHelp(fgc_extendedBody + "\n  %s\n" +fgc_reset);
  XBU::wrap_paragraph(_extendedHelp, 2, m_maxColumnWidth, false, formattedString);
  if (!formattedString.empty()) 
    std::cout << fmtExtHelp % formattedString;
}

void 
XBUtilities::report_subcommand_help( const std::string &_executableName,
                                     const std::string &_subCommand,
                                     const std::string &_description, 
                                     const std::string &_extendedHelp,
                                     const boost::program_options::options_description &_optionDescription,
                                     const boost::program_options::options_description &_optionHidden,
                                     const SubCmd::SubOptionOptions & _subOptionOptions)
{
  // Formatting color parameters
  // Color references: https://en.wikipedia.org/wiki/ANSI_escape_code
  const std::string fgc_header       = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_HEADER).string();
  const std::string fgc_headerBody   = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_HEADER_BODY).string();
  const std::string fgc_commandBody  = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_SUBCMD).string();
  const std::string fgc_usageBody    = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_USAGE_BODY).string();

  const std::string fgc_ooption      = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_OOPTION).string();
  const std::string fgc_ooptionBody  = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_OOPTION_BODY).string();
  const std::string fgc_poption      = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_POSITIONAL).string();
  const std::string fgc_poptionBody  = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_POSITIONAL_BODY).string();
  const std::string fgc_extendedBody = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor(FGC_EXTENDED_BODY).string();
  const std::string fgc_reset        = XBUtilities::is_esc_enabled() ? "" : ec::fgcolor::reset();

  // Helper string
  std::string formattedString;

  // -- Command
  boost::format fmtCommand(fgc_header + "\nCOMMAND: " + fgc_commandBody + "%s\n" + fgc_reset);
  if ( !_subCommand.empty() )
    std::cout << fmtCommand % _subCommand;
 
  // -- Command description
  XBU::wrap_paragraphs(_description, 15, m_maxColumnWidth, false, formattedString);
  boost::format fmtHeader(fgc_header + "\nDESCRIPTION: " + fgc_headerBody + "%s\n" + fgc_reset);
  if ( !formattedString.empty() )
    std::cout << fmtHeader % formattedString;

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

  if (XBU::getShowHidden()) 
    report_option_help("OPTIONS (Hidden)", _optionHidden, emptyPOD, false);

  // Extended help
  boost::format fmtExtHelp(fgc_extendedBody + "\n  %s\n" +fgc_reset);
  XBU::wrap_paragraph(_extendedHelp, 2, m_maxColumnWidth, false, formattedString);
  if (!formattedString.empty()) 
    std::cout << fmtExtHelp % formattedString;
}

std::string 
XBUtilities::create_suboption_list_string(const VectorPairStrings &_collection)
{
  // Working variables
  const unsigned int maxColumnWidth = m_maxColumnWidth - m_shortDescriptionColumn; 
  std::string supportedValues;        // Formatted string of supported values
  std::string formattedString;        // Helper working string
                                      
  // Determine the indention width
  unsigned int maxStringLength = 0;
  for (const auto & pairs : _collection)
    maxStringLength = std::max(maxStringLength, (unsigned int) pairs.first.length());

  const unsigned int indention = maxStringLength + 5;  // New line indention after the '-' character (5 extra spaces)
  boost::format reportFmt(std::string("  %-") + std::to_string(maxStringLength) + "s - %s\n");  

  // report names and discription
  for (const auto & pairs : _collection) {
    XBU::wrap_paragraphs(boost::str(reportFmt % pairs.first % pairs.second), indention, maxColumnWidth, false /*indent first line*/, formattedString);
    supportedValues += formattedString;
  }

  return supportedValues;
}



std::string 
XBUtilities::create_suboption_list_string( const ReportCollection &_reportCollection, 
                                           bool _addAllOption)
{
  VectorPairStrings reportDescriptionCollection;

  // Add the report names and description
  for (const auto & report : _reportCollection) 
    reportDescriptionCollection.emplace_back(report->getReportName(), report->getShortDescription());

  // 'verbose' option
  if (_addAllOption) 
    reportDescriptionCollection.emplace_back("all", "All known reports are produced");

  // Sort the collection
  sort(reportDescriptionCollection.begin(), reportDescriptionCollection.end(), 
       [](const std::pair<std::string, std::string> & a, const std::pair<std::string, std::string> & b) -> bool
       { return (a.first.compare(b.first) < 0); });

  return create_suboption_list_string(reportDescriptionCollection);
}

std::string 
XBUtilities::create_suboption_list_string( const Report::SchemaDescriptionVector &_formatCollection)
{
  VectorPairStrings reportDescriptionCollection;

  // report names and description
  for (const auto & format : _formatCollection) {
    if (format.isVisable == true) 
      reportDescriptionCollection.emplace_back(format.optionName, format.shortDescription);
  }

  return create_suboption_list_string(reportDescriptionCollection);
}


void 
XBUtilities::collect_and_validate_reports( const ReportCollection &allReportsAvailable,
                                           const std::vector<std::string> &reportNamesToAdd,
                                           ReportCollection & reportsToUse)
{
  // If "verbose" used, then use all of the reports
  if (std::find(reportNamesToAdd.begin(), reportNamesToAdd.end(), "all") != reportNamesToAdd.end()) {
    reportsToUse = allReportsAvailable;
  } else { 
    // Examine each report name for a match 
    for (const auto & reportName : reportNamesToAdd) {
      auto iter = std::find_if(allReportsAvailable.begin(), allReportsAvailable.end(), 
                               [&reportName](const std::shared_ptr<Report>& obj) {return obj->getReportName() == reportName;});
      if (iter != allReportsAvailable.end()) 
        reportsToUse.push_back(*iter);
      else {
        throw xrt_core::error((boost::format("No report generator found for report: '%s'\n") % reportName).str());
      }
    }
  }
}


void 
XBUtilities::produce_reports( xrt_core::device_collection _devices, 
                              const ReportCollection & _reportsToProcess, 
                              Report::SchemaVersion _schemaVersion, 
                              std::vector<std::string> & _elementFilter,
                              std::ostream &_ostream)
{
  // Some simple DRCs
  if (_reportsToProcess.empty()) {
    _ostream << "Info: No action taken, no reports given.\n";
    return;
  }

  if (_schemaVersion == Report::SchemaVersion::unknown) {
    _ostream << "Info: No action taken, 'UNKNOWN' schema value specified.\n";
    return;
  }

  // Working property tree
  boost::property_tree::ptree ptRoot;

  // Add schema version
  {
    boost::property_tree::ptree ptSchemaVersion;
    ptSchemaVersion.put("schema", Report::getSchemaDescription(_schemaVersion).optionName.c_str());
    ptSchemaVersion.put("creation_date", xrt_core::timestamp());

    ptRoot.add_child("schema_version", ptSchemaVersion);
  }


  // -- Process the reports that don't require a device
  boost::property_tree::ptree ptSystem;
  for (const auto & report : _reportsToProcess) {
    if (report->isDeviceRequired() == true)
      continue;

    boost::any output = report->getFormattedReport(nullptr, _schemaVersion, _elementFilter);

    // Simple string output
    if (output.type() == typeid(std::string)) 
      _ostream << boost::any_cast<std::string>(output);

    if (output.type() == typeid(boost::property_tree::ptree)) {
      boost::property_tree::ptree ptReport = boost::any_cast< boost::property_tree::ptree>(output);

      // Only support 1 node on the root
      if (ptReport.size() > 1)
        throw xrt_core::error((boost::format("Invalid JSON - The report '%s' has too many root nodes.") % Report::getSchemaDescription(_schemaVersion).optionName).str());

      // We have 1 node, copy the child to the root property tree
      if (ptReport.size() == 1) {
        for (const auto & ptChild : ptReport) {
          ptSystem.add_child(ptChild.first, ptChild.second);
        }
      }
    }
  }
  if (!ptSystem.empty()) 
    ptRoot.add_child("system", ptSystem);

  // -- Check if any device sepcific report is requested
  auto dev_report = [_reportsToProcess]() {
    for (auto &report : _reportsToProcess) {
      if (report->isDeviceRequired() == true)
        return true;
      }
    return false;
  };

  if(dev_report()) {
    // -- Process reports that work on a device
    boost::property_tree::ptree ptDevices;
    int dev_idx = 0;
    for (const auto & device : _devices) {
      boost::property_tree::ptree ptDevice;
      auto bdf = xrt_core::device_query<xrt_core::query::pcie_bdf>(device);
      ptDevice.put("device_id", xrt_core::query::pcie_bdf::to_string(bdf));
      if (_schemaVersion == Report::SchemaVersion::text) {
        bool is_mfg = false;
        try {
          is_mfg = xrt_core::device_query<xrt_core::query::is_mfg>(device);
        } catch (...) {}
        
        //if factory mode
        std::string platform = "";
          if (is_mfg) {
            platform = "xilinx_" + xrt_core::device_query<xrt_core::query::board_name>(device) + "_GOLDEN";
          }
          else {
            platform = xrt_core::device_query<xrt_core::query::rom_vbnv>(device);
          }
        std::string dev_desc = (boost::format("%d/%d [%s] : %s\n") % ++dev_idx % _devices.size() % ptDevice.get<std::string>("device_id") % platform).str();
        _ostream << std::string(dev_desc.length(), '-') << std::endl;
        _ostream << dev_desc;
        _ostream << std::string(dev_desc.length(), '-') << std::endl;
      }
      for (auto &report : _reportsToProcess) {
        if (report->isDeviceRequired() == false)
          continue;

        boost::any output = report->getFormattedReport(device.get(), _schemaVersion, _elementFilter);

        // Simple string output
        if (output.type() == typeid(std::string)) 
          _ostream << boost::any_cast<std::string>(output);

        if (output.type() == typeid(boost::property_tree::ptree)) {
          boost::property_tree::ptree ptReport = boost::any_cast< boost::property_tree::ptree>(output);

          // Only support 1 node on the root
          if (ptReport.size() > 1)
            throw xrt_core::error((boost::format("Invalid JSON - The report '%s' has too many root nodes.") % Report::getSchemaDescription(_schemaVersion).optionName).str());

          // We have 1 node, copy the child to the root property tree
          if (ptReport.size() == 1) {
            for (const auto & ptChild : ptReport) {
              ptDevice.add_child(ptChild.first, ptChild.second);
            }
          }
        }
      }
      if (!ptDevice.empty()) 
        ptDevices.push_back(std::make_pair("", ptDevice));   // Used to make an array of objects
    }
    if (!ptDevices.empty())
      ptRoot.add_child("devices", ptDevices);
  }


  // Did we add anything to the property tree.  If so, then write it out.
  if ((_schemaVersion != Report::SchemaVersion::text) &&
      (_schemaVersion != Report::SchemaVersion::unknown)) {
    // Write out JSON format
    std::ostringstream outputBuffer;
    boost::property_tree::write_json(outputBuffer, ptRoot, true /*Pretty print*/);
    _ostream << outputBuffer.str() << std::endl;
  }
}

