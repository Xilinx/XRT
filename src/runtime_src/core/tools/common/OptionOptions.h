/**
 * Copyright (C) 2020-2022 Xilinx, Inc
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

#ifndef __OptionOptions_h_
#define __OptionOptions_h_

// Please keep eternal include file dependencies to a minimum
#include <boost/program_options.hpp>
#include <string>
#include <vector>

class OptionOptions {
 public:
  typedef std::vector<std::string> SubCmdOptions;

  virtual void execute(const SubCmdOptions& _options) const = 0;

 public:
  const boost::shared_ptr<boost::program_options::option_description>& option() const { return m_selfOption.options()[0]; };
  const std::string& longName() const { return m_longName; };
  const std::string optionNameString() const { return m_shortName.empty() ? m_longName : m_longName + "," + m_shortName; };
  const std::string& description() const { return m_description; };
  const std::string& extendedHelp() const { return m_extendedHelp; };
  bool isHidden() const { return m_isHidden; };

  void setExecutable(const std::string& _executable) { m_executable = _executable; };
  void setCommand(const std::string& _command) { m_command = _command; };

  const boost::program_options::options_description& getOptionsDescription() const { return m_optionsDescription; };
  const boost::program_options::positional_options_description& getPositionalOptions() const { return m_positionalOptions; };

  void setGlobalOptions(const boost::program_options::options_description& globalOptions) { m_globalOptions.add(globalOptions); };

 public:
  virtual ~OptionOptions(){};

  // Child class Helper methods
 protected:
  OptionOptions(const std::string& longName, bool isHidden, const std::string& description);
  OptionOptions(const std::string& longName,
                const std::string& shortName,
                const std::string& optionDescription,
                const boost::program_options::value_semantic* optionValue,
                const std::string& valueDescription,
                bool isHidden);
  void setExtendedHelp(const std::string& extendedHelp) { m_extendedHelp = extendedHelp; };
  void printHelp() const;
  std::vector<std::string> process_arguments(boost::program_options::variables_map& vm,
                                             const SubCmdOptions& options,
                                             bool validate_arguments = true) const;

 private:
  OptionOptions() = delete;

 protected:
  boost::program_options::options_description m_selfOption;
  boost::program_options::options_description m_optionsDescription;
  boost::program_options::options_description m_optionsHidden;
  boost::program_options::positional_options_description m_positionalOptions;

 private:
  std::string m_executable;
  std::string m_command;
  std::string m_longName;
  std::string m_shortName;
  bool m_isHidden;
  std::string m_description;
  std::string m_extendedHelp;
  bool m_defaultOptionValue;
  boost::program_options::options_description m_globalOptions;
};

#endif
