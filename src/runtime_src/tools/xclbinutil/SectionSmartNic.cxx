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

namespace XUtil = XclBinUtilities;

#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

// Need to convert between boost property trees and rapid json
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <fstream>


// TODO: Insert JSON Schema here as a string literal
//static std::string foo = R"~~~~(
//This is a test
//)~~~~";

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
void
readAndTransposeByteFiles_recursive(const std::string& scope,
                                    rapidjson::Value& aValue,
                                    std::vector<std::string>& byteFileEntries,
                                    rapidjson::Document::AllocatorType& allocator,
                                    const std::string& relativeFromDir) {
  XUtil::TRACE((boost::format("-- Scope: %s") % scope).str());

  // A dictionary
  if (aValue.IsObject()) {
    std::vector<std::string> renameCollection;
    for (rapidjson::Value::MemberIterator itr = aValue.MemberBegin(); itr != aValue.MemberEnd(); ++itr) {
      std::string currentScope = scope + "::" + itr->name.GetString();
      // Do we have a match
      if (std::find(byteFileEntries.begin(), byteFileEntries.end(), currentScope) != byteFileEntries.end()) {
        std::string myString = itr->name.GetString();
        renameCollection.push_back(myString);
      }

      readAndTransposeByteFiles_recursive(currentScope, itr->value, byteFileEntries, allocator, relativeFromDir);
    }

    // Now that we are out of that "nasty" Iterator loop, rename the updated keys.
    // TODO: Refactor code to have the new file name be data driven
    for (const std::string& oldKey : renameCollection) {
      const static std::string removeSubName = "_file";
      std::string newKey = oldKey;
      auto index = newKey.find(removeSubName);
      if (index == std::string::npos)
        continue;
      newKey.replace(index, removeSubName.size(), "");

      // Now rename keys by swapping their values
      rapidjson::Value::MemberIterator itrOldKey = aValue.FindMember(oldKey.c_str());
      aValue.AddMember(rapidjson::Value(newKey.c_str(), allocator).Move(), itrOldKey->value, allocator);
      aValue.RemoveMember(oldKey.c_str());
    }

    return;  // -- Return --
  }

  // An array
  if (aValue.IsArray()) {
    for (auto itr = aValue.Begin(); itr != aValue.End(); ++itr) {
      if (itr->IsObject())
        readAndTransposeByteFiles_recursive(scope + "[]", *itr, byteFileEntries, allocator, relativeFromDir);
    }
    return;  // -- Return --
  }

  // End point String
  if (aValue.IsString()) {
    // Do we have a match
    if (std::find(byteFileEntries.begin(), byteFileEntries.end(), scope) == byteFileEntries.end())
      return;  // -- Return --

    std::vector<char> buffer;
    read_file_into_buffer(aValue.GetString(), relativeFromDir, buffer);

    // Convert the binary data to hex and re-add
    std::string byteString(buffer.data(), buffer.size());
    byteString = boost::algorithm::hex(byteString);
    aValue.SetString(byteString.data(), byteString.size(), allocator);

    return;    // -- Return --
  }
}


static void
readAndTransposeByteFiles(rapidjson::Document& doc,
                          std::vector<std::string>& byteFileEntries,
                          const std::string& dirRelativeFrom) {
  readAndTransposeByteFiles_recursive("", doc, byteFileEntries, doc.GetAllocator(), dirRelativeFrom);
}

