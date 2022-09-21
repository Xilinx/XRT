/**
 * Copyright (C) 2019-2022 Xilinx, Inc
 * Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
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

#ifndef __SubCmd_h_
#define __SubCmd_h_

// Please keep eternal include file dependencies to a minimum
#include <vector>
#include <string>
#include <map>
#include <boost/program_options.hpp>

#include "OptionOptions.h"
  
class SubCmd {
 public:
   using SubCmdOptions = std::vector<std::string>;
   using SubOptionOptions = std::vector<std::shared_ptr<OptionOptions>>;

 public:
   virtual void execute(const SubCmdOptions &_options) const = 0;

 public:
   const std::string &getName() const { return m_subCmdName; };
   const std::string &getShortDescription() const { return m_shortDescription; };
   bool isHidden() const { return m_isHidden; };
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
  void setExampleSyntax(const std::string &_exampleSyntax) {m_exampleSyntax = _exampleSyntax; };
  void printHelp(const boost::program_options::options_description & _optionDescription,
                 const boost::program_options::options_description & _optionHidden,
                 bool removeLongOptDashes = false,
                 const std::string& customHelpSection = "") const;
  void printHelp( const boost::program_options::options_description & _optionDescription,
                  const boost::program_options::options_description & _optionHidden,
                  const SubOptionOptions & _subOptionOptions) const;
  void printHelp() const;
  std::vector<std::string> process_arguments( boost::program_options::variables_map& vm,
                           const SubCmdOptions& _options,
                           const boost::program_options::options_description& common_options,
                           const boost::program_options::options_description& hidden_options,
                           const boost::program_options::positional_options_description& positionals = boost::program_options::positional_options_description(),
                           const SubOptionOptions& suboptions = SubOptionOptions(),
                           bool validate_arguments = true) const;
  std::vector<std::string> process_arguments( boost::program_options::variables_map& vm,
                           const SubCmdOptions& _options,
                           bool validate_arguments = true) const;
  void conflictingOptions( const boost::program_options::variables_map& _vm, 
                           const std::string &_opt1, const std::string &_opt2) const;
  void addSubOption(std::shared_ptr<OptionOptions> option);
  std::shared_ptr<OptionOptions> checkForSubOption(const boost::program_options::variables_map& vm) const;


  SubOptionOptions m_subOptionOptions;
  boost::program_options::options_description m_commonOptions;
  boost::program_options::options_description m_hiddenOptions;
  boost::program_options::positional_options_description m_positionals;

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

  bool m_isHidden;
  bool m_isDeprecated;
  bool m_isPreliminary;
  bool m_defaultDeviceValid;
};

#endif


