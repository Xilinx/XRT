// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include <string>
#include <map>

#include "boost/property_tree/ptree.hpp"
#include "boost/program_options.hpp"

namespace SubCmdJsonObjects {

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
  std::string m_type;

  public:

  OptionBasic(const pt::ptree& configurations);

  std::string getName() const { return m_name; }
  std::string getDescription() const { return m_description; }
  std::string getType() const { return m_type; }
  void printOption() const;
};


class SubCommandOption : public OptionBasic {
  std::string m_alias;
  std::string m_defaultValue;
  std::string m_valueType;
  std::vector<OptionBasic> m_description_array;
  pt::ptree   m_ptEmpty;

public:
  SubCommandOption(const pt::ptree& configurations);

  std::string getValueType() const { return m_valueType; }
  std::string getAlias() const { return m_alias; }
  std::string getDefaultValue() const { return m_defaultValue; }
  std::vector<OptionBasic> createDescriptionArray(const pt::ptree& pt);

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
  std::map<std::string, SubCommandOption> m_optionMap;

  std::map<std::string, SubCommandOption>
  createSubCommandOptions(const pt::ptree& pt);

public:
  SubCommand(const pt::ptree& configurations); 
  std::map<std::string,SubCommandOption> getOptionMap() const { return m_optionMap; }

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
  std::map<std::string, SubCommand> m_subCommandMap;
  std::map<std::string, SubCommand>
  createSubCommands(const pt::ptree& pt, const std::string& subCommand);
public:
  JsonConfig(const pt::ptree& configurations, const std::string& subCommand)
    : m_subCommandMap(createSubCommands(configurations, subCommand))
    {}
  JsonConfig() = default;

  void addProgramOptions(po::options_description& options, const std::string& optionsType, const std::string& subCommand);
  void printConfigurations() const;
};
} // namespace SubCmdJsonObjects
