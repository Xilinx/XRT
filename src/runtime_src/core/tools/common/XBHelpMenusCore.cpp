// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2020-2022 Xilinx, Inc
// Copyright (C) 2022-2025 Advanced Micro Devices, Inc. All rights reserved.
// ------ I N C L U D E   F I L E S -------------------------------------------
// Local - Include Files
#include "XBHelpMenusCore.h"
#include "XBUtilitiesCore.h"

namespace XBU = XBUtilities;


// 3rd Party Library - Include Files
#include <boost/property_tree/json_parser.hpp>
#include <boost/program_options/cmdline.hpp>
#include <boost/format.hpp>
namespace po = boost::program_options;

// System - Include Files
#include <iostream>
#include <algorithm>
#include <numeric>
#include <set>

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

class FormatHelper {
private:
  static FormatHelper* m_instance;
  FormatHelper() {
    this->fgc_header       = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_HEADER).string();
    this->fgc_headerBody   = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_HEADER_BODY).string();
    this->fgc_commandBody  = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_SUBCMD).string();
    this->fgc_usageBody    = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_USAGE_BODY).string();
    this->fgc_ooption      = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_OOPTION).string();
    this->fgc_ooptionBody  = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_OOPTION_BODY).string();
    this->fgc_poption      = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_POSITIONAL).string();
    this->fgc_poptionBody  = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_POSITIONAL_BODY).string();
    this->fgc_extendedBody = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_EXTENDED_BODY).string();
    this->fgc_reset        = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor::reset();
    this->fgc_optionName   = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_OPTION).string();
    this->fgc_optionBody   = XBUtilities::is_escape_codes_disabled() ? "" : ec::fgcolor(FGC_OPTION_BODY).string();
  }
  
public:
  std::string fgc_optionName;
  std::string fgc_optionBody;
  std::string fgc_header;
  std::string fgc_headerBody;
  std::string fgc_commandBody;
  std::string fgc_usageBody;
  std::string fgc_ooption;
  std::string fgc_ooptionBody;
  std::string fgc_poption;
  std::string fgc_poptionBody;
  std::string fgc_extendedBody;
  std::string fgc_reset;

  static FormatHelper& instance();
};

FormatHelper* FormatHelper::m_instance = nullptr;

FormatHelper&
FormatHelper::instance() {
  static FormatHelper formatHelper;
  return formatHelper;
}


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

/**
 * An enumeration to describe the type of argument a given
 * program option represents.
 * This determines the output order of options in the usage string
 */
enum OptionDescriptionFlagType {
  short_required = 0,
  long_required,
  short_required_arg,
  long_required_arg,
  short_simple,
  long_simple,
  short_arg,
  long_arg,
  positional,
  flag_type_size
};

static enum OptionDescriptionFlagType
get_option_type(const boost::shared_ptr<boost::program_options::option_description>& option,
                const boost::program_options::positional_options_description& _pod)
{
  const static int SHORT_OPTION_STRING_SIZE = 2;
  std::string optionDisplayName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);

  if (isPositional(optionDisplayName, _pod))
    return positional;

  if (option->semantic()->is_required()) {
    if (option->semantic()->max_tokens() == 0) {
      if (optionDisplayName.size() == SHORT_OPTION_STRING_SIZE)
        return short_required;

      return long_required;
    } else {
      if (optionDisplayName.size() == SHORT_OPTION_STRING_SIZE)
        return short_required_arg;

      return long_required_arg;
    }
  }

  if (option->semantic()->max_tokens() == 0) {  // Parse for simple flags
    if (optionDisplayName.size() == SHORT_OPTION_STRING_SIZE)
      return short_simple;

    return long_simple;
  }
  else { // Parse for flags with arguments
    if (optionDisplayName.size() == SHORT_OPTION_STRING_SIZE)
      return short_arg;

    return long_arg;
  }
}

static std::string
create_option_string(enum OptionDescriptionFlagType optionType,
                     const boost::shared_ptr<boost::program_options::option_description>& option,
                     bool removeLongOptDashes)
{
  const std::string& shortName = option->canonical_display_name(po::command_line_style::allow_dash_for_short);
  const std::string& longName =
      removeLongOptDashes ? option->long_name() : option->canonical_display_name(po::command_line_style::allow_long);
  switch (optionType) {
    case short_simple:
      return boost::str(boost::format("%s") % shortName[1]);
      break;
    case long_simple:
      return boost::str(boost::format("[%s]") % longName);
      break;
    case short_arg:
      return boost::str(boost::format("[%s arg]") % shortName);
      break;
    case long_arg:
      return boost::str(boost::format("[%s arg]") % longName);
      break;
    case short_required:
      return boost::str(boost::format("%s") % shortName);
      break;
    case long_required:
      return boost::str(boost::format("%s") % longName);
      break;
    case short_required_arg:
      return boost::str(boost::format("%s arg") % shortName);
      break;
    case long_required_arg:
      return boost::str(boost::format("%s arg") % longName);
      break;
    case positional:
      return boost::str(boost::format("%s") % shortName);
      break;
    case flag_type_size:
      throw std::runtime_error("Invalid argument setup detected");
      break;
  }
  throw std::runtime_error("Invalid argument setup detected");
}


