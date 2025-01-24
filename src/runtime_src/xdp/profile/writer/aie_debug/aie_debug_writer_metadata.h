/**
 * Copyright (C) 2022-2024 Advanced Micro Devices, Inc. - All rights reserved
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

#ifndef AIE_DEBUG_WRITER_METADATA_H
#define AIE_DEBUG_WRITER_METADATA_H

#include <map>
#include <vector>
#include <string>
#include <cstdint>

namespace xdp {

/*************************************************************************************
The class WriterUsedRegisters is what gives us AIE hw generation specific data. The base class
has virtual functions which populate the correct registers according to the AIE hw generation 
in the derived classes. Thus we can dynamically populate the correct registers at runtime.
**************************************************************************************/
class WriterUsedRegisters {
  public:
    struct RegData {
      std::string field_name;
      std::string bit_range;
      int shift;
      uint32_t mask;

      RegData(std::string n, std::string b, int s, uint32_t m)
        : field_name(n), bit_range(b), shift(s), mask(m) {}
    };

  protected:
    std::map<std::string, std::vector<RegData>> regDataMap;

  public:
    WriterUsedRegisters() { }

    virtual ~WriterUsedRegisters() {
      regDataMap.clear();
    }

    std::map<std::string, std::vector<RegData>>& getRegDataMap() {
      return regDataMap;
    }    

    virtual void populateRegDataMap() {};

};

/*************************************************************************************
 AIE1 Registers
 *************************************************************************************/
class AIE1WriterUsedRegisters : public WriterUsedRegisters {
public:
  AIE1WriterUsedRegisters() {
    populateRegDataMap();
  }
  ~AIE1WriterUsedRegisters() = default;

  void populateRegDataMap();

};

/*************************************************************************************
 AIE2 Registers
 *************************************************************************************/
class AIE2WriterUsedRegisters : public WriterUsedRegisters {
public:
  AIE2WriterUsedRegisters() {
    populateRegDataMap();
  }
  ~AIE2WriterUsedRegisters() = default;

  void populateRegDataMap();

};

/*************************************************************************************
 AIE2PS Registers
 *************************************************************************************/
class AIE2PSWriterUsedRegisters : public WriterUsedRegisters {
public:
  AIE2PSWriterUsedRegisters() {
    populateRegDataMap();
  }
  ~AIE2PSWriterUsedRegisters() = default;

  void populateRegDataMap();

};

} // end namesapce xdp

#endif
