/**
 * Copyright (C) 2019-2020 Xilinx, Inc
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
#include <boost/program_options.hpp>
  
class SubCmd {
 public:
   typedef std::vector<std::string> SubCmdOptions;

 public:
   virtual void execute(const SubCmdOptions &_options) const = 0;

 public:
   const std::string &getName() const { return m_subCmdName; };
   const std::string &getShortDescription() const { return m_shortDescription; };
   bool isHidden() const { return m_isHidden; };
   bool isDeprecated() const { return m_isDeprecated; };
   bool isPreliminary() const { return m_isPreliminary; };

 public:
   void setExecutableName(const std::string & _name) { m_executableName = _name; };
   const std::string & getExectuableName() const {return m_executableName; };

 public:
   virtual ~SubCmd() {};

 // Child class Helper methods
 protected:
  SubCmd(const std::string & _name, const std::string & _shortDescription);
  void setIsHidden(bool _isHidden) { m_isHidden = _isHidden; };
  void setIsDeprecated(bool _isDeprecated) { m_isDeprecated = _isDeprecated; };
  void setIsPreliminary(bool _isPreliminary) { m_isPreliminary = _isPreliminary; };
  void setLongDescription(const std::string &_longDescription) {m_longDescription = _longDescription; };
  void setExampleSyntax(const std::string &_exampleSyntax) {m_exampleSyntax = _exampleSyntax; };
  void printHelp(const boost::program_options::options_description & _optionDescription) const;

 private:
  SubCmd() = delete;

 // Variables
 private:
  std::string m_executableName;
  std::string m_subCmdName;
  std::string m_shortDescription;
  std::string m_longDescription;
  std::string m_exampleSyntax;

  bool m_isHidden;
  bool m_isDeprecated;
  bool m_isPreliminary;
};
  
#endif