std::string
XBUtilities::create_usage_string( const boost::program_options::options_description &_od,
                                  const boost::program_options::positional_options_description & _pod,
                                  bool removeLongOptDashes)
{
  // Create list of buffers to store each argument type
  std::vector<std::stringstream> buffers(flag_type_size);

  const auto& options = _od.options();

  for (const auto& option : options) {
    const auto optionType = get_option_type(option, _pod);
    const auto optionString = create_option_string(optionType, option, removeLongOptDashes);
    const auto is_buffer_empty = buffers[optionType].str().empty();
    // The short options have a bracket surrounding all options
    if ((optionType == short_simple) && is_buffer_empty)
      buffers[optionType] << "[-";
    // Add spaces only after the first character to simplify upper level formatting
    else if ((optionType != short_simple) && !is_buffer_empty)
      buffers[optionType] << " ";

    buffers[optionType] << boost::format("%s") % optionString;
  }

  // The short simple options have a bracket surrounding all options
  if (!buffers[short_simple].str().empty())
    buffers[short_simple] << "]";

  std::stringstream outputBuffer;
  for (const auto& buffer : buffers) {
    if (!buffer.str().empty()) {
      // Add spaces only after the first buffer to simplify upper level formatting
      if (!outputBuffer.str().empty())
        outputBuffer << " ";
      outputBuffer << buffer.str();
    }
  }

  return outputBuffer.str();
}

