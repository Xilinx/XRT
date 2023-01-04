/**
 * Copyright (C) 2018-2019, 2022 Xilinx, Inc
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

#ifndef __SectionIPLayout_h_
#define __SectionIPLayout_h_

// ----------------------- I N C L U D E S -----------------------------------
#include "Section.h"

// --------- C L A S S :   S e c t i o n I P L a y o u t ---------------------
class SectionIPLayout : public Section {
 public:
  void appendToSectionMetadata(const boost::property_tree::ptree& _ptAppendData, boost::property_tree::ptree& _ptToAppendTo)override;

 public:
  // static so that these two methods can be used in KernelUtilities 
  static std::string getFunctionalEnumStr(const std::string& sFunctional);
  static std::string getSubTypeEnumStr(const std::string& sSubType);

  static PS_FUNCTIONAL getFunctional(const std::string& sFunctional);
  static PS_SUBTYPE getSubType(const std::string& sSubType);
   
 protected:
  void marshalToJSON(char* _pDataSection, unsigned int _sectionSize, boost::property_tree::ptree& _ptree) const override;
  void marshalFromJSON(const boost::property_tree::ptree& _ptSection, std::ostringstream& _buf) const override;

 protected:
  const std::string getIPTypeStr(IP_TYPE _ipType) const;
  const std::string getIPControlTypeStr(IP_CONTROL _ipControlType) const;
  const std::string getFunctionalStr(PS_FUNCTIONAL eFunctional) const;
  const std::string getSubTypeStr(PS_SUBTYPE eSubType) const;
  IP_TYPE getIPType(std::string& _sIPType) const;
  IP_CONTROL getIPControlType(std::string& _sIPControlType) const;

 private:
  // Static initializer helper class
  static class init {
   public:
    init();
  } initializer;
};

#endif
