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

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/hex.hpp>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

namespace XUtil = XclBinUtilities;

std::string  
XclBinUtilities::get_dtype_str(DType data_type)
{
  switch (data_type) {
    case DType::unknown:
      return "unknown";

    case DType::integer:
      return "integer";

    case DType::text_string:
      return "text_string";

    case DType::byte_string:
      return "byte_string";

    case DType::hex_byte_string:
      return "hex_byte_string";

    case DType::byte_file:
      return "byte_file";

    case DType::enumeration:
      return "enumeration";
  }

  // Code will never get here, but unfortunately some compilers don't know that
  throw std::runtime_error("Error: Unknown DType enumeration value.");
}



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
XclBinUtilities::DType
XclBinUtilities::get_expected_type( const std::string& scope,
                                    const XclBinUtilities::KeyTypeCollection& keyTypeCollection) {
  // Check to see if there is a mapping for this scope
  auto it = std::find_if(keyTypeCollection.begin(),
                         keyTypeCollection.end(),
                         [&scope](const XclBinUtilities::KeyTypePair& element) {return element.first.compare(scope) == 0;});

  // No mapping
  if (it == keyTypeCollection.end())
    return XUtil::DType::unknown;

  // Return back the expected mapping
  return it->second;
}

// Transform the rapidjson values to their "real" types
static
void recursive_transformation(const std::string& scope,
                              rapidjson::Value::MemberIterator itrObject,
                              const XclBinUtilities::KeyTypeCollection& keyTypeCollection)
{
  XUtil::TRACE((boost::format("TScope: %s") % scope).str());

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
    if (mappingType == XUtil::DType::integer) {

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
      recursive_transformation(std::string("#")+itr->name.GetString(), itr, keyTypeCollection);
    }
  }
}


static void
recursive_write_cbor(const std::string& scope,
                     const rapidjson::Value& attribute,
                     const XUtil::KeyTypeCollection& keyTypeCollection,
                     std::ostringstream& buffer) {
  XUtil::TRACE((boost::format("EScope: %s") % scope).str());

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
    if (mappingType == XUtil::DType::hex_byte_string) {
      std::string hexString = attribute.GetString();
      buffer << XUtil::encode_byte_string(boost::algorithm::unhex(hexString));
    } else
      buffer << XUtil::encode_text_string(attribute.GetString());

    return; // -- Return --
  }

  // Serialize the unsigned integer
  if (attribute.IsUint64()) {
    buffer << XUtil::encode_positive_integer(attribute.GetUint64());
    return; // -- Return --
  }

  // Serialize the negative integer
  if (attribute.IsInt64()) {
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
    recursive_write_cbor(std::string("#") + itr->name.GetString(), itr->value, keyTypeCollection, buffer);
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

        XUtil::TRACE((boost::format("               Text String: '%s'") % textString).str());
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
      throw std::runtime_error("Error: Map of items key is not a string (read_cbor).");

    // Get the value
    rapidjson::Value mapValue;
    recursive_read_cbor(istr, mapValue, doc.GetAllocator());

    doc.AddMember(key, mapValue, doc.GetAllocator());
  }
}

// Some Linux OSs packages of rapidjson don't support schema validation
#ifndef ENABLE_JSON_SCHEMA_VALIDATION

void
XclBinUtilities::validate_against_schema( const std::string &, 
                                          const rapidjson::Document &, 
                                          const std::string &)
{
  std::cout << "Info: JSON Schema Validation is not support with this version of software.";
}

#else
#include <rapidjson/schema.h>

void 
XclBinUtilities::validate_against_schema(const std::string &nodeName, const rapidjson::Document & doc, const std::string & schema)
{
  rapidjson::Document sd;
  if(sd.Parse(schema.c_str()).HasParseError()) {
    XUtil::TRACE(boost::str(boost::format("Schema:\n %s") % schema));
    throw std::runtime_error("Error: The given JSON schema is not valid JSON.");
  }

  rapidjson::SchemaDocument schemaDoc(sd);        // Convert to a schema document
  rapidjson::SchemaValidator validator(schemaDoc);

  // Valid the given JSON file
  if (doc.Accept(validator)) {
    XUtil::TRACE("JSON syntax successfully validated against the schema.");
    return;
  }

  // The JSON image is invalid according to the schema
  rapidjson::StringBuffer sb;
  validator.GetInvalidSchemaPointer().StringifyUriFragment(sb);
    
  std::ostringstream buf;
  buf << "Error: JSON schema violation" << std::endl;
  boost::format simpleFormat("  %-22s: %s\n");
  buf << simpleFormat % "JSON Node" % nodeName;
  buf << simpleFormat % "Schema violation rule" % sb.GetString();
  buf << simpleFormat % "Violation type" % validator.GetInvalidSchemaKeyword();

  sb.Clear();
  validator.GetInvalidDocumentPointer().StringifyUriFragment(sb);
  buf << simpleFormat % "JSON document path" % sb.GetString();

  XUtil::TRACE(boost::str(boost::format("Schema:\n %s") % schema));
  XUtil::TRACE_PrintTree("JSON", doc);
  throw std::runtime_error(buf.str());
}
#endif