void
XBUtilities::report_commands_help( const std::string &_executable,
                                   const std::string &_description,
                                   const boost::program_options::options_description& _optionDescription,
                                   const boost::program_options::options_description& _optionHidden,
                                   const SubCmdsCollection &_subCmds)
{
  // Formatting color parameters
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
  boost::format fmtUsage(fgc_header + "\nUSAGE: " + fgc_usageBody + "%s %s\n" + fgc_reset);
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

  report_option_help("OPTIONS", _optionDescription);

  if (XBU::getShowHidden())
    report_option_help(std::string("OPTIONS ") + sHidden, _optionHidden);
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

static void
print_options(std::stringstream& stream,
              const boost::program_options::options_description& options,
              const bool report_param,
              const bool remove_long_dashes)
{
  const auto& fh = FormatHelper::instance();
  boost::format fmtOption(fh.fgc_optionName + "  %-18s " + fh.fgc_optionBody + "- %s\n" + fh.fgc_reset);
  for (auto & option : options.options()) {
    std::string optionDisplayFormat = create_option_format_name(option.get(), report_param, remove_long_dashes);
    const unsigned int optionDescTab = 23;
    auto formattedString = XBU::wrap_paragraphs(option->description(), optionDescTab, m_maxColumnWidth - optionDescTab, false);
    stream << fmtOption % optionDisplayFormat % formattedString;
  }
}

void
XBUtilities::report_option_help(const std::string & _groupName,
                                const boost::program_options::options_description& _optionDescription,
                                const bool _bReportParameter,
                                const bool removeLongOptDashes,
                                const std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>>& all_device_options,
                                const std::string& deviceClass)
{
  const auto& fh = FormatHelper::instance();

  // Report option group name (if defined)
  boost::format fmtHeader(fh.fgc_header + "\n%s:\n" + fh.fgc_reset);
  if ( !_groupName.empty() )
    std::cout << fmtHeader % _groupName;

  bool printAllOptions = false;
  boost::program_options::options_description common_options(_optionDescription);
  // If a device is specified only print applicable commands
  const auto it = all_device_options.find(deviceClass);
  if (it != all_device_options.end()) {
    for (const auto& subOption : it->second)
      common_options.add_options()(subOption->getConfigName().c_str(), subOption->getConfigDescription().c_str());
  } else if (!all_device_options.empty())
    printAllOptions = true;

  const auto& commonDeviceOptions = JSONConfigurable::extract_common_options(all_device_options);
  if (printAllOptions) {
    for (const auto& subOption : commonDeviceOptions)
      common_options.add_options()(subOption->getConfigName().c_str(), subOption->getConfigDescription().c_str());
  }

  // Generate the common options
  std::stringstream commonOutput;
  print_options(commonOutput, common_options, _bReportParameter, removeLongOptDashes);

  if (!printAllOptions) {
    std::cout << commonOutput.str();
    return;
  }

  if (all_device_options.size() > 1)
    std::cout << " Common:\n";
  std::cout << commonOutput.str();

  // Report all device class options
  for (const auto& [device_class, device_options] : all_device_options) {
    std::stringstream deviceSpecificOutput;
    for (const auto& subOption : device_options) {
      // Skip common options
      const auto& commonOptionsIt = commonDeviceOptions.find(subOption);
      if (commonOptionsIt != commonDeviceOptions.end())
        continue;

      boost::program_options::options_description options;
      options.add_options()(subOption->getConfigName().c_str(), subOption->getConfigDescription().c_str());
      print_options(deviceSpecificOutput, options, _bReportParameter, removeLongOptDashes);
    }

    const auto deviceSpecificOutputStr = deviceSpecificOutput.str();
    if (!deviceSpecificOutputStr.empty()) {
      auto map_it = JSONConfigurable::device_type_map.find(device_class);
      const std::string& valid_device_class = (map_it == JSONConfigurable::device_type_map.end()) ? device_class : map_it->second;
      std::cout << boost::format(" %s:\n%s") % valid_device_class % deviceSpecificOutputStr;
    }
  }
}

static std::string
create_suboption_usage_string(const std::vector<std::shared_ptr<JSONConfigurable>>& subOptions)
{
  std::string usage;
  for (const auto& subOption : subOptions) {
    if (subOption->getConfigHidden() && !XBU::getShowHidden())
      continue;

    if (!usage.empty())
      usage.append(" | ");

    boost::program_options::options_description newOptions;
    bool tempBool;
    newOptions.add_options()(subOption->getConfigName().c_str(),
                          boost::program_options::bool_switch(&tempBool)->required(),
                          subOption->getConfigDescription().c_str());
    const auto& option = newOptions.options()[0];
    const auto optionType = get_option_type(option, boost::program_options::positional_options_description());
    const auto optionString = create_option_string(optionType, option, false);
    usage.append(optionString);
  }
  return usage;
}

static std::vector<std::shared_ptr<JSONConfigurable>>
cast_vector(const std::vector<std::shared_ptr<OptionOptions>>& items)
{
  std::vector<std::shared_ptr<JSONConfigurable>> output;
  for (const auto& item : items)
    output.push_back(item);
  return output;
}

static void
display_subcommand_options(const std::string& executable,
                           const std::string& subcommand,
                           const std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>>& commandConfig,
                           const boost::program_options::options_description& options,
                           const boost::program_options::options_description& hiddenOptions,
                           const boost::program_options::positional_options_description& positionals,
                           const SubCmd::SubOptionOptions& subOptions,
                           const std::string& deviceClass
                           )
{
  const auto& fh = FormatHelper::instance();

  const std::string usage = XBU::create_usage_string(options, positionals, false);
  std::string usageSuboption;
  if (deviceClass.empty() || commandConfig.empty())
    usageSuboption = create_suboption_usage_string(cast_vector(subOptions));
  else {
    auto it = commandConfig.find(deviceClass);
    if (it == commandConfig.end())
      throw_cancel(boost::format("Invalid device class: %s\n") % deviceClass);
    usageSuboption = create_suboption_usage_string(it->second);
  }

  if (usageSuboption.empty())
    std::cout << boost::format(fh.fgc_header + "\nUSAGE: " + fh.fgc_usageBody + "%s %s %s\n" + fh.fgc_reset) % executable % subcommand % usage;
  else
    std::cout << boost::format(fh.fgc_header + "\nUSAGE: " + fh.fgc_usageBody + "%s %s %s %s\n" + fh.fgc_reset) % executable % subcommand % usageSuboption % usage;

  std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>> commonJsonOptions;
  if (commandConfig.empty())
    commonJsonOptions.emplace("", cast_vector(subOptions));

  std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>> hiddenJsonOptions;
  for (const auto& devicePair : commandConfig) {
    std::vector<std::shared_ptr<JSONConfigurable>> common;
    std::vector<std::shared_ptr<JSONConfigurable>> hidden;
    for (const auto& option : devicePair.second) {
      if (option->getConfigHidden())
        hidden.push_back(option);
      else
        common.push_back(option);
    }

    if (!common.empty())
      commonJsonOptions.emplace(devicePair.first, common);

    if (!hidden.empty())
      hiddenJsonOptions.emplace(devicePair.first, hidden);
  }

  report_option_help("OPTIONS", options, false, false, commonJsonOptions, deviceClass); // Make a new report_option_help that prints the separated by device class?

  if (XBU::getShowHidden())
    report_option_help("OPTIONS (Hidden)", hiddenOptions, false, false, hiddenJsonOptions, deviceClass);
}

void
XBUtilities::report_subcommand_help(const std::string& _executableName,
                                    const std::string& _subCommand,
                                    const std::string& _description,
                                    const std::string& _extendedHelp,
                                    const boost::program_options::options_description& _optionDescription,
                                    const boost::program_options::options_description& _optionHidden,
                                    const boost::program_options::options_description& _globalOptions,
                                    const boost::program_options::positional_options_description& _positionalDescription,
                                    const SubCmd::SubOptionOptions& _subOptionOptions,
                                    bool /*removeLongOptDashes*/,
                                    const std::string& customHelpSection,
                                    const std::map<std::string, std::vector<std::shared_ptr<JSONConfigurable>>>& commandConfig,
                                    const std::string& deviceClass)
{
  const auto& fh = FormatHelper::instance();

  // -- Command
  boost::format fmtCommand(fh.fgc_header + "\nCOMMAND: " + fh.fgc_commandBody + "%s\n" + fh.fgc_reset);
  if (!_subCommand.empty())
    std::cout << fmtCommand % _subCommand;

  // -- Command description
  {
    auto formattedString = XBU::wrap_paragraphs(_description, 15, m_maxColumnWidth, false);
    boost::format fmtHeader(fh.fgc_header + "\nDESCRIPTION: " + fh.fgc_headerBody + "%s\n" + fh.fgc_reset);
    if (!formattedString.empty())
      std::cout << fmtHeader % formattedString;
  }

  display_subcommand_options(_executableName, _subCommand, commandConfig, _optionDescription, _optionHidden, _positionalDescription, _subOptionOptions, deviceClass);

  // -- Custom Section
  std::cout << customHelpSection << "\n";

  // -- Global Options
  report_option_help("GLOBAL OPTIONS", _globalOptions, false);

  // Extended help
  {
    boost::format fmtExtHelp(fh.fgc_extendedBody + "\n  %s\n" + fh.fgc_reset);
    auto formattedString = XBU::wrap_paragraphs(_extendedHelp, 2, m_maxColumnWidth, false);
    if (!formattedString.empty())
      std::cout << fmtExtHelp % formattedString;
  }
}

std::vector<std::string>
XBUtilities::process_arguments( po::variables_map& vm,
                                po::command_line_parser& parser,
                                const po::options_description& options,
                                const po::positional_options_description& positionals,
                                bool validate_arguments
                                )
{
  // Add unregistered "option"" that will catch all extra positional arguments
  po::options_description all_options(options);
  all_options.add_options()("__unreg", po::value<std::vector<std::string> >(), "Holds all unregistered options");
  po::positional_options_description all_positionals(positionals);
  all_positionals.add("__unreg", -1);

  // Parse the given options and hold onto the results
  auto parsed_options = parser.options(all_options)
                        .positional(all_positionals)
                        .allow_unregistered()
                        .style(po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
                        .run();

  if (validate_arguments) {
    // This variables holds options denoted with a '-' or '--' that were not registered
    const auto unrecognized_options = po::collect_unrecognized(parsed_options.options, po::exclude_positional);
    // Parse out all extra positional arguments from the boost results
    // This variable holds arguments that do not have a '-' or '--' that did not have a registered positional space
    std::vector<std::string> extra_positionals;
    for (const auto& option : parsed_options.options) {
      if (boost::equals(option.string_key, "__unreg"))
        // Each option is a vector even though most of the time they contain only one element
        for (const auto& bad_option : option.value)
          extra_positionals.push_back(bad_option);
    }

    // Throw an exception if we have any unknown options or extra positionals
    if (!unrecognized_options.empty() || !extra_positionals.empty()) {
      std::string error_str;
      error_str.append("Unrecognized arguments:\n");
      for (const auto& option : unrecognized_options)
        error_str.append(boost::str(boost::format("  %s\n") % option));
      for (const auto& option : extra_positionals)
        error_str.append(boost::str(boost::format("  %s\n") % option));
      throw boost::program_options::error(error_str);
    }
  }

  // Parse the options into the variable map
  // If an exception occurs let it bubble up and be handled elsewhere
  po::store(parsed_options, vm);
  po::notify(vm);

  // Return all the unrecognized arguments for use in a lower level command if needed
  return po::collect_unrecognized(parsed_options.options, po::include_positional);
}
