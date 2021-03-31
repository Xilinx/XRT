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

// Not yet supported on windows, but soon will be.
#ifndef _WIN32

#ifndef __RapidJsonUtilities_h_
#define __RapidJsonUtilities_h_

// Include files
#include <rapidjson/document.h>
#include <vector>
#include <utility>
#include <sstream>

namespace XclBinUtilities {

enum class DType {
  unknown,
  integer,
  text_string,
  byte_string,
  hex_byte_string,
  byte_file,
  enumeration,
};

using KeyTypePair = std::pair<std::string, DType>;
using KeyTypeCollection = std::vector<KeyTypePair>;

void transform_to_primatives(rapidjson::Document& doc, const KeyTypeCollection& keyTypeCollection);
void write_cbor(rapidjson::Document& doc, const KeyTypeCollection& keyTypeCollection, std::ostringstream& buffer);
void read_cbor(std::istream& istr, rapidjson::Document& doc);

void TRACE_PrintTree(const std::string& msg, const rapidjson::Document& doc);

void collect_key_types(const std::string & jsonSchema, KeyTypeCollection & key_type_collection);

std::string get_dtype_str(DType data_type);
DType get_expected_type( const std::string& scope, const XclBinUtilities::KeyTypeCollection& keyTypeCollection);
void validate_against_schema(const std::string & nodeName, const rapidjson::Document & doc, const std::string & schema);

};
#endif
#endif
