/**
 * Copyright (C) 2021 Xilinx, Inc
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

#ifndef __CBOR_h_
#define __CBOR_h_

// ----------------------- I N C L U D E S -----------------------------------

// #includes here - please keep these to a bare minimum!
#include <string>
#include <sstream>

// ------------ F O R W A R D - D E C L A R A T I O N S ----------------------
// Forward declarations - use these instead whenever possible...


namespace XclBinUtilities {

enum class MajorTypes{
  positive_integer = 0,
  negative_integer = 1,
  byte_string      = 2,
  text_string      = 3,
  array_of_items   = 4,
  map_of_items     = 5,
  semantic_tag     = 6,
  primitives       = 7
};

std::string enum_to_string(const MajorTypes majorType);
std::string encode_major_type(const MajorTypes majorType, const uint64_t count);
std::string encode_positive_integer(const uint64_t value);
std::string encode_negative_integer(const uint64_t value);
std::string encode_text_string(const std::string& text_string);
std::string encode_byte_string(const std::string& byte_string);

std::string get_string(std::istream& istr, uint64_t size);
void get_next_type_and_count(std::istream& istr, MajorTypes& majorType, uint64_t& count);

};

#endif
