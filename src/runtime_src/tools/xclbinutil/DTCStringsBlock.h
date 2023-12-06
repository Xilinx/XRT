/**
 * Copyright (C) 2018 Xilinx, Inc
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

#ifndef __DTCStringsBlock_h_
#define __DTCStringsBlock_h_

// ----------------------- I N C L U D E S -----------------------------------
#include <cstdint>
#include <sstream>
#include <string>

// ----------- C L A S S :   D T C S t r i n g s B l o c k -------------------

class DTCStringsBlock {

 public:
  DTCStringsBlock();
  ~DTCStringsBlock();

 public:
  uint32_t addString(const std::string& _dtcString);
  std::string getString(unsigned int _offset) const;

  void parseDTCStringsBlock(const char* _pBuffer, const unsigned int _size);
  void marshalToDTC(std::ostream& _buf) const;

 private:
  std::ostringstream* m_pDTCStringBlock;
};

#endif
