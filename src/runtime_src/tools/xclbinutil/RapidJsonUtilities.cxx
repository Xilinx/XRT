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

// Not yet supported on windows, but it will be soon.
#ifndef _WIN32

#include "RapidJsonUtilities.h"
#include "XclBinUtilities.h"
#include "CBOR.h"

#include <boost/format.hpp>
#include <boost/algorithm/hex.hpp>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>


namespace XUtil = XclBinUtilities;


void
XclBinUtilities::TRACE_PrintTree(const std::string& msg,
                                 const rapidjson::Document& doc) {
  // Only report data in verbose mode
  if (!XUtil::getVerbose())
    return;

  std::cout << "Trace: Rapid JSON Tree (" << msg << ")" << std::endl;

  // Create the JSON pretty print JSON file
  rapidjson::StringBuffer buf;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
  doc.Accept(writer);

  // Print the JSON
  std::cout << buf.GetString() << std::endl;
}


// Determine what the expected type is for the given scope
static XclBinUtilities::DType
get_expected_type(const std::string& scope,
                  const XclBinUtilities::KeyTypeCollection& keyTypeCollection) {
  // Check to see if there is a mapping for this scope
  auto it = std::find_if(keyTypeCollection.begin(),
                         keyTypeCollection.end(),
                         [&scope](const XclBinUtilities::KeyTypePair& element) {return element.first.compare(scope) == 0;});

  // No mapping
  if (it == keyTypeCollection.end())
    return XUtil::DType_UNKNOWN;

  // Return back the expected mapping
  return it->second;
}

// Transform the rapidjson values to their "real" types
static
void recursive_transformation(const std::string& scope,
                              rapidjson::Value::MemberIterator itrObject,
                              const XclBinUtilities::KeyTypeCollection& keyTypeCollection)
// Algorithm:
//    Only look for endpoints in the JSON graph and those points need to be strings
{
  XUtil::TRACE((boost::format("-- Scope: %s") % scope).str());

  // A dictionary
  if (itrObject->value.IsObject()) {
    for (auto itr = itrObject->value.MemberBegin(); itr != itrObject->value.MemberEnd(); ++itr) {
      recursive_transformation(scope + "::" + itr->name.GetString(), itr, keyTypeCollection);
    }
    return;  // -- Return --
  }

  // An array
  if (itrObject->value.IsArray()) {
    for (auto itr = itrObject->value.Begin(); itr != itrObject->value.End(); ++itr) {
      rapidjson::Value& attribute = *itr;

      if (attribute.IsObject()) {
        for (auto itr2 = attribute.MemberBegin(); itr2 != attribute.MemberEnd(); ++itr2) {
          recursive_transformation(scope + "[]" + itr2->name.GetString(), itr2, keyTypeCollection);
        }
      }
    }
    return;  // -- Return --
  }

  // End point String
  if (itrObject->value.IsString()) {
    // We have a string endpoint, now determine if the type needs to change
    XUtil::DType mappingType = get_expected_type(scope, keyTypeCollection);

    const std::string& workingString = itrObject->value.GetString();

    // Integer
    if (mappingType == XUtil::DType_INTEGER) {

      // Determine if this is a negative value
      auto negativeCount = std::count(workingString.begin(), workingString.end(), '-');
      if (negativeCount > 1) {
        std::string errMsg = (boost::format("Error: Multiple negative (e.g., '-') found: '%s'") % workingString).str();
        throw std::runtime_error(errMsg);
      }

      // Negative Value
      if (negativeCount == 1) {
        if (workingString.find_first_not_of("-0123456789") != std::string::npos) {
          std::string errMsg =  (boost::format("Error: Malformed negative integer: '%s'") % workingString).str();
          throw std::runtime_error(errMsg);
        }

        itrObject->value.SetInt64(std::stoll(itrObject->value.GetString()));
      } else {
        // Positive Value
        if (workingString.find_first_not_of("0123456789") != std::string::npos) {
          std::string errMsg = (boost::format("Error: Malformed integer: '%s'") % workingString).str();
          throw std::runtime_error(errMsg);
        }

        itrObject->value.SetUint64(std::stoull(itrObject->value.GetString()));
      }
      return;    // -- Return --
    }
  }
}


// Transform the rapidjson primitives to their expected values
void
XclBinUtilities::transform_to_primatives(rapidjson::Document& doc,
                                         const KeyTypeCollection& keyTypeCollection) {
  if (doc.IsObject()) {
    for (auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
      recursive_transformation(itr->name.GetString(), itr, keyTypeCollection);
    }
  }
}


