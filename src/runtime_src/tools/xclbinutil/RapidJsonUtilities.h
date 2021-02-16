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

typedef enum {
  DType_UNKNOWN,
  DType_INTEGER,
  DType_TEXT_STRING,
  DType_BYTE_STRING
} DType;

using KeyTypePair = std::pair<std::string, DType>;
using KeyTypeCollection = std::vector<KeyTypePair>;

void transform_to_primatives(rapidjson::Document& doc, const KeyTypeCollection& keyTypeCollection);
void write_cbor(rapidjson::Document& doc, const KeyTypeCollection& keyTypeCollection, std::ostringstream& buffer);
void read_cbor(std::istream& istr, rapidjson::Document& doc);

void TRACE_PrintTree(const std::string& msg, const rapidjson::Document& doc);

};
#endif
#endif
