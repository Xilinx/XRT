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

// Not yet supported on windows, but it will be soon
#ifndef _WIN32


#include "SectionSmartNic.h"

#include "XclBinUtilities.h"
#include "CBOR.h"
#include "RapidJsonUtilities.h"
#include "ResourcesSmartNic.h"

namespace XUtil = XclBinUtilities;

#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

// Need to convert between boost property trees and rapid json
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <fstream>

// Static Variables / Classes
SectionSmartNic::_init SectionSmartNic::_initializer;


bool
SectionSmartNic::doesSupportAddFormatType(FormatType _eFormatType) const {
  if ((_eFormatType == FT_JSON) ||
      (_eFormatType == FT_RAW))
    return true;

  return false;
}

bool
SectionSmartNic::doesSupportDumpFormatType(FormatType _eFormatType) const {
  if ((_eFormatType == FT_JSON) ||
      (_eFormatType == FT_HTML) ||
      (_eFormatType == FT_RAW))
    return true;

  return false;
}


void
SectionSmartNic::marshalToJSON(char* _pDataSection,
                               unsigned int _sectionSize,
                               boost::property_tree::ptree& _ptree) const {
  XUtil::TRACE("");
  XUtil::TRACE("Extracting: CBOR Image");
  XUtil::TRACE_BUF("CBOR", _pDataSection, _sectionSize);

  // Wrap the buffer in an input stream.
  std::string dataBuffer(_pDataSection, _sectionSize);
  std::istringstream iStrBuffer(dataBuffer);

  // Transform the CBOR image to JSON
  rapidjson::Document document;
  XclBinUtilities::read_cbor(iStrBuffer, document);

  XUtil::TRACE_PrintTree("SmartNic: Read CBOR", document);

  // Convert from rapidjson to boost
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
  document.Accept(writer);

  std::stringstream ss(buf.GetString());
  boost::property_tree::ptree pt;
  boost::property_tree::read_json(ss, pt);

  // Add child tree
  _ptree.add_child("smartnic", pt);
}


static
void
read_file_into_buffer(const std::string& fileName,
                      const std::string& fromRelativeDir,
                      std::vector<char>& buffer) {
  // Build the path to our file of interest
  boost::filesystem::path filePath(fromRelativeDir);
  filePath /= fileName;

  // Open the file
  std::ifstream file(filePath.string(), std::ifstream::in | std::ifstream::binary);
  if (!file.is_open())
    throw std::runtime_error("ERROR: Unable to open the file for reading: " + fileName);

  // Get the file size
  file.seekg(0, file.end);
  std::streamsize fileSize = file.tellg();
  file.seekg(0, file.beg);

  // Resize the buffer and read in the array
  buffer.resize(fileSize);
  file.read(buffer.data(), fileSize);

  // Make sure that the entire buffer was read
  if (file.gcount() != fileSize)
    throw std::runtime_error("ERROR: Input stream for the binary buffer is smaller then the expected size.");
}

static
void rename_file_node(const std::string & fileNodeName, 
                      rapidjson::Value &value,
                      rapidjson::Document::AllocatorType& allocator)
{
  // Remove the subname (e.g., _file) from the existing key
  const static std::string removeSubName = "_file";

  std::string newKey = fileNodeName;              
  auto index = newKey.find(removeSubName); 
  if (index == std::string::npos)
     return;

  newKey.replace(index, removeSubName.size(), "");

  XUtil::TRACE(boost::str(boost::format("Renaming node '%s' to '%s'") % fileNodeName % newKey));

  // Rename keys by swapping their values
  rapidjson::Value::MemberIterator itrOldKey = value.FindMember(fileNodeName.c_str());
  value.AddMember(rapidjson::Value(newKey.c_str(), allocator).Move(), itrOldKey->value, allocator);
  value.RemoveMember(fileNodeName.c_str());
}

