/**
 * Copyright (C) 2018 - 2019, 2022 Xilinx, Inc
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

#ifndef __SectionMemTopology_h_
#define __SectionMemTopology_h_

// ----------------------- I N C L U D E S -----------------------------------
#include "Section.h"

// ------- C L A S S :   S e c t i o n M e m T o p o l o g y -----------------
class SectionMemTopology : public Section {
 public:
  bool doesSupportAddFormatType(FormatType _eFormatType) const override;
  bool doesSupportDumpFormatType(FormatType _eFormatType) const override;

 protected:
  void marshalToJSON(char* _pDataSection, unsigned int _sectionSize, boost::property_tree::ptree& _ptree) const override;
  void marshalFromJSON(const boost::property_tree::ptree& _ptSection, std::ostringstream& _buf) const override;

 protected:
  const std::string getMemTypeStr(enum MEM_TYPE _memType) const;
  enum MEM_TYPE getMemType(std::string& _sMemType) const;

 private:
  // Static initializer helper class
  static class init {
   public:
    init();
  } initializer;
};

#endif