void
SectionSmartNic::marshalFromJSON(const boost::property_tree::ptree& _ptSection,
                                 std::ostringstream& _buf) const {
  XUtil::TRACE("");
  XUtil::TRACE("SmartNic : Marshalling From JSON");

  // -- Get the JSON tree associated with the SmartNic section
  const boost::property_tree::ptree& ptSmartNic = _ptSection.get_child("smartnic");

  // -- Convert from a boost property tree to a rapidjson document
  std::ostringstream buf;
  boost::property_tree::write_json(buf, ptSmartNic, false);

  rapidjson::Document document;
  const std::string& bufString = buf.str();
  document.Parse(bufString.c_str());

  XUtil::TRACE_PrintTree("SmartNic: Pre-RapidJson Transform", document);

  // -- Transform JSON elements back to their primitive values (from strings)
  // Temporary collection until functionality is added to enable this data
  // to be obtained from the JSON schema
  static const XUtil::KeyTypeCollection m_keyTypeCollection = {
    { "schema_version::major",                               XUtil::DType_INTEGER },
    { "schema_version::minor",                               XUtil::DType_INTEGER },
    { "schema_version::patch",                               XUtil::DType_INTEGER },
    { "extensions[]version_info::minor",                     XUtil::DType_INTEGER },
    { "extensions[]version_info::patch",                     XUtil::DType_INTEGER },
    { "extensions[]cam_instances::id",                       XUtil::DType_INTEGER },
    { "extensions[]cam_instances::driver_index",             XUtil::DType_INTEGER },
    { "extensions[]address_mapping::offset",                 XUtil::DType_INTEGER },
    { "extensions[]address_mapping::aperture_size_bytes",    XUtil::DType_INTEGER },
    { "extensions[]messages[]id",                            XUtil::DType_INTEGER },
    { "extensions[]messages[]param_size_bytes",              XUtil::DType_INTEGER },
    { "extensions[]resource_classes[]max_count",             XUtil::DType_INTEGER },
    { "extensions[]resource_classes[]memory_size_bytes",     XUtil::DType_INTEGER },
    { "extensions[]global_memory_size_bytes",                XUtil::DType_INTEGER },
    { "extensions[]per_handle_memory_size_bytes",            XUtil::DType_INTEGER },
    { "softhub_connections[]version",                        XUtil::DType_INTEGER },
    { "softhub_connections[]id",                             XUtil::DType_INTEGER },
    { "cam_drivers[]compatible_version",                     XUtil::DType_INTEGER },
  };
  XUtil::transform_to_primatives(document, m_keyTypeCollection);

  XUtil::TRACE_PrintTree("SmartNic: Post-RapidJson Transform", document);

  // TODO: Insert JSON schema checking here

  // Temporary collection until functionality is added to enable this data
  // to be obtained from the JSON schema
  std::vector<std::string> byteFileEntries = {
    "::extensions[]::cam_instances::config_file",
    "::extensions[]::setup::ebpf_file",
    "::extensions[]::background_proc::ebpf_file",
    "::extensions[]::tear_down::ebpf_file",
    "::extensions[]::messages[]::ebpf_file",
    "::extensions[]::resource_classes[]::dtor_file",
    "::cam_drivers[]::driver_file",
    "::cam_drivers[]::signature_file",
  };

  // Read in the byte code
  boost::filesystem::path p(getPathAndName());
  std::string fromRelativeDir = p.parent_path().string();
  readAndTransposeByteFiles(document, byteFileEntries, fromRelativeDir);

  XUtil::TRACE_PrintTree("SmartNic: ReadByte-RapidJson Transform", document);

  // Write the CBOR image
  // Temporary collection until functionality is added to enable this data
  // to be obtained from the JSON schema
  static const XUtil::KeyTypeCollection m_encodeKeyTypeCollection = {
    { "extensions[]::version_info::uuid",                    XUtil::DType_BYTE_STRING },
    { "extensions[]::features",                              XUtil::DType_BYTE_STRING },
    { "extensions[]::cam_instances::config",                 XUtil::DType_BYTE_STRING },
    { "extensions[]::setup::ebpf",                           XUtil::DType_BYTE_STRING },
    { "extensions[]::background_proc::ebpf",                 XUtil::DType_BYTE_STRING },
    { "extensions[]::tear_down::ebpf",                       XUtil::DType_BYTE_STRING },
    { "extensions[]::messages[]::ebpf",                      XUtil::DType_BYTE_STRING },
    { "extensions[]::resource_classes[]::dtor",              XUtil::DType_BYTE_STRING },
    { "cam_drivers[]::driver",                               XUtil::DType_BYTE_STRING },
    { "cam_drivers[]::signature",                            XUtil::DType_BYTE_STRING },
  };

  std::ostringstream buffer;
  XUtil::write_cbor(document, m_encodeKeyTypeCollection, buffer);

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