static
void
readAndTransposeByteFiles_recursive(const std::string& scope,
                                    rapidjson::Value::MemberIterator itrObject,
                                    const XclBinUtilities::KeyTypeCollection& keyTypeCollection,
                                    rapidjson::Document::AllocatorType& allocator,
                                    const std::string& relativeFromDir) {
  XUtil::TRACE((boost::format("BScope: %s") % scope).str());

  // A dictionary
  if (itrObject->value.IsObject()) {
    std::vector<std::string> renameCollection;
    for (rapidjson::Value::MemberIterator itr = itrObject->value.MemberBegin(); itr != itrObject->value.MemberEnd(); ++itr) {

      // Look "forward" to see if this dictionary contains the node of interest
      std::string currentScope = scope + "::" + itr->name.GetString();
      if (get_expected_type(currentScope, keyTypeCollection) == XUtil::DType::byte_file) 
        renameCollection.push_back(std::string(itr->name.GetString()));

      readAndTransposeByteFiles_recursive(scope + "::" + itr->name.GetString(), itr, keyTypeCollection, allocator, relativeFromDir);
    }

    // Now that we are out of that "nasty" Iterator loop, rename the updated keys.
    for (const std::string& oldFileNodeName : renameCollection)
      rename_file_node(oldFileNodeName, itrObject->value, allocator);

    return;
  }

  // An array
  if (itrObject->value.IsArray()) {
    for (auto itr = itrObject->value.Begin(); itr != itrObject->value.End(); ++itr) {
      std::vector<std::string> renameCollection;
      rapidjson::Value& attribute = *itr;

      if (!attribute.IsObject()) 
        continue;

      for (auto itr2 = attribute.MemberBegin(); itr2 != attribute.MemberEnd(); ++itr2) {

        // Look "forward" to see if this dictionary contains the node of interest
        std::string currentScope = scope + "[]" + itr2->name.GetString();
        if (get_expected_type(currentScope, keyTypeCollection) == XUtil::DType::byte_file) 
          renameCollection.push_back(std::string(itr2->name.GetString()));

        readAndTransposeByteFiles_recursive(scope + "[]" + itr2->name.GetString(), itr2, keyTypeCollection, allocator, relativeFromDir);
      }

      // Now that we are out of that "nasty" Iterator loop, rename the updated keys.
      for (const std::string& oldFileNodeName : renameCollection)
        rename_file_node(oldFileNodeName, attribute, allocator);
    }

    return;  
  }

  // End point String
  if (itrObject->value.IsString()) {
    // Do we have a match
    if (get_expected_type(scope, keyTypeCollection) != XUtil::DType::byte_file) 
      return;  

    // Read file image from disk
    std::vector<char> buffer;
    read_file_into_buffer(itrObject->value.GetString(), relativeFromDir, buffer);

    // Convert the binary data to hex and re-add
    std::string byteString(buffer.data(), buffer.size());
    byteString = boost::algorithm::hex(byteString);
    itrObject->value.SetString(byteString.data(), byteString.size(), allocator);

    return;  
  }
}


static void
readAndTransposeByteFiles(rapidjson::Document& doc,
                          const XclBinUtilities::KeyTypeCollection& keyTypeCollection,
                          const std::string& dirRelativeFrom) {
  if (!doc.IsObject())
    return;

  for (auto itr = doc.MemberBegin(); itr != doc.MemberEnd(); ++itr)
    readAndTransposeByteFiles_recursive(std::string("#") + itr->name.GetString(), itr, keyTypeCollection, doc.GetAllocator(), dirRelativeFrom);

}

void
SectionSmartNic::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                 std::ostringstream& _buf) const {
  const std::string nodeName = "smartnic";

  XUtil::TRACE("");
  XUtil::TRACE("SmartNic : Marshalling From JSON");

  // -- Retrieve only the JSON tree associated with the SmartNic section
  boost::property_tree::ptree ptSmartNic = _ptSection.get_child(nodeName);

  // -- Convert from a boost property tree to a rapidjson document
  std::ostringstream buf;
  boost::property_tree::write_json(buf, ptSmartNic, false);

  rapidjson::Document document;
  if (document.Parse(buf.str().c_str()).HasParseError()) {
    throw std::runtime_error("Error: The 'smartnic' JSON format is not valid JSON.");
  }

  // -- Transform the JSON elements to their expected primitive value
  XUtil::KeyTypeCollection keyTypeCollection;
  XUtil::collect_key_types(getSmartNicSchema(), keyTypeCollection);

  XUtil::transform_to_primatives(document, keyTypeCollection);

  // -- Validate the smartnic schema
  XUtil::validate_against_schema(nodeName, document, getSmartNicSchema());

  // -- Read in the byte code
  boost::filesystem::path p(getPathAndName());
  std::string fromRelativeDir = p.parent_path().string();
  readAndTransposeByteFiles(document, keyTypeCollection, fromRelativeDir);

  // -- Create the cbor buffer
  std::ostringstream buffer;
  XUtil::write_cbor(document, keyTypeCollection, buffer);

  // Write the contents to given output stream
  std::string aString = buffer.str();
  _buf.write(aString.data(), aString.length());
}



void
SectionSmartNic::appendToSectionMetadata(const boost::property_tree::ptree& /*_ptAppendData*/,
                                         boost::property_tree::ptree& /*_ptToAppendTo*/) {
  throw std::runtime_error("Error: appendToSectionMetadata() not implemented....yet.");
}

#endif
