// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2019-2022 Xilinx, Inc
// Copyright (C) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.

#ifndef __SubCmd_h_
#define __SubCmd_h_

// Please keep eternal include file dependencies to a minimum
#include <vector>
#include <string>
#include <map>
#include <boost/program_options.hpp>

#include "JSONConfigurable.h"
#include "SubCmdJsonObjects.h"
#include "OptionOptions.h"
  
class SubCmd : public JSONConfigurable {
 public:
   using SubCmdOptions = std::vector<std::string>;
   using SubOptionOptions = std::vector<std::shared_ptr<OptionOptions>>;

 public:
   virtual void execute(const SubCmdOptions &_options) const = 0;
   virtual void setOptionConfig(const boost::property_tree::ptree &config);

 public:
   const std::string &getName() const { return m_subCmdName; };
   const std::string &getConfigName() const { return getName(); };
   const std::string &getShortDescription() const { return m_shortDescription; };
   const std::string &getConfigDescription() const { return getShortDescription(); };
   bool isHidden() const { return m_isHidden; };
   bool getConfigHidden() const {return isHidden();};
   bool isDeprecated() const { return m_isDeprecated; };
   bool isPreliminary() const { return m_isPreliminary; };
   bool isDefaultDeviceValid() const { return m_defaultDeviceValid; };

 public:
   void setExecutableName(const std::string & _name) { m_executableName = _name; };
   const std::string & getExecutableName() const {return m_executableName; };

   void setGlobalOptions(const boost::program_options::options_description &globalOptions) { m_globalOptions.add(globalOptions); };

 protected:
   const boost::program_options::options_description & getGlobalOptions() const { return m_globalOptions; };

 public:
   virtual ~SubCmd() {};

public:

 // Child class Helper methods
 protected:
  SubCmd(const std::string & _name, const std::string & _shortDescription);
  void setIsHidden(bool _isHidden) { m_isHidden = _isHidden; };
  void setIsDeprecated(bool _isDeprecated) { m_isDeprecated = _isDeprecated; };
  void setIsPreliminary(bool _isPreliminary) { m_isPreliminary = _isPreliminary; };
  void setIsDefaultDevValid(bool _defaultDeviceValid) { m_defaultDeviceValid = _defaultDeviceValid; };
  void setLongDescription(const std::string &_longDescription) {m_longDescription = _longDescription; };
  void setExampleSyntax(const std::string& _exampleSyntax) { m_exampleSyntax = _exampleSyntax; };
  void printHelp(const bool removeLongOptDashes = false,
                 const std::string& customHelpSection = "",
                 const std::string& deviceClass = "") const;
  void printHelp(const boost::program_options::options_description& commonOptions,
                 const boost::program_options::options_description& hiddenOptions,
                 const std::string& deviceClass = "",
                 const bool removeLongOptDashes = false,
                 const std::string& customHelpSection = "") const;
  std::vector<std::string> process_arguments( boost::program_options::variables_map& vm,
                           const SubCmdOptions& _options,
                           const boost::program_options::options_description& common_options,
                           const boost::program_options::options_description& hidden_options,
                           const boost::program_options::positional_options_description& positionals = boost::program_options::positional_options_description(),
                           const SubOptionOptions& suboptions = SubOptionOptions(),
                           bool validate_arguments = true) const;
  std::vector<std::string> process_arguments(boost::program_options::variables_map& vm,
                                             const SubCmdOptions& _options,
                                             bool validate_arguments = true) const;
  void conflictingOptions( const boost::program_options::variables_map& _vm, 
                           const std::string &_opt1, const std::string &_opt2) const;
  void addSubOption(std::shared_ptr<OptionOptions> option);
  std::shared_ptr<OptionOptions> checkForSubOption(const boost::program_options::variables_map& vm, const std::string& deviceClass = "") const;

 protected:
  SubOptionOptions m_subOptionOptions;
  boost::program_options::options_description m_commonOptions;
  boost::program_options::options_description m_hiddenOptions;
  boost::program_options::positional_options_description m_positionals;
  boost::property_tree::ptree m_commandConfig;
  SubCmdJsonObjects::JsonConfig m_jsonConfig;

  template<class T>
  std::vector<std::shared_ptr<T>>
  validateConfigurables(const std::string& device_class,
                        const std::string& target,
                        const std::vector<std::shared_ptr<T>>& allOptions) const
  {
    const auto& configs = JSONConfigurable::parse_configuration_tree(m_commandConfig);
    const auto& deviceOptions = JSONConfigurable::extract_subcmd_config<T, T>(allOptions, configs, getConfigName(), target);

    std::vector<std::shared_ptr<T>> configurables;
    auto it = deviceOptions.find(device_class);
    if (it != deviceOptions.end()) {
      for (const auto& configurable : it->second)
        configurables.push_back(configurable);
    } else { // If an unknown device class is specified allow for the running of any test
      for (const auto& configurable : allOptions)
        configurables.push_back(configurable);
    }

    return configurables;
  }

 private:
  SubCmd() = delete;

 // Variables
 private:
  std::string m_executableName;
  std::string m_subCmdName;
  std::string m_shortDescription;
  std::string m_longDescription;
  std::string m_exampleSyntax;
  boost::program_options::options_description m_globalOptions;
  void printHelpInternal(const bool removeLongOptDashes,
                         const std::string& customHelpSection,
                         const std::string& deviceClass,
                         const boost::program_options::options_description& common_options,
                         const boost::program_options::options_description& hidden_options) const;

  bool m_isHidden;
  bool m_isDeprecated;
  bool m_isPreliminary;
  bool m_defaultDeviceValid;
};

#endif