static void recursive_collect_properties( const std::string& scope, rapidjson::Value::MemberIterator itrObject, XclBinUtilities::KeyTypeCollection& keyTypeCollection);
static void recursive_collect_array( const std::string& scope, rapidjson::Value::MemberIterator itrObject, XclBinUtilities::KeyTypeCollection& keyTypeCollection);


static 
void recursive_collect_array( const std::string& scope, 
                              rapidjson::Value::MemberIterator itrObject, 
                              XclBinUtilities::KeyTypeCollection& keyTypeCollection)
{
  const rapidjson::Value::MemberIterator itemsItr = itrObject->value.FindMember("items");
  if (itemsItr == itrObject->value.MemberEnd())
    return;

  const rapidjson::Value::MemberIterator typeItr = itemsItr->value.FindMember("type");
  if (typeItr == itemsItr->value.MemberEnd()) 
    return;

  // -- array of objects --
  if (std::string("object") == typeItr->value.GetString()) {
    const rapidjson::Value::MemberIterator propertiesItr = itemsItr->value.FindMember("properties");
    if (propertiesItr == itemsItr->value.MemberEnd()) 
      return;

    for (auto itr = propertiesItr->value.MemberBegin(); itr != propertiesItr->value.MemberEnd(); ++itr) 
      recursive_collect_properties(scope + "[]" + itr->name.GetString(), itr, keyTypeCollection);

    return; 
  }
}

static
void recursive_collect_properties( const std::string& scope,
                                   rapidjson::Value::MemberIterator itrObject,
                                   XclBinUtilities::KeyTypeCollection& keyTypeCollection)
{
  XUtil::TRACE((boost::format("CScope: %s") % scope).str());

  // Look for a non-default "string" 'type'
  const rapidjson::Value::MemberIterator typeItr = itrObject->value.FindMember("type");
  if (typeItr == itrObject->value.MemberEnd())
    return;

  // -- object --
  if (std::string("object") == typeItr->value.GetString()) {
    const rapidjson::Value::MemberIterator propertiesItr = itrObject->value.FindMember("properties");
    if (propertiesItr == itrObject->value.MemberEnd())
      return;

    for (auto itr = propertiesItr->value.MemberBegin(); itr != propertiesItr->value.MemberEnd(); ++itr) 
      recursive_collect_properties(scope + "::" + itr->name.GetString(), itr, keyTypeCollection);
      
    return; 
  }

  // -- array --
  if (std::string("array") == typeItr->value.GetString()) 
    return recursive_collect_array(scope, itrObject, keyTypeCollection);
    
  // -- integer --
  if (std::string("integer") == typeItr->value.GetString()) 
    return keyTypeCollection.emplace_back(scope, XUtil::DType::integer);

  // -- string --
  if (std::string("string") == typeItr->value.GetString()) {
    const rapidjson::Value::MemberIterator cborTypeItr = itrObject->value.FindMember("extendedType");
    if (cborTypeItr == itrObject->value.MemberEnd())
      return;

    //-- hex-encoded_string --
    if (std::string("hex-encoded") == cborTypeItr->value.GetString()) 
      return keyTypeCollection.emplace_back(scope, XUtil::DType::hex_byte_string);
  
    //-- byte file on disk --
    if (std::string("file-image") == cborTypeItr->value.GetString()) 
      return keyTypeCollection.emplace_back(scope, XUtil::DType::byte_file);
  
    //-- enumeration --
    if (std::string("enum-encoded") == cborTypeItr->value.GetString()) 
      return keyTypeCollection.emplace_back(scope, XUtil::DType::enumeration);
  
    return;
  }
}


void 
XclBinUtilities::collect_key_types(const std::string & jsonSchema, 
                                   KeyTypeCollection & key_type_collection)
{
  // Initialize return values
  key_type_collection.clear();

  rapidjson::Document doc;
  if (doc.Parse(jsonSchema.c_str()).HasParseError()) {
    throw std::runtime_error("Error: The given JSON schema is not valid JSON.");
  }


  // Examine the properties
  rapidjson::Value::MemberIterator itr = doc.FindMember("properties");
  if (itr == doc.MemberEnd()) {
    XUtil::TRACE("Did not find the node 'properties'");
    XUtil::TRACE_PrintTree("Schema", doc);
    return;
  }


  XUtil::TRACE("Found 'properties'");
  for (auto itr2 = itr->value.MemberBegin(); itr2 !=itr->value.MemberEnd(); ++itr2) 
    recursive_collect_properties(std::string("#")+itr2->name.GetString(), itr2, key_type_collection);
  
  if (XUtil::getVerbose()) {
    for (const auto & entry : key_type_collection) {
      std::cout << boost::str(boost::format("%s : %s(%d)\n") % entry.first % get_dtype_str(entry.second) % (int) entry.second);
    }
  }
}


#endif