static void
recursive_write_cbor(const std::string& scope,
                     const rapidjson::Value& attribute,
                     const XUtil::KeyTypeCollection& keyTypeCollection,
                     std::ostringstream& buffer) {
  XUtil::TRACE((boost::format("-- Scope: %s") % scope).str());

  // Serialize the MAP of items (Note: Objects are maps)
  if (attribute.IsObject()) {
    auto mapSize = attribute.MemberCount();
    buffer << XUtil::encode_major_type(XUtil::MajorTypes::map_of_items, mapSize);

    for (auto itr = attribute.MemberBegin(); itr != attribute.MemberEnd(); ++itr) {
      buffer << XUtil::encode_text_string(itr->name.GetString());
      recursive_write_cbor(scope + "::" + itr->name.GetString(), itr->value, keyTypeCollection, buffer);
    }
    return; // -- Return --
  }

  // Serialize the array of items
  if (attribute.IsArray()) {
    // Add array
    auto arraySize = attribute.Size();
    buffer << XUtil::encode_major_type(XUtil::MajorTypes::array_of_items, arraySize);

    // Add array elements
    for (auto itr = attribute.Begin(); itr != attribute.End(); ++itr) {
      recursive_write_cbor(scope + "[]", *itr, keyTypeCollection, buffer);
    }

    return; // -- Return
  }

  // Serialize the string
  if (attribute.IsString()) {
    XUtil::DType mappingType = get_expected_type(scope, keyTypeCollection);
    if (mappingType == XUtil::DType_BYTE_STRING) {
      std::string hexString = attribute.GetString();
      buffer << XUtil::encode_byte_string(boost::algorithm::unhex(hexString));
    } else
      buffer << XUtil::encode_text_string(attribute.GetString());

    return; // -- Return --
  }

  // Serialize the unsigned integer
  if (attribute.IsUint()) {
    buffer << XUtil::encode_positive_integer(attribute.GetUint64());
    return; // -- Return --
  }

  // Serialize the negative integer
  if (attribute.IsInt()) {
    buffer << XUtil::encode_negative_integer(attribute.GetInt64());
    return; // -- Return --
  }

}

void
XclBinUtilities::write_cbor(rapidjson::Document& doc,
                            const KeyTypeCollection& keyTypeCollection,
                            std::ostringstream& buffer) {

  auto mapSize = doc.MemberCount();

  // Do we have anything to work with.
  if (!mapSize)
    return;


  // The root is a mapping of pairs
  buffer << XUtil::encode_major_type(XUtil::MajorTypes::map_of_items, mapSize);

  // Add the pair mapping
  for (auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr) {
    buffer << XUtil::encode_text_string(itr->name.GetString());
    recursive_write_cbor(itr->name.GetString(), itr->value, keyTypeCollection, buffer);
  }
}

void
recursive_read_cbor(std::istream& istr,
                    rapidjson::Value& aValue,
                    rapidjson::Document::AllocatorType& allocator) {
  XclBinUtilities::MajorTypes majorType;
  uint64_t count = 0;
  XclBinUtilities::get_next_type_and_count(istr, majorType, count);

  switch (majorType) {
    case XclBinUtilities::MajorTypes::positive_integer:
      aValue.SetUint64(count);
      break;

    case XclBinUtilities::MajorTypes::negative_integer:
      aValue.SetInt64(-count);
      break;

    case XclBinUtilities::MajorTypes::byte_string: {
        std::string byteString = boost::algorithm::hex(XclBinUtilities::get_string(istr, count));
        aValue.SetString(byteString.data(), byteString.size(), allocator);
      }
      break;

    case XclBinUtilities::MajorTypes::text_string: {
        std::string textString = XclBinUtilities::get_string(istr, count);
        aValue.SetString(textString.data(), textString.size(), allocator);
      }
      break;

    case XclBinUtilities::MajorTypes::array_of_items:
      aValue.SetArray();
      for (uint64_t index = 0; index < count; ++index) {
        rapidjson::Value arrayItem;
        recursive_read_cbor(istr, arrayItem, allocator);
        aValue.PushBack(arrayItem, allocator);
      }
      break;

    case XclBinUtilities::MajorTypes::map_of_items: {
        aValue.SetObject();
        for (uint64_t index = 0; index < count; ++index) {
          // Get the key string
          rapidjson::Value key;
          recursive_read_cbor(istr, key, allocator);

          if (!key.IsString())
            throw std::runtime_error("Error: Map of Items key is not a string.");

          // Get the value
          rapidjson::Value mapValue;
          recursive_read_cbor(istr, mapValue, allocator);

          aValue.AddMember(key, mapValue, allocator);
        }
      }
      break;

    case XclBinUtilities::MajorTypes::semantic_tag:
      throw std::runtime_error("Error: Decoding CBOR Major Type 'Semantic Tag' is not supported.");
      break;

    case XclBinUtilities::MajorTypes::primitives:
      throw std::runtime_error("Error: Decoding CBOR Major Type 'Primitives' is not supported.");
      break;
  }
}

void
XclBinUtilities::read_cbor(std::istream& istr,
                           rapidjson::Document& doc) {
  MajorTypes majorType;
  uint64_t count = 0;

  XclBinUtilities::get_next_type_and_count(istr, majorType, count);
  if (majorType !=MajorTypes:: map_of_items)
    throw std::runtime_error("Error: CBOR images does not start with Major Type 5 'Map of Items'");

  // We are working with a map of items, indicate so in the document
  doc.SetObject();
  for (uint64_t index = 0; index < count; ++index) {
    // Get the key string
    rapidjson::Value key;
    recursive_read_cbor(istr, key, doc.GetAllocator());

    if (!key.IsString())
      throw std::runtime_error("Error: Map of Items key is not a string.");

    // Get the value
    rapidjson::Value mapValue;
    recursive_read_cbor(istr, mapValue, doc.GetAllocator());

    doc.AddMember(key, mapValue, doc.GetAllocator());
  }
}

#endif
