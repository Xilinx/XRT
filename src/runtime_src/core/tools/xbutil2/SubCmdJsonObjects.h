// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <string>
#include <unordered_map>

#include "boost/property_tree/ptree.hpp"
#include "boost/program_options.hpp"

static constexpr std::string_view const_name_literal = "name";
static constexpr std::string_view const_description_literal = "description";
static constexpr std::string_view const_tag_literal = "tag";
static constexpr std::string_view const_alias_literal = "alias";
static constexpr std::string_view const_default_value_literal = "default_value";
static constexpr std::string_view const_option_type_literal = "option_type";
static constexpr std::string_view const_value_type_literal = "value_type";
static constexpr std::string_view const_options_literal = "options";

namespace po = boost::program_options;
namespace pt = boost::property_tree;

enum class ValueType {
  boolean,
  string,
  array,
  none
};

class OptionBasic {

public:
  std::string m_name;
  std::string m_description;
  std::string m_tag;
  OptionBasic(const pt::ptree& configurations)
    : m_name(configurations.get<std::string>(std::string(const_name_literal))), 
      m_description(configurations.get<std::string>(std::string(const_description_literal))), 
      m_tag(configurations.get<std::string>(std::string(const_tag_literal)))
    {}

  std::string getName() const { return m_name; }
  std::string getDescription() const { return m_description; }
  std::string getTag() const { return m_tag; }
  void printOption() const;
};


class SubCommandOption : public OptionBasic {
  std::string m_alias;
  std::string m_defaultValue;
  std::string m_optionType;
  std::string m_valueType;
  pt::ptree   m_ptEmpty;

  /*
  * Map of option name vs SubCommandOption objects. Example:
  * --run can have multiple option Values like latency, throughput etc.
  * latency : OptionBasic object
  * throughput : OptionBasic object
  * .................
  * df-bw : OptionBasic object
  */
  std::unordered_map<std::string, OptionBasic> m_subOptionMap;
  std::unordered_map<std::string, OptionBasic>
  createBasicOptions(const pt::ptree& pt);

protected:
  const std::unordered_map<std::string, ValueType> m_valueTypeMap = {
    {"bool", ValueType::boolean},
    {"string", ValueType::string},
    {"array", ValueType::array},
    {"none", ValueType::none}
  };

public:

  SubCommandOption(const pt::ptree& configurations):
      OptionBasic(configurations),
      m_alias(configurations.get<std::string>(std::string(const_alias_literal), "")),
      m_defaultValue(configurations.get<std::string>(std::string(const_default_value_literal), "")),
      m_optionType(configurations.get<std::string>(std::string(const_option_type_literal), "")),
      m_valueType(configurations.get<std::string>(std::string(const_value_type_literal), "")),
      m_ptEmpty(pt::ptree()),
      m_subOptionMap(createBasicOptions(configurations.get_child(std::string(const_options_literal), m_ptEmpty)))
    {}

  std::string getValueType() const { return m_valueType; }
  std::string getAlias() const { return m_alias; }
  std::string getDefaultValue() const { return m_defaultValue; }
  std::string getOptionType() const { return m_optionType; }
  std::unordered_map<std::string, OptionBasic> getSubOptionMap() const { return m_subOptionMap; }

  void addProgramOption(po::options_description& options, const std::string& optionsType);
  void printOption() const;
};

class SubCommand : public OptionBasic {
  /*
  * Map of option name vs SubCommandOption objects. Example:
  * --device : SubCommandOption object
  * --format : SubCommandOption object
  * .................
  * --run : SubCommandOption object
  */
  std::unordered_map<std::string, SubCommandOption> m_optionMap;

  std::unordered_map<std::string, SubCommandOption>
  createSubCommandOptions(const pt::ptree& pt);

public:
  SubCommand(const pt::ptree& configurations) :
      OptionBasic(configurations),
      m_optionMap(createSubCommandOptions(configurations.get_child(std::string(const_options_literal)))) 
    {}
  std::unordered_map<std::string,SubCommandOption> getOptionMap() const { return m_optionMap; }

  void addProgramOptions(po::options_description& options, const std::string& optionsType);
};

/**
 * @brief JsonConfig class to handle the json configurations.
 * Each SubCommand class will keep an object of this class type. 
 * Ideally SubCommand object creation should also be done at run-time
 * and there should only be one object of this class type in existence.
 * But that's a task for future enhancements.
 */

class JsonConfig {
  /* Map of subcommand name vs Subcommand objects
  * validate : SubCommand object
  * configure : SubCommand object
  * examine : SubCommand object 
  */
  std::unordered_map<std::string, SubCommand> m_subCommandMap;
  std::unordered_map<std::string, SubCommand>
  createSubCommands(const pt::ptree& pt, const std::string& subCommand);
public:
  JsonConfig(const pt::ptree& configurations, const std::string& subCommand)
    : m_subCommandMap(createSubCommands(configurations, subCommand))
    {}

  void addProgramOptions(po::options_description& options, const std::string& optionsType, const std::string& subCommand);
  void printConfigurations() const;
};